# WRO Migration Guide: v12 → v13

**Team Faith | WRO Future Engineers 2026 | May 2026**

---

## Why this migration exists

v12 changed both the MCU (ESP32 → ESP32-S3) **and** the sensor stack (AS5600 → AS5048A SPI; VL53L1X → TFMini-S UART). The AS5048A and TFMini-S parts hadn't arrived; meanwhile the v11 TCA9548A I2C mux on the existing board burned out.

**v13 keeps the new ESP32-S3 main controller from v12 but reverts to the v11 sensor stack** (AS5600 + VL53L1X). The mux is gone permanently. The address conflicts are resolved differently:

- **AS5600** has a hard-coded address (0x36). With two of them, the TCA mux used to switch between them. v13 puts one AS5600 on each of the ESP32-S3's two native I2C peripherals (Wire and Wire1).
- **VL53L1X** has a default address (0x29) but it's RAM-only; software can move it. v13 holds every VL53L1X in reset via XSHUT pins, brings them up one at a time, and reassigns each to a unique runtime address.

Result: same v11 sensors, no mux, two clean I2C buses.

---

## What's changing (v12 vs v13)

| | v12 (planned) | v13 (current) |
|-|---------------|---------------|
| MCU | ESP32-S3-DevKitC-1 N8R8 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders | AS5048A (SPI, 14-bit) | AS5600 (I2C, 12-bit) |
| Distance | TFMini-S (UART, 12 m) | VL53L1X (I2C, ~3 m) |
| I2C buses | 1 (IMU only) | 2 (Wire + Wire1) |
| Odometry | 1110 ticks/cm | 277 ticks/cm |

---

## Step 1 — Hardware

### Encoders (AS5600 ×2)
- [ ] Wire **Left** AS5600 to **I2C0**: SDA=GPIO8, SCL=GPIO9
- [ ] Wire **Right** AS5600 to **I2C1**: SDA=GPIO11, SCL=GPIO12
- [ ] Power: 3.3 V + GND (3.0–5.5 V supported; use 3.3 V from the S3)
- [ ] Magnet gap: 0.5–3 mm from the chip face, axially centered

### VL53L1X ×2
- [ ] Wire **Front** sensor to **I2C0**, XSHUT → GPIO15
- [ ] Wire **Side** sensor to **I2C1**, XSHUT → GPIO16
- [ ] (Optional 3rd unit) wire to **I2C0**, XSHUT → GPIO47
- [ ] Power: 5 V on VIN (Pololu/Adafruit breakout has onboard 2.8 V LDO)

### IMU
- [ ] No change — ICM-20948 stays on **I2C0** (0x68, AD0 → GND)

### TCA9548A mux
- [ ] **Remove** if still on the board (it's gone permanently)

---

## Step 2 — Arduino IDE

- Board: **ESP32S3 Dev Module** (unchanged from v12)
- USB CDC on Boot: **Enabled**
- Flash Size: **8MB**
- PSRAM: **OPI PSRAM**
- Upload Speed: 921600

### Required libraries
- `Adafruit ICM20948` (existing)
- `ESP32Servo` (existing)
- **`VL53L1X` by Pololu** (Library Manager → search "VL53L1X" → install Pololu version)
- AS5600 driver is implemented inline in `as5600_dual_i2c.h` — no library install needed.

---

## Step 3 — Code changes

The v13 firmware is already a clean rewrite — there's no `eps323.cpp` patching to do. The active build target is **`WRO_TARGET_V13_MAIN`** (= 11) in `wro_build_target.h`, and it builds `wro_v13_main.cpp` plus all the `wro_*` modules. The bottom-of-stack drivers swap as follows:

| v12 | v13 |
|-----|-----|
| `as5048a_spi.h` | `as5600_dual_i2c.h` |
| `tfmini_s.h` | `vl53l1x_dual.h` |
| `wro_hw_config_v12.h` | `wro_hw_config_v13.h` |
| `wro_config_v12.h` | `wro_config_v13.h` |
| `wro_v12_main.cpp` | `wro_v13_main.cpp` |

The estimation/behavior/FSM modules (`wro_imu`, `wro_odometry`, `wro_camera`, `wro_corner`, `wro_behavior_*`, `wro_park`, `wro_race_fsm`, `wro_estop`, `wro_telemetry`, `wro_sensors`) had only their `#include`s and target-macro guards updated — the algorithms are unchanged.

---

## Step 4 — Verification order

1. `WRO_ACTIVE_TARGET = 2` → `scan_i2c_v13.cpp` → expect:
   - I2C0: 0x68 (IMU), 0x36 (AS5600 L), 0x29 (VL53L1X F before remap)
   - I2C1: 0x36 (AS5600 R), 0x29 (VL53L1X S before remap)
2. `WRO_ACTIVE_TARGET = 8` → `test_encoders.cpp` → spin wheels, both encoders accumulate ticks (4096/rev, 277 ticks/cm)
3. `WRO_ACTIVE_TARGET = 9` → `test_vl53l1x.cpp` → both sensors come up at remapped addresses, return real distances
4. `WRO_ACTIVE_TARGET = 10` → `bench_test_v13.cpp` → all sensors + actuators + camera + E-Stop
5. `WRO_ACTIVE_TARGET = 11` → `wro_v13_main.cpp` → wheels-up smoke test, then floor

---

## Rollback plan

If a v13 issue appears at competition and v11 hardware is on the spare board:
1. Swap ESP32-S3 → ESP32 DevKitC V4 (if you still have one) and re-flash a v11 git tag.
2. Reconnect TCA9548A (would need to source a new mux — the original is dead).
3. Time estimate: ≥ 60 min.

A safer fallback inside v13: comment out the side VL53L1X (`HAS_SIDE_TOF 0` in `wro_config_v13.h`) and run on Open Challenge with front sensor only.
