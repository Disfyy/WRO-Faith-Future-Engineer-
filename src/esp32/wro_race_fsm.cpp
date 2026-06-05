#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_pid.h"
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

int g_cmd_steer_us  = SERVO_CENTER_US;
int g_cmd_speed_pwm = 0;
int g_race_state    = RS_INIT;
int g_lap_count     = 0;

// Direction: -1 = CCW (drive left from start), +1 = CW (drive right).
// Determined dynamically from camera line color in WAIT_START or first lap;
// defaults to DEFAULT_RACE_DIRECTION (set to your known start orientation in
// wro_config_v13.h) until the camera confirms.
static int  globalDirection = DEFAULT_RACE_DIRECTION;
static bool directionConfirmed = false;
static bool warnedUnconfirmedTurn = false;   // one-shot guard for the warning

// Direction-detection debounce. The OpenMV camera can briefly flag both
// orange and blue at sector boundaries or under threshold ambiguity, so we
// require an EXCLUSIVE color (one bit, not the other) to hold for several
// consecutive frames before locking direction. Without this debounce the
// race could lock to the wrong direction on a stray dual-bit frame.
#define DIRECTION_CONFIRM_FRAMES 4
static int  dirVoteCw  = 0;
static int  dirVoteCcw = 0;

static int  prevRaceState         = RS_INIT;
static int  magentaConfirmStreak  = 0;
static unsigned long safeStopEnteredMs = 0;
static unsigned long lastLapMs    = 0;
static float lastLapYawTotal      = 0.0f;
static unsigned long camLineLastSeen = 0;
static unsigned long brownoutWarnSinceMs = 0;

static void enterState(int s) {
  if (g_race_state != s) {
    prevRaceState = g_race_state;
    g_race_state = s;
    if (s == RS_SAFE_STOP) safeStopEnteredMs = millis();
  }
}

static void detectDirection() {
  if (directionConfirmed) return;
  if (!g_cam.online) return;

  bool orange = (g_cam.modeFlag & CAM_FLAG_ORANGE) != 0;
  bool blue   = (g_cam.modeFlag & CAM_FLAG_BLUE)   != 0;

  // Require exclusivity. If both bits fire (sector boundary, threshold
  // overlap, or a confused frame) the vote is wasted, not credited to
  // whichever bit happens to come first.
  if (orange && !blue) {
    dirVoteCw++;
    dirVoteCcw = 0;
  } else if (blue && !orange) {
    dirVoteCcw++;
    dirVoteCw = 0;
  } else {
    // ambiguous — decay both counters slowly toward zero
    if (dirVoteCw  > 0) dirVoteCw--;
    if (dirVoteCcw > 0) dirVoteCcw--;
  }

  if (dirVoteCw >= DIRECTION_CONFIRM_FRAMES) {
    globalDirection = +1;
    directionConfirmed = true;
  } else if (dirVoteCcw >= DIRECTION_CONFIRM_FRAMES) {
    globalDirection = -1;
    directionConfirmed = true;
  }

  corner_set_direction(globalDirection);
  open_set_direction(globalDirection);
}

// Loud, one-shot notice if a corner maneuver begins while the camera has not
// yet confirmed the race direction. The turn falls back to globalDirection
// (DEFAULT_RACE_DIRECTION); if that default is wrong for the track the robot
// turns into the wall and the corner FSM panics to SAFE_STOP. Worth a warning.
static void warnIfUnconfirmedTurn() {
  if (corner_active() && !directionConfirmed && !warnedUnconfirmedTurn) {
    Serial.println("WARN: corner started with UNCONFIRMED direction - "
                   "using DEFAULT_RACE_DIRECTION (check camera line detection)");
    warnedUnconfirmedTurn = true;
  }
}

static void updateLapCounter(unsigned long now) {
  // Primary: gyro 360-degree accumulator. Count from the MAGNITUDE of total
  // yaw so the locked race direction can't invert the sign. The OLD form
  //   (g_yaw_total * globalDirection)
  // was sign-inverted in BOTH directions (with +yaw=CCW: CW -> neg*(+1),
  // CCW -> pos*(-1)), so ilaps stayed <= 0 and g_lap_count never incremented
  // -> the robot never reached FINISH (Open) or the parking trigger (Obstacle).
  // - lastIlaps is monotonic (never regresses) so a brief back-spin then
  //   forward return can't re-credit the same lap once cooldown expires.
  float laps  = fabsf(g_yaw_total) / GYRO_LAP_DEG;
  int   ilaps = (int)laps;
  static int lastIlaps = 0;
  if (ilaps > lastIlaps && (now - lastLapMs) > LAP_COOLDOWN_MS) {
    g_lap_count++;
    lastLapMs = now;
    lastLapYawTotal = g_yaw_total;
  }
  if (ilaps > lastIlaps) lastIlaps = ilaps;

  // Sanity-check secondary: camera line bits — accept if gyro fired within grace window.
  bool lineNow = (g_cam.modeFlag & (CAM_FLAG_ORANGE | CAM_FLAG_BLUE)) != 0;
  if (lineNow) {
    if (camLineLastSeen == 0) camLineLastSeen = now;
    if (now - lastLapMs < LAP_LINE_GRACE_MS) {
      // gyro lap just fired; line confirms it. Nothing to do.
    } else if (now - camLineLastSeen > LAP_LINE_WARN_MS) {
      Serial.println("WARN: camera line fired without gyro lap");
      camLineLastSeen = now;
    }
  } else {
    camLineLastSeen = 0;
  }
}

static bool sensorsHealthy() {
#if ENCODERS_PRESENT
  return g_imu_ok && g_enc_ok;
#else
  // No-encoder mode: the AS5600s can't report wheel motion without a magnet,
  // so race health rides on the IMU alone. See ENCODERS_PRESENT in
  // wro_config_v13.h. (g_enc_ok is left untouched for telemetry.)
  return g_imu_ok;
#endif
}

static void runStraightOpen(unsigned long now) {
  // Corner FSM owns steering when active.
  if (corner_active()) {
    g_cmd_steer_us  = g_corner_steer_us;
    g_cmd_speed_pwm = g_corner_speed_pwm;
    return;
  }

  // After a corner exit, snap target heading and let open behavior take over.
  if (g_corner_just_exited) {
    float snapped = snapDeg(g_yaw, 90.0f);
    open_set_target_heading(snapped);
    g_corner_just_exited = false;
  }

  open_update(g_yaw, sens_tf_side_ok() ? sens_tf_side_mm() : -1);
  g_cmd_steer_us  = g_open_steer_us;
  g_cmd_speed_pwm = g_open_speed_pwm;
}

static void runStraightObstacle(unsigned long now) {
  if (corner_active()) {
    g_cmd_steer_us  = g_corner_steer_us;
    g_cmd_speed_pwm = g_corner_speed_pwm;
    return;
  }
  if (g_corner_just_exited) {
    float snapped = snapDeg(g_yaw, 90.0f);
    obs_set_target_heading(snapped);
    g_corner_just_exited = false;
  }
  bool brake = obs_update(g_yaw, sens_tf_front_mm(), sens_tf_front_ok(), g_cam);
  g_cmd_steer_us  = g_obs_steer_us;
  g_cmd_speed_pwm = brake ? 0 : g_obs_speed_pwm;
}

static void runParking(unsigned long now) {
  park_update(g_yaw, g_yaw_rate,
              sens_tf_front_mm(), sens_tf_front_ok(),
              odo_avg_dist_ticks(),
              g_cam, now);
  g_cmd_steer_us  = g_park_steer_us;
  g_cmd_speed_pwm = g_park_speed_pwm;
  if (park_done() || park_aborted()) enterState(RS_FINISH);
}

void race_init() {
  g_race_state = RS_INIT;
  g_cmd_steer_us  = SERVO_CENTER_US;
  g_cmd_speed_pwm = 0;
  g_lap_count = 0;
  globalDirection = DEFAULT_RACE_DIRECTION;
  directionConfirmed = false;
  warnedUnconfirmedTurn = false;
  dirVoteCw  = 0;
  dirVoteCcw = 0;
  magentaConfirmStreak = 0;
  safeStopEnteredMs = 0;
  lastLapMs = 0;
  lastLapYawTotal = 0.0f;
  camLineLastSeen = 0;
  brownoutWarnSinceMs = 0;
  prevRaceState = RS_INIT;
}

void race_update() {
  unsigned long now = millis();

  // Software E-Stop from telemetry command
  if (tlm_consume_software_estop()) {
    if (g_race_state != RS_SAFE_STOP && g_race_state != RS_FINISH) {
      g_cmd_speed_pwm = 0;
      enterState(RS_SAFE_STOP);
    }
  }

  // E-Stop button (always observed)
  if (estop_held()) {
    if (g_race_state == RS_RUN_OPEN || g_race_state == RS_RUN_OBS ||
        g_race_state == RS_TURN     || g_race_state == RS_PARKING) {
      g_cmd_steer_us  = SERVO_CENTER_US;
      g_cmd_speed_pwm = 0;
      // remember we were running
      enterState(RS_SAFE_STOP);
    }
  }

  switch (g_race_state) {

    case RS_INIT:
      g_cmd_steer_us  = SERVO_CENTER_US;
      g_cmd_speed_pwm = 0;
      if (sensorsHealthy()) enterState(RS_WAIT_START);
      break;

    case RS_WAIT_START:
      g_cmd_steer_us  = SERVO_CENTER_US;
      g_cmd_speed_pwm = 0;
      detectDirection();    // optional pre-roll while waiting
      if (estop_start_requested()) {
        estop_consume_start();
        estop_set_race_active(true);
        odo_reset();
        g_lap_count = 0;
        // Snap initial target heading to current yaw (whatever the start orientation is).
        float h0 = snapDeg(g_yaw, 90.0f);
        open_set_target_heading(h0);
        obs_set_target_heading(h0);
        corner_reset();
        corner_set_direction(globalDirection);
        open_set_direction(globalDirection);
#if OBSTACLE_MODE == 1
        enterState(RS_RUN_OBS);
#else
        enterState(RS_RUN_OPEN);
#endif
      }
      break;

    case RS_RUN_OPEN: {
      detectDirection();
      corner_update(g_yaw, sens_tf_front_mm(), sens_tf_front_ok(), now);
      warnIfUnconfirmedTurn();
      if (corner_failed()) { enterState(RS_SAFE_STOP); break; }
      runStraightOpen(now);
      updateLapCounter(now);

      // Open finish: 3 laps done, robot is heading-snapped → just stop.
      // Conservative: stop on lap_count >= TARGET (judges grant points for
      // any stop within finish section after 3 laps). Real-track tuning
      // can refine this with odometry-since-last-corner.
      if (g_lap_count >= TARGET_LAPS_RACE) enterState(RS_FINISH);
      break;
    }

    case RS_RUN_OBS: {
      detectDirection();
      corner_update(g_yaw, sens_tf_front_mm(), sens_tf_front_ok(), now);
      warnIfUnconfirmedTurn();
      if (corner_failed()) { enterState(RS_SAFE_STOP); break; }

      // Camera silence escalation in Obstacle mode.
      if (!g_cam.online) {
        if (g_cam.lastValidMs == 0 ||
            (now - g_cam.lastValidMs) > CAM_SILENT_STOP_MS) {
          enterState(RS_SAFE_STOP);
          break;
        }
      }

      runStraightObstacle(now);
      updateLapCounter(now);

      // Parking trigger: lap 3 done AND magenta seen N+ frames in a row.
      if (g_lap_count >= TARGET_LAPS_RACE) {
        bool magenta = (g_cam.modeFlag & CAM_FLAG_MAGENTA) != 0;
        if (magenta) magentaConfirmStreak++;
        else         magentaConfirmStreak = 0;
        if (magentaConfirmStreak >= PARK_MAGENTA_CONFIRM) {
          park_init();
          park_begin(snapDeg(g_yaw, 90.0f));
          enterState(RS_PARKING);
        }
      }
      break;
    }

    case RS_PARKING:
      runParking(now);
      break;

    case RS_FINISH:
      g_cmd_steer_us  = SERVO_CENTER_US;
      g_cmd_speed_pwm = 0;
      estop_set_race_active(false);
      break;

    case RS_SAFE_STOP:
      g_cmd_steer_us  = SERVO_CENTER_US;
      g_cmd_speed_pwm = 0;
      // Resume on release. Recovery rules:
      //   - From a running state (RS_RUN_*/RS_PARKING) → resume that state.
      //   - From RS_TURN (reserved; not entered today) → resume the race mode
      //     that was actually selected, not a hardcoded RS_RUN_OPEN.
      //   - From RS_INIT/RS_WAIT_START → drop back to RS_WAIT_START so an
      //     operator who hit E-Stop before pressing start isn't trapped in
      //     SAFE_STOP requiring a power cycle.
      if (estop_released_after_held()) {
        estop_consume_release();
        int resumeTo = prevRaceState;
        if (resumeTo == RS_TURN) {
#if OBSTACLE_MODE == 1
          resumeTo = RS_RUN_OBS;
#else
          resumeTo = RS_RUN_OPEN;
#endif
        }
        if (resumeTo == RS_INIT || resumeTo == RS_WAIT_START) {
          enterState(RS_WAIT_START);
        } else if (resumeTo == RS_RUN_OPEN || resumeTo == RS_RUN_OBS ||
                   resumeTo == RS_PARKING) {
          corner_reset();
          open_reset();
          obs_reset();
          // Resuming a parking maneuver: re-anchor its timers so the no-encoder
          // (time-based) reverse phases account for the pause instead of ending
          // early. now == millis() at the top of race_update().
          if (resumeTo == RS_PARKING) park_shift_clock(now - safeStopEnteredMs);
          enterState(resumeTo);
        }
        // else: unknown prev state → remain in RS_SAFE_STOP (defensive).
      }
      break;
  }

#if ENCODERS_PRESENT
  // Brownout proxy: |PWM|>deadband but speed near zero for too long -> warn (covers reverse).
  // Needs a real wheel-speed signal, so it is disabled in no-encoder mode.
  if (abs(g_cmd_speed_pwm) > MIN_DRIVE_PWM && fabsf(g_speed_cm_s) < 5.0f) {
    if (brownoutWarnSinceMs == 0) brownoutWarnSinceMs = now;
    else if (now - brownoutWarnSinceMs > BROWNOUT_PROXY_MS) {
      Serial.println("WARN: motor stalled (brownout proxy)");
      brownoutWarnSinceMs = now;
    }
  } else {
    brownoutWarnSinceMs = 0;
  }
#else
  (void)brownoutWarnSinceMs;   // speed signal needs encoders; proxy disabled
#endif

  // Health failsafes
  if (!sensorsHealthy() &&
      g_race_state != RS_INIT && g_race_state != RS_FINISH) {
    g_cmd_speed_pwm = 0;
    enterState(RS_SAFE_STOP);
  }

  (void)lastLapYawTotal;  // set on each lap; reserved for future per-lap drift sanity checks
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
