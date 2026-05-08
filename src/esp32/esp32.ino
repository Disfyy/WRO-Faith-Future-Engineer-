// WRO Future Engineers — точка входа для Arduino IDE (v13)
//
// Target: ESP32-S3-DevKitC-1 (N8R8)
// Hardware: 2× AS5600 (dual I2C, no mux) + 2× VL53L1X (XSHUT addr-remap) +
//           ICM-20948 (I2C0 with AS5600 Left + VL53L1X Front) +
//           OpenMV H7 Plus (UART2)
//
// Все setup() и loop() находятся в .cpp файлах.
// Чтобы выбрать режим работы — измени wro_build_target.h:
//
//   WRO_ACTIVE_TARGET = 1   →  legacy_eps323.cpp                  LEGACY v11
//   WRO_ACTIVE_TARGET = 2   →  scan_i2c_v13.cpp                   v13 двушинный I2C сканер
//   WRO_ACTIVE_TARGET = 3   →  legacy_test_motor_servo_drive.cpp  LEGACY v11
//   WRO_ACTIVE_TARGET = 4   →  legacy_test_no_sensors.cpp         LEGACY v11
//   WRO_ACTIVE_TARGET = 5   →  legacy_test_servo.cpp              LEGACY v11
//   WRO_ACTIVE_TARGET = 6   →  legacy_test_short_sequence.cpp     LEGACY v11
//   WRO_ACTIVE_TARGET = 7   →  legacy_test_servo_calibrate.cpp    LEGACY v11 (v13 port pending)
//   WRO_ACTIVE_TARGET = 8   →  test_encoders.cpp                  v13 AS5600 dual-I2C test
//   WRO_ACTIVE_TARGET = 9   →  test_vl53l1x.cpp                   v13 VL53L1X test
//   WRO_ACTIVE_TARGET = 10  →  bench_test_v13.cpp                 v13 full bench
//   WRO_ACTIVE_TARGET = 11  →  wro_v13_main.cpp                   v13 main firmware  ← ACTIVE

#include "wro_build_target.h"
