#pragma once
/*
 * Odometry — wraps the AS5600 dual-I2C driver, handles 12-bit roll-over,
 * accumulates signed tick distance, computes instantaneous speed.
 *
 * Sign convention: forward driving increases the accumulator.
 * If a wheel mounts mirrored, flip ENC_LEFT_SIGN / ENC_RIGHT_SIGN in the .cpp.
 *
 * Exports:
 *   g_dist_left_ticks  / g_dist_right_ticks  — signed accumulators
 *   g_speed_cm_s       — averaged wheel speed, cm/s (signed)
 *   g_enc_ok           — false after ENC_FAIL_LIMIT consecutive errors
 */

#include <stdint.h>

bool  odo_init();
void  odo_update();           // call every loop tick
void  odo_reset();
long  odo_avg_dist_ticks();   // (left+right)/2
float odo_avg_dist_cm();
bool  odo_is_healthy();

extern long  g_dist_left_ticks;
extern long  g_dist_right_ticks;
extern float g_speed_cm_s;
extern bool  g_enc_ok;
