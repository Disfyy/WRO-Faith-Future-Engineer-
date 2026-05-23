# `docs/` — WRO Future Engineers documentation

All written documentation for Team Faith's WRO robot lives here, grouped by
purpose so a judge (or a teammate) can find what they need fast.

> Looking for everything in one place? Open [`INDEX.md`](INDEX.md) for the
> full table-of-contents.

## Subfolders

| Folder | What's inside |
|---|---|
| [`guides/`](guides/) | Engineering reference: wiring map, assembly + startup, servo calibration, OpenMV UART protocol |
| [`checklists/`](checklists/) | Race-day operations: master preflight, quick-race checklist, fillable preflight log |
| [`strategy/`](strategy/) | Analysis & planning: migration history, rule-compliance matrix, characteristics audit, track test cases, risk register, templates |
| [`logs/`](logs/) | Running CSV logs: PID tuning, maintenance, test results |
| [`rules/`](rules/) | Official WRO 2026 Future Engineers rules (reference copies, MD + PDF) |

## At a glance

### Firmware (`src/esp32/`) — for context, not maintained here

The active build is **v13** (ESP32-S3, dual native I2C, no mux). Entry point
`wro_v13_main.cpp` (target 11). v11 sources are archived under
[`../src/esp32/legacy/`](../src/esp32/legacy/).

### Recommended preflight workflow (v13)

1. `diag_scan_i2c_v13.cpp` (target 2) — verify both buses, expected addresses on each.
2. `diag_test_encoders.cpp` (target 8) — verify both AS5600s on dual-I2C accumulate ticks.
3. `diag_test_vl53l1x.cpp` (target 9) — verify front + side VL53L1X come up post-XSHUT-remap.
4. `diag_bench_test_v13.cpp` (target 10) — full hardware sanity check.
5. `wro_v13_main.cpp` (target 11) — wheels-up smoke test.
6. On-track tuning — straight, single corner, full lap.

For step-by-step preflight see
[`checklists/WRO_Robot_Master_Checklist_2026-03-27.md`](checklists/WRO_Robot_Master_Checklist_2026-03-27.md)
and [`checklists/WRO_Quick_Race_Checklist.md`](checklists/WRO_Quick_Race_Checklist.md).
