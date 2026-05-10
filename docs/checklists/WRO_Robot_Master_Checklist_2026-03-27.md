# WRO Future Engineers — Robot Master Checklist

Date: 9 April 2026 (initial) | Updated to v13 hardware: 9 May 2026
Team: ____________________
Robot Version: ____________________
Firmware Version: ____________________

> **v13 hardware (May 2026):** ESP32-S3 + 2× AS5600 (dual native I2C) + 2× VL53L1X (XSHUT addr-remap) + ICM-20948 IMU + OpenMV H7 Plus + BTS7960 + JX servo. **No TCA9548A multiplexer** (the original v11 mux burned out). The address conflicts are resolved by splitting the AS5600s across `Wire`/`Wire1` and remapping VL53L1X addresses at boot. For race day, also use [`WRO_Quick_Race_Checklist.md`](WRO_Quick_Race_Checklist.md) and [`WRO_Preflight_Log.md`](WRO_Preflight_Log.md).

---

## 1) Purpose
Use this checklist before every test and race run to confirm the robot is safe, rule-compliant, and stable.

---

## 2) Compliance references

- Official rules: `docs/rules/WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.pdf`
- Rule mapping: `docs/strategy/WRO_Rule_Compliance_Matrix.md`
- Characteristics audit: `docs/strategy/WRO_Characteristics_Audit_2026-04-09.md`
- Main build / operation guide: `docs/guides/WRO_Robot_Assembly_and_Startup_Guide.md`

---

## 3) Hardware required (minimum)

### Controller and logic (v13)
- ESP32-S3-DevKitC-1 N8R8 (USB-C, native USB-OTG)
- ICM-20948 IMU (on I2C0 with AS5600 Left + VL53L1X Front)
- AS5600 magnetic encoder × 2 (one on each native I2C bus, fixed `0x36`)
- VL53L1X distance sensor × 2 (one on each I2C bus, default `0x29` → remapped at boot)
- OpenMV H7 Plus (UART2)

### Motion and power
- BTS7960 motor driver
- DC drive motor
- Steering servo JX PDI-6221MG
- LiPo 7.4 V + LM2596 step-down to 5 V (and optional 3.3 V)
- One main power switch (KCD3)
- E-Stop button (GPIO 21, INPUT_PULLUP, active LOW)

### Wiring and mechanics
- Quality wires, crimped connectors
- Common GND between ESP32-S3, BTS7960, sensors, camera (star ground)
- Sensors mounted with vibration reduction
- Servo horn aligned at the mechanical center

---

## 4) Required wiring map (must match firmware)

### ESP32-S3 pins (v13)
- **I2C0 SDA:** GPIO 8
- **I2C0 SCL:** GPIO 9
- **I2C1 SDA:** GPIO 11
- **I2C1 SCL:** GPIO 12
- **VL53L1X Front XSHUT:** GPIO 15
- **VL53L1X Side XSHUT:** GPIO 16
- **VL53L1X Third XSHUT (reserved):** GPIO 47
- **OpenMV camera RX:** GPIO 17
- **OpenMV camera TX:** GPIO 18
- **BTS7960 R_EN:** GPIO 38
- **BTS7960 L_EN:** GPIO 39
- **BTS7960 R_PWM:** GPIO 40 (LEDC Ch0 — forward)
- **BTS7960 L_PWM:** GPIO 41 (LEDC Ch1 — reverse)
- **Steering servo:** GPIO 42
- **E-Stop:** GPIO 21
- **Status LED:** GPIO 2
- **Onboard RGB LED:** GPIO 48

### I2C0 (Wire) device map
- ICM-20948 IMU at `0x68` (AD0 → GND)
- AS5600 Left at `0x36`
- VL53L1X Front: `0x29` at boot → `0x30` after remap

### I2C1 (Wire1) device map
- AS5600 Right at `0x36`
- VL53L1X Side: `0x29` at boot → `0x31` after remap

> **No `0x70`** — the TCA9548A is gone in v13.

---

## 5) Software / library requirements

### Arduino IDE
- ESP32 board package (Espressif v3.x)
- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Size: **8MB**, PSRAM: **OPI PSRAM**
- Serial Monitor at 115200 baud

### Libraries
- Wire (built-in)
- ESP32Servo
- Adafruit ICM20948
- Adafruit Unified Sensor
- **VL53L1X by Pololu** (new in v13)
- (AS5600 driver is inline in `as5600_dual_i2c.h` — no library install)

### Active code files (v13)
- `wro_v13_main.cpp` (main race firmware — target 11)
- `scan_i2c_v13.cpp` (dual-bus diagnostic scanner — target 2)
- `test_encoders.cpp` (AS5600 dual-I2C test — target 8)
- `test_vl53l1x.cpp` (VL53L1X with XSHUT remap — target 9)
- `bench_test_v13.cpp` (full bench — target 10)
- `src/esp32/legacy/legacy_eps323.cpp` and other `legacy_*.cpp` (v11 reference, archived, not active)

### Key protocol file
- `WRO_OpenMV_UART_Protocol.md` (v3.0 — 6 fields + XOR checksum)

---

## 6) Safety requirements (non-negotiable)

| Requirement | Acceptance criteria | If FAIL |
|-------------|---------------------|---------|
| E-Stop works in all states | Pressing E-Stop forces `RS_SAFE_STOP`; motor = 0; steering centered | Stop testing; fix GPIO 21 wiring and debounce path |
| Startup is safe | On boot, robot stays in `RS_INIT`/`RS_WAIT_START` with motor = 0 | Block run; inspect startup transitions |
| Camera-loss fail-safe | Warning after camera timeout (500 ms) and full SAFE_STOP if offline > 3000 ms | Block run; inspect UART2 + camera power |
| Encoder-loss fail-safe | Persistent encoder loss (50 consecutive errors) → SAFE_STOP | Block run; inspect AS5600, magnets, I2C wiring |
| Wheels-off-ground first | Initial verification on a stand | Do not move to floor tests |

---

## 7) Pre-flight procedure (every run)

### Step A — Visual and electrical
- Check connectors for looseness
- No exposed shorts
- Battery voltage in range (LiPo 7.0–8.4 V)
- Common GND continuity (multimeter: 0 Ω between every GND)

If FAIL → fix immediately and rerun Step A.

### Step B — Sensor bus check (v13)
1. Set `WRO_ACTIVE_TARGET = WRO_TARGET_SCAN_I2C` (target 2) → upload `scan_i2c_v13.cpp`.
2. Confirm:
   - **I2C0:** `0x68` + `0x36` + `0x29`
   - **I2C1:** `0x36` + `0x29`
   - **No `0x70`** — that would mean a TCA9548A is still on the bus (it shouldn't be in v13).
3. Run target 8 (`test_encoders.cpp`): both AS5600s sweep 0–4095, ticks accumulate.
4. Run target 9 (`test_vl53l1x.cpp`): both VL53L1X come up at remapped addresses (`0x30`, `0x31`) and report distances.
5. If anything mismatches, stop and verify wiring against [`docs/guides/WRO_Wiring_Map_v13.md`](../guides/WRO_Wiring_Map_v13.md).

### Step C — Main firmware check (v13)
1. Set `WRO_ACTIVE_TARGET = WRO_TARGET_V13_MAIN` (target 11) → upload `wro_v13_main.cpp`.
2. Open Serial Monitor at 115200.
3. Confirm boot sequence:
   - Banner: `WRO FE 2026 -- Team Faith -- v13.0 main firmware`
   - `Mode: OPEN` or `OBSTACLE` (matches `OBSTACLE_MODE` in `wro_config_v13.h`)
   - `WiFi: OFF, BT: OFF` (Rule 11.10)
   - `AS5600 dual I2C: OK`
   - `VL53L1X FRONT: OK at 0x30`
   - `VL53L1X SIDE: OK at 0x31`
   - `Camera UART2 ready`
   - `ICM-20948 IMU: OK`
   - `Calibrating gyro Z bias...` followed by bias value
   - `System ready. Press E-Stop to start.`
4. Press + release E-Stop. Verify the firmware accepts it as start.
5. While the race runs, hold E-Stop and verify motor stops in <50 ms.

If FAIL → no floor tests; fix the firmware/serial errors first.

### Step D — Actuator sanity
- Steering centered at startup
- Left/right steering direction correct
- Motor direction matches "forward" command
- No motor jitter when target speed = 0

If FAIL → re-check servo center calibration and BTS7960 polarity.

---

## 8) Calibration

### IMU gyro
- Robot still during calibration
- Acceptance: `gyroZbias < 0.05`
- Acceptance: static yaw drift ≤ 2° in 30 s

### Steering center
- Wheels mechanically straight at `SERVO_CENTER_US`
- Adjust horn physically first, then trim software constants
- Acceptance: tracks straight for 1 m without steering trim

### Camera pipeline
- UART v3.0 with 6 fields + XOR checksum
  - `RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n`
- Range checks:
  - RedX / GreenX: -160..160
  - RedDist / GreenDist: 0..999

### Odometry
- 3 × 100 cm straight runs
- Acceptance: ≤ 2 cm error on control run

### PID and heading
- Start from v13 defaults (in `wro_config_v13.h`):
  - `PILLAR_KP = 0.45`, `PILLAR_KI = 0.001`, `PILLAR_KD = 0.30`
  - `HEADING_KP = 12.0`, `HEADING_KI = 0`, `HEADING_KD = 2.0`
- Tune Kp first, then Kd, then Ki
- Record every change in `WRO_PID_Tuning_Log.csv`

---

## 9) Runtime health indicators
- Camera timeout warnings
- Encoder loss alarms
- Unexpected lap jumps
- E-Stop release/engage entries
- Oscillating steering (Kp too high)
- Sluggish correction (Kp too low)

---

## 10) Track testing plan (in order)

Run in this sequence (see [`docs/strategy/WRO_Track_Test_Cases.md`](../strategy/WRO_Track_Test_Cases.md)):
1. TC-01 Stand Safety
2. TC-02 E-Stop Reaction
3. TC-03 Camera Timeout Stop
4. TC-04 Encoder Loss Handling
5. TC-05 Lap Counting Stability
6. TC-06 Recovery Behavior
7. TC-07 Start Procedure Compliance
8. TC-08 Finish Behavior Compliance

Record per run: Kp/Kd, battery voltage, floor condition, laps completed, failure mode (if any).

---

## 11) Common failure modes + quick fixes

### Wrong devices on the I2C scan
- Confirm you ran `scan_i2c_v13.cpp` (target 2), not the legacy v11/v12 versions
- `0x70` showing up = stray TCA9548A still wired (shouldn't be in v13)
- Missing `0x29` after boot = VL53L1X XSHUT pin not driven LOW correctly

### IMU/encoder not found
- Verify `0x68` for IMU (AD0 must be GND in v13)
- Verify `0x36` on the correct bus (Left → I2C0, Right → I2C1)
- Check 3.3 V + GND + SDA + SCL with a multimeter
- Reduce bus noise; shorten I2C wires; verify 4.7 kΩ pull-ups present on each bus

### Random camera loss
- Verify OpenMV UART baud (115200) and packet format
- Confirm RX/TX crossed
- Improve cable strain relief and ground

### Encoder dropouts
- Check AS5600 supply (3.3 V) and magnet alignment (1-2 mm gap, centered)
- Check 4.7 kΩ pull-ups on the relevant I2C bus
- Run `test_encoders.cpp` (target 8) to inspect raw data live

### VL53L1X stuck at default `0x29`
- XSHUT pin floating or shorted — verify `vl53l1x_dual.h` boot order
- One sensor's XSHUT released before another's address was written — single-step the boot dance with debug prints

### Robot oscillates
- Lower `PILLAR_KP` / `HEADING_KP`
- Increase damping (`PILLAR_KD` / `HEADING_KD`)
- Reduce max speed (`OBS_MAX_PWM`, `OPEN_MAX_PWM`)

### Heading drift
- Recalibrate gyro on a still surface
- Reduce IMU vibration (foam mount)
- Verify the snap-to-90° works after each corner

Mandatory stop conditions:
- Repeated SAFE_STOP with unknown cause
- Loss of deterministic startup state
- Any uncommanded motor movement

---

## 12) Competition-day Go / No-Go

- One power switch only (Rule 9.10)
- One start button only with a waiting state (Rule 9.11) — same E-Stop button
- No physical mode switches (Rule 9.9) — `OBSTACLE_MODE` is compile-time
- Spare wires / connectors / sensors ready
- Backup flashed ESP32-S3 ready
- Printed wiring map and Quick Race Checklist on hand
- Final firmware version tagged in git
- Battery fully charged and verified
- Tool kit ready (hex keys, screwdrivers, tape, zip ties)
- Last full pre-flight completed and signed

Decision:
- `GO` only if every critical check is PASS
- `NO-GO` if any safety or rule check fails

---

## 13) Sign-off
- Hardware lead: ____________________
- Software lead: ____________________
- Safety lead: ____________________
- Time: ____________________

---

## 14) Suggested next improvements
- Automated preflight script that diffs scanner output against the expected map
- Telemetry CSV export with timestamps for fault post-analysis
- Explicit voltage-sag alarm threshold in firmware and checklist
- Keep `WRO_Rule_Compliance_Matrix.md` and audit docs updated each release
