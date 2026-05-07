# CHANGELOG

## v12.0 — Hardware Upgrade (May 2026)

### Breaking changes
- **MCU:** ESP32 DevKitC V4 → ESP32-S3-DevKitC-1 N8R8
- **Encoders:** AS5600 (I2C, 12-bit) → AS5048A (SPI, 14-bit)
- **Distance sensors:** VL53L1X (I2C, 4m) → TFMini-S (UART, 12m)
- **All pin assignments changed** — see `docs/WRO_Wiring_Map_v12.md`
- TCA9548A I2C mux **removed** (no longer needed)
- Second I2C bus Wire1 **removed**

### New files
| File | Purpose |
|------|---------|
| `src/esp32/wro_hw_config_v12.h` | All pin definitions + constants for ESP32-S3 |
| `src/esp32/as5048a_spi.h` | AS5048A SPI driver (replaces AS5600 I2C reads) |
| `src/esp32/tfmini_s.h` | TFMini-S UART driver (replaces VL53L1X) |
| `src/esp32/test_tfmini.cpp` | TFMini-S distance test |
| `src/esp32/bench_test_v12.cpp` | Full sensor + actuator bench test |
| `docs/WRO_Wiring_Map_v12.md` | Complete v12 wiring reference |
| `docs/WRO_Migration_v11_to_v12.md` | Step-by-step migration guide |

### Updated files
| File | Change |
|------|--------|
| `src/esp32/wro_build_target.h` | Added targets 9 (TFMini) and 10 (bench test) |
| `src/esp32/esp32.ino` | Updated comments for v12 targets |
| `src/esp32/test_encoders.cpp` | Rewritten for AS5048A SPI |
| `src/esp32/scanerI2C.cpp` | Updated for v12 (expects IMU only on bus) |

### Architecture improvements
| Metric | v11 | v12 |
|--------|-----|-----|
| I2C devices | 5 | 1 (IMU only) |
| I2C buses | 2 | 1 |
| Odometry | 277 ticks/cm | 1110 ticks/cm (+4×) |
| Max distance | 4 m | 12 m (+3×) |
| Encoder bus | I2C 400 kHz | SPI 1 MHz |
| PSRAM | 0 | 8 MB |
| USB | CP2102 bridge | Native USB-C |
| All-bidirectional GPIO | No (GPIO 34-39 input-only) | Yes |

### TODO before competition
- [ ] Update `eps323.cpp` main firmware (see migration guide)
- [ ] Recalibrate odometry on real track (new 14-bit resolution)
- [ ] Physical wiring per new pin map
- [ ] Wire TFMini-S side sensor (GPIO47)
- [ ] Competition track testing

---

## v11.0 — Competition Ready (March 2026)
- Dual I2C bus architecture (no TCA9548A mux)
- Camera UART v3 with XOR checksum
- Parallel parking FSM (APPROACH → ALIGN → CREEP → FINAL)
- Blind turn improvements: WALL_BIT OR pillar dist < 35cm
- S-curve pre-positioning from FAR pillar when both visible
- Bug fixes: lap counting cooldown, parking alignment delta
