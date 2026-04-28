// WRO — Тест двух AS5600 энкодеров на двух I2C шинах ESP32
//
// Шина 1 (Wire):  SDA=21, SCL=22  → ЛЕВЫЙ энкодер
// Шина 2 (Wire1): SDA=25, SCL=26  → ПРАВЫЙ энкодер
//
// Serial Monitor: 115200 baud
// Крути колёса руками — значения должны меняться плавно.

#include <Wire.h>

#define SDA1 21
#define SCL1 22
#define SDA2 25
#define SCL2 26

#define AS5600_ADDR       0x36
#define REG_RAW_ANGLE_H2  0x0E
#define REG_STATUS        0x0B
#define REG_AGC           0x1A

// Длина окружности одного колеса (1 круг = 14.6 см = 146.0 мм)
#define WHEEL_CIRC_MM 146.0
#define TICKS_PER_REV 4096

const float ticksPerCM = TICKS_PER_REV / (WHEEL_CIRC_MM / 10.0);

int  prevLeft  = -1, prevRight  = -1;
long totalLeft = 0,  totalRight = 0;

int readAngle(TwoWire &bus) {
  bus.beginTransmission(AS5600_ADDR);
  bus.write(REG_RAW_ANGLE_H2);
  if (bus.endTransmission(false) != 0) return -1;
  if (bus.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) >= 2) {
    int h = bus.read();
    int l = bus.read();
    return ((h & 0x0F) << 8) | l;
  }
  return -1;
}

String readMagnet(TwoWire &bus) {
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

int readAGC(TwoWire &bus) {
  bus.beginTransmission(AS5600_ADDR);
  bus.write(REG_AGC);
  if (bus.endTransmission(false) != 0) return -1;
  if (bus.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1) < 1) return -1;
  return bus.read();
}

bool checkPresent(TwoWire &bus) {
  bus.beginTransmission(AS5600_ADDR);
  return (bus.endTransmission() == 0);
}

void updateOdometry(int rawL, int rawR) {
  if (prevLeft < 0) { prevLeft = rawL; prevRight = rawR; return; }
  int dL = rawL - prevLeft;
  int dR = rawR - prevRight;
  if (dL >  2048) dL -= 4096;
  if (dL < -2048) dL += 4096;
  if (dR >  2048) dR -= 4096;
  if (dR < -2048) dR += 4096;
  totalLeft  += dL;
  totalRight += dR;
  prevLeft = rawL;
  prevRight = rawR;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Включаем внутреннюю подтяжку (pull-up) для уверенного I2C сигнала
  pinMode(SDA1, INPUT_PULLUP);
  pinMode(SCL1, INPUT_PULLUP);
  pinMode(SDA2, INPUT_PULLUP);
  pinMode(SCL2, INPUT_PULLUP);

  Wire.begin(SDA1, SCL1);
  Wire.setClock(400000);

  Wire1.begin(SDA2, SCL2);
  Wire1.setClock(400000);

  Serial.println("\n=== WRO ENCODER TEST (две I2C шины) ===");
  Serial.print("Шина 1 (левый):  SDA=");  Serial.print(SDA1);
  Serial.print(", SCL=");                Serial.println(SCL1);
  Serial.print("Шина 2 (правый): SDA="); Serial.print(SDA2);
  Serial.print(", SCL=");                Serial.println(SCL2);

  Serial.print("Левый  AS5600: ");
  Serial.println(checkPresent(Wire)  ? "НАЙДЕН" : "НЕ НАЙДЕН!");
  Serial.print("Правый AS5600: ");
  Serial.println(checkPresent(Wire1) ? "НАЙДЕН" : "НЕ НАЙДЕН!");

  Serial.print("Магнит левый:  "); Serial.print(readMagnet(Wire));
  Serial.print("  AGC=");          Serial.println(readAGC(Wire));
  Serial.print("Магнит правый: "); Serial.print(readMagnet(Wire1));
  Serial.print("  AGC=");          Serial.println(readAGC(Wire1));

  Serial.println("\nКрути колёса руками. 1 оборот = 4096 тиков.\n");
  Serial.println("ЛЕВЫЙ                       ПРАВЫЙ");
  Serial.println("raw  градус  тики  см       raw  градус  тики  см");
}

void loop() {
  int rawL = readAngle(Wire);
  int rawR = readAngle(Wire1);

  if (rawL >= 0 && rawR >= 0) updateOdometry(rawL, rawR);

  if (rawL >= 0) {
    Serial.print(rawL); Serial.print("\t");
    Serial.print(rawL * 360.0f / 4096.0f, 1); Serial.print("°\t");
    Serial.print(totalLeft); Serial.print("\t");
    Serial.print(totalLeft / ticksPerCM, 1); Serial.print("cm");
  } else {
    Serial.print("ОШИБКА\t-\t-\t-");
  }

  Serial.print("    |    ");

  if (rawR >= 0) {
    Serial.print(rawR); Serial.print("\t");
    Serial.print(rawR * 360.0f / 4096.0f, 1); Serial.print("°\t");
    Serial.print(totalRight); Serial.print("\t");
    Serial.print(totalRight / ticksPerCM, 1); Serial.print("cm");
  } else {
    Serial.print("ОШИБКА\t-\t-\t-");
  }

  Serial.println();
  delay(300);
}
