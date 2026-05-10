---
name: wro-fe-team-faith
description: >
  Use this skill for ALL tasks related to Team Faith's WRO Future Engineers 2026 project.
  Trigger whenever the user mentions WRO, robot, ESP32, ESP32-S3, OpenMV, PID, servo, FSM, odometry,
  obstacle challenge, open challenge, firmware, sensors, I2C, SPI, UART, camera, encoders, checklist,
  preflight, tuning, competition, engineering journal, or anything related to their autonomous car.
  This skill contains full project context so Claude can help without re-explanation every time.
---

# WRO Future Engineers 2026 — Team Faith Skill

## Project Overview
**Team:** Faith
**Competition:** World Robot Olympiad (WRO) Future Engineers 2026
**Category:** Self-Driving Cars (Open Challenge + Obstacle Challenge)
**Repo location:** `/Users/nr_ulan/Desktop/WRO_Project_Pack/`
**Active hardware:** v13 (ESP32-S3 + 2× AS5600 dual-I2C + 2× VL53L1X XSHUT-remap, no I2C mux)

---

## Hardware Stack (v13 — current)

| Component | Model | Role |
|---|---|---|
| Main Controller | ESP32-S3-DevKitC-1 N8R8 | PID, Odometry, FSM, sensor fusion |
| Vision | OpenMV H7 Plus | Color blob tracking, obstacle detection |
| IMU | Adafruit ICM-20948 (9-DoF) | Heading, yaw, lap counting |
| Encoders | 2× AS5600 magnetic (12-bit, dual I2C) | Odometry (277 ticks/cm), one per native I2C peripheral |
| Front distance | VL53L1X (I2C, ~3 m Medium mode) | Corner detection, pre-emptive braking |
| Side distance | VL53L1X (I2C) | Wall-follow for narrow Open Challenge corridors |
| Motor Driver | BTS7960 43A H-Bridge | Drive motor control |
| Steering | JX PDI-6221MG Digital Servo | Steering control |
| Power | 2S/3S LiPo + 5V Step-Down | Power supply |

> v12 was a planned migration to AS5048A SPI + TFMini-S UART; the parts didn't arrive. Meanwhile the v11 TCA9548A I2C mux burned out. v13 keeps the new ESP32-S3 main controller and reverts to the v11 sensor stack — collisions resolved by dual native I2C peripherals (one AS5600 per bus) and runtime XSHUT-based VL53L1X address remapping. See [`docs/strategy/WRO_Migration_v12_to_v13.md`](strategy/WRO_Migration_v12_to_v13.md).

### ESP32-S3 Pin Map (v13)
- **I2C0 (Wire), GPIO 8/9:** ICM-20948 (0x68) + AS5600 Left (0x36) + VL53L1X Front (0x29 → 0x30)
- **I2C1 (Wire1), GPIO 11/12:** AS5600 Right (0x36) + VL53L1X Side (0x29 → 0x31)
- **VL53L1X XSHUT pins:** GPIO 15 (Front), GPIO 16 (Side), GPIO 47 (reserved 3rd)
- **UART2 (OpenMV camera):** RX=17, TX=18
- **BTS7960 motor:** R_EN=38, L_EN=39, R_PWM=40, L_PWM=41
- **Steering servo:** GPIO 42
- **E-Stop:** GPIO 21 (INPUT_PULLUP)
- **Status LED:** GPIO 2 / RGB WS2812: GPIO 48

### I2C Address Map (post-boot)
- **I2C0:** 0x68 (IMU), 0x36 (AS5600 L), 0x30 (VL53L1X F)
- **I2C1:** 0x36 (AS5600 R), 0x31 (VL53L1X S)
- The AS5600 0x36 collision is sidestepped by the bus split. The VL53L1X collision is sidestepped by the boot-time XSHUT remap dance in `vl53l1x_dual.h`.

---

## Software Architecture

### Two independent threads:
1. **Vision (OpenMV, MicroPython):** `src/openmv/` — Detects Orange/Blue lines (lap direction), Red/Green pillars, magenta parking blocks, black walls. Sends UART v3 frame: `RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n` at ~50 Hz with XOR checksum. Spec: `docs/guides/WRO_OpenMV_UART_Protocol.md`.
2. **Control (ESP32-S3, C++):** `src/esp32/wro_v13_main.cpp` — modular, layered. Build target = `WRO_TARGET_V13_MAIN` (11) in `wro_build_target.h`.

### Layered Architecture (v13)
```
HAL          drivers (as5600_dual_i2c.h, vl53l1x_dual.h, ICM20948 lib)
Estimation   wro_imu / wro_odometry / wro_camera / wro_sensors
Behavior     wro_corner / wro_behavior_open / wro_behavior_obstacle / wro_park
Control      wro_v13_main (steering_mixer + speed_ramp)
FSM          wro_race_fsm (top) + wro_estop (parallel)
```

### FSM States
`INIT → WAIT_START → RUN_OPEN | RUN_OBSTACLE ↔ TURN_90 → PARKING* | FINISH`
plus `SAFE_STOP` (parallel — entered on E-Stop held / sensor fault, resumes on release).

### Key Algorithms (unchanged from v12 architecture)
- **Cornering owned by VL53L1X front + IMU yaw delta** — camera is NEVER an exit condition (the v11 turn failure was a camera-gated trigger). Trigger: front<350 mm × 3 frames. Exit: 80° gyro delta. Speed during turn: PWM 70.
- **Pillar PID (Obstacle):** Red → keep right (setpoint +60 px), Green → keep left (-60 px). KP=0.45, KI=0.001, KD=0.30 (EMA-filtered derivative). Reset integral on color switch. Far-pillar pre-position at 0.4× gain.
- **Heading-hold (Open):** snap target heading to nearest 90° after each corner exit. KP=12 µs/deg, KD=2.0, KI=0 (drift over 3 min).
- **Lap counter:** primary = gyro 360° accumulator. Secondary sanity-check = camera line bit with cooldown.
- **Parallel parking (Obstacle, after lap 3):** APPROACH → ALIGN → 3-phase REVERSE → FINAL. Aborts to FINISH if camera silent.
- **E-Stop FSM:** press+release before race = ARM/START. Held during race = SAFE_STOP (motor off in <50 ms). Released = RESUME with PIDs reset. 500 ms boot grace.

---

## Mode Selection (Rule 9.9 compliant — compile-time only)
```cpp
// in src/esp32/wro_config_v13.h
#define OBSTACLE_MODE  0   // 0 = Open, 1 = Obstacle
```

## Key Tunables (in `src/esp32/wro_config_v13.h`)
```cpp
TURN_SLOWDOWN_MM   600
TURN_COMMIT_MM     350
TURN_TARGET_DEG    80
TURN_SPEED_PWM     70           // was 110 in v11; slip is what cost the turn
OPEN_MAX_PWM       80
OBS_MAX_PWM        130          // was 140
PILLAR_KP          0.45f        // KI=0.001, KD=0.30 (EMA alpha 0.30)
HEADING_KP         12.0f        // KI=0 deliberately
TARGET_LAPS_RACE   3
LOOP_INTERVAL_MS   10
ESTOP_BOOT_GRACE_MS 500
HAS_SIDE_TOF       1            // v13: side VL53L1X is part of the default build
```

Servo center/limits in `wro_hw_config_v13.h` are datasheet defaults — re-measure on the actual chassis with target 7 (TEST_SERVO_CAL, currently a v11 legacy and pending a v13 port; can also read µs from target 10 bench output) and add `SERVO_MARGIN_US=60` from each end-stop.

---

## Repository Structure (v13)
```
WRO_Project_Pack/
├── src/
│   ├── esp32/
│   │   ├── wro_v13_main.cpp     # active main firmware (target 11)
│   │   ├── wro_*.{h,cpp}         # modular layers
│   │   ├── wro_hw_config_v13.h   # pin map (dual I2C + XSHUT)
│   │   ├── wro_config_v13.h      # tunables + OBSTACLE_MODE + HAS_SIDE_TOF
│   │   ├── wro_build_target.h    # target selector
│   │   ├── as5600_dual_i2c.h     # AS5600 dual-bus driver
│   │   ├── vl53l1x_dual.h        # VL53L1X driver with XSHUT addr remap
│   │   ├── bench_test_v13.cpp    # full bench (target 10)
│   │   ├── test_encoders.cpp     # AS5600 dual-I2C test (target 8)
│   │   ├── test_vl53l1x.cpp      # VL53L1X test (target 9)
│   │   ├── scan_i2c_v13.cpp      # 2-bus I2C scanner (target 2)
│   │   └── legacy/               # v11 archived sources (legacy_*.cpp + README)
│   └── openmv/                   # MicroPython vision
├── schemes/                      # Wiring diagrams (v13)
├── models/HSP94182_3D/           # CAD chassis files (STL/STEP/SCAD)
├── docs/                         # Checklists, guides, logs
├── t-photos/, v-photos/, video/  # Required WRO assets
├── CHANGELOG.md
└── README.md
```

---

## Key Documents (in `docs/`)

### `docs/guides/` — engineering reference
- `WRO_Wiring_Map_v13.md` — full v13 pin reference (authoritative)
- `WRO_OpenMV_UART_Protocol.md` — camera frame spec (v3)
- `WRO_Robot_Assembly_and_Startup_Guide.md` — physical build (line refs may be v11; verify)
- `WRO_Servo_Calibration_Guide.md` — servo tuning procedure

### `docs/checklists/` — race-day operations
- `WRO_Quick_Race_Checklist.md` — competition-day quick check
- `WRO_Robot_Master_Checklist_2026-03-27.md` — full preflight (line refs v11; verify)
- `WRO_Preflight_Log.md` — fillable preflight log

### `docs/strategy/` — analysis and planning
- `WRO_Migration_v12_to_v13.md` — what changed and why
- `WRO_Track_Test_Cases.md` — structured test scenarios
- `WRO_Rule_Compliance_Matrix.md` — WRO rule compliance (line refs v11; needs re-audit)
- `WRO_Characteristics_Audit_2026-04-09.md` — historical audit (v11 lines, stale)
- `WRO_Risk_Register.md` — risk assessment
- `WRO_Release_Notes_Template.md` / `WRO_Config_Template.h` — templates

### `docs/logs/` — running CSV logs
- `WRO_PID_Tuning_Log.csv` / `WRO_Maintenance_Log.csv` / `WRO_Test_Log.csv`

### `docs/rules/` — reference
- `WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.{md,pdf}`

---

## Common Failure Modes & Fixes (v13)
| Problem | Fix |
|---|---|
| I2C scan finds wrong devices | v13 expects 0x68 + 0x36 + 0x29/0x30 on I2C0; 0x36 + 0x29/0x31 on I2C1. 0x70 = old TCA9548A (gone). |
| AS5600 reads -1 | Check the *correct* bus (Wire vs Wire1), magnet gap (0.5–3 mm), 3.3 V power, pull-ups (4.7 kΩ). |
| VL53L1X never comes up post-remap | XSHUT pin must be driven HIGH only after `setBus()` and before `init()`. Sequence is in `vl53l1x_dual.h`. |
| Both VL53L1Xs end up at 0x29 | XSHUT logic order broken — front must finish boot+remap before side is released. |
| Camera frame XOR fail | Check baud 115200, RX/TX crossed, common GND with ESP32-S3. Jump-defense rejects sudden distance jumps. |
| Robot under/over-rotates corners | Tune `TURN_TARGET_DEG`, `TURN_SPEED_PWM`. NEVER use encoder ticks as exit. |
| Strong oscillation in straight | Lower `HEADING_KP`, increase `HEADING_KD`, reduce speed. |
| Heading drift over 3 min | ICM-20948 gyro Z drifts with motor heat. Snap-to-90° after each corner already mitigates. |

---

## Pre-Flight Sequence (v13, always before run)
1. **Target 2** (`WRO_TARGET_SCAN_I2C`): dual-bus I2C scan — expected addresses on each bus
2. **Target 8** (`WRO_TARGET_TEST_ENCODERS`): spin wheels, both AS5600s accumulate ticks (one per bus)
3. **Target 9** (`WRO_TARGET_TEST_VL53L1X`): front + side VL53L1X come up at remapped addresses, return real distances
4. **Target 10** (`WRO_TARGET_BENCH_TEST`): IMU + encoders + VL53L1X + motor + servo + camera + E-Stop all live
5. **Target 11** (`WRO_TARGET_V13_MAIN`): wheels-up smoke test — heading-hold rotates servo as chassis rotates by hand
6. Test E-Stop physically — verify immediate stop and clean resume
7. Steering centered, motor direction correct
8. Wheels off ground first, then floor test

---

## Live Tuning over USB Serial (during run)
Single character + value, newline-terminated:
- `P0.45` / `I0.001` / `D0.30` — pillar PID gains
- `G1.20` — heading PID gain
- `S+` / `S-` — bump max speed ±5
- `?` — full state dump
- `!` — software E-Stop

---

## How Claude Should Help
When the user asks about this project, Claude should:
- Always reference actual file paths in `/Users/nr_ulan/Desktop/WRO_Project_Pack/`
- Treat **v13** as current; treat `src/esp32/legacy/legacy_*.cpp` and any v12 references in older docs as historical
- Read `wro_hw_config_v13.h`, `wro_config_v13.h`, `wro_v13_main.cpp` before answering hardware/code questions
- Use the v13 tunables above when discussing PID / corner / speed
- Remember the I2C topology: AS5600 Left + IMU + VL53L1X Front share Wire (I2C0); AS5600 Right + VL53L1X Side share Wire1 (I2C1)
- Remember that VL53L1X addresses are remapped at boot — pre-remap they're all at 0x29
- Follow the v13 pre-flight sequence (targets 2 → 8 → 9 → 10 → 11) for run preparation
- Suggest logging results to the CSV files in `docs/`
- Keep WRO rule compliance in mind (Rule 9.9 = compile-time mode, Rule 9.11 = single start button, Rule 11.10 = no wireless during runs)
- Cornering trigger / exit must stay distance-sensor + IMU based — do NOT regress to camera-gated triggers or encoder-distance exits
