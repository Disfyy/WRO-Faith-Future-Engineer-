// WRO Future Engineers — точка входа для Arduino IDE (v12)
//
// Target: ESP32-S3-DevKitC-1 (N8R8)
// Hardware: AS5048A (SPI) + TFMini-S (UART) + ICM-20948 (I2C)
//
// Все setup() и loop() находятся в .cpp файлах.
// Чтобы выбрать режим работы — измени wro_build_target.h:
//
//   WRO_ACTIVE_TARGET = 1   →  legacy_eps323.cpp                  LEGACY v11
//   WRO_ACTIVE_TARGET = 2   →  scan_i2c_v12.cpp                   v12 I2C scan (IMU only)
//   WRO_ACTIVE_TARGET = 3   →  legacy_test_motor_servo_drive.cpp  LEGACY v11
//   WRO_ACTIVE_TARGET = 4   →  legacy_test_no_sensors.cpp         LEGACY v11
//   WRO_ACTIVE_TARGET = 5   →  legacy_test_servo.cpp              LEGACY v11
//   WRO_ACTIVE_TARGET = 6   →  legacy_test_short_sequence.cpp     LEGACY v11
//   WRO_ACTIVE_TARGET = 7   →  legacy_test_servo_calibrate.cpp    LEGACY v11 (v12 port pending)
//   WRO_ACTIVE_TARGET = 8   →  test_encoders.cpp                  v12 AS5048A SPI test
//   WRO_ACTIVE_TARGET = 9   →  test_tfmini.cpp                    v12 TFMini-S UART test
//   WRO_ACTIVE_TARGET = 10  →  bench_test_v12.cpp                 v12 full bench
//   WRO_ACTIVE_TARGET = 11  →  wro_v12_main.cpp                   v12 main firmware  ← ACTIVE

#include "wro_build_target.h"
