#pragma once
/*
 * VL53L1X driver — XSHUT-based address remapping, dual-bus aware.
 *
 * VL53L1X powers up at default I2C address 0x29 (RAM-only, lost on reset),
 * so multiple sensors on the same bus need an address-remap dance:
 *
 *   1. Hold all VL53L1X XSHUT pins LOW (sensors in reset).
 *   2. Bring sensor #1 out of reset, wait for boot, change its address.
 *   3. Bring sensor #2 out of reset (still default 0x29 — no collision now).
 *   4. Continue for any remaining sensors.
 *
 * In v13 we use one VL53L1X per I2C bus by default (front on Wire, side on
 * Wire1). That arrangement actually doesn't *need* address remapping, but
 * we still allocate distinct addresses (0x30 front, 0x31 side, 0x32 third)
 * so that adding a third sensor on either bus just works.
 *
 * Compatibility: the public sensor structs (tfFront / tfSide) and member
 * names (distMM / ok / lastRead) match the v12 TFMini-S driver, so the
 * upper layers (wro_sensors, wro_corner) need no API change.
 *
 * Library: Pololu VL53L1X (https://github.com/pololu/vl53l1x-arduino).
 *   Install via Library Manager: "VL53L1X" by Pololu.
 *
 * Team Faith | WRO Future Engineers 2026 | v13.0
 */

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include "wro_hw_config_v13.h"

// ─── Sensor state struct (drop-in for the v12 TFMiniS struct) ─
struct VL53L1Sensor {
  VL53L1X        sensor;
  TwoWire       *bus;
  uint8_t        xshutPin;
  uint8_t        targetAddr;
  int            distMM;
  int            distCM;
  int            strength;     // unused for VL53L1X; kept for API parity
  bool           ok;
  unsigned long  lastRead;
};

static VL53L1Sensor tfFront;
static VL53L1Sensor tfSide;
static VL53L1Sensor tfThird;   // optional 3rd unit

// ─── Hold every XSHUT low so all VL53L1X devices stay in reset ─
static inline void vl53_hold_all_in_reset() {
  pinMode(VL53L1X_FRONT_XSHUT, OUTPUT);
  pinMode(VL53L1X_SIDE_XSHUT,  OUTPUT);
  pinMode(VL53L1X_THIRD_XSHUT, OUTPUT);
  digitalWrite(VL53L1X_FRONT_XSHUT, LOW);
  digitalWrite(VL53L1X_SIDE_XSHUT,  LOW);
  digitalWrite(VL53L1X_THIRD_XSHUT, LOW);
  delay(10);
}

// ─── Boot + configure one sensor ──────────────────────────────
//   Releases its XSHUT, lets it claim 0x29, then reassigns to targetAddr.
//   Distance mode = Medium (~3 m, 50 ms budget) — fast enough for corner detection.
static bool vl53_boot_one(VL53L1Sensor &s, TwoWire &bus,
                          uint8_t xshut, uint8_t targetAddr,
                          const char *name) {
  s.bus        = &bus;
  s.xshutPin   = xshut;
  s.targetAddr = targetAddr;
  s.distMM     = TOF_INVALID_MM;
  s.distCM     = -1;
  s.strength   = 0;
  s.ok         = false;
  s.lastRead   = 0;

  digitalWrite(xshut, HIGH);
  delay(10);                                    // boot time

  s.sensor.setBus(&bus);
  s.sensor.setTimeout(100);
  if (!s.sensor.init()) {
    Serial.print("VL53L1X "); Serial.print(name);
    Serial.println(": init() FAILED at 0x29");
    digitalWrite(xshut, LOW);                   // park back in reset
    return false;
  }
  s.sensor.setAddress(targetAddr);
  // Verify the remap actually took. setAddress() returns void (a NAK is lost),
  // so probe the new address directly: a silent failure leaves the device at
  // 0x29 and would collide with a same-bus sensor (e.g. the reserved third).
  s.bus->beginTransmission(targetAddr);
  if (s.bus->endTransmission() != 0) {
    Serial.print("VL53L1X "); Serial.print(name);
    Serial.println(": address remap FAILED (no ACK at target) - disabling");
    s.ok = false;
    digitalWrite(xshut, LOW);
    return false;
  }
  s.sensor.setDistanceMode(VL53L1X::Medium);
  s.sensor.setMeasurementTimingBudget(50000);   // µs
  s.sensor.startContinuous(50);                 // ms between reads → 20 Hz

  s.ok = true;
  Serial.print("VL53L1X "); Serial.print(name);
  Serial.print(": OK at 0x");
  if (targetAddr < 16) Serial.print("0");
  Serial.println(targetAddr, HEX);
  return true;
}

// ─── Initialize all wired VL53L1X sensors ─────────────────────
static inline void vl53_initAll() {
  vl53_hold_all_in_reset();

  // Front lives on Wire (I2C0). It must come up before the third sensor on
  // the same bus, so that we never have two devices at 0x29 simultaneously.
  if (!vl53_boot_one(tfFront, Wire,  VL53L1X_FRONT_XSHUT, VL53L1X_FRONT_ADDR, "FRONT")) {
    tfFront.ok = false;
  }

  // Side lives on Wire1 (I2C1) — alone on its bus, but still gets its own address.
  if (!vl53_boot_one(tfSide,  Wire1, VL53L1X_SIDE_XSHUT,  VL53L1X_SIDE_ADDR,  "SIDE")) {
    tfSide.ok = false;
  }

  // Dual-fail is silent-deadly: if BOTH front and side fail, the corner FSM
  // never sees a wall and the robot drives straight forever. Make that loud.
  if (!tfFront.ok && !tfSide.ok) {
    Serial.println("WARN: BOTH VL53L1X sensors failed boot");
    Serial.println("WARN: corner FSM will never trigger - check XSHUT wiring, I2C pull-ups, 2.8V rails");
  }

  // Third sensor reserved — left disabled by default. Set its XSHUT low here
  // so noise on a floating pin can't accidentally power it onto the bus.
  digitalWrite(VL53L1X_THIRD_XSHUT, LOW);
  tfThird.ok     = false;
  tfThird.distMM = TOF_INVALID_MM;
}

// ─── Non-blocking poll on one sensor ──────────────────────────
static inline void vl53_read_one(VL53L1Sensor &s) {
  if (!s.ok) return;
  if (!s.sensor.dataReady()) return;

  uint16_t mm = s.sensor.read(false);            // false = don't block
  if (s.sensor.timeoutOccurred()) {
    s.distMM = TOF_INVALID_MM;
    return;
  }
  if (mm == 0 || mm > TOF_MAX_VALID_MM) {
    s.distMM = TOF_INVALID_MM;
    return;
  }
  s.distMM   = mm;
  s.distCM   = (int)(mm / 10);
  s.lastRead = millis();
}

// ─── Poll all sensors — call every loop tick ──────────────────
static inline void vl53_readAll() {
  vl53_read_one(tfFront);
  vl53_read_one(tfSide);
  vl53_read_one(tfThird);
}
