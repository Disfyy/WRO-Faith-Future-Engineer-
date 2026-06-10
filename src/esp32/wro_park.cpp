#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_pid.h"
#include "wro_park.h"

int g_park_steer_us  = SERVO_CENTER_US;
int g_park_speed_pwm = 0;
int g_park_phase     = PK_IDLE;

static float         parkTargetYaw    = 0.0f;
static long          phaseStartTicks  = 0;
static unsigned long phaseStartMs     = 0;   // no-encoder fallback timing base
static unsigned long stableSinceMs    = 0;
static unsigned long camLossSinceMs   = 0;
static int           bayDir           = +1;   // +1 = bay to the right of robot, -1 left

// Clamp steering to the servo's safe mechanical range. PK_APPROACH/PK_REV_C
// compute SERVO_CENTER_US + HEADING_KP*herr, which overshoots the end-stops on
// a large heading error; previously only writeSteeringUs() guarded that.
static inline int parkClampSteerUs(int us) {
  if (us > SERVO_RIGHT_SAFE_US) us = SERVO_RIGHT_SAFE_US;
  if (us < SERVO_LEFT_SAFE_US)  us = SERVO_LEFT_SAFE_US;
  return us;
}

void park_init() {
  g_park_phase = PK_IDLE;
  g_park_steer_us = SERVO_CENTER_US;
  g_park_speed_pwm = 0;
  parkTargetYaw = 0.0f;
  phaseStartTicks = 0;
  phaseStartMs = 0;
  stableSinceMs = 0;
  camLossSinceMs = 0;
  bayDir = +1;
}

void park_begin(float startYaw) {
  parkTargetYaw = startYaw;
  g_park_phase = PK_APPROACH;
  phaseStartTicks = 0;
  phaseStartMs = millis();   // timer base for the PK_APPROACH watchdog (and E-stop re-anchor)
  stableSinceMs = 0;
  camLossSinceMs = 0;
}

bool park_done()    { return g_park_phase == PK_FINAL; }
bool park_aborted() { return g_park_phase == PK_ABORT; }

void park_shift_clock(unsigned long deltaMs) {
  // Push every wall-clock anchor forward by the paused duration so elapsed-time
  // checks resume where they left off instead of jumping ahead.
  if (phaseStartMs)   phaseStartMs   += deltaMs;
  if (stableSinceMs)  stableSinceMs  += deltaMs;
  if (camLossSinceMs) camLossSinceMs += deltaMs;
}

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
      // Heading-hold while creeping FORWARD; stop at back-wall or when bay is
      // clearly visible. MINUS sign: heading error is yaw-space (+ = CCW/left)
      // but servo µs is + = right — same convention as wro_behavior_open.cpp.
      float herr = wrap180(parkTargetYaw - yaw_deg);
      g_park_steer_us  = parkClampSteerUs((int)(SERVO_CENTER_US - HEADING_KP * herr));
      g_park_speed_pwm = PARK_APPROACH_PWM;

      // Decide bay side from magenta X. `cam.extraTag` carries the magenta
      // block's X-position in OpenMV image coordinates (-IMG_W/2..+IMG_W/2,
      // center-relative; see openmv_main.py and docs/guides/WRO_OpenMV_UART_Protocol.md).
      // Negative = block left of center → bay to the LEFT  (bayDir = -1).
      // Positive = block right of center → bay to the RIGHT (bayDir = +1).
      // Dead-center (extraTag == 0) keeps the LAST decided side instead of
      // defaulting right by coin flip; the marker de-centers as we approach.
      if (magenta && cam.extraTag != 0) bayDir = (cam.extraTag > 0) ? +1 : -1;

      bool nearBackWall = (tf_front_ok && tf_front_mm > 0 && tf_front_mm < 250);
      if (nearBackWall) {
        phaseStartTicks = enc_avg_ticks_signed;
        phaseStartMs    = now;          // PK_ALIGN timeout anchor
        g_park_phase = PK_ALIGN;
        stableSinceMs = 0;
      } else if (now - phaseStartMs >= PARK_APPROACH_MAX_MS) {
        // Front ToF never tripped (dead sensor / lost marker): don't creep into
        // the back wall forever. Abort safely (motor off, FSM declares aborted).
        g_park_phase = PK_ABORT;
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
          phaseStartMs    = now;
          g_park_phase = PK_REV_A;
        }
      } else {
        stableSinceMs = 0;
      }
      // Timeout: if gyro bias has drifted since boot (thermal, 3-min run) the
      // stationary yaw-rate may never read < PARK_ALIGN_RATE_DPS — every other
      // phase has a cap; without this one the robot sits motionless here until
      // the match clock runs out. Proceeding scores better than idling.
      if (g_park_phase == PK_ALIGN && now - phaseStartMs >= PARK_ALIGN_MAX_MS) {
        Serial.println("WARN: PK_ALIGN timeout (gyro never settled) - reversing anyway");
        phaseStartTicks = enc_avg_ticks_signed;
        phaseStartMs    = now;
        g_park_phase = PK_REV_A;
      }
      break;
    }

    case PK_REV_A: {
      // Reverse while steering INTO the bay direction.
      g_park_steer_us  = (bayDir > 0) ? SERVO_RIGHT_SAFE_US : SERVO_LEFT_SAFE_US;
      g_park_speed_pwm = -PARK_REV_PWM;
#if ENCODERS_PRESENT
      long delta = enc_avg_ticks_signed - phaseStartTicks;
      float cm = (float)(-delta) / TICKS_PER_CM;     // reverse distance (positive)
      bool phaseDone = (cm >= PARK_PHASE_A_CM);
#else
      bool phaseDone = (now - phaseStartMs >= PARK_PHASE_A_MS);
#endif
      if (phaseDone) {
        phaseStartTicks = enc_avg_ticks_signed;
        phaseStartMs    = now;
        g_park_phase = PK_REV_B;
      }
      break;
    }

    case PK_REV_B: {
      // Counter-steer until heading is aligned with parkTargetYaw.
      g_park_steer_us  = (bayDir > 0) ? SERVO_LEFT_SAFE_US : SERVO_RIGHT_SAFE_US;
      g_park_speed_pwm = -PARK_REV_PWM;
      float herr = fabsf(wrap180(parkTargetYaw - yaw_deg));
      // Failsafe: if heading never converges (IMU drift, wrong bay, wheel slip)
      // don't reverse indefinitely into a wall — cap the phase by time.
      if (herr < 5.0f || (now - phaseStartMs >= PARK_PHASE_B_MS)) {
        phaseStartTicks = enc_avg_ticks_signed;
        phaseStartMs    = now;
        g_park_phase = PK_REV_C;
      }
      break;
    }

    case PK_REV_C: {
      // Straight back until total reverse > PARK_PHASE_C_CM OR front clears.
      // PLUS sign is CORRECT here (unlike PK_APPROACH): when REVERSING,
      // wheels-right swings the nose left → yaw INCREASES, so the forward
      // steering↔yaw relation is inverted and `+` is the stable sign.
      float herr = wrap180(parkTargetYaw - yaw_deg);
      g_park_steer_us  = parkClampSteerUs((int)(SERVO_CENTER_US + HEADING_KP * herr));
      g_park_speed_pwm = -PARK_REV_PWM;

#if ENCODERS_PRESENT
      long delta = enc_avg_ticks_signed - phaseStartTicks;
      float cm = (float)(-delta) / TICKS_PER_CM;
      bool phaseDone = (cm >= PARK_PHASE_C_CM);
#else
      bool phaseDone = (now - phaseStartMs >= PARK_PHASE_C_MS);
#endif
      bool frontClear = (tf_front_ok && tf_front_mm > PARK_FRONT_CLEAR_MM);
      if (phaseDone || frontClear) {
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

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
