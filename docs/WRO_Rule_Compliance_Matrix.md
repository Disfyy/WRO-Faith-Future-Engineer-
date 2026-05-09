# WRO Rule Compliance Matrix

Date: 9 April 2026 (v11-era audit) — **v13 re-audit pending**
Scope: Official WRO 2026 rules + internal project safety rules

> ⚠️ **v13 status (May 2026):** All `src/esp32/eps323.cpp:NNN` line references in this matrix are **stale** — `eps323.cpp` is now `legacy_eps323.cpp` (v11 reference, inactive). The active firmware is `src/esp32/wro_v13_main.cpp` and its modules `wro_*.{cpp,h}`. The mapping below should be regenerated against the v13 source. Until then, treat status entries as historical.
>
> Rule compliance points that map to v13 modules:
> - **Rule 9.9** (compile-time mode): `src/esp32/wro_config_v13.h` — `#define OBSTACLE_MODE 0/1`
> - **Rule 9.10/9.11** (single start button): `src/esp32/wro_estop.cpp` — press+release start
> - **Rule 9.24.2** (Open finish): `src/esp32/wro_race_fsm.cpp` — `RS_FINISH` after `lap_count >= TARGET_LAPS_RACE`
> - **Rule 9.24.4** (Obstacle finish): `src/esp32/wro_park.cpp` — `PK_FINAL` → `RS_FINISH`
> - **Rule 11.10** (no wireless during runs): `src/esp32/wro_v13_main.cpp` — `WiFi.mode(WIFI_OFF); btStop();` in `setup()`
> - **Internal E-Stop safety**: `src/esp32/wro_estop.cpp` + `src/esp32/wro_race_fsm.cpp` (RS_SAFE_STOP path)

Official source:
- `WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.pdf`
- Extracted anchors used in this matrix:
  - Page 17: rules 9.6, 9.9, 9.10, 9.11, 9.13, 9.14
  - Page 19: rules 9.24.2, 9.24.4
  - Page 23: rules 11.1, 11.2

---

## A) Official WRO Rules -> Implementation -> Test

| ID | Rule | Requirement summary | Implementation anchor | Verification anchor | Status |
|----|------|---------------------|------------------------|---------------------|--------|
| O-9.6 | WRO 9.6 | Vehicle must be placed in start zone switched off | `docs/WRO_Robot_Assembly_and_Startup_Guide.md` (Stage 11 + Stage 15 startup flow) | `docs/WRO_Track_Test_Cases.md` TC-07 | NEEDS PHYSICAL VERIFY |
| O-9.9 | WRO 9.9 | No data entry via physical switch/config changes during round prep | `src/esp32/eps323.cpp:48` (`#define OBSTACLE_CHALLENGE_MODE` compile-time), `docs/WRO_Robot_Assembly_and_Startup_Guide.md:697` | Code review + TC-07 | PASS (design), NEEDS EVENT DISCIPLINE |
| O-9.10 | WRO 9.10 | Only one switch allowed for power on | `docs/WRO_Robot_Assembly_and_Startup_Guide.md:178` (single KCD3 architecture) | Hardware inspection + TC-07 | NEEDS PHYSICAL VERIFY |
| O-9.11 | WRO 9.11 | After power on vehicle must wait for one start button | `src/esp32/eps323.cpp:404` (`handleStartButton`), `src/esp32/eps323.cpp:366` (`checkEStop`) | TC-07 | NEEDS TRACK VERIFY |
| O-9.13 | WRO 9.13 | Judge command then start button press starts attempt time | Team operational procedure in guide/checklists | TC-07 rehearsal with judge countdown | NEEDS OPERATIONAL VERIFY |
| O-9.14 | WRO 9.14 | Pressing start button must trigger challenge movement | `src/esp32/eps323.cpp:404` and transition to tracking state in loop | TC-07 | NEEDS TRACK VERIFY |
| O-9.24.2 | WRO 9.24.2 | Open Challenge: after 3 laps stop in finish section autonomously | `src/esp32/eps323.cpp:640` (`checkLapCompletion`) + `src/esp32/eps323.cpp:429` (`finishRace`) | TC-08 (Open) | RISK: finish-section position must be confirmed on real track |
| O-9.24.4 | WRO 9.24.4 | Obstacle Challenge: after 3 laps stop (valid section or parking) | `src/esp32/eps323.cpp:640` -> parking flow, `src/esp32/eps323.cpp:1103` (`finishRace` from parking FSM) | TC-08 (Obstacle) | NEEDS TRACK VERIFY |
| O-11.1 | WRO 11.1 | Dimensions <= 300x200x300 mm | Mechanical build constraints in assembly guide | Pre-competition measurement checklist | NEEDS PHYSICAL VERIFY |
| O-11.2 | WRO 11.2 | Weight <= 1.5 kg | Mechanical/BOM discipline | Scale check at inspection | NEEDS PHYSICAL VERIFY |

---

## B) Internal Project Rules -> Implementation -> Test

| ID | Internal requirement | Implementation anchor | Verification anchor | Status |
|----|----------------------|------------------------|---------------------|--------|
| I-SAFE-ESTOP | E-Stop must force safe stop in all states | `src/esp32/eps323.cpp:349` (`safeStop`), `src/esp32/eps323.cpp:366` (`checkEStop`) | TC-02 | NEEDS TRACK VERIFY |
| I-SAFE-CAM | Camera timeout warning + offline safe stop | `src/esp32/eps323.cpp:139`, `src/esp32/eps323.cpp:140`, `src/esp32/eps323.cpp:731`, `src/esp32/eps323.cpp:1153` | TC-03 | NEEDS TRACK VERIFY |
| I-SAFE-ENC | Persistent encoder loss must trigger safe stop | `src/esp32/eps323.cpp` (`updateOdometry`, encoder fault path) | TC-04 | NEEDS TRACK VERIFY |
| I-I2C-MAP | Dual-bus device map must match firmware (no TCA9548A in v13) | `docs/WRO_Robot_Assembly_and_Startup_Guide.md` Stage 6, `docs/WRO_Wiring_Map_v13.md` | `scan_i2c_v13.cpp` (target 2) + TC-01 | NEEDS PHYSICAL VERIFY |
| I-LAPS-3 | Target laps must be 3 in both modes | `src/esp32/eps323.cpp:160`, `src/esp32/eps323.cpp:640` | TC-05 / TC-08 | NEEDS TRACK VERIFY |
| I-UART-V3 | OpenMV must send 6-field UART v3 frame with CRC | `docs/WRO_OpenMV_UART_Protocol.md`, `src/openmv/openmv_main.py` frame assembly | Serial parsing check + TC-03 | NEEDS TRACK VERIFY |

---

## C) Gaps and actions

1. Open finish-section compliance (`O-9.24.2`) is potentially sensitive: lap count trigger may happen near boundary, so real-track validation is mandatory.
2. Physical constraints (`O-11.1`, `O-11.2`) are not enforceable in firmware; keep hardware measurement records in preflight log.
3. Operational rules (`O-9.13`) require team discipline, not only firmware behavior.

Recommended next action:
- Execute `docs/WRO_Track_Test_Cases.md` TC-01..TC-08 and update statuses from `NEEDS ... VERIFY` to `PASS/FAIL` with evidence links to logs/videos.
