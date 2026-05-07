#pragma once
/*
 * AS5048A SPI Driver for ESP32-S3
 * 14-bit absolute magnetic encoder — replaces AS5600 (I2C, 12-bit)
 *
 * Wiring:
 *   MISO → GPIO13  (AS5048A data out)
 *   SCK  → GPIO12  (clock)
 *   MOSI → 3.3V    (AS5048A doesn't use MOSI for reads)
 *   CS_L → GPIO10  (left encoder chip select)
 *   CS_R → GPIO14  (right encoder chip select)
 *
 * Protocol: 16-bit SPI, MSB first, Mode 1 (CPOL=0, CPHA=1)
 *   Command:  bit15=parity, bit14=R/W(1=read), bits13:0=register
 *   Response: bit15=parity, bit14=error flag, bits13:0=data
 *   NOTE: AS5048A returns the result of the PREVIOUS command,
 *         so every read requires two transfers.
 *
 * Team Faith | WRO Future Engineers 2026 | v12.0
 */

#include <SPI.h>
#include "wro_hw_config_v12.h"

static SPIClass *encoderSPI = nullptr;

// ─── Even parity check ──────────────────────────────────────
static inline bool evenParity(uint16_t val) {
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return val & 1;
}

// ─── Build read command word with parity ────────────────────
static uint16_t as5048a_cmd(uint16_t reg) {
  uint16_t cmd = (1 << 14) | (reg & 0x3FFF);  // bit14 = read
  if (evenParity(cmd)) cmd |= (1 << 15);
  return cmd;
}

// ─── Single SPI transfer ────────────────────────────────────
static uint16_t as5048a_transfer(uint8_t csPin, uint16_t cmd) {
  digitalWrite(csPin, LOW);
  uint16_t result = encoderSPI->transfer16(cmd);
  digitalWrite(csPin, HIGH);
  return result;
}

// ─── Initialize SPI bus and CS pins ────────────────────────
void as5048a_init() {
  pinMode(ENC_LEFT_CS,  OUTPUT);
  pinMode(ENC_RIGHT_CS, OUTPUT);
  digitalWrite(ENC_LEFT_CS,  HIGH);
  digitalWrite(ENC_RIGHT_CS, HIGH);

  encoderSPI = new SPIClass(HSPI);
  encoderSPI->begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
}

// ─── Read raw 14-bit angle (0–16383). Returns -1 on error ──
int as5048a_readAngle(uint8_t csPin) {
  uint16_t cmd = as5048a_cmd(AS5048A_ANGLE_REG);

  encoderSPI->beginTransaction(SPISettings(SPI_SPEED_HZ, MSBFIRST, SPI_MODE1));
  as5048a_transfer(csPin, cmd);              // first transfer: send cmd (response stale)
  uint16_t raw = as5048a_transfer(csPin, cmd); // second transfer: get actual angle
  encoderSPI->endTransaction();

  if (raw & (1 << 14)) return -1;  // error flag
  return raw & 0x3FFF;
}

// ─── Read angle in degrees (0.0–359.98°) ───────────────────
float as5048a_readAngleDeg(uint8_t csPin) {
  int raw = as5048a_readAngle(csPin);
  if (raw < 0) return -1.0f;
  return raw * 360.0f / (float)AS5048A_RESOLUTION;
}

// ─── Drop-in replacements for old readEncoder() calls ──────
int readEncoderLeft()  { return as5048a_readAngle(ENC_LEFT_CS);  }
int readEncoderRight() { return as5048a_readAngle(ENC_RIGHT_CS); }
