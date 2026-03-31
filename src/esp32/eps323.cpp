/*
 * WRO Future Engineers — ESP32 Control Firmware
 * Версия 10.0 — CHAMPIONSHIP EDITION
 *
 * Железо: ESP32 DevKitC V4, BTS7960, JX PDI-6221MG,
 *         TCA9548A, ICM-20948, AS5600 x2, OpenMV H7 Plus,
 *         VL53L1X x2 (TCA CH3=фронт, CH4=бок)
 *
 * v10.0 — Ключевые фичи:
 *  1.  UART v3: RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*CS
 *  2.  Двойной столб: S-кривая объезда когда оба видны
 *  3.  VL53L1X: фронтальный (экстренное торможение) + боковой (парковка)
 *  4.  Параллельная парковка FSM (approach → align → creep → stop)
 *  5.  Одометрия: 174 тиков/см (AS5600 4096 тиков, D=75мм колесо)
 *  6.  Подсчёт кругов: камера (оранж/синяя линия) + гироскоп резерв
 *  7.  Кнопка старта через E-STOP
 *  8.  Адаптивные скорости Open / Obstacle
 *  9.  EMA-фильтрованная PID производная
 * 10.  Детектирование малиновых парковочных блоков от камеры
 */

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <math.h>

// ============================================================
// ПИНЫ
// ============================================================
#define I2C_SDA 21
#define I2C_SCL 22
#define SERVO_PIN 18
#define MOTOR_R_EN 19
#define MOTOR_L_EN 23
#define MOTOR_R_PWM 5
#define MOTOR_L_PWM 14
#define UART_RX 16
#define UART_TX 17
#define ESTOP_PIN 32
#define MODE_SWITCH_PIN 4 // LOW = OBSTACLE, HIGH = OPEN
#define LED_PIN 2

// ============================================================
// СЕРВО И МОТОР
// ============================================================
#define SERVO_CENTER 90
#define SERVO_MAX_RIGHT 135
#define SERVO_MAX_LEFT 45

// Скорости Open Challenge (скорость)
#define OPEN_MAX_SPEED 180
#define OPEN_TURN_FAST 140
#define OPEN_TURN_SLOW 95
#define OPEN_MIN_SPEED 40

// Скорости Obstacle Challenge (точность)
#define OBS_MAX_SPEED 140
#define OBS_TURN_FAST 110
#define OBS_TURN_SLOW 75
#define OBS_MIN_SPEED 35

// Активные (назначаются в setup)
int MOTOR_MAX_SPEED = OBS_MAX_SPEED;
int MOTOR_TURN_FAST = OBS_TURN_FAST;
int MOTOR_TURN_SLOW = OBS_TURN_SLOW;
int MOTOR_MIN_SPEED = OBS_MIN_SPEED;

// ============================================================
// PID
// ============================================================
float PID_KP = 0.55f;
float PID_KI = 0.002f;
float PID_KD = 0.18f;
float GYRO_KP = 1.20f;

float integralError = 0.0f;
float filteredDerivative = 0.0f;
#define DERIVATIVE_ALPHA 0.3f
#define INTEGRAL_CLAMP 400

// ============================================================
// ОБЪЕЗД СТОЛБОВ — Динамический offset
// ============================================================
// Красный: offset < 0 (робот едет ЛЕВЕЕ столба → столб СПРАВА)
// Зелёный: offset > 0 (робот едет ПРАВЕЕ столба → столб СЛЕВА)
#define AVOID_OFFSET_MIN 25 // Далёкий столб
#define AVOID_OFFSET_MAX 65 // Близкий столб
#define AVOID_RANGE_FAR 80  // Активация (см)
#define AVOID_RANGE_NEAR 20 // Макс. смещение (см)

// ============================================================
// I2C / TCA9548A
// ============================================================
#define TCA_ADDRESS 0x70
#define AS5600_ADDRESS 0x36
#define VL53L1X_ADDRESS 0x29
#define TCA_CH_IMU 0
#define TCA_CH_ENC_LEFT 1
#define TCA_CH_ENC_RIGHT 2
#define TCA_CH_TOF_FRONT 3
#define TCA_CH_TOF_SIDE 4

// ============================================================
// VL53L1X (упрощённые регистры)
// ============================================================
// Полноценная инициализация VL53L1X комплексна (>100 регистров).
// Мы используем библиотеку VL53L1X (Pololu) через TCA.
// Если библиотека не установлена — закомментировать USE_VL53L1X.
#define USE_VL53L1X // Раскомментировать когда датчики подключены

#ifdef USE_VL53L1X
#include <VL53L1X.h>
VL53L1X tofFront;
VL53L1X tofSide;
bool tofFrontOK = false;
bool tofSideOK = false;
#endif

int distFrontMM = 9999; // Фронтальный ToF (мм)
int distSideMM = 9999;  // Боковой ToF (мм)

#define TOF_EMERGENCY_MM 120 // 12 см — экстренное торможение
#define TOF_SLOW_DOWN_MM 300 // 30 см — начать замедление
#define TOF_SIDE_TARGET_MM                                                     \
  100 // 10 см — целевое расстояние до стены при парковке
#define TOF_PARALLEL_TOL_MM 20 // 2 см допуск параллельности

// ============================================================
// ТАЙМАУТЫ И ОДОМЕТРИЯ
// ============================================================
#define CAMERA_TIMEOUT 500
#define PRINT_INTERVAL 200
#define LOOP_INTERVAL 10
#define GYRO_CALIB_SAMPLES 300
#define FINISH_BLINK_MS 250
#define ESTOP_DEBOUNCE_MS 20
#define MAX_IMU_DT_SEC 0.05f
#define ENCODER_FAIL_STOP 50
#define SPEED_RAMP_STEP 8
#define BLIND_TURN_TIMEOUT 3000
#define BLIND_TURN_ANGLE 85.0f
#define ENCODER_TURN_TICKS 3000

// Одометрия: AS5600 = 4096 тиков/оборот, D=75мм → C=235.6мм
// 4096 / 235.6 = 17.38 тиков/мм = 174 тиков/см
#define TICKS_PER_CM 174

// ============================================================
// ГОНКА
// ============================================================
#define TARGET_LAPS 3
#define LAP_DEGREES 360.0f

// ============================================================
// ПОДСЧЁТ КРУГОВ ПО ЛИНИЯМ
// ============================================================
#define LINE_DETECT_COOLDOWN_MS 3000
#define LINE_CONFIRM_FRAMES 3

// ============================================================
// ПАРКОВКА
// ============================================================
#define PARK_APPROACH_SPEED 60 // Скорость фазы подъезда
#define PARK_CREEP_SPEED 40    // Скорость заезда в зону
#define PARK_DISTANCE_CM 60    // Расстояние заезда в зону
#define PARK_BRAKE_ZONE_CM 15 // Зона торможения перед остановкой
#define PARK_MAGENTA_CONFIRM 5 // Кадров подряд для подтверждения маркера

enum ParkPhase {
  PARK_APPROACH, // Едем к стартовой зоне, ищем малиновые блоки
  PARK_ALIGN, // Выравниваемся параллельно стене (гироскоп + боковой ToF)
  PARK_CREEP, // Заезжаем между блоками
  PARK_FINAL_STOP // Полная остановка
};

// ============================================================
// ОБЪЕКТЫ
// ============================================================
Servo steeringServo;
Adafruit_ICM20948 icm;

// ============================================================
// КАМЕРА v3 — 6 полей
// ============================================================
int camRedX = 0;        // X красного столба (-160..160)
int camRedDist = 999;   // Дистанция до красного (см)
int camGreenX = 0;      // X зелёного столба
int camGreenDist = 999; // Дистанция до зелёного
int camModeFlag = 0;    // Битовые флаги (orange/blue/magenta/wall)
int camExtraTag = 0;    // Доп. данные

bool cameraOnline = false;
bool newCameraData = false;
unsigned long lastCameraTime = 0;

#define CAM_FLAG_ORANGE 1  // bit 0
#define CAM_FLAG_BLUE 2    // bit 1
#define CAM_FLAG_MAGENTA 4 // bit 2
#define CAM_FLAG_WALL 8    // bit 3

#define UART_BUF_SIZE 64
static char uartBuf[UART_BUF_SIZE];
static uint8_t uartBufPos = 0;

// ============================================================
// ГИРОСКОП
// ============================================================
float yawAngle = 0.0f;
float gyroZbias = 0.0f;
float totalRotation = 0.0f;
float lastYaw = 0.0f;
unsigned long lastIMUTime = 0;
float targetHeading = 0.0f;
float magBiasX = 0.0f;
float magBiasY = 0.0f;

// ============================================================
// ОДОМЕТРИЯ
// ============================================================
int encLeft = 0;
int encRight = 0;
int encLeftPrev = 0;
int encRightPrev = 0;
long totalDistLeft = 0;
long totalDistRight = 0;
int encErrorCount = 0;
bool encoderFaultActive = false;

// ============================================================
// ПОДСЧЁТ КРУГОВ
// ============================================================
int lineLapCount = 0;
unsigned long lastLineSeen = 0;
int lineConfirmCount = 0;
bool lastFrameHadLine = false;

// ============================================================
// СОСТОЯНИЯ
// ============================================================
int targetSteering = SERVO_CENTER;
int targetSpeed = 0;
int commandSpeed = 0;
int lastCameraError = 0;
int lastActivePillar = 0; // 3=red, 4=green — для сброса integral при смене

int globalTrackDirection = 1; // +1=CW, -1=CCW
bool directionConfirmed = false;

long turnStartDistLeft = 0;
long turnStartDistRight = 0;
int closeWallCount = 0;

enum RobotState {
  STATE_INIT,
  STATE_TRACKING,
  STATE_BLIND_TURN,
  STATE_PARKING,
  STATE_SAFE_STOP,
  STATE_FINISH
};
RobotState currentState = STATE_INIT;

float turnStartYaw = 0.0f;
int turnDirection = 1;
unsigned long turnStartTime = 0;

// ============================================================
// ГОНКА
// ============================================================
int lapCount = 0;
bool raceFinished = false;
bool estopActive = false;
bool isObstacleMode = false;
bool raceStarted = false;

// Парковка
ParkPhase parkPhase = PARK_APPROACH;
long parkStartTicks = 0;
float parkTargetYaw = 0.0f;
int magentaSeenCount = 0;

// ============================================================
//                      УТИЛИТЫ
// ============================================================
bool tcaSelect(uint8_t channel) {
  if (channel > 7)
    return false;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << channel);
  return (Wire.endTransmission() == 0);
}

bool tcaDisable() {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(0);
  return (Wire.endTransmission() == 0);
}

void setSteering(int angle) {
  angle = constrain(angle, SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
  steeringServo.write(angle);
}

int applyDeadband(int speed) {
  if (speed == 0)
    return 0;
  // FIX: маппинг на актуальный MOTOR_MAX_SPEED, а не хардкод 255
  return map(abs(speed), 1, MOTOR_MAX_SPEED, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
}

void setMotorSpeed(int speed) {
  speed = constrain(speed, -255, 255);
  int mapped = applyDeadband(speed);
  if (speed > 0) {
    ledcWrite(1, 0);
    ledcWrite(0, mapped);
  } else if (speed < 0) {
    ledcWrite(0, 0);
    ledcWrite(1, mapped);
  } else {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
  }
}

void applySpeedRamp() {
  if (commandSpeed < targetSpeed)
    commandSpeed = min(commandSpeed + SPEED_RAMP_STEP, targetSpeed);
  else if (commandSpeed > targetSpeed)
    commandSpeed = max(commandSpeed - SPEED_RAMP_STEP, targetSpeed);
  setMotorSpeed(commandSpeed);
}

void safeStop() {
  targetSpeed = 0;
  commandSpeed = 0;
  setMotorSpeed(0);
  targetSteering = SERVO_CENTER;
  setSteering(SERVO_CENTER);
  lastCameraError = 0;
  integralError = 0;
  filteredDerivative = 0;
  newCameraData = false;
  closeWallCount = 0;
  currentState = STATE_SAFE_STOP;
}

// ============================================================
//                      E-STOP & START
// ============================================================
void checkEStop() {
  static unsigned long estopTriggerTime = 0;
  static bool estopArmed = false;
  bool pinLow = (digitalRead(ESTOP_PIN) == LOW);

  if (pinLow && !estopArmed) {
    estopArmed = true;
    estopTriggerTime = millis();
    return;
  }
  if (estopArmed && !pinLow) {
    estopArmed = false;
    if (!estopActive)
      return;
  }
  if (estopArmed && (millis() - estopTriggerTime >= ESTOP_DEBOUNCE_MS)) {
    if (!estopActive) {
      estopActive = true;
      safeStop();
      Serial.println("=== E-STOP АКТИВИРОВАН ===");
    }
  }
  if (estopActive && !pinLow) {
    estopActive = false;
    estopArmed = false;
    if (raceStarted) {
      currentState = STATE_TRACKING;
      lastIMUTime = millis();
      lastCameraTime = millis();
      lastYaw = yawAngle;
      targetHeading = yawAngle;
      Serial.println("=== ПРОДОЛЖЕНИЕ ГОНКИ ===");
    }
  }
  if (estopActive)
    safeStop();
}

void handleStartButton() {
  static bool wasPressed = false;
  bool pinLow = (digitalRead(ESTOP_PIN) == LOW);

  if (pinLow) {
    wasPressed = true;
    digitalWrite(LED_PIN, (millis() / 100) % 2);
  }
  if (wasPressed && !pinLow) {
    raceStarted = true;
    currentState = STATE_TRACKING;
    lastIMUTime = millis();
    lastCameraTime = millis();
    lastYaw = yawAngle;
    targetHeading = yawAngle;
    Serial.println("╔══════════════════════════╗");
    Serial.println("║      ГОНКА НАЧАЛАСЬ!     ║");
    Serial.println("╚══════════════════════════╝");
    digitalWrite(LED_PIN, HIGH);
  }
}

// ============================================================
//                      ФИНИШ
// ============================================================
void finishRace() {
  safeStop();
  raceFinished = true;
  currentState = STATE_FINISH;
  Serial.println("╔══════════════════════════════════╗");
  Serial.println("║  ФИНИШ! 3 КРУГА + ПАРКОВКА DONE  ║");
  Serial.println("╚══════════════════════════════════╝");
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
//                      IMU
// ============================================================
void calibrateGyro() {
  Serial.println("Калибровка гироскопа (3 сек)...");
  float sum = 0;
  for (int i = 0; i < GYRO_CALIB_SAMPLES; i++) {
    tcaSelect(TCA_CH_IMU);
    sensors_event_t accel, gyro, mag, temp;
    icm.getEvent(&accel, &gyro, &temp, &mag);
    tcaDisable();
    sum += gyro.gyro.z;
    delay(10);
  }
  gyroZbias = sum / GYRO_CALIB_SAMPLES;
  Serial.print("Bias: ");
  Serial.println(gyroZbias, 6);
}

void updateYaw() {
  unsigned long now = millis();
  if (lastIMUTime == 0) {
    lastIMUTime = now;
    return;
  }

  float dt = (now - lastIMUTime) / 1000.0f;
  lastIMUTime = now;
  if (dt <= 0.0f)
    return;
  if (dt > MAX_IMU_DT_SEC)
    dt = MAX_IMU_DT_SEC;

  tcaSelect(TCA_CH_IMU);
  sensors_event_t accel, gyro, mag, temp;
  bool ok = icm.getEvent(&accel, &gyro, &temp, &mag);
  tcaDisable();
  if (!ok)
    return;

  float correctedZ = gyro.gyro.z - gyroZbias;
  if (fabsf(correctedZ) < 0.01f)
    correctedZ = 0.0f;

  yawAngle += correctedZ * dt * (180.0f / PI);
  while (yawAngle > 180)
    yawAngle -= 360;
  while (yawAngle < -180)
    yawAngle += 360;

  // FIX: Магнитометрическая коррекция ОТКЛЮЧЕНА.
  // magBiasX/Y = 0 без калибровки + EMI от мотора 380 = чистый шум.
  // Раскомментировать ТОЛЬКО после калибровки magBias на треке:
  //   Покрутить робота 360° → записать min/max mag X/Y
  //   magBiasX = (minX + maxX) / 2;  magBiasY = (minY + maxY) / 2;
  //
  // float magX = mag.magnetic.x - magBiasX;
  // float magY = mag.magnetic.y - magBiasY;
  // float magHeading = atan2f(magY, magX) * (180.0f / PI);
  // float magError = magHeading - yawAngle;
  // while (magError > 180) magError -= 360;
  // while (magError < -180) magError += 360;
  // yawAngle += 0.02f * magError;
  // while (yawAngle > 180) yawAngle -= 360;
  // while (yawAngle < -180) yawAngle += 360;
}

// ============================================================
//                 VL53L1X (ToF дальномеры)
// ============================================================
#ifdef USE_VL53L1X
void initToF() {
  // --- Фронтальный (TCA CH3) ---
  if (tcaSelect(TCA_CH_TOF_FRONT)) {
    tofFront.setBus(&Wire);
    tofFront.setTimeout(200);
    if (tofFront.init()) {
      tofFront.setDistanceMode(VL53L1X::Short);
      tofFront.setMeasurementTimingBudget(33000); // 33мс — быстрые замеры
      tofFront.startContinuous(33);
      tofFrontOK = true;
      Serial.println("VL53L1X ФРОНТ: OK");
    } else {
      Serial.println("VL53L1X ФРОНТ: НЕ НАЙДЕН (продолжаем без него)");
    }
    tcaDisable();
  }

  // --- Боковой (TCA CH4) ---
  if (tcaSelect(TCA_CH_TOF_SIDE)) {
    tofSide.setBus(&Wire);
    tofSide.setTimeout(200);
    if (tofSide.init()) {
      tofSide.setDistanceMode(VL53L1X::Short);
      tofSide.setMeasurementTimingBudget(33000);
      tofSide.startContinuous(33);
      tofSideOK = true;
      Serial.println("VL53L1X БОК:   OK");
    } else {
      Serial.println("VL53L1X БОК:   НЕ НАЙДЕН (продолжаем без него)");
    }
    tcaDisable();
  }
}

void readToF() {
  if (tofFrontOK) {
    tcaSelect(TCA_CH_TOF_FRONT);
    if (tofFront.dataReady()) {
      distFrontMM = tofFront.read(false);
      if (tofFront.timeoutOccurred())
        distFrontMM = 9999;
    }
    tcaDisable();
  }
  if (tofSideOK) {
    tcaSelect(TCA_CH_TOF_SIDE);
    if (tofSide.dataReady()) {
      distSideMM = tofSide.read(false);
      if (tofSide.timeoutOccurred())
        distSideMM = 9999;
    }
    tcaDisable();
  }
}
#else
void initToF() {
  Serial.println("VL53L1X: ОТКЛЮЧЕНЫ (USE_VL53L1X не определён)");
}
void readToF() { /* noop */
}
#endif

// ============================================================
//                  ПОДСЧЁТ КРУГОВ
// ============================================================
void checkLapCountGyro() {
  float deltaYaw = yawAngle - lastYaw;
  while (deltaYaw > 180)
    deltaYaw -= 360;
  while (deltaYaw < -180)
    deltaYaw += 360;
  totalRotation += deltaYaw;
  lastYaw = yawAngle;
}

void checkLapCountLine() {
  // Ищем оранжевую (bit 0) или синюю (bit 1) линию в ModeFlag
  bool orangeSeen = (camModeFlag & CAM_FLAG_ORANGE) != 0;
  bool blueSeen = (camModeFlag & CAM_FLAG_BLUE) != 0;
  bool lineSeen = orangeSeen || blueSeen;

  // Определяем направление по первой линии
  if (!directionConfirmed) {
    if (orangeSeen) {
      globalTrackDirection = 1;
      directionConfirmed = true;
    }
    if (blueSeen) {
      globalTrackDirection = -1;
      directionConfirmed = true;
    }
    if (directionConfirmed) {
      Serial.print(">>> НАПРАВЛЕНИЕ: ");
      Serial.println(globalTrackDirection == 1 ? "CW" : "CCW");
    }
  }

  // Cooldown
  if (millis() - lastLineSeen < LINE_DETECT_COOLDOWN_MS) {
    lineConfirmCount = 0; // FIX: сброс счётчика в cooldown, иначе ложный круг
    return;
  }

  // Подтверждение: N кадров подряд
  if (lineSeen) {
    lineConfirmCount++;
  } else {
    lineConfirmCount = 0;
  }

  if (lineConfirmCount >= LINE_CONFIRM_FRAMES) {
    lineLapCount++;
    lastLineSeen = millis();
    lineConfirmCount = 0;
    Serial.print("=== КРУГ ");
    Serial.print(lineLapCount);
    Serial.print("/");
    Serial.print(TARGET_LAPS);
    Serial.println(" ===");
  }
}

void checkLapCompletion() {
  int gyroLaps = (int)(fabsf(totalRotation) / LAP_DEGREES);
  lapCount = max(lineLapCount, gyroLaps);

  if (lapCount >= TARGET_LAPS && currentState == STATE_TRACKING) {
    if (obstacleMode) {
      Serial.println(">>> 3 КРУГА (OBSTACLE) → ПЕРЕХОД К ПАРКОВКЕ <<<");
      enterParking();
    } else {
      // ПРАВИЛА WRO 2026: Open Challenge - просто остановиться в стартовой секции
      Serial.println(">>> 3 КРУГА (OPEN) → НЕМЕДЛЕННЫЙ ФИНИШ <<<");
      finishRace();
    }
  }
}

// ============================================================
//                    ОДОМЕТРИЯ
// ============================================================
int readEncoder(uint8_t tcaChannel) {
  if (!tcaSelect(tcaChannel))
    return -1;
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(0x0E);
  if (Wire.endTransmission(false) != 0) {
    tcaDisable();
    return -1;
  }
  uint8_t got = Wire.requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)2);
  if (got >= 2 && Wire.available() >= 2) {
    int h = Wire.read(), l = Wire.read();
    tcaDisable();
    return ((h & 0x0F) << 8) | l;
  }
  tcaDisable();
  return -1;
}

void updateOdometry() {
  if (encLeft < 0 || encRight < 0) {
    encErrorCount++;
    if (encErrorCount == 10) {
      Serial.println("ALARM: ЭНКОДЕРЫ ПОТЕРЯНЫ!");
      encLeftPrev = max(encLeft, 0);
      encRightPrev = max(encRight, 0);
    }
    if (encErrorCount >= ENCODER_FAIL_STOP) {
      encoderFaultActive = true;
      Serial.println("CRITICAL: ЭНКОДЕРЫ — SAFE STOP");
      safeStop();
    }
    return;
  }
  if (encoderFaultActive) {
    encoderFaultActive = false;
    Serial.println("INFO: ЭНКОДЕРЫ OK");
  }
  if (encErrorCount >= 10) {
    encLeftPrev = encLeft;
    encRightPrev = encRight;
  }
  encErrorCount = 0;

  int dL = encLeft - encLeftPrev;
  int dR = encRight - encRightPrev;
  if (dL > 2048)
    dL -= 4096;
  if (dL < -2048)
    dL += 4096;
  if (dR > 2048)
    dR -= 4096;
  if (dR < -2048)
    dR += 4096;

  totalDistLeft += dL;
  totalDistRight += dR;
  encLeftPrev = encLeft;
  encRightPrev = encRight;
}

// ============================================================
//                    КАМЕРА (UART v3)
// ============================================================
uint8_t xorChecksum(const char *s, int len) {
  uint8_t cs = 0;
  for (int i = 0; i < len; i++)
    cs ^= (uint8_t)s[i];
  return cs;
}

void readCameraData() {
  newCameraData = false;

  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\r')
      continue;
    if (c == '\n') {
      uartBuf[uartBufPos] = '\0';
      uartBufPos = 0;
      if (strlen(uartBuf) == 0)
        continue;

      // CRC
      char *star = strchr(uartBuf, '*');
      if (!star)
        continue;
      *star = '\0';
      uint8_t expected = xorChecksum(uartBuf, strlen(uartBuf));
      uint8_t received = (uint8_t)strtol(star + 1, NULL, 16);
      if (expected != received)
        continue; // EMI corruption

      // Парсим 6 полей
      int f[6] = {0, 999, 0, 999, 0, 0};
      char *tok = uartBuf;
      for (int i = 0; i < 6 && tok; i++) {
        char *comma = strchr(tok, ',');
        if (comma)
          *comma = '\0';
        f[i] = atoi(tok);
        tok = comma ? comma + 1 : NULL;
      }

      // Валидация
      if (f[0] >= -160 && f[0] <= 160 && f[1] >= 0 && f[1] <= 999 &&
          f[2] >= -160 && f[2] <= 160 && f[3] >= 0 && f[3] <= 999) {
        camRedX = f[0];
        camRedDist = f[1];
        camGreenX = f[2];
        camGreenDist = f[3];
        camModeFlag = f[4];
        camExtraTag = f[5];
        lastCameraTime = millis();
        cameraOnline = true;
        newCameraData = true;
      }
    } else {
      if (uartBufPos < UART_BUF_SIZE - 1)
        uartBuf[uartBufPos++] = c;
      else
        uartBufPos = 0;
    }
  }

  if (millis() - lastCameraTime > CAMERA_TIMEOUT) {
    if (cameraOnline)
      Serial.println("WARN: Камера offline → гироскоп");
    cameraOnline = false;
  }
}

// ============================================================
//               ОБЪЕЗД СТОЛБОВ (Dual-Pillar)
// ============================================================
int calcSingleOffset(bool isRed, int dist) {
  if (dist <= 0 || dist > AVOID_RANGE_FAR)
    return 0;
  int offset;
  if (dist <= AVOID_RANGE_NEAR)
    offset = AVOID_OFFSET_MAX;
  else
    offset = map(dist, AVOID_RANGE_NEAR, AVOID_RANGE_FAR, AVOID_OFFSET_MAX,
                 AVOID_OFFSET_MIN);
  return isRed ? -offset : offset;
  // Red: offset<0 → робот влево → столб справа (WRO correct)
  // Green: offset>0 → робот вправо → столб слева (WRO correct)
}

struct SteerResult {
  int steeringAngle;
  int speedLimit;
};

SteerResult calcDualPillarSteering() {
  // Случай: видим ОБА столба
  // Строим целевую точку МЕЖДУ ними, со смещением по правилам WRO
  bool hasRed = (camRedDist < 999);
  bool hasGreen = (camGreenDist < 999);

  SteerResult r;
  r.steeringAngle = SERVO_CENTER;
  r.speedLimit = MOTOR_MAX_SPEED;

  if (hasRed && hasGreen) {
    // === ОБА СТОЛБА ВИДНЫ — S-кривая ===
    // Целевая X-позиция: середина между столбами,
    // со смещением чтобы красный остался справа, зелёный слева
    int midX = (camRedX + camGreenX) / 2;

    // Дополнительное смещение: если столбы близко, сильнее «протискиваемся»
    int closerDist = min(camRedDist, camGreenDist);
    int safetyShift = 0;
    if (closerDist < 40) {
      // Сдвигаем целевую точку от ближайшего столба
      if (camRedDist < camGreenDist) {
        safetyShift = 10; // Уехать правее от красного (он ближе)
      } else {
        safetyShift = -10; // Уехать левее от зелёного (он ближе)
      }
    }

    int targetX = midX + safetyShift;
    // targetX это "ошибка" — 0 значит робот смотрит точно между столбами

    float correction = PID_KP * targetX;
    r.steeringAngle = constrain(SERVO_CENTER + (int)correction, SERVO_MAX_LEFT,
                                SERVO_MAX_RIGHT);

    // Замедление пропорционально близости
    r.speedLimit = map(closerDist, 15, 60, MOTOR_TURN_SLOW, MOTOR_TURN_FAST);
    r.speedLimit = constrain(r.speedLimit, MOTOR_MIN_SPEED, MOTOR_TURN_FAST);

  } else if (hasRed) {
    // === Только красный ===
    // FIX: сброс integral при смене столба (red↔green)
    if (lastActivePillar != 3) {
      integralError = 0;
      filteredDerivative = 0;
      lastActivePillar = 3;
    }
    int offset = calcSingleOffset(true, camRedDist);
    int error = camRedX + offset;

    int rawDeriv = error - lastCameraError;
    filteredDerivative = DERIVATIVE_ALPHA * rawDeriv +
                         (1.0f - DERIVATIVE_ALPHA) * filteredDerivative;
    integralError += error;
    integralError = constrain(integralError, -INTEGRAL_CLAMP, INTEGRAL_CLAMP);
    lastCameraError = error;

    int corr = (int)(PID_KP * error + PID_KI * integralError +
                     PID_KD * filteredDerivative);
    r.steeringAngle =
        constrain(SERVO_CENTER + corr, SERVO_MAX_LEFT, SERVO_MAX_RIGHT);

    if (camRedDist < 40)
      r.speedLimit = MOTOR_TURN_SLOW;

  } else if (hasGreen) {
    // === Только зелёный ===
    // FIX: сброс integral при смене столба (green↔red)
    if (lastActivePillar != 4) {
      integralError = 0;
      filteredDerivative = 0;
      lastActivePillar = 4;
    }
    int offset = calcSingleOffset(false, camGreenDist);
    int error = camGreenX + offset;

    int rawDeriv = error - lastCameraError;
    filteredDerivative = DERIVATIVE_ALPHA * rawDeriv +
                         (1.0f - DERIVATIVE_ALPHA) * filteredDerivative;
    integralError += error;
    integralError = constrain(integralError, -INTEGRAL_CLAMP, INTEGRAL_CLAMP);
    lastCameraError = error;

    int corr = (int)(PID_KP * error + PID_KI * integralError +
                     PID_KD * filteredDerivative);
    r.steeringAngle =
        constrain(SERVO_CENTER + corr, SERVO_MAX_LEFT, SERVO_MAX_RIGHT);

    if (camGreenDist < 40)
      r.speedLimit = MOTOR_TURN_SLOW;
  }
  // else: ни одного столба — вернём SERVO_CENTER и полную скорость

  return r;
}

// ============================================================
//                   СЛЕПОЙ ПОВОРОТ
// ============================================================
void enterBlindTurn(int direction) {
  currentState = STATE_BLIND_TURN;
  turnStartYaw = yawAngle;
  turnDirection = direction;
  turnStartTime = millis();
  turnStartDistLeft = totalDistLeft;
  turnStartDistRight = totalDistRight;
  integralError = 0;
  filteredDerivative = 0;
  closeWallCount = 0;
  Serial.print(">>> СЛЕПОЙ ПОВОРОТ: ");
  Serial.println(turnDirection == 1 ? "ПРАВО" : "ЛЕВО");
}

bool updateBlindTurn() {
  if (millis() - turnStartTime > BLIND_TURN_TIMEOUT) {
    currentState = STATE_TRACKING;
    targetHeading = yawAngle;
    closeWallCount = 0;
    lastCameraError = 0;
    Serial.println(">>> ТАЙМАУТ ПОВОРОТА <<<");
    return true;
  }

  targetSteering = (turnDirection == 1) ? SERVO_MAX_RIGHT : SERVO_MAX_LEFT;
  targetSpeed = MOTOR_TURN_SLOW;

  float turned = yawAngle - turnStartYaw;
  while (turned > 180)
    turned -= 360;
  while (turned < -180)
    turned += 360;

  long distT = ((totalDistLeft - turnStartDistLeft) +
                (totalDistRight - turnStartDistRight)) /
               2;

  if (fabsf(turned) >= BLIND_TURN_ANGLE || labs(distT) > ENCODER_TURN_TICKS) {
    currentState = STATE_TRACKING;
    targetHeading = yawAngle;
    closeWallCount = 0;
    lastCameraError = 0;
    Serial.println(">>> КОНЕЦ ПОВОРОТА <<<");
    return true;
  }
  return false;
}

// ============================================================
//                   ПАРКОВКА FSM
// ============================================================
void enterParking() {
  currentState = STATE_PARKING;
  parkPhase = PARK_APPROACH;
  parkStartTicks = totalDistLeft;
  magentaSeenCount = 0;

  // Запоминаем текущий курс как целевой для парковки (вдоль стены)
  // Округляем до ближайших 90° (стены трассы перпендикулярны)
  float raw = yawAngle;
  float snapped = roundf(raw / 90.0f) * 90.0f;
  parkTargetYaw = snapped;

  integralError = 0;
  filteredDerivative = 0;
  Serial.println(">>> ПАРКОВКА: ПОДЪЕЗД <<<");
}

void updateParking() {
  bool magentaSeen = (camModeFlag & CAM_FLAG_MAGENTA) != 0;

  switch (parkPhase) {

  // ----- APPROACH: Замедляемся, ищем малиновые блоки -----
  case PARK_APPROACH: {
    // Держим курс по гироскопу
    float hErr = parkTargetYaw - yawAngle;
    while (hErr > 180)
      hErr -= 360;
    while (hErr < -180)
      hErr += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    targetSpeed = PARK_APPROACH_SPEED;

// Боковой ToF: корректируем расстояние до стены
#ifdef USE_VL53L1X
    if (tofSideOK && distSideMM < 500) {
      int sideErr = distSideMM - TOF_SIDE_TARGET_MM;
      // Мягкая коррекция: +/- 5° руля на каждые 10мм ошибки
      int sideCorr = constrain(sideErr / 2, -15, 15);
      // Знак зависит от направления (бок стены слева или справа)
      targetSteering =
          constrain(targetSteering + sideCorr * globalTrackDirection,
                    SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    }
#endif

    if (magentaSeen) {
      magentaSeenCount++;
    } else {
      magentaSeenCount = max(magentaSeenCount - 1, 0);
    }

    // Подтверждение: увидели малиновый блок N раз. 
    // WRO 2026: Блоков два (слева и справа), паркуемся между ними.
    if (magentaSeenCount >= PARK_MAGENTA_CONFIRM) {
      parkPhase = PARK_ALIGN;
      parkStartTicks = totalDistLeft;
      Serial.println(">>> ПАРКОВКА: ВЫРАВНИВАНИЕ (МАЛИНОВЫЙ НАЙДЕН) <<<");
    }

    // Fallback: если проехали слишком далеко без маркера (2 метра), всё равно паркуемся
    long distTraveled = abs(totalDistLeft - parkStartTicks);
    if (distTraveled > 200L * TICKS_PER_CM) { 
      parkPhase = PARK_ALIGN;
      parkStartTicks = totalDistLeft;
      Serial.println(">>> ПАРКОВКА: FALLBACK ВЫРАВНИВАНИЕ <<<");
    }
    break;
  }

  // ----- ALIGN: Выравниваемся параллельно стене -----
  case PARK_ALIGN: {
    // Снижаем скорость, выравниваем курс точно
    float hErr = parkTargetYaw - yawAngle;
    while (hErr > 180)
      hErr -= 360;
    while (hErr < -180)
      hErr += 360;

    targetSteering = constrain(SERVO_CENTER + (int)(2.0f * GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    targetSpeed = PARK_CREEP_SPEED;

    // Если курс достаточно точный (±2°), переходим к заезду
    if (fabsf(hErr) < 2.0f) {
      parkPhase = PARK_CREEP;
      parkStartTicks = totalDistLeft;
      Serial.println(">>> ПАРКОВКА: ЗАЕЗД В ЗОНУ <<<");
    }
    break;
  }

  // ----- CREEP: Медленно заезжаем между маркерами -----
  case PARK_CREEP: {
    float hErr = parkTargetYaw - yawAngle;
    while (hErr > 180)
      hErr -= 360;
    while (hErr < -180)
      hErr += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);

    long distTraveled = abs(totalDistLeft - parkStartTicks);
    // PARK_DISTANCE_CM = расстояние от момента Align до полной остановки в зоне
    long targetTicks = (long)PARK_DISTANCE_CM * TICKS_PER_CM;

    // Плавное торможение в парковочной зоне
    long brakeZone = (long)PARK_BRAKE_ZONE_CM * TICKS_PER_CM;
    if (distTraveled > targetTicks - brakeZone) {
      int rampDown = map(distTraveled, targetTicks - brakeZone, targetTicks,
                         PARK_CREEP_SPEED, 0);
      targetSpeed = max(rampDown, 0);
    } else {
      targetSpeed = PARK_CREEP_SPEED;
    }

// Боковой ToF: финальная коррекция
#ifdef USE_VL53L1X
    if (tofSideOK && distSideMM < 300) {
      int sideErr = distSideMM - TOF_SIDE_TARGET_MM;
      int sideCorr = constrain(sideErr / 3, -10, 10);
      targetSteering =
          constrain(targetSteering + sideCorr * globalTrackDirection,
                    SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    }
#endif

    if (distTraveled >= targetTicks) {
      parkPhase = PARK_FINAL_STOP;
      Serial.println(">>> ПАРКОВКА: ОСТАНОВКА <<<");
    }
    break;
  }

  // ----- FINAL STOP -----
  case PARK_FINAL_STOP: {
    finishRace();
    break;
  }
  }
}

// ============================================================
//                    УПРАВЛЕНИЕ
// ============================================================
void updateControl() {
  if (raceFinished)
    return;
  if (encoderFaultActive) {
    safeStop();
    return;
  }

  if (currentState == STATE_SAFE_STOP) {
    targetSpeed = 0;
    targetSteering = SERVO_CENTER;
    return;
  }
  if (currentState == STATE_PARKING) {
    updateParking();
    return;
  }
  if (currentState == STATE_BLIND_TURN) {
    updateBlindTurn();
    return;
  }

// ===================== STATE_TRACKING =====================

// --- Экстренное торможение (фронтальный ToF) ---
#ifdef USE_VL53L1X
  if (tofFrontOK && distFrontMM < TOF_EMERGENCY_MM && distFrontMM > 0) {
    // СТЕНА ПРЯМО ПЕРЕД РОБОТОМ! Тормозим + поворачиваем
    targetSpeed = 0;
    if (directionConfirmed) {
      enterBlindTurn(globalTrackDirection);
    } else {
      enterBlindTurn(1); // CW по умолчанию
    }
    return;
  }
#endif

  // --- Камера offline → гироскоп ---
  if (!cameraOnline) {
    float hErr = targetHeading - yawAngle;
    while (hErr > 180)
      hErr -= 360;
    while (hErr < -180)
      hErr += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    targetSpeed = MOTOR_TURN_FAST;
    return;
  }

  // --- Детектирование близкой стены → blind turn ---
  if ((camModeFlag & CAM_FLAG_WALL) && directionConfirmed) {
    closeWallCount++;
  } else {
    closeWallCount = 0;
  }
  if (closeWallCount >= 3) {
    enterBlindTurn(globalTrackDirection);
    return;
  }

  // --- Объезд столбов (Obstacle Challenge) ---
  if (isObstacleMode && (camRedDist < 999 || camGreenDist < 999)) {
    if (newCameraData) {
      SteerResult sr = calcDualPillarSteering();
      targetSteering = sr.steeringAngle;

      // Динамическая скорость
      int turnAmount = abs(targetSteering - SERVO_CENTER);
      if (turnAmount > 30)
        targetSpeed = MOTOR_TURN_SLOW;
      else if (turnAmount > 15)
        targetSpeed = MOTOR_TURN_FAST;
      else
        targetSpeed = MOTOR_MAX_SPEED;

      targetSpeed = min(targetSpeed, sr.speedLimit);

// Замедление от фронтального ToF
#ifdef USE_VL53L1X
      if (tofFrontOK && distFrontMM < TOF_SLOW_DOWN_MM && distFrontMM > 0) {
        int tofSpeed = map(distFrontMM, TOF_EMERGENCY_MM, TOF_SLOW_DOWN_MM,
                           MOTOR_MIN_SPEED, MOTOR_TURN_FAST);
        tofSpeed = constrain(tofSpeed, MOTOR_MIN_SPEED, MOTOR_TURN_FAST);
        targetSpeed = min(targetSpeed, tofSpeed);
      }
#endif
    }
    targetHeading = yawAngle;

  } else {
    // Нет столбов → курс по гироскопу
    float hErr = targetHeading - yawAngle;
    while (hErr > 180)
      hErr -= 360;
    while (hErr < -180)
      hErr += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * hErr),
                               SERVO_MAX_LEFT, SERVO_MAX_RIGHT);

    // Скорость
    int turnAmount = abs(targetSteering - SERVO_CENTER);
    if (turnAmount > 30)
      targetSpeed = MOTOR_TURN_SLOW;
    else if (turnAmount > 15)
      targetSpeed = MOTOR_TURN_FAST;
    else
      targetSpeed = MOTOR_MAX_SPEED;

// Замедление от фронтального ToF
#ifdef USE_VL53L1X
    if (tofFrontOK && distFrontMM < TOF_SLOW_DOWN_MM && distFrontMM > 0) {
      int tofSpeed = map(distFrontMM, TOF_EMERGENCY_MM, TOF_SLOW_DOWN_MM,
                         MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
      tofSpeed = constrain(tofSpeed, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
      targetSpeed = min(targetSpeed, tofSpeed);
    }
#endif
  }
}

// ============================================================
//                    LIVE TUNING
// ============================================================
void checkSerialCommands() {
  static char cmdBuf[16];
  static uint8_t cmdPos = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      cmdBuf[cmdPos] = '\0';
      if (cmdPos > 1) {
        char t = cmdBuf[0];
        float v = atof(&cmdBuf[1]);
        if (t == 'P' || t == 'p') {
          PID_KP = v;
          Serial.print("Kp=");
          Serial.println(PID_KP);
        }
        if (t == 'D' || t == 'd') {
          PID_KD = v;
          Serial.print("Kd=");
          Serial.println(PID_KD);
        }
        if (t == 'I' || t == 'i') {
          PID_KI = v;
          Serial.print("Ki=");
          Serial.println(PID_KI);
          integralError = 0;
        }
        if (t == 'G' || t == 'g') {
          GYRO_KP = v;
          Serial.print("Gk=");
          Serial.println(GYRO_KP);
        }
      }
      cmdPos = 0;
    } else if (cmdPos < 15) {
      cmdBuf[cmdPos++] = c;
    }
  }
}

// ============================================================
//                       SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  delay(500);

  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║   WRO v10.0 CHAMPIONSHIP EDITION      ║");
  Serial.println("╚═══════════════════════════════════════╝");

  // Пины
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Режим
  isObstacleMode = (digitalRead(MODE_SWITCH_PIN) == LOW);
  Serial.print("РЕЖИМ: ");
  if (isObstacleMode) {
    Serial.println("OBSTACLE (ПРЕПЯТСТВИЯ)");
    MOTOR_MAX_SPEED = OBS_MAX_SPEED;
    MOTOR_TURN_FAST = OBS_TURN_FAST;
    MOTOR_TURN_SLOW = OBS_TURN_SLOW;
    MOTOR_MIN_SPEED = OBS_MIN_SPEED;
  } else {
    Serial.println("OPEN (СКОРОСТЬ)");
    MOTOR_MAX_SPEED = OPEN_MAX_SPEED;
    MOTOR_TURN_FAST = OPEN_TURN_FAST;
    MOTOR_TURN_SLOW = OPEN_TURN_SLOW;
    MOTOR_MIN_SPEED = OPEN_MIN_SPEED;
  }

  // UART → OpenMV
  Serial2.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  Serial2.setTimeout(10);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Wire.setWireTimeout(3000, true);

  // Серво
  steeringServo.attach(SERVO_PIN, 500, 2500);
  setSteering(SERVO_CENTER);

  // Мотор
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);
  ledcSetup(0, 20000, 8);
  ledcAttachPin(MOTOR_R_PWM, 0);
  ledcSetup(1, 20000, 8);
  ledcAttachPin(MOTOR_L_PWM, 1);
  setMotorSpeed(0);

  // IMU
  if (!tcaSelect(TCA_CH_IMU) || !icm.begin_I2C()) {
    Serial.println("ICM-20948 ОШИБКА!");
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  tcaDisable();
  calibrateGyro();

  // VL53L1X
  initToF();

  lastIMUTime = millis();
  lastCameraTime = millis();
  targetHeading = yawAngle;
  currentState = STATE_INIT;

  Serial.println("=== ГОТОВО ===");
  Serial.println("НАЖМИ E-STOP → ОТПУСТИ → СТАРТ!");
}

// ============================================================
//                        LOOP
// ============================================================
void loop() {
  checkSerialCommands();
  if (raceFinished) {
    handleFinishState();
    return;
  }

  static unsigned long lastLoop = 0;
  unsigned long now = millis();
  if (now - lastLoop < LOOP_INTERVAL)
    return;
  lastLoop += LOOP_INTERVAL; // FIX: инкрементальный тайминг, без дрейфа

  checkEStop();
  if (estopActive) {
    currentState = STATE_SAFE_STOP;
    safeStop();
    return;
  }

  // INIT → ждём кнопку
  if (currentState == STATE_INIT) {
    handleStartButton();
    readCameraData();
    updateYaw();
    readToF();
    return;
  }

  // === АКТИВНАЯ ГОНКА ===
  readCameraData();
  updateYaw();
  readToF();

  checkLapCountGyro();
  checkLapCountLine();
  checkLapCompletion();

  encLeft = readEncoder(TCA_CH_ENC_LEFT);
  encRight = readEncoder(TCA_CH_ENC_RIGHT);
  updateOdometry();
  if (encoderFaultActive) {
    currentState = STATE_SAFE_STOP;
    return;
  }

  updateControl();
  setSteering(targetSteering);
  applySpeedRamp();

  // Телеметрия
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= PRINT_INTERVAL) {
    lastPrint = millis();
    Serial.print("Y:");
    Serial.print(yawAngle, 1);
    Serial.print(" L:");
    Serial.print(lapCount);
    Serial.print("(");
    Serial.print(lineLapCount);
    Serial.print("/");
    Serial.print((int)(fabsf(totalRotation) / LAP_DEGREES));
    Serial.print(") R:");
    Serial.print(camRedDist);
    Serial.print(" G:");
    Serial.print(camGreenDist);
    Serial.print(" F:");
    Serial.print(distFrontMM);
    Serial.print(" S:");
    Serial.print(distSideMM);
    Serial.print(" M:");
    Serial.print(camModeFlag);
    Serial.print(" v:");
    Serial.print(commandSpeed);
    Serial.print(" s:");
    Serial.print(targetSteering);
    Serial.print(" ");
    switch (currentState) {
    case STATE_INIT:
      Serial.print("INIT");
      break;
    case STATE_TRACKING:
      Serial.print("TRK");
      break;
    case STATE_BLIND_TURN:
      Serial.print("BLN");
      break;
    case STATE_PARKING:
      Serial.print("PRK");
      Serial.print((int)parkPhase);
      break;
    case STATE_SAFE_STOP:
      Serial.print("STP");
      break;
    case STATE_FINISH:
      Serial.print("FIN");
      break;
    }
    Serial.println();
  }
}
