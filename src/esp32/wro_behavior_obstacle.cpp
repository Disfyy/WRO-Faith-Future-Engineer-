#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_pid.h"
#include "wro_telemetry.h"   // live-tune globals (g_pid_k*_obs, g_gyro_kp, g_max_pwm_obs)
#include "wro_behavior_obstacle.h"

static_assert(PILLAR_BLEND_OUT_MS > 0,
              "PILLAR_BLEND_OUT_MS must be > 0 (divisor in obstacle blend).");

int g_obs_steer_us       = SERVO_CENTER_US;
int g_obs_speed_pwm      = OBS_MAX_PWM;
int g_obs_active_pillar  = 0;

static float    headingTarget    = 0.0f;
static PidState pidPillar;
static PidState pidHeading;
static unsigned long lastUpdateMs = 0;
static unsigned long noPillarSinceMs = 0;
static unsigned long brakeUntilMs    = 0;
static int      lastActivePillar = 0;

void obs_init() {
  pid_init(pidPillar,
           PILLAR_KP, PILLAR_KI, PILLAR_KD,
           PILLAR_INT_CLAMP, (float)PILLAR_OUTPUT_CLAMP_US, PILLAR_DERIV_EMA_A);
  pid_init(pidHeading,
           HEADING_KP, 0.0f, HEADING_KD,
           150.0f, 350.0f, 0.30f);
  headingTarget    = 0.0f;
  noPillarSinceMs  = 0;
  brakeUntilMs     = 0;
  lastActivePillar = 0;
  lastUpdateMs     = millis();
}

void obs_reset() {
  pid_reset(pidPillar);
  pid_reset(pidHeading);
  noPillarSinceMs  = 0;
  brakeUntilMs     = 0;
  lastActivePillar = 0;
  lastUpdateMs     = millis();
}

void obs_set_target_heading(float deg) {
  headingTarget = deg;
  pid_reset(pidHeading);
}

bool obs_update(float yaw_deg, int tf_front_mm, bool tf_front_ok,
                const CameraData &cam) {
  unsigned long now = millis();
  float dt = (now - lastUpdateMs) * 0.001f;
  lastUpdateMs = now;
  if (dt <= 0.0f) dt = 0.001f;

  // Live-tunable over USB (P/I/D/G commands in wro_telemetry.cpp).
  pidPillar.kp  = g_pid_kp_obs;
  pidPillar.ki  = g_pid_ki_obs;
  pidPillar.kd  = g_pid_kd_obs;
  pidHeading.kp = g_gyro_kp;

  // Pillar selection: pick the closer of the two visible pillars.
  bool redSeen   = (cam.redDist   < 999) && cam.online;
  bool greenSeen = (cam.greenDist < 999) && cam.online;

  int  active = 0;     // 0=none, 3=red, 4=green
  int  far    = 0;     // pre-position pillar (the one we're not avoiding)
  if (redSeen && greenSeen) {
    if (cam.redDist <= cam.greenDist) { active = 3; far = 4; }
    else                              { active = 4; far = 3; }
  } else if (redSeen)    { active = 3; }
  else if (greenSeen)    { active = 4; }

  if (active != lastActivePillar) {
    pid_reset(pidPillar);   // reset integral on color switch
    lastActivePillar = active;
  }
  g_obs_active_pillar = active;

  float u = 0.0f;

  if (active != 0) {
    // PASS-SIDE RULE (WRO FE): RED pillar → the VEHICLE passes on the
    // pillar's RIGHT side, so the pillar must be held LEFT of image center
    // (negative X setpoint). GREEN → vehicle passes LEFT → pillar held
    // RIGHT (+). The pre-fix signs were inverted (a misread of "keep
    // right" as the pillar's image position) and passed every sign on the
    // penalized side.
    int activeX, setpoint;
    if (active == 3) { activeX = cam.redX;   setpoint = -PILLAR_OFFSET_PX; }   // red → hold pillar left, pass right
    else             { activeX = cam.greenX; setpoint = +PILLAR_OFFSET_PX; }   // green → hold pillar right, pass left

    float err = (float)(activeX - setpoint);
    u = pid_step(pidPillar, err, dt);

    if (far != 0) {
      int farX = (far == 3) ? cam.redX : cam.greenX;
      int farSet = (far == 3) ? -PILLAR_OFFSET_PX : +PILLAR_OFFSET_PX;
      float fe = (float)(farX - farSet);
      u += PILLAR_FAR_GAIN * (pidPillar.kp * fe);  // contribution from the far pillar
    }
    noPillarSinceMs = 0;
  } else {
    // No pillar visible — blend toward heading-hold over PILLAR_BLEND_OUT_MS.
    if (noPillarSinceMs == 0) {
      noPillarSinceMs = now;
      pid_reset(pidPillar);
    }
    float blend = (float)(now - noPillarSinceMs) / (float)PILLAR_BLEND_OUT_MS;
    if (blend > 1.0f) blend = 1.0f;

    float herr = wrap180(headingTarget - yaw_deg);
    float hu   = pid_step(pidHeading, herr, dt);
    // Minus: heading error is in yaw-space (+ = CCW/left) but u is in
    // servo-space (+ = right). Same convention fix as wro_behavior_open.cpp.
    u = -(blend * hu);   // blend toward heading-hold; pre-blend baseline is 0
  }

  if (u >  PILLAR_OUTPUT_CLAMP_US) u =  PILLAR_OUTPUT_CLAMP_US;
  if (u < -PILLAR_OUTPUT_CLAMP_US) u = -PILLAR_OUTPUT_CLAMP_US;

  int us = (int)(SERVO_CENTER_US + u);
  if (us > SERVO_RIGHT_SAFE_US) us = SERVO_RIGHT_SAFE_US;
  if (us < SERVO_LEFT_SAFE_US)  us = SERVO_LEFT_SAFE_US;
  g_obs_steer_us  = us;
  g_obs_speed_pwm = g_max_pwm_obs;   // live-tunable over USB ('S+'/'S-')

  // Safety override: hard brake if a wall is close (corner FSM hasn't taken over yet).
  bool brakeNow = false;
  if (tf_front_ok && tf_front_mm > 0 && tf_front_mm < PILLAR_SAFETY_FRONT_MM) {
    brakeUntilMs = now + 100;
  }
  if (now < brakeUntilMs) {
    g_obs_speed_pwm = 0;
    brakeNow = true;
  }
  return brakeNow;
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
