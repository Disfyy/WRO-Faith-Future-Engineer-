#pragma once

// Select which ESP32-S3 firmware source file is active during build.
// This prevents multiple setup()/loop() definitions from compiling together.
//
// v12: Updated for ESP32-S3 + AS5048A (SPI) + TFMini-S (UART)
//
// Targets 1, 3-7 are LEGACY (v11 hardware pins) — kept for reference only.
// They will not work correctly on v12 hardware. The v12 active path is
// target 11 (WRO_TARGET_V12_MAIN). v12-compatible bench/test targets are
// 2 (SCAN_I2C), 8 (TEST_ENCODERS), 9 (TEST_TFMINI), 10 (BENCH_TEST).

#define WRO_TARGET_EPS323                  1   // LEGACY v11 — legacy_eps323.cpp
#define WRO_TARGET_SCAN_I2C                2   // v12 — scan_i2c_v12.cpp
#define WRO_TARGET_TEST_MOTOR_SERVO_DRIVE  3   // LEGACY v11 — legacy_test_motor_servo_drive.cpp
#define WRO_TARGET_TEST_NO_SENSORS         4   // LEGACY v11 — legacy_test_no_sensors.cpp
#define WRO_TARGET_TEST_SERVO              5   // LEGACY v11 — legacy_test_servo.cpp
#define WRO_TARGET_TEST_SHORT_SEQUENCE     6   // LEGACY v11 — legacy_test_short_sequence.cpp
#define WRO_TARGET_TEST_SERVO_CAL          7   // LEGACY v11 — legacy_test_servo_calibrate.cpp (needs v12 port)
#define WRO_TARGET_TEST_ENCODERS           8   // v12 — test_encoders.cpp (AS5048A SPI)
#define WRO_TARGET_TEST_TFMINI             9   // v12 — test_tfmini.cpp
#define WRO_TARGET_BENCH_TEST              10  // v12 — bench_test_v12.cpp
#define WRO_TARGET_V12_MAIN                11  // v12 — wro_v12_main.cpp  ← ACTIVE

// ── Активный режим — меняй только эту строку ──────────────────
//   1  EPS323                  LEGACY v11 (только справочник)
//   2  SCAN_I2C                v12 (только IMU должен быть на I2C)
//   3  TEST_MOTOR_SERVO_DRIVE  LEGACY v11
//   4  TEST_NO_SENSORS         LEGACY v11
//   5  TEST_SERVO              LEGACY v11
//   6  TEST_SHORT_SEQUENCE     LEGACY v11
//   7  TEST_SERVO_CAL          LEGACY v11 (нужен порт под v12)
//   8  TEST_ENCODERS           v12 (AS5048A SPI энкодеры)
//   9  TEST_TFMINI             v12 (TFMini-S дальномеры)
//  10  BENCH_TEST              v12 (все датчики + актуаторы)
//  11  V12_MAIN                v12 (Open + Obstacle Challenge)  ← основная программа
#define WRO_ACTIVE_TARGET WRO_TARGET_V12_MAIN
