# Legacy v11 firmware (archived)

These files are the **v11 firmware** — the original ESP32 DevKitC V4 build that
preceded our v13 ESP32-S3 hybrid migration. They are kept here for traceability
and engineering-journal reference; **they do not compile in the active sketch**
(Arduino IDE does not pick up sources from this subfolder).

If you need to rebuild a legacy target, copy the corresponding `.cpp` back to
`src/esp32/` temporarily and select its `WRO_ACTIVE_TARGET` value in
`src/esp32/wro_build_target.h`.

| File | Original target | Notes |
|------|------------------|-------|
| `legacy_eps323.cpp` | `WRO_TARGET_EPS323` (1) | v11 main race firmware |
| `legacy_test_motor_servo_drive.cpp` | `WRO_TARGET_TEST_MOTOR_SERVO_DRIVE` (3) | Drivetrain bring-up |
| `legacy_test_no_sensors.cpp` | `WRO_TARGET_TEST_NO_SENSORS` (4) | Open-loop timed lap |
| `legacy_test_servo.cpp` | `WRO_TARGET_TEST_SERVO` (5) | Servo sweep |
| `legacy_test_short_sequence.cpp` | `WRO_TARGET_TEST_SHORT_SEQUENCE` (6) | Short pattern |
| `legacy_test_servo_calibrate.cpp` | `WRO_TARGET_TEST_SERVO_CAL` (7) | Servo center calibration |

The current active build is **v13 main** (`WRO_TARGET_V13_MAIN` = 11), entry
point `src/esp32/wro_v13_main.cpp`. See `CHANGELOG.md` and
`docs/strategy/WRO_Migration_v12_to_v13.md` for the migration story.
