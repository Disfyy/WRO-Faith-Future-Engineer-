/*
 * WRO Future Engineers — Главный код ESP32
 * Версия 8.7 — RACE DAY PRO (Obstacle & Open Challenge)
 *
 * Железо: ESP32 DevKitC V4, BTS7960, JX PDI-6221MG,
 *         TCA9548A, ICM-20948, AS5600 x2, OpenMV H7 Plus,
 *         VL53L1X x2 (через TCA CH3/CH4)
 *
 * Ключевые изменения 8.7:
 * - Поддержка двух режимов (Open/Obstacle Challenge) через тумблер (PIN 4).
 * - Динамическое смещение (Dynamic Offset) ПИД для плавного объезда столбов.
 * - Одометрия финишной зоны: точная остановка ровно через 3 круга.
 * - Hardware I2C Timeout (3000 мс) для защиты от зависаний TCA/AS5600 от помех мотора.
 * - Конечный автомат: INIT, TRACKING, BLIND_TURN, SAFE_STOP, FINISH
 * - 3-польный UART протокол: errorX,distance,objectType\n
 * - Жёсткая защита: dt clamp IMU, остаток поворота для кругов,
 *   блокировка движения при критической потере энкодеров.
 */

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// ==============================
// ПИНЫ
// ==============================
#define I2C_SDA          21
#define I2C_SCL          22
#define SERVO_PIN        18
#define MOTOR_R_EN       19
#define MOTOR_L_EN       23
#define MOTOR_R_PWM       5
#define MOTOR_L_PWM      14
#define UART_RX          16
#define UART_TX          17
#define ESTOP_PIN        32
#define MODE_SWITCH_PIN   4  // ТУМБЛЕР РЕЖИМА: LOW = OBSTACLE, HIGH = OPEN
#define LED_PIN           2

// ==============================
// НАСТРОЙКИ
// ==============================
#define SERVO_CENTER     90
#define SERVO_MAX_RIGHT 135
#define SERVO_MAX_LEFT   45
#define MOTOR_MAX_SPEED 150
#define MOTOR_TURN_FAST 120
#define MOTOR_TURN_SLOW  80
#define MOTOR_MIN_SPEED  35

#define TARGET_LAPS       3
#define LAP_DEGREES     360.0f
#define FINISH_ZONE_CM  100  // Расстояние стартовой зоны для плавной остановки

// ==============================
// PID (камера/курс) и гироскоп
// ==============================
float PID_KP  = 0.50f;
float PID_KI  = 0.001f;
float PID_KD  = 0.10f;
float GYRO_KP = 1.20f;
float integralError = 0.0f;
int dynamicErrorOffset = 0;  // Смещение ПИД для объезда препятствий

// ==============================
// I2C
// ==============================
#define TCA_ADDRESS      0x70
#define AS5600_ADDRESS   0x36
#define TCA_CH_IMU        0
#define TCA_CH_ENC_LEFT   1
#define TCA_CH_ENC_RIGHT  2
// TCA_CH_VL53_LEFT/RIGHT (3,4) reserved for future distance sensors

// ==============================
// ТАЙМАУТЫ И ОДОМЕТРИЯ
// ==============================
#define CAMERA_TIMEOUT       500
#define PRINT_INTERVAL       200
#define LOOP_INTERVAL         10
#define GYRO_CALIB_SAMPLES   300
#define FINISH_BLINK_MS      500
#define ESTOP_DEBOUNCE_MS     20
#define MAX_IMU_DT_SEC     0.05f
#define ENCODER_FAIL_STOP    50
#define SPEED_RAMP_STEP       8
#define BLIND_TURN_TIMEOUT  3000
#define BLIND_TURN_ANGLE      85.0f
#define ENCODER_TURN_TICKS  3000

// ==============================
// ОБЪЕКТЫ
// ==============================
Servo steeringServo;
Adafruit_ICM20948 icm;

// ==============================
// КАМЕРА
// ==============================
int   cameraErrorX      = 0;
int   cameraDistance    = 0;
int   cameraObjectType  = 0; // 0=none,1=orange,2=blue,3=red,4=green
bool  cameraOnline      = false;
bool  newCameraData     = false;
unsigned long lastCameraTime = 0;
int   lastKnownDistance = 999;
int   lastObjectType    = 0;

#define UART_BUF_SIZE 32
static char    uartBuf[UART_BUF_SIZE];
static uint8_t uartBufPos = 0;

// ==============================
// ГИРОСКОП / МАГНИТОМЕТР
// ==============================
float yawAngle      = 0.0f;
float gyroZbias     = 0.0f;
float totalRotation = 0.0f;
float lastYaw       = 0.0f;
unsigned long lastIMUTime = 0;
float targetHeading = 0.0f;
float magBiasX      = 0.0f;
float magBiasY      = 0.0f;

// ==============================
// ОДОМЕТРИЯ
// ==============================
int  encLeft        = 0;
int  encRight       = 0;
int  encLeftPrev    = 0;
int  encRightPrev   = 0;
long totalDistLeft  = 0;
long totalDistRight = 0;
int  encErrorCount  = 0;
bool encoderFaultActive = false;

// ==============================
// СОСТОЯНИЯ И УПРАВЛЕНИЕ
// ==============================
int  targetSteering  = SERVO_CENTER;
int  targetSpeed     = 0;
int  commandSpeed    = 0; // после плавного ramp
int  lastCameraError = 0;

int  globalTrackDirection = 1; 
bool directionConfirmed   = false; 

long turnStartDistLeft    = 0;
long turnStartDistRight   = 0;
int  closeWallCount       = 0; 

enum RobotState {
  STATE_INIT,
  STATE_TRACKING,
  STATE_BLIND_TURN,
  STATE_SAFE_STOP,
  STATE_FINISH
};
RobotState currentState = STATE_INIT;

float turnStartYaw  = 0.0f;
int   turnDirection = 1; 
unsigned long turnStartTime = 0;

// ==============================
// ГОНКА
// ==============================
int  lapCount       = 0;
bool raceFinished   = false;
bool estopActive    = false;
bool isObstacleMode = false;
long odometryStartLaps = 0;  // Колесные тики с момента начала контроля кругов

// ==============================
// УТИЛИТЫ
// ==============================
bool tcaSelect(uint8_t channel) {
  if (channel > 7) return false;
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
  if (speed == 0) return 0;
  return map(abs(speed), 1, MOTOR_MAX_SPEED, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
}

void setMotorSpeed(int speed) {
  speed = constrain(speed, -MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);
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
  if (commandSpeed < targetSpeed) {
    commandSpeed = min(commandSpeed + SPEED_RAMP_STEP, targetSpeed);
  } else if (commandSpeed > targetSpeed) {
    commandSpeed = max(commandSpeed - SPEED_RAMP_STEP, targetSpeed);
  }
  setMotorSpeed(commandSpeed);
}

void safeStop() {
  targetSpeed        = 0;
  commandSpeed       = 0;
  setMotorSpeed(0);
  targetSteering     = SERVO_CENTER;
  setSteering(SERVO_CENTER);
  lastCameraError    = 0;
  integralError      = 0;
  newCameraData      = false;
  cameraObjectType   = 0;
  closeWallCount     = 0;
  directionConfirmed = false;
  lastObjectType     = 0;
  currentState       = STATE_SAFE_STOP;
}

// ==============================
// E-STOP
// ==============================
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
      Serial.println("=== АВАРИЙНАЯ ОСТАНОВКА (E-STOP) ===");
    }
  }

  if (estopActive && !pinLow) {
    estopActive = false;
    estopArmed  = false;
    currentState   = STATE_TRACKING;  // CRITICAL FIX: reset state to allow resume
    lastIMUTime    = millis();
    lastCameraTime = millis();
    lastYaw        = yawAngle;
    targetHeading  = yawAngle;
    Serial.println("=== ПРОДОЛЖЕНИЕ ГОНКИ ===");
  }

  if (estopActive) safeStop();
}

// ==============================
// ФИНИШ
// ==============================
void finishRace() {
  safeStop();
  raceFinished = true;
  currentState = STATE_FINISH;
  Serial.println("╔══════════════════════════╗");
  Serial.println("║   ФИНИШ! 3 КРУГА!        ║");
  Serial.println("╚══════════════════════════╝");
}

void handleFinishState() {
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > FINISH_BLINK_MS) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  checkEStop();
}

// ==============================
// IMU
// ==============================
void calibrateGyro() {
  Serial.println("Калибровка гироскопа (3 сек)...");
  Serial.println("Не двигай робота!");
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
  Serial.print("Bias: "); Serial.println(gyroZbias, 6);
  Serial.println("Калибровка завершена!");
}

void updateYaw() {
  unsigned long now = millis();
  if (lastIMUTime == 0) { lastIMUTime = now; return; }

  float dt = (now - lastIMUTime) / 1000.0f;
  lastIMUTime = now;
  if (dt <= 0.0f) return;
  if (dt > MAX_IMU_DT_SEC) dt = MAX_IMU_DT_SEC;

  tcaSelect(TCA_CH_IMU);
  sensors_event_t accel, gyro, mag, temp;
  bool ok = icm.getEvent(&accel, &gyro, &temp, &mag);
  tcaDisable();
  if (!ok) return;

  float correctedZ = gyro.gyro.z - gyroZbias;
  if (fabsf(correctedZ) < 0.01f) correctedZ = 0.0f;

  yawAngle += correctedZ * dt * (180.0f / PI);
  while (yawAngle >  180) yawAngle -= 360;
  while (yawAngle < -180) yawAngle += 360;

  // Простая коррекция магнитометром (при необходимости откалибровать magBias)
  float magX = mag.magnetic.x - magBiasX;
  float magY = mag.magnetic.y - magBiasY;
  float magHeading = atan2f(magY, magX) * (180.0f / PI);
  float magError = magHeading - yawAngle;
  while (magError >  180) magError -= 360;
  while (magError < -180) magError += 360;
  yawAngle += 0.02f * magError;
  while (yawAngle >  180) yawAngle -= 360;
  while (yawAngle < -180) yawAngle += 360;
}

void checkLapCount() {
  float deltaYaw = yawAngle - lastYaw;
  while (deltaYaw >  180) deltaYaw -= 360;
  while (deltaYaw < -180) deltaYaw += 360;

  totalRotation += deltaYaw;
  lastYaw = yawAngle;

  if (fabsf(totalRotation) >= LAP_DEGREES) {
    lapCount++;
    if (totalRotation > 0) totalRotation -= LAP_DEGREES;
    else                   totalRotation += LAP_DEGREES;
    Serial.print("=== КРУГ "); Serial.print(lapCount);
    Serial.print(" ИЗ ");      Serial.print(TARGET_LAPS);
    Serial.println(" ===");
    
    // Включаем алгоритм точной парковки (одометрия стартовой зоны)
    if (lapCount >= TARGET_LAPS) {
      odometryStartLaps = totalDistLeft;  // Захватываем значение отсчета, как пройдем линию
      Serial.println(">>> АКТИВИРОВАН ОДОМЕТР ФИНИШНОЙ ЗОНЫ! Паркуемся... <<<");
    }
  }

  // Если проехали нужное кол-во кругов, докатываемся N сантиметров (например, 70-80 см по тикам) внутри стартовой зоны
  if (lapCount >= TARGET_LAPS) {
      long distSinceLine = abs(totalDistLeft - odometryStartLaps);
      if (distSinceLine > (FINISH_ZONE_CM * 40)) { // ~40 тиков энкодера на 1 см дистанции (подберите коэффициент!)
          finishRace();
      } else {
          // Плавное торможение перед финишем
          targetSpeed = MOTOR_TURN_SLOW; 
          commandSpeed = min(commandSpeed, MOTOR_TURN_SLOW);
      }
  }
}

// ==============================
// ОДОМЕТРИЯ
// ==============================
int readEncoder(uint8_t tcaChannel) {
  if (!tcaSelect(tcaChannel)) return -1;
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(0x0E);
  if (Wire.endTransmission(false) != 0) {
    tcaDisable();
    return -1;
  }
  uint8_t got = Wire.requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)2);
  if (got >= 2 && Wire.available() >= 2) {
    int high = Wire.read();
    int low  = Wire.read();
    tcaDisable();
    return ((high & 0x0F) << 8) | low;
  }
  tcaDisable();
  return -1;
}

void updateOdometry() {
  if (encLeft < 0 || encRight < 0) {
    encErrorCount++;
    if (encErrorCount == 10) {
      Serial.println("ALARM: ПОТЕРЯ ЭНКОДЕРОВ!");
      encLeftPrev  = (encLeft  < 0) ? 0 : encLeft;
      encRightPrev = (encRight < 0) ? 0 : encRight;
    }
    if (encErrorCount >= ENCODER_FAIL_STOP) {
      encoderFaultActive = true;
      Serial.println("CRITICAL: ЭНКОДЕРЫ OFFLINE — SAFE STOP");
      safeStop();
    }
    return;
  }

  if (encoderFaultActive) {
    encoderFaultActive = false;
    Serial.println("INFO: ЭНКОДЕРЫ ВОССТАНОВЛЕНЫ, движение разрешено");
  }

  if (encErrorCount >= 10) {
    Serial.println("ЭНКОДЕРЫ ВОССТАНОВЛЕНЫ!");
    encLeftPrev  = encLeft;
    encRightPrev = encRight;
  }
  encErrorCount = 0;

  int deltaLeft = encLeft - encLeftPrev;
  if (deltaLeft >  2048) deltaLeft -= 4096;
  if (deltaLeft < -2048) deltaLeft += 4096;

  int deltaRight = encRight - encRightPrev;
  if (deltaRight >  2048) deltaRight -= 4096;
  if (deltaRight < -2048) deltaRight += 4096;

  totalDistLeft  += deltaLeft;
  totalDistRight += deltaRight;

  encLeftPrev  = encLeft;
  encRightPrev = encRight;
}

// ==============================
// КАМЕРА (3 поля: errorX,distance,objectType)
// ==============================
void readCameraData() {
  newCameraData = false;

  while (Serial2.available()) {
    char c = (char)Serial2.read();

    if (c == '\r') continue;

    if (c == '\n') {
      uartBuf[uartBufPos] = '\0';
      uartBufPos = 0;
      if (strlen(uartBuf) == 0) continue;

      char *comma1 = strchr(uartBuf, ',');
      if (!comma1) continue;
      *comma1 = '\0';
      char *comma2 = strchr(comma1 + 1, ',');
      if (!comma2) continue;
      *comma2 = '\0';

      int parsedErrorX     = atoi(uartBuf);
      int parsedDistance   = atoi(comma1 + 1);
      int parsedObjectType = atoi(comma2 + 1);

      if (parsedErrorX >= -160 && parsedErrorX <= 160 &&
          parsedDistance >= 0 && parsedDistance <= 10000) {
        cameraErrorX     = parsedErrorX;
        cameraDistance   = parsedDistance;
        cameraObjectType = parsedObjectType;
        lastCameraTime   = millis();
        cameraOnline     = true;
        newCameraData    = true;
        lastKnownDistance = cameraDistance;
      }

    } else {
      if (uartBufPos < UART_BUF_SIZE - 1) {
        uartBuf[uartBufPos++] = c;
      } else {
        uartBufPos = 0;
      }
    }
  }

  if (millis() - lastCameraTime > CAMERA_TIMEOUT) {
    if (cameraOnline) Serial.println("WARN: Камера потеряна! Переход на курс по гироскопу.");
    cameraOnline     = false;
    cameraObjectType = 0;
  }
}

// ==============================
// УПРАВЛЕНИЕ
// ==============================
void enterBlindTurn(int direction) {
  currentState      = STATE_BLIND_TURN;
  turnStartYaw      = yawAngle;
  turnDirection     = direction;
  turnStartTime     = millis();
  turnStartDistLeft  = totalDistLeft;
  turnStartDistRight = totalDistRight;
  integralError      = 0;
  closeWallCount     = 0;
  Serial.print(">>> СТАРТ СЛЕПОГО ПОВОРОТА | В сторону: ");
  Serial.println(turnDirection == 1 ? "ПРАВО" : "ЛЕВО");
}

bool updateBlindTurn() {
  if (millis() - turnStartTime > BLIND_TURN_TIMEOUT) {
    currentState    = STATE_TRACKING;
    targetHeading   = yawAngle;
    closeWallCount  = 0;
    lastCameraError = 0;
    Serial.println(">>> ТАЙМАУТ ПОВОРОТА (АВАРИЙНЫЙ ВЫХОД) <<<");
    return true;
  }

  targetSteering = (turnDirection == 1) ? SERVO_MAX_RIGHT : SERVO_MAX_LEFT;
  targetSpeed    = MOTOR_TURN_SLOW;

  float turnedAngle = yawAngle - turnStartYaw;
  while (turnedAngle >  180) turnedAngle -= 360;
  while (turnedAngle < -180) turnedAngle += 360;

  long distTraveled = ((totalDistLeft - turnStartDistLeft) + (totalDistRight - turnStartDistRight)) / 2;

  if (fabsf(turnedAngle) >= BLIND_TURN_ANGLE || labs(distTraveled) > ENCODER_TURN_TICKS) {
    currentState    = STATE_TRACKING;
    targetHeading   = yawAngle;
    closeWallCount  = 0;
    lastCameraError = 0;
    Serial.println(">>> КОНЕЦ СЛЕПОГО ПОВОРОТА <<<");
    return true;
  }
  return false;
}

void updateControl() {
  if (raceFinished) return;
  if (encoderFaultActive) { safeStop(); return; }

  // SAFE_STOP state keeps holding until cleared
  if (currentState == STATE_SAFE_STOP) {
    targetSpeed   = 0;
    targetSteering= SERVO_CENTER;
    return;
  }

  // BLIND_TURN state
  if (currentState == STATE_BLIND_TURN) {
    updateBlindTurn();
    return;
  }

  // TRACKING with camera offline -> курс по гироскопу
  if (!cameraOnline) {
    float headingError = targetHeading - yawAngle;
    while (headingError >  180) headingError -= 360;
    while (headingError < -180) headingError += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * headingError), SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    targetSpeed    = (lastKnownDistance < 80) ? MOTOR_TURN_SLOW : MOTOR_TURN_FAST;
    return;
  }

  // TRACKING with camera online
  if (cameraObjectType == 1) { globalTrackDirection =  1; directionConfirmed = true; }
  if (cameraObjectType == 2) { globalTrackDirection = -1; directionConfirmed = true; }

  if (cameraDistance > 0 && cameraDistance < 35 && directionConfirmed) closeWallCount++; else closeWallCount = 0;

  if (closeWallCount >= 3) {
    enterBlindTurn(globalTrackDirection);
    return;
  }

  // Обнуляем динамическое смещение для Open Challenge
  dynamicErrorOffset = 0;

  // Динамическое смещение для ОБЪЕЗДА ПРЕПЯТСТВИЙ (Obstacle Challenge)
  // Объезжаем только если мод активен и мы видим цилиндр
  if (isObstacleMode && (cameraObjectType == 3 || cameraObjectType == 4)) {
    if (cameraDistance > 0 && cameraDistance < 60) {
      if (cameraObjectType == 3) {
        // Красный цилиндр (Red) -> смещение -35 (ехать левее объекта, оставляя его справа)
        dynamicErrorOffset = -35; 
      } else if (cameraObjectType == 4) {
        // Зелёный цилиндр (Green) -> смещение +35 (ехать правее объекта, оставляя его слева)
        dynamicErrorOffset = 35;
      }
    }
  }

  // PID по камере / объекту
  if (cameraObjectType != 0) {
    if (newCameraData) {
      int avoidError = cameraErrorX;
      
      // Игнорируем хардкорные смещения, оставляем только плавное:
      avoidError += dynamicErrorOffset;

      if (cameraObjectType != lastObjectType) {
        integralError   = 0;
        lastCameraError = avoidError;
      }
      lastObjectType = cameraObjectType;

      int errorDerivative = avoidError - lastCameraError;
      integralError += avoidError;
      integralError = constrain(integralError, -500, 500); // Урезаем windup до 500 для безопасности
      lastCameraError = avoidError;

      int steeringCorrection = (int)(PID_KP * avoidError + PID_KI * integralError + PID_KD * errorDerivative);
      targetSteering = constrain(SERVO_CENTER + steeringCorrection, SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
    }
    targetHeading = yawAngle;

  } else {
    float headingError = targetHeading - yawAngle;
    while (headingError >  180) headingError -= 360;
    while (headingError < -180) headingError += 360;
    targetSteering = constrain(SERVO_CENTER + (int)(GYRO_KP * headingError), SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
  }

  // Динамическая скорость
  int turnAmount = abs(targetSteering - SERVO_CENTER);
  if      (turnAmount > 30) targetSpeed = MOTOR_TURN_SLOW;
  else if (turnAmount > 15) targetSpeed = MOTOR_TURN_FAST;
  else                      targetSpeed = MOTOR_MAX_SPEED;

  if (cameraDistance > 0 && cameraDistance < 100) {
    int rampSpeed = map(cameraDistance, 35, 100, MOTOR_TURN_SLOW, MOTOR_MAX_SPEED);
    rampSpeed = constrain(rampSpeed, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
    targetSpeed = min(targetSpeed, rampSpeed);
  }
}

// ==============================
// LIVE TUNING
// ==============================
void checkSerialCommands() {
  static char    cmdBuf[16];
  static uint8_t cmdPos = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      cmdBuf[cmdPos] = '\0';
      if (cmdPos > 1) {
        char  type = cmdBuf[0];
        float val  = atof(&cmdBuf[1]);
        if (type == 'P' || type == 'p') { PID_KP = val; Serial.print("Kp = "); Serial.println(PID_KP); }
        if (type == 'D' || type == 'd') { PID_KD = val; Serial.print("Kd = "); Serial.println(PID_KD); }
        if (type == 'I' || type == 'i') { PID_KI = val; Serial.print("Ki = "); Serial.println(PID_KI); integralError = 0; }
      }
      cmdPos = 0;
    } else if (cmdPos < 15) {
      cmdBuf[cmdPos++] = c;
    }
  }
}

// ==============================
// SETUP
// ==============================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  delay(500);

  Serial.println("=== WRO V8.7 RACE DAY PRO (Obstacle/Open) ===");

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  isObstacleMode = (digitalRead(MODE_SWITCH_PIN) == LOW);
  Serial.print("РЕЖИМ ГОНКИ: "); 
  Serial.println(isObstacleMode ? "OBSTACLE CHALLENGE (ПРЕПЯТСТВИЯ)" : "OPEN CHALLENGE (СКОРОСТЬ)");

  Serial2.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  Serial2.setTimeout(10);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Wire.setWireTimeout(3000, true); // I2C Hardware Timeout 3мс с автоматическим сбросом шины

  steeringServo.attach(SERVO_PIN, 500, 2500);
  setSteering(SERVO_CENTER);

  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);
  ledcSetup(0, 20000, 8); ledcAttachPin(MOTOR_R_PWM, 0);
  ledcSetup(1, 20000, 8); ledcAttachPin(MOTOR_L_PWM, 1);
  setMotorSpeed(0);

  if (!tcaSelect(TCA_CH_IMU) || !icm.begin_I2C()) {
    Serial.println("ICM-20948 ОШИБКА!");
    while (true);
  }
  tcaDisable();

  calibrateGyro();

  lastIMUTime    = millis();
  lastCameraTime = millis();
  targetHeading  = yawAngle;

  currentState = STATE_INIT;
  Serial.println("=== СИСТЕМА ГОТОВА ===");
}

// ==============================
// LOOP
// ==============================
void loop() {
  checkSerialCommands();

  if (raceFinished) { handleFinishState(); return; }

  static unsigned long lastLoopTime = 0;
  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL) return;
  lastLoopTime = now;

  checkEStop();
  if (estopActive) { currentState = STATE_SAFE_STOP; safeStop(); return; }

  readCameraData();
  updateYaw();
  checkLapCount();

  encLeft  = readEncoder(TCA_CH_ENC_LEFT);
  encRight = readEncoder(TCA_CH_ENC_RIGHT);
  updateOdometry();
  if (encoderFaultActive) { currentState = STATE_SAFE_STOP; return; }

  if (currentState == STATE_INIT) {
    currentState = STATE_TRACKING; // после инициализации переходим в трекинг
  }

  updateControl();
  setSteering(targetSteering);
  applySpeedRamp();

  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = millis();
    Serial.print("Yaw: ");    Serial.print(yawAngle, 1);
    Serial.print(" | Lap: "); Serial.print(lapCount);
    Serial.print(" | Dist: "); Serial.print(cameraDistance);
    Serial.print(" | State: ");
    if (currentState == STATE_INIT) Serial.print("INIT");
    else if (currentState == STATE_TRACKING) Serial.print("TRACK");
    else if (currentState == STATE_BLIND_TURN) Serial.print("BLIND");
    else if (currentState == STATE_SAFE_STOP) Serial.print("SAFE");
    else if (currentState == STATE_FINISH) Serial.print("FINISH");
    else Serial.print("?");
    Serial.print(" | DirConfirmed: "); Serial.println(directionConfirmed ? "YES" : "NO");
  }
}
