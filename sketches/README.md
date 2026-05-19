# `sketches/` — Standalone Arduino sketches

These are self-contained Arduino IDE sketches (one `.ino` per folder) for
**bench bring-up** and **subsystem testing**. Each one flashes to the
ESP32-S3 without the rest of the v13 codebase — useful when you want to
verify a single subsystem in isolation, or hand a tester to someone who
only has the Arduino IDE.

## How this differs from `src/esp32/`

The production firmware lives in [`../src/esp32/`](../src/esp32/) and uses a
**build-target macro system** (one sketch, switch which target compiles
via `WRO_ACTIVE_TARGET`). That's the right path for race firmware and
preflight diagnostics, but it requires opening the multi-file sketch.

The sketches in this folder are the **standalone alternative**: each is
one `.ino` with everything inlined. Open it, flash it, watch Serial.

## Sketches

| Folder | Sketch | Purpose |
|---|---|---|
| [`bench_test/`](bench_test/) | `bench_test.ino` | Full robot bench check — IMU, both AS5600, both VL53L1X (with XSHUT remap), steering servo, BTS7960 motor, E-Stop, OpenMV UART. Serial commands: `s` status, `live` toggle, `sl/sc/sr` servo, `f/b/fast/stop` motor. **Lift wheels before flashing.** |
| [`servo_calibrate/`](servo_calibrate/) | `servo_calibrate.ino` | Sweep the JX PDI-6221MG digital servo over a microsecond range to find left/center/right values. Use this to determine the `SERVO_*_US` constants in [`../src/esp32/wro_config_v13.h`](../src/esp32/wro_config_v13.h). Procedure: [`../docs/guides/WRO_Servo_Calibration_Guide.md`](../docs/guides/WRO_Servo_Calibration_Guide.md). |
| [`test_encoders/`](test_encoders/) | `test_encoders.ino` | Print live tick counts from both AS5600 encoders on dual I2C. Spin a wheel by hand; values should change smoothly. 1 revolution ≈ 4096 ticks; 1 cm ≈ 277 ticks (47 mm wheel). |

## Required libraries

Same as the main firmware:
- `Adafruit ICM20948` (+ `Adafruit BusIO` + `Adafruit Unified Sensor`)
- `ESP32Servo`
- `VL53L1X` by **Pololu**

Board: **ESP32-S3 Dev Module**. Serial Monitor: **115200 baud**.

## When to use a sketch vs. a `src/esp32/diag_*.cpp` target

| Situation | Use |
|---|---|
| Race-day preflight, you already have the main sketch open | `src/esp32/diag_*.cpp` (set `WRO_ACTIVE_TARGET`) |
| Handing a hardware test to a teammate with only Arduino IDE | This folder |
| Calibrating servo center values | `sketches/servo_calibrate/` (the diag flow doesn't have an equivalent — yet) |
| First-time bring-up after re-soldering | Either; pick the one you already have flashed |
