#pragma once

// Select which ESP32-S3 firmware source file is active during build.
// This prevents multiple setup()/loop() definitions from compiling together.
//
// v13: ESP32-S3 + AS5600 (dual I2C, no mux) + VL53L1X (XSHUT addr-remap) + ICM-20948
//      The TCA9548A I2C mux that v11 used is gone (it burned out); two AS5600s
//      now share the load across the two native I2C peripherals on the S3.
//
// Targets 1, 3-7 are LEGACY (v11 hardware pins) — kept for reference only.
// They will not work correctly on v13 hardware. The v13 active path is
// target 11 (WRO_TARGET_V13_MAIN). v13-compatible bench/test targets are
// 2 (SCAN_I2C), 8 (TEST_ENCODERS), 9 (TEST_VL53L1X), 10 (BENCH_TEST).

#define WRO_TARGET_EPS323                  1   // LEGACY v11 — legacy_eps323.cpp
#define WRO_TARGET_SCAN_I2C                2   // v13 — scan_i2c_v13.cpp (both buses)
#define WRO_TARGET_TEST_MOTOR_SERVO_DRIVE  3   // LEGACY v11 — legacy_test_motor_servo_drive.cpp
#define WRO_TARGET_TEST_NO_SENSORS         4   // LEGACY v11 — legacy_test_no_sensors.cpp
#define WRO_TARGET_TEST_SERVO              5   // LEGACY v11 — legacy_test_servo.cpp
#define WRO_TARGET_TEST_SHORT_SEQUENCE     6   // LEGACY v11 — legacy_test_short_sequence.cpp
#define WRO_TARGET_TEST_SERVO_CAL          7   // LEGACY v11 — legacy_test_servo_calibrate.cpp (needs v13 port)
#define WRO_TARGET_TEST_ENCODERS           8   // v13 — test_encoders.cpp (AS5600 dual I2C)
#define WRO_TARGET_TEST_VL53L1X            9   // v13 — test_vl53l1x.cpp
#define WRO_TARGET_BENCH_TEST              10  // v13 — bench_test_v13.cpp
#define WRO_TARGET_V13_MAIN                11  // v13 — wro_v13_main.cpp  ← ACTIVE

// ── Активный режим — меняй только эту строку ──────────────────
//   1  EPS323                  LEGACY v11 (только справочник)
//   2  SCAN_I2C                v13 (обе I2C шины: IMU+AS5600+VL53L1X)
//   3  TEST_MOTOR_SERVO_DRIVE  LEGACY v11
//   4  TEST_NO_SENSORS         LEGACY v11
//   5  TEST_SERVO              LEGACY v11
//   6  TEST_SHORT_SEQUENCE     LEGACY v11
//   7  TEST_SERVO_CAL          LEGACY v11 (нужен порт под v13)
//   8  TEST_ENCODERS           v13 (AS5600 на двух I2C шинах)
//   9  TEST_VL53L1X            v13 (VL53L1X с XSHUT-переадресацией)
//  10  BENCH_TEST              v13 (все датчики + актуаторы)
//  11  V13_MAIN                v13 (Open + Obstacle Challenge)  ← основная программа
#define WRO_ACTIVE_TARGET WRO_TARGET_V13_MAIN
