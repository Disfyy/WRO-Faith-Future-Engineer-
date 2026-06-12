#pragma once
/*
 * Steering asymmetry trim — scales servo deflection per side.
 *
 * 2026-06-11 corner-weight check: right-rear corner is heavier, left side
 * light, so equal servo deflection gives unequal turn radii. Fix the
 * mechanics first (move the LiPo toward the light side); this gain trims
 * the residual only.
 *
 * Shared by the race firmware (wro_v13_main.cpp writeSteeringUs) and the
 * bench test (diag_bench_test_v13.cpp 'g' check) so the bench exercises
 * the exact code path that races.
 *
 * µs below SERVO_CENTER_US is LEFT lock (1000..1350..1700 in v13), so
 * gainLeft scales the left half, gainRight the right half. 1.00 = off.
 * Callers clamp to the SAFE end-stops AFTER this — a gain can push the
 * command toward the limit but never past it.
 */
#include <math.h>
#include "wro_hw_config_v13.h"

static inline int steer_compensate_us(int us, float gainLeft, float gainRight) {
  int   defl = us - SERVO_CENTER_US;
  float g    = (defl < 0) ? gainLeft : gainRight;
  return SERVO_CENTER_US + (int)lroundf((float)defl * g);
}
