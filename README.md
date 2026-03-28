# WRO Future Engineers 2026 - Team Faith 🚀

[![C++](https://img.shields.io/badge/C++-ESP32-blue.svg)](https://isocpp.org/)
[![MicroPython](https://img.shields.io/badge/MicroPython-OpenMV-yellow.svg)](https://micropython.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

Official repository for **Team Faith**, participating in the World Robot Olympiad (WRO) Future Engineers category. This repository contains all source code (Vision and Control), electromechanical schemes, and project documentation for our autonomous vehicle.

---

## 🏎️ Main Hardware Components
Our vehicle is built on a custom-designed reliable chassis equipped with a powerful processing stack to ensure optimal performance in both Open Challenge and Obstacle Challenge.

- **Main Controller:** ESP32 DevKitC V4 (Handles PID, Odometry, and FSM)
- **Computer Vision:** OpenMV H7 Plus (Color Blob tracking and obstacle spatial calculation)
- **IMU:** Adafruit ICM-20948 (9-DoF for precise layout tracking on straight sections)
- **Encoders:** 2x AS5600 Magnetic Encoders (12-bit precision for odometry)
- **I2C Multiplexer:** TCA9548A (Prevents I2C address conflicts)
- **Motor Driver:** BTS7960 (43A High Power H-Bridge)
- **Steering:** JX PDI-6221MG Digital Servo 
- **Power:** 2S/3S LiPo + 5V Step-Down Converters

## 🧠 Software Architecture

Our software is divided into two continuous independent threads:
1. **Vision System (`src/openmv/`):** A MicroPython script running on the OpenMV camera. It scans for 4 target colors (Orange/Blue for directions, Red/Green for obstacles) and black walls. It calculates dynamic tracking errors and distances, transmitting payloads via UART at `115200` baud.
2. **Control System (`src/esp32/`):** A rigid Finite State Machine (FSM) written in C++ running on the ESP32.

### Algorithms Implemented
*   **Finite State Machine (FSM):** Five distinct operational states `INIT`, `TRACKING`, `BLIND_TURN`, `SAFE_STOP`, `FINISH`.
*   **Dynamic Offset PID:** Instead of harsh steering angles to avoid obstacles, our PID dynamic offset dynamically shifts the setpoint by `±35` units when an obstacle is within 60 cm. 
*   **Hardware E-Stop Check:** A physical switch connected to an interrupt loop instantly invokes `STATE_SAFE_STOP`. When released, it safely resumes `STATE_TRACKING`.
*   **Finish Zone Odometry:** Utilizing AS5600 encoders to calculate traversed distances inside the finish zone, ensuring the robot performs a full structural stop post-3 laps precisely inside the start/finish bounds.

---

## 📂 Repository Structure

According to WRO engineering requirements, the repository is structured as follows:

```text
├── models             # 3D chassis design files (STL/STEP) *To be added*
├── schemes            # Electromechanical diagrams (Wiring Map)
├── src                
│   ├── esp32          # Core ESP32 Control firmware (C++)
│   └── openmv         # Machine Vision scripts for OpenMV (MicroPython)
├── t-photos           # Team presentation photos *To be added*
├── v-photos           # Vehicle engineering photos *To be added*
├── video              # Demonstration recordings *To be added*
├── docs               # Checklists, Logs, Setup Guides generated during R&D
└── README.md          # You are here
```

## ⚙️ How to Setup

1. Assemble and wire the chassis according to the map in `schemes/WRO_Wiring_Map.md`.
2. Connect OpenMV to the IDE, run Threshold Editor to configure your field lighting, and save `openmv_main.py` directly to the camera flash as `main.py`.
3. In Arduino IDE, install `Adafruit ICM20948` and `ESP32Servo`. Compile and flash `src/esp32/eps323.cpp` into your ESP32.
4. Set the physical switch to `OPEN CHALLENGE` or `OBSTACLE CHALLENGE`, place on the track, and release the E-STOP.

---

> *"Engineering is the closest thing to magic that exists in the world."* 
> 
> **— Team Faith**
