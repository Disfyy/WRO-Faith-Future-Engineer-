#pragma once
/*
 * TFMini-S UART Driver for ESP32-S3
 * Single-point LiDAR, 0.1–12m, up to 1000 Hz
 * Replaces VL53L1X (I2C, 4m) — now fully off the I2C bus.
 *
 * Frame format (9 bytes @ 115200 baud):
 *   [0]  0x59  header byte 1
 *   [1]  0x59  header byte 2
 *   [2]  dist_L  distance low byte (cm)
 *   [3]  dist_H  distance high byte
 *   [4]  str_L   signal strength low
 *   [5]  str_H   signal strength high
 *   [6]  temp    temperature (raw)
 *   [7]  reserved
 *   [8]  checksum (low byte of sum of bytes 0–7)
 *
 * Compatibility: tfFront.distMM and tfSide.distMM are
 * drop-in replacements for old distFrontMM / distSideMM.
 *
 * Team Faith | WRO Future Engineers 2026 | v12.0
 */

#include <HardwareSerial.h>
#include "wro_hw_config_v12.h"

// ─── Sensor state struct ─────────────────────────────────────
struct TFMiniS {
  HardwareSerial *serial;
  int     distCM;
  int     distMM;      // mm — drop-in for old distFrontMM / distSideMM
  int     strength;
  bool    ok;
  uint8_t buf[9];
  uint8_t bufPos;
  unsigned long lastRead;
};

static TFMiniS tfFront;
static TFMiniS tfSide;

// ─── Initialize one sensor ──────────────────────────────────
void tfmini_init(TFMiniS &tf, HardwareSerial *ser, int rxPin, int txPin) {
  tf.serial   = ser;
  tf.distCM   = -1;
  tf.distMM   = 9999;
  tf.strength = 0;
  tf.ok       = false;
  tf.bufPos   = 0;
  tf.lastRead = 0;

  ser->begin(TFMINI_BAUD, SERIAL_8N1, rxPin, txPin);
  delay(100);

  unsigned long start = millis();
  while (millis() - start < 500) {
    if (ser->available()) { tf.ok = true; break; }
    delay(10);
  }
}

// ─── Non-blocking frame parser — call every loop ─────────────
void tfmini_read(TFMiniS &tf) {
  while (tf.serial->available()) {
    uint8_t b = tf.serial->read();

    if (tf.bufPos == 0) {
      if (b == 0x59) { tf.buf[0] = b; tf.bufPos = 1; }
      continue;
    }
    if (tf.bufPos == 1) {
      if (b == 0x59) { tf.buf[1] = b; tf.bufPos = 2; }
      else            { tf.bufPos = 0; }
      continue;
    }

    tf.buf[tf.bufPos++] = b;

    if (tf.bufPos == 9) {
      tf.bufPos = 0;

      // Checksum validation
      uint8_t sum = 0;
      for (int i = 0; i < 8; i++) sum += tf.buf[i];
      if ((sum & 0xFF) != tf.buf[8]) continue;

      int dist = tf.buf[2] | (tf.buf[3] << 8);
      int sig  = tf.buf[4] | (tf.buf[5] << 8);

      // Signal below 100 or saturated = unreliable
      if (sig < 100 || sig == 0xFFFF) {
        tf.distCM = -1;
        tf.distMM = 9999;
        continue;
      }

      tf.distCM   = dist;
      tf.distMM   = dist * 10;
      tf.strength = sig;
      tf.lastRead = millis();
    }
  }
}

// ─── Initialize both sensors ────────────────────────────────
static HardwareSerial TFSerial1(1);

void tfmini_initAll() {
  // Front sensor on UART1
  tfmini_init(tfFront, &TFSerial1, TFMINI_FRONT_RX, TFMINI_FRONT_TX);
  Serial.print("TFMini-S FRONT (UART1 RX=GPIO");
  Serial.print(TFMINI_FRONT_RX); Serial.print("): ");
  Serial.println(tfFront.ok ? "OK" : "NOT FOUND");

  // Side sensor disabled in v12.0 — wire to GPIO47 to enable
  tfSide.ok     = false;
  tfSide.distMM = 9999;
  Serial.println("TFMini-S SIDE:  DISABLED (wire GPIO47 and uncomment in tfmini_s.h)");
}

// ─── Poll all sensors — call every loop ─────────────────────
void tfmini_readAll() {
  if (tfFront.ok) tfmini_read(tfFront);
  if (tfSide.ok)  tfmini_read(tfSide);
}
