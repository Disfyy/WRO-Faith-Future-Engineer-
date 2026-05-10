# WRO Wiring Map — v13

**Hardware revision:** v13.0 (ESP32-S3 + 2× AS5600 dual-I2C + 2× VL53L1X XSHUT-remap)
**Previous revisions:** v11 (ESP32 DevKitC V4 + AS5600 + VL53L1X via TCA9548A) → v12 (ESP32-S3 + AS5048A SPI + TFMini-S UART)
**Date:** May 2026

---

## Why v13 exists

v12 was the planned migration to AS5048A SPI encoders + TFMini-S UART distance sensors, sidestepping the I2C address conflicts that the v11 stack solved with a TCA9548A multiplexer. **The hardware never made it onto the robot:** the AS5048A and TFMini-S parts hadn't arrived, and meanwhile the TCA9548A on the v11 board burned out.

Rather than wait for the v12 parts, the team is shipping v13 — same v11 sensors (AS5600 + VL53L1X) but on the new ESP32-S3 main board, using **two native I2C peripherals** to split the AS5600 pair (one per bus) and **XSHUT-based address remapping** for the VL53L1X pair. No mux required.

---

## Bus Architecture

```
ESP32-S3-DevKitC-1 (N8R8)
│
├── I2C0 (Wire,  GPIO 8/9) ──── ICM-20948 IMU       (0x68)
│                            ├── AS5600 Left         (0x36)
│                            └── VL53L1X Front       (0x29 → 0x30)  XSHUT=GPIO15
│
├── I2C1 (Wire1, GPIO 11/12) ── AS5600 Right        (0x36)
│                            └── VL53L1X Side        (0x29 → 0x31)  XSHUT=GPIO16
│
├── UART2 (GPIO 17/18) ──────── OpenMV H7 Plus Camera
├── UART0 (USB-C native) ─────── Debug serial
│
├── LEDC (GPIO 40/41) ─────────  BTS7960 Motor Driver
└── PWM  (GPIO 42) ─────────────  JX PDI-6221MG Steering Servo
```

> **VL53L1X address remapping** runs once at boot, in `vl53l1x_dual.h`:
>   1. All XSHUTs driven LOW (every VL53L1X in reset).
>   2. Front XSHUT released → boots at 0x29 → reassigned to 0x30.
>   3. Side  XSHUT released → boots at 0x29 (different bus, no clash) → reassigned to 0x31.
> The new addresses live only in RAM, so the dance repeats every reset.

---

## Complete Pin Table

### I2C0 — Wire (3 devices)
| GPIO | Signal | Devices |
|:----:|--------|---------|
| 8 | SDA | ICM-20948 (0x68) + AS5600 Left (0x36) + VL53L1X Front (0x30) |
| 9 | SCL | same |

### I2C1 — Wire1 (2 devices)
| GPIO | Signal | Devices |
|:----:|--------|---------|
| 11 | SDA | AS5600 Right (0x36) + VL53L1X Side (0x31) |
| 12 | SCL | same |

### VL53L1X XSHUT pins
| GPIO | Sensor | Notes |
|:----:|--------|-------|
| 15 | VL53L1X Front XSHUT | I2C0 |
| 16 | VL53L1X Side XSHUT | I2C1 |
| 47 | (reserved) VL53L1X Third XSHUT | optional 3rd unit on I2C0 |

### UART2 — OpenMV Camera
| GPIO | Signal | Connected to |
|:----:|--------|-------------|
| 17 | RX | OpenMV TX |
| 18 | TX | OpenMV RX |

### BTS7960 Motor Driver
| GPIO | Signal | |
|:----:|--------|--|
| 38 | R_EN | Right enable |
| 39 | L_EN | Left enable |
| 40 | R_PWM | Forward (LEDC Ch0) |
| 41 | L_PWM | Reverse (LEDC Ch1) |

### Steering Servo
| GPIO | Signal | |
|:----:|--------|--|
| 42 | PWM | JX PDI-6221MG |

### Control
| GPIO | Signal | |
|:----:|--------|--|
| 21 | E-Stop | INPUT_PULLUP button |
| 2 | Status LED | |
| 48 | RGB LED | WS2812 onboard (DevKit silkscreen) |

---

## Power
- **5V rail:** ESP32-S3 VIN, OpenMV, JX servo (high current!)
- **3.3V rail:** ICM-20948, 2× AS5600, 2× VL53L1X (from ESP32-S3 onboard regulator)
- **Motor power:** LiPo 2S/3S → BTS7960 directly
- **Common GND** across all components is mandatory

> **VL53L1X powers from 2.6–5.5 V** via VIN; the breakout boards typically expose a 3.3 V LDO and accept 5 V on VIN. AS5600 needs 3.0–5.5 V. The ESP32-S3 onboard 3.3 V regulator is fine for both AS5600s and both VL53L1Xs together.

---

## Pull-ups
The ESP32-S3 has weak internal pull-ups. For reliable I2C at 400 kHz with 5 devices on a bus, add **external 4.7 kΩ pull-ups** from SDA→3.3V and SCL→3.3V, on each bus. The AS5600 breakout boards usually include them; the VL53L1X breakout typically does too. Verify with a meter — too many pull-ups in parallel will pull the bus too hard (target ~2.5 kΩ effective).

---

## What changed across revisions

| Component | v11 | v12 (planned, never wired) | v13 (current) |
|-----------|-----|----------------------------|---------------|
| MCU | ESP32 DevKitC V4 | ESP32-S3-DevKitC-1 N8R8 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders | 2× AS5600 (I2C, 12-bit) | 2× AS5048A (SPI, 14-bit) | 2× AS5600 (I2C, 12-bit) |
| Distance front | VL53L1X (I2C, 4 m) | TFMini-S (UART, 12 m) | VL53L1X (I2C, ~3 m) |
| Distance side | VL53L1X (I2C) | TFMini-S (pending) | VL53L1X (I2C) |
| I2C mux | TCA9548A (0x70) | None | None — burned out, replaced by dual-bus + XSHUT |
| I2C buses | 2 (Wire + Wire1 via mux scheme) | 1 (Wire only) | 2 (Wire + Wire1 native peripherals) |
| Odometry | 277 ticks/cm | 1110 ticks/cm | 277 ticks/cm |
| Max distance | 4 m | 12 m | ~3 m (Medium mode, 50 ms budget) |
