// =====================================================================
//  WRO v13 BENCH TEST — bring-up sketch for the whole robot on a bench
//  BEFORE you upload: LIFT THE CHASSIS so the wheels DO NOT touch the floor!
// =====================================================================
//
//  This sketch exercises:
//    1. Both AS5600 encoders (Wire on I2C0 + Wire1 on I2C1)
//    2. ICM-20948 IMU (yaw / gyro Z)
//    3. Both VL53L1X ToF sensors (front + side, with XSHUT addr remap)
//    4. Steering servo (JX PDI-6221MG)
//    5. BTS7960 motor driver (forward / reverse)
//    6. E-Stop button
//    7. OpenMV camera UART feed
//
//  Serial Monitor (115200 baud) commands:
//    h        - help
//    s        - one-shot status print of every sensor
//    live     - live readings every 200 ms (toggle)
//    sl       - servo LEFT
//    sc       - servo CENTER
//    sr       - servo RIGHT
//    f        - motor FORWARD 1 second @ PWM 80
//    b        - motor REVERSE 1 second @ PWM 80
//    fast     - motor FORWARD 2 seconds @ PWM 150
//    stop     - emergency stop (motor off)
//
//  Bring-up procedure:
//    1. Lift the chassis on a box; wheels in the air.
//    2. Upload this sketch.
//    3. Open Serial Monitor at 115200.
//    4. Type `s` once -> see all sensor states.
//    5. Type `live` -> rolling status. Spin a wheel to see encoder ticks
//       change; rotate the chassis by hand to see gyro Z move.
//    6. Wave your hand near each VL53L1X to see distance change.
//    7. `sl` / `sc` / `sr` to sweep the servo.
//    8. `f` to spin the motor (wheels are still in the air, right?).
// =====================================================================

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <VL53L1X.h>     // Pololu library

// ---- Pins (v13 — ESP32-S3-DevKitC-1 N8R8) ----
#define I2C0_SDA       8
#define I2C0_SCL       9
#define I2C1_SDA       11
#define I2C1_SCL       12
#define VL53_FRONT_XSHUT  15
#define VL53_SIDE_XSHUT   16
#define CAM_RX         17
#define CAM_TX         18
#define MOTOR_R_EN     38
#define MOTOR_L_EN     39
#define MOTOR_R_PWM    40
#define MOTOR_L_PWM    41
#define SERVO_PIN      42
#define ESTOP_PIN      21
#define LED_PIN        2

// ---- I2C addresses ----
#define AS5600_ADDR        0x36    // both encoders, on different buses
#define ICM20948_ADDR      0x68    // AD0 = GND
#define VL53_DEFAULT_ADDR  0x29
#define VL53_FRONT_ADDR    0x30
#define VL53_SIDE_ADDR     0x31

// ---- AS5600 registers ----
#define REG_RAW_ANGLE  0x0E
#define REG_STATUS     0x0B
#define REG_AGC        0x1A

// ---- Servo calibration (datasheet defaults — re-measure on the chassis) ----
#define SERVO_CENTER_US  1500
#define SERVO_LEFT_US    1000
#define SERVO_RIGHT_US   2000

// ---- Motor PWM ----
#define PWM_FREQ  20000
#define PWM_RES   8

Servo steeringServo;
Adafruit_ICM20948 icm;
VL53L1X tofFront;
VL53L1X tofSide;

bool icmOK       = false;
bool encLeftOK   = false;
bool encRightOK  = false;
bool tofFrontOK  = false;
bool tofSideOK   = false;
bool liveMode    = false;
unsigned long motorStopAt = 0;
String lastCameraMsg      = "no data";
unsigned long lastCameraMs = 0;

// =======================================================
//  AS5600 helpers
// =======================================================
int readAS5600Angle(TwoWire &bus) {
  bus.beginTransmission(AS5600_ADDR);
  bus.write(REG_RAW_ANGLE);
  if (bus.endTransmission(false) != 0) return -1;
  if (bus.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) >= 2) {
    int h = bus.read();
    int l = bus.read();
    return ((h & 0x0F) << 8) | l;
  }
  return -1;
}

String readAS5600Magnet(TwoWire &bus) {
  bus.beginTransmission(AS5600_ADDR);
  bus.write(REG_STATUS);
  if (bus.endTransmission(false) != 0) return "I2C ERR";
  if (bus.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1) < 1) return "NO RESP";
  uint8_t s = bus.read();
  if (!(s & 0x20)) return "NO MAGNET!";
  if   (s & 0x10)  return "TOO WEAK";
  if   (s & 0x08)  return "TOO STRONG";
  return "OK";
}

bool checkI2C(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return (bus.endTransmission() == 0);
}

// =======================================================
//  Motor
// =======================================================
void motorStop() {
  ledcWrite(MOTOR_R_PWM, 0);
  ledcWrite(MOTOR_L_PWM, 0);
  motorStopAt = 0;
}

void motorRun(int speed, int durationMs) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    ledcWrite(MOTOR_L_PWM, 0);
    ledcWrite(MOTOR_R_PWM, speed);
  } else if (speed < 0) {
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, -speed);
  } else {
    motorStop();
    return;
  }
  motorStopAt = millis() + durationMs;
}

// =======================================================
//  Bus scan
// =======================================================
void scanBus(TwoWire &bus, const char* name) {
  Serial.print("  ");
  Serial.print(name);
  Serial.print(": ");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      if (found > 0) Serial.print(", ");
      Serial.print("0x");
      if (a < 16) Serial.print("0");
      Serial.print(a, HEX);
      found++;
    }
  }
  if (found == 0) Serial.print("(empty)");
  Serial.println();
}

// =======================================================
//  Status print
// =======================================================
void printStatus() {
  Serial.println("\n========== STATUS ==========");

  Serial.println("I2C scan:");
  scanBus(Wire,  "I2C0 (Wire) ");
  scanBus(Wire1, "I2C1 (Wire1)");

  Serial.println("\nEncoders:");
  int rawL = readAS5600Angle(Wire);
  int rawR = readAS5600Angle(Wire1);
  Serial.print("  Left  AS5600: ");
  if (rawL >= 0) {
    Serial.print(rawL); Serial.print(" raw, ");
    Serial.print(rawL * 360.0 / 4096.0, 1); Serial.print(" deg, magnet=");
    Serial.println(readAS5600Magnet(Wire));
  } else Serial.println("NO RESPONSE");

  Serial.print("  Right AS5600: ");
  if (rawR >= 0) {
    Serial.print(rawR); Serial.print(" raw, ");
    Serial.print(rawR * 360.0 / 4096.0, 1); Serial.print(" deg, magnet=");
    Serial.println(readAS5600Magnet(Wire1));
  } else Serial.println("NO RESPONSE");

  Serial.print("\nIMU ICM-20948: ");
  if (icmOK) {
    sensors_event_t a, g, m, t;
    icm.getEvent(&a, &g, &t, &m);
    Serial.print("OK  gyroZ=");
    Serial.print(g.gyro.z, 3);
    Serial.print(" rad/s  accelX=");
    Serial.print(a.acceleration.x, 2);
    Serial.println();
  } else Serial.println("NOT FOUND");

  Serial.print("\nVL53L1X Front (I2C0 0x30): ");
  if (tofFrontOK) {
    uint16_t mm = tofFront.read(false);
    if (tofFront.timeoutOccurred()) Serial.println("TIMEOUT");
    else { Serial.print(mm); Serial.println(" mm"); }
  } else Serial.println("NOT FOUND");

  Serial.print("VL53L1X Side  (I2C1 0x31): ");
  if (tofSideOK) {
    uint16_t mm = tofSide.read(false);
    if (tofSide.timeoutOccurred()) Serial.println("TIMEOUT");
    else { Serial.print(mm); Serial.println(" mm"); }
  } else Serial.println("NOT FOUND");

  Serial.print("\nE-Stop button (GPIO");
  Serial.print(ESTOP_PIN); Serial.print("): ");
  Serial.println(digitalRead(ESTOP_PIN) ? "released" : "PRESSED");

  Serial.print("\nCamera (OpenMV UART2): ");
  if (millis() - lastCameraMs < 1000) {
    Serial.print("OK. Last frame: ");
    Serial.println(lastCameraMsg);
  } else {
    Serial.println("NO DATA (>1s timeout)");
  }

  Serial.println("============================\n");
}

// =======================================================
//  Live (toggle with `live`)
// =======================================================
void printLive() {
  int rawL = readAS5600Angle(Wire);
  int rawR = readAS5600Angle(Wire1);

  Serial.print("ENC L:");
  Serial.print(rawL >= 0 ? rawL : -1);
  Serial.print(" R:");
  Serial.print(rawR >= 0 ? rawR : -1);

  if (icmOK) {
    sensors_event_t a, g, m, t;
    icm.getEvent(&a, &g, &t, &m);
    Serial.print("  GYRO z:");
    Serial.print(g.gyro.z, 2);
  }

  Serial.print("  ESTOP:");
  Serial.print(digitalRead(ESTOP_PIN) ? "0" : "1");

  Serial.print("  TOF F:");
  if (tofFrontOK && tofFront.dataReady()) {
    Serial.print(tofFront.read(false)); Serial.print("mm");
  } else { Serial.print("---"); }

  Serial.print(" S:");
  if (tofSideOK && tofSide.dataReady()) {
    Serial.print(tofSide.read(false)); Serial.print("mm");
  } else { Serial.print("---"); }

  Serial.print("  CAM:");
  Serial.print(millis() - lastCameraMs < 500 ? "V" : "X");

  Serial.println();
}

// =======================================================
//  Commands
// =======================================================
void printHelp() {
  Serial.println("\n--- COMMANDS ---");
  Serial.println("  h     - help");
  Serial.println("  s     - status (one-shot)");
  Serial.println("  live  - live status (toggle)");
  Serial.println("  sl    - servo LEFT");
  Serial.println("  sc    - servo CENTER");
  Serial.println("  sr    - servo RIGHT");
  Serial.println("  f     - motor FORWARD 1s @ PWM 80");
  Serial.println("  b     - motor REVERSE 1s @ PWM 80");
  Serial.println("  fast  - motor FORWARD 2s @ PWM 150");
  Serial.println("  stop  - emergency stop");
  Serial.println();
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if      (cmd == "h" || cmd == "help") printHelp();
  else if (cmd == "s")    printStatus();
  else if (cmd == "live") {
    liveMode = !liveMode;
    Serial.println(liveMode ? "live: ON" : "live: OFF");
  }
  else if (cmd == "sl") {
    steeringServo.writeMicroseconds(SERVO_LEFT_US);
    Serial.println("servo: LEFT");
  }
  else if (cmd == "sc") {
    steeringServo.writeMicroseconds(SERVO_CENTER_US);
    Serial.println("servo: CENTER");
  }
  else if (cmd == "sr") {
    steeringServo.writeMicroseconds(SERVO_RIGHT_US);
    Serial.println("servo: RIGHT");
  }
  else if (cmd == "f") {
    Serial.println("motor: FORWARD 1s @ 80");
    motorRun(80, 1000);
  }
  else if (cmd == "b") {
    Serial.println("motor: REVERSE 1s @ 80");
    motorRun(-80, 1000);
  }
  else if (cmd == "fast") {
    Serial.println("motor: FORWARD 2s @ 150");
    motorRun(150, 2000);
  }
  else if (cmd == "stop") {
    motorStop();
    Serial.println("motor: STOP");
  }
  else if (cmd.length() > 0) {
    Serial.print("unknown command: ");
    Serial.println(cmd);
    Serial.println("type 'h' for help");
  }
}

// =======================================================
//  VL53L1X bring-up with XSHUT addr remap
// =======================================================
bool bootVL53(VL53L1X &s, TwoWire &bus, uint8_t xshutPin, uint8_t targetAddr,
              const char *name) {
  digitalWrite(xshutPin, HIGH);
  delay(10);

  s.setBus(&bus);
  s.setTimeout(100);
  if (!s.init()) {
    Serial.print(name); Serial.println(": init() failed");
    digitalWrite(xshutPin, LOW);
    return false;
  }
  s.setAddress(targetAddr);
  s.setDistanceMode(VL53L1X::Medium);
  s.setMeasurementTimingBudget(50000);
  s.startContinuous(50);
  Serial.print(name); Serial.print(": OK at 0x");
  if (targetAddr < 16) Serial.print("0");
  Serial.println(targetAddr, HEX);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println("\n\n=================================");
  Serial.println("  WRO v13 BENCH TEST");
  Serial.println("  LIFT THE CHASSIS — WHEELS IN AIR!");
  Serial.println("=================================");

  // I2C
  Wire.begin (I2C0_SDA, I2C0_SCL);  Wire.setClock(400000);
  Wire1.begin(I2C1_SDA, I2C1_SCL);  Wire1.setClock(400000);

  // GPIO
  pinMode(LED_PIN, OUTPUT);
  pinMode(ESTOP_PIN, INPUT_PULLUP);

  // VL53L1X XSHUTs LOW first, then bring sensors up one at a time
  pinMode(VL53_FRONT_XSHUT, OUTPUT); digitalWrite(VL53_FRONT_XSHUT, LOW);
  pinMode(VL53_SIDE_XSHUT,  OUTPUT); digitalWrite(VL53_SIDE_XSHUT,  LOW);
  delay(10);

  // OpenMV camera on UART2
  Serial2.begin(115200, SERIAL_8N1, CAM_RX, CAM_TX);

  // Servo
  steeringServo.attach(SERVO_PIN, 500, 2500);
  steeringServo.writeMicroseconds(SERVO_CENTER_US);

  // Motor
  pinMode(MOTOR_R_EN, OUTPUT); digitalWrite(MOTOR_R_EN, HIGH);
  pinMode(MOTOR_L_EN, OUTPUT); digitalWrite(MOTOR_L_EN, HIGH);
  ledcAttach(MOTOR_R_PWM, PWM_FREQ, PWM_RES);
  ledcAttach(MOTOR_L_PWM, PWM_FREQ, PWM_RES);
  motorStop();

  // IMU
  Serial.print("IMU init... ");
  icmOK = icm.begin_I2C(ICM20948_ADDR, &Wire);
  Serial.println(icmOK ? "OK" : "NOT FOUND");

  // Encoders
  encLeftOK  = checkI2C(Wire,  AS5600_ADDR);
  encRightOK = checkI2C(Wire1, AS5600_ADDR);
  Serial.print("Left  AS5600: "); Serial.println(encLeftOK  ? "OK" : "NOT FOUND");
  Serial.print("Right AS5600: "); Serial.println(encRightOK ? "OK" : "NOT FOUND");

  // VL53L1X — bring up Front (I2C0) first, then Side (I2C1)
  tofFrontOK = bootVL53(tofFront, Wire,  VL53_FRONT_XSHUT, VL53_FRONT_ADDR, "VL53L1X FRONT");
  tofSideOK  = bootVL53(tofSide,  Wire1, VL53_SIDE_XSHUT,  VL53_SIDE_ADDR,  "VL53L1X SIDE ");

  Serial.println();
  printHelp();
  Serial.println("Ready. Type 's' for status.");
}

void loop() {
  static String inputBuf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuf.length() > 0) {
        handleCommand(inputBuf);
        inputBuf = "";
      }
    } else {
      inputBuf += c;
    }
  }

  static String uartBuf = "";
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      lastCameraMsg = uartBuf;
      lastCameraMs = millis();
      uartBuf = "";
    } else {
      uartBuf += c;
    }
  }

  // Auto-stop motor after timer expires
  if (motorStopAt > 0 && millis() >= motorStopAt) {
    motorStop();
    Serial.println("(motor auto-stop)");
  }

  // Live mode tick
  static unsigned long lastLive = 0;
  if (liveMode && millis() - lastLive >= 200) {
    lastLive = millis();
    printLive();
  }

  // Heartbeat LED
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}
