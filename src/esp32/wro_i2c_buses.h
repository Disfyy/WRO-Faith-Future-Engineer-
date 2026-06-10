#pragma once
/*
 * I2C bus bring-up — single source of truth for both native peripherals.
 *
 * The ESP32-S3 has two hardware I2C controllers: Wire (I2C0, GPIO 8/9) and
 * Wire1 (I2C1, GPIO 3/4). Three driver families share them:
 *   I2C0 (Wire):  ICM-20948 IMU + AS5600 Left + VL53L1X Front
 *   I2C1 (Wire1): AS5600 Right + VL53L1X Side
 *
 * Previously the begin()/setClock()/setTimeOut() calls were a side-effect
 * buried inside as5600_init(), so the IMU and BOTH VL53L1X sensors silently
 * depended on the encoder init running first (and in the right order). This
 * header makes bus bring-up explicit and order-independent: call
 * i2c_buses_init() once, early in setup(), before any device driver touches
 * a bus.
 *
 * Safe to call from multiple init paths (e.g. main setup() AND as5600_init()):
 * Wire.begin() is itself safe to repeat, and the per-translation-unit once-guard
 * below skips redundant re-init within the same source file. Worst case in the
 * active build is one extra harmless begin() across two TUs at boot.
 *
 * Team Faith | WRO Future Engineers 2026 | v13.0
 */

#include <Arduino.h>
#include <Wire.h>
#include "wro_hw_config_v13.h"

static inline void i2c_buses_init() {
  static bool done = false;
  if (done) return;

  Wire.begin(I2C0_SDA, I2C0_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  Wire.setTimeOut(10);            // ms — fail fast on a wedged bus so the loop
                                  // degrades via the ENC/IMU fail-limits instead
                                  // of stalling into a watchdog panic-reset.
  Wire1.begin(I2C1_SDA, I2C1_SCL);
  Wire1.setClock(I2C_FREQ_HZ);
  Wire1.setTimeOut(10);

  done = true;
}
