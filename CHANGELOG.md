# CHANGELOG

## v13.0 — Sensor revert + dual-I2C (May 2026)

### Why
The v12 plan called for AS5048A SPI encoders + TFMini-S UART distance sensors. The parts hadn't arrived in time, and meanwhile the TCA9548A I2C mux on the existing v11 board burned out. Rather than wait, v13 keeps the **new ESP32-S3** main controller from v12 but **reverts the sensor stack to the v11 hardware** (AS5600 + VL53L1X). The address-collision problem the mux used to solve is now handled by:
- **Two native I2C peripherals** on the ESP32-S3 (Wire / Wire1) — one AS5600 per bus.
- **XSHUT-based runtime address remapping** for the VL53L1X pair.

### Breaking changes (vs v12)
- **Encoders:** AS5048A (SPI, 14-bit) → 2× AS5600 (dual I2C, 12-bit)
- **Distance sensors:** TFMini-S (UART, 12 m) → 2× VL53L1X (I2C, ~3 m Medium mode)
- **Pin map:** SPI HSPI freed; I2C1 added on GPIO 11/12; VL53L1X XSHUTs on GPIO 15/16/47
- **Odometry constant:** 1110 ticks/cm → 277 ticks/cm
- **Build target macro:** `WRO_TARGET_V12_MAIN` → `WRO_TARGET_V13_MAIN` (still target 11)

### New files
| File | Purpose |
|------|---------|
| `src/esp32/wro_hw_config_v13.h` | Pin map + constants for ESP32-S3 with dual I2C + AS5600 + VL53L1X |
| `src/esp32/wro_config_v13.h` | Algorithm tunables (replaces v12 config) |
| `src/esp32/wro_v13_main.cpp` | Main firmware (replaces wro_v12_main.cpp) |
| `src/esp32/as5600_dual_i2c.h` | AS5600 driver, dual-bus aware |
| `src/esp32/vl53l1x_dual.h` | VL53L1X driver with XSHUT-based addr remap |
| `src/esp32/scan_i2c_v13.cpp` | Two-bus I2C scanner |
| `src/esp32/bench_test_v13.cpp` | Full bench test |
| `src/esp32/test_vl53l1x.cpp` | VL53L1X distance test |
| `docs/WRO_Wiring_Map_v13.md` | Complete v13 wiring reference |
| `docs/WRO_Migration_v12_to_v13.md` | Step-by-step migration guide |

### Removed files
| File | Replacement |
|------|-------------|
| `src/esp32/wro_hw_config_v12.h` | `wro_hw_config_v13.h` |
| `src/esp32/wro_config_v12.h` | `wro_config_v13.h` |
| `src/esp32/wro_v12_main.cpp` | `wro_v13_main.cpp` |
| `src/esp32/as5048a_spi.h` | `as5600_dual_i2c.h` |
| `src/esp32/tfmini_s.h` | `vl53l1x_dual.h` |
| `src/esp32/scan_i2c_v12.cpp` | `scan_i2c_v13.cpp` |
| `src/esp32/bench_test_v12.cpp` | `bench_test_v13.cpp` |
| `src/esp32/test_tfmini.cpp` | `test_vl53l1x.cpp` |
| `docs/WRO_Wiring_Map_v12.md` | `WRO_Wiring_Map_v13.md` |
| `docs/WRO_Migration_v11_to_v12.md` | `WRO_Migration_v12_to_v13.md` |

### Updated files
- `src/esp32/wro_build_target.h` — `WRO_TARGET_V13_MAIN` is the new active target; target 9 renamed to `TEST_VL53L1X`
- `src/esp32/wro_odometry.{cpp,h}` — AS5600 12-bit unwrap, dual-bus init via `as5600_init()`
- `src/esp32/wro_sensors.{cpp,h}` — bridges `vl53l1x_dual.h` (same `sens_tf_*` accessor names as v12)
- `src/esp32/wro_imu.cpp` — `Wire.begin(I2C0_SDA, I2C0_SCL)` now uses v13 macros; bus is shared with AS5600 L + VL53L1X F
- `src/esp32/wro_corner.{cpp,h}` — comments updated for VL53L1X; FSM logic unchanged
- `src/esp32/wro_behavior_*.cpp`, `wro_park.cpp`, `wro_race_fsm.cpp`, `wro_estop.cpp`, `wro_telemetry.cpp`, `wro_camera.cpp` — `#include "wro_config_v13.h"` and `WRO_TARGET_V13_MAIN` guard; logic unchanged
- `src/esp32/esp32.ino` — comments updated for v13 target table
- `src/esp32/test_encoders.cpp` — rewritten for AS5600 dual I2C
- `docs/README.md` — reflects v13 file set and workflow
- `README.md` — hardware list updated to v13
- Schemes regenerated for v13 hardware

### Architecture deltas
| Metric | v11 | v12 (planned) | v13 (current) |
|--------|-----|---------------|---------------|
| MCU | ESP32 DevKitC V4 | ESP32-S3 N8R8 | ESP32-S3 N8R8 |
| Encoders | AS5600 (12-bit, mux) | AS5048A (14-bit, SPI) | AS5600 (12-bit, dual-I2C) |
| Distance | VL53L1X (4 m, mux) | TFMini-S (12 m) | VL53L1X (~3 m) |
| I2C devices | 5 | 1 | 5 (split across 2 buses) |
| I2C buses | 2 (via mux) | 1 | 2 (native peripherals) |
| Mux | TCA9548A | none | none (mux burned, replaced by topology) |

### TODO before competition
- [ ] Bench-test the AS5600 + VL53L1X stack on the actual chassis once parts are mounted
- [ ] Re-measure servo end-stops with target 7 (still pending v13 port; can read µs from target 10 bench output)
- [ ] Re-render `schemes/WRO_Full_System_Diagram.png` / `.svg` from the v13 `.mmd` source
- [ ] Compete

---

## v12.1 — Legacy file rename (May 2026, never fielded)

Renamed v11 source files to `legacy_*.cpp` so the v12 architecture could land cleanly. v12 was never deployed to hardware (parts weren't available); the file structure carried over into v13.

## v12.0 — Hardware Upgrade (May 2026, never fielded)
- **MCU:** ESP32 DevKitC V4 → ESP32-S3-DevKitC-1 N8R8 ✓ (carried into v13)
- **Encoders:** AS5600 (I2C) → AS5048A (SPI) ✗ (parts didn't arrive; v13 reverted)
- **Distance:** VL53L1X (I2C) → TFMini-S (UART) ✗ (parts didn't arrive; v13 reverted)
- TCA9548A I2C mux removed ✓ (carried into v13 — mux burned out anyway)

## v11.0 — Competition Ready (March 2026)
- Dual I2C bus architecture via TCA9548A mux
- Camera UART v3 with XOR checksum
- Parallel parking FSM (APPROACH → ALIGN → CREEP → FINAL)
- Blind turn improvements: WALL_BIT OR pillar dist < 35 cm
- S-curve pre-positioning from FAR pillar when both visible
- Bug fixes: lap counting cooldown, parking alignment delta
