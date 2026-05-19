#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_SCAN_I2C

/*
 * WRO v13 — Dual-bus I2C Scanner for ESP32-S3
 *
 * v13 has TWO I2C buses:
 *   I2C0 (Wire,  GPIO 8/9):   ICM-20948 (0x68) + AS5600 Left (0x36) +
 *                              VL53L1X Front (0x30 after boot, 0x29 before)
 *   I2C1 (Wire1, GPIO 11/12): AS5600 Right (0x36) +
 *                              VL53L1X Side  (0x31 after boot, 0x29 before)
 *
 * Run this BEFORE any other v13 test — confirms the wiring before the
 * VL53L1X address remapping fires (so VL53L1Xs will appear at 0x29 here).
 * The TCA9548A I2C mux from v11 is gone — if you see 0x70 you're scanning
 * the wrong board.
 */

#include <Wire.h>
#include "wro_hw_config_v13.h"

static int scanBus(TwoWire &bus, const char *name) {
  Serial.println();
  Serial.print("=== "); Serial.print(name); Serial.println(" ===");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      found++;
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX); Serial.print(" — ");
      switch (addr) {
        case 0x68: Serial.println("ICM-20948 IMU (AD0 → GND)");        break;
        case 0x69: Serial.println("ICM-20948 IMU (AD0 → VCC)");         break;
        case 0x29: Serial.println("VL53L1X (default — pre-remap)");     break;
        case 0x30: Serial.println("VL53L1X Front (post-remap)");        break;
        case 0x31: Serial.println("VL53L1X Side  (post-remap)");        break;
        case 0x32: Serial.println("VL53L1X Third (optional, post-remap)"); break;
        case 0x36: Serial.println("AS5600 magnetic encoder");           break;
        case 0x70: Serial.println("TCA9548A ✗ (mux removed in v13!)");  break;
        default:   Serial.println("Unknown device");                    break;
      }
    }
  }
  Serial.print("Total on this bus: "); Serial.println(found);
  return found;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v13 DUAL-BUS I2C SCANNER ===");
  Serial.print("I2C0 SDA=GPIO"); Serial.print(I2C0_SDA);
  Serial.print(" SCL=GPIO"); Serial.println(I2C0_SCL);
  Serial.print("I2C1 SDA=GPIO"); Serial.print(I2C1_SDA);
  Serial.print(" SCL=GPIO"); Serial.println(I2C1_SCL);

  Wire.begin (I2C0_SDA, I2C0_SCL);  Wire.setClock(I2C_FREQ_HZ);
  Wire1.begin(I2C1_SDA, I2C1_SCL);  Wire1.setClock(I2C_FREQ_HZ);

  int n0 = scanBus(Wire,  "I2C0 (Wire)");
  int n1 = scanBus(Wire1, "I2C1 (Wire1)");

  Serial.println();
  Serial.print("Grand total: "); Serial.println(n0 + n1);
  Serial.println();
  Serial.println("Expected on I2C0: 0x68 (IMU) + 0x36 (AS5600 L) + 0x29 (VL53L1X F)");
  Serial.println("Expected on I2C1: 0x36 (AS5600 R) + 0x29 (VL53L1X S)");
}

void loop() { delay(10000); setup(); }

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_SCAN_I2C
