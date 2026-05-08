#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "as5600_dual_i2c.h"
#include "wro_odometry.h"

// Flip these if a wheel reads backwards on the bench.
static const int ENC_LEFT_SIGN  = +1;
static const int ENC_RIGHT_SIGN = +1;

long  g_dist_left_ticks  = 0;
long  g_dist_right_ticks = 0;
float g_speed_cm_s       = 0.0f;
bool  g_enc_ok           = false;

static int  prevLeft  = 0;
static int  prevRight = 0;
static int  failStreak = 0;
static unsigned long lastUpdateMs = 0;
static long  lastAvgTicks = 0;

bool odo_init() {
  if (!as5600_init()) {
    g_enc_ok = false;
    return false;
  }
  int rL = readEncoderLeft();
  int rR = readEncoderRight();
  if (rL < 0 || rR < 0) {
    g_enc_ok = false;
    return false;
  }
  prevLeft  = rL;
  prevRight = rR;
  g_dist_left_ticks  = 0;
  g_dist_right_ticks = 0;
  g_speed_cm_s = 0.0f;
  g_enc_ok = true;
  failStreak = 0;
  lastUpdateMs = millis();
  return true;
}

void odo_reset() {
  g_dist_left_ticks  = 0;
  g_dist_right_ticks = 0;
  lastAvgTicks = 0;
  g_speed_cm_s = 0.0f;
}

static inline long unwrap12(int now, int prev) {
  long d = (long)now - (long)prev;
  if (d >  AS5600_HALF_RES) d -= AS5600_RESOLUTION;
  if (d < -AS5600_HALF_RES) d += AS5600_RESOLUTION;
  return d;
}

void odo_update() {
  int rL = readEncoderLeft();
  int rR = readEncoderRight();

  if (rL < 0 || rR < 0) {
    if (++failStreak >= ENC_FAIL_LIMIT_V13) g_enc_ok = false;
    return;
  }
  failStreak = 0;

  long dL = unwrap12(rL, prevLeft)  * ENC_LEFT_SIGN;
  long dR = unwrap12(rR, prevRight) * ENC_RIGHT_SIGN;
  prevLeft  = rL;
  prevRight = rR;

  g_dist_left_ticks  += dL;
  g_dist_right_ticks += dR;

  unsigned long now = millis();
  float dt = (now - lastUpdateMs) * 0.001f;
  lastUpdateMs = now;
  if (dt > 0.0f) {
    long avg = (g_dist_left_ticks + g_dist_right_ticks) / 2;
    long dAvg = avg - lastAvgTicks;
    lastAvgTicks = avg;
    g_speed_cm_s = (float)dAvg / TICKS_PER_CM / dt;
  }
}

long  odo_avg_dist_ticks() { return (g_dist_left_ticks + g_dist_right_ticks) / 2; }
float odo_avg_dist_cm()    { return odo_avg_dist_ticks() / TICKS_PER_CM; }
bool  odo_is_healthy()     { return g_enc_ok; }

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
