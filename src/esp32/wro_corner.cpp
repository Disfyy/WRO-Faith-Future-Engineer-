#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_pid.h"
#include "wro_camera.h"   // g_cam — orange/blue corner-line slowdown trigger
#include "wro_corner.h"

int  g_corner_steer_us    = SERVO_CENTER_US;
int  g_corner_speed_pwm   = OPEN_MAX_PWM;
int  g_corner_state       = CN_ARMED;
bool g_corner_just_exited = false;

static int           turnDirection      = -1;       // +1 right (CW), -1 left (CCW)
static unsigned long stateEnteredMs     = 0;
static unsigned long lockoutUntilMs     = 0;
static int           validFrameStreak   = 0;        // for COMMIT debounce
static float         turnStartYaw       = 0.0f;
static int           lastTfMm           = TOF_INVALID_MM;
static int           backupTries        = 0;         // reverse legs used in the current turn (3-point)
static int           lineStreak         = 0;         // consecutive camera FRAMES the orange/blue corner line is seen
static uint32_t      lastLineFrame      = 0;         // g_cam.framesOk high-water → debounce per FRAME, not per tick

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
  backupTries        = 0;
  lineStreak         = 0;
  lastLineFrame      = 0;
}

void corner_reset() { corner_init(); }
void corner_set_direction(int dir) { turnDirection = (dir >= 0) ? +1 : -1; }
bool corner_active()  {
  // Own the actuators from the first slowdown tick through turn execution. If
  // SLOWDOWN/COMMIT are excluded, the behavior module overwrites the corner's
  // reduced speed with full PWM and the robot reaches the wall at full speed.
  return g_corner_state == CN_SLOWDOWN || g_corner_state == CN_COMMIT ||
         g_corner_state == CN_BRAKE    || g_corner_state == CN_EXECUTE ||
         g_corner_state == CN_BACK;
}
bool corner_failed()  { return g_corner_state == CN_FAIL; }

void corner_update(float yaw_deg, int tf_front_mm, bool tf_front_ok, unsigned long now) {
  // Frames where the front sensor is unreliable can NOT trigger a turn.
  // (tf_front_mm comes from wro_sensors which now wraps VL53L1X in v13.)
  bool framePresent = tf_front_ok && tf_front_mm > 0 && tf_front_mm < TOF_INVALID_MM;
  if (framePresent) lastTfMm = tf_front_mm;

  // Orange/blue corner line from the camera — an early, high-contrast SLOWDOWN
  // trigger the matte-black wall can't give the ToF. SLOWDOWN-only: COMMIT still
  // needs the ToF wall, so a misread line never fires a turn mid-straight (v11).
#if CORNER_CAMERA_SLOWDOWN
  // Advance the streak ONLY on a new camera frame (g_cam.framesOk changing),
  // never per FSM tick: one frame is latched across several 10 ms ticks — and
  // held indefinitely during a UART stall — so counting ticks would let a
  // SINGLE glitch frame satisfy the debounce. Same per-frame guard the
  // direction vote uses in wro_race_fsm.cpp::detectDirection.
  if (g_cam.framesOk != lastLineFrame) {
    lastLineFrame = g_cam.framesOk;
    bool lineRaw = g_cam.online &&
                   (g_cam.modeFlag & (CAM_FLAG_ORANGE | CAM_FLAG_BLUE)) != 0;
    if (lineRaw) { if (lineStreak < 1000) lineStreak++; } else lineStreak = 0;
  }
  bool lineSeen = lineStreak >= CORNER_LINE_DEBOUNCE;
#else
  bool lineSeen = false;
#endif

  switch (g_corner_state) {

    case CN_ARMED:
      g_corner_steer_us  = SERVO_CENTER_US;          // pass-through to caller
      g_corner_speed_pwm = OPEN_MAX_PWM;             // caller may overwrite outside corner
      if (now > lockoutUntilMs &&
          ((framePresent && tf_front_mm < TURN_SLOWDOWN_MM) || lineSeen)) {
        enter(CN_SLOWDOWN, now);
      }
      break;

    case CN_SLOWDOWN:
      g_corner_steer_us  = SERVO_CENTER_US;
      g_corner_speed_pwm = TURN_SPEED_PWM;
      if (!framePresent) {
        // No wall yet (black wall not in ToF range). Hold the slow approach
        // while the corner line is visible; only bail if BOTH are gone.
        if (!lineSeen && now - stateEnteredMs > 500) enter(CN_ARMED, now);
        break;
      }
      if (tf_front_mm < TURN_COMMIT_MM) {
        validFrameStreak++;
        if (validFrameStreak >= TURN_FRAMES_DEBOUNCE) {
          turnStartYaw = yaw_deg;
          backupTries  = 0;            // fresh turn → reset 3-point counter
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

      float delta = fabsf(wrap180(yaw_deg - turnStartYaw));
      if (delta >= TURN_TARGET_DEG) {
        // Order matters: enter() resets g_corner_just_exited, so we must
        // set the flag AFTER entering CN_EXIT — otherwise race_fsm never
        // sees the exit edge and never snaps the heading target.
        enter(CN_EXIT, now);
        g_corner_just_exited = true;
        break;
      }

      // Wall closing before the turn is finished. If the corner is tighter than
      // the car's turn radius (or a matte-black wall was seen late), one forward
      // arc can't make it — saw backward (CN_BACK) instead of aborting. Give up
      // only after TURN_BACK_MAX_TRIES reverse legs (wrong direction / no room).
      if (TURN_BACKUP_ENABLE && framePresent && tf_front_mm < TURN_BACK_TRIGGER_MM) {
        if (backupTries < TURN_BACK_MAX_TRIES) {
          backupTries++;
          enter(CN_BACK, now);
        } else {
          enter(CN_FAIL, now);
        }
        break;
      }

      // Hard collision floor — normally pre-empted by the back-up trigger above.
      if (framePresent && tf_front_mm < TURN_PANIC_MM) { enter(CN_FAIL, now); break; }
      if (now - stateEnteredMs > TURN_MAX_MS)          { enter(CN_FAIL, now); break; }
      break;
    }

    case CN_BACK: {
      // Reverse leg of a 3-point turn. Counter-steer (opposite of EXECUTE) so
      // yaw keeps rotating the SAME way while backing off the wall, then resume
      // the forward arc in CN_EXECUTE from further out.
      g_corner_steer_us  = (turnDirection > 0) ? SERVO_LEFT_SAFE_US : SERVO_RIGHT_SAFE_US;
      g_corner_speed_pwm = -TURN_SPEED_PWM;   // reverse (signed; main ramps + drives it)

      float delta = fabsf(wrap180(yaw_deg - turnStartYaw));
      if (delta >= TURN_TARGET_DEG) {
        enter(CN_EXIT, now);
        g_corner_just_exited = true;
        break;
      }

      // Collision floor also applies while reversing. EXECUTE checks panic
      // *before* its back-up trigger, so once we're in CN_BACK there is no
      // panic at all unless we repeat it here: if the front keeps closing
      // (reverse not actually moving the car — slip/brownout/wedged), bail
      // instead of grinding into the wall.
      if (framePresent && tf_front_mm < TURN_PANIC_MM) { enter(CN_FAIL, now); break; }

      // "Cleared" only on a REAL far reading. A dropped frame near a matte-black
      // wall is NOT clearance (the wall flickers invalid up close), so a dropout
      // must not kick us forward early — fall through to the time cap instead.
      bool cleared = framePresent && tf_front_mm > TURN_BACK_CLEAR_MM;
      if (cleared || (now - stateEnteredMs > TURN_BACK_MAX_MS)) {
        enter(CN_EXECUTE, now);
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

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
