# WRO Future Engineers 2026 - Team Faith 🚀

[![C++](https://img.shields.io/badge/C++-ESP32--S3-blue.svg)](https://isocpp.org/)
[![MicroPython](https://img.shields.io/badge/MicroPython-OpenMV-yellow.svg)](https://micropython.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

Official repository for **Team Faith**, participating in the World Robot Olympiad (WRO) Future Engineers category. This repository contains all source code (Vision and Control), electromechanical schemes, and project documentation for our autonomous vehicle.

---

## 🏎️ Main Hardware Components (v13)
Our vehicle is built on a custom-designed reliable chassis equipped with a powerful processing stack to ensure optimal performance in both Open Challenge and Obstacle Challenge.

- **Main Controller:** ESP32-S3-DevKitC-1 N8R8 (PID, Odometry, FSM)
- **Computer Vision:** OpenMV H7 Plus (color blob tracking + obstacle distance)
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

---

## 📂 Repository Structure

```text
├── models/HSP94182_3D     # 3D chassis CAD (STL / STEP / SCAD / Python + viewer.html)
├── schemes                # Electromechanical diagrams (Mermaid + rendered PNG/SVG)
├── src
│   ├── esp32              # ESP32-S3 control firmware (C++) — v13 modular layers
│   │   └── legacy         # v11 archived sources (legacy_*.cpp + README)
│   └── openmv             # MicroPython machine-vision scripts
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
2. Connect OpenMV to the IDE, run Threshold Editor to configure your field lighting, and save `openmv_main.py` directly to the camera flash as `main.py`.
3. In Arduino IDE, install `Adafruit ICM20948`, `ESP32Servo`, and `VL53L1X` (by Pololu). Open the `src/esp32/` folder as a sketch, set `WRO_ACTIVE_TARGET` in `wro_build_target.h` to `WRO_TARGET_V13_MAIN` (target 11) and flash. Entry point is `src/esp32/wro_v13_main.cpp`.
4. Select challenge mode at compile time: `OBSTACLE_MODE 0` (Open) or `1` (Obstacle) in `src/esp32/wro_config_v13.h` (Rule 9.9: no physical mode switches). Place on the track, press the E-Stop button to start.

> ℹ️ The hardware was on a v12 plan (ESP32-S3 + AS5048A SPI + TFMini-S UART) but the new sensors didn't arrive in time and the v11 TCA9548A mux burned out. **v13 is the actual fielded build:** new ESP32-S3 main controller + original v11 sensors (AS5600 + VL53L1X), with the address conflicts resolved via dual native I2C peripherals and runtime XSHUT-based VL53L1X address remapping. See [`CHANGELOG.md`](CHANGELOG.md) and [`docs/strategy/WRO_Migration_v12_to_v13.md`](docs/strategy/WRO_Migration_v12_to_v13.md) for the full story.

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
