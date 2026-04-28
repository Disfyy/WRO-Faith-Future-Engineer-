#if __has_include("wro_build_target.h")
#include "wro_build_target.h"
#else
#define WRO_TARGET_TEST_NO_SENSORS 4
#define WRO_ACTIVE_TARGET WRO_TARGET_TEST_NO_SENSORS
#endif

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_NO_SENSORS

// ================================================================
// WRO Future Engineers — Open-Loop (No Sensor) Track Test
// ================================================================
//
// Drives a timed lap script with NO sensors attached.
// Purpose: verify that the chassis, motor, servo, and power are
//          working before adding I2C sensors and camera.
//
// Script per lap (4 walls, each wall = 4 phases):
//   STRAIGHT → BRAKE → CORNER → EXIT  (×4 = 1 lap)
//
// Safety:
//   - E-Stop button on GPIO 32 (press = start, press again = stop)
//   - Serial "stop" command
//   - Speeds default LOW — the robot must NOT touch walls
//
// Tuning over Serial Monitor (115200 baud):
//   speed <N>   — straight speed  (default 50, range 30..90)
//   cspeed <N>  — corner speed    (default 38, range 25..70)
//   angle <N>   — corner angle    (default 35, range 15..44)
//   time <N>    — straight time   (default 2100, ms)
//   ctime <N>   — corner time     (default 1300, ms)
//   trim <N>    — steering trim   (default 0, range -10..10)
//   dir <1/-1>  — CW(+1) or CCW(-1)
//   laps <N>    — target laps     (default 3)
//   go          — start script
//   stop        — emergency stop
//   q           — print all parameters
//   h           — help
// ================================================================

#include <Arduino.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#define USE_LEDC_PIN_API 1
#else
#define USE_LEDC_PIN_API 0
#endif

// ======================== PINS (hardware-fixed) =================
constexpr int SERVO_PIN    = 27;  // JX PDI-6221MG steering servo
constexpr int MOTOR_R_EN   = 19;  // BTS7960 R_EN
constexpr int MOTOR_L_EN   = 23;  // BTS7960 L_EN
constexpr int MOTOR_R_PWM  = 5;   // BTS7960 R_PWM (forward)
constexpr int MOTOR_L_PWM  = 14;  // BTS7960 L_PWM (reverse)
constexpr int ESTOP_PIN    = 32;  // E-Stop button, INPUT_PULLUP, active LOW
constexpr int LED_PIN      = 2;

// ======================== SERVO ==================================
constexpr int SERVO_MIN_US     = 1215;
constexpr int SERVO_MAX_US     = 1915;
constexpr int SERVO_CENTER     = 90;
constexpr int SERVO_LEFT_LIMIT = 45;
constexpr int SERVO_RIGHT_LIMIT = 135;

// ======================== MOTOR ==================================
constexpr int PWM_CHANNEL_FWD        = 0;
constexpr int PWM_CHANNEL_REV        = 1;
constexpr int PWM_FREQ               = 5000;
constexpr int PWM_RES_BITS           = 8;
constexpr int MOTOR_MIN_EFFECTIVE_PWM = 70;   // below this the 380 motor stalls
constexpr int MOTOR_MAX_CMD          = 255;

// ======================== RAMP / TIMING ==========================
constexpr int SPEED_RAMP_STEP           = 4;     // gentler ramp than before
constexpr unsigned long CONTROL_PERIOD_MS = 20;   // 50 Hz control loop
constexpr unsigned long STATUS_PRINT_MS  = 500;
constexpr unsigned long DEBOUNCE_MS      = 250;   // E-Stop debounce

// ======================== TUNABLE DEFAULTS =======================
// All of these can be changed at runtime via serial commands.
// They are intentionally LOW — better to be slow and safe than fast
// and crashing.

int straightSpeed = 50;     // PWM command on straights   (30..90)
int brakeSpeed    = 30;     // PWM command during pre-corner braking
int cornerSpeed   = 38;     // PWM command during the turn (25..70)
int exitSpeed     = 40;     // PWM command after corner exit
int cornerAngle   = 35;     // degrees off center for the turn (15..44)
int straightTrim  = 0;      // small steering trim for straight drift (-10..+10)
int turnDir       = 1;      // +1 = CW (right turns), -1 = CCW (left turns)
int targetLaps    = 3;      // WRO Open Challenge = 3 laps

// Timing per phase (ms) — these are the main knobs to tweak per track.
int straightTimeMs = 2100;  // how long to drive straight (each wall)
int brakeTimeMs    = 400;   // slow-down window before the corner
int cornerTimeMs   = 1300;  // time spent turning
int exitTimeMs     = 350;   // post-corner recovery (straighten out)

// ======================== SCRIPT ENGINE ==========================
enum Phase : uint8_t {
  PH_STRAIGHT,
  PH_BRAKE,
  PH_CORNER,
  PH_EXIT
};
constexpr int PHASES_PER_WALL = 4;
constexpr int WALLS_PER_LAP   = 4;
constexpr int TOTAL_PHASES    = PHASES_PER_WALL * WALLS_PER_LAP;  // 16

bool  runActive      = false;
bool  runFinished     = false;
int   phaseIndex      = 0;     // 0..15 within a lap
int   completedLaps   = 0;
unsigned long phaseStartMs = 0;
unsigned long lastControlMs = 0;
unsigned long setupFinishedMs = 0;

int targetSteering   = SERVO_CENTER;
int targetSpeedCmd   = 0;
int appliedSpeedCmd  = 0;

// E-Stop state
bool  estopPrevState    = HIGH;
unsigned long lastEstopMs = 0;
bool  waitingForStart   = true;   // true until first button press+release

// ======================== HELPERS ================================

int clampAngle(int angle) {
  return constrain(angle, SERVO_LEFT_LIMIT, SERVO_RIGHT_LIMIT);
}

int speedCmdToPwm(int speedCmd) {
  speedCmd = constrain(speedCmd, -MOTOR_MAX_CMD, MOTOR_MAX_CMD);
  if (speedCmd == 0) return 0;
  int absCmd = abs(speedCmd);
  int pwm = map(absCmd, 1, MOTOR_MAX_CMD, MOTOR_MIN_EFFECTIVE_PWM, 255);
  pwm = constrain(pwm, MOTOR_MIN_EFFECTIVE_PWM, 255);
  return (speedCmd > 0) ? pwm : -pwm;
}

void setMotorPwm(int pwmSigned) {
  pwmSigned = constrain(pwmSigned, -255, 255);
  if (pwmSigned > 0) {
#if USE_LEDC_PIN_API
    ledcWrite(MOTOR_L_PWM, 0);
    ledcWrite(MOTOR_R_PWM, pwmSigned);
#else
    ledcWrite(PWM_CHANNEL_REV, 0);
    ledcWrite(PWM_CHANNEL_FWD, pwmSigned);
#endif
  } else if (pwmSigned < 0) {
#if USE_LEDC_PIN_API
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, -pwmSigned);
#else
    ledcWrite(PWM_CHANNEL_FWD, 0);
    ledcWrite(PWM_CHANNEL_REV, -pwmSigned);
#endif
  } else {
#if USE_LEDC_PIN_API
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
#else
    ledcWrite(PWM_CHANNEL_FWD, 0);
    ledcWrite(PWM_CHANNEL_REV, 0);
#endif
  }
}

void setServoAngle(int angle) {
  angle = clampAngle(angle);
  int us = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  uint32_t duty = (us * 16384UL) / 20000;
#if USE_LEDC_PIN_API
  ledcWrite(SERVO_PIN, duty);
#else
  ledcWrite(2, duty);
#endif
}

// ======================== START / STOP ===========================

void fullStop(const char *reason) {
  runActive = false;
  targetSpeedCmd  = 0;
  appliedSpeedCmd = 0;
  targetSteering  = SERVO_CENTER;
  setServoAngle(SERVO_CENTER);
  setMotorPwm(0);
  Serial.print("[STOP] ");
  Serial.println(reason);
}

void startScript() {
  runActive     = true;
  runFinished   = false;
  phaseIndex    = 0;
  completedLaps = 0;
  phaseStartMs  = 0;
  targetSpeedCmd  = 0;
  appliedSpeedCmd = 0;
  targetSteering  = SERVO_CENTER;
  Serial.println("[RUN] Script started");
}

// ======================== PHASE LOGIC ============================

Phase currentPhase() {
  int slot = phaseIndex % PHASES_PER_WALL;
  return static_cast<Phase>(slot);
}

int currentWall() {
  return phaseIndex / PHASES_PER_WALL;  // 0..3 = A..D
}

const char *phaseLabel() {
  static char buf[24];
  const char wallLetter = 'A' + currentWall();
  const char *phaseName;
  switch (currentPhase()) {
    case PH_STRAIGHT: phaseName = "STRAIGHT"; break;
    case PH_BRAKE:    phaseName = "BRAKE";    break;
    case PH_CORNER:   phaseName = "CORNER";   break;
    case PH_EXIT:     phaseName = "EXIT";     break;
    default:          phaseName = "?";        break;
  }
  snprintf(buf, sizeof(buf), "%s %c", phaseName, wallLetter);
  return buf;
}

unsigned long phaseDurationMs() {
  switch (currentPhase()) {
    case PH_STRAIGHT: return straightTimeMs;
    case PH_BRAKE:    return brakeTimeMs;
    case PH_CORNER:   return cornerTimeMs;
    case PH_EXIT:     return exitTimeMs;
  }
  return 0;
}

void setPhaseTargets() {
  int steerCenter = SERVO_CENTER + straightTrim;
  int steerTurn   = SERVO_CENTER + turnDir * cornerAngle;

  switch (currentPhase()) {
    case PH_STRAIGHT:
      targetSpeedCmd = straightSpeed;
      targetSteering = clampAngle(steerCenter);
      break;
    case PH_BRAKE:
      targetSpeedCmd = brakeSpeed;
      targetSteering = clampAngle(steerCenter);
      break;
    case PH_CORNER:
      targetSpeedCmd = cornerSpeed;
      targetSteering = clampAngle(steerTurn);
      break;
    case PH_EXIT:
      targetSpeedCmd = exitSpeed;
      targetSteering = clampAngle(steerCenter + turnDir * 5);
      break;
  }
}

void updateScript(unsigned long now) {
  if (!runActive) {
    targetSpeedCmd = 0;
    targetSteering = SERVO_CENTER;
    return;
  }

  if (completedLaps >= targetLaps) {
    runActive    = false;
    runFinished  = true;
    targetSpeedCmd = 0;
    targetSteering = SERVO_CENTER;
    Serial.println("[RUN] ALL LAPS COMPLETE");
    return;
  }

  if (phaseStartMs == 0) {
    setPhaseTargets();
    phaseStartMs = now;
    Serial.print("[RUN] Lap ");
    Serial.print(completedLaps + 1);
    Serial.print("/");
    Serial.print(targetLaps);
    Serial.print("  ");
    Serial.println(phaseLabel());
  }

  if (now - phaseStartMs >= phaseDurationMs()) {
    phaseIndex++;
    phaseStartMs = 0;

    if (phaseIndex >= TOTAL_PHASES) {
      phaseIndex = 0;
      completedLaps++;
      Serial.print("[RUN] Finished lap ");
      Serial.println(completedLaps);
    }
  }
}

// ======================== OUTPUT =================================

void applyOutputs() {
  int desiredSpeed = runActive ? targetSpeedCmd : 0;

  if (appliedSpeedCmd < desiredSpeed) {
    appliedSpeedCmd = min(appliedSpeedCmd + SPEED_RAMP_STEP, desiredSpeed);
  } else if (appliedSpeedCmd > desiredSpeed) {
    appliedSpeedCmd = max(appliedSpeedCmd - SPEED_RAMP_STEP, desiredSpeed);
  }

  setServoAngle(targetSteering);
  setMotorPwm(speedCmdToPwm(appliedSpeedCmd));
}

// ======================== E-STOP =================================

void handleEstop(unsigned long now) {
  bool pressed = (digitalRead(ESTOP_PIN) == LOW);

  if (pressed != estopPrevState && (now - lastEstopMs > DEBOUNCE_MS)) {
    lastEstopMs = now;
    estopPrevState = pressed;

    if (!pressed) {  // released edge
      if (waitingForStart) {
        waitingForStart = false;
        Serial.println("[BTN] Starting in 1.5 s ...");
        setupFinishedMs = now;
      } else if (runActive) {
        fullStop("E-Stop pressed");
      } else if (!runFinished) {
        startScript();
      }
    }
  }
}

// ======================== SERIAL COMMANDS =========================

void printHelp() {
  Serial.println();
  Serial.println("=== No-Sensor Track Test — Commands ===");
  Serial.println("go          start script");
  Serial.println("stop        emergency stop");
  Serial.println("q           show all parameters");
  Serial.println("h           this help");
  Serial.println("------- tuning (applied immediately) ------");
  Serial.println("speed <N>   straight speed  (30..90)");
  Serial.println("cspeed <N>  corner speed    (25..70)");
  Serial.println("angle <N>   corner angle    (15..44)");
  Serial.println("time <N>    straight time ms");
  Serial.println("ctime <N>   corner time ms");
  Serial.println("trim <N>    steering trim   (-10..10)");
  Serial.println("dir <1/-1>  CW=+1, CCW=-1");
  Serial.println("laps <N>    target laps     (1..10)");
  Serial.println();
}

void printParams() {
  Serial.println("--- Current Parameters ---");
  Serial.print("  straightSpeed = "); Serial.println(straightSpeed);
  Serial.print("  brakeSpeed    = "); Serial.println(brakeSpeed);
  Serial.print("  cornerSpeed   = "); Serial.println(cornerSpeed);
  Serial.print("  exitSpeed     = "); Serial.println(exitSpeed);
  Serial.print("  cornerAngle   = "); Serial.println(cornerAngle);
  Serial.print("  straightTrim  = "); Serial.println(straightTrim);
  Serial.print("  turnDir       = "); Serial.println(turnDir);
  Serial.print("  targetLaps    = "); Serial.println(targetLaps);
  Serial.println("--- Timing (ms) ---");
  Serial.print("  straightTime  = "); Serial.println(straightTimeMs);
  Serial.print("  brakeTime     = "); Serial.println(brakeTimeMs);
  Serial.print("  cornerTime    = "); Serial.println(cornerTimeMs);
  Serial.print("  exitTime      = "); Serial.println(exitTimeMs);
  Serial.print("  total/lap     = ");
  Serial.print((straightTimeMs + brakeTimeMs + cornerTimeMs + exitTimeMs) * 4);
  Serial.println(" ms");
  Serial.println("--- State ---");
  Serial.print("  run="); Serial.print(runActive ? "YES" : "NO");
  Serial.print("  lap="); Serial.print(completedLaps);
  Serial.print("/"); Serial.print(targetLaps);
  Serial.print("  phase="); Serial.print(phaseIndex);
  Serial.print("/"); Serial.print(TOTAL_PHASES);
  Serial.print("  vTarget="); Serial.print(targetSpeedCmd);
  Serial.print("  vApplied="); Serial.print(appliedSpeedCmd);
  Serial.print("  steer="); Serial.println(targetSteering);
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "h")    { printHelp();  return; }
  if (cmd == "go")   { startScript(); return; }
  if (cmd == "stop") { fullStop("serial command"); return; }
  if (cmd == "q")    { printParams(); return; }

  int spaceIdx = cmd.indexOf(' ');
  if (spaceIdx < 1) {
    Serial.println("Unknown command. Send 'h' for help.");
    return;
  }

  String key = cmd.substring(0, spaceIdx);
  int val    = cmd.substring(spaceIdx + 1).toInt();

  if (key == "speed")  { straightSpeed = constrain(val, 30, 90);
                         brakeSpeed    = max(straightSpeed - 20, 25);
                         exitSpeed     = max(straightSpeed - 10, 28);
                         Serial.print("straightSpeed="); Serial.println(straightSpeed); }
  else if (key == "cspeed") { cornerSpeed = constrain(val, 25, 70);
                              Serial.print("cornerSpeed="); Serial.println(cornerSpeed); }
  else if (key == "angle")  { cornerAngle = constrain(val, 15, 44);
                              Serial.print("cornerAngle="); Serial.println(cornerAngle); }
  else if (key == "time")   { straightTimeMs = constrain(val, 500, 5000);
                              Serial.print("straightTime="); Serial.println(straightTimeMs); }
  else if (key == "ctime")  { cornerTimeMs = constrain(val, 400, 3000);
                              Serial.print("cornerTime="); Serial.println(cornerTimeMs); }
  else if (key == "trim")   { straightTrim = constrain(val, -10, 10);
                              Serial.print("straightTrim="); Serial.println(straightTrim); }
  else if (key == "dir")    { turnDir = (val < 0) ? -1 : 1;
                              Serial.print("turnDir="); Serial.println(turnDir); }
  else if (key == "laps")   { targetLaps = constrain(val, 1, 10);
                              Serial.print("targetLaps="); Serial.println(targetLaps); }
  else { Serial.println("Unknown command. Send 'h' for help."); }
}

// ======================== SETUP ==================================

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  estopPrevState = digitalRead(ESTOP_PIN);

  // Servo on LEDC channel 2, 50 Hz, 14-bit resolution
#if USE_LEDC_PIN_API
  ledcAttach(SERVO_PIN, 50, 14);
#else
  ledcSetup(2, 50, 14);
  ledcAttachPin(SERVO_PIN, 2);
#endif
  setServoAngle(SERVO_CENTER);

  // Motor driver enable + PWM channels
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);

#if USE_LEDC_PIN_API
  ledcAttach(MOTOR_R_PWM, PWM_FREQ, PWM_RES_BITS);
  ledcAttach(MOTOR_L_PWM, PWM_FREQ, PWM_RES_BITS);
#else
  ledcSetup(PWM_CHANNEL_FWD, PWM_FREQ, PWM_RES_BITS);
  ledcSetup(PWM_CHANNEL_REV, PWM_FREQ, PWM_RES_BITS);
  ledcAttachPin(MOTOR_R_PWM, PWM_CHANNEL_FWD);
  ledcAttachPin(MOTOR_L_PWM, PWM_CHANNEL_REV);
#endif
  setMotorPwm(0);

  setupFinishedMs = millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println("  WRO No-Sensor Track Test  v2.0");
  Serial.println("========================================");
  Serial.println("Pins: servo=27, motor EN=19/23, PWM=5/14, E-Stop=32");
  Serial.println("Press E-Stop button or type 'go' to start.");
  Serial.println("Type 'q' to see parameters, 'h' for help.");
  Serial.println();
  printParams();
}

// ======================== LOOP ===================================

void loop() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  const unsigned long now = millis();

  handleEstop(now);

  // Auto-start 1.5 s after button press (or serial 'go')
  if (!waitingForStart && !runActive && !runFinished &&
      (now - setupFinishedMs >= 1500)) {
    startScript();
  }

  // Fixed-timestep control loop
  if (now - lastControlMs >= CONTROL_PERIOD_MS) {
    if (now - lastControlMs > 5 * CONTROL_PERIOD_MS) {
      lastControlMs = now;
    } else {
      lastControlMs += CONTROL_PERIOD_MS;
    }
    updateScript(now);
    applyOutputs();
  }

  // Status LED + periodic telemetry
  static unsigned long lastStatusMs = 0;
  if (now - lastStatusMs >= STATUS_PRINT_MS) {
    lastStatusMs = now;
    if (runActive) {
      digitalWrite(LED_PIN, (millis() / 250) & 1);  // blink while running
      Serial.print("[");
      Serial.print(phaseLabel());
      Serial.print("] v=");
      Serial.print(appliedSpeedCmd);
      Serial.print(" steer=");
      Serial.println(targetSteering);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_NO_SENSORS
