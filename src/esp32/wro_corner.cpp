#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V12_MAIN

#include <Arduino.h>
#include "wro_config_v12.h"
#include "wro_pid.h"
#include "wro_corner.h"

int  g_corner_steer_us    = SERVO_CENTER_US;
int  g_corner_speed_pwm   = OPEN_MAX_PWM;
int  g_corner_state       = CN_ARMED;
bool g_corner_just_exited = false;
float g_corner_exit_yaw   = 0.0f;

static int           turnDirection      = -1;       // +1 right (CW), -1 left (CCW)
static unsigned long stateEnteredMs     = 0;
static unsigned long lockoutUntilMs     = 0;
static int           validFrameStreak   = 0;        // for COMMIT debounce
static float         turnStartYaw       = 0.0f;
static int           lastTfMm           = 9999;

static inline void enter(CornerState s, unsigned long now) {
  g_corner_state    = s;
  stateEnteredMs    = now;
  validFrameStreak  = 0;
  g_corner_just_exited = false;
}

void corner_init() {
  enter(CN_ARMED, millis());
  g_corner_steer_us  = SERVO_CENTER_US;
  g_corner_speed_pwm = OPEN_MAX_PWM;
  lockoutUntilMs     = 0;
}

void corner_reset() { corner_init(); }
void corner_set_direction(int dir) { turnDirection = (dir >= 0) ? +1 : -1; }
bool corner_active()  { return g_corner_state == CN_BRAKE || g_corner_state == CN_EXECUTE; }
bool corner_failed()  { return g_corner_state == CN_FAIL; }

void corner_update(float yaw_deg, int tf_front_mm, bool tf_front_ok, unsigned long now) {
  // Frames where the front sensor is unreliable can NOT trigger a turn.
  bool framePresent = tf_front_ok && tf_front_mm > 0 && tf_front_mm < 9999;
  if (framePresent) lastTfMm = tf_front_mm;

  switch (g_corner_state) {

    case CN_ARMED:
      g_corner_steer_us  = SERVO_CENTER_US;          // pass-through to caller
      g_corner_speed_pwm = OPEN_MAX_PWM;             // caller may overwrite outside corner
      if (framePresent && tf_front_mm < TURN_SLOWDOWN_MM &&
          now > lockoutUntilMs) {
        enter(CN_SLOWDOWN, now);
      }
      break;

    case CN_SLOWDOWN:
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = TURN_SPEED_PWM;
      if (!framePresent) {
        // lost sight during approach: bail back to ARMED, no commit
        if (now - stateEnteredMs > 500) enter(CN_ARMED, now);
        break;
      }
      if (tf_front_mm < TURN_COMMIT_MM) {
        validFrameStreak++;
        if (validFrameStreak >= TURN_FRAMES_DEBOUNCE) {
          turnStartYaw = yaw_deg;
          enter(CN_COMMIT, now);
        }
      } else if (tf_front_mm > TURN_SLOWDOWN_MM + 50) {
        enter(CN_ARMED, now);                         // wall went away (false alarm)
      } else {
        validFrameStreak = 0;                         // hold in slowdown
      }
      break;

    case CN_COMMIT:
      // 1-tick decision state — pick direction, brake-straight.
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = 0;
      enter(CN_BRAKE, now);
      break;

    case CN_BRAKE:
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = 0;
      if (now - stateEnteredMs >= TURN_BRAKE_MS) enter(CN_EXECUTE, now);
      break;

    case CN_EXECUTE: {
      g_corner_steer_us  = (turnDirection > 0) ? SERVO_RIGHT_SAFE_US : SERVO_LEFT_SAFE_US;
      g_corner_speed_pwm = TURN_SPEED_PWM;

      if (framePresent && tf_front_mm < TURN_PANIC_MM) {
        enter(CN_FAIL, now);
        break;
      }
      if (now - stateEnteredMs > TURN_MAX_MS) {
        enter(CN_FAIL, now);
        break;
      }

      float delta = fabsf(wrap180(yaw_deg - turnStartYaw));
      if (delta >= TURN_TARGET_DEG) {
        g_corner_exit_yaw = yaw_deg;
        g_corner_just_exited = true;
        enter(CN_EXIT, now);
      }
      break;
    }

    case CN_EXIT:
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = TURN_SPEED_PWM;
      lockoutUntilMs     = now + TURN_LOCKOUT_MS;
      enter(CN_LOCKOUT, now);
      break;

    case CN_LOCKOUT:
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = OPEN_MAX_PWM;             // caller may override
      if (now >= lockoutUntilMs) enter(CN_ARMED, now);
      break;

    case CN_FAIL:
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = 0;
      // race FSM polls corner_failed() and goes to SAFE_STOP
      break;
  }

  // Note: the just_exited flag is consumed by race_fsm on its next tick;
  // we do NOT clear it here so a single update is observable for one tick.
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V12_MAIN
