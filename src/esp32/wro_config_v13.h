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
// 0b. ENCODER PRESENCE  (TEMPORARY — magnet lost 2026-06-05)
// ============================================================
//   1 = AS5600 magnets installed → full wheel odometry (normal config).
//   0 = magnet(s) missing / on order → run WITHOUT encoders:
//         • the encoder health check no longer gates the race (IMU only),
//         • Obstacle parking uses TIME-based reverse phases (PARK_PHASE_*_MS)
//           instead of encoder distance,
//         • the speed-based brownout proxy is disabled (no speed signal).
//       Open Challenge is otherwise unaffected: lap counting is gyro-based,
//       steering is heading/wall-PID, and corners use ToF + gyro — none of
//       which need the encoders.
//   NOTE: odo_init() is still called in setup() even when this is 0, because
//         it is what runs Wire.begin()/Wire1.begin() for the I2C buses that
//         the IMU and both VL53L1X sensors depend on. Do not remove that call.
//   >>> SET BACK TO 1 once the replacement magnet is glued and the air-gap is
//       verified (0.5–3 mm) with bench target 8 (TEST_ENCODERS). <<<
#define ENCODERS_PRESENT  0

// ============================================================
// 0c. DEFAULT RACE DIRECTION (fallback until the camera confirms)
// ============================================================
//   -1 = CCW (robot drives LEFT from start / counter-clockwise loop)
//   +1 = CW  (robot drives RIGHT from start / clockwise loop)
//   The camera locks the true direction from the orange/blue line within the
//   first few frames; this value is only used until then (e.g. before the
//   first colored line is visible). SET IT to match your known start
//   orientation so that if a corner is reached before direction is confirmed,
//   the robot still turns the right way. The firmware prints a one-shot
//   "UNCONFIRMED direction" warning over USB if that ever happens.
#define DEFAULT_RACE_DIRECTION  (-1)

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
#define PILLAR_OFFSET_PX        30      // setpoint shift (px) for active pillar.
                                        // Camera X is ±80 (cx−80 on a 160px frame), NOT ±160;
                                        // 60 was sized for a ±160 span that never existed.
                                        // RE-TUNE ON TRACK together with PILLAR_KP.
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

// No-encoder fallback (ENCODERS_PRESENT == 0): reverse phases are TIMED
// instead of measured by wheel distance. These are rough equivalents of
// PARK_PHASE_*_CM at PARK_REV_PWM — TUNE ON BENCH with a stopwatch before
// trusting them on track. (An E-Stop pause mid-reverse is handled: the FSM
// shifts the parking clock forward by the paused duration on resume.)
#define PARK_PHASE_A_MS         900     // ≈ PARK_PHASE_A_CM at PARK_REV_PWM
#define PARK_PHASE_C_MS         1600    // ≈ PARK_PHASE_C_CM at PARK_REV_PWM
#define PARK_PHASE_B_MS         1500    // failsafe cap on REV_B counter-steer if heading never converges (drift/wrong bay)
#define PARK_APPROACH_MAX_MS    6000    // abort PK_APPROACH if back-wall never detected (dead front ToF / lost marker)

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
#define IMU_GYRO_MAX_RPS        9.2f    // reject finite gyro spikes beyond ~500 dps full-scale (+margin)
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
#define WDT_TIMEOUT_MS          200     // task watchdog: 20× nominal loop period;
                                        // panic-reset if loop hangs longer than this

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
