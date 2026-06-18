# Documentation Index

A single page that lists every document in this repository, grouped by
purpose. If you are a WRO judge or a new teammate, this is the fastest
way to find what you need.

## Top-level

| Path | Purpose |
|---|---|
| [`../README.md`](../README.md) | Project overview, hardware summary, software architecture, quick setup |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Versioned engineering history (v11 → v12 plan → v13 actual) |
| [`README.md`](README.md) | `docs/` folder navigation hub |

## Engineering writeups by topic — `../other/`

These are the WRO-rubric-aligned summaries; each one links out to the
canonical sources of truth.

| Topic | Writeup |
|---|---|
| Mobility (drivetrain / steering / chassis / control loops) | [`../other/mobility-management/README.md`](../other/mobility-management/README.md) |
| Power and sensing (battery, power dist., sensor stack) | [`../other/power-and-sense-management/README.md`](../other/power-and-sense-management/README.md) |
| Obstacle Challenge strategy (perception, FSM, parking) | [`../other/obstacle-management/README.md`](../other/obstacle-management/README.md) |

## Engineering reference — `guides/`

| Document | What it covers |
|---|---|
| [`guides/WRO_Wiring_Map_v13.md`](guides/WRO_Wiring_Map_v13.md) | Authoritative pin map for the v13 (ESP32-S3) build |
| [`guides/WRO_Robot_Assembly_and_Startup_Guide.md`](guides/WRO_Robot_Assembly_and_Startup_Guide.md) | Physical build steps + first power-on |
| [`guides/WRO_Servo_Calibration_Guide.md`](guides/WRO_Servo_Calibration_Guide.md) | Servo center / end-stop / µs-mapping procedure |
| [`guides/WRO_OpenMV_UART_Protocol.md`](guides/WRO_OpenMV_UART_Protocol.md) | Camera-to-ESP32 frame format spec (v3, original/fallback OpenMV backend) |
| [`guides/WRO_Pixy2_Setup.md`](guides/WRO_Pixy2_Setup.md) | Pixy2/2.1 signature teaching + UART config (active camera backend) |

## Race-day operations — `checklists/`

| Document | When to use it |
|---|---|
| [`checklists/WRO_Quick_Race_Checklist.md`](checklists/WRO_Quick_Race_Checklist.md) | 5-minute pre-run sanity pass |
| [`checklists/WRO_Robot_Master_Checklist_2026-03-27.md`](checklists/WRO_Robot_Master_Checklist_2026-03-27.md) | Full preflight — first run of the day or after major changes |
| [`checklists/WRO_Preflight_Log.md`](checklists/WRO_Preflight_Log.md) | Fillable run-by-run log template |

## Strategy and analysis — `strategy/`

| Document | Content |
|---|---|
| [`strategy/WRO_Migration_v12_to_v13.md`](strategy/WRO_Migration_v12_to_v13.md) | Why v12 (AS5048A SPI + TFMini-S) was abandoned and how v13 was assembled |
| [`strategy/WRO_Rule_Compliance_Matrix.md`](strategy/WRO_Rule_Compliance_Matrix.md) | Per-rule mapping to firmware/hardware (line refs v11; needs v13 re-audit) |
| [`strategy/WRO_Characteristics_Audit_2026-04-09.md`](strategy/WRO_Characteristics_Audit_2026-04-09.md) | Historical audit of template-vs-runtime values (v11 lines, archival) |
| [`strategy/WRO_Track_Test_Cases.md`](strategy/WRO_Track_Test_Cases.md) | Structured test scenarios with expected behaviour |
| [`strategy/WRO_Risk_Register.md`](strategy/WRO_Risk_Register.md) | Identified risks + mitigations |
| [`strategy/WRO_Release_Notes_Template.md`](strategy/WRO_Release_Notes_Template.md) | Template for tagging firmware releases |
| [`strategy/WRO_Config_Template.h`](strategy/WRO_Config_Template.h) | Reference list of tunables (v11 vintage; cross-check against `src/esp32/wro_config_v13.h`) |

## Running logs — `logs/`

| Log | What's recorded |
|---|---|
| [`logs/WRO_PID_Tuning_Log.csv`](logs/WRO_PID_Tuning_Log.csv) | Each PID gain change + observed behaviour |
| [`logs/WRO_Maintenance_Log.csv`](logs/WRO_Maintenance_Log.csv) | Hardware swaps, repairs, soldering events |
| [`logs/WRO_Test_Log.csv`](logs/WRO_Test_Log.csv) | Per-run lap times and incidents |

## Competition reference — `rules/`

| Document | Content |
|---|---|
| [`rules/WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.md`](rules/WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.md) | Markdown copy of the official rules |
| [`rules/WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.pdf`](rules/WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.pdf) | Original PDF |

## Internal / meta

| Document | Purpose |
|---|---|
| [`WRO_FE_SKILL.md`](WRO_FE_SKILL.md) | Project-context document for AI-assisted work on this repo |
