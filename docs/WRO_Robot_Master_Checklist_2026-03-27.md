# WRO Future Engineers — Robot Master Checklist

Date: 9 April 2026
Team: ____________________
Robot Version: ____________________
Firmware Version: ____________________

---

## 1) Purpose of this file
Use this checklist before every test and race run to confirm the robot is safe, rule-compliant, and stable.

---

## 2) Compliance references

- Official rules: `WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.pdf`
- Rule mapping: `docs/WRO_Rule_Compliance_Matrix.md`
- Characteristics audit: `docs/WRO_Characteristics_Audit_2026-04-09.md`
- Main build/operation guide: `docs/WRO_Robot_Assembly_and_Startup_Guide.md`

---

## 3) Hardware Required (Minimum)

### Controller and logic
- ESP32 DevKitC V4
- TCA9548A I2C multiplexer
- ICM-20948 IMU
- AS5600 encoder x2
- OpenMV H7 Plus camera (or configured equivalent)

### Motion and power
- BTS7960 motor driver
- DC drive motor
- Steering servo JX PDI-6221MG
- Stable battery for motor + logic power strategy
- Main power switch
- Emergency stop button (physical, normally HIGH with pull-up in code)

### Wiring and mechanics
- Good quality wires + crimped connectors
- Common GND between ESP32, driver, sensors, camera
- Mounted sensors with vibration reduction
- Proper servo horn alignment at center position

---

## 4) Required Wiring Map (must match firmware)

### ESP32 pins
- I2C SDA: GPIO 21
- I2C SCL: GPIO 22
- Servo PWM: GPIO 18
- BTS7960 R_EN: GPIO 19
- BTS7960 L_EN: GPIO 23
- BTS7960 R_PWM: GPIO 5
- BTS7960 L_PWM: GPIO 14
- Camera UART RX: GPIO 16
- Camera UART TX: GPIO 17
- E-Stop input: GPIO 32
- Status LED: GPIO 2

### TCA9548A channel mapping
- Channel 0: ICM-20948 (expected 0x69)
- Channel 1: AS5600 left (0x36)
- Channel 2: AS5600 right (0x36)
- Channel 3-7: empty (unless intentionally used)

### Multiplexer base address
- TCA9548A: 0x70 (A0/A1/A2 to GND)

---

## 5) Software/Library Requirements

### Arduino IDE / PlatformIO
- ESP32 board package installed
- Correct board profile selected
- Serial monitor at 115200 baud

### Libraries
- Wire (built-in)
- ESP32Servo
- Adafruit ICM20948
- Adafruit Unified Sensor

### Code files
- scanerI2C.cpp (diagnostic scanner)
- eps323.cpp (main race logic)

### Key protocol file
- WRO_OpenMV_UART_Protocol.md (v3.0 with 6 fields + CRC)

---

## 6) Safety Requirements (non-negotiable)

| Requirement | Acceptance criteria | If FAIL |
|-------------|---------------------|---------|
| E-Stop works in all states | Pressing E-Stop forces SAFE_STOP; motor output = 0; steering = center | Stop testing; fix GPIO32 wiring and debounce path |
| Startup is safe | On boot, robot waits in init state with motor output = 0 | Block run; inspect startup state transitions |
| Camera-loss fail-safe | Warning after camera timeout (500 ms) and full safe stop if offline > 3000 ms | Block run; inspect UART and camera power wiring |
| Encoder-loss fail-safe | Persistent encoder loss triggers critical safe stop | Block run; inspect AS5600, magnets, CH1/CH2 routing |
| Wheels-off-ground first | First verification is done on stand | Do not continue to floor tests |

---

## 7) Pre-Flight Procedure (every run)

### Step A — Visual and electrical
- Check all connectors for looseness
- Check no exposed short points
- Verify battery voltage in safe range
- Verify common GND continuity

If FAIL:
- Fix cable/connector issues immediately
- Re-run Step A before proceeding

### Step B — Sensor bus check
1. Upload and run scanerI2C.cpp
2. Confirm:
   - TCA9548A detected at 0x70
   - CH0 has IMU at expected address
   - CH1 and CH2 have AS5600
   - No unexpected devices on empty channels
3. If mismatch appears, apply rollback sequence from Stage 6.7 in the main guide

Expected map:
- TCA9548A: 0x70
- CH0: 0x69 (ICM-20948)
- CH1: 0x36 (AS5600 left)
- CH2: 0x36 (AS5600 right)

### Step C — Main firmware check
1. Upload eps323.cpp
2. Open serial output
3. Confirm boot sequence:
   - IMU init OK
   - Gyro calibration completes
   - System ready message appears
4. Trigger E-Stop physically and verify immediate stop

If FAIL:
- No floor tests
- Fix firmware/serial errors first

### Step D — Actuator sanity
- Steering centered at startup
- Left/right steering direction correct
- Motor direction matches intended forward command
- No motor jitter when target speed is 0

If FAIL:
- Re-check servo center and BTS7960 polarity
- Repeat Step D until stable

---

## 8) Calibration Requirements

### IMU gyro
- Keep robot still during calibration
- Acceptance: `gyroZbias < 0.05`
- Acceptance: static yaw drift <= 2 deg in 30 seconds
- Repeat calibration if drift exceeds threshold

### Steering center
- Mechanical wheels straight at SERVO_CENTER
- Adjust horn physically first, code trim second
- Acceptance: robot tracks straight for 1 m with no steering trim changes

### Camera pipeline
- Ensure UART protocol is v3.0 with 6 fields + CRC:
   - `RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n`
- Validate runtime ranges:
   - RedX/GreenX: -160..160
   - RedDist/GreenDist: 0..999

### Odometry
- Run 3 x 100 cm straight tests
- Acceptance: error <= 2 cm on control run

### PID and heading
- Start from firmware defaults: P=0.55, I=0.002, D=0.18, GyroKp=1.20
- Tune Kp first, then Kd, then Ki
- Record each change in tuning log

---

## 9) Runtime Health Indicators to Watch
- Camera timeout warnings
- Encoder loss alarms
- Unexpected lap jumps
- E-Stop release/engage logs
- Oscillation in steering (too high Kp)
- Slow correction (too low Kp)

---

## 10) Track Testing Plan (must do in order)

Run tests in this sequence (see detailed criteria in `docs/WRO_Track_Test_Cases.md`):
1. TC-01 Stand Safety Test
2. TC-02 E-Stop Reaction
3. TC-03 Camera Timeout Stop
4. TC-04 Encoder Loss Handling
5. TC-05 Lap Counting Stability
6. TC-06 Recovery Behavior
7. TC-07 Start Procedure Compliance
8. TC-08 Finish Behavior Compliance

Record each run:
- Kp/Kd
- Battery voltage
- Floor condition
- Laps completed
- Failure mode if any

---

## 11) Common Failure Modes + Quick Fixes

### TCA/IMU not found
- Recheck channel mapping and address
- Recheck 3.3V/GND/SDA/SCL
- Reduce bus noise, shorten I2C wires

### Random camera loss
- Verify OpenMV UART baud and packet format
- Check RX/TX crossed correctly
- Improve cable fixation and grounding

### Encoder dropouts
- Check AS5600 supply and magnet alignment
- Check I2C pull-ups and wiring quality
- Confirm channel selection logic

### Car oscillates strongly
- Lower Kp
- Increase damping (Kd) slightly
- Reduce max speed in sharp turns

### Robot drifts in heading
- Recalibrate gyro bias
- Reduce vibration near IMU
- Validate dt stability and integration behavior

Mandatory stop conditions:
- Repeated SAFE_STOP with unknown cause
- Loss of deterministic startup state
- Any uncommanded motor movement

---

## 12) Competition-Day Go/No-Go Checklist

- One power switch only (WRO 9.10)
- One start button only, with waiting state before start (WRO 9.11)
- No data entry through physical mode switches/adjustments (WRO 9.9)
- Spare wires/connectors/sensors ready
- Backup flashed ESP32 ready
- Printed wiring map and quick checklist available
- Final tested firmware version tagged
- Battery fully charged and verified
- Tool kit ready (hex keys, screwdrivers, tape, zip ties)
- Last full pre-flight completed and signed

Go/No-Go decision:
- `GO` only if all critical checks are PASS
- `NO-GO` if any safety or rule check is FAIL

---

## 13) Sign-off
- Hardware lead: ____________________
- Software lead: ____________________
- Safety lead: ____________________
- Time: ____________________

---

## 14) Suggested Next Improvements
- Add automated preflight script that validates scanner output against expected map
- Add telemetry CSV export with timestamps for fault post-analysis
- Add explicit voltage sag alarm threshold in firmware and checklist
- Keep `WRO_Rule_Compliance_Matrix.md` and audit docs updated for each release
