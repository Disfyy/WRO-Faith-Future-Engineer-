#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "wro_config_v13.h"
#include "wro_imu.h"

static Adafruit_ICM20948 icm;

float g_yaw       = 0.0f;
float g_yaw_total = 0.0f;
float g_yaw_rate  = 0.0f;
bool  g_imu_ok    = false;

static float gyroZBias    = 0.0f;
static float lastYaw      = 0.0f;
static unsigned long lastImuMs = 0;
static int   failStreak   = 0;

bool imu_init() {
  // I2C0 (Wire) is shared with AS5600 Left + VL53L1X Front; the AS5600/VL53L1X
  // drivers also call Wire.begin(I2C0_SDA, I2C0_SCL) — ESP-IDF tolerates
  // repeated begin() so the order doesn't matter, but keeping a single
  // setClock here ensures every device runs at I2C_FREQ_HZ.
  Wire.begin(I2C0_SDA, I2C0_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  if (!icm.begin_I2C(ICM20948_ADDRESS)) {
    g_imu_ok = false;
    return false;
  }
  icm.setGyroRange(ICM20948_GYRO_RANGE_500_DPS);
  icm.setAccelRange(ICM20948_ACCEL_RANGE_4_G);
  g_imu_ok = true;
  return true;
}

bool imu_calibrate_gyro() {
  if (!g_imu_ok) return false;
  Serial.print("Calibrating gyro Z bias");
  double sum = 0.0;
  int n = 0;
  for (int i = 0; i < GYRO_CALIB_SAMPLES_V13; i++) {
    sensors_event_t a, g, t, m;
    if (icm.getEvent(&a, &g, &t, &m)) {
      sum += g.gyro.z;
      n++;
    }
    if ((i % 50) == 0) Serial.print(".");
    delay(LOOP_INTERVAL_MS);
  }
  Serial.println();
  if (n < GYRO_CALIB_SAMPLES_V13 / 2) {
    Serial.println("Gyro cal FAILED (not enough samples)");
    return false;
  }
  gyroZBias = (float)(sum / n);
  Serial.print("Gyro Z bias = ");
  Serial.print(gyroZBias, 5);
  Serial.println(" rad/s");
  g_yaw = 0.0f;
  g_yaw_total = 0.0f;
  lastYaw = 0.0f;
  lastImuMs = millis();
  return true;
}

void imu_update() {
  if (!g_imu_ok) return;

  sensors_event_t a, g, t, m;
  if (!icm.getEvent(&a, &g, &t, &m)) {
    if (++failStreak >= IMU_FAIL_LIMIT) g_imu_ok = false;
    return;
  }
  failStreak = 0;

  unsigned long now = millis();
  float dt = (now - lastImuMs) * 0.001f;
  lastImuMs = now;
  if (dt > IMU_DT_MAX) dt = IMU_DT_MAX;

  float omegaZ = g.gyro.z - gyroZBias;     // rad/s, sign-corrected for chassis below
  float deltaDeg = -omegaZ * dt * 57.29577951f;  // negate so +yaw = CCW (standard)

  g_yaw_total += deltaDeg;
  g_yaw       += deltaDeg;
  while (g_yaw >  180.0f) g_yaw -= 360.0f;
  while (g_yaw <= -180.0f) g_yaw += 360.0f;

  if (dt > 0.0001f) g_yaw_rate = deltaDeg / dt;
  lastYaw = g_yaw;
}

bool imu_is_healthy() { return g_imu_ok; }

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
