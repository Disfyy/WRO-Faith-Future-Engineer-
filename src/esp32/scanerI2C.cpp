#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_SCAN_I2C

/*
 * WRO v12 — I2C Bus Scanner for ESP32-S3
 *
 * In v12, the I2C bus contains ONLY the IMU:
 *   0x68 — ICM-20948 (AD0 → GND)
 *
 * Any other device found = wiring issue:
 *   0x36 → AS5600 encoder (replaced by AS5048A SPI in v12)
 *   0x29 → VL53L1X ToF (replaced by TFMini-S UART in v12)
 *   0x70 → TCA9548A mux (removed in v12)
 */

#include <Wire.h>
#include "wro_hw_config_v12.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v12 I2C SCANNER ===");
  Serial.print("SDA=GPIO"); Serial.print(I2C_SDA);
  Serial.print("  SCL=GPIO"); Serial.println(I2C_SCL);
  Serial.println("Expected: ONLY 0x68 (ICM-20948 IMU)");
  Serial.println();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found++;
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX); Serial.print(" — ");
      switch (addr) {
        case 0x68: Serial.println("ICM-20948 IMU ✓ (expected)");  break;
        case 0x69: Serial.println("ICM-20948 IMU (AD0=HIGH)");    break;
        case 0x29: Serial.println("VL53L1X ✗ (removed in v12!)"); break;
        case 0x36: Serial.println("AS5600   ✗ (removed in v12!)"); break;
        case 0x70: Serial.println("TCA9548A ✗ (removed in v12!)"); break;
        default:   Serial.println("Unknown device"); break;
      }
    }
  }

  Serial.println();
  Serial.print("Total: "); Serial.println(found);
  if      (found == 1) Serial.println("PASS ✓ — only IMU on bus. Clean!");
  else if (found == 0) Serial.println("FAIL ✗ — no devices! Check SDA/SCL.");
  else                 Serial.println("WARNING — extra devices detected.");
}

void loop() { delay(10000); setup(); }

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_SCAN_I2C
