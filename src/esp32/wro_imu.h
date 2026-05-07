#pragma once
/*
 * IMU module — ICM-20948 over I2C (Wire, GPIO 8/9)
 *
 * Yaw integration uses gyro Z only with a static-bias offset measured
 * during boot calibration. Magnetometer is intentionally unused — brushed
 * motor EMI corrupts mag readings on this chassis.
 *
 * Exports:
 *   g_yaw       — wrapped to (-180, 180]
 *   g_yaw_total — unbounded accumulator (used by lap counter)
 *   g_yaw_rate  — deg/s (filtered via dt clamp only)
 */

bool  imu_init();
bool  imu_calibrate_gyro();
void  imu_update();           // call every loop tick
bool  imu_is_healthy();

extern float g_yaw;
extern float g_yaw_total;
extern float g_yaw_rate;
extern bool  g_imu_ok;
