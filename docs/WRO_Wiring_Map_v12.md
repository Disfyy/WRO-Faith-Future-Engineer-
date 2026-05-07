# WRO Wiring Map — v12

**Hardware revision:** v12.0 (ESP32-S3 + AS5048A SPI + TFMini-S UART)  
**Previous revision:** v11 (ESP32 DevKitC V4 + AS5600 I2C + VL53L1X I2C)  
**Date:** May 2026

---

## Bus Architecture

```
ESP32-S3-DevKitC-1 (N8R8)
│
├── I2C (GPIO 8/9) ────────── ICM-20948 IMU (0x68) ← ONLY device on I2C
│
├── SPI HSPI (GPIO 11-14) ─── AS5048A Left  (CS=GPIO10)
│                          └── AS5048A Right (CS=GPIO14)
│                              14-bit, 16384 ticks/rev, 1 MHz SPI
│
├── UART1 (GPIO 15/16) ─────── TFMini-S Front (12m range, 100 Hz)
│
├── UART2 (GPIO 17/18) ─────── OpenMV H7 Plus Camera
│
├── UART0 (USB-C native) ───── Debug serial (freed — no CP2102 bridge!)
│
├── LEDC (GPIO 40/41) ──────── BTS7960 Motor Driver
│
└── PWM (GPIO 42) ──────────── JX PDI-6221MG Steering Servo
```

---

## Complete Pin Table

### I2C — ICM-20948 IMU
| GPIO | Signal | Note |
|:----:|--------|------|
| 8 | SDA | Only IMU — no mux needed |
| 9 | SCL | |

### SPI HSPI — AS5048A Encoders
| GPIO | Signal | Connected to |
|:----:|--------|-------------|
| 11 | MOSI | 3.3V (AS5048A doesn't use MOSI for reads) |
| 12 | SCK | Both encoders CLK |
| 13 | MISO | Both encoders DATA out |
| 10 | CS Left | Left encoder CS |
| 14 | CS Right | Right encoder CS |

### UART1 — TFMini-S Front
| GPIO | Signal | Connected to |
|:----:|--------|-------------|
| 15 | RX | TFMini-S TX |
| 16 | TX | TFMini-S RX |

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
| 48 | RGB LED | WS2812 on DevKit |

### TFMini-S Side (optional)
| GPIO | Signal | |
|:----:|--------|--|
| 47 | SW Serial RX | TFMini-S side TX |

---

## Power
- **5V rail:** ESP32-S3 VIN, TFMini-S VIN (5V required!), OpenMV (check)
- **3.3V rail:** ICM-20948, AS5048A × 2 (from ESP32-S3 onboard reg)
- **Motor power:** LiPo 2S/3S → BTS7960 directly
- **Common GND** across all components is mandatory

> ⚠️ **TFMini-S requires 4.5–6V.** Do NOT power from 3.3V — it will not respond.

---

## What changed from v11

| Component | v11 | v12 |
|-----------|-----|-----|
| MCU | ESP32 DevKitC V4 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders | 2× AS5600 (I2C, 12-bit) | 2× AS5048A (SPI, 14-bit) |
| Distance front | VL53L1X (I2C, 4m) | TFMini-S (UART, 12m) |
| Distance side | VL53L1X (I2C, 4m) | TFMini-S (pending) |
| I2C mux | TCA9548A (0x70) | None — not needed |
| I2C devices | 5 | 1 (IMU only) |
| I2C buses | 2 (Wire + Wire1) | 1 (Wire only) |
| USB bridge | CP2102 (wastes UART0) | Native USB-C |
| GPIO | 34 (6 input-only) | 45 (all bidirectional) |
| PSRAM | 0 | 8 MB |
| Odometry | 277 ticks/cm | 1110 ticks/cm (4×) |
| Max distance | 4 m | 12 m (3×) |
