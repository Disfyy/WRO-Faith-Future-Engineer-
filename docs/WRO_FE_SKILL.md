---
name: wro-fe-team-faith
description: >
  Use this skill for ALL tasks related to Team Faith's WRO Future Engineers 2026 project.
  Trigger whenever the user mentions WRO, robot, ESP32, OpenMV, PID, servo, FSM, odometry,
  obstacle challenge, open challenge, firmware, sensors, I2C, UART, camera, encoders, checklist,
  preflight, tuning, competition, engineering journal, or anything related to their autonomous car.
  This skill contains full project context so Claude can help without re-explanation every time.
---

# WRO Future Engineers 2026 — Team Faith Skill

## Project Overview
**Team:** Faith  
**Competition:** World Robot Olympiad (WRO) Future Engineers 2026  
**Category:** Self-Driving Cars (Open Challenge + Obstacle Challenge)  
**Repo location:** `/Users/nr_ulan/Desktop/WRO_Project_Pack/`

---

## Hardware Stack

| Component | Model | Role |
|---|---|---|
| Main Controller | ESP32 DevKitC V4 | PID, Odometry, FSM |
| Vision | OpenMV H7 Plus | Color blob tracking, obstacle detection |
| IMU | Adafruit ICM-20948 (9-DoF) | Heading, yaw, straight-line tracking |
| Encoders | 2x AS5600 Magnetic (12-bit) | Odometry, lap counting |
| I2C Mux | TCA9548A | Prevents I2C address conflicts |
| Motor Driver | BTS7960 43A H-Bridge | Drive motor control |
| Steering | JX PDI-6221MG Digital Servo | Steering control |
| Power | 2S/3S LiPo + 5V Step-Down | Power supply |

### ESP32 Pin Map
- I2C SDA: GPIO 21 | SCL: GPIO 22
- Servo PWM: GPIO 18
- BTS7960: R_EN=19, L_EN=23, R_PWM=5, L_PWM=14
- Camera UART: RX=16, TX=17
- E-Stop: GPIO 32
- Status LED: GPIO 2

### TCA9548A Channel Map
- CH0: ICM-20948 (0x69)
- CH1: AS5600 left (0x36)
- CH2: AS5600 right (0x36)
- TCA9548A address: 0x70

---

## Software Architecture

### Two independent threads:
1. **Vision (OpenMV, MicroPython):** `src/openmv/` — Detects Orange/Blue lines (direction) and Red/Green obstacles. Sends UART payload: `errorX,distance\n` at 115200 baud.
2. **Control (ESP32, C++):** `src/esp32/eps323.cpp` — Rigid FSM with 5 states.

### FSM States
`INIT` → `TRACKING` → `BLIND_TURN` → `SAFE_STOP` / `FINISH`

### Key Algorithms
- **Dynamic Offset PID:** Shifts setpoint ±35 units when obstacle within 60cm
- **Hardware E-Stop:** GPIO 32 interrupt → instant SAFE_STOP, resumes on release
- **Finish Zone Odometry:** AS5600 encoders track distance post-3 laps, stops inside start/finish bounds

---

## Config Values (from WRO_Config_Template.h)
```cpp
SERVO_CENTER = 90, MAX_RIGHT = 135, MAX_LEFT = 45
MOTOR_MAX_SPEED = 150, TURN_FAST = 120, TURN_SLOW = 80, MIN_SPEED = 35
PID_KP = 0.50f, PID_KD = 0.10f
CAMERA_TIMEOUT_MS = 500
LOOP_INTERVAL_MS = 10
TARGET_LAPS = 3
ESTOP_DEBOUNCE_MS = 20
```

---

## Repository Structure
```
WRO_Project_Pack/
├── src/
│   ├── esp32/        # C++ firmware (eps323.cpp)
│   └── openmv/       # MicroPython vision (main.py)
├── schemes/          # Wiring diagrams
├── models/           # 3D chassis files (STL/STEP)
├── docs/             # All documentation, logs, checklists
├── t-photos/         # Team photos
├── v-photos/         # Vehicle photos
├── video/            # Demo recordings
└── README.md
```

---

## Key Documents (in docs/)
- `WRO_Robot_Master_Checklist_2026-03-27.md` — Full preflight checklist
- `WRO_Quick_Race_Checklist.md` — Competition day quick check
- `WRO_PID_Tuning_Log.csv` — PID tuning history
- `WRO_Maintenance_Log.csv` — Hardware maintenance log
- `WRO_Test_Log.csv` — Test run results
- `WRO_Track_Test_Cases.md` — Structured test scenarios
- `WRO_Rule_Compliance_Matrix.md` — WRO rule compliance
- `WRO_Risk_Register.md` — Risk assessment
- `WRO_Servo_Calibration_Guide.md` — Servo tuning guide
- `WRO_OpenMV_UART_Protocol.md` — Camera protocol spec
- `WRO_Robot_Assembly_and_Startup_Guide.md` — Assembly guide

---

## Common Failure Modes & Fixes
| Problem | Fix |
|---|---|
| TCA/IMU not found | Recheck channel map, 3.3V/GND, shorten I2C wires |
| Camera loss | Verify UART baud 115200, check RX/TX crossed, fix grounding |
| Encoder dropout | Check AS5600 magnet alignment, I2C pull-ups |
| Strong oscillation | Lower Kp, increase Kd slightly, reduce speed in turns |
| Heading drift | Recalibrate gyro, reduce IMU vibration |

---

## Pre-Flight Steps (always before run)
1. Run `scanerI2C.cpp` → verify 0x70, CH0=IMU, CH1/CH2=encoders
2. Upload `eps323.cpp` → confirm boot: IMU OK, gyro calibration, system ready
3. Test E-Stop physically → verify immediate stop
4. Steering centered, motor direction correct
5. Wheels off ground first → then floor test

---

## How Claude Should Help
When the user asks about this project, Claude should:
- Always reference actual file paths in `/Users/nr_ulan/Desktop/WRO_Project_Pack/`
- Read relevant source files before answering code questions
- Use the config values above when discussing tuning
- Follow the preflight checklist structure for run preparation
- Suggest logging results to the CSV files in `docs/`
- Keep WRO rule compliance in mind (from `WRO_Rule_Compliance_Matrix.md`)
