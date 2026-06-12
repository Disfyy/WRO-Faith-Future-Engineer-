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
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_task_wdt.h>

#include "wro_config_v13.h"
#include "wro_i2c_buses.h"
#include "wro_imu.h"
#include "wro_odometry.h"
#include "wro_camera.h"
#include "wro_corner.h"
#include "wro_behavior_open.h"
#include "wro_behavior_obstacle.h"
#include "wro_park.h"
#include "wro_estop.h"
#include "wro_telemetry.h"
#include "wro_telemetry_wifi.h"
#include "wro_race_fsm.h"
#include "wro_sensors.h"
#include "wro_steering_comp.h"

static int  commandSpeed = 0;        // ramped actual PWM (signed)
static unsigned long lastLoopMs = 0;

// ─── Servo write with calibrated µs ───────────────────────────
static void writeSteeringUs(int us) {
  // Asymmetry trim must run BEFORE the safe clamp so a gain >1 can never
  // push the horn past the mechanical end-stop.
  us = steer_compensate_us(us, g_steer_gain_left, g_steer_gain_right);
  if (us > SERVO_RIGHT_SAFE_US) us = SERVO_RIGHT_SAFE_US;
  if (us < SERVO_LEFT_SAFE_US)  us = SERVO_LEFT_SAFE_US;
  
  // Convert µs to 14-bit duty cycle for 50Hz PWM (20000 µs period)
  // duty = (us * 16384) / 20000
  uint32_t duty = (us * 16384) / 20000;
  ledcWrite(SERVO_PIN, duty);
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
#if WIFI_TELEMETRY
  Serial.println(" WiFi: TELEMETRY AP ON - TESTING BUILD, set WIFI_TELEMETRY 0 for competition (Rule 11.10)");
#else
  Serial.println(" WiFi: OFF, BT: OFF (Rule 11.10)");
#endif
  Serial.println("============================================================");

  // ─── E-Stop input (early so any held-button bug is grace-windowed) ──
  estop_init();

  // ─── Status LED ─────────────────────────────────────────────
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ─── Steering servo ─────────────────────────────────────────
  ledcAttach(SERVO_PIN, 50, 14);       // 50 Hz, 14-bit resolution
  writeSteeringUs(SERVO_CENTER_US);

  // ─── Motor driver pins ──────────────────────────────────────
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);
  
  ledcAttach(MOTOR_R_PWM, 20000, 8);   // 20 kHz, 8-bit (0..255)
  ledcAttach(MOTOR_L_PWM, 20000, 8);
  ledcWrite(MOTOR_R_PWM, 0);
  ledcWrite(MOTOR_L_PWM, 0);

  // ─── I2C buses (Wire + Wire1) ───────────────────────────────
  i2c_buses_init();

  // ─── Encoders (AS5600 dual I2C) ─────────────────────────────
  if (!odo_init()) {
    Serial.println("ERROR: AS5600 init failed (check both I2C buses + magnet air-gap 0.5-3 mm)");
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
      esp_task_wdt_reset();           // no-op until WDT armed below; keeps blink-of-death reboot-free
    }
  } else {
    Serial.println("ICM-20948 IMU: OK");
  }
  // Gyro bias calibration. A failed calibration leaves bias = 0, which means
  // 30–120°/min of yaw drift — retry before giving up, and be loud if it
  // still fails.
  {
    bool gyroCalOk = false;
    for (int attempt = 1; attempt <= 3 && !gyroCalOk; attempt++) {
      gyroCalOk = imu_calibrate_gyro();
      if (!gyroCalOk) {
        Serial.printf("WARN: gyro calibration weak (attempt %d/3) - retrying; keep the robot STILL\n", attempt);
      }
    }
    if (!gyroCalOk) {
      Serial.println("WARN: gyro calibration FAILED after 3 attempts - yaw will drift!");
      Serial.println("WARN: power-cycle on a still surface before racing.");
    }
  }

  // ─── Algorithm modules ──────────────────────────────────────
  open_init();
  obs_init();
  park_init();
  corner_init();
  tlm_init();
  race_init();

  // Wi-Fi telemetry mirror (testing builds only; no-op when WIFI_TELEMETRY=0).
  // Must run before the WDT below: softAP() blocks longer than WDT_TIMEOUT_MS.
  tlm_wifi_init();

  // Arm task watchdog last — once registered, every loop iteration must
  // call esp_task_wdt_reset() within WDT_TIMEOUT_MS or the board hard-resets.
  // Done after gyro calibration (which uses delay() and would tickle the WDT).
  // reconfigure() works when the Arduino core pre-initialized the TWDT
  // (the default); fall back to init() for cores built without it. Either
  // way, VERIFY the result — printing "armed" while arming silently failed
  // would leave the race loop with no hang protection.
  esp_task_wdt_config_t wdt_cfg = {
      .timeout_ms    = WDT_TIMEOUT_MS,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_err_t wdtErr = esp_task_wdt_reconfigure(&wdt_cfg);
  if (wdtErr != ESP_OK) wdtErr = esp_task_wdt_init(&wdt_cfg);   // TWDT not pre-inited by this core
  esp_err_t wdtAddErr = esp_task_wdt_add(NULL);
  if (wdtErr != ESP_OK || wdtAddErr != ESP_OK) {
    Serial.printf("ERROR: task watchdog arming FAILED (cfg=%d add=%d) - NO hang protection!\n",
                  (int)wdtErr, (int)wdtAddErr);
  } else {
    Serial.print("Task watchdog armed at ");
    Serial.print(WDT_TIMEOUT_MS);
    Serial.println(" ms");
  }

  Serial.println("System ready. Press E-Stop to start.");
  digitalWrite(LED_PIN, HIGH);
  lastLoopMs = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopMs < (unsigned long)LOOP_INTERVAL_MS) return;
  lastLoopMs = now;

  // Pet the watchdog at the top of every active tick. If any blocking
  // call below stalls (I2C lock-up, library timeout) for >WDT_TIMEOUT_MS
  // (200 ms), the ESP32 reboots rather than sit motionless on the track.
  esp_task_wdt_reset();

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
  // SAFE_STOP must cut drive immediately, not coast down through the ramp
  // (estop.h contract: "motor off immediately, NOT through ramp").
  if (g_race_state == RS_SAFE_STOP) {
    commandSpeed = 0;
    writeMotor(0);
  } else {
    applySpeedRamp(g_cmd_speed_pwm);
  }

  // ─── Telemetry (rate-limited internally) ───────────────────
  tlm_update_periodic(g_race_state, g_corner_state, g_lap_count,
                      commandSpeed, g_cmd_steer_us);

  // ─── Pet the dog ───────────────────────────────────────────
  esp_task_wdt_reset();
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
