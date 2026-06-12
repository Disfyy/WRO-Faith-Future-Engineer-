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
// 0b. DEFAULT RACE DIRECTION (fallback until the camera confirms)
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
// 2026-06-11 run log: matte-black wall first returns valid frames around
// 400-450 mm (flickering with 9999 before that). Waiting down to 350 left
// EXECUTE starting at ~175 mm -> TURN_PANIC abort. Commit on first stable
// sighting instead.
#define TURN_COMMIT_MM        420    // commit to turning
#define TURN_FRAMES_DEBOUNCE  3      // valid frames before commit
#define TURN_BRAKE_MS         100    // brake-straight phase (180 ms ate ~14 cm at 0.8 m/s)
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
// Ignore side readings beyond this: on WRO Open the inner walls are
// randomized, so the side sensor legitimately sees 1–3 m across gaps and
// diagonals. Correcting toward a wall that far away is never useful and
// (unclamped) used to slam the servo to the end-stop mid-straight.
#define WALL_MAX_VALID_MM       400
#define WALL_TERM_CLAMP_US      150     // ± µs cap on the wall-trim term
// Physical mounting of the side VL53L1X on the chassis:
//   +1 = sensor on the RIGHT side of the robot
//   -1 = sensor on the LEFT  side of the robot
// NOTE: the wall-trim sign does NOT depend on lap direction — "too far from
// the wall my sensor faces → steer toward it" is the same geometry in CW and
// CCW. (The old trackDirection factor guaranteed positive feedback in one of
// the two directions.) Verify on first bench run; flip THIS sign only if the
// robot pulls AWAY from the wall instead of toward it.
#define WALL_TOF_SIDE          (+1)

// ============================================================
// 2b. STEERING ASYMMETRY TRIM — weight-imbalance compensation
// ============================================================
// 2026-06-11 corner-weight check: right-rear heavier, left side light —
// equal servo deflection gives unequal turn radii. Fix mechanics first
// (move the LiPo); these gains trim the residual only. Each gain scales
// the µs deflection on its own side of SERVO_CENTER_US (1.00 = off).
// Measure: yaw-rate ratio at fixed deflection, L vs R → gain = ratio.
// Expect 1.10–1.25; NOT 2.0 — that saturates the servo end-stop and
// doubles the effective heading-hold Kp on one side (oscillation).
// Live-tune over USB: "L1.15" / "R1.00" (see wro_telemetry.cpp).
#define STEER_GAIN_LEFT_DEFAULT   1.00f
#define STEER_GAIN_RIGHT_DEFAULT  1.00f

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
// Testing value while front-ToF range on black walls is short (~420 mm):
// slower approach buys reaction distance. Raise back toward 80 with S+ once
// corners are reliable (or after the static range test confirms more range).
#define OPEN_MAX_PWM            65
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
#define PARK_MAGENTA_CONFIRM    5       // consecutive CAMERA FRAMES of magenta before approach
#define PARK_FRONT_CLEAR_MM     350     // tfFront > this → we are inside the bay
#define PARK_ALIGN_RATE_DPS     2.0f    // °/s threshold for "stable"
#define PARK_ALIGN_MAX_MS       4000    // gyro never settles (bias drift after a 3-min
                                        // run) → proceed with reverse anyway instead of
                                        // idling out the match clock in PK_ALIGN

// Safety timeouts on parking phases (NOT phase-completion timers — those
// are encoder-distance based). Each is a failsafe abort for a sensor that
// stopped reporting or a state that can't otherwise terminate. All survive
// an E-Stop pause via park_shift_clock() so a 5 s human pause doesn't
// trip them on resume.
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
// >>> BENCH-ONLY FLAG — MUST be 0 for any run where the robot can move. <<<
//   1 = hardware E-Stop ignored AND the race auto-starts 5 s after boot
//       (for FSM bench tests with the button not wired, wheels OFF ground).
//       Boot log prints a loud warning banner while enabled.
//   0 = normal: physical button on ESTOP_PIN arms/starts/stops the race.
#define ESTOP_BYPASS_AUTOSTART  0
#define ESTOP_DEBOUNCE_MS_V13   20
#define ESTOP_BOOT_GRACE_MS     500     // ignore button this long after boot
#define ESTOP_LED_BLINK_IDLE    1000    // ms half-period
#define ESTOP_LED_BLINK_ARMED   200

// ============================================================
// 7b. WIFI TELEMETRY (TESTING ONLY)
// ============================================================
// >>> TESTING-ONLY FLAG — MUST be 0 for competition rounds. <<<
// Rule 11.10: no Wi-Fi/BT/RF during competition rounds; built-in radios
// must be disabled. This flag IS that disable: 0 compiles the radio out.
//   1 = robot opens its own Wi-Fi hotspot and mirrors every telemetry
//       line over UDP broadcast. Laptop: join the AP, run `nc -ul 3333`.
//   0 = competition: Wi-Fi never initialized, module compiles to no-ops.
#define WIFI_TELEMETRY          1
#define WIFI_TLM_SSID           "WRO-FAITH"
#define WIFI_TLM_PASS           "faith2026"     // min 8 chars (WPA2)
#define WIFI_TLM_CHANNEL        6
#define WIFI_TLM_PORT           3333

// ============================================================
// 8.  SAFETY THRESHOLDS
// ============================================================
#define ENC_FAIL_LIMIT_V13      50      // consecutive read failures → SAFE_STOP
#define IMU_FAIL_LIMIT          20
#define IMU_GYRO_MAX_RPS        9.2f    // reject finite gyro spikes beyond ~500 dps full-scale (+margin)
#define TF_SILENT_MS            1000    // no valid VL53L1X frame this long → block corner detect
#define TF_FRONT_DEAD_MS        3000    // front ToF silent this long mid-race → SAFE_STOP
                                        // (without it the corner FSM never commits and the
                                        // robot drives full-speed into the wall). Must be
                                        // longer than any legitimate >3.5 m diagonal view.
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

// ============================================================
// 12. CAMERA BACKEND
//     2026-06-12: OpenMV OV5640 sensor module damaged 5 days before
//     the competition. Interim camera is a Pixy2/2.1 on the SAME
//     UART2 pins. Both backends fill the same g_cam struct, so the
//     FSM, failsafes and telemetry are backend-agnostic.
//
//     OPENMV — newline text frames (openmv_main.py / espcam_vision.ino)
//     PIXY2  — binary request/response block protocol. Signatures must
//              be taught in PixyMon in THIS order:
//                1 = red pillar      2 = green pillar
//                3 = orange line     4 = blue line
//                5 = magenta parking
//              PixyMon → Configure → Interface: set "Data out port" to
//              UART, baud 115200 (factory default is 19200!).
// ============================================================
#define CAMERA_BACKEND_OPENMV   0
#define CAMERA_BACKEND_PIXY2    1
#define CAMERA_BACKEND          CAMERA_BACKEND_PIXY2   // ← revert to OPENMV if the module is revived

#define PIXY_POLL_MS            20      // getBlocks request period (~50 Hz)
#define PIXY_RESP_TIMEOUT_MS    100     // stalled response → re-request
#define PIXY_FOCAL_PIX          190     // pinhole focal in Pixy px. CALIBRATE:
                                        // red pillar at exactly 50 cm → focal = 5 * block_h
                                        // (190 assumes Pixy2.1 ~80° HFOV; Pixy2 60° ≈ 270)
#define PIXY_MIN_AREA_PILLAR    180     // w*h px² (≈ OpenMV 50 px @QQVGA scaled to 316×208)
#define PIXY_MIN_AREA_LINE      300
#define PIXY_MIN_AREA_MAGENTA   250
#define PIXY_LINE_Y_MIN         130     // floor lines: lower band only (y of 0..207)
#define PIXY_MAGENTA_Y_MIN      70      // parking blocks: mid-lower band
