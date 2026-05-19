#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS

/*
 * WRO v13 — Test 2× AS5600 on dual I2C (ESP32-S3)
 *
 * Wiring:
 *   Left  encoder → I2C0 (Wire):  SDA=GPIO8,  SCL=GPIO9
 *   Right encoder → I2C1 (Wire1): SDA=GPIO11, SCL=GPIO12
 *
 * Both encoders use the same fixed I2C address (0x36); the dual-bus split
 * is what avoids a collision (no TCA9548A needed).
 *
 * Serial: 115200 baud
 * Spin wheels by hand — values should change smoothly.
 * 1 revolution = 4096 ticks. 1 cm ≈ 277 ticks (47 mm wheel).
 */

#include "wro_hw_config_v13.h"
#include "as5600_dual_i2c.h"

int  prevLeft  = -1, prevRight  = -1;
long totalLeft = 0,  totalRight = 0;

void updateOdometry(int rawL, int rawR) {
  if (prevLeft < 0) { prevLeft = rawL; prevRight = rawR; return; }
  int dL = rawL - prevLeft;
  int dR = rawR - prevRight;
  if (dL >  AS5600_HALF_RES) dL -= AS5600_RESOLUTION;
  if (dL < -AS5600_HALF_RES) dL += AS5600_RESOLUTION;
  if (dR >  AS5600_HALF_RES) dR -= AS5600_RESOLUTION;
  if (dR < -AS5600_HALF_RES) dR += AS5600_RESOLUTION;
  totalLeft  += dL;
  totalRight += dR;
  prevLeft  = rawL;
  prevRight = rawR;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v13 AS5600 DUAL-I2C ENCODER TEST ===");
  Serial.print("I2C0 (Left):  SDA=GPIO"); Serial.print(I2C0_SDA);
  Serial.print(" SCL=GPIO"); Serial.println(I2C0_SCL);
  Serial.print("I2C1 (Right): SDA=GPIO"); Serial.print(I2C1_SDA);
  Serial.print(" SCL=GPIO"); Serial.println(I2C1_SCL);

  bool ok = as5600_init();
  Serial.print("init both encoders: "); Serial.println(ok ? "OK" : "FAIL");

  int tL = readEncoderLeft();
  int tR = readEncoderRight();
  Serial.print("Left  AS5600: ");  Serial.println(tL >= 0 ? "FOUND" : "NOT FOUND — check wiring!");
  Serial.print("Right AS5600: ");  Serial.println(tR >= 0 ? "FOUND" : "NOT FOUND — check wiring!");
  Serial.print("Ticks per cm: ");  Serial.println(TICKS_PER_CM, 1);
  Serial.println();
  Serial.println("  LEFT (raw / deg / cm)           RIGHT (raw / deg / cm)");
  Serial.println("  ─────────────────────────────   ─────────────────────────────");
}

void loop() {
  int rawL = readEncoderLeft();
  int rawR = readEncoderRight();
  if (rawL >= 0 && rawR >= 0) updateOdometry(rawL, rawR);

  Serial.print("  ");
  if (rawL >= 0) {
    Serial.print(rawL); Serial.print(" / ");
    Serial.print(rawL * 360.0f / AS5600_RESOLUTION, 1); Serial.print("° / ");
    Serial.print(totalLeft / TICKS_PER_CM, 1); Serial.print("cm");
  } else { Serial.print("ERROR"); }

  Serial.print("          ");

  if (rawR >= 0) {
    Serial.print(rawR); Serial.print(" / ");
    Serial.print(rawR * 360.0f / AS5600_RESOLUTION, 1); Serial.print("° / ");
    Serial.print(totalRight / TICKS_PER_CM, 1); Serial.print("cm");
  } else { Serial.print("ERROR"); }

  Serial.println();
  delay(200);
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS
