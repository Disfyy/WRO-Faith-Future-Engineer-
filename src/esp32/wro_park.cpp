#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V12_MAIN

#include <Arduino.h>
#include "wro_config_v12.h"
#include "wro_pid.h"
#include "wro_park.h"

int g_park_steer_us  = SERVO_CENTER_US;
int g_park_speed_pwm = 0;
int g_park_phase     = PK_IDLE;

static float         parkTargetYaw    = 0.0f;
static long          phaseStartTicks  = 0;
static unsigned long stableSinceMs    = 0;
static unsigned long camLossSinceMs   = 0;
static int           bayDir           = +1;   // +1 = bay to the right of robot, -1 left

void park_init() {
  g_park_phase = PK_IDLE;
  g_park_steer_us = SERVO_CENTER_US;
  g_park_speed_pwm = 0;
  parkTargetYaw = 0.0f;
  phaseStartTicks = 0;
  stableSinceMs = 0;
  camLossSinceMs = 0;
  bayDir = +1;
}

void park_begin(float startYaw) {
  parkTargetYaw = startYaw;
  g_park_phase = PK_APPROACH;
  phaseStartTicks = 0;
  stableSinceMs = 0;
  camLossSinceMs = 0;
}

bool park_done()    { return g_park_phase == PK_FINAL; }
bool park_aborted() { return g_park_phase == PK_ABORT; }

void park_update(float yaw_deg, float yaw_rate_dps,
                 int tf_front_mm, bool tf_front_ok,
                 long enc_avg_ticks_signed,
                 const CameraData &cam,
                 unsigned long now) {

  bool camValid = cam.online;
  bool magenta  = (cam.modeFlag & CAM_FLAG_MAGENTA) != 0;
  if (!camValid && g_park_phase == PK_APPROACH) {
    if (camLossSinceMs == 0) camLossSinceMs = now;
    if (now - camLossSinceMs > CAM_SILENT_DEGRADE_MS) {
      g_park_phase = PK_ABORT;
    }
  } else {
    camLossSinceMs = 0;
  }

  switch (g_park_phase) {

    case PK_IDLE:
      g_park_steer_us  = SERVO_CENTER_US;
      g_park_speed_pwm = 0;
      break;

    case PK_APPROACH: {
      // Heading-hold while creeping forward; stop at back-wall or when bay is clearly visible.
      float herr = wrap180(parkTargetYaw - yaw_deg);
      g_park_steer_us  = (int)(SERVO_CENTER_US + HEADING_KP * herr);
      g_park_speed_pwm = PARK_APPROACH_PWM;

      // Decide bay side from magenta X (negative = left, positive = right).
      if (magenta) bayDir = (cam.extraTag >= 0) ? +1 : -1;

      bool nearBackWall = (tf_front_ok && tf_front_mm > 0 && tf_front_mm < 250);
      if (nearBackWall) {
        phaseStartTicks = enc_avg_ticks_signed;
        g_park_phase = PK_ALIGN;
        stableSinceMs = 0;
      }
      break;
    }

    case PK_ALIGN: {
      g_park_steer_us  = SERVO_CENTER_US;
      g_park_speed_pwm = 0;
      bool stable = fabsf(yaw_rate_dps) < PARK_ALIGN_RATE_DPS;
      if (stable) {
        if (stableSinceMs == 0) stableSinceMs = now;
        if (now - stableSinceMs > 200) {
          phaseStartTicks = enc_avg_ticks_signed;
          g_park_phase = PK_REV_A;
        }
      } else {
        stableSinceMs = 0;
      }
      break;
    }

    case PK_REV_A: {
      // Reverse while steering INTO the bay direction.
      g_park_steer_us  = (bayDir > 0) ? SERVO_RIGHT_SAFE_US : SERVO_LEFT_SAFE_US;
      g_park_speed_pwm = -PARK_REV_PWM;
      long delta = enc_avg_ticks_signed - phaseStartTicks;
      float cm = (float)(-delta) / TICKS_PER_CM;     // reverse distance (positive)
      if (cm >= PARK_PHASE_A_CM) {
        phaseStartTicks = enc_avg_ticks_signed;
        g_park_phase = PK_REV_B;
      }
      break;
    }

    case PK_REV_B: {
      // Counter-steer until heading is aligned with parkTargetYaw.
      g_park_steer_us  = (bayDir > 0) ? SERVO_LEFT_SAFE_US : SERVO_RIGHT_SAFE_US;
      g_park_speed_pwm = -PARK_REV_PWM;
      float herr = fabsf(wrap180(parkTargetYaw - yaw_deg));
      if (herr < 5.0f) {
        phaseStartTicks = enc_avg_ticks_signed;
        g_park_phase = PK_REV_C;
      }
      break;
    }

    case PK_REV_C: {
      // Straight back until total reverse > PARK_PHASE_C_CM OR front clears.
      float herr = wrap180(parkTargetYaw - yaw_deg);
      g_park_steer_us  = (int)(SERVO_CENTER_US + HEADING_KP * herr);
      g_park_speed_pwm = -PARK_REV_PWM;

      long delta = enc_avg_ticks_signed - phaseStartTicks;
      float cm = (float)(-delta) / TICKS_PER_CM;
      bool frontClear = (tf_front_ok && tf_front_mm > PARK_FRONT_CLEAR_MM);
      if (cm >= PARK_PHASE_C_CM || frontClear) {
        g_park_phase = PK_FINAL;
      }
      break;
    }

    case PK_FINAL:
      g_park_steer_us  = SERVO_CENTER_US;
      g_park_speed_pwm = 0;
      break;

    case PK_ABORT:
      g_park_steer_us  = SERVO_CENTER_US;
      g_park_speed_pwm = 0;
      break;
  }
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V12_MAIN
