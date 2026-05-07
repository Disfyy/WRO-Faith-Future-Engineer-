# WRO Quick Race Checklist (v12)

> Hardware: ESP32-S3 + AS5048A SPI encoders + TFMini-S front + ICM-20948 IMU.
> No I2C mux. See `WRO_Wiring_Map_v12.md` for full pin reference.

## 1) Before Power ON
- Battery charged and voltage checked
- Wheels free, no mechanical jam
- E-Stop button physically works (GPIO 21, INPUT_PULLUP)
- Connectors fixed, no loose wire
- One power switch only (WRO 9.10)
- One start button only (WRO 9.11) — same E-Stop button (press+release = start)
- Wi-Fi/BT confirmed disabled in firmware (Rule 11.10)

## 2) Power ON
- ESP32-S3 boots; serial banner reads `WiFi: OFF, BT: OFF`
- Gyro calibration runs (`Calibrating gyro Z bias...`) — 3 s, no movement
- Steering centers correctly
- Motor remains stopped at startup
- Status LED behavior normal (LED on after init complete)

## 3) Sensor Check (run each in turn before race)
- Target 2 (`WRO_TARGET_SCAN_I2C` → `scan_i2c_v12.cpp`)
  - Finds **only** `0x68` (ICM-20948 IMU). No other devices.
  - If anything else appears → NO-GO until fixed.
- Target 8 (`test_encoders.cpp`)
  - Both AS5048A encoders read 0–16383, ticks accumulate when wheels spin.
- Target 9 (`test_tfmini.cpp`)
  - Front TFMini-S reports a sane distance (cm) with strength > 100.
  - Confirm 5 V power on TFMini (NOT 3.3 V).
- Target 10 (`bench_test_v12.cpp`)
  - All sensors + actuators + camera + E-Stop respond live.
- Switch back to target 11 (`wro_v12_main.cpp`) for the race.

## 4) Safety Check
- Press E-Stop: motor=0, steering centers, LED indicator
- Release E-Stop: system resumes (PIDs reset, corner lockout 800 ms)
- Camera disconnect test in Obstacle mode → degraded → SAFE_STOP after 3 s
- Software E-Stop: send `!` over USB serial → motor stops

## 5) Mode Selection (Rule 9.9)
- Confirm `OBSTACLE_MODE` value in `wro_config_v12.h`:
  - `0` = Open Challenge
  - `1` = Obstacle Challenge
- This is **compile-time only** — no physical mode switch on robot.

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
