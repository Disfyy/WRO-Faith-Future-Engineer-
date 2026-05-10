#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

/*
 * WRO Future Engineers — v13 Main Firmware
 *
 * Hardware: ESP32-S3-DevKitC-1 N8R8 + 2× AS5600 (dual I2C) +
 *           VL53L1X front + side (XSHUT remap) + ICM-20948 IMU (I2C0) +
 *           OpenMV H7 Plus (UART2) + BTS7960 motor + JX PDI-6221MG servo.
 *
 * v13 keeps the v12 architecture intact — only the bottom HAL layer changed.
 * The TCA9548A I2C mux is GONE (it burned out); two AS5600s now sit on the
 * ESP32-S3's two native I2C peripherals (Wire and Wire1), and the VL53L1X
 * pair uses XSHUT-based address remapping at boot.
 *
 * Layered architecture (top-down only):
 *   HAL: drivers (as5600_dual_i2c.h, vl53l1x_dual.h, ICM20948 lib)
 *   Estimation: wro_imu, wro_odometry, wro_camera
 *   Behavior: wro_corner, wro_behavior_open, wro_behavior_obstacle, wro_park
 *   Control: this file (steering_mixer + speed_ramp)
 *   FSM: wro_race_fsm + wro_estop (parallel)
 *
 * Mode is a single compile-time #define in wro_config_v13.h:
 *   OBSTACLE_MODE 0 = Open Challenge   (3 laps, walls only)
 *   OBSTACLE_MODE 1 = Obstacle Challenge (red/green pillars + parking)
 * (Rule 9.9 compliance: NO physical mode switches.)
 *
 * Team Faith | WRO Future Engineers 2026 | v13.0
 */

#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_task_wdt.h>

#include "wro_config_v13.h"
#include "wro_imu.h"
#include "wro_odometry.h"
#include "wro_camera.h"
#include "wro_corner.h"
#include "wro_behavior_open.h"
#include "wro_behavior_obstacle.h"
#include "wro_park.h"
#include "wro_estop.h"
#include "wro_telemetry.h"
#include "wro_race_fsm.h"
#include "wro_sensors.h"

static Servo steeringServo;

static int  commandSpeed = 0;        // ramped actual PWM (signed)
static unsigned long lastLoopMs = 0;

// ─── Servo write with calibrated µs ───────────────────────────
static void writeSteeringUs(int us) {
  if (us > SERVO_RIGHT_SAFE_US) us = SERVO_RIGHT_SAFE_US;
  if (us < SERVO_LEFT_SAFE_US)  us = SERVO_LEFT_SAFE_US;
  steeringServo.writeMicroseconds(us);
}

// ─── Motor: BTS7960 LEDC PWM, signed speed ────────────────────
static void writeMotor(int signedPwm) {
  if (signedPwm >  255) signedPwm =  255;
  if (signedPwm < -255) signedPwm = -255;
  int mag = abs(signedPwm);
  if (mag != 0 && mag < MIN_DRIVE_PWM) mag = MIN_DRIVE_PWM;     // deadband floor

  if (signedPwm > 0) {
    ledcWrite(MOTOR_L_PWM, 0);
    ledcWrite(MOTOR_R_PWM, mag);
  } else if (signedPwm < 0) {
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, mag);
  } else {
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
  }
}

// ─── Speed ramp: avoid torque spikes / wheel slip ─────────────
static void applySpeedRamp(int target) {
  if (commandSpeed < target) commandSpeed = min(commandSpeed + SPEED_RAMP_STEP, target);
  else if (commandSpeed > target) commandSpeed = max(commandSpeed - SPEED_RAMP_STEP, target);
  writeMotor(commandSpeed);
}

void setup() {
  // ─── Rule 11.10: kill all wireless before anything else ────
  WiFi.mode(WIFI_OFF);
  btStop();

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("============================================================");
  Serial.println(" WRO FE 2026 — Team Faith — v13.0 main firmware");
  Serial.print  (" Mode: ");
#if OBSTACLE_MODE == 1
  Serial.println("OBSTACLE CHALLENGE");
#else
  Serial.println("OPEN CHALLENGE");
#endif
  Serial.println(" WiFi: OFF, BT: OFF (Rule 11.10)");
  Serial.println("============================================================");

  // ─── E-Stop input (early so any held-button bug is grace-windowed) ──
  estop_init();

  // ─── Status LED ─────────────────────────────────────────────
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ─── Motor driver pins ──────────────────────────────────────
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);
  ledcAttach(MOTOR_R_PWM, 20000, 8);   // 20 kHz, 8-bit (0..255)
  ledcAttach(MOTOR_L_PWM, 20000, 8);
  ledcWrite(MOTOR_R_PWM, 0);
  ledcWrite(MOTOR_L_PWM, 0);

  // ─── Steering servo ─────────────────────────────────────────
  steeringServo.setPeriodHertz(50);
  steeringServo.attach(SERVO_PIN, SERVO_LEFT_US, SERVO_RIGHT_US);
  writeSteeringUs(SERVO_CENTER_US);

  // ─── Encoders (AS5600 dual I2C) ─────────────────────────────
  if (!odo_init()) {
    Serial.println("ERROR: AS5600 init failed (check both I2C buses)");
  } else {
    Serial.println("AS5600 dual I2C: OK");
  }

  // ─── VL53L1X distance sensors (owned by wro_sensors) ───────
  sens_init();

  // ─── Camera (UART2) ─────────────────────────────────────────
  cam_init();

  // ─── IMU + gyro calibration ─────────────────────────────────
  if (!imu_init()) {
    Serial.println("ERROR: ICM-20948 not found! Halting.");
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(100);
      digitalWrite(LED_PIN, LOW);  delay(100);
    }
  } else {
    Serial.println("ICM-20948 IMU: OK");
  }
  if (!imu_calibrate_gyro()) {
    Serial.println("WARN: gyro calibration weak — continuing anyway");
  }

  // ─── Algorithm modules ──────────────────────────────────────
  open_init();
  obs_init();
  park_init();
  corner_init();
  tlm_init();
  race_init();

  // Arm task watchdog last — once registered, every loop iteration must
  // call esp_task_wdt_reset() within WDT_TIMEOUT_MS or the board hard-resets.
  // Done after gyro calibration (which uses delay() and would tickle the WDT).
  esp_task_wdt_config_t wdt_cfg = {
      .timeout_ms    = WDT_TIMEOUT_MS,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);
  Serial.print("Task watchdog armed at ");
  Serial.print(WDT_TIMEOUT_MS);
  Serial.println(" ms");

  Serial.println("System ready. Press E-Stop to start.");
  digitalWrite(LED_PIN, HIGH);
  lastLoopMs = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopMs < (unsigned long)LOOP_INTERVAL_MS) return;
  lastLoopMs = now;

  // ─── Sensor updates (HAL → estimation) ─────────────────────
  imu_update();
  odo_update();
  sens_update();
  cam_update();
  estop_update();

  // ─── Top-level FSM produces commands ───────────────────────
  race_update();

  // ─── Apply commands to actuators ───────────────────────────
  writeSteeringUs(g_cmd_steer_us);
  applySpeedRamp(g_cmd_speed_pwm);

  // ─── Telemetry (rate-limited internally) ───────────────────
  tlm_update_periodic(g_race_state, g_corner_state, g_lap_count,
                      commandSpeed, g_cmd_steer_us);

  // ─── Pet the dog ───────────────────────────────────────────
  esp_task_wdt_reset();
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
