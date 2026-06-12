#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_BENCH_TEST

/*
 * WRO v13 — Full Bench Test (ESP32-S3)
 * Tests: IMU (I2C0), 2× AS5600 (I2C0/I2C1), 2× VL53L1X (I2C0/I2C1 with XSHUT),
 *        BTS7960 motor, JX servo, OpenMV camera, E-Stop.
 *
 * Serial commands:
 *   s  — full status print
 *   e  — toggle live mode (200ms)
 *   m  — motor forward 2s
 *   r  — motor reverse 2s
 *   l  — servo left
 *   c  — servo center
 *   k  — servo right
 *   h  — help
 */

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>

#include "wro_hw_config_v13.h"
#include "wro_config_v13.h"      // SERVO_*_SAFE_US + STEER_GAIN defaults
#include "wro_steering_comp.h"
#include "as5600_dual_i2c.h"
#include "vl53l1x_dual.h"

Adafruit_ICM20948 icm;
Servo steeringServo;

#define BENCH_CAM_BUF   96
#define BENCH_CMD_BUF   64

bool imuOK      = false;
bool encOK      = false;
bool liveMode   = false;
float benchGainL = STEER_GAIN_LEFT_DEFAULT;
float benchGainR = STEER_GAIN_RIGHT_DEFAULT;
unsigned long motorStopAt = 0;
char camLast[BENCH_CAM_BUF] = {0};
unsigned long camLastMs = 0;

void printHelp() {
  Serial.println("=== WRO v13 BENCH TEST ===");
  Serial.println("  s  full status  |  e  toggle live");
  Serial.println("  m  motor fwd 2s |  r  motor rev 2s");
  Serial.println("  l  servo left   |  c  center  |  k  right");
  Serial.println("  g  steer-gain check (table + sweep)");
  Serial.println("  L1.22 / R1.05    set steer asymmetry gains");
  Serial.println("  h  this help");
}

void motorFwd(int pwm, int ms) {
  ledcWrite(MOTOR_L_PWM, 0); ledcWrite(MOTOR_R_PWM, pwm);
  motorStopAt = millis() + ms;
  Serial.print("Motor FWD pwm="); Serial.print(pwm);
  Serial.print(" for "); Serial.print(ms); Serial.println("ms");
}

void motorRev(int pwm, int ms) {
  ledcWrite(MOTOR_R_PWM, 0); ledcWrite(MOTOR_L_PWM, pwm);
  motorStopAt = millis() + ms;
  Serial.print("Motor REV pwm="); Serial.print(pwm);
  Serial.print(" for "); Serial.print(ms); Serial.println("ms");
}

void motorOff() { ledcWrite(MOTOR_R_PWM, 0); ledcWrite(MOTOR_L_PWM, 0); motorStopAt = 0; }

void printStatus() {
  Serial.println("\n=== STATUS ===");

  // IMU
  Serial.print("IMU 0x68 (I2C0 GPIO"); Serial.print(I2C0_SDA);
  Serial.print("/"); Serial.print(I2C0_SCL); Serial.print("): ");
  if (imuOK) {
    sensors_event_t a, g, m, t;
    icm.getEvent(&a, &g, &t, &m);
    Serial.print("OK  gz="); Serial.print(g.gyro.z, 2);
    Serial.print(" ax="); Serial.println(a.acceleration.x, 2);
  } else { Serial.println("NOT FOUND"); }

  // Encoders (dual I2C)
  int rL = readEncoderLeft();
  int rR = readEncoderRight();
  Serial.print("AS5600 Left  (I2C0 0x36): ");
  if (rL >= 0) {
    Serial.print("OK  raw="); Serial.print(rL);
    Serial.print("  "); Serial.print(rL * 360.0f / AS5600_RESOLUTION, 1);
    Serial.println("°");
  } else { Serial.println("ERROR"); }
  Serial.print("AS5600 Right (I2C1 0x36): ");
  if (rR >= 0) {
    Serial.print("OK  raw="); Serial.print(rR);
    Serial.print("  "); Serial.print(rR * 360.0f / AS5600_RESOLUTION, 1);
    Serial.println("°");
  } else { Serial.println("ERROR"); }

  // VL53L1X
  Serial.print("VL53L1X Front (I2C0 0x30): ");
  if (tfFront.ok) { Serial.print("OK  "); Serial.print(tfFront.distMM); Serial.println("mm"); }
  else            { Serial.println("NOT FOUND"); }
  Serial.print("VL53L1X Side  (I2C1 0x31): ");
  if (tfSide.ok)  { Serial.print("OK  "); Serial.print(tfSide.distMM);  Serial.println("mm"); }
  else            { Serial.println("NOT FOUND"); }

  // Camera
  Serial.print("Camera (UART2): ");
  if (millis() - camLastMs < 1000) {
    Serial.print("receiving: ");
    Serial.println(camLast);
  } else {
    Serial.println("NO DATA");
  }

  // E-Stop
  Serial.print("E-Stop (GPIO"); Serial.print(ESTOP_PIN); Serial.print("): ");
  Serial.println(digitalRead(ESTOP_PIN) == LOW ? "PRESSED" : "released");
  Serial.println();
}

void printLive() {
  vl53_readAll();
  int rL = readEncoderLeft();
  int rR = readEncoderRight();
  Serial.print("L:"); Serial.print(rL >= 0 ? rL : -1);
  Serial.print(" R:"); Serial.print(rR >= 0 ? rR : -1);
  Serial.print(" F:"); Serial.print(tfFront.ok ? tfFront.distMM : -1); Serial.print("mm");
  Serial.print(" S:"); Serial.print(tfSide.ok  ? tfSide.distMM  : -1); Serial.print("mm");
  Serial.print(" E:"); Serial.print(digitalRead(ESTOP_PIN) == LOW ? "P" : "-");
  if (millis() - camLastMs < 500) { Serial.print(" CAM:"); Serial.print(camLast); }
  Serial.println();
}

// Same trim + clamp pipeline as writeSteeringUs() in wro_v13_main.cpp,
// so what you see on the bench is what the race firmware will command.
int steerComp(int us) {
  int comp = steer_compensate_us(us, benchGainL, benchGainR);
  return constrain(comp, SERVO_LEFT_SAFE_US, SERVO_RIGHT_SAFE_US);
}

void gainCheck() {
  Serial.println("\n=== STEER GAIN CHECK ===");
  Serial.printf("gains: L=%.3f  R=%.3f   (set with L1.22 / R1.05)\n",
                benchGainL, benchGainR);
  Serial.println("cmd_us -> comp_us (after SAFE clamp):");
  const int pct[3] = {25, 50, 100};
  for (int side = 0; side < 2; side++) {
    int lock = (side == 0) ? SERVO_LEFT_US : SERVO_RIGHT_US;
    Serial.println(side == 0 ? " LEFT:" : " RIGHT:");
    for (int i = 0; i < 3; i++) {
      int cmd = SERVO_CENTER_US + (lock - SERVO_CENTER_US) * pct[i] / 100;
      Serial.printf("  %3d%%  %4d -> %4d\n", pct[i], cmd, steerComp(cmd));
    }
  }
  Serial.println("sweep: mark the wheel angle at each lock to compare travel");
  steeringServo.writeMicroseconds(SERVO_CENTER_US);            delay(600);
  Serial.printf("  LEFT  lock -> %d us\n", steerComp(SERVO_LEFT_US));
  steeringServo.writeMicroseconds(steerComp(SERVO_LEFT_US));   delay(1500);
  steeringServo.writeMicroseconds(SERVO_CENTER_US);            delay(600);
  Serial.printf("  RIGHT lock -> %d us\n", steerComp(SERVO_RIGHT_US));
  steeringServo.writeMicroseconds(steerComp(SERVO_RIGHT_US));  delay(1500);
  steeringServo.writeMicroseconds(SERVO_CENTER_US);
  Serial.println("=== DONE — back to center ===\n");
}

void handleCmd(const char *cmd) {
  if      (strcmp(cmd, "s") == 0) printStatus();
  else if (strcmp(cmd, "m") == 0) motorFwd(120, 2000);
  else if (strcmp(cmd, "r") == 0) motorRev(120, 2000);
  else if (strcmp(cmd, "l") == 0) { steeringServo.writeMicroseconds(SERVO_LEFT_US);  Serial.println("Servo LEFT");   }
  else if (strcmp(cmd, "c") == 0) { steeringServo.writeMicroseconds(SERVO_CENTER_US);    Serial.println("Servo CENTER"); }
  else if (strcmp(cmd, "k") == 0) { steeringServo.writeMicroseconds(SERVO_RIGHT_US); Serial.println("Servo RIGHT");  }
  else if (strcmp(cmd, "e") == 0) { liveMode = !liveMode; Serial.print("Live: "); Serial.println(liveMode ? "ON" : "OFF"); }
  else if (strcmp(cmd, "g") == 0) gainCheck();
  else if ((cmd[0] == 'L' || cmd[0] == 'R') && cmd[1] != '\0') {
    // Same [0.5, 1.5] guard as the race firmware's live-tune commands.
    float v = constrain((float)atof(cmd + 1), 0.5f, 1.5f);
    if (cmd[0] == 'L') benchGainL = v; else benchGainR = v;
    Serial.printf("steer gain %c = %.3f\n", cmd[0], v);
  }
  else if (strcmp(cmd, "h") == 0) printHelp();
  else { Serial.print("Unknown: "); Serial.println(cmd); }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n+=======================================+");
  Serial.println("|   WRO v13 — BENCH TEST (ESP32-S3)     |");
  Serial.println("+=======================================+");

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // Bring up I2C0 and the IMU first; AS5600/VL53L1X drivers will ride on
  // the same Wire object.
  Wire.begin(I2C0_SDA, I2C0_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  imuOK = icm.begin_I2C(ICM20948_ADDRESS, &Wire);
  Serial.print("IMU: "); Serial.println(imuOK ? "OK" : "NOT FOUND");

  encOK = as5600_init();    // also calls Wire1.begin
  Serial.print("Enc init: "); Serial.println(encOK ? "OK" : "FAIL");
  Serial.print("Enc L: "); Serial.println(readEncoderLeft()  >= 0 ? "OK" : "NOT FOUND");
  Serial.print("Enc R: "); Serial.println(readEncoderRight() >= 0 ? "OK" : "NOT FOUND");

  vl53_initAll();

  Serial2.begin(CAMERA_BAUD, SERIAL_8N1, CAMERA_RX, CAMERA_TX);

  ledcAttach(MOTOR_R_PWM, 1000, 8);
  ledcAttach(MOTOR_L_PWM, 1000, 8);
  pinMode(MOTOR_R_EN, OUTPUT); digitalWrite(MOTOR_R_EN, HIGH);
  pinMode(MOTOR_L_EN, OUTPUT); digitalWrite(MOTOR_L_EN, HIGH);

  steeringServo.setPeriodHertz(50);
  steeringServo.attach(SERVO_PIN, SERVO_LEFT_US, SERVO_RIGHT_US);
  steeringServo.writeMicroseconds(SERVO_CENTER_US);

  Serial.println();
  printHelp();
}

void loop() {
  static char   inputBuf[BENCH_CMD_BUF] = {0};
  static size_t inputBufPos             = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBufPos > 0) {
        inputBuf[inputBufPos] = '\0';
        handleCmd(inputBuf);
        inputBufPos = 0;
      }
    } else if (inputBufPos < sizeof(inputBuf) - 1) {
      inputBuf[inputBufPos++] = c;
    }
    // else: silently drop chars past the buffer cap (oversize line ignored).
  }

  static char   uartBuf[BENCH_CAM_BUF] = {0};
  static size_t uartBufPos             = 0;
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      uartBuf[uartBufPos] = '\0';
      strncpy(camLast, uartBuf, sizeof(camLast) - 1);
      camLast[sizeof(camLast) - 1] = '\0';
      camLastMs = millis();
      uartBufPos = 0;
    } else if (c != '\r' && uartBufPos < sizeof(uartBuf) - 1) {
      uartBuf[uartBufPos++] = c;
    }
  }

  vl53_readAll();

  if (motorStopAt && millis() >= motorStopAt) { motorOff(); Serial.println("(motor stop)"); }

  static unsigned long lastLive = 0;
  if (liveMode && millis() - lastLive >= 200) { lastLive = millis(); printLive(); }

  static unsigned long lastBlink = 0;
  if (millis() - lastBlink >= 500) { lastBlink = millis(); digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_BENCH_TEST
