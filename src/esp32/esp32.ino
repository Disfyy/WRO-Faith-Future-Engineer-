// WRO Future Engineers — точка входа для Arduino IDE (v12)
//
// Target: ESP32-S3-DevKitC-1 (N8R8)
// Hardware: AS5048A (SPI) + TFMini-S (UART) + ICM-20948 (I2C)
//
// Все setup() и loop() находятся в .cpp файлах.
// Чтобы выбрать режим работы — измени wro_build_target.h:
//
//   WRO_ACTIVE_TARGET = 1   →  eps323.cpp              (основная программа)
//   WRO_ACTIVE_TARGET = 2   →  scan_i2c_v12.cpp        (I2C скан — только IMU)
//   WRO_ACTIVE_TARGET = 3   →  test_motor_servo_drive  (мотор + серво)
//   WRO_ACTIVE_TARGET = 4   →  test_no_sensors         (круги без датчиков)
//   WRO_ACTIVE_TARGET = 5   →  test_servo              (только сервопривод)
//   WRO_ACTIVE_TARGET = 6   →  test_short_sequence     (короткая трасса)
//   WRO_ACTIVE_TARGET = 7   →  test_servo_calibrate    (калибровка серво)
//   WRO_ACTIVE_TARGET = 8   →  test_encoders_v12.cpp   (AS5048A SPI тест)
//   WRO_ACTIVE_TARGET = 9   →  test_tfmini.cpp         (TFMini-S UART тест)
//   WRO_ACTIVE_TARGET = 10  →  bench_test_v12.cpp      (все датчики + актуаторы)
//   WRO_ACTIVE_TARGET = 11  →  wro_v12_main.cpp        (новая основная программа)

#include "wro_build_target.h"
