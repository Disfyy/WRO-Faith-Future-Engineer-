# WRO Preflight Log (v13)

## Session
- Date:
- Location:
- Firmware: `wro_v13_main.cpp` (target 11)
- `OBSTACLE_MODE`: 0 (Open) / 1 (Obstacle) — circle one
- `HAS_SIDE_TOF`: 0 / 1 — circle one
- Operator:

## Hardware Check
- [ ] Battery voltage in range (LiPo 7.0–8.4 V)
- [ ] Connectors tight (motor power, servo, sensors)
- [ ] No wire damage
- [ ] Wheels / steering move freely
- [ ] VL53L1X sensors powered from 5 V (VIN), not 3.3 V

## I2C Scanner Check (`scan_i2c_v13.cpp`, target 2)
Both buses scanned. Pre-VL53L1X-remap, expected addresses are:
- [ ] **I2C0 (Wire)**: `0x68` (IMU) + `0x36` (AS5600 Left) + `0x29` (VL53L1X Front)
- [ ] **I2C1 (Wire1)**: `0x36` (AS5600 Right) + `0x29` (VL53L1X Side)
- [ ] **No `0x70`** (TCA9548A is gone in v13)

## Encoder Check (`test_encoders.cpp`, target 8)
- [ ] AS5600 Left (I2C0) raw 0–4095, ticks accumulate when wheel turns
- [ ] AS5600 Right (I2C1) raw 0–4095, ticks accumulate when wheel turns
- [ ] No `-1` returns from `as5600_read()`
- [ ] Magnet status reports `OK` for both encoders

## VL53L1X Check (`test_vl53l1x.cpp`, target 9)
- [ ] Front and side sensors come up at remapped addresses (0x30, 0x31)
- [ ] Distances change smoothly when a hand is moved 10–100 cm in front
- [ ] No `9999` saturation reads at normal distances
- [ ] XSHUT pins driven HIGH after boot dance completes

## Bench Test (`bench_test_v13.cpp`, target 10)
- [ ] IMU yaw responds to chassis rotation by hand
- [ ] Servo sweeps left/center/right cleanly via `sl`/`sc`/`sr`
- [ ] Motor PWM ramps forward and reverse via `f`/`b` (wheels off the ground!)
- [ ] Camera frames received over UART2 (live mode `e`)
- [ ] E-Stop button reads correctly (status `s`)

## Safety Check (under target 11)
- [ ] E-Stop press → immediate motor stop (<50 ms)
- [ ] E-Stop release → controlled resume, PIDs reset
- [ ] Camera timeout (Obstacle mode) → `SAFE_STOP` after `CAM_SILENT_STOP_MS` (3 s)
- [ ] Software E-Stop (`!` over USB serial) works
- [ ] Encoder fail-counter trips → `SAFE_STOP` (verify by yanking one I2C bus briefly)

## Servo Calibration (`sketches/servo_calibrate/servo_calibrate.ino`)
- [ ] Center, left, right µs measured on the actual chassis (using the calibration sketch)
- [ ] `SERVO_CENTER_US` / `SERVO_LEFT_US` / `SERVO_RIGHT_US` updated in `wro_hw_config_v13.h`
- [ ] 60 µs safety margin (`SERVO_MARGIN_US`) leaves the servo unstalled at the end-stops

## Final Decision
- [ ] READY FOR TRACK
- [ ] NEED FIXES BEFORE RUN

Notes:
