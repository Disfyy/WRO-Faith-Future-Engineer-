#pragma once
/*
 * Corner detector — the critical fix for the v11 turn failure.
 *
 * v11 mistake: cornering was triggered by a *combination* of camera/wall
 * signals AND directionConfirmed AND debounce → too gated; sometimes never
 * fired. Exit used encoder distance, which lies during a slipping turn.
 *
 * v12 fix: corner detection is owned by front-distance LiDAR + IMU yaw delta.
 * Camera is NEVER an exit condition. Encoder ticks are telemetry-only.
 * v13 reverts the front sensor from TFMini-S (UART, 12 m) back to VL53L1X
 * (I2C, ~3 m practical) — same API contract through wro_sensors, only the
 * driver under it changes. The corner FSM logic is untouched.
 *
 * State machine:
 *   ARMED → SLOWDOWN → COMMIT → BRAKE → EXECUTE → EXIT (→ ARMED via lockout)
 *   EXECUTE <-> BACK: 3-point "saw" turn — reverse-and-continue when the wall
 *     closes before the turn finishes (corner tighter than the car's turn
 *     radius, or a matte-black wall seen too late). The reverse leg counter-
 *     steers so yaw keeps rotating the SAME way.
 *   PANIC: hard SAFE_STOP only after TURN_BACK_MAX_TRIES reverse legs.
 *
 * Outputs each tick:
 *   corner_steer_us — steering microseconds (use directly during EXECUTE/BRAKE)
 *   corner_speed_pwm — speed override (0 during BRAKE, TURN_SPEED_PWM during EXECUTE)
 *   corner_active() — whether the FSM owns the actuators
 */

#include <stdint.h>

enum CornerState {
  CN_ARMED,
  CN_SLOWDOWN,
  CN_COMMIT,
  CN_BRAKE,
  CN_EXECUTE,
  CN_EXIT,
  CN_LOCKOUT,
  CN_FAIL,
  CN_BACK        // reversing leg of a 3-point turn (appended → telemetry code 8)
};

void corner_init();
void corner_reset();

// Direction: +1 = turn right (CW track), -1 = turn left (CCW track).
// Updated externally based on globalTrackDirection.
void corner_set_direction(int dir);

// Returns: true if the corner FSM is currently overriding control.
bool corner_active();

// Returns: true if the FSM is in CN_FAIL (caller should SAFE_STOP).
bool corner_failed();

// Update step. Call every loop tick after sensors (VL53L1X front, IMU) are updated.
// Pass current yaw and current front-distance reading in mm.
void corner_update(float yaw_deg, int tf_front_mm, bool tf_front_ok, unsigned long now_ms);

// Outputs (read after corner_update):
extern int  g_corner_steer_us;     // µs (1000..2000) — only valid when corner_active()
extern int  g_corner_speed_pwm;    // PWM (0..255) — speed override
extern int  g_corner_state;        // for telemetry

// Triggered after EXIT — race FSM should snap target heading to nearest 90°.
extern bool g_corner_just_exited;
