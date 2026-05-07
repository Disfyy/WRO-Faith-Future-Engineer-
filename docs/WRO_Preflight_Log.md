# WRO Preflight Log (v12)

## Session
- Date:
- Location:
- Firmware: `wro_v12_main.cpp` (target 11)
- `OBSTACLE_MODE`: 0 (Open) / 1 (Obstacle) — circle one
- Operator:

## Hardware Check
- [ ] Battery voltage in range
- [ ] Connectors tight (motor power, servo, sensors)
- [ ] No wire damage
- [ ] Wheels / steering free movement
- [ ] TFMini-S powered from 5 V (not 3.3 V)

## I2C Scanner Check (`scan_i2c_v12.cpp`, target 2)
- [ ] ICM-20948 IMU at `0x68`
- [ ] **No other devices** on the bus (v12 has only the IMU on I2C)

## Encoder Check (`test_encoders.cpp`, target 8)
- [ ] AS5048A Left raw read 0–16383, ticks accumulate when wheel turns
- [ ] AS5048A Right raw read 0–16383, ticks accumulate when wheel turns
- [ ] No "-1" error returns from `as5048a_readAngle()`

## TFMini-S Check (`test_tfmini.cpp`, target 9)
- [ ] Front sensor reports distance in cm
- [ ] Signal strength > 100 (else `dist=9999` is filtered)
- [ ] Sensor mounted at 3–5 cm height, laser parallel to ground

## Bench Test (`bench_test_v12.cpp`, target 10)
- [ ] IMU yaw responds to chassis rotation
- [ ] Servo sweeps left/center/right cleanly
- [ ] Motor PWM ramps in commanded direction (wheels off ground)
- [ ] Camera frames received (UART2)
- [ ] E-Stop button reads correctly

## Safety Check (under target 11)
- [ ] E-Stop press = immediate motor stop (<50 ms)
- [ ] E-Stop release = controlled resume, PIDs reset
- [ ] Camera timeout (Obstacle mode) → SAFE_STOP after 3 s
- [ ] Software E-Stop (`!` over USB serial) works

## Servo Calibration
- [ ] Center, left, right µs measured on actual chassis (target 7 — needs v12 port)
- [ ] `SERVO_CENTER_US` / `SERVO_LEFT_US` / `SERVO_RIGHT_US` updated in `wro_hw_config_v12.h`
- [ ] 60 µs safety margin from each end-stop verified (`SERVO_LEFT_SAFE_US`, `SERVO_RIGHT_SAFE_US`)

## Final Decision
- [ ] READY FOR TRACK
- [ ] NEED FIXES BEFORE RUN

Notes:
