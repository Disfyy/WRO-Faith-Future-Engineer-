# WRO Team Faith — Test-Drive Subagents (v13)

Four project-scoped Claude Code sub-agents that collapse the test-drive workflow into one invocation per phase. All four are **read-only** — they print what to change, you apply it.

Each agent maps to one decision you actually make during a session:

| Decision | Agent | Invoke when… |
|---|---|---|
| Should I drive? | [`wro-preflight`](./wro-preflight.md) | "preflight", "ready for track", "pre-run check", first run of the day, or after any hardware swap |
| Are my numbers right? | [`wro-calibrator`](./wro-calibrator.md) | "calibrate servo / encoder / IMU / camera", or after any mechanical change (new horn, wheel, magnet, camera mount) |
| What is the robot doing / did it do? | [`wro-run-coach`](./wro-run-coach.md) | "watch this run", "what's happening", "analyze this log", "what went wrong on lap 2" |
| What should I change next? | [`wro-pid-tuner`](./wro-pid-tuner.md) | After run-coach reports oscillation/overshoot/lag, or "suggest next gain", "PID tuning session", "log this tuning attempt" |

## End-to-end test-drive loop

```
wro-preflight  →  drive  →  (pipe serial into) wro-run-coach (live mode)
                                                       │
                                       save log → wro-run-coach (post mode)
                                                       │
                                              wro-pid-tuner → change RAM gain
                                                       │
                                                 drive again
                                                       │
                                                  repeat …
                                                       │
                                  winning gain → paste into wro_config_v13.h
```

For mechanical changes (servo horn, wheel, magnet, camera mount), insert `wro-calibrator` before the next preflight.

## Invocation

In any Claude Code session in this repo:
- Mention the agent's keyword in your prompt — Claude routes automatically based on the agent description.
- Or explicitly: `@wro-preflight`, `@wro-calibrator`, etc.

## Source-of-truth files the agents read

- `docs/WRO_FE_SKILL.md` — canonical project knowledge (hardware stack, FSM, failure modes)
- `docs/checklists/WRO_Preflight_Log.md` — preflight schema
- `docs/checklists/WRO_Quick_Race_Checklist.md` / `WRO_Robot_Master_Checklist_2026-03-27.md` — quick / master paths
- `docs/strategy/WRO_Track_Test_Cases.md` — TC-01..TC-08 pass criteria
- `docs/guides/WRO_Servo_Calibration_Guide.md`, `WRO_OpenMV_UART_Protocol.md`, `WRO_Wiring_Map_v13.md` — guides
- `docs/logs/WRO_PID_Tuning_Log.csv`, `WRO_Test_Log.csv`, `WRO_Maintenance_Log.csv` — CSV schemas
- `src/esp32/wro_config_v13.h` — all tunables, default gains
- `src/esp32/wro_hw_config_v13.h` — pin map, servo µs constants
- `src/esp32/wro_telemetry.cpp` — telemetry grammar + live-tune UART command set
- `src/openmv/openmv_main.py` — camera focal-length formula, LAB thresholds

## Design notes (why four and not more)

The temptation is one agent per workflow step. That creates "which one do I call right now?" friction. Four maps cleanly to the four decisions above. Intentionally NOT separate agents:

- **Battery monitor** — one checklist line + a multimeter; folded into `wro-preflight`.
- **Wiring auditor** — `docs/guides/WRO_Wiring_Map_v13.md` is the static source of truth.
- **Rule-compliance checker** — one-time matrix at `docs/strategy/WRO_Rule_Compliance_Matrix.md`; preflight already gates Rules 9.9 / 9.10 / 9.11 / 11.10.
- **Build/upload helper** — a single `#define` in `wro_build_target.h`.
- **Camera vision-debug** — covered by `wro-run-coach`'s `CAM=` reading. Revisit only if LAB thresholds are tweaked weekly.

## Firmware wishlist (future, not in these agents)

These agents work today against the existing 5 Hz telemetry. They get **substantially more useful** if `wro_telemetry.cpp` later adds:

- `framesOk / framesBadCs / framesRejected` camera-health counters
- `g_yaw_rate` instantaneous (cheap oscillation metric)
- Per-device consecutive I2C-error counts
- Gyro Z bias from boot calibration (one-time line)
- Motor-stall flag (`PWM > MIN_DRIVE_PWM && |v| < threshold for N ticks`)
- Per-corner event line on transition (`CORNER trig_tf=348 exit_yaw=82.1 dt=1240ms`)

File a separate task when ready.
