#pragma once

// Select which ESP32 firmware source file is active during build.
// This prevents multiple setup()/loop() definitions from compiling together.
#define WRO_TARGET_EPS323 1
#define WRO_TARGET_SCAN_I2C 2
#define WRO_TARGET_TEST_MOTOR_SERVO_DRIVE 3
#define WRO_TARGET_TEST_NO_SENSORS 4
#define WRO_TARGET_TEST_SERVO 5
#define WRO_TARGET_TEST_SHORT_SEQUENCE 6
#define WRO_TARGET_TEST_SERVO_CAL      7
#define WRO_TARGET_TEST_ENCODERS       8

// ── Активный режим — меняй только эту строку ──────────────────
//   1  EPS323 (основная программа)
//   2  SCAN_I2C (проверить все датчики на I2C шине)
//   3  TEST_MOTOR_SERVO_DRIVE (мотор + серво вместе)
//   4  TEST_NO_SENSORS (круги без датчиков, таймер)
//   5  TEST_SERVO (только сервопривод)
//   6  TEST_SHORT_SEQUENCE (короткая трасса)
//   7  TEST_SERVO_CAL (калибровка нейтрали серво)
//   8  TEST_ENCODERS (оба AS5600 энкодера)
#define WRO_ACTIVE_TARGET WRO_TARGET_TEST_ENCODERS
