#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_SHORT_SEQUENCE

#include <Arduino.h>
#include <ESP32Servo.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

Servo steeringServo;

constexpr int SERVO_PIN = 27;
constexpr int MOTOR_R_EN = 19;
constexpr int MOTOR_L_EN = 23;
constexpr int MOTOR_R_PWM = 5;
constexpr int MOTOR_L_PWM = 14;

constexpr int SERVO_CENTER = 90;
constexpr int TURN_ANGLE = 45;
constexpr int DRIVE_SPEED = 120;
constexpr int TURN_SPEED = 95;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#define USE_LEDC_PIN_API 1
#else
#define USE_LEDC_PIN_API 0
#endif

void setMotorSpeed(int speed) {
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
#if USE_LEDC_PIN_API
    ledcWrite(MOTOR_L_PWM, 0);
    ledcWrite(MOTOR_R_PWM, speed);
#else
    ledcWrite(1, 0);
    ledcWrite(0, speed);
#endif
  } else if (speed < 0) {
#if USE_LEDC_PIN_API
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, -speed);
#else
    ledcWrite(0, 0);
    ledcWrite(1, -speed);
#endif
  } else {
#if USE_LEDC_PIN_API
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
#else
    ledcWrite(0, 0);
    ledcWrite(1, 0);
#endif
  }
}

void setup() {
  Serial.begin(115200);

  steeringServo.attach(SERVO_PIN, 500, 2500);
  steeringServo.write(SERVO_CENTER);

  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);

#if USE_LEDC_PIN_API
  ledcAttach(MOTOR_R_PWM, 20000, 8);
  ledcAttach(MOTOR_L_PWM, 20000, 8);
#else
  ledcSetup(0, 20000, 8);
  ledcAttachPin(MOTOR_R_PWM, 0);
  ledcSetup(1, 20000, 8);
  ledcAttachPin(MOTOR_L_PWM, 1);
#endif

  setMotorSpeed(0);
  delay(500);

  // 1) Drive forward a little for 3 seconds.
  setMotorSpeed(DRIVE_SPEED);
  delay(3000);

  // 2) Turn steering to +45 and keep moving briefly.
  steeringServo.write(SERVO_CENTER + TURN_ANGLE);
  setMotorSpeed(TURN_SPEED);
  delay(1000);

  // 3) Turn steering to -45 and hold shortly.
  steeringServo.write(SERVO_CENTER - TURN_ANGLE);
  setMotorSpeed(0);
  delay(1000);

  // 4) Center steering and drive backward.
  steeringServo.write(SERVO_CENTER);
  setMotorSpeed(-DRIVE_SPEED);
  delay(2000);

  // 5) Full stop.
  setMotorSpeed(0);
  steeringServo.write(SERVO_CENTER);
  Serial.println("Sequence complete.");
}

void loop() {
  // Run once.
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_SHORT_SEQUENCE
