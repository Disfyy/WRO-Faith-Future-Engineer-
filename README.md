# WRO Future Engineers 2026 - Team Faith 🚀

[![C++](https://img.shields.io/badge/C++-ESP32--S3-blue.svg)](https://isocpp.org/)
[![MicroPython](https://img.shields.io/badge/MicroPython-OpenMV-yellow.svg)](https://micropython.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

Official repository for **Team Faith**, participating in the World Robot Olympiad (WRO) Future Engineers category. This repository contains all source code (Vision and Control), electromechanical schemes, and project documentation for our autonomous vehicle.

---

## 👋 For first-time viewers / WRO judges

If you've just landed on this repo, start here:

| What you want | Where to look |
|---|---|
| **What this robot does** | [Hardware](#-main-hardware-components-v13) and [Software Architecture](#-software-architecture) below |
| **Active firmware (the file we actually flash)** | [`src/esp32/wro_v13_main.cpp`](src/esp32/wro_v13_main.cpp) — build target 11 |
| **How we test the robot** | [Testing & Calibration Workflow](#-testing--calibration-workflow) below |
| **Source code map** | [`src/esp32/README.md`](src/esp32/README.md) (control firmware) · [`src/openmv/README.md`](src/openmv/README.md) (original vision backend) · [`src/espcam/README.md`](src/espcam/README.md) (active Pixy2 vision backend) |
| **Engineering journal** | [`docs/`](docs/) — checklists, guides, strategy, run logs, rules |
| **Engineering writeups (WRO rubric)** | [`other/`](other/) — mobility / power-and-sense / obstacle management |
| **3D-printable parts** | [`models/HSP94182_3D/`](models/HSP94182_3D/) |
| **Wiring & system diagrams** | [`schemes/`](schemes/) |
| **Photos & video** | [`v-photos/`](v-photos/), [`t-photos/`](t-photos/), [`video/`](video/) |

> 📝 The robot is in **bench calibration** as of this writing — race-day logs in [`docs/logs/`](docs/logs/) are still template-only. See [`CHANGELOG.md`](CHANGELOG.md) for status.

---

## 🏎️ Main Hardware Components (v13)
Our vehicle is built on a custom-designed reliable chassis equipped with a powerful processing stack to ensure optimal performance in both Open Challenge and Obstacle Challenge.

- **Main Controller:** ESP32-S3-DevKitC-1 N8R8 (PID, Odometry, FSM)
- **Computer Vision:** Pixy2/2.1 (active backend — color blob tracking + obstacle distance). OpenMV H7 Plus is the original/fallback backend; its OV5640 sensor failed 5 days before competition, see `wro_config_v13.h` §12.
- **IMU:** Adafruit ICM-20948 (9-DoF, on I2C0)
- **Encoders:** 2× AS5600 magnetic encoders (12-bit, dual I2C — one per bus, no mux)
- **Distance:** 2× VL53L1X ToF sensors (XSHUT-based runtime address remap)
- **Motor Driver:** BTS7960 (43A H-Bridge)
- **Steering:** JX PDI-6221MG digital servo
- **Power:** 2S/3S LiPo + 5V step-down

> **No I2C multiplexer.** The original v11 build used a TCA9548A to share one I2C bus across two AS5600s and two VL53L1Xs; that mux burned out. v13 instead uses both native I2C peripherals on the ESP32-S3 (one AS5600 per bus) and runtime XSHUT-based address remapping for the VL53L1X pair.

## 🧠 Software Architecture

Our software is divided into two continuous independent threads:
1. **Vision System (`src/openmv/`):** A MicroPython script running on the OpenMV camera. It scans for 4 target colors (Orange/Blue for directions, Red/Green for obstacles) and black walls. It calculates dynamic tracking errors and distances, transmitting payloads via UART at `115200` baud.
2. **Control System (`src/esp32/`):** A layered architecture (HAL → estimation → behavior → control → top FSM) running on the ESP32-S3. Active firmware: `wro_v13_main.cpp` (build target 11).

### Algorithms Implemented
*   **Finite State Machine (FSM):** `INIT → WAIT_START → RUN_OPEN | RUN_OBS → (corner FSM, parking FSM) → FINISH`, with `SAFE_STOP` available from any running state.
*   **Cornering FSM:** Owned by front-distance + IMU yaw delta — the camera is never an exit condition. Encoder ticks are telemetry-only during the turn.
*   **Dynamic Offset PID:** Setpoint shifts ±60 px when an active pillar is in view; far pillar provides a 0.4× pre-position term when both colors are visible.
*   **Hardware E-Stop:** Press+release arms the start; held during the race triggers `SAFE_STOP` and resumes on release.
*   **Finish Zone Odometry:** Dual AS5600 encoders track post-3-laps distance to land the robot in the finish band.
*   **Pixy2 Camera Backend:** Binary block-protocol driver (`wro_camera_pixy2.cpp`) feeding the same `g_cam` struct as the OpenMV backend, so FSM/failsafes/telemetry stay backend-agnostic. See [`docs/guides/WRO_Pixy2_Setup.md`](docs/guides/WRO_Pixy2_Setup.md).
*   **Steering Asymmetry Trim:** Live-tunable per-side steering gain compensation (`wro_steering_comp.h`) to correct chassis bias.
*   **Encoder Health Diagnostics:** Magnet-presence and connection-stability checks (target 8) run before odometry is trusted.
*   **Wi-Fi Telemetry Mirror (testing only):** Optional softAP + UDP broadcast of telemetry for bench debugging (`wro_telemetry_wifi.cpp`), gated off by default — disabled for competition per Rule 11.10.

---

## 📂 Repository Structure

```text
├── models/HSP94182_3D     # 3D chassis CAD (STL / STEP / SCAD / Python + viewer.html)
├── schemes                # Electromechanical diagrams (Mermaid + rendered PNG/SVG)
├── src
│   ├── esp32              # ESP32-S3 control firmware (C++) — v13 modular layers
│   │   ├── wro_v13_main.cpp        # Production firmware (target 11) ← active
│   │   ├── diag_*.cpp              # Diagnostic targets (2, 8, 9, 10, 12)
│   │   ├── wro_*.{cpp,h}           # Layered HAL / estimation / behavior / FSM
│   │   ├── as5600_dual_i2c.h       # AS5600 driver (dual I2C, no mux)
│   │   ├── vl53l1x_dual.h          # VL53L1X driver (XSHUT addr remap)
│   │   ├── wro_build_target.h      # Edit one line to switch target
│   │   └── legacy/                 # v11 archived sources (legacy_*.cpp + README)
│   ├── openmv              # MicroPython machine-vision scripts (original/fallback backend)
│   └── espcam              # ESP32-CAM + Pixy2 vision sketch (active backend, see espcam/README.md)
├── sketches               # Standalone Arduino sketches (bench tests, servo cal)
├── other                  # Engineering writeups by WRO topic
│   ├── mobility-management              # Drivetrain, steering, chassis, control loops
│   ├── power-and-sense-management       # Battery, power dist., sensor stack
│   └── obstacle-management              # Obstacle Challenge strategy + parking
├── docs                   # Project documentation
│   ├── INDEX.md           # Single-page navigation hub
│   ├── guides             # Wiring map, assembly, servo cal, UART protocol
│   ├── checklists         # Race-day preflight & log
│   ├── strategy           # Migration, rule compliance, audit, risk, templates
│   ├── logs               # PID / maintenance / test CSV logs
│   └── rules              # Official WRO 2026 rules (reference copies)
├── t-photos               # Team presentation photos
├── v-photos               # Vehicle engineering photos
├── video                  # Demonstration recordings
├── CHANGELOG.md           # Versioned engineering history
└── README.md              # You are here
```

## ⚙️ How to Setup

1. Assemble and wire the chassis according to [`docs/guides/WRO_Wiring_Map_v13.md`](docs/guides/WRO_Wiring_Map_v13.md).
2. Camera (active backend, Pixy2): teach signatures in PixyMon per [`docs/guides/WRO_Pixy2_Setup.md`](docs/guides/WRO_Pixy2_Setup.md) and set "Data out port" to UART @ 115200 baud. *(Fallback backend, OpenMV: connect to the IDE, run Threshold Editor to configure field lighting, and save `openmv_main.py` directly to the camera flash as `main.py`.)*
3. In Arduino IDE, install `Adafruit ICM20948`, `ESP32Servo`, and `VL53L1X` (by Pololu). Open the `src/esp32/` folder as a sketch, set `WRO_ACTIVE_TARGET` in `wro_build_target.h` to `WRO_TARGET_V13_MAIN` (target 11) and flash. Entry point is `src/esp32/wro_v13_main.cpp`.
4. Select challenge mode at compile time: `OBSTACLE_MODE 0` (Open) or `1` (Obstacle) in `src/esp32/wro_config_v13.h` (Rule 9.9: no physical mode switches). Place on the track, press the E-Stop button to start.

> ℹ️ The hardware was on a v12 plan (ESP32-S3 + AS5048A SPI + TFMini-S UART) but the new sensors didn't arrive in time and the v11 TCA9548A mux burned out. **v13 is the actual fielded build:** new ESP32-S3 main controller + original v11 sensors (AS5600 + VL53L1X), with the address conflicts resolved via dual native I2C peripherals and runtime XSHUT-based VL53L1X address remapping. See [`CHANGELOG.md`](CHANGELOG.md) and [`docs/strategy/WRO_Migration_v12_to_v13.md`](docs/strategy/WRO_Migration_v12_to_v13.md) for the full story.

---

## 🧪 Testing & Calibration Workflow

We test the robot in **two layers** before any race-pace driving:

### Layer 1 — Subsystem diagnostics

Standalone programs that verify one piece of hardware at a time. Run these in order before every test session:

| Order | What we test | How to run |
|---:|---|---|
| 1 | Both I2C buses + every address | Set target 2 ([`src/esp32/diag_scan_i2c_v13.cpp`](src/esp32/diag_scan_i2c_v13.cpp)) |
| 2 | Both AS5600 encoders: connection, magnet health, stability, odometry (keys `s`/`m`/`z`/`h`) | Set target 8 ([`src/esp32/diag_test_encoders.cpp`](src/esp32/diag_test_encoders.cpp)) |
| 3 | Both VL53L1X ToF after XSHUT remap | Set target 9 ([`src/esp32/diag_test_vl53l1x.cpp`](src/esp32/diag_test_vl53l1x.cpp)) |
| 4 | Full bench (IMU + servo + motor + camera + E-Stop) | Set target 10 ([`src/esp32/diag_bench_test_v13.cpp`](src/esp32/diag_bench_test_v13.cpp)) |
| 5 | Camera link check (active backend per `CAMERA_BACKEND`) | Set target 12 ([`src/esp32/diag_test_camera.cpp`](src/esp32/diag_test_camera.cpp)) |

Switch targets by editing one line in [`src/esp32/wro_build_target.h`](src/esp32/wro_build_target.h). Standalone `.ino` versions for the Arduino IDE live in [`sketches/`](sketches/).

### Layer 2 — Production firmware

When all 5 diagnostics pass, switch to target 11 ([`src/esp32/wro_v13_main.cpp`](src/esp32/wro_v13_main.cpp)) — the actual race firmware. It logs CSV-formatted telemetry over USB serial at 5 Hz.

### Logging

Every test-drive session is logged into [`docs/logs/`](docs/logs/):

- `WRO_Test_Log.csv` — preflight + run results (one row per session)
- `WRO_PID_Tuning_Log.csv` — controller gain experiments
- `WRO_Maintenance_Log.csv` — hardware changes

> Logs are currently **template-only** because the v13 chassis is still in bench calibration. They'll be populated as we move into floor and track testing.

### Checklists & guides

- Race-day quick check: [`docs/checklists/WRO_Quick_Race_Checklist.md`](docs/checklists/WRO_Quick_Race_Checklist.md)
- Full preflight log template: [`docs/checklists/WRO_Preflight_Log.md`](docs/checklists/WRO_Preflight_Log.md)
- Master checklist (every subsystem): [`docs/checklists/WRO_Robot_Master_Checklist_2026-03-27.md`](docs/checklists/WRO_Robot_Master_Checklist_2026-03-27.md)
- Step-by-step migration verification: [`docs/strategy/WRO_Migration_v12_to_v13.md`](docs/strategy/WRO_Migration_v12_to_v13.md)

---

## 📚 Reading Order (for judges and new readers)

1. **This README** — you are here.
2. **Engineering writeups by topic** — three short docs, one per WRO rubric area:
   - [`other/mobility-management/`](other/mobility-management/) — drivetrain, steering, chassis, control loops
   - [`other/power-and-sense-management/`](other/power-and-sense-management/) — battery, power, sensors
   - [`other/obstacle-management/`](other/obstacle-management/) — Obstacle Challenge strategy + parking
3. **Reference docs** — the [`docs/INDEX.md`](docs/INDEX.md) lists every document grouped by use-case (guides, checklists, strategy, logs, rules).
4. **Source** — vision in [`src/openmv/`](src/openmv/), control in [`src/esp32/`](src/esp32/) (entry point [`wro_v13_main.cpp`](src/esp32/wro_v13_main.cpp), tunables in [`wro_config_v13.h`](src/esp32/wro_config_v13.h)).
5. **Engineering history** — [`CHANGELOG.md`](CHANGELOG.md) tells the v11 → v12-plan → v13-actual story chronologically.

---

> *"Engineering is the closest thing to magic that exists in the world."* 
> 
> **— Team Faith**
