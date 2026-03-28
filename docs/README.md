# WRO Project Pack

This folder contains firmware, diagnostics, and operation documents for the WRO robot.

## Core firmware
- eps323.cpp — Main robot firmware
- scanerI2C.cpp — I2C/TCA diagnostics scanner
- WRO_Config_Template.h — Tunable parameters template

## Operation docs
- WRO_Robot_Master_Checklist_2026-03-27.md
- WRO_Quick_Race_Checklist.md
- WRO_Wiring_Map.md
- WRO_OpenMV_UART_Protocol.md

## Logs and templates
- WRO_Test_Log.csv
- WRO_PID_Tuning_Log.csv
- WRO_Preflight_Log.md
- WRO_Release_Notes_Template.md

## Recommended workflow
1. Run scanner and verify channel map.
2. Upload main firmware.
3. Validate E-Stop and camera timeout behavior.
4. Run short track tests and record in logs.
5. Tune Kp/Kd with controlled runs.
