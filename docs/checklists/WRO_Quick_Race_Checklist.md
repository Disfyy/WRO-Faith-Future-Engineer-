# WRO Quick Race Checklist (v13)

> Hardware: ESP32-S3 + 2× AS5600 (dual native I2C) + 2× VL53L1X (XSHUT addr-remap) + ICM-20948 IMU.
> No I2C mux (TCA9548A removed). See [`WRO_Wiring_Map_v13.md`](../guides/WRO_Wiring_Map_v13.md) for the full pin reference.

## 1) Before Power ON
- Battery charged and voltage checked
- Wheels free, no mechanical jam
- E-Stop button physically works (GPIO 21, INPUT_PULLUP)
- Connectors fixed, no loose wire
- One power switch only (WRO Rule 9.10)
- One start button only (WRO Rule 9.11) — same E-Stop button (press+release = start)
- Wi-Fi/BT confirmed disabled in firmware (Rule 11.10)

## 2) Power ON
- ESP32-S3 boots; serial banner reads `WiFi: OFF, BT: OFF`
- Banner: `WRO FE 2026 -- Team Faith -- v13.0 main firmware`
- VL53L1X boot dance succeeds (`VL53L1X FRONT: OK at 0x30`, `VL53L1X SIDE: OK at 0x31`)
- AS5600 dual I2C: `OK`
- Gyro calibration runs (`Calibrating gyro Z bias...`) — 3 s, robot still
- Steering centers correctly
- Motor remains stopped at startup
- Status LED behavior normal (LED on after init complete)

## 3) Sensor Check (run each in turn before race)
- Target 2 (`WRO_TARGET_SCAN_I2C` → `scan_i2c_v13.cpp`)
  - **I2C0** finds `0x68` (IMU) + `0x36` (AS5600 L) + `0x29` (VL53L1X F pre-remap)
  - **I2C1** finds `0x36` (AS5600 R) + `0x29` (VL53L1X S pre-remap)
  - **No `0x70`** (mux is gone). Anything unexpected → NO-GO.
- Target 8 (`test_encoders.cpp`)
  - Both AS5600s read 0–4095, ticks accumulate when wheels spin
  - Magnet status `OK` for both
- Target 9 (`test_vl53l1x.cpp`)
  - Front and side report distances, no `9999` saturation under normal conditions
  - 5 V on VIN of each VL53L1X (NOT 3.3 V)
- Target 10 (`bench_test_v13.cpp`)
  - All sensors + actuators + camera + E-Stop respond live (`s` then `e`)
- Switch back to target 11 (`wro_v13_main.cpp`) for the race.

## 4) Safety Check
- Press E-Stop: motor = 0, steering centers, LED indicator
- Release E-Stop: system resumes (PIDs reset, corner lockout 800 ms)
- Camera disconnect test in Obstacle mode → degraded → `SAFE_STOP` after 3 s
- Software E-Stop: send `!` over USB serial → motor stops

## 5) Mode Selection (Rule 9.9)
- Confirm `OBSTACLE_MODE` value in `wro_config_v13.h`:
  - `0` = Open Challenge
  - `1` = Obstacle Challenge
- Compile-time only — no physical mode switch on the robot.

## 6) Track Start
- Correct profile loaded (PWM, KP/KI/KD)
- First run at reduced speed (use `S-` over serial to bump down)
- Observe 1 full lap before race pace
- Watch live telemetry: `T=... ST=... LAP=... YAW=... TF=...mm CAM=...`

## 7) Final Go/No-Go
- No critical warnings in serial
- Stable steering and lap counting (gyro 360° accumulator)
- Team sign-off complete
- If any safety/rule check fails → NO-GO

Sign: ____________________  Time: ____________________
