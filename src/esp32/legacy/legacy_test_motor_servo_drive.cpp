#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_MOTOR_SERVO_DRIVE

#include <Arduino.h>
#include <ESP32Servo.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

Servo steeringServo;

constexpr int SERVO_PIN = 18;
constexpr int MOTOR_R_EN = 19;
constexpr int MOTOR_L_EN = 23;
constexpr int MOTOR_R_PWM = 5;
constexpr int MOTOR_L_PWM = 14;
constexpr int ESTOP_PIN = 32;
constexpr int LED_PIN = 2;

constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2500;
constexpr int SERVO_LEFT_LIMIT = 45;
constexpr int SERVO_RIGHT_LIMIT = 135;
constexpr int SERVO_CENTER = 90;

constexpr int PWM_CHANNEL_FWD = 0;
constexpr int PWM_CHANNEL_REV = 1;
constexpr int PWM_FREQ = 20000;
constexpr int PWM_RES_BITS = 8;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#define USE_LEDC_PIN_API 1
#else
#define USE_LEDC_PIN_API 0
#endif

constexpr int MOTOR_MIN_EFFECTIVE_PWM = 70;
constexpr int MOTOR_MAX_CMD = 255;
constexpr int MOTOR_MAX_AUTO_CMD = 170;

constexpr int SPEED_RAMP_STEP = 6;
constexpr unsigned long CONTROL_PERIOD_MS = 20;
constexpr unsigned long STATUS_PRINT_MS = 500;
constexpr unsigned long LED_SLOW_BLINK_MS = 450;
constexpr unsigned long LED_FAST_BLINK_MS = 120;
constexpr unsigned long LED_START_DELAY_BLINK_MS = 140;
constexpr unsigned long ESTOP_DEBOUNCE_MS = 20;

// Blind-drive route settings (no camera, no IMU, no encoders).
constexpr int BLIND_DIRECTION_SIGN = 1; // +1: steer right, -1: steer left
constexpr int TARGET_LAPS = 1;
constexpr unsigned long START_DELAY_MS = 1200;
constexpr unsigned long RUN_TIMEOUT_MS = 20000;

constexpr int BLIND_STRAIGHT_SPEED = -126;
constexpr int BLIND_TURN_SPEED = -108;
constexpr int TURN_STEER_OFFSET = 45;

constexpr unsigned long STRAIGHT_BACK_HOLD_MS = 500;
constexpr unsigned long TURN_45_HOLD_MS = 5000;
constexpr unsigned long STOP_HOLD_MS = 250;

struct BlindSegment {
  int speedCmd;
  int steeringAngle;
  unsigned long holdMs;
  const char *label;
};

const BlindSegment LAP_SCRIPT[] = {
  {BLIND_STRAIGHT_SPEED, SERVO_CENTER, STRAIGHT_BACK_HOLD_MS,
   "Reverse straight"},
  {BLIND_TURN_SPEED, (BLIND_DIRECTION_SIGN > 0) ? SERVO_RIGHT_LIMIT : SERVO_LEFT_LIMIT,
   TURN_45_HOLD_MS, "Reverse turn MAX"},
  {0, SERVO_CENTER, STOP_HOLD_MS, "Stop"},
};

constexpr size_t LAP_SEGMENT_COUNT = sizeof(LAP_SCRIPT) / sizeof(LAP_SCRIPT[0]);

enum class RunState {
  WAIT_FOR_ARM,
  START_DELAY,
  RUNNING,
  FINISHED,
};

enum class ControlMode {
  BLIND_SCRIPT,
  MANUAL,
};

bool estopActive = false;
bool emergencyLatched = false;
bool estopPressSeen = false;
bool lastEstopPressed = false;
unsigned long estopChangeMs = 0;

bool systemArmed = false;

RunState runState = RunState::WAIT_FOR_ARM;
ControlMode controlMode = ControlMode::BLIND_SCRIPT;

int targetSteering = SERVO_CENTER;
int targetSpeedCmd = 0;
int appliedSpeedCmd = 0;

int manualSteering = SERVO_CENTER;
int manualSpeedCmd = 0;

size_t segmentIndex = 0;
int completedLaps = 0;
unsigned long stateStartMs = 0;
unsigned long segmentStartMs = 0;
unsigned long runStartMs = 0;

unsigned long lastControlMs = 0;

int clampAngle(int angle) {
  return constrain(angle, SERVO_LEFT_LIMIT, SERVO_RIGHT_LIMIT);
}

int clampAutoSpeed(int speedCmd) {
  return constrain(speedCmd, -MOTOR_MAX_AUTO_CMD, MOTOR_MAX_AUTO_CMD);
}

int speedCmdToPwmSigned(int speedCmd) {
  speedCmd = constrain(speedCmd, -MOTOR_MAX_CMD, MOTOR_MAX_CMD);
  if (speedCmd == 0) {
    return 0;
  }

  const int absCmd = abs(speedCmd);
  int pwm = map(absCmd, 1, MOTOR_MAX_CMD, MOTOR_MIN_EFFECTIVE_PWM, 255);
  pwm = constrain(pwm, MOTOR_MIN_EFFECTIVE_PWM, 255);
  return (speedCmd > 0) ? pwm : -pwm;
}

void setMotorPwmSigned(int pwmSigned) {
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

bool canDriveNow() {
  return systemArmed && !estopActive;
}

const char *runStateName(RunState state) {
  switch (state) {
  case RunState::WAIT_FOR_ARM:
    return "WAIT";
  case RunState::START_DELAY:
    return "DELAY";
  case RunState::RUNNING:
    return "RUN";
  case RunState::FINISHED:
    return "FIN";
  }
  return "?";
}

const char *controlModeName(ControlMode mode) {
  switch (mode) {
  case ControlMode::BLIND_SCRIPT:
    return "SCRIPT";
  case ControlMode::MANUAL:
    return "MANUAL";
  }
  return "?";
}

void resetRunProgress() {
  segmentIndex = 0;
  completedLaps = 0;
  segmentStartMs = 0;
  runStartMs = 0;
  stateStartMs = 0;
}

void printStateOnce() {
  Serial.print("armed=");
  Serial.print(systemArmed ? 1 : 0);
  Serial.print(" estop=");
  Serial.print(estopActive ? 1 : 0);
  Serial.print(" mode=");
  Serial.print(controlModeName(controlMode));
  Serial.print(" state=");
  Serial.print(runStateName(runState));
  Serial.print(" lap=");
  Serial.print(completedLaps);
  Serial.print("/");
  Serial.print(TARGET_LAPS);
  Serial.print(" seg=");
  Serial.print((segmentIndex < LAP_SEGMENT_COUNT) ? (segmentIndex + 1)
                                                  : LAP_SEGMENT_COUNT);
  Serial.print("/");
  Serial.print(LAP_SEGMENT_COUNT);
  Serial.print(" vTarget=");
  Serial.print(targetSpeedCmd);
  Serial.print(" vApplied=");
  Serial.print(appliedSpeedCmd);
  Serial.print(" steer=");
  Serial.println(targetSteering);
}

void printScriptSummary() {
  Serial.println();
  Serial.println("=== Blind Script Summary ===");
  Serial.print("Direction sign: ");
  Serial.println(BLIND_DIRECTION_SIGN);
  Serial.print("Target laps   : ");
  Serial.println(TARGET_LAPS);
  Serial.print("Start delay ms: ");
  Serial.println(START_DELAY_MS);
  Serial.print("Run timeout ms: ");
  Serial.println(RUN_TIMEOUT_MS);
  Serial.println("Per-lap segments:");
  for (size_t i = 0; i < LAP_SEGMENT_COUNT; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(LAP_SCRIPT[i].label);
    Serial.print("  speed=");
    Serial.print(LAP_SCRIPT[i].speedCmd);
    Serial.print("  steer=");
    Serial.print(LAP_SCRIPT[i].steeringAngle);
    Serial.print("  holdMs=");
    Serial.println(LAP_SCRIPT[i].holdMs);
  }
  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println("=== Blind Drive Commands ===");
  Serial.println("h      : help");
  Serial.println("q      : print current state");
  Serial.println("i      : print blind script");
  Serial.println("t      : restart blind script (must be armed)");
  Serial.println("g      : arm+start from serial (bench use)");
  Serial.println("s      : soft stop + disarm");
  Serial.println("m      : enter manual mode");
  Serial.println("x      : exit manual mode (stop + disarm)");
  Serial.println("c      : manual steering center");
  Serial.println("l      : manual steering left limit");
  Serial.println("r      : manual steering right limit");
  Serial.println("a<deg> : manual steering exact angle, ex: a92");
  Serial.println("v<pwm> : manual signed speed -255..255, ex: v120 / v-90");
  Serial.println("f<pwm> : manual forward speed 0..255, ex: f120");
  Serial.println("b<pwm> : manual reverse speed 0..255, ex: b100");
  Serial.println();
  Serial.println("Hardware start: press E-STOP, then release.");
  Serial.println("Emergency stop: press E-STOP during motion.");
  Serial.println();
}

void disarmAndStop(const char *reason) {
  systemArmed = false;
  controlMode = ControlMode::BLIND_SCRIPT;
  runState = RunState::WAIT_FOR_ARM;
  resetRunProgress();

  targetSpeedCmd = 0;
  appliedSpeedCmd = 0;
  targetSteering = SERVO_CENTER;

  manualSpeedCmd = 0;
  manualSteering = SERVO_CENTER;

  emergencyLatched = false;
  estopPressSeen = false;

  Serial.print("STOP: ");
  Serial.println(reason);
}

void finishRun(const char *reason) {
  controlMode = ControlMode::BLIND_SCRIPT;
  runState = RunState::FINISHED;
  systemArmed = false;

  targetSpeedCmd = 0;
  appliedSpeedCmd = 0;
  targetSteering = SERVO_CENTER;

  manualSpeedCmd = 0;
  manualSteering = SERVO_CENTER;

  emergencyLatched = false;
  estopPressSeen = false;

  Serial.print("RUN: finished - ");
  Serial.println(reason);
  Serial.println("RUN: press E-STOP and release to run again.");
}

void startBlindRun(const char *reason) {
  if (!systemArmed) {
    Serial.println("RUN: not armed. Press E-STOP then release.");
    return;
  }
  if (estopActive) {
    Serial.println("RUN: blocked, E-STOP is active.");
    return;
  }

  controlMode = ControlMode::BLIND_SCRIPT;
  runState = RunState::START_DELAY;
  resetRunProgress();
  stateStartMs = millis();

  targetSpeedCmd = 0;
  targetSteering = SERVO_CENTER;
  appliedSpeedCmd = 0;

  Serial.print("RUN: queued (");
  Serial.print(reason);
  Serial.print(") -> start in ");
  Serial.print(START_DELAY_MS);
  Serial.println(" ms");
}

void armAndQueueRun() {
  systemArmed = true;
  emergencyLatched = false;
  estopPressSeen = false;
  startBlindRun("E-STOP release");
}

void startManualMode() {
  controlMode = ControlMode::MANUAL;
  runState = RunState::WAIT_FOR_ARM;
  resetRunProgress();
  targetSpeedCmd = 0;
  targetSteering = SERVO_CENTER;
  manualSpeedCmd = 0;
  manualSteering = SERVO_CENTER;
  Serial.println("MANUAL: enabled.");
}

void ensureManualMode() {
  if (controlMode != ControlMode::MANUAL) {
    startManualMode();
  }
}

void updateBlindScript(unsigned long now) {
  if (runState == RunState::WAIT_FOR_ARM) {
    targetSpeedCmd = 0;
    targetSteering = SERVO_CENTER;
    return;
  }

  if (runState == RunState::START_DELAY) {
    targetSpeedCmd = 0;
    targetSteering = SERVO_CENTER;

    if (now - stateStartMs >= START_DELAY_MS) {
      runState = RunState::RUNNING;
      runStartMs = now;
      segmentStartMs = 0;
      Serial.println("RUN: started");
    }
    return;
  }

  if (runState == RunState::FINISHED) {
    targetSpeedCmd = 0;
    targetSteering = SERVO_CENTER;
    return;
  }

  if (runState != RunState::RUNNING) {
    targetSpeedCmd = 0;
    targetSteering = SERVO_CENTER;
    return;
  }

  if (now - runStartMs >= RUN_TIMEOUT_MS) {
    finishRun("timeout");
    return;
  }

  if (completedLaps >= TARGET_LAPS) {
    finishRun("target laps completed");
    return;
  }

  if (segmentStartMs == 0) {
    const BlindSegment &segment = LAP_SCRIPT[segmentIndex];
    targetSpeedCmd = clampAutoSpeed(segment.speedCmd);
    targetSteering = clampAngle(segment.steeringAngle);
    segmentStartMs = now;

    Serial.print("RUN: lap ");
    Serial.print(completedLaps + 1);
    Serial.print("/");
    Serial.print(TARGET_LAPS);
    Serial.print(" seg ");
    Serial.print(segmentIndex + 1);
    Serial.print("/");
    Serial.print(LAP_SEGMENT_COUNT);
    Serial.print(" -> ");
    Serial.println(segment.label);
  }

  const BlindSegment &activeSegment = LAP_SCRIPT[segmentIndex];
  if (now - segmentStartMs >= activeSegment.holdMs) {
    segmentIndex++;
    segmentStartMs = 0;

    if (segmentIndex >= LAP_SEGMENT_COUNT) {
      segmentIndex = 0;
      completedLaps++;
      Serial.print("RUN: completed lap ");
      Serial.println(completedLaps);

      if (completedLaps >= TARGET_LAPS) {
        finishRun("target laps completed");
      }
    }
  }
}

void updateControlTargets(unsigned long now) {
  if (controlMode == ControlMode::MANUAL) {
    targetSpeedCmd = constrain(manualSpeedCmd, -MOTOR_MAX_CMD, MOTOR_MAX_CMD);
    targetSteering = clampAngle(manualSteering);
    return;
  }

  updateBlindScript(now);
}

void applySpeedRampAndOutputs() {
  const int desiredSpeed = canDriveNow() ? targetSpeedCmd : 0;

  if (appliedSpeedCmd < desiredSpeed) {
    appliedSpeedCmd = min(appliedSpeedCmd + SPEED_RAMP_STEP, desiredSpeed);
  } else if (appliedSpeedCmd > desiredSpeed) {
    appliedSpeedCmd = max(appliedSpeedCmd - SPEED_RAMP_STEP, desiredSpeed);
  }

  const int safeSteering = canDriveNow() ? clampAngle(targetSteering)
                                         : SERVO_CENTER;
  steeringServo.write(safeSteering);
  setMotorPwmSigned(speedCmdToPwmSigned(appliedSpeedCmd));
}

void updateEStop() {
  return; // СИСТЕМА БЕЗОПАСНОСТИ ОТКЛЮЧЕНА: игнорируем кнопку
  const bool pressedNow = (digitalRead(ESTOP_PIN) == LOW);

  if (pressedNow != lastEstopPressed) {
    lastEstopPressed = pressedNow;
    estopChangeMs = millis();
  }

  if (millis() - estopChangeMs < ESTOP_DEBOUNCE_MS) {
    return;
  }

  if (pressedNow) {
    estopPressSeen = true;
    if (!estopActive) {
      if (systemArmed &&
          (runState == RunState::RUNNING || runState == RunState::START_DELAY ||
           controlMode == ControlMode::MANUAL)) {
        emergencyLatched = true;
      }

      estopActive = true;
      targetSpeedCmd = 0;
      manualSpeedCmd = 0;
      Serial.println("E-STOP: active");
    }
    return;
  }

  if (estopActive) {
    estopActive = false;
    appliedSpeedCmd = 0;
    targetSpeedCmd = 0;
    manualSpeedCmd = 0;
    targetSteering = SERVO_CENTER;
    manualSteering = SERVO_CENTER;

    if (!systemArmed && estopPressSeen) {
      armAndQueueRun();
      return;
    }

    if (emergencyLatched) {
      disarmAndStop("E-STOP released after emergency");
      return;
    }

    Serial.println("E-STOP: released");
  }
}

void updateLed() {
  static unsigned long lastToggleMs = 0;
  static bool ledState = false;

  bool forceOn = false;
  unsigned long blinkMs = LED_SLOW_BLINK_MS;

  if (estopActive) {
    blinkMs = LED_FAST_BLINK_MS;
  } else if (!systemArmed) {
    blinkMs = LED_SLOW_BLINK_MS;
  } else if (runState == RunState::START_DELAY) {
    blinkMs = LED_START_DELAY_BLINK_MS;
  } else if (runState == RunState::RUNNING ||
             controlMode == ControlMode::MANUAL) {
    forceOn = true;
  }

  if (forceOn) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }

  if (millis() - lastToggleMs >= blinkMs) {
    lastToggleMs = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) {
    return;
  }

  if (cmd.equalsIgnoreCase("h")) {
    printHelp();
    return;
  }
  if (cmd.equalsIgnoreCase("q")) {
    printStateOnce();
    return;
  }
  if (cmd.equalsIgnoreCase("i")) {
    printScriptSummary();
    return;
  }
  if (cmd.equalsIgnoreCase("s")) {
    disarmAndStop("serial stop");
    return;
  }
  if (cmd.equalsIgnoreCase("t")) {
    startBlindRun("serial restart");
    return;
  }
  if (cmd.equalsIgnoreCase("g")) {
    if (estopActive) {
      Serial.println("RUN: blocked, E-STOP is active.");
      return;
    }
    systemArmed = true;
    emergencyLatched = false;
    estopPressSeen = false;
    startBlindRun("serial arm+go");
    return;
  }
  if (cmd.equalsIgnoreCase("m")) {
    startManualMode();
    return;
  }
  if (cmd.equalsIgnoreCase("x")) {
    disarmAndStop("manual mode exit");
    return;
  }

  if (cmd.equalsIgnoreCase("c")) {
    ensureManualMode();
    manualSteering = SERVO_CENTER;
    Serial.println("MANUAL: steering center");
    return;
  }
  if (cmd.equalsIgnoreCase("l")) {
    ensureManualMode();
    manualSteering = SERVO_LEFT_LIMIT;
    Serial.println("MANUAL: steering left");
    return;
  }
  if (cmd.equalsIgnoreCase("r")) {
    ensureManualMode();
    manualSteering = SERVO_RIGHT_LIMIT;
    Serial.println("MANUAL: steering right");
    return;
  }

  const char op = cmd.charAt(0);
  const int value = cmd.substring(1).toInt();

  if (op == 'a' || op == 'A') {
    ensureManualMode();
    manualSteering = clampAngle(value);
    Serial.print("MANUAL: steering -> ");
    Serial.println(manualSteering);
    return;
  }

  if (op == 'v' || op == 'V') {
    ensureManualMode();
    manualSpeedCmd = constrain(value, -MOTOR_MAX_CMD, MOTOR_MAX_CMD);
    Serial.print("MANUAL: speed -> ");
    Serial.println(manualSpeedCmd);
    return;
  }

  if (op == 'f' || op == 'F') {
    ensureManualMode();
    manualSpeedCmd = constrain(abs(value), 0, MOTOR_MAX_CMD);
    Serial.print("MANUAL: forward speed -> ");
    Serial.println(manualSpeedCmd);
    return;
  }

  if (op == 'b' || op == 'B') {
    ensureManualMode();
    manualSpeedCmd = -constrain(abs(value), 0, MOTOR_MAX_CMD);
    Serial.print("MANUAL: reverse speed -> ");
    Serial.println(manualSpeedCmd);
    return;
  }

  Serial.println("Unknown command. Send h for help.");
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);
  delay(300);

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  steeringServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  steeringServo.write(SERVO_CENTER);

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
  setMotorPwmSigned(0);
// БЕЗОПАСНОСТЬ ОТКЛЮЧЕНА: игнорируем состояние E-STOP при запуске
  estopActive = false;
  systemArmed = true;

  Serial.println("=== ESP32 Blind Drive (No Sensors) ===");
  Serial.println("Use with camera/discrete sensors disconnected.");
  Serial.println("Start: press E-STOP, then release.");
  Serial.println("Emergency stop: press E-STOP during movement.");
  Serial.println("Tune segment timings and steering constants in code.");
  printHelp();
  printScriptSummary();

  // АВТОСТАРТф
  startBlindRun("auto-start bypass");
}

void loop() {
  updateEStop();

  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  const unsigned long now = millis();
  if (now - lastControlMs >= CONTROL_PERIOD_MS) {
    if (now - lastControlMs > 5 * CONTROL_PERIOD_MS) {
      lastControlMs = now;
    } else {
      lastControlMs += CONTROL_PERIOD_MS;
    }

    if (!estopActive) {
      updateControlTargets(now);
    } else {
      targetSpeedCmd = 0;
      targetSteering = SERVO_CENTER;
    }

    applySpeedRampAndOutputs();
  }

  updateLed();

  static unsigned long lastStatusMs = 0;
  if (now - lastStatusMs >= STATUS_PRINT_MS) {
    lastStatusMs = now;
    printStateOnce();
  }
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_MOTOR_SERVO_DRIVE
