/*
 * WRO Future Engineers — AS5600 Encoder Test
 *
 * Проверяет оба энкодера AS5600 через TCA9548A.
 * Показывает в Serial Monitor:
 *   - Сырой угол (0–4095)
 *   - Угол в градусах (0.0–360.0)
 *   - Пройденные тики (с учётом перехода через 0)
 *   - Расстояние в см (если D колеса верный)
 *   - Статус магнита (найден / слабый / не найден)
 *
 * Как пользоваться:
 *   1. В wro_build_target.h: #define WRO_ACTIVE_TARGET WRO_TARGET_TEST_ENCODERS
 *   2. Upload, Serial Monitor 115200
 *   3. Крутить колёса руками — значения должны меняться плавно
 *
 * Что проверять:
 *   - Оба энкодера отвечают (нет "ERROR")
 *   - При повороте колеса значение растёт / убывает плавно
 *   - Нет скачков (скачки = магнит сдвинулся или плохой контакт I2C)
 *   - При повороте колеса на 1 оборот тики ≈ 4096
 */

#if __has_include("wro_build_target.h")
#include "wro_build_target.h"
#else
#define WRO_TARGET_TEST_ENCODERS 8
#define WRO_ACTIVE_TARGET WRO_TARGET_TEST_ENCODERS
#endif

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS

#include <Arduino.h>
#include <Wire.h>

// ── I2C пины ────────────────────────────────────────────────
#define I2C_SDA       21
#define I2C_SCL       22

// ── TCA9548A ────────────────────────────────────────────────
#define TCA_ADDRESS       0x70
#define TCA_CH_ENC_LEFT   1    // AS5600 левый
#define TCA_CH_ENC_RIGHT  2    // AS5600 правый

// ── AS5600 ──────────────────────────────────────────────────
#define AS5600_ADDR       0x36
#define REG_RAW_ANGLE_H   0x0C   // ANGLE (filtered), high byte
#define REG_RAW_ANGLE_H2  0x0E   // RAW ANGLE (unfiltered), high byte
#define REG_STATUS        0x0B   // Magnet status register
#define REG_AGC           0x1A   // Automatic gain control

// ── Одометрия ───────────────────────────────────────────────
// AS5600: 4096 тиков/оборот. Колесо D = 47 мм → C = π × 47 = 147.65 мм
// TICKS_PER_CM = 4096 / 14.765 ≈ 277
#define WHEEL_CIRC_MM       146.0
#define TICKS_PER_REV       4096

const float ticksPerCM = TICKS_PER_REV / (WHEEL_CIRC_MM / 10.0);

// ── Состояние ───────────────────────────────────────────────
int  prevLeft  = -1, prevRight  = -1;
long totalLeft = 0,  totalRight = 0;

// ═════════════════════════════════════════════════════════════
//  TCA9548A — выбор канала
// ═════════════════════════════════════════════════════════════
bool tcaSelect(uint8_t ch) {
  if (ch > 7) return false;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << ch);
  return (Wire.endTransmission() == 0);
}

void tcaDisable() {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
}

// ═════════════════════════════════════════════════════════════
//  AS5600 — чтение угла (12 бит, 0..4095)
// ═════════════════════════════════════════════════════════════
int readAngle(uint8_t tcaCh) {
  if (!tcaSelect(tcaCh)) return -1;

  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_RAW_ANGLE_H2);
  if (Wire.endTransmission(false) != 0) { tcaDisable(); return -1; }

  uint8_t got = Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
  if (got >= 2 && Wire.available() >= 2) {
    int h = Wire.read();
    int l = Wire.read();
    tcaDisable();
    return ((h & 0x0F) << 8) | l;
  }
  tcaDisable();
  return -1;
}

// ═════════════════════════════════════════════════════════════
//  AS5600 — статус магнита
// ═════════════════════════════════════════════════════════════
// Регистр STATUS (0x0B):
//   bit 5 (MD) = magnet detected
//   bit 4 (ML) = magnet too weak
//   bit 3 (MH) = magnet too strong
String readMagnetStatus(uint8_t tcaCh) {
  if (!tcaSelect(tcaCh)) return "TCA ERROR";

  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_STATUS);
  if (Wire.endTransmission(false) != 0) { tcaDisable(); return "I2C ERROR"; }

  uint8_t got = Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1);
  if (got < 1 || !Wire.available()) { tcaDisable(); return "NO RESPONSE"; }

  uint8_t status = Wire.read();
  tcaDisable();

  bool detected  = status & 0x20;  // MD
  bool tooWeak   = status & 0x10;  // ML
  bool tooStrong = status & 0x08;  // MH

  if (!detected)  return "NO MAGNET!";
  if (tooWeak)    return "WEAK (raise magnet closer)";
  if (tooStrong)  return "TOO STRONG (lower magnet)";
  return "OK";
}

// ═════════════════════════════════════════════════════════════
//  AS5600 — AGC (automatic gain control)
// ═════════════════════════════════════════════════════════════
// AGC 0 = max gain (magnet far), 255 = min gain (magnet close).
// Ideal range: 30–220.
int readAGC(uint8_t tcaCh) {
  if (!tcaSelect(tcaCh)) return -1;

  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_AGC);
  if (Wire.endTransmission(false) != 0) { tcaDisable(); return -1; }

  uint8_t got = Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1);
  if (got < 1 || !Wire.available()) { tcaDisable(); return -1; }

  uint8_t agc = Wire.read();
  tcaDisable();
  return agc;
}

// ═════════════════════════════════════════════════════════════
//  Одометрия — подсчёт тиков с переходом через 0
// ═════════════════════════════════════════════════════════════
void updateOdometry(int rawLeft, int rawRight) {
  if (prevLeft < 0 || prevRight < 0) {
    prevLeft  = rawLeft;
    prevRight = rawRight;
    return;
  }

  int dL = rawLeft  - prevLeft;
  int dR = rawRight - prevRight;

  if (dL >  2048) dL -= 4096;
  if (dL < -2048) dL += 4096;
  if (dR >  2048) dR -= 4096;
  if (dR < -2048) dR += 4096;

  totalLeft  += dL;
  totalRight += dR;
  prevLeft  = rawLeft;
  prevRight = rawRight;
}

// ═════════════════════════════════════════════════════════════
//  I2C сканер — проверка TCA и датчиков
// ═════════════════════════════════════════════════════════════
void scanBus() {
  Serial.println("\n=== I2C SCAN ===");

  Wire.beginTransmission(TCA_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("  TCA9548A @ 0x70 — NOT FOUND!");
    Serial.println("  Check: VIN → 3.3V, GND, SDA → GPIO21, SCL → GPIO22");
    Serial.println("  Check: A0, A1, A2 → GND");
    return;
  }
  Serial.println("  TCA9548A @ 0x70 — OK");

  // CH1 — left encoder
  Serial.print("  CH1 (left  AS5600): ");
  if (tcaSelect(TCA_CH_ENC_LEFT)) {
    Wire.beginTransmission(AS5600_ADDR);
    if (Wire.endTransmission() == 0) Serial.println("FOUND");
    else Serial.println("NOT FOUND — check SD1/SC1 wiring");
  } else Serial.println("TCA channel error");

  // CH2 — right encoder
  Serial.print("  CH2 (right AS5600): ");
  if (tcaSelect(TCA_CH_ENC_RIGHT)) {
    Wire.beginTransmission(AS5600_ADDR);
    if (Wire.endTransmission() == 0) Serial.println("FOUND");
    else Serial.println("NOT FOUND — check SD2/SC2 wiring");
  } else Serial.println("TCA channel error");

  tcaDisable();
  Serial.println();
}

// ═════════════════════════════════════════════════════════════
//  Setup
// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Wire.setWireTimeout(3000, true);

  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  WRO AS5600 ENCODER TEST               ║");
  Serial.println("║  Left = TCA CH1, Right = TCA CH2       ║");
  Serial.print("║  Wheel D = ");
  Serial.print(WHEEL_DIAMETER_MM);
  Serial.print(" mm, ticks/cm = ");
  Serial.println(ticksPerCM);
  Serial.println("╚════════════════════════════════════════╝");

  scanBus();

  String magL = readMagnetStatus(TCA_CH_ENC_LEFT);
  String magR = readMagnetStatus(TCA_CH_ENC_RIGHT);
  int agcL = readAGC(TCA_CH_ENC_LEFT);
  int agcR = readAGC(TCA_CH_ENC_RIGHT);

  Serial.println("=== MAGNET STATUS ===");
  Serial.print("  Left  magnet: "); Serial.print(magL);
  Serial.print("  (AGC="); Serial.print(agcL); Serial.println(")");
  Serial.print("  Right magnet: "); Serial.print(magR);
  Serial.print("  (AGC="); Serial.print(agcR); Serial.println(")");
  Serial.println();

  if (magL != "OK" || magR != "OK") {
    Serial.println("  ⚠ Fix magnet issues before continuing!");
    Serial.println("  Magnet should be 0.5–3 mm from AS5600 chip, centered.");
    Serial.println();
  }

  Serial.println("=== LIVE DATA (every 200ms) ===");
  Serial.println("  Spin the wheels by hand — values should change smoothly.");
  Serial.println("  1 full wheel revolution = 4096 ticks.");
  Serial.println();
  Serial.println("  LEFT                                    RIGHT");
  Serial.println("  raw    deg    ticks   cm   mag         raw    deg    ticks   cm   mag");
  Serial.println("  ─────────────────────────────────────  ─────────────────────────────────────");
}

// ═════════════════════════════════════════════════════════════
//  Loop — выводим данные каждые 200 мс
// ═════════════════════════════════════════════════════════════
void loop() {
  int rawL = readAngle(TCA_CH_ENC_LEFT);
  int rawR = readAngle(TCA_CH_ENC_RIGHT);

  if (rawL >= 0 && rawR >= 0) {
    updateOdometry(rawL, rawR);
  }

  float degL = (rawL >= 0) ? rawL * 360.0f / 4096.0f : -1;
  float degR = (rawR >= 0) ? rawR * 360.0f / 4096.0f : -1;
  float cmL  = totalLeft  / ticksPerCM;
  float cmR  = totalRight / ticksPerCM;

  // Left side
  if (rawL >= 0) {
    Serial.print("  ");
    Serial.print(rawL);    Serial.print("\t");
    Serial.print(degL, 1); Serial.print("°\t");
    Serial.print(totalLeft); Serial.print("\t");
    Serial.print(cmL, 1);   Serial.print("cm");
  } else {
    Serial.print("  ERROR\t-\t-\t-");
  }

  Serial.print("\t\t");

  // Right side
  if (rawR >= 0) {
    Serial.print(rawR);     Serial.print("\t");
    Serial.print(degR, 1);  Serial.print("°\t");
    Serial.print(totalRight); Serial.print("\t");
    Serial.print(cmR, 1);    Serial.print("cm");
  } else {
    Serial.print("ERROR\t-\t-\t-");
  }

  Serial.println();
  delay(200);
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS
