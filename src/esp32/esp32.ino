// WRO Future Engineers — точка входа для Arduino IDE
//
// Все setup() и loop() находятся в .cpp файлах.
// Чтобы выбрать режим работы — измени wro_build_target.h:
//
//   WRO_ACTIVE_TARGET = 2  →  scanerI2C.cpp       (I2C скан всех датчиков)
//   WRO_ACTIVE_TARGET = 8  →  test_encoders.cpp   (тест энкодеров AS5600)
//   WRO_ACTIVE_TARGET = 5  →  test_servo.cpp      (тест сервопривода)
//   WRO_ACTIVE_TARGET = 3  →  test_motor_servo_drive.cpp (мотор + серво)
//   WRO_ACTIVE_TARGET = 7  →  test_servo_calibrate.cpp   (калибровка серво)
//   WRO_ACTIVE_TARGET = 4  →  test_no_sensors.cpp (круги без датчиков)
//   WRO_ACTIVE_TARGET = 6  →  test_short_sequence.cpp    (короткая трасса)
//   WRO_ACTIVE_TARGET = 1  →  eps323.cpp          (основная программа)

#include "wro_build_target.h"
