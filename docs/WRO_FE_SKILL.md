---
name: wro-fe-team-faith
description: >
  Use this skill for ALL tasks related to Team Faith's WRO Future Engineers 2026 project.
  Trigger whenever the user mentions WRO, robot, ESP32, OpenMV, PID, servo, FSM, odometry,
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
**Active hardware:** v12 (ESP32-S3 + AS5048A SPI + TFMini-S UART, no I2C mux)

---

## Hardware Stack (v12 — current)

| Component | Model | Role |
|---|---|---|
| Main Controller | ESP32-S3-DevKitC-1 N8R8 | PID, Odometry, FSM, sensor fusion |
| Vision | OpenMV H7 Plus | Color blob tracking, obstacle detection |
| IMU | Adafruit ICM-20948 (9-DoF) | Heading, yaw, lap counting |
| Encoders | 2x AS5048A Magnetic (14-bit, SPI) | High-resolution odometry (1110 ticks/cm) |
| Front distance | TFMini-S (UART, 0.1–12 m) | Corner detection, pre-emptive braking |
| Side distance | TFMini-S (UART SW, optional) | Wall-follow for narrow Open Challenge corridors |
| Motor Driver | BTS7960 43A H-Bridge | Drive motor control |
| Steering | JX PDI-6221MG Digital Servo | Steering control |
| Power | 2S/3S LiPo + 5V Step-Down | Power supply |

> v11 hardware (ESP32 DevKitC V4 + AS5600 + VL53L1X + TCA9548A) is retired. See [`docs/WRO_Migration_v11_to_v12.md`](WRO_Migration_v11_to_v12.md).

### ESP32-S3 Pin Map
- I2C (IMU only): SDA=GPIO 8, SCL=GPIO 9
- SPI HSPI (2× AS5048A): MOSI=11 (tied 3V3), SCK=12, MISO=13, CS_L=10, CS_R=14
- UART1 (TFMini-S front): RX=15, TX=16
- UART2 (OpenMV camera): RX=17, TX=18
- TFMini-S side (optional): RX=47
- BTS7960 motor: R_EN=38, L_EN=39, R_PWM=40, L_PWM=41
- Steering servo: GPIO 42
- E-Stop: GPIO 21 (INPUT_PULLUP)
- Status LED: GPIO 2 / RGB WS2812: GPIO 48

### I2C Address Map
- ICM-20948 IMU: 0x68 (AD0 → GND) — **only device on bus** (no mux)

---

## Software Architecture

### Two independent threads:
1. **Vision (OpenMV, MicroPython):** `src/openmv/` — Detects Orange/Blue lines (lap direction), Red/Green pillars, magenta parking blocks, black walls. Sends UART v3 frame: `RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n` at ~50 Hz with XOR checksum. Spec: `docs/WRO_OpenMV_UART_Protocol.md`.
2. **Control (ESP32, C++):** `src/esp32/wro_v12_main.cpp` — modular, layered. Build target = `WRO_TARGET_V12_MAIN` (11) in `wro_build_target.h`.

### Layered Architecture (v12)
```
HAL          drivers (as5048a_spi.h, tfmini_s.h, ICM20948 lib)
Estimation   wro_imu / wro_odometry / wro_camera / wro_sensors
Behavior     wro_corner / wro_behavior_open / wro_behavior_obstacle / wro_park
Control      wro_v12_main (steering_mixer + speed_ramp)
FSM          wro_race_fsm (top) + wro_estop (parallel)
```

### FSM States
`INIT → WAIT_START → RUN_OPEN | RUN_OBSTACLE ↔ TURN_90 → PARKING* | FINISH`
plus `SAFE_STOP` (parallel — entered on E-Stop held / sensor fault, resumes on release).

### Key Algorithms (v12)
- **Cornering owned by TFMini front + IMU yaw delta** — camera is NEVER an exit condition (the v11 turn failure was a camera-gated trigger). Trigger: front<350 mm × 3 frames. Exit: 80° gyro delta. Speed during turn: PWM 70.
- **Pillar PID (Obstacle):** Red → keep right (setpoint +60 px), Green → keep left (-60 px). KP=0.45, KI=0.001, KD=0.30 (EMA-filtered derivative). Reset integral on color switch. Far-pillar pre-position at 0.4× gain.
- **Heading-hold (Open):** snap target heading to nearest 90° after each corner exit. KP=12 µs/deg, KD=2.0, KI=0 (drift over 3 min).
- **Lap counter:** primary = gyro 360° accumulator. Secondary sanity-check = camera line bit with cooldown.
- **Parallel parking (Obstacle, after lap 3):** APPROACH → ALIGN → 3-phase REVERSE → FINAL. Aborts to FINISH if camera silent.
- **E-Stop FSM:** press+release before race = ARM/START. Held during race = SAFE_STOP (motor off in <50 ms). Released = RESUME with PIDs reset. 500 ms boot grace.

---

## Mode Selection (Rule 9.9 compliant — compile-time only)
```cpp
// in src/esp32/wro_config_v12.h
#define OBSTACLE_MODE  0   // 0 = Open, 1 = Obstacle
```

## Key Tunables (in `src/esp32/wro_config_v12.h`)
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
```

Servo center/limits in `wro_hw_config_v12.h` are datasheet defaults — re-measure on the actual chassis with target 7 (TEST_SERVO_CAL) and add `SERVO_MARGIN_US=60` from each end-stop.

---

## Repository Structure (v12)
```
WRO_Project_Pack/
├── src/
│   ├── esp32/
│   │   ├── wro_v12_main.cpp     # active main firmware (target 11)
│   │   ├── wro_*.{h,cpp}         # modular layers
│   │   ├── wro_hw_config_v12.h   # pin map
│   │   ├── wro_config_v12.h      # tunables + OBSTACLE_MODE
│   │   ├── wro_build_target.h    # target selector
│   │   ├── as5048a_spi.h         # AS5048A SPI driver
│   │   ├── tfmini_s.h            # TFMini-S UART driver
│   │   ├── bench_test_v12.cpp    # full bench (target 10)
│   │   ├── test_encoders.cpp     # AS5048A test (target 8)
│   │   ├── test_tfmini.cpp       # TFMini test (target 9)
│   │   ├── scan_i2c_v12.cpp      # I2C scanner (target 2)
│   │   └── legacy_*.cpp          # v11 reference (inactive)
│   └── openmv/                   # MicroPython vision
├── schemes/                      # Wiring diagrams (v12)
├── models/, HSP94182_3D/         # CAD chassis files
├── docs/                         # Checklists, guides, logs
├── t-photos/, v-photos/, video/  # Required WRO assets
├── CHANGELOG.md
└── README.md
```

---

## Key Documents (in `docs/`)
- `WRO_Wiring_Map_v12.md` — full v12 pin reference (authoritative)
- `WRO_Migration_v11_to_v12.md` — what changed and why
- `WRO_OpenMV_UART_Protocol.md` — camera frame spec (v3)
- `WRO_Robot_Assembly_and_Startup_Guide.md` — physical build
- `WRO_Servo_Calibration_Guide.md` — servo tuning procedure
- `WRO_Quick_Race_Checklist.md` — competition-day quick check
- `WRO_Robot_Master_Checklist_2026-03-27.md` — full preflight (line refs may be v11; verify)
- `WRO_PID_Tuning_Log.csv` / `WRO_Maintenance_Log.csv` / `WRO_Test_Log.csv`
- `WRO_Track_Test_Cases.md` — structured test scenarios
- `WRO_Rule_Compliance_Matrix.md` — WRO rule compliance (line refs v11; needs re-audit)
- `WRO_Risk_Register.md` — risk assessment

---

## Common Failure Modes & Fixes (v12)
| Problem | Fix |
|---|---|
| I2C scan finds extra devices | v12 expects ONLY 0x68. Anything else = stale wiring. |
| AS5048A returns -1 | Check magnet gap (0.5–1.5 mm), CS pins, MOSI tied to 3.3 V |
| TFMini-S silent | **Power must be 5 V**, not 3.3 V. Check signal strength filter (>100). |
| Camera frame XOR fail | Check baud 115200, RX/TX crossed, common GND with ESP32. Monotonicity defense rejects sudden jumps. |
| Robot under/over-rotates corners | Tune `TURN_TARGET_DEG`, `TURN_SPEED_PWM`. NEVER use encoder ticks as exit. |
| Strong oscillation in straight | Lower `HEADING_KP`, increase `HEADING_KD`, reduce speed. |
| Heading drift over 3 min | ICM-20948 gyro Z drifts with motor heat. Snap-to-90° after each corner already mitigates. |

---

## Pre-Flight Sequence (v12, always before run)
1. **Target 2** (`WRO_TARGET_SCAN_I2C`): I2C scan — finds ONLY `0x68`
2. **Target 8** (`WRO_TARGET_TEST_ENCODERS`): spin wheels, both ticks accumulate
3. **Target 9** (`WRO_TARGET_TEST_TFMINI`): front sensor reports a sane distance
4. **Target 10** (`WRO_TARGET_BENCH_TEST`): IMU + encoders + TFMini + motor + servo + camera + E-Stop all live
5. **Target 11** (`WRO_TARGET_V12_MAIN`): wheels-up smoke test — heading-hold rotates servo as chassis rotates by hand
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
- Treat **v12** as current; treat `legacy_*.cpp` as reference only
- Read `wro_hw_config_v12.h`, `wro_config_v12.h`, `wro_v12_main.cpp` before answering hardware/code questions
- Use the v12 tunables above when discussing PID / corner / speed
- Follow the v12 pre-flight sequence (targets 2 → 8 → 9 → 10 → 11) for run preparation
- Suggest logging results to the CSV files in `docs/`
- Keep WRO rule compliance in mind (Rule 9.9 = compile-time mode, Rule 9.11 = single start button, Rule 11.10 = no wireless during runs)
- Cornering trigger / exit must stay TFMini + IMU based — do NOT regress to camera-gated triggers or encoder-distance exits
