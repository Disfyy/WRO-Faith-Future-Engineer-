#if __has_include("wro_build_target.h")
#include "wro_build_target.h"
#else
#define WRO_TARGET_TEST_SERVO 3
#define WRO_ACTIVE_TARGET WRO_TARGET_TEST_SERVO
#endif

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_SERVO

#include <Arduino.h>

constexpr int SERVO_PIN = 27;

void setup() {
  Serial.begin(115200);
  delay(300);

  // Настройка аппаратного таймера для нового ядра ESP32 Core V3
  // Указываем пин, частоту (50 Гц) и разрешение (14 бит)
  ledcAttach(SERVO_PIN, 50, 14);

  Serial.println("Start LEDC Servo Test (ESP32 Core V3)");
}

void setServoUS(int us) {
  // Для 50 Гц период равен 20000 мкс
  // Значение ШИМ = (us / 20000.0) * 16384
  uint32_t duty = (us * 16384) / 20000;
  
  // В ESP32 Core V3 пишем прямо в пин, а не в канал
  ledcWrite(SERVO_PIN, duty);
  
  Serial.print("Servo US -> ");
  Serial.println(us);
}

void loop() {
  setServoUS(1215); // Лево
  delay(1000);
  
  setServoUS(1565); // Центр
  delay(1000);
  
  setServoUS(1915); // Право
  delay(1000);
  
  setServoUS(1565); // Центр
  delay(1000);
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_SERVO
