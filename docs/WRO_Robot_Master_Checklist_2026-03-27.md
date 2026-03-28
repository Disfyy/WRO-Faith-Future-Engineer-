# WRO Future Engineers — Robot Master Checklist

Date: 27 March 2026
Team: ____________________
Robot Version: ____________________
Firmware Version: ____________________

---

## 1) Goal of this file
Use this checklist before every test and race run to make sure the robot is safe, stable, and ready.

---

## 2) Hardware Required (Minimum)

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

## 3) Required Wiring Map (must match firmware)

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

## 4) Software/Library Requirements

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

---

## 5) Safety Requirements (non-negotiable)
- Emergency stop works instantly in all states
- On E-Stop: motor speed goes to 0 and steering centers
- Camera timeout triggers safe stop
- Persistent encoder failure triggers safe stop
- Robot starts with motor output = 0
- Test with wheels lifted before floor tests

---

## 6) Pre-Flight Procedure (every run)

### Step A — Visual and electrical
- Check all connectors for looseness
- Check no exposed short points
- Verify battery voltage in safe range
- Verify common GND continuity

### Step B — Sensor bus check
1. Upload and run scanerI2C.cpp
2. Confirm:
   - TCA9548A detected at 0x70
   - CH0 has IMU at expected address
   - CH1 and CH2 have AS5600
   - No unexpected devices on empty channels
3. Fix wiring before continuing if mismatch appears

### Step C — Main firmware check
1. Upload eps323.cpp
2. Open serial output
3. Confirm boot sequence:
   - IMU init OK
   - Gyro calibration completes
   - System ready message appears
4. Trigger E-Stop physically and verify immediate stop

### Step D — Actuator sanity
- Steering centered at startup
- Left/right steering direction correct
- Motor direction matches intended forward command
- No motor jitter when target speed is 0

---

## 7) Calibration Requirements

### IMU gyro
- Keep robot still during calibration
- Repeat calibration if large yaw drift appears while stationary

### Steering center
- Mechanical wheels straight at SERVO_CENTER
- Adjust horn physically first, code trim second

### Camera pipeline
- Ensure UART message format is exactly: error,distance\n
- Validate valid ranges in runtime:
  - errorX: -160 to 160
  - distance: 0 to 10000

### PID tuning
- Start with conservative values
- Tune Kp first, then Kd
- Use fixed test track and record results

---

## 8) Runtime Health Indicators to Watch
- Camera timeout warnings
- Encoder loss alarms
- Unexpected lap jumps
- E-Stop release/engage logs
- Oscillation in steering (too high Kp)
- Slow correction (too low Kp)

---

## 9) Track Testing Plan (must do in order)
1. Static test on stand (wheels off ground)
2. Straight-line short run
3. Controlled turns at low speed
4. Full lap at reduced speed
5. Race speed trials with repeated runs
6. Stress test with quick re-start and E-Stop events

Record each run:
- Kp/Kd
- Battery voltage
- Floor condition
- Laps completed
- Failure mode if any

---

## 10) Common Failure Modes + Quick Fixes

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

---

## 11) Competition-Day Checklist
- Spare wires/connectors/sensors ready
- Backup flashed ESP32 ready
- Printed wiring map available
- Final tested firmware version tagged
- Battery fully charged and verified
- Tool kit ready (hex keys, screwdrivers, tape, zip ties)
- Last full pre-flight completed and signed

Sign-off:
- Hardware lead: ____________________
- Software lead: ____________________
- Safety lead: ____________________
- Time: ____________________

---

## 12) Suggested Next Improvements
- Add formal finite-state machine: INIT, READY, RUN, SAFE_STOP, FINISH
- Add acceleration ramp limiter for smoother traction
- Add telemetry CSV output for data-driven tuning
- Add watchdog/reset strategy for long runs
- Add config header with all tunables in one place
