// =====================================================================
//  WRO BENCH TEST — проверка всех систем робота на столе
//  ПЕРЕД запуском: ПОДНИМИ ШАССИ так чтобы колёса НЕ КАСАЛИСЬ ПОЛА!
// =====================================================================
//
//  Этот скетч проверяет:
//    1. Оба энкодера AS5600 (Wire + Wire1)
//    2. IMU ICM-20948 (yaw угол)
//    3. ToF VL53L1X (фронт + бок)
//    4. Сервопривод рулевого
//    5. Мотор BTS7960 (вперёд/назад)
//    6. Кнопку E-Stop
//
//  Команды в Serial Monitor (115200 baud):
//    h        — справка
//    s        — статус всех датчиков один раз
//    live     — живые показания каждые 200мс (повторно — выкл)
//    sl       — серво ВЛЕВО
//    sc       — серво ЦЕНТР
//    sr       — серво ВПРАВО
//    f        — мотор ВПЕРЁД 1 секунду на скорости 80
//    b        — мотор НАЗАД 1 секунду на скорости 80
//    stop     — экстренная остановка мотора
//    fast     — мотор вперёд 2 сек на 150
//
//  Как пользоваться:
//    1. Подними шасси на коробку, колёса в воздухе
//    2. Залей этот скетч
//    3. Открой Serial Monitor 115200
//    4. Набери `s` — увидишь все датчики
//    5. Набери `live` — данные в реальном времени
//    6. Крути колёса руками — энкодеры считают
//    7. Поверни робот рукой — IMU показывает угол
//    8. Поднеси руку к ToF — видно расстояние
//    9. `sl`/`sc`/`sr` — серво двигается
//   10. `f` — мотор крутит колёса (они в воздухе!)
// =====================================================================

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>

// ── Пины ────────────────────────────────────────────────
#define I2C_SDA1     21
#define I2C_SCL1     22
#define I2C_SDA2     25
#define I2C_SCL2     26
#define SERVO_PIN    27
#define MOTOR_R_EN   19
#define MOTOR_L_EN   23
#define MOTOR_R_PWM  5
#define MOTOR_L_PWM  14
#define ESTOP_PIN    32
#define LED_PIN      2
#define UART_RX      16
#define UART_TX      17

// ── Адреса I2C ──────────────────────────────────────────
#define AS5600_ADDR       0x36
#define ICM20948_ADDR     0x68    // AD0=GND
#define VL53L1X_ADDR      0x29

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_VL53L1X.h>

// ── Пины ────────────────────────────────────────────────
#define REG_RAW_ANGLE     0x0E
#define REG_STATUS        0x0B
#define REG_AGC           0x1A

// ── Сервопривод ─────────────────────────────────────────
#define SERVO_CENTER_US   1565
#define SERVO_LEFT_US     1215
#define SERVO_RIGHT_US    1915

// ── Мотор ───────────────────────────────────────────────
#define PWM_CH_FWD        0
#define PWM_CH_REV        1
#define PWM_FREQ          20000
#define PWM_RES           8

Servo steeringServo;
Adafruit_ICM20948 icm;

bool icmOK = false;
bool encLeftOK = false;
bool encRightOK = false;
bool tofFrontOK = false;
bool tofSideOK = false;
bool liveMode = false;
unsigned long motorStopAt = 0;
String lastCameraMsg = "Нет данных";
unsigned long lastCameraMs = 0;

Adafruit_VL53L1X tofFront;
Adafruit_VL53L1X tofSide;

// ═══════════════════════════════════════════════════════
//  AS5600 — чтение угла
// ═══════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════
//  Мотор
// ═══════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════
//  Сканирование I2C
// ═══════════════════════════════════════════════════════
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
  if (found == 0) Serial.print("пусто");
  Serial.println();
}

// ═══════════════════════════════════════════════════════
//  Полный статус
// ═══════════════════════════════════════════════════════
void printStatus() {
  Serial.println("\n========== СТАТУС ==========");

  // I2C сканеры
  Serial.println("I2C:");
  scanBus(Wire,  "Шина 1 (Wire)");
  scanBus(Wire1, "Шина 2 (Wire1)");

  // Энкодеры
  Serial.println("\nЭнкодеры:");
  int rawL = readAS5600Angle(Wire);
  int rawR = readAS5600Angle(Wire1);
  Serial.print("  Левый  AS5600: ");
  if (rawL >= 0) {
    Serial.print(rawL); Serial.print(" raw, ");
    Serial.print(rawL * 360.0 / 4096.0, 1); Serial.print("°, magnet=");
    Serial.println(readAS5600Magnet(Wire));
  } else Serial.println("НЕ ОТВЕЧАЕТ");

  Serial.print("  Правый AS5600: ");
  if (rawR >= 0) {
    Serial.print(rawR); Serial.print(" raw, ");
    Serial.print(rawR * 360.0 / 4096.0, 1); Serial.print("°, magnet=");
    Serial.println(readAS5600Magnet(Wire1));
  } else Serial.println("НЕ ОТВЕЧАЕТ");

  // IMU
  Serial.print("\nIMU ICM-20948: ");
  if (icmOK) {
    sensors_event_t a, g, m, t;
    icm.getEvent(&a, &g, &t, &m);
    Serial.print("OK  gyroZ=");
    Serial.print(g.gyro.z, 3);
    Serial.print(" rad/s  accelX=");
    Serial.print(a.acceleration.x, 2);
    Serial.println();
  } else Serial.println("НЕ НАЙДЕН");

  // E-Stop
  Serial.print("\nE-Stop кнопка (GPIO32): ");
  Serial.println(digitalRead(ESTOP_PIN) ? "не нажата" : "НАЖАТА");

  // Camera
  Serial.print("\nКамера (OpenMV UART): ");
  if (millis() - lastCameraMs < 1000) {
    Serial.print("OK. Последний пакет: ");
    Serial.println(lastCameraMsg);
  } else {
    Serial.println("НЕТ ДАННЫХ (более 1 сек таймаут)");
  }

  Serial.println("============================\n");
}

// ═══════════════════════════════════════════════════════
//  Живые данные
// ═══════════════════════════════════════════════════════
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

  // E-Stop
  Serial.print("  ESTOP:");
  Serial.print(digitalRead(ESTOP_PIN) ? "0" : "1");

  Serial.print("  TOF Front:");
  Serial.print(tofFrontOK && tofFront.dataReady() ? String(tofFront.distance()) : "---");
  if (tofFrontOK && tofFront.dataReady()) tofFront.clearInterrupt();

  Serial.print(" Side:");
  Serial.print(tofSideOK && tofSide.dataReady() ? String(tofSide.distance()) : "---");
  if (tofSideOK && tofSide.dataReady()) tofSide.clearInterrupt();

  Serial.print("  CAM:");
  if (millis() - lastCameraMs < 500) {
      Serial.print("V"); 
  } else {
      Serial.print("X");
  }

  Serial.println();
}

// ═══════════════════════════════════════════════════════
//  Команды Serial
// ═══════════════════════════════════════════════════════
void printHelp() {
  Serial.println("\n--- КОМАНДЫ ---");
  Serial.println("  h     - справка");
  Serial.println("  s     - статус один раз");
  Serial.println("  live  - живые данные (повторно - выкл)");
  Serial.println("  sl    - серво ВЛЕВО");
  Serial.println("  sc    - серво ЦЕНТР");
  Serial.println("  sr    - серво ВПРАВО");
  Serial.println("  f     - мотор ВПЕРЁД 1сек, скорость 80");
  Serial.println("  b     - мотор НАЗАД 1сек, скорость 80");
  Serial.println("  fast  - мотор ВПЕРЁД 2сек, скорость 150");
  Serial.println("  stop  - стоп мотор");
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
    Serial.println("серво: ВЛЕВО");
  }
  else if (cmd == "sc") {
    steeringServo.writeMicroseconds(SERVO_CENTER_US);
    Serial.println("серво: ЦЕНТР");
  }
  else if (cmd == "sr") {
    steeringServo.writeMicroseconds(SERVO_RIGHT_US);
    Serial.println("серво: ВПРАВО");
  }
  else if (cmd == "f") {
    Serial.println("мотор: ВПЕРЁД 1сек @ 80");
    motorRun(80, 1000);
  }
  else if (cmd == "b") {
    Serial.println("мотор: НАЗАД 1сек @ 80");
    motorRun(-80, 1000);
  }
  else if (cmd == "fast") {
    Serial.println("мотор: ВПЕРЁД 2сек @ 150");
    motorRun(150, 2000);
  }
  else if (cmd == "stop") {
    motorStop();
    Serial.println("мотор: СТОП");
  }
  else if (cmd.length() > 0) {
    Serial.print("неизвестная команда: ");
    Serial.println(cmd);
    Serial.println("набери 'h' для справки");
  }
}

// ═══════════════════════════════════════════════════════
//  Setup
// ═══════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println("\n\n=================================");
  Serial.println("  WRO BENCH TEST");
  Serial.println("  ПОДНИМИ ШАССИ — КОЛЁСА В ВОЗДУХЕ!");
  Serial.println("=================================");

  // I2C
  Wire.begin(I2C_SDA1, I2C_SCL1);
  Wire.setClock(400000);
  Wire1.begin(I2C_SDA2, I2C_SCL2);
  Wire1.setClock(400000);

  // GPIO
  pinMode(LED_PIN, OUTPUT);
  pinMode(ESTOP_PIN, INPUT_PULLUP);

  // Serial1 for OpenMV
  Serial1.begin(115200, SERIAL_8N1, UART_RX, UART_TX);

  // Сервопривод
  steeringServo.attach(SERVO_PIN, 500, 2500);
  steeringServo.writeMicroseconds(SERVO_CENTER_US);

  // Мотор
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);
  
  // ESP32 Arduino Core V3 syntax: ledcAttach(pin, freq, resolution)
  ledcAttach(MOTOR_R_PWM, PWM_FREQ, PWM_RES);
  ledcAttach(MOTOR_L_PWM, PWM_FREQ, PWM_RES);
  
  motorStop();

  // IMU
  Serial.print("IMU init... ");
  icmOK = icm.begin_I2C(ICM20948_ADDR, &Wire);
  Serial.println(icmOK ? "OK" : "НЕ НАЙДЕН");

  // Энкодеры
  encLeftOK  = checkI2C(Wire,  AS5600_ADDR);
  encRightOK = checkI2C(Wire1, AS5600_ADDR);
  Serial.print("Левый  AS5600: ");  Serial.println(encLeftOK  ? "OK" : "НЕ НАЙДЕН");
  Serial.print("Правый AS5600: "); Serial.println(encRightOK ? "OK" : "НЕ НАЙДЕН");

  // ToF
  Serial.print("Фронтальный ToF VL53L1X (Wire): ");
  if (checkI2C(Wire, VL53L1X_ADDR) && tofFront.begin(0x29, &Wire)) {
    tofFrontOK = true;
    tofFront.startRanging();
    Serial.println("OK");
  } else {
    Serial.println("НЕ НАЙДЕН");
  }

  Serial.print("Боковой ToF VL53L1X (Wire1): ");
  if (checkI2C(Wire1, VL53L1X_ADDR) && tofSide.begin(0x29, &Wire1)) {
    tofSideOK = true;
    tofSide.startRanging();
    Serial.println("OK");
  } else {
    Serial.println("НЕ НАЙДЕН");
  }

  Serial.println();
  printHelp();
  Serial.println("Готов. Набери 's' для статуса.");
}

// ═══════════════════════════════════════════════════════
//  Loop
// ═══════════════════════════════════════════════════════
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
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      lastCameraMsg = uartBuf;
      lastCameraMs = millis();
      uartBuf = "";
    } else {
      uartBuf += c;
    }
  }

  // Авто-стоп мотора по таймеру
  if (motorStopAt > 0 && millis() >= motorStopAt) {
    motorStop();
    Serial.println("(мотор автостоп)");
  }

  // Live режим
  static unsigned long lastLive = 0;
  if (liveMode && millis() - lastLive >= 200) {
    lastLive = millis();
    printLive();
  }

  // Мигание LED раз в секунду — heartbeat
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}
