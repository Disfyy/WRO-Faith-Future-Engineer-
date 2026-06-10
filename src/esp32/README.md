# `src/esp32/` — Control firmware (ESP32-S3)

This is the control-system source for Team Faith's WRO v13 robot.
Compiled by the Arduino IDE using `esp32.ino` as the sketch entry point and
**one** `.cpp` file at a time selected by the `WRO_ACTIVE_TARGET` macro in
[`wro_build_target.h`](wro_build_target.h).

## How the build-target system works

The Arduino IDE compiles every `.cpp` file in this folder together. Each
`.cpp` that defines `setup()` / `loop()` wraps its body in
`#if WRO_ACTIVE_TARGET == WRO_TARGET_*` so that only **one** target's body
compiles. To switch targets, edit one line in `wro_build_target.h`:

```cpp
#define WRO_ACTIVE_TARGET WRO_TARGET_V13_MAIN   // ← change this
```

> ⚠️ The Arduino IDE does **not** recurse into subfolders.
> Files under [`legacy/`](legacy/) are kept for reference only and don't
> link into the active build. Diagnostic targets live at the root of this
> folder with a `diag_` filename prefix so they sort together but still
> compile.

## Build targets

| Target | File | Kind | Purpose |
|---:|---|---|---|
|  1 | `legacy/legacy_eps323.cpp` | LEGACY v11 | Original v11 race firmware (archived) |
|  2 | [`diag_scan_i2c_v13.cpp`](diag_scan_i2c_v13.cpp) | Diagnostic | Scan both I2C buses, list every address found |
|  3 | [`target_test_motor_servo_drive.cpp`](target_test_motor_servo_drive.cpp) | Diagnostic | Drivetrain bring-up (v11 code via root shim → `legacy/`) |
|  4 | `legacy/legacy_test_no_sensors.cpp` | LEGACY v11 | Open-loop timed lap (archived) |
|  5 | `legacy/legacy_test_servo.cpp` | LEGACY v11 | Servo sweep (archived) |
|  6 | `legacy/legacy_test_short_sequence.cpp` | LEGACY v11 | Short pattern (archived) |
|  7 | [`diag_test_servo_calibrate_v13.cpp`](diag_test_servo_calibrate_v13.cpp) | Diagnostic | Interactive steering-servo µs calibration (center / end-stops) |
|  8 | [`diag_test_encoders.cpp`](diag_test_encoders.cpp) | Diagnostic | 2× AS5600 on dual I2C — tick accumulation |
|  9 | [`diag_test_vl53l1x.cpp`](diag_test_vl53l1x.cpp) | Diagnostic | 2× VL53L1X with XSHUT address remap |
| 10 | [`diag_bench_test_v13.cpp`](diag_bench_test_v13.cpp) | Diagnostic | Full hardware sanity (IMU + encoders + ToF + servo + motor + camera + E-Stop) |
| 11 | [`wro_v13_main.cpp`](wro_v13_main.cpp) | **PRODUCTION** | v13 race firmware (Open + Obstacle Challenge) **← active** |

Recommended preflight sequence: **2 → 8 → 9 → 10 → 11**. See
[`../../docs/checklists/WRO_Robot_Master_Checklist_2026-03-27.md`](../../docs/checklists/WRO_Robot_Master_Checklist_2026-03-27.md)
for the step-by-step procedure.

## File layout (non-diagnostic, non-legacy)

The production firmware (`wro_v13_main.cpp`) is built on a layered architecture
spread across these files:

| Layer | Files |
|---|---|
| **HAL / drivers** | `as5600_dual_i2c.h`, `vl53l1x_dual.h`, `wro_hw_config_v13.h` |
| **Sensor wrappers** | `wro_imu.{cpp,h}`, `wro_sensors.{cpp,h}`, `wro_camera.{cpp,h}` |
| **Estimation** | `wro_odometry.{cpp,h}` |
| **Control** | `wro_pid.h`, `wro_corner.{cpp,h}`, `wro_park.{cpp,h}` |
| **Behavior** | `wro_behavior_open.{cpp,h}`, `wro_behavior_obstacle.{cpp,h}` |
| **Top-level FSM** | `wro_race_fsm.{cpp,h}`, `wro_estop.{cpp,h}` |
| **Telemetry** | `wro_telemetry.{cpp,h}` |
| **Tunables** | `wro_config_v13.h` ← edit here for PID gains, speeds, mode select |
| **Build switch** | `wro_build_target.h` ← edit here to change which target compiles |

## Required Arduino libraries

Install via Arduino IDE Library Manager:

- `Adafruit ICM20948` (and its `Adafruit BusIO` + `Adafruit Unified Sensor` deps)
- `ESP32Servo`
- `VL53L1X` by **Pololu** (not the ST/Adafruit forks — the API differs)

Board: **ESP32-S3 Dev Module** (Arduino-ESP32 core **v3.x** required — the
firmware uses the IDF 5 task-watchdog API (`esp_task_wdt_config_t` /
`esp_task_wdt_reconfigure`) and `ledcAttach`, which do not exist in 2.x).

## See also

- Wiring reference: [`../../docs/guides/WRO_Wiring_Map_v13.md`](../../docs/guides/WRO_Wiring_Map_v13.md)
- Assembly & first boot: [`../../docs/guides/WRO_Robot_Assembly_and_Startup_Guide.md`](../../docs/guides/WRO_Robot_Assembly_and_Startup_Guide.md)
- Telemetry CSV schema: [`wro_telemetry.cpp`](wro_telemetry.cpp) — emitted at 5 Hz from `wro_v13_main.cpp`
- Migration story (v11 → v12-planned → v13-actual): [`../../docs/strategy/WRO_Migration_v12_to_v13.md`](../../docs/strategy/WRO_Migration_v12_to_v13.md)
