#pragma once
/*
 * WRO Future Engineers v13 — Algorithm Tunables
 *
 * Pin map and hardware constants live in wro_hw_config_v13.h.
 * This file holds only behavior gains, thresholds, and the
 * compile-time challenge mode flag (Rule 9.9: no physical switches).
 *
 * v13 = v12 architecture (FSM + behaviors unchanged) but rewired for the
 * original v11 sensor stack: AS5600 (dual I2C) + VL53L1X (XSHUT remap).
 *
 * Team Faith | WRO Future Engineers 2026 | v13.0
 */

#include "wro_hw_config_v13.h"

// ============================================================
// 0.  CHALLENGE MODE — the ONLY compile-time flag (Rule 9.9)
// ============================================================
//   0 = Open Challenge     (no pillars, just walls + 3 laps)
//   1 = Obstacle Challenge (red/green pillars + parking)
#define OBSTACLE_MODE  0

// Side VL53L1X is wired by default in v13 (the sensor exists; mux issues are
// gone). Set to 0 only if you physically disconnect the side sensor.
#define HAS_SIDE_TOF   1

// ============================================================
// 1.  CORNERING — VL53L1X-front + IMU yaw delta state machine
// ============================================================
#define TURN_SLOWDOWN_MM      600    // begin pre-corner slowdown
#define TURN_COMMIT_MM        350    // commit to turning
#define TURN_FRAMES_DEBOUNCE  3      // valid frames before commit
#define TURN_BRAKE_MS         180    // brake-straight phase
#define TURN_SPEED_PWM        70     // PWM during EXECUTE (slip kills v11 turn)
#define TURN_TARGET_DEG       80.0f  // exit angle (under-rotated; heading-hold cleans last 10°)
#define TURN_MAX_MS           2500   // failsafe abort
#define TURN_LOCKOUT_MS       800    // suppress re-detect after exit
#define TURN_PANIC_MM         100    // hard SAFE_STOP if wall this close in EXECUTE

// ============================================================
// 2.  OPEN CHALLENGE — heading-hold + segment dead-reckoning
// ============================================================
#define OPEN_FIRST_SEGMENT_CM   50      // first straight after start
#define OPEN_SEGMENT_CM         90      // typical straight length between corners
#define HEADING_KP              12.0f   // µs per degree of yaw error
#define HEADING_KD              2.0f
#define HEADING_KI              0.0f    // intentionally 0 — drift accumulates over 3 min

// Side-wall PID (only when HAS_SIDE_TOF=1)
#define WALL_TARGET_MM          100
#define WALL_KP                 0.40f   // µs per mm error
#define WALL_KD                 0.05f
// Physical mounting of the side VL53L1X on the chassis:
//   +1 = sensor on the RIGHT side of the robot
//   -1 = sensor on the LEFT  side of the robot
// Combined with the live lap direction this keeps the wall-PID sign correct
// in both CW and CCW races. Verify on first bench run; flip sign if the
// robot pulls AWAY from the wall instead of toward it.
#define WALL_TOF_SIDE          (+1)

// ============================================================
// 3.  OBSTACLE CHALLENGE — pillar avoidance PID
// ============================================================
#define PILLAR_OFFSET_PX        60      // setpoint shift (px) for active pillar
#define PILLAR_KP               0.45f
#define PILLAR_KI               0.001f
#define PILLAR_KD               0.30f
#define PILLAR_INT_CLAMP        400.0f  // anti-windup
#define PILLAR_DERIV_EMA_A      0.30f   // alpha for EMA-filtered derivative
#define PILLAR_OUTPUT_CLAMP_US  350     // ± µs around servo center
#define PILLAR_BLEND_OUT_MS     250     // ms to blend toward heading-hold on dropout
#define PILLAR_FAR_GAIN         0.40f   // pre-position gain from far pillar
#define PILLAR_SAFETY_FRONT_MM  200     // hard brake if wall this close

// ============================================================
// 4.  SPEED PROFILES
// ============================================================
#define OPEN_MAX_PWM            80
#define OBS_MAX_PWM             130     // down from 140 in v11
#define SPEED_RAMP_STEP         8       // PWM units per 10 ms tick
#define MIN_DRIVE_PWM           35      // motor deadband floor

// ============================================================
// 5.  PARKING (Obstacle only, after lap 3)
// ============================================================
#define PARK_APPROACH_PWM       40
#define PARK_REV_PWM            45
#define PARK_PHASE_A_CM         25      // reverse + steer toward bay
#define PARK_PHASE_C_CM         45      // straight back into bay (total)
#define PARK_MAGENTA_CONFIRM    5       // frames of magenta before approach
#define PARK_FRONT_CLEAR_MM     350     // tfFront > this → we are inside the bay
#define PARK_ALIGN_RATE_DPS     2.0f    // °/s threshold for "stable"

// ============================================================
// 6.  LAP COUNTING
// ============================================================
#define TARGET_LAPS_RACE        3
#define GYRO_LAP_DEG            360.0f
#define LAP_COOLDOWN_MS         3000    // min gap between laps
#define LAP_LINE_GRACE_MS       1000    // accept camera-line if gyro fired within this
#define LAP_LINE_WARN_MS        4000    // warn if camera-line alone fires this long

// ============================================================
// 7.  E-STOP / START BUTTON
// ============================================================
#define ESTOP_DEBOUNCE_MS_V13   20
#define ESTOP_BOOT_GRACE_MS     500     // ignore button this long after boot
#define ESTOP_LED_BLINK_IDLE    1000    // ms half-period
#define ESTOP_LED_BLINK_ARMED   200

// ============================================================
// 8.  SAFETY THRESHOLDS
// ============================================================
#define ENC_FAIL_LIMIT_V13      50      // consecutive read failures → SAFE_STOP
#define IMU_FAIL_LIMIT          20
#define TF_SILENT_MS            1000    // no valid VL53L1X frame this long → block corner detect
#define CAM_SILENT_DEGRADE_MS   500     // mark camera offline
#define CAM_SILENT_STOP_MS      3000    // SAFE_STOP in Obstacle if silent this long
#define BROWNOUT_PROXY_MS       500     // PWM>0 but speed<5 cm/s this long → warn

// ============================================================
// 9.  TIMING
// ============================================================
#define LOOP_INTERVAL_MS        10      // main control tick
#define TELEMETRY_INTERVAL_MS   200
#define GYRO_CALIB_SAMPLES_V13  300     // ~3 s @ 10 ms
#define IMU_DT_MAX              0.05f   // s — clamp for yaw integration

// ============================================================
// 10. SERVO SAFETY MARGIN
//     wro_hw_config_v13.h has datasheet defaults (1000/1500/2000 µs).
//     Re-measure with target 7 (TEST_SERVO_CAL); add a margin to avoid
//     stalling the servo against the mechanical end-stops (causes brownout).
// ============================================================
#define SERVO_MARGIN_US         60
#define SERVO_RIGHT_SAFE_US     (SERVO_RIGHT_US - SERVO_MARGIN_US)
#define SERVO_LEFT_SAFE_US      (SERVO_LEFT_US  + SERVO_MARGIN_US)

// ============================================================
// 11. CAMERA SANITY
// ============================================================
#define CAM_DIST_MAX_CM         300     // reject reads above this
#define CAM_DIST_JUMP_MAX_CM    50      // reject frame-to-frame jump above this
