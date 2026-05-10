#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_pid.h"
#include "wro_behavior_open.h"

int g_open_steer_us  = SERVO_CENTER_US;
int g_open_speed_pwm = OPEN_MAX_PWM;

static float headingTarget = 0.0f;
static PidState pidHeading;
static unsigned long lastUpdateMs = 0;
static int   trackDirection = -1;        // +1 = CW, -1 = CCW

void open_init() {
  pid_init(pidHeading,
           HEADING_KP, HEADING_KI, HEADING_KD,
           /*intClamp*/ 150.0f, /*outClamp*/ 350.0f, /*alpha*/ 0.30f);
  headingTarget = 0.0f;
  lastUpdateMs = millis();
}

void open_reset() {
  pid_reset(pidHeading);
  lastUpdateMs = millis();
}

void open_set_target_heading(float deg) {
  headingTarget = deg;
  pid_reset(pidHeading);
}
float open_get_target_heading() { return headingTarget; }

void open_set_direction(int dir) {
  trackDirection = (dir >= 0) ? +1 : -1;
}

void open_update(float yaw_deg, int side_tof_mm) {
  unsigned long now = millis();
  float dt = (now - lastUpdateMs) * 0.001f;
  lastUpdateMs = now;
  if (dt <= 0.0f) dt = 0.001f;

  float err = wrap180(headingTarget - yaw_deg);
  float u   = pid_step(pidHeading, err, dt);

#if HAS_SIDE_TOF
  // Optional side-wall correction. side_tof_mm > 0 means valid reading.
  // Sign depends on lap direction × physical mounting side of the ToF.
  // WALL_TOF_SIDE = +1 when the side sensor is on the right of the chassis,
  // -1 if mounted on the left. Combined with trackDirection, this keeps the
  // same physical sensor pulling the robot back toward its target wall in
  // both CW and CCW races without re-tuning WALL_KP.
  if (side_tof_mm > 0 && side_tof_mm < 9999) {
    float wallErr = (float)(side_tof_mm - WALL_TARGET_MM);
    u += (float)(trackDirection * WALL_TOF_SIDE) * WALL_KP * wallErr;
  }
#else
  (void)side_tof_mm;
#endif

  int us = (int)(SERVO_CENTER_US + u);
  if (us > SERVO_RIGHT_SAFE_US) us = SERVO_RIGHT_SAFE_US;
  if (us < SERVO_LEFT_SAFE_US)  us = SERVO_LEFT_SAFE_US;
  g_open_steer_us  = us;
  g_open_speed_pwm = OPEN_MAX_PWM;
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
