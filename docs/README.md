# WRO Project Pack

This folder contains firmware, diagnostics, and operation documents for the WRO robot.

## Core firmware (v12, active)
- `wro_v12_main.cpp` — main robot firmware (target 11, the active build)
- `wro_*.{h,cpp}` — modular layers (HAL / estimation / behavior / control / FSM)
- `wro_hw_config_v12.h` — pin map for ESP32-S3
- `wro_config_v12.h` — algorithm tunables + `OBSTACLE_MODE` flag
- `as5048a_spi.h` / `tfmini_s.h` — low-level drivers
- `scan_i2c_v12.cpp` — I2C diagnostics scanner (expects only IMU on bus)
- `bench_test_v12.cpp` — full sensor + actuator bench test
- `test_encoders.cpp` / `test_tfmini.cpp` — per-sensor tests

## Legacy firmware (v11, kept for reference)
- `legacy_eps323.cpp` — old v11 main robot firmware
- `legacy_test_*.cpp` — old v11 test/calibration utilities
- WRO_Config_Template.h — original tunable parameters template

## Operation docs
- WRO_Robot_Master_Checklist_2026-03-27.md
- WRO_Quick_Race_Checklist.md
- WRO_Wiring_Map_v12.md
- WRO_OpenMV_UART_Protocol.md
- WRO_Rule_Compliance_Matrix.md
- WRO_Characteristics_Audit_2026-04-09.md
- WRO_Migration_v11_to_v12.md

## Logs and templates
- WRO_Test_Log.csv
- WRO_PID_Tuning_Log.csv
- WRO_Preflight_Log.md
- WRO_Release_Notes_Template.md

## Recommended workflow (v12)
1. Run `scan_i2c_v12.cpp` (target 2) — verify only `0x68` (IMU) is on the bus.
2. Run `test_encoders.cpp` (target 8) — verify AS5048A SPI reads.
3. Run `test_tfmini.cpp` (target 9) — verify TFMini-S front distance.
4. Run `bench_test_v12.cpp` (target 10) — full hardware sanity check.
5. Switch to `wro_v12_main.cpp` (target 11) — wheels-up smoke test.
6. On-track tuning — straight, single corner, full lap.
