#pragma once
/*
 * Parking sub-FSM — Obstacle Challenge only, after lap 3.
 *
 * Trigger: lap == 3 AND camera reports magenta blocks for ≥ PARK_MAGENTA_CONFIRM
 * consecutive frames. Race FSM passes control here.
 *
 * Phases:
 *   APPROACH — drive forward heading-locked until front clear-to-bay or
 *              tfFront < 250 mm (close to back wall).
 *   ALIGN    — stop, hold heading until yaw rate < PARK_ALIGN_RATE_DPS for 200 ms.
 *   REVERSE  — three-phase reverse:
 *                A: reverse + steer toward bay (PARK_PHASE_A_CM)
 *                B: counter-steer until heading aligned with parkTargetYaw
 *                C: straight back until total reverse > PARK_PHASE_C_CM OR
 *                   tfFront > PARK_FRONT_CLEAR_MM (we're inside the bay).
 *   FINAL    — motors off, race FSM transitions to FINISH.
 *
 * If camera goes silent during APPROACH, abort parking → FINISH outside the bay.
 */

#include "wro_camera.h"

enum ParkPhase {
  PK_IDLE,
  PK_APPROACH,
  PK_ALIGN,
  PK_REV_A,
  PK_REV_B,
  PK_REV_C,
  PK_FINAL,
  PK_ABORT
};

void  park_init();
void  park_begin(float startYaw);

// Re-anchor the parking wall-clock timers forward by deltaMs. Call on resume
// from an E-Stop pause so TIME-based reverse phases (no-encoder mode) don't end
// early because millis() advanced while parking was suspended. Harmless when
// encoders are present (those phases are distance-based).
void  park_shift_clock(unsigned long deltaMs);

// Returns true when parking is complete or aborted (race FSM should transition).
bool  park_done();
bool  park_aborted();

// Update step. Returns brakeNow (force speed=0 on caller this tick).
void  park_update(float yaw_deg, float yaw_rate_dps,
                  int tf_front_mm, bool tf_front_ok,
                  long enc_avg_ticks_signed,
                  const CameraData &cam,
                  unsigned long now_ms);

// Outputs:
extern int g_park_steer_us;
extern int g_park_speed_pwm;     // signed: + forward, − reverse
extern int g_park_phase;         // for telemetry
