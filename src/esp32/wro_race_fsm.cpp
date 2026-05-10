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
// defaults to CCW which is the team's historical hardcode.
static int  globalDirection = -1;
static bool directionConfirmed = false;

static int  prevRaceState         = RS_INIT;
static int  magentaConfirmStreak  = 0;
static unsigned long lastLapMs    = 0;
static float lastLapYawTotal      = 0.0f;
static unsigned long camLineLastSeen = 0;
static unsigned long brownoutWarnSinceMs = 0;

static void enterState(int s) {
  if (g_race_state != s) {
    prevRaceState = g_race_state;
    g_race_state = s;
  }
}

static void detectDirection() {
  if (directionConfirmed) return;
  if (!g_cam.online) return;
  if (g_cam.modeFlag & CAM_FLAG_ORANGE) {
    globalDirection = +1;        // CW
    directionConfirmed = true;
  } else if (g_cam.modeFlag & CAM_FLAG_BLUE) {
    globalDirection = -1;        // CCW
    directionConfirmed = true;
  }
  corner_set_direction(globalDirection);
}

static void updateLapCounter(unsigned long now) {
  // Primary: gyro 360° accumulator, signed against the locked race direction.
  // - Backward yaw → ilaps decreases → no spurious lap fire.
  // - lastIlaps is monotonic (never regresses) so a brief back-spin then
  //   forward return can't re-credit the same lap once cooldown expires.
  // - If direction never confirmed (stays at -1 default) and the actual race
  //   is CW, ilaps will stay ≤ 0 and no lap fires. Fail-safe over fail-loud.
  float laps  = (g_yaw_total * (float)globalDirection) / GYRO_LAP_DEG;
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
  return g_imu_ok && g_enc_ok;
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
  globalDirection = -1;
  directionConfirmed = false;
  magentaConfirmStreak = 0;
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
      // Resume on release, only if previous state was running.
      if (estop_released_after_held()) {
        estop_consume_release();
        if (prevRaceState == RS_RUN_OPEN || prevRaceState == RS_RUN_OBS ||
            prevRaceState == RS_TURN     || prevRaceState == RS_PARKING) {
          corner_reset();
          open_reset();
          obs_reset();
          enterState(prevRaceState == RS_TURN ? RS_RUN_OPEN : prevRaceState);
        }
      }
      break;
  }

  // Brownout proxy: PWM>0 but speed near zero for too long → warn.
  if (g_cmd_speed_pwm > MIN_DRIVE_PWM && fabsf(g_speed_cm_s) < 5.0f) {
    if (brownoutWarnSinceMs == 0) brownoutWarnSinceMs = now;
    else if (now - brownoutWarnSinceMs > BROWNOUT_PROXY_MS) {
      Serial.println("WARN: motor stalled (brownout proxy)");
      brownoutWarnSinceMs = now;
    }
  } else {
    brownoutWarnSinceMs = 0;
  }

  // Health failsafes
  if (!sensorsHealthy() &&
      g_race_state != RS_INIT && g_race_state != RS_FINISH) {
    g_cmd_speed_pwm = 0;
    enterState(RS_SAFE_STOP);
  }

  (void)lastLapYawTotal;  // set on each lap; reserved for future per-lap drift sanity checks
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
