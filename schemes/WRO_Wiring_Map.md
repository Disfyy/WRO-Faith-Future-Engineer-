# WRO Wiring Map (v12)

> **Active hardware revision:** v12 (ESP32-S3 + AS5048A SPI + TFMini-S UART, no I2C mux).
> The full pin reference is at [`docs/WRO_Wiring_Map_v12.md`](../docs/WRO_Wiring_Map_v12.md);
> this file is the short summary for at-a-glance lookup.

## ESP32-S3-DevKitC-1 (N8R8) Pin Map

### I2C (one bus, IMU only)
- GPIO 8  → I2C SDA
- GPIO 9  → I2C SCL

### SPI HSPI — 2× AS5048A encoders (14-bit, 16384 ticks/rev)
- GPIO 11 → MOSI (tied to 3.3V; AS5048A doesn't use MOSI for reads)
- GPIO 12 → SCK
- GPIO 13 → MISO
- GPIO 10 → CS Left
- GPIO 14 → CS Right

### UART1 — TFMini-S front (12 m range, 100 Hz)
- GPIO 15 → RX (from TFMini TX)
- GPIO 16 → TX (to TFMini RX)

### UART2 — OpenMV H7 Plus camera
- GPIO 17 → RX (from OpenMV TX)
- GPIO 18 → TX (to OpenMV RX)

### TFMini-S side (optional, not yet wired)
- GPIO 47 → SW Serial RX

### BTS7960 motor driver
- GPIO 38 → R_EN
- GPIO 39 → L_EN
- GPIO 40 → R_PWM (forward, LEDC Ch0)
- GPIO 41 → L_PWM (reverse, LEDC Ch1)

### Steering servo
- GPIO 42 → JX PDI-6221MG PWM

### Control I/O
- GPIO 21 → E-Stop button (INPUT_PULLUP, active LOW)
- GPIO 2  → Status LED
- GPIO 48 → RGB LED (WS2812 onboard)

## I2C Devices
- ICM-20948 IMU @ 0x68 (AD0 → GND) — **only device on the bus** (no TCA9548A)

## Power Rails
- **5V rail:** ESP32-S3 VIN, **TFMini-S VIN (5V required, NOT 3.3V)**, OpenMV
- **3.3V rail:** ICM-20948, AS5048A × 2 (from ESP32-S3 onboard regulator)
- **Motor power:** LiPo 2S/3S → BTS7960 directly
- **Common GND** across all components is mandatory

## Notes
- Keep I2C wiring short and away from motor power lines.
- TFMini-S **requires 4.5–6 V** — do not power from 3.3 V; it will not respond.
- Servo: re-measure lock-to-lock µs on the actual chassis (target 7 firmware) and add a 60 µs margin from each end-stop to avoid stall-brownout.

## What changed from v11
| Component | v11 | v12 |
|-----------|-----|-----|
| MCU | ESP32 DevKitC V4 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders | 2× AS5600 (I2C, 12-bit) | 2× AS5048A (SPI, 14-bit) |
| Distance front | VL53L1X (I2C, 4 m) | TFMini-S (UART, 12 m) |
| I2C mux | TCA9548A (0x70) | None — not needed |
| I2C devices | 5 | 1 (IMU only) |
| Odometry | 277 ticks/cm | 1110 ticks/cm |
| Max distance | 4 m | 12 m |

For the migration step-by-step, see [`docs/WRO_Migration_v11_to_v12.md`](../docs/WRO_Migration_v11_to_v12.md).
