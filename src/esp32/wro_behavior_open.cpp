#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_pid.h"
#include "wro_telemetry.h"   // live-tune globals (g_gyro_kp, g_max_pwm_open)
#include "wro_behavior_open.h"

int g_open_steer_us  = SERVO_CENTER_US;
int g_open_speed_pwm = OPEN_MAX_PWM;

static float headingTarget = 0.0f;
static PidState pidHeading;
static unsigned long lastUpdateMs = 0;
// Lap direction, kept for telemetry/future segment logic. NOT used in the
// wall trim: that geometry is direction-independent (see wro_config_v13.h).
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

  pidHeading.kp = g_gyro_kp;   // live-tunable over USB ('G' command)

  // SIGN CONVENTION (bench-verify once, wheels off the ground):
  //   +yaw = CCW/left (wro_imu.cpp), but larger servo µs = RIGHT
  //   (SERVO_RIGHT_US = 2000; corner FSM steers RIGHT_SAFE_US for CW turns).
  //   A positive heading error (target is CCW of current yaw → must turn
  //   LEFT) therefore needs a NEGATIVE µs offset — hence the minus sign.
  //   Bench check: rotate the chassis CW by hand → wheels must steer LEFT.
  //   (The old `CENTER + u` form was positive feedback: any heading error
  //   ramped the servo to full lock instead of correcting.)
  float err = wrap180(headingTarget - yaw_deg);
  float u   = -pid_step(pidHeading, err, dt);   // servo-space: + = right

#if HAS_SIDE_TOF
  // Side-wall trim: hold WALL_TARGET_MM to the wall the sensor faces.
  // Direction-independent geometry — too far from that wall (wallErr > 0)
  // → steer toward the sensor's side. WALL_TOF_SIDE (+1 = right-mounted)
  // maps that into servo-space sign. Readings beyond WALL_MAX_VALID_MM are
  // gaps/diagonals in the randomized inner walls, not "our" wall — skip.
  if (side_tof_mm > 0 && side_tof_mm <= WALL_MAX_VALID_MM) {
    float wallErr = (float)(side_tof_mm - WALL_TARGET_MM);
    float wallU   = (float)WALL_TOF_SIDE * WALL_KP * wallErr;
    if (wallU >  WALL_TERM_CLAMP_US) wallU =  WALL_TERM_CLAMP_US;
    if (wallU < -WALL_TERM_CLAMP_US) wallU = -WALL_TERM_CLAMP_US;
    u += wallU;
  }
#else
  (void)side_tof_mm;
#endif

  int us = (int)(SERVO_CENTER_US + u);
  if (us > SERVO_RIGHT_SAFE_US) us = SERVO_RIGHT_SAFE_US;
  if (us < SERVO_LEFT_SAFE_US)  us = SERVO_LEFT_SAFE_US;
  g_open_steer_us  = us;
  g_open_speed_pwm = g_max_pwm_open;   // live-tunable over USB ('S+'/'S-')
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
