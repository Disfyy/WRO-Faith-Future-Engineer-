#pragma once
/*
 * AS5600 dual-bus I2C driver for ESP32-S3
 *
 * The AS5600 has a hard-coded I2C address (0x36) and offers no chip-select
 * pin, so two of them on the same bus collide. The ESP32-S3 exposes two
 * native I2C peripherals (Wire = I2C0, Wire1 = I2C1) — we put one AS5600
 * on each bus, which sidesteps the address conflict without a TCA9548A mux.
 *
 * Wiring:
 *   Left  encoder → I2C0 (Wire):  SDA=GPIO8,  SCL=GPIO9
 *   Right encoder → I2C1 (Wire1): SDA=GPIO11, SCL=GPIO12
 *
 * Protocol:
 *   ANGLE register pair at 0x0E/0x0F (high byte first, 12-bit value).
 *   Returns 0..4095. -1 indicates an I2C error (no ACK or short read).
 *
 * Team Faith | WRO Future Engineers 2026 | v13.0
 */

#include <Arduino.h>
#include <Wire.h>
#include "wro_hw_config_v13.h"

#define AS5600_REG_ANGLE_H  0x0E

// ─── Bring up both I2C buses with the AS5600s ────────────────
//   Returns true if both encoders responded to a status read.
static inline bool as5600_init() {
  Wire.begin(I2C0_SDA, I2C0_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  Wire1.begin(I2C1_SDA, I2C1_SCL);
  Wire1.setClock(I2C_FREQ_HZ);

  bool leftOk  = false;
  bool rightOk = false;

  Wire.beginTransmission(AS5600_ADDRESS);
  leftOk = (Wire.endTransmission() == 0);

  Wire1.beginTransmission(AS5600_ADDRESS);
  rightOk = (Wire1.endTransmission() == 0);

  return leftOk && rightOk;
}

// ─── Read raw 12-bit angle (0..4095). Returns -1 on I2C error ─
static inline int as5600_read(TwoWire &bus) {
  bus.beginTransmission(AS5600_ADDRESS);
  bus.write(AS5600_REG_ANGLE_H);
  if (bus.endTransmission(false) != 0) return -1;     // repeated start

  if (bus.requestFrom((int)AS5600_ADDRESS, 2) != 2) return -1;
  uint8_t hi = bus.read();
  uint8_t lo = bus.read();
  return ((int)(hi & 0x0F) << 8) | lo;
}

// ─── Drop-in replacements for old readEncoder() calls ─────────
static inline int readEncoderLeft()  { return as5600_read(Wire);  }
static inline int readEncoderRight() { return as5600_read(Wire1); }

// ─── Read angle in degrees (0.0..360.0). -1 on error ─────────
static inline float as5600_read_deg(TwoWire &bus) {
  int raw = as5600_read(bus);
  if (raw < 0) return -1.0f;
  return raw * 360.0f / (float)AS5600_RESOLUTION;
}
