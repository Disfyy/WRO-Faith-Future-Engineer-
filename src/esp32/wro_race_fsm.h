#pragma once
/*
 * Top-level race FSM.
 *
 * Pure logic: takes sensor readings + inputs, decides a single (steer_us,
 * speed_pwm) command per tick, exposes state for telemetry.
 *
 * Transitions (mode-aware):
 *   INIT       → WAIT_START (after IMU + encoders OK + gyro calibrated)
 *   WAIT_START → RUN_OPEN     (button pressed && OBSTACLE_MODE=0)
 *              → RUN_OBSTACLE (button pressed && OBSTACLE_MODE=1)
 *   RUN_*      ↔ corner FSM owns control during turns
 *   RUN_OBSTACLE → PARKING (lap==3 && magenta confirmed)
 *   RUN_OPEN   → FINISH (lap==3 && in finish zone)
 *   ANY        → SAFE_STOP (estop held, IMU dead, encoder dead, cam silent in obstacle)
 *   SAFE_STOP  → previous state (estop released && was running)
 */

enum RaceState {
  RS_INIT       = 0,
  RS_WAIT_START = 1,
  RS_RUN_OPEN   = 2,
  RS_RUN_OBS    = 3,
  RS_TURN       = 4,
  RS_PARKING    = 5,
  RS_FINISH     = 6,
  RS_SAFE_STOP  = 7
};

void  race_init();
void  race_update();    // call every loop tick

// Outputs to actuators (use these in main loop):
extern int g_cmd_steer_us;       // 1000..2000
extern int g_cmd_speed_pwm;      // signed: + forward, − reverse
extern int g_race_state;
extern int g_lap_count;
