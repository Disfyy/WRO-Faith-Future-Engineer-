// WRO v13 — Standalone test for 2x AS5600 on dual I2C (ESP32-S3)
//
// I2C0 (Wire):  SDA=GPIO 8,  SCL=GPIO 9   -> LEFT  encoder
// I2C1 (Wire1): SDA=GPIO 11, SCL=GPIO 12  -> RIGHT encoder
//
// Both encoders share the fixed AS5600 address 0x36; the bus split is
// what avoids a collision (no TCA9548A mux).
//
// Serial Monitor: 115200 baud
// Spin the wheels by hand — values should change smoothly.

#include <Wire.h>

#define SDA0 8
#define SCL0 9
#define SDA1 11
#define SCL1 12

#define AS5600_ADDR       0x36
#define REG_RAW_ANGLE_H2  0x0E
#define REG_STATUS        0x0B
#define REG_AGC           0x1A

// Wheel circumference (1 revolution = 14.6 cm = 146.0 mm)
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

  // Enable internal pull-ups so I2C is reliable even without external 4.7k.
  pinMode(SDA0, INPUT_PULLUP);
  pinMode(SCL0, INPUT_PULLUP);
  pinMode(SDA1, INPUT_PULLUP);
  pinMode(SCL1, INPUT_PULLUP);

  Wire.begin(SDA0, SCL0);
  Wire.setClock(400000);

  Wire1.begin(SDA1, SCL1);
  Wire1.setClock(400000);

  Serial.println("\n=== WRO v13 ENCODER TEST (dual I2C) ===");
  Serial.print("I2C0 (left):  SDA=");  Serial.print(SDA0);
  Serial.print(", SCL=");               Serial.println(SCL0);
  Serial.print("I2C1 (right): SDA=");  Serial.print(SDA1);
  Serial.print(", SCL=");               Serial.println(SCL1);

  Serial.print("Left  AS5600: ");
  Serial.println(checkPresent(Wire)  ? "FOUND" : "NOT FOUND!");
  Serial.print("Right AS5600: ");
  Serial.println(checkPresent(Wire1) ? "FOUND" : "NOT FOUND!");

  Serial.print("Left  magnet:  "); Serial.print(readMagnet(Wire));
  Serial.print("  AGC=");          Serial.println(readAGC(Wire));
  Serial.print("Right magnet:  "); Serial.print(readMagnet(Wire1));
  Serial.print("  AGC=");          Serial.println(readAGC(Wire1));

  Serial.println("\nSpin the wheels by hand. 1 revolution = 4096 ticks.\n");
  Serial.println("LEFT                        RIGHT");
  Serial.println("raw  deg   ticks  cm        raw  deg   ticks  cm");
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
    Serial.print("ERROR\t-\t-\t-");
  }

  Serial.print("    |    ");

  if (rawR >= 0) {
    Serial.print(rawR); Serial.print("\t");
    Serial.print(rawR * 360.0f / 4096.0f, 1); Serial.print("°\t");
    Serial.print(totalRight); Serial.print("\t");
    Serial.print(totalRight / ticksPerCM, 1); Serial.print("cm");
  } else {
    Serial.print("ERROR\t-\t-\t-");
  }

  Serial.println();
  delay(300);
}
