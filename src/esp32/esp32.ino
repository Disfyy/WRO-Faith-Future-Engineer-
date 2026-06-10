// WRO Future Engineers — Arduino IDE entry point (v13)
//
// Target: ESP32-S3-DevKitC-1 (N8R8)
// Hardware: 2x AS5600 (dual I2C, no mux) + 2x VL53L1X (XSHUT addr-remap) +
//           ICM-20948 (I2C0 with AS5600 Left + VL53L1X Front) +
//           OpenMV H7 Plus (UART2)
//
// All setup() and loop() functions live in the .cpp files.
// To switch which firmware is active, edit wro_build_target.h:
//
//   WRO_ACTIVE_TARGET = 1   ->  legacy/legacy_eps323.cpp                  ARCHIVED v11 (see legacy/README.md)
//   WRO_ACTIVE_TARGET = 2   ->  diag_scan_i2c_v13.cpp                     v13 dual-bus I2C scanner
//   WRO_ACTIVE_TARGET = 3   ->  target_test_motor_servo_drive.cpp         v11 drivetrain test (root shim -> legacy/)
//   WRO_ACTIVE_TARGET = 4   ->  legacy/legacy_test_no_sensors.cpp         ARCHIVED v11
//   WRO_ACTIVE_TARGET = 5   ->  legacy/legacy_test_servo.cpp              ARCHIVED v11
//   WRO_ACTIVE_TARGET = 6   ->  legacy/legacy_test_short_sequence.cpp     ARCHIVED v11
//   WRO_ACTIVE_TARGET = 7   ->  diag_test_servo_calibrate_v13.cpp         v13 servo calibration (interactive)
//   WRO_ACTIVE_TARGET = 8   ->  diag_test_encoders.cpp                    v13 AS5600 dual-I2C test
//   WRO_ACTIVE_TARGET = 9   ->  diag_test_vl53l1x.cpp                     v13 VL53L1X test
//   WRO_ACTIVE_TARGET = 10  ->  diag_bench_test_v13.cpp                   v13 full bench
//   WRO_ACTIVE_TARGET = 11  ->  wro_v13_main.cpp                          v13 main firmware  <-- ACTIVE
//
// NOTE: ARCHIVED v11 targets do not compile in the active sketch.
// Arduino IDE does not pick up sources from src/esp32/legacy/.
// To rebuild a legacy target, temporarily copy its .cpp back into src/esp32/.

#include "wro_build_target.h"
