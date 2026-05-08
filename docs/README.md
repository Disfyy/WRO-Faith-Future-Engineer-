# WRO Project Pack

This folder contains firmware, diagnostics, and operation documents for the WRO robot.

## Core firmware (v13, active)
- `wro_v13_main.cpp` — main robot firmware (target 11, the active build)
- `wro_*.{h,cpp}` — modular layers (HAL / estimation / behavior / control / FSM)
- `wro_hw_config_v13.h` — pin map for ESP32-S3 with dual I2C
- `wro_config_v13.h` — algorithm tunables + `OBSTACLE_MODE` flag
- `as5600_dual_i2c.h` / `vl53l1x_dual.h` — low-level drivers
- `scan_i2c_v13.cpp` — I2C diagnostics scanner (both buses)
- `bench_test_v13.cpp` — full sensor + actuator bench test
- `test_encoders.cpp` / `test_vl53l1x.cpp` — per-sensor tests

## Legacy firmware (v11, kept for reference)
- `legacy_eps323.cpp` — old v11 main robot firmware
- `legacy_test_*.cpp` — old v11 test/calibration utilities
- WRO_Config_Template.h — original tunable parameters template

## Operation docs
- WRO_Robot_Master_Checklist_2026-03-27.md
- WRO_Quick_Race_Checklist.md
- WRO_Wiring_Map_v13.md
- WRO_OpenMV_UART_Protocol.md
- WRO_Rule_Compliance_Matrix.md
- WRO_Characteristics_Audit_2026-04-09.md
- WRO_Migration_v12_to_v13.md

## Logs and templates
- WRO_Test_Log.csv
- WRO_PID_Tuning_Log.csv
- WRO_Preflight_Log.md
- WRO_Release_Notes_Template.md

## Recommended workflow (v13)
1. Run `scan_i2c_v13.cpp` (target 2) — verify both buses, expected addresses on each.
2. Run `test_encoders.cpp` (target 8) — verify both AS5600s on dual-I2C accumulate ticks.
3. Run `test_vl53l1x.cpp` (target 9) — verify front + side VL53L1X come up post-XSHUT-remap.
4. Run `bench_test_v13.cpp` (target 10) — full hardware sanity check.
5. Switch to `wro_v13_main.cpp` (target 11) — wheels-up smoke test.
6. On-track tuning — straight, single corner, full lap.
