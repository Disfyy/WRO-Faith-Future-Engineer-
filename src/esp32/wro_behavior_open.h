#pragma once
/*
 * Open Challenge behavior — heading-hold using IMU yaw target.
 *
 * Without a side TFMini-S, true wall-following isn't possible (camera blob
 * width is too noisy under harsh lighting). Strategy:
 *   - After each corner exit, snap target heading to nearest 90° quadrant.
 *   - Drive straight using a heading-error PID on the steering servo.
 *   - Speed: OPEN_MAX_PWM, ramped via speed_ramp.
 *
 * When HAS_SIDE_TOF=1, an additional lateral correction is mixed in
 * (target wall distance = WALL_TARGET_MM).
 */

void  open_init();
void  open_reset();
void  open_set_target_heading(float deg);
float open_get_target_heading();

// Compute control outputs for this tick. Pass current yaw (deg).
// Outputs are written to global g_open_steer_us / g_open_speed_pwm.
void  open_update(float yaw_deg, int side_tof_mm);

extern int g_open_steer_us;
extern int g_open_speed_pwm;
