#pragma once
/*
 * Generic PID — header-only, host-unit-testable
 *
 * Features:
 *   - Anti-windup via integral clamp (symmetric)
 *   - EMA-filtered derivative (alpha = derivAlpha; 0 = no filter)
 *   - Output clamp (symmetric)
 *   - Reset clears integral, derivative state, prev error
 *
 * Team Faith | WRO Future Engineers 2026 | v12.0
 */

struct PidState {
  float kp, ki, kd;
  float integral;
  float prevError;
  float derivFiltered;
  float derivAlpha;     // 0..1, 0 = no smoothing
  float intClamp;       // ± clamp on integral
  float outClamp;       // ± clamp on output
  bool  hasPrev;
};

static inline void pid_init(PidState &p, float kp, float ki, float kd,
                            float intClamp, float outClamp, float derivAlpha) {
  p.kp = kp; p.ki = ki; p.kd = kd;
  p.integral = 0.0f;
  p.prevError = 0.0f;
  p.derivFiltered = 0.0f;
  p.derivAlpha = derivAlpha;
  p.intClamp = intClamp;
  p.outClamp = outClamp;
  p.hasPrev = false;
}

static inline void pid_reset(PidState &p) {
  p.integral = 0.0f;
  p.prevError = 0.0f;
  p.derivFiltered = 0.0f;
  p.hasPrev = false;
}

// dt in seconds. Returns the (clamped) controller output.
static inline float pid_step(PidState &p, float error, float dt) {
  if (dt <= 0.0f) dt = 0.001f;

  p.integral += error * dt;
  if (p.integral >  p.intClamp) p.integral =  p.intClamp;
  if (p.integral < -p.intClamp) p.integral = -p.intClamp;

  float rawDeriv = 0.0f;
  if (p.hasPrev) rawDeriv = (error - p.prevError) / dt;
  p.prevError = error;
  p.hasPrev = true;

  // EMA: filtered = alpha*raw + (1-alpha)*prev_filtered
  if (p.derivAlpha <= 0.0f) p.derivFiltered = rawDeriv;
  else                      p.derivFiltered =
                              p.derivAlpha * rawDeriv +
                              (1.0f - p.derivAlpha) * p.derivFiltered;

  float out = p.kp * error + p.ki * p.integral + p.kd * p.derivFiltered;
  if (out >  p.outClamp) out =  p.outClamp;
  if (out < -p.outClamp) out = -p.outClamp;
  return out;
}

// Wrap angle to (-180, 180]. Used for heading error computations.
static inline float wrap180(float deg) {
  while (deg >  180.0f) deg -= 360.0f;
  while (deg <= -180.0f) deg += 360.0f;
  return deg;
}

// Snap a heading to the nearest multiple of step (e.g. 90°).
static inline float snapDeg(float deg, float step) {
  float n = (deg + (deg >= 0 ? step/2.0f : -step/2.0f)) / step;
  long  i = (long)n;
  return (float)i * step;
}
