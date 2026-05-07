#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_EPS323

/*
 * WRO Future Engineers — ESP32 Control Firmware
 * Version 11.0 — COMPETITION READY
 *
 * Hardware: ESP32 DevKitC V4, BTS7960 43A (brushed DC 380),
 *           JX PDI-6221MG servo, ICM-20948 IMU,
 *           2x AS5600 encoders (две независимые I2C шины),
 *           OpenMV H7 Plus (UART2).
 *
 * I2C ARCHITECTURE (v11.1 — без PCA9548A):
 *   Bus 1 (Wire,  GPIO21/22): IMU 0x68, AS5600 left 0x36
 *   Bus 2 (Wire1, GPIO25/26): AS5600 right 0x36
 *
 * IMPLEMENTED FEATURES (v11.0):
 *  1.  Camera UART protocol v3 with XOR checksum validation:
 *        RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*CS
 *  2.  FSM: INIT / TRACKING / BLIND_TURN / PARKING / SAFE_STOP / FINISH
 *  3.  Priority order in updateControl(): BLIND_TURN (camera-independent)
 *        → camera offline (gyro heading) → tracking (pillar/heading PID)
 *  4.  Blind-turn entry: WALL_BIT OR RedDist<35 OR GreenDist<35
 *        (3 consecutive frames) AND directionConfirmed
 *  5.  Dual-pillar PID avoidance with EMA-filtered derivative and
 *        integral reset on pillar-color switch. S-curve pre-positioning
 *        from the FAR pillar when BOTH are visible.
 *  6.  Gyroscope heading hold when no landmarks are visible.
 *  7.  (ToF отключён — нет VL53 в прошивке.)
 *  8.  Parallel parking FSM (APPROACH → ALIGN → CREEP (reverse) → FINAL).
 *  9.  Odometry: AS5600 4096 ticks/rev, D=47mm → 277 ticks/cm,
 *        with wrap-around handling and 50-error safe-stop guard.
 * 10.  Dual lap counting: camera line (orange/blue) + gyroscope 360°.
 * 11.  E-Stop button: press+release = START; during race = SAFE_STOP / resume.
 * 12.  Live PID tuning over USB Serial: P/I/D/G commands.
 * 13.  Adaptive speed profiles Open vs. Obstacle (set at compile time).
 * 14.  Smooth acceleration ramp (SPEED_RAMP_STEP per 10 ms cycle).
 * 15.  Telemetry frame every 200 ms.
 *
 * FIXED BUGS (v10 → v11):
 *  - closeWallCount now ignites on WALL_BIT OR pillar distance < 35 cm
 *    (was only WALL_BIT).
 *  - S-curve for simultaneous pillars now adds a lateral offset from the
 *    FAR pillar so the robot starts to pre-position early.
 *  - Parking ALIGN: выравнивание только по гироскопу (без ToF).
 *  - Parking CREEP now reverses (negative speed) per competition technique.
 *  - Servo pin fixed to GPIO27 (matches WRO_Wiring_Map.md).
 *  - lineConfirmCount is explicitly reset while the cooldown window is
 *    active, preventing false extra laps.
 *  - safeStop() resets lastActivePillar and currentState consistently.
 *  - Blind turn trigger evaluated BEFORE any camera dependence.
 *
 * WRO Rule 9.9: NO physical mode switches. Select mode at compile time
 *               via #define OBSTACLE_CHALLENGE_MODE below.
 */

// ============================================================
// 2.  INCLUDES
// ============================================================
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <math.h>

// ============================================================
// 3.  PINS  (do not change — hardware-defined)
// ============================================================
#define I2C_SDA      21    // Bus 1 (Wire)  — левый AS5600 + IMU
#define I2C_SCL      22
#define I2C_SDA2     25    // Bus 2 (Wire1) — правый AS5600
#define I2C_SCL2     26
#define SERVO_PIN    27    // Steering servo PWM (per WRO_Wiring_Map.md)
#define MOTOR_R_EN   19    // BTS7960 right-enable
#define MOTOR_L_EN   23    // BTS7960 left-enable
#define MOTOR_R_PWM  5     // BTS7960 R-PWM (forward channel)
#define MOTOR_L_PWM  14    // BTS7960 L-PWM (reverse channel)
#define UART_RX      16    // from OpenMV TX
#define UART_TX      17    // to OpenMV RX
#define ESTOP_PIN    32    // E-Stop / Start button, INPUT_PULLUP, active LOW
#define LED_PIN      2     // Status LED

// ============================================================
// 4.  RACE MODE (WRO Rule 9.9 — set at compile time, NOT by a switch)
// ============================================================
// Uncomment for OBSTACLE Challenge (laps + pillars + parking).
// Comment  for OPEN     Challenge (laps only, no pillars/no parking).
// #define OBSTACLE_CHALLENGE_MODE   // <-- Open Challenge активен (CCW, tails-tails)

// ============================================================
// 5.  SERVO AND SPEEDS
// ============================================================
// Calibrated pulse widths in microseconds.
// Run test_servo_calibrate.cpp to find your exact values.
// Center: 1565, Limits: 1215 and 1915 (+/- 350µs).
#define SERVO_CENTER_US    1565  // µs — wheels perfectly straight
#define SERVO_RIGHT_US     1915  // µs — max right steering
#define SERVO_LEFT_US      1215  // µs — max left steering

// Safety margin away from the mechanical end-stops.
// If the servo "knocks/buzzes" at the ends, increase this value (e.g. 80–120).
#define SERVO_MARGIN_US      60
#define SERVO_RIGHT_SAFE_US  (SERVO_RIGHT_US - SERVO_MARGIN_US)
#define SERVO_LEFT_SAFE_US   (SERVO_LEFT_US  + SERVO_MARGIN_US)

// Legacy degree aliases — used throughout updateControl() calculations.
// These are mapped to µs inside setSteering().
#define SERVO_CENTER      90
#define SERVO_MAX_RIGHT   135
#define SERVO_MAX_LEFT    45

// Open Challenge speed profile: gentle, careful drive.
#define OPEN_MAX_SPEED    80    // PWM, 0..255 — обычная езда
#define OPEN_TURN_FAST    70
#define OPEN_TURN_SLOW    55
#define OPEN_MIN_SPEED    40    // Motor deadband compensation floor

// Obstacle Challenge speed profile (favours precision — tighter avoidance).
#define OBS_MAX_SPEED     140
#define OBS_TURN_FAST     110
#define OBS_TURN_SLOW     75
#define OBS_MIN_SPEED     35

// Active values (selected in setup() from the profile above).
int MOTOR_MAX_SPEED = OBS_MAX_SPEED;
int MOTOR_TURN_FAST = OBS_TURN_FAST;
int MOTOR_TURN_SLOW = OBS_TURN_SLOW;
int MOTOR_MIN_SPEED = OBS_MIN_SPEED;

// ============================================================
// 6.  PID COEFFICIENTS AND STATE
// ============================================================
// Pillar-avoidance PID (tunable over Serial: P/I/D commands).
float PID_KP  = 0.35f;   // proportional  — main steering force (уменьшено)
float PID_KI  = 0.001f;  // integral      — corrects steady-state bias (уменьшено)
float PID_KD  = 0.25f;   // derivative    — damps overshoot (EMA-filtered) (увеличено для плавности)
float GYRO_KP = 1.00f;   // heading-hold gain when no camera cue is present

float integralError      = 0.0f;
float filteredDerivative = 0.0f;

// ============================================================
// 7.  I2C — две независимые шины (без PCA9548A)
// ============================================================
//   Шина 1 (Wire,  GPIO21/22): IMU + AS5600 левый
//   Шина 2 (Wire1, GPIO25/26): AS5600 правый
//
// Два AS5600 не могут жить на одной шине (общий адрес 0x36) —
// поэтому распределяем их по разным аппаратным I2C интерфейсам ESP32.
#define AS5600_ADDRESS    0x36
#define ICM20948_ADDRESS  0x68   // AD0 → GND (если AD0 → 3.3V, то 0x69)

// Идентификаторы устройств — заменяют старые TCA каналы.
// Используются для выбора нужной TwoWire шины через busFor().
#define BUS_IMU           0
#define BUS_ENC_LEFT      1
#define BUS_ENC_RIGHT     2

// ============================================================
// Emergency motor diagnostic. Keep 0 for normal driving.
// Set to 1 only when testing BTS7960/motor wiring.
// ============================================================
#define OPEN_FORCE_MOTOR_TEST 0
#define OPEN_FORCE_TEST_SPEED 180
#define OPEN_FORCE_REVERSE_MS 2500

// ============================================================
// 9.  TIMEOUTS
// ============================================================
#define CAMERA_TIMEOUT           500    // ms — frame gap after which camera = offline
#define CAMERA_OFFLINE_STOP_MS   3000   // ms — safe-stop if camera silent this long
#define PRINT_INTERVAL           200    // ms — telemetry rate
#define LOOP_INTERVAL            10     // ms — main control-loop period
#define GYRO_CALIB_SAMPLES       300    // samples (~3 s at 10 ms/sample)
#define FINISH_BLINK_MS          250    // ms — LED blink half-period at finish
#define ESTOP_DEBOUNCE_MS        20     // ms — debounce window for the E-Stop pin
#define MAX_IMU_DT_SEC           0.05f  // s — clamp for yaw integration dt
#define ENCODER_FAIL_STOP        50     // consecutive errors before SAFE_STOP
#define SPEED_RAMP_STEP          8      // PWM units per 10 ms loop
#define BLIND_TURN_TIMEOUT       3000   // ms — emergency exit for blind turn
#define BLIND_TURN_ANGLE         85.0f  // deg — normal exit angle
#define ENCODER_TURN_TICKS       3000   // ticks — travel-based backup exit

// ============================================================
// 10.  ODOMETRY
// ============================================================
// AS5600 = 4096 ticks/rev. Wheel D = 47 mm → C = π·47 = 147.65 mm.
// Ticks per mm  = 4096 / 147.65 ≈ 27.74
// Ticks per cm  = 4096 / 147.65 · 10 ≈ 277
#define TICKS_PER_CM  277

// ============================================================
// 11.  RACE
// ============================================================
// Auto-finish по gyro отключён (99 кругов).
// Open  state machine: 4 сегмента → стоп.
// Obs   state machine: 12 сегментов → стоп.
#define TARGET_LAPS   99
#define LAP_DEGREES   360.0f

// ============================================================
// 12.  LAP COUNTING
// ============================================================
#define LINE_DETECT_COOLDOWN_MS  3000  // ms — minimum gap between two detected laps
#define LINE_CONFIRM_FRAMES      3     // consecutive frames needed to confirm line

// ============================================================
// 13.  PARKING
// ============================================================
#define PARK_APPROACH_SPEED   60    // PWM while scanning for magenta
#define PARK_CREEP_SPEED      40    // PWM magnitude during reverse into the bay
#define PARK_DISTANCE_CM      60    // cm — total reverse-in distance
#define PARK_BRAKE_ZONE_CM    15    // cm — tail-end gentle-brake region
#define PARK_MAGENTA_CONFIRM  5     // frames of magenta-visible to confirm entry

// ============================================================
// 14.  ENUMS
// ============================================================
enum RobotState {
  STATE_INIT,         // Idle, awaiting the start (E-Stop press+release)
  STATE_TRACKING,     // Normal driving (camera PID / gyro heading hold)
  STATE_BLIND_TURN,   // Corner with no visual reference (gyro + odometry exit)
  STATE_PARKING,      // After 3 laps in Obstacle mode — parallel parking
  STATE_SAFE_STOP,    // Emergency stop (E-Stop held / encoder fault)
  STATE_FINISH        // Race completed successfully
};

enum ParkPhase {
  PARK_APPROACH,     // Scan forward, looking for magenta wooden blocks
  PARK_ALIGN,        // Halt, align heading (gyro), then CREEP
  PARK_CREEP,        // Reverse into the 20 cm parking zone
  PARK_FINAL_STOP    // Full stop → finishRace()
};

// ============================================================
// 15.  GLOBAL OBJECTS
// ============================================================
Servo              steeringServo;
Adafruit_ICM20948  icm;

// ============================================================
// 16.  GLOBAL VARIABLES — grouped by subsystem
// ============================================================

// ----- Camera (UART v3, 6 fields) -----
int  camRedX       = 0;     // X of the red pillar   (-160..160)
int  camRedDist    = 999;   // Distance to red       (cm; 999 = not seen)
int  camGreenX     = 0;     // X of the green pillar (-160..160)
int  camGreenDist  = 999;   // Distance to green     (cm; 999 = not seen)
int  camModeFlag   = 0;     // Bit field (see CAM_FLAG_*)
int  camExtraTag   = 0;     // Reserved extra byte   (0..255)

bool           cameraOnline   = false;
bool           newCameraData  = false;
unsigned long  lastCameraTime = 0;

#define CAM_FLAG_ORANGE   1   // bit 0 — orange line visible (CW)
#define CAM_FLAG_BLUE     2   // bit 1 — blue   line visible (CCW)
#define CAM_FLAG_MAGENTA  4   // bit 2 — magenta parking block visible
#define CAM_FLAG_WALL     8   // bit 3 — wall is close ahead

#define UART_BUF_SIZE  64
static char     uartBuf[UART_BUF_SIZE];
static uint8_t  uartBufPos = 0;

// ----- IMU / yaw integration -----
float  yawAngle       = 0.0f;   // deg, signed, wrapped to ±180
float  gyroZbias      = 0.0f;   // rad/s — static zero-rate calibration offset
float  totalRotation  = 0.0f;   // deg, unbounded accumulator (for gyro lap count)
float  lastYaw        = 0.0f;
unsigned long lastIMUTime = 0;
float  targetHeading  = 0.0f;   // deg — heading setpoint during gyro hold

// Magnetometer calibration offsets (kept for future use — see updateYaw()).
float  magBiasX = 0.0f;
float  magBiasY = 0.0f;

// ----- Odometry -----
int   encLeft        = 0;
int   encRight       = 0;
int   encLeftPrev    = 0;
int   encRightPrev   = 0;
long  totalDistLeft  = 0;   // signed tick accumulator (forward positive)
long  totalDistRight = 0;
int   encErrorCount       = 0;
bool  encoderFaultActive  = false;

// ----- Lap counting -----
int            lineLapCount     = 0;
unsigned long  lastLineSeen     = 0;
int            lineConfirmCount = 0;

// ----- Control setpoints -----
int  targetSteering     = SERVO_CENTER;
int  targetSpeed        = 0;         // requested PWM (signed: +fwd, -rev)
int  commandSpeed       = 0;         // ramped actual PWM
int  lastCameraError    = 0;
int  lastActivePillar   = 0;         // 3 = red, 4 = green, 0 = none

// ----- Track direction -----
// Hardcoded to CCW ("Tails") based on user request.
int   globalTrackDirection = -1;     // +1 = CW, -1 = CCW (blue first)
bool  directionConfirmed   = true;   // Force confirmation without waiting for line

// ----- Blind turn state -----
long           turnStartDistLeft  = 0;
long           turnStartDistRight = 0;
float          turnStartYaw       = 0.0f;
int            turnDirection      = 1;
unsigned long  turnStartTime      = 0;
int            closeWallCount     = 0;

// ----- FSM & race progress -----
RobotState  currentState   = STATE_INIT;
int         lapCount       = 0;
bool        raceFinished   = false;
bool        estopActive    = false;
bool        raceStarted    = false;

#ifdef OBSTACLE_CHALLENGE_MODE
bool        isObstacleMode = true;
#else
bool        isObstacleMode = false;
#endif

// ----- Parking state -----
ParkPhase      parkPhase        = PARK_APPROACH;
long           parkStartTicks   = 0;
float          parkTargetYaw    = 0.0f;
int            magentaSeenCount = 0;

// ============================================================
// 17.  FUNCTION PROTOTYPES (needed for forward calls)
// ============================================================
void safeStop();
void enterParking();
void updateParking();
void enterBlindTurn(int direction);
bool updateBlindTurn();
void updateOpenOval();
void updateObstacleBlind();
void finishRace();

// ============================================================
// 18.  UTILITIES
// ============================================================

// Возвращает указатель на правильную I2C шину для устройства.
// (Раньше использовался PCA9548A — теперь его нет, два разных аппаратных
// I2C интерфейса ESP32 справляются сами.)
TwoWire& busFor(uint8_t device) {
  switch (device) {
    case BUS_IMU:        return Wire;    // GPIO21/22
    case BUS_ENC_LEFT:   return Wire;    // GPIO21/22
    case BUS_ENC_RIGHT:  return Wire1;   // GPIO25/26
    default:             return Wire;
  }
}

// Заглушки для совместимости со старыми вызовами tcaSelect/tcaDisable —
// сейчас никакого мультиплексора нет, эти функции ничего не делают.
inline bool tcaSelect(uint8_t /*ch*/) { return true; }
inline bool tcaDisable() { return true; }

void setSteering(int angle) {
  angle = constrain(angle, SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
  // Map the legacy 45–135° range to calibrated microseconds so the
  // rest of the code doesn't need to change.  When SERVO_CENTER_US is
  // tuned via test_servo_calibrate.cpp, the physical centre is correct
  // regardless of the servo's factory offset.
  int us;
  if (angle >= SERVO_CENTER)
    us = map(angle, SERVO_CENTER, SERVO_MAX_RIGHT, SERVO_CENTER_US, SERVO_RIGHT_SAFE_US);
  else
    us = map(angle, SERVO_MAX_LEFT, SERVO_CENTER, SERVO_LEFT_SAFE_US, SERVO_CENTER_US);
  steeringServo.writeMicroseconds(constrain(us, SERVO_LEFT_SAFE_US, SERVO_RIGHT_SAFE_US));
}

// Map a desired speed magnitude to the actual PWM, compensating for
// the BTS7960 + brushed-DC deadband (motor won't start below MIN_SPEED).
int applyDeadband(int speed) {
  if (speed == 0) return 0;
  int mapped = map(abs(speed), 1, MOTOR_MAX_SPEED, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
  return constrain(mapped, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
}

// Write signed PWM to BTS7960 using pins (ESP32 V3).
void setMotorSpeed(int speed) {
  speed = constrain(speed, -255, 255);
  int mapped = applyDeadband(speed);
  if (speed > 0) {              // forward
    ledcWrite(MOTOR_L_PWM, 0);
    ledcWrite(MOTOR_R_PWM, mapped);
  } else if (speed < 0) {       // reverse
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, mapped);
  } else {                      // coast stop
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
  }
}

// Move commandSpeed toward targetSpeed by SPEED_RAMP_STEP per loop cycle.
// Avoids torque spikes / tipping when speed requests jump.
void applySpeedRamp() {
  if (commandSpeed < targetSpeed)
    commandSpeed = min(commandSpeed + SPEED_RAMP_STEP, targetSpeed);
  else if (commandSpeed > targetSpeed)
    commandSpeed = max(commandSpeed - SPEED_RAMP_STEP, targetSpeed);
  setMotorSpeed(commandSpeed);
}

// Safe stop: zero the actuators and reset transient PID / latch state so
// that the next resume starts from a clean slate.
void safeStop() {
  targetSpeed        = 0;
  commandSpeed       = 0;
  setMotorSpeed(0);
  targetSteering     = SERVO_CENTER;
  setSteering(SERVO_CENTER);
  integralError      = 0.0f;
  filteredDerivative = 0.0f;
  lastCameraError    = 0;
  lastActivePillar   = 0;
  closeWallCount     = 0;
  newCameraData      = false;
  currentState       = STATE_SAFE_STOP;
}

// ============================================================
// 19.  E-STOP AND START BUTTON
// ============================================================
// Same physical pin has two behaviours:
//   - Before raceStarted: press+release arms and starts the race.
//   - After  raceStarted: held-down = SAFE_STOP; released = resume.
void checkEStop() {
  static unsigned long estopTriggerTime = 0;
  static bool          estopArmed       = false;
  bool pinLow = (digitalRead(ESTOP_PIN) == LOW);

  if (pinLow && !estopArmed) {
    estopArmed       = true;
    estopTriggerTime = millis();
    return;
  }
  if (estopArmed && !pinLow) {
    estopArmed = false;
    if (!estopActive) return;
  }
  if (estopArmed && (millis() - estopTriggerTime >= ESTOP_DEBOUNCE_MS)) {
    if (!estopActive) {
      estopActive = true;
      safeStop();
      Serial.println("=== E-STOP ENGAGED ===");
    }
  }
  if (estopActive && !pinLow) {
    estopActive = false;
    estopArmed  = false;
    if (raceStarted) {
      currentState   = STATE_TRACKING;
      lastIMUTime    = millis();
      lastCameraTime = millis();
      lastYaw        = yawAngle;
      targetHeading  = yawAngle;
      Serial.println("=== RACE RESUMED ===");
    }
  }
  if (estopActive) safeStop();
}

// Used while in STATE_INIT: press + release → race begins.
void handleStartButton() {
  static bool wasPressed = false;
  bool pinLow = (digitalRead(ESTOP_PIN) == LOW);

  if (pinLow) {
    wasPressed = true;
    digitalWrite(LED_PIN, (millis() / 100) % 2);   // fast blink while armed
  }
  if (wasPressed && !pinLow) {
    raceStarted    = true;
    currentState   = STATE_TRACKING;
    lastIMUTime    = millis();
    lastCameraTime = millis();
    lastYaw        = yawAngle;
    targetHeading  = yawAngle;
    Serial.println("+==========================+");
    Serial.println("|       RACE STARTED       |");
    Serial.println("+==========================+");
    digitalWrite(LED_PIN, HIGH);
  }
}

// ============================================================
// 20.  FINISH
// ============================================================
void finishRace() {
  safeStop();
  raceFinished = true;
  currentState = STATE_FINISH;
  Serial.println("+==================================+");
  Serial.println("|  FINISH — 3 LAPS + PARKING DONE  |");
  Serial.println("+==================================+");
}

void handleFinishState() {
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > FINISH_BLINK_MS) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  checkEStop();
}

// ============================================================
// 21.  IMU — ICM-20948 (yaw integration from gyro Z)
// ============================================================
void calibrateGyro() {
  Serial.println("Gyro calibration (3 s, keep robot still)...");
  float sum = 0.0f;
  for (int i = 0; i < GYRO_CALIB_SAMPLES; i++) {
    sensors_event_t accel, gyro, mag, temp;
    icm.getEvent(&accel, &gyro, &temp, &mag);
    sum += gyro.gyro.z;
    delay(10);
  }
  gyroZbias = sum / GYRO_CALIB_SAMPLES;
  Serial.print("gyroZbias = ");
  Serial.println(gyroZbias, 6);
}

void updateYaw() {
  unsigned long now = millis();
  if (lastIMUTime == 0) { lastIMUTime = now; return; }

  float dt = (now - lastIMUTime) / 1000.0f;
  lastIMUTime = now;
  if (dt <= 0.0f) return;
  if (dt > MAX_IMU_DT_SEC) dt = MAX_IMU_DT_SEC;   // clamp after I2C hiccups

  sensors_event_t accel, gyro, mag, temp;
  bool ok = icm.getEvent(&accel, &gyro, &temp, &mag);
  if (!ok) return;

  float correctedZ = gyro.gyro.z - gyroZbias;
  if (fabsf(correctedZ) < 0.01f) correctedZ = 0.0f;    // kill residual drift

  yawAngle += correctedZ * dt * (180.0f / PI);
  while (yawAngle >  180) yawAngle -= 360;
  while (yawAngle < -180) yawAngle += 360;

  // ----------------------------------------------------------
  // Magnetometer correction — INTENTIONALLY DISABLED.
  // The brushed 380 motor generates strong EMI which corrupts the
  // ICM-20948 magnetometer. Do NOT enable this block until you have
  // calibrated magBiasX/magBiasY on the competition track:
  //
  //   1. Power on the robot and rotate it through 360° slowly by hand.
  //   2. Log mag.magnetic.x / .y min and max.
  //   3. magBiasX = (minX + maxX) / 2;  magBiasY = (minY + maxY) / 2;
  //   4. Store those two floats and uncomment the block below.
  //
  // float magX = mag.magnetic.x - magBiasX;
  // float magY = mag.magnetic.y - magBiasY;
  // float magHeading = atan2f(magY, magX) * (180.0f / PI);
  // float magError   = magHeading - yawAngle;
  // while (magError >  180) magError -= 360;
  // while (magError < -180) magError += 360;
  // yawAngle += 0.02f * magError;
  // while (yawAngle >  180) yawAngle -= 360;
  // while (yawAngle < -180) yawAngle += 360;
  // ----------------------------------------------------------
}

// ============================================================

// Accumulate signed yaw delta into totalRotation. The integer division
// |totalRotation| / 360 gives a reliable lap count even without visual line.
void checkLapCountGyro() {
  float deltaYaw = yawAngle - lastYaw;
  while (deltaYaw >  180) deltaYaw -= 360;
  while (deltaYaw < -180) deltaYaw += 360;
  totalRotation += deltaYaw;
  lastYaw = yawAngle;
}

// Camera-based lap counting from orange / blue line detection.
// Uses a cooldown + consecutive-frame confirmation to reject false spikes.
void checkLapCountLine() {
  bool orangeSeen = (camModeFlag & CAM_FLAG_ORANGE) != 0;
  bool blueSeen   = (camModeFlag & CAM_FLAG_BLUE)   != 0;
  bool lineSeen   = orangeSeen || blueSeen;

  // First-seen colour decides the track direction for the whole run.
  if (!directionConfirmed) {
    if (orangeSeen) { globalTrackDirection =  1; directionConfirmed = true; }
    if (blueSeen)   { globalTrackDirection = -1; directionConfirmed = true; }
    if (directionConfirmed) {
      Serial.print(">>> TRACK DIRECTION: ");
      Serial.println(globalTrackDirection == 1 ? "CW (orange)" : "CCW (blue)");
    }
  }

  // Cooldown: suppress and (FIX) hard-reset the confirm counter so the
  // next lap starts counting only after the cooldown window expires.
  if (millis() - lastLineSeen < LINE_DETECT_COOLDOWN_MS) {
    lineConfirmCount = 0;
    return;
  }

  if (lineSeen) lineConfirmCount++;
  else          lineConfirmCount = 0;

  if (lineConfirmCount >= LINE_CONFIRM_FRAMES) {
    lineLapCount++;
    lastLineSeen     = millis();
    lineConfirmCount = 0;
    Serial.print("=== LAP ");
    Serial.print(lineLapCount);
    Serial.print("/");
    Serial.print(TARGET_LAPS);
    Serial.println(" (line) ===");
  }
}

// Combine both systems and trigger the end-of-race transition.
void checkLapCompletion() {
  int gyroLaps = (int)(fabsf(totalRotation) / LAP_DEGREES);
  lapCount = max(lineLapCount, gyroLaps);

  if (lapCount >= TARGET_LAPS && currentState == STATE_TRACKING) {
    if (isObstacleMode) {
      Serial.println(">>> 3 LAPS DONE (OBSTACLE) -> ENTERING PARKING <<<");
      enterParking();
    } else {
      // WRO 2026 Open Challenge: just stop in the start section.
      Serial.println(">>> 3 LAPS DONE (OPEN) -> IMMEDIATE FINISH <<<");
      finishRace();
    }
  }
}

// ============================================================
// 24.  ODOMETRY — AS5600 angle → tick accumulator
// ============================================================
int readEncoder(uint8_t which) {
  TwoWire& bus = busFor(which);
  bus.beginTransmission(AS5600_ADDRESS);
  bus.write(0x0E);                              // RAW_ANGLE high byte register
  if (bus.endTransmission(false) != 0) return -1;
  uint8_t got = bus.requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)2);
  if (got >= 2 && bus.available() >= 2) {
    int h = bus.read(), l = bus.read();
    return ((h & 0x0F) << 8) | l;               // 12-bit angle (0..4095)
  }
  return -1;
}

void updateOdometry() {
  if (encLeft < 0 || encRight < 0) {
    encErrorCount++;
    if (encErrorCount == 10) {
      Serial.println("ALARM: encoders lost!");
      encLeftPrev  = max(encLeft,  0);
      encRightPrev = max(encRight, 0);
    }
    if (encErrorCount >= ENCODER_FAIL_STOP) {
      encoderFaultActive = true;
      Serial.println("CRITICAL: ENCODER FAULT -> SAFE STOP");
      safeStop();
    }
    return;
  }
  if (encoderFaultActive) {
    encoderFaultActive = false;
    Serial.println("INFO: encoders recovered");
  }
  if (encErrorCount >= 10) {                    // resync after transient loss
    encLeftPrev  = encLeft;
    encRightPrev = encRight;
  }
  encErrorCount = 0;

  int dL = encLeft  - encLeftPrev;
  int dR = encRight - encRightPrev;
  // Wrap-around handling for 12-bit counter (4096 ticks / rev).
  if (dL >  2048) dL -= 4096;
  if (dL < -2048) dL += 4096;
  if (dR >  2048) dR -= 4096;
  if (dR < -2048) dR += 4096;

  totalDistLeft  += dL;
  totalDistRight += dR;
  encLeftPrev  = encLeft;
  encRightPrev = encRight;
}

// ============================================================
// 25.  CAMERA — OpenMV UART v3
// ============================================================
// Frame format: RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*CS\n
// CS = XOR of all characters before '*' (hex, upper or lower case).
uint8_t xorChecksum(const char *s, int len) {
  uint8_t cs = 0;
  for (int i = 0; i < len; i++) cs ^= (uint8_t)s[i];
  return cs;
}

void readCameraData() {
  newCameraData = false;

  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      uartBuf[uartBufPos] = '\0';
      uartBufPos = 0;
      if (strlen(uartBuf) == 0) continue;

      // Optional checksum: validate if '*' present; skip otherwise.
      char *star = strchr(uartBuf, '*');
      if (star) {
        *star = '\0';
        uint8_t expected = xorChecksum(uartBuf, strlen(uartBuf));
        uint8_t received = (uint8_t)strtol(star + 1, NULL, 16);
        if (expected != received) continue;          // EMI corruption
      }

      // Parse 6 integer fields.
      int f[6] = {0, 999, 0, 999, 0, 0};
      char *tok = uartBuf;
      for (int i = 0; i < 6 && tok; i++) {
        char *comma = strchr(tok, ',');
        if (comma) *comma = '\0';
        f[i] = atoi(tok);
        tok = comma ? comma + 1 : NULL;
      }

      // Range validation — reject obviously malformed frames.
      if (f[0] >= -160 && f[0] <= 160 &&
          f[1] >=    0 && f[1] <= 999 &&
          f[2] >= -160 && f[2] <= 160 &&
          f[3] >=    0 && f[3] <= 999) {
        camRedX        = f[0];
        camRedDist     = f[1];
        camGreenX      = f[2];
        camGreenDist   = f[3];
        camModeFlag    = f[4];
        camExtraTag    = f[5];
        lastCameraTime = millis();
        cameraOnline   = true;
        newCameraData  = true;
      }
    } else {
      if (uartBufPos < UART_BUF_SIZE - 1) uartBuf[uartBufPos++] = c;
      else                                uartBufPos = 0;            // overflow — resync
    }
  }

  if (millis() - lastCameraTime > CAMERA_TIMEOUT) {
    if (cameraOnline) Serial.println("WARN: camera offline -> gyro hold");
    cameraOnline = false;
  }
}

// ============================================================
// 26.  PARKING — parallel parking FSM
// ============================================================
void enterParking() {
  currentState     = STATE_PARKING;
  parkPhase        = PARK_APPROACH;
  parkStartTicks   = totalDistLeft;
  magentaSeenCount = 0;
  // Snap the approach heading to the nearest 90° — track walls are
  // axis-aligned so this gives a clean parallel reference.
  parkTargetYaw      = roundf(yawAngle / 90.0f) * 90.0f;
  integralError      = 0.0f;
  filteredDerivative = 0.0f;
  Serial.println(">>> PARKING: APPROACH <<<");
}

void updateParking() {
  bool magentaSeen = (camModeFlag & CAM_FLAG_MAGENTA) != 0;

  switch (parkPhase) {

  // ----- APPROACH: hold course, look for magenta blocks -----
  case PARK_APPROACH: {
    float hErr = parkTargetYaw - yawAngle;
    while (hErr >  180) hErr -= 360;
    while (hErr < -180) hErr += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    targetSpeed = PARK_APPROACH_SPEED;

    if (magentaSeen) magentaSeenCount++;
    else             magentaSeenCount = max(magentaSeenCount - 1, 0);

    if (magentaSeenCount >= PARK_MAGENTA_CONFIRM) {
      parkPhase      = PARK_ALIGN;
      parkStartTicks = totalDistLeft;
      Serial.println(">>> PARKING: ALIGN (magenta acquired) <<<");
    }

    // Fallback: never got a confident magenta — try to park after 2 m.
    long distTraveled = labs(totalDistLeft - parkStartTicks);
    if (distTraveled > 200L * TICKS_PER_CM) {
      parkPhase      = PARK_ALIGN;
      parkStartTicks = totalDistLeft;
      Serial.println(">>> PARKING: ALIGN (fallback by distance) <<<");
    }
    break;
  }

  // ----- ALIGN: стоп по гироскопу, затем CREEP -----
  case PARK_ALIGN: {
    float hErr = parkTargetYaw - yawAngle;
    while (hErr >  180) hErr -= 360;
    while (hErr < -180) hErr += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(2.0f * GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    targetSpeed = 0;
    if (fabsf(hErr) < 2.0f) {
      parkPhase      = PARK_CREEP;
      parkStartTicks = totalDistLeft;
      Serial.println(">>> PARKING: CREEP (heading aligned) <<<");
    }
    break;
  }

  // ----- CREEP: reverse straight into the bay for PARK_DISTANCE_CM -----
  case PARK_CREEP: {
    float hErr = parkTargetYaw - yawAngle;
    while (hErr >  180) hErr -= 360;
    while (hErr < -180) hErr += 360;
    // Keep steering close to centre when reversing — strong corrections
    // while going backwards are unstable for Ackermann steering.
    targetSteering = constrain(SERVO_CENTER + (int)(0.5f * GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);

    long distTraveled = labs(totalDistLeft - parkStartTicks);
    long targetTicks  = (long)PARK_DISTANCE_CM * TICKS_PER_CM;
    long brakeZone    = (long)PARK_BRAKE_ZONE_CM * TICKS_PER_CM;

    // Proportional braking in the last brakeZone cm.
    int magnitude;
    if (distTraveled > targetTicks - brakeZone) {
      magnitude = map(distTraveled, targetTicks - brakeZone, targetTicks,
                      PARK_CREEP_SPEED, 0);
      magnitude = max(magnitude, 0);
    } else {
      magnitude = PARK_CREEP_SPEED;
    }
    targetSpeed = -magnitude;                 // negative = reverse

    if (distTraveled >= targetTicks) {
      parkPhase = PARK_FINAL_STOP;
      Serial.println(">>> PARKING: FINAL STOP <<<");
    }
    break;
  }

  // ----- FINAL STOP -----
  case PARK_FINAL_STOP: {
    setMotorSpeed(0);
    finishRace();
    break;
  }

  }  // switch
}

// ============================================================
// 27.  CONTROL — blind turns + main updateControl()
// ============================================================

void enterBlindTurn(int direction) {
  currentState       = STATE_BLIND_TURN;
  turnStartYaw       = yawAngle;
  turnDirection      = direction;
  turnStartTime      = millis();
  turnStartDistLeft  = totalDistLeft;
  turnStartDistRight = totalDistRight;
  integralError      = 0.0f;
  filteredDerivative = 0.0f;
  closeWallCount     = 0;
  Serial.print(">>> BLIND TURN: ");
  Serial.println(turnDirection == 1 ? "RIGHT (CW)" : "LEFT (CCW)");
}

// Runs once per loop while in STATE_BLIND_TURN. Returns true when exit
// conditions are met and the FSM has transitioned back to TRACKING.
// Snap a heading angle to the nearest 90° axis ( -180 / -90 / 0 / 90 ).
// Used after blind turns so small under/overshoot doesn't accumulate.
static inline float snapHeadingTo90(float yaw) {
  float snapped = roundf(yaw / 90.0f) * 90.0f;
  while (snapped >  180) snapped -= 360;
  while (snapped < -180) snapped += 360;
  return snapped;
}

// Brake-before-turn: how long to decelerate (and keep wheels straight)
// before applying full steering lock.  Without this the car still rolls
// 8–10 cm forward on full lock and clips the front wall on tight tracks.
#define BLIND_TURN_BRAKE_MS  180

bool updateBlindTurn() {
  unsigned long elapsed = millis() - turnStartTime;

  // Emergency 3 s timeout — always bail out so we never get stuck.
  if (elapsed > BLIND_TURN_TIMEOUT) {
    currentState    = STATE_TRACKING;
    targetHeading   = snapHeadingTo90(yawAngle);
    closeWallCount  = 0;
    lastCameraError = 0;
    Serial.println(">>> BLIND TURN: timeout exit <<<");
    return true;
  }

  // ---- Phase 1: brake straight (until the car has actually slowed down) ----
  // Wheels straight, request zero speed. The speed-ramp will pull
  // commandSpeed down quickly. Once the car is slow enough OR a short
  // window has passed, fall through to the steering phase.
  if (elapsed < BLIND_TURN_BRAKE_MS && commandSpeed > MOTOR_MIN_SPEED) {
    targetSteering = SERVO_CENTER;
    targetSpeed    = 0;
    return false;
  }

  // ---- Phase 2: full lock, low forward speed ----
  targetSteering = (turnDirection == 1) ? SERVO_MAX_RIGHT : SERVO_MAX_LEFT;
  targetSpeed    = MOTOR_TURN_SLOW;

  // Yaw-based exit.
  float turned = yawAngle - turnStartYaw;
  while (turned >  180) turned -= 360;
  while (turned < -180) turned += 360;

  // Encoder-distance backup exit.
  long distT = ((totalDistLeft  - turnStartDistLeft) +
                (totalDistRight - turnStartDistRight)) / 2;

  if (fabsf(turned) >= BLIND_TURN_ANGLE || labs(distT) > ENCODER_TURN_TICKS) {
    currentState    = STATE_TRACKING;
    targetHeading   = snapHeadingTo90(yawAngle);
    closeWallCount  = 0;
    lastCameraError = 0;
    Serial.println(">>> BLIND TURN: exit by yaw/odometry <<<");
    return true;
  }
  return false;
}

// ----- Open Challenge: BLIND dead-reckoning (encoder + gyro only) -----
// Никакого ToF, никакой камеры, никакого wall-following.
// Тупо: проехал N см → повернул 90° → повторил 4 раза → стоп.
//
// Прямая держится гироскопом (heading-hold), длина прямой меряется энкодером,
// поворот меряется гироскопом. Это самая надёжная dead-reckoning схема.
enum BlindDriveState { B_DRIVE, B_TURN, B_DONE };
static BlindDriveState blindState        = B_DRIVE;
static float           blindTargetYaw    = 0.0f;
static long            blindStartTicks   = 0;
static float           blindTurnStartYaw = 0.0f;
static unsigned long   blindTurnStartMs  = 0;
static int             blindSegments     = 0;
static bool            blindInited       = false;

// --- Параметры. Подкручивай если робот не дотянет/перетянет. ---
#define BLIND_SEGMENT_CM       90    // длина каждой прямой, см
#define BLIND_FIRST_SEGMENT_CM 50    // первая прямая (мы стартуем в середине секции)
#define BLIND_TURN_DEG         85    // на сколько крутим по гироскопу
#define BLIND_TURN_TIMEOUT_MS  3500
#define BLIND_HEADING_GAIN     1.6f
#define BLIND_TOTAL_SEGMENTS   4     // 4 поворота = 1 полный круг

static float wrap180(float a) {
  while (a >  180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

void updateOpenOval() {
#if OPEN_FORCE_MOTOR_TEST
  unsigned long phase = (millis() / OPEN_FORCE_REVERSE_MS) % 2;
  targetSteering    = SERVO_CENTER;
  targetSpeed       = phase == 0 ? OPEN_FORCE_TEST_SPEED : -OPEN_FORCE_TEST_SPEED;
  targetHeading     = yawAngle;
  lastActivePillar  = 0;
  lastCameraError   = 0;
  return;
#endif

  // Первый раз — фиксируем стартовый курс и точку отсчёта энкодера.
  if (!blindInited) {
    blindTargetYaw  = yawAngle;
    blindStartTicks = totalDistLeft;
    blindInited     = true;
  }

  switch (blindState) {

    // -------- Прямая: едем по энкодеру, держим курс гироскопом --------
    case B_DRIVE: {
      float hErr      = wrap180(blindTargetYaw - yawAngle);
      int steerOffset = (int)(hErr * BLIND_HEADING_GAIN);

      targetSteering  = constrain(SERVO_CENTER + steerOffset,
                                  SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
      targetSpeed     = MOTOR_MAX_SPEED;

      long traveled = labs(totalDistLeft - blindStartTicks);
      long needCm   = (blindSegments == 0)
                      ? (long)BLIND_FIRST_SEGMENT_CM
                      : (long)BLIND_SEGMENT_CM;

      bool distanceReached = (traveled > needCm * (long)TICKS_PER_CM);

      if (distanceReached) {
        blindState        = B_TURN;
        blindTurnStartYaw = yawAngle;
        blindTurnStartMs  = millis();
        Serial.print(">>> BLIND TURN ");
        Serial.print(blindSegments + 1);
        Serial.print(" START (dist ");
        Serial.print(needCm);
        Serial.println(" cm) <<<");
      }
      break;
    }

    // -------- Поворот: brake → full-lock в сторону трассы до ~85° --------
    case B_TURN: {
      unsigned long turnElapsed = millis() - blindTurnStartMs;

      // Фаза 1: тормозим прямо, прежде чем выкручивать руль.
      // Без этого робот ещё успевает проехать 8–15 см на полном замке
      // и клипает носом фронтальную стену.
      if (turnElapsed < BLIND_TURN_BRAKE_MS && commandSpeed > MOTOR_MIN_SPEED) {
        targetSteering = SERVO_CENTER;
        targetSpeed    = 0;
        break;
      }

      targetSteering = (globalTrackDirection == -1) ? SERVO_MAX_LEFT : SERVO_MAX_RIGHT;
      targetSpeed    = MOTOR_TURN_SLOW;

      float turned    = wrap180(yawAngle - blindTurnStartYaw);
      bool angleOK    = (fabsf(turned) >= BLIND_TURN_DEG);
      bool timeoutHit = (millis() - blindTurnStartMs > BLIND_TURN_TIMEOUT_MS);

      if (angleOK || timeoutHit) {
        // CCW: yaw увеличивается → +90° к target. CW: -90°.
        blindTargetYaw -= 90.0f * globalTrackDirection;
        blindTargetYaw  = wrap180(blindTargetYaw);
        blindSegments++;

        Serial.print(">>> BLIND TURN ");
        Serial.print(blindSegments);
        Serial.print(timeoutHit ? " (TIMEOUT)" : "");
        Serial.print(" DONE newYaw=");
        Serial.println(blindTargetYaw, 1);

        if (blindSegments >= BLIND_TOTAL_SEGMENTS) {
          blindState = B_DONE;
          Serial.println(">>> 1 LAP COMPLETE — STOP <<<");
        } else {
          blindState      = B_DRIVE;
          blindStartTicks = totalDistLeft;
        }
      }
      break;
    }

    // -------- Финиш --------
    case B_DONE: {
      targetSteering = SERVO_CENTER;
      targetSpeed    = 0;
      break;
    }
  }

  targetHeading    = yawAngle;
  lastActivePillar = 0;
  lastCameraError  = 0;
}

// ----- Obstacle Challenge: blind dead-reckoning + reactive pillar avoidance -----
// Тот же шаблон, что в updateOpenOval(), но 4 поворота × 3 круга = 12 сегментов.
// Если камера видит столб ближе ~80 см — добавляется коррекция руля:
//   RED   → проходим слева, держим в правой части кадра  (X≈+60)
//   GREEN → проходим справа, держим в левой части кадра  (X≈-60)
// Если камера выключена/не калибрована, столбы просто не видны и работает чистый
// dead-reckoning. После 12 сегментов робот останавливается.
enum ObstacleDriveState { O_DRIVE, O_TURN, O_DONE };
static ObstacleDriveState obsState        = O_DRIVE;
static float              obsTargetYaw    = 0.0f;
static long               obsStartTicks   = 0;
static float              obsTurnStartYaw = 0.0f;
static unsigned long      obsTurnStartMs  = 0;
static int                obsSegments     = 0;
static bool               obsInited       = false;

#define OBS_TOTAL_SEGMENTS    12   // 4 поворота × 3 круга
#define OBS_PILLAR_RANGE_CM   80
#define OBS_PILLAR_TARGET_X   60   // целевая X-позиция столба в кадре (px)
#define OBS_PILLAR_GAIN_DIV   4    // делитель для коррекции
#define OBS_PILLAR_CLAMP      35   // максимум коррекции от камеры

void updateObstacleBlind() {
#if OPEN_FORCE_MOTOR_TEST
  unsigned long phase = (millis() / OPEN_FORCE_REVERSE_MS) % 2;
  targetSteering    = SERVO_CENTER;
  targetSpeed       = phase == 0 ? OPEN_FORCE_TEST_SPEED : -OPEN_FORCE_TEST_SPEED;
  targetHeading     = yawAngle;
  lastActivePillar  = 0;
  lastCameraError   = 0;
  return;
#endif

  if (!obsInited) {
    obsTargetYaw  = yawAngle;
    obsStartTicks = totalDistLeft;
    obsInited     = true;
  }

  switch (obsState) {

    // -------- Прямая: heading-hold + реактивное обхождение столбов --------
    case O_DRIVE: {
      float hErr      = wrap180(obsTargetYaw - yawAngle);
      int steerOffset = (int)(hErr * BLIND_HEADING_GAIN);

      // --- Обход столбов (поверх heading-hold) ---
      bool hasRed   = (camRedDist   > 0 && camRedDist   < OBS_PILLAR_RANGE_CM);
      bool hasGreen = (camGreenDist > 0 && camGreenDist < OBS_PILLAR_RANGE_CM);

      int pillarErr = 0;
      if (hasRed && hasGreen) {
        // Видим оба — реагируем на ближайший.
        if (camRedDist <= camGreenDist)
          pillarErr = camRedX - OBS_PILLAR_TARGET_X;
        else
          pillarErr = camGreenX + OBS_PILLAR_TARGET_X;
      } else if (hasRed) {
        pillarErr = camRedX - OBS_PILLAR_TARGET_X;     // RED → keep on RIGHT
      } else if (hasGreen) {
        pillarErr = camGreenX + OBS_PILLAR_TARGET_X;   // GREEN → keep on LEFT
      }

      int pillarSteer = pillarErr / OBS_PILLAR_GAIN_DIV;
      pillarSteer     = constrain(pillarSteer, -OBS_PILLAR_CLAMP, OBS_PILLAR_CLAMP);
      steerOffset    += pillarSteer;

      targetSteering = constrain(SERVO_CENTER + steerOffset,
                                 SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
      targetSpeed    = MOTOR_MAX_SPEED;

      long traveled = labs(totalDistLeft - obsStartTicks);
      long needCm   = (obsSegments == 0)
                      ? (long)BLIND_FIRST_SEGMENT_CM
                      : (long)BLIND_SEGMENT_CM;

      bool distanceReached = (traveled > needCm * (long)TICKS_PER_CM);

      if (distanceReached) {
        obsState        = O_TURN;
        obsTurnStartYaw = yawAngle;
        obsTurnStartMs  = millis();
        Serial.print(">>> OBS TURN ");
        Serial.print(obsSegments + 1);
        Serial.print(" START (dist ");
        Serial.print(needCm);
        Serial.println(" cm) <<<");
      }
      break;
    }

    // -------- Поворот: brake → full-lock в сторону трассы до ~85° по гироскопу --------
    case O_TURN: {
      unsigned long turnElapsed = millis() - obsTurnStartMs;

      // Фаза 1: тормозим прямо перед выкручиванием руля.
      if (turnElapsed < BLIND_TURN_BRAKE_MS && commandSpeed > MOTOR_MIN_SPEED) {
        targetSteering = SERVO_CENTER;
        targetSpeed    = 0;
        break;
      }

      targetSteering = (globalTrackDirection == -1) ? SERVO_MAX_LEFT : SERVO_MAX_RIGHT;
      targetSpeed    = MOTOR_TURN_SLOW;

      float turned    = wrap180(yawAngle - obsTurnStartYaw);
      bool angleOK    = (fabsf(turned) >= BLIND_TURN_DEG);
      bool timeoutHit = (millis() - obsTurnStartMs > BLIND_TURN_TIMEOUT_MS);

      if (angleOK || timeoutHit) {
        obsTargetYaw -= 90.0f * globalTrackDirection;
        obsTargetYaw  = wrap180(obsTargetYaw);
        obsSegments++;

        Serial.print(">>> OBS TURN ");
        Serial.print(obsSegments);
        Serial.print(timeoutHit ? " (TIMEOUT)" : "");
        Serial.print(" DONE  lap=");
        Serial.print(obsSegments / 4 + (obsSegments % 4 == 0 ? 0 : 1));
        Serial.print(" newTargetYaw=");
        Serial.println(obsTargetYaw, 1);

        if (obsSegments >= OBS_TOTAL_SEGMENTS) {
          obsState = O_DONE;
          Serial.println(">>> 3 LAPS COMPLETE — STOP <<<");
        } else {
          obsState      = O_DRIVE;
          obsStartTicks = totalDistLeft;
        }
      }
      break;
    }

    // -------- Финиш --------
    case O_DONE: {
      targetSteering = SERVO_CENTER;
      targetSpeed    = 0;
      break;
    }
  }

  targetHeading    = yawAngle;
  lastActivePillar = 0;
  lastCameraError  = 0;
}

// Main control function — called every LOOP_INTERVAL (10 ms).
// TRACKING: только updateOpenOval / updateObstacleBlind (энкодер + гироскоп + камера на столбы).
void updateControl() {
  if (raceFinished)        return;
  if (encoderFaultActive) { safeStop(); return; }

  if (currentState == STATE_SAFE_STOP) {
    targetSpeed    = 0;
    targetSteering = SERVO_CENTER;
    return;
  }
  if (currentState == STATE_PARKING)    { updateParking();    return; }
  if (currentState == STATE_BLIND_TURN) { updateBlindTurn();  return; }

  if (!isObstacleMode) {
    updateOpenOval();
    return;
  }
  updateObstacleBlind();
}

// ============================================================
// 28.  LIVE TUNING — Serial commands
// ============================================================
// P0.55  → PID_KP = 0.55
// I0.002 → PID_KI = 0.002  (also resets integralError)
// D0.18  → PID_KD = 0.18
// G1.2   → GYRO_KP = 1.2
void checkSerialCommands() {
  static char    cmdBuf[16];
  static uint8_t cmdPos = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      cmdBuf[cmdPos] = '\0';
      if (cmdPos > 1) {
        char  t = cmdBuf[0];
        float v = atof(&cmdBuf[1]);
        if (t == 'P' || t == 'p') { PID_KP = v; Serial.print("Kp=");  Serial.println(PID_KP,  4); }
        if (t == 'D' || t == 'd') { PID_KD = v; Serial.print("Kd=");  Serial.println(PID_KD,  4); }
        if (t == 'I' || t == 'i') { PID_KI = v; integralError = 0.0f;
                                    Serial.print("Ki=");  Serial.println(PID_KI,  5); }
        if (t == 'G' || t == 'g') { GYRO_KP = v; Serial.print("Gk="); Serial.println(GYRO_KP, 4); }
      } else if (cmdPos == 1) {
        char t = cmdBuf[0];
        if (t == 'g' || t == 'G') {
          if (!raceStarted) {
            raceStarted    = true;
            currentState   = STATE_TRACKING;
            lastIMUTime    = millis();
            lastCameraTime = millis();
            lastYaw        = yawAngle;
            targetHeading  = yawAngle;
            Serial.println("+==========================+");
            Serial.println("|   RACE STARTED via 'g'   |");
            Serial.println("+==========================+");
            digitalWrite(LED_PIN, HIGH);
          }
        }
        if ((t == 's' || t == 'S') && raceStarted) {
          currentState = STATE_SAFE_STOP;
          safeStop();
          Serial.println(">>> STOPPED via 's' <<<");
        }
      }
      cmdPos = 0;
    } else if (cmdPos < 15) {
      cmdBuf[cmdPos++] = c;
    }
  }
}

// ============================================================
// 29.  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  delay(500);

  Serial.println("+=======================================+");
  Serial.println("|   WRO v11.0 — COMPETITION READY       |");
  Serial.println("+=======================================+");

  // GPIO
  pinMode(ESTOP_PIN, INPUT_PULLUP);   // WRO 9.9: NO physical mode switch
  pinMode(LED_PIN,   OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Race profile (compile-time).
  Serial.print("MODE: ");
  if (isObstacleMode) {
    Serial.println("OBSTACLE (precision profile)");
    MOTOR_MAX_SPEED = OBS_MAX_SPEED;
    MOTOR_TURN_FAST = OBS_TURN_FAST;
    MOTOR_TURN_SLOW = OBS_TURN_SLOW;
    MOTOR_MIN_SPEED = OBS_MIN_SPEED;
  } else {
    Serial.println("OPEN (speed profile)");
    MOTOR_MAX_SPEED = OPEN_MAX_SPEED;
    MOTOR_TURN_FAST = OPEN_TURN_FAST;
    MOTOR_TURN_SLOW = OPEN_TURN_SLOW;
    MOTOR_MIN_SPEED = OPEN_MIN_SPEED;
  }

  // UART to OpenMV.
  Serial2.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  Serial2.setTimeout(10);

  // I2C — две независимые шины.
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Wire.setTimeout(3);    // 3 ms HW timeout

  Wire1.begin(I2C_SDA2, I2C_SCL2);
  Wire1.setClock(400000);
  Wire1.setTimeout(3);

  // Steering servo.
  steeringServo.attach(SERVO_PIN, 500, 2500);
  setSteering(SERVO_CENTER);

  // BTS7960 motor driver — both half-bridges enabled.
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);
  
  // ESP_ARDUINO_VERSION_MAJOR >= 3 APIs
  ledcAttach(MOTOR_R_PWM, 20000, 8); 
  ledcAttach(MOTOR_L_PWM, 20000, 8);
  setMotorSpeed(0);

  // IMU (ICM-20948) — Bus 1 (Wire), адрес 0x68 (AD0=GND).
  if (!icm.begin_I2C(ICM20948_ADDRESS, &Wire)) {
    Serial.println("ICM-20948 NOT FOUND — halting.");
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  calibrateGyro();

  // Time-base priming.
  targetHeading  = yawAngle;

  Serial.println("=== READY ===");
  Serial.println(">>> AUTO-STARTING IN 3 SECONDS... <<<");
  delay(3000);

  raceStarted    = true;
  currentState   = STATE_TRACKING;
  lastIMUTime    = millis();
  lastCameraTime = millis();
  lastYaw        = yawAngle;
  targetHeading  = yawAngle;
  Serial.println("+==========================+");
  Serial.println("|   RACE AUTO-STARTED!     |");
  Serial.println("+==========================+");
  digitalWrite(LED_PIN, HIGH);
}

// ============================================================
// 30.  LOOP
// ============================================================
void loop() {
  checkSerialCommands();
  if (raceFinished) { handleFinishState(); return; }

  // Fixed-period scheduler with guard against I2C hang accumulation.
  static unsigned long lastLoop = 0;
  unsigned long now = millis();
  if (now - lastLoop < LOOP_INTERVAL) return;
  if (now - lastLoop > 5 * LOOP_INTERVAL) lastLoop = now;
  else                                    lastLoop += LOOP_INTERVAL;

  checkEStop();
  if (estopActive) {
    currentState = STATE_SAFE_STOP;
    safeStop();
    return;
  }

  // STATE_INIT: wait for the start button, but still run sensors so
  // the operator can see live telemetry while arming.
  if (currentState == STATE_INIT) {
    handleStartButton();
    readCameraData();
    updateYaw();
    return;
  }

  // ===== Active race frame =====
  readCameraData();
  updateYaw();

  checkLapCountGyro();
  checkLapCountLine();
  checkLapCompletion();

  encLeft  = readEncoder(BUS_ENC_LEFT);
  encRight = readEncoder(BUS_ENC_RIGHT);
  updateOdometry();
  if (encoderFaultActive) {
    currentState = STATE_SAFE_STOP;
    return;
  }

  updateControl();
  setSteering(targetSteering);
  applySpeedRamp();

  // --- Telemetry (200 ms cadence) ---
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= PRINT_INTERVAL) {
    lastPrint = millis();
    Serial.print("Y:");    Serial.print(yawAngle, 1);
    Serial.print(" L:");   Serial.print(lapCount);
    Serial.print("(");     Serial.print(lineLapCount);
    Serial.print("/");     Serial.print((int)(fabsf(totalRotation) / LAP_DEGREES));
    Serial.print(") R:");  Serial.print(camRedDist);
    Serial.print(" G:");   Serial.print(camGreenDist);
    Serial.print(" M:");   Serial.print(camModeFlag);
    Serial.print(" t:");   Serial.print(targetSpeed);
    Serial.print(" v:");   Serial.print(commandSpeed);
    Serial.print(" s:");   Serial.print(targetSteering);
    Serial.print(" ");
    switch (currentState) {
      case STATE_INIT:       Serial.print("INIT"); break;
      case STATE_TRACKING:   Serial.print("TRK");  break;
      case STATE_BLIND_TURN: Serial.print("BLN");  break;
      case STATE_PARKING:    Serial.print("PRK");  Serial.print((int)parkPhase); break;
      case STATE_SAFE_STOP:  Serial.print("STP");  break;
      case STATE_FINISH:     Serial.print("FIN");  break;
    }
    Serial.println();
  }
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_EPS323
