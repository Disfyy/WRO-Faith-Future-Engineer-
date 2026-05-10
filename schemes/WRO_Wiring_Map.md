# WRO Wiring Map (v13)

> **Active hardware revision:** v13 (ESP32-S3 + 2× AS5600 dual-I2C + 2× VL53L1X XSHUT-remap).
> The full pin reference is at [`docs/guides/WRO_Wiring_Map_v13.md`](../docs/guides/WRO_Wiring_Map_v13.md);
> this file is the short summary for at-a-glance lookup.

## ESP32-S3-DevKitC-1 (N8R8) Pin Map

### I2C0 — Wire (3 devices)
- GPIO 8  → SDA (ICM-20948 0x68 + AS5600 Left 0x36 + VL53L1X Front 0x30 post-remap)
- GPIO 9  → SCL (same)

### I2C1 — Wire1 (2 devices)
- GPIO 11 → SDA (AS5600 Right 0x36 + VL53L1X Side 0x31 post-remap)
- GPIO 12 → SCL (same)

### VL53L1X XSHUT pins
- GPIO 15 → VL53L1X Front XSHUT (I2C0)
- GPIO 16 → VL53L1X Side XSHUT (I2C1)
- GPIO 47 → reserved for an optional 3rd VL53L1X (I2C0)

### UART2 — OpenMV H7 Plus camera
- GPIO 17 → RX (from OpenMV TX)
- GPIO 18 → TX (to OpenMV RX)

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
- GPIO 48 → RGB LED (WS2812 onboard DevKit)

## Power Rails
- **5V rail:** ESP32-S3 VIN, OpenMV, JX servo, VL53L1X VIN (sensors include onboard 2.8 V LDO)
- **3.3V rail:** ICM-20948, AS5600 × 2 (from ESP32-S3 onboard regulator)
- **Motor power:** LiPo 2S/3S → BTS7960 directly
- **Common GND** across all components is mandatory

## Notes
- Add external 4.7 kΩ pull-ups on each I2C bus (most breakouts already include them; verify with a meter).
- VL53L1X address-remap dance (in `vl53l1x_dual.h`): hold every XSHUT low at boot, bring sensors up one at a time, write each to a unique runtime address.
- Servo: re-measure lock-to-lock µs on the actual chassis (target 7 firmware) and add a 60 µs margin from each end-stop to avoid stall-brownout.

## What changed across revisions
| Component | v11 | v12 (planned) | v13 (current) |
|-----------|-----|---------------|---------------|
| MCU | ESP32 DevKitC V4 | ESP32-S3-DevKitC-1 N8R8 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders | 2× AS5600 (I2C, mux) | 2× AS5048A (SPI) | 2× AS5600 (dual native I2C) |
| Distance | VL53L1X (mux) | TFMini-S (UART, 12 m) | VL53L1X (XSHUT remap) |
| I2C mux | TCA9548A (0x70) | None | None — replaced by topology |
| I2C buses | 1 (with mux) | 1 | 2 (Wire + Wire1) |
| Odometry | 277 ticks/cm | 1110 ticks/cm | 277 ticks/cm |

For the migration step-by-step, see [`docs/strategy/WRO_Migration_v12_to_v13.md`](../docs/strategy/WRO_Migration_v12_to_v13.md).
