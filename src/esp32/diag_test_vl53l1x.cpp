#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_VL53L1X

/*
 * WRO v13 — Test 2× VL53L1X with XSHUT address-remapping (ESP32-S3)
 *
 * Wiring:
 *   Front sensor → I2C0 (Wire,  GPIO8/9), XSHUT=GPIO15, runtime addr 0x30
 *   Side  sensor → I2C1 (Wire1, GPIO11/12), XSHUT=GPIO16, runtime addr 0x31
 *
 * The boot dance (held in vl53l1x_dual.h):
 *   1. Drive every XSHUT low (sensors in reset).
 *   2. Bring Front out of reset → claims 0x29 → reassign to 0x30.
 *   3. Bring Side out of reset → claims 0x29 on Wire1 → reassign to 0x31.
 *
 * Library: Pololu VL53L1X (Library Manager → "VL53L1X" by Pololu).
 *
 * Serial: 115200 baud
 */

#include <Wire.h>
#include "wro_hw_config_v13.h"
#include "vl53l1x_dual.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v13 VL53L1X TEST ===");
  Serial.print("I2C0 (Front): SDA=GPIO"); Serial.print(I2C0_SDA);
  Serial.print(" SCL=GPIO"); Serial.print(I2C0_SCL);
  Serial.print(" XSHUT=GPIO"); Serial.println(VL53L1X_FRONT_XSHUT);
  Serial.print("I2C1 (Side):  SDA=GPIO"); Serial.print(I2C1_SDA);
  Serial.print(" SCL=GPIO"); Serial.print(I2C1_SCL);
  Serial.print(" XSHUT=GPIO"); Serial.println(VL53L1X_SIDE_XSHUT);
  Serial.println();

  Wire.begin(I2C0_SDA, I2C0_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  Wire1.begin(I2C1_SDA, I2C1_SCL);
  Wire1.setClock(I2C_FREQ_HZ);

  vl53_initAll();

  Serial.println();
  Serial.println("  FRONT (mm)   SIDE (mm)   status");
  Serial.println("  ──────────────────────────────────────");
}

void loop() {
  vl53_readAll();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 200) return;
  lastPrint = millis();

  Serial.print("  ");
  if (tfFront.ok) { Serial.print(tfFront.distMM); Serial.print("mm"); }
  else            { Serial.print("NF      "); }
  Serial.print("\t");
  if (tfSide.ok)  { Serial.print(tfSide.distMM);  Serial.print("mm"); }
  else            { Serial.print("NF      "); }
  Serial.print("\t");

  if      (tfFront.ok && tfFront.distMM < TOF_EMERGENCY_MM) Serial.print("EMERGENCY!");
  else if (tfFront.ok && tfFront.distMM < TOF_SLOW_DOWN_MM) Serial.print("CLOSE");
  else                                                       Serial.print("OK");
  Serial.println();
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_VL53L1X
