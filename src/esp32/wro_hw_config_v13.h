#pragma once
/*
 * WRO Future Engineers — Hardware Configuration v13
 * ESP32-S3-DevKitC-1 (N8R8) + 2× AS5600 (dual I2C) + 2× VL53L1X (XSHUT addr-remap)
 *
 * What's different from v12:
 *   v12 used AS5048A SPI encoders + TFMini-S UART distance.
 *   v13 reverts to the original v11 sensor stack (AS5600 I2C + VL53L1X I2C)
 *   while keeping the v12 ESP32-S3 main controller (the TCA9548A mux that
 *   v11 used to share one I2C bus is gone — it burned out in service).
 *
 * How the I2C address conflicts are resolved without a mux:
 *   - 2× AS5600 collide on 0x36 → split across the ESP32-S3's two native
 *     I2C peripherals (Wire = I2C0, Wire1 = I2C1). One AS5600 per bus.
 *   - 2× VL53L1X collide on 0x29 → addresses are remapped at boot via
 *     XSHUT pins (only one VL53L1X per bus enabled at a time, gets a new
 *     address, then the next is enabled). VL53L1X address is RAM-only,
 *     so the sequence is repeated every reset.
 *
 * Bus topology:
 *   I2C0 (Wire,  GPIO 8/9):    ICM-20948 (0x68) + AS5600 Left (0x36) +
 *                              VL53L1X Front (0x29 → 0x30)
 *   I2C1 (Wire1, GPIO 3/4):    AS5600 Right (0x36) +
 *                              VL53L1X Side  (0x29 → 0x31)
 *   UART0 (native USB-C):      Debug / telemetry
 *   UART2 (GPIO 17/18):        OpenMV H7 Plus camera
 *
 * Team Faith | WRO Future Engineers 2026 | v13.0
 */

// ============================================================
//  1.  I2C — bus 0 (Wire) and bus 1 (Wire1)
// ============================================================
#define I2C0_SDA            8
#define I2C0_SCL            9
#define I2C1_SDA            3     // moved from GPIO11 (2026-06). NOTE: GPIO3 is an
                                  // ESP32-S3 strapping pin (JTAG sel) — fine for I2C
                                  // in practice; if boot ever misbehaves, use GPIO13.
#define I2C1_SCL            4     // moved from GPIO12
#define I2C_FREQ_HZ         400000

#define ICM20948_ADDRESS    0x68    // I2C0, AD0 → GND
#define AS5600_ADDRESS      0x36    // both encoders, on different buses

// ============================================================
//  2.  VL53L1X — XSHUT pins (active LOW = sensor in reset)
// ============================================================
//   Each VL53L1X powers up at default address 0x29. We hold all sensors
//   in reset, then bring them up one by one and reassign each to a unique
//   address. Addresses below are runtime targets after the boot dance.
#define VL53L1X_FRONT_XSHUT   15      // I2C0
#define VL53L1X_SIDE_XSHUT    16      // I2C1
#define VL53L1X_THIRD_XSHUT   47      // reserved for optional 3rd sensor (I2C0)

#define VL53L1X_DEFAULT_ADDR  0x29
#define VL53L1X_FRONT_ADDR    0x30
#define VL53L1X_SIDE_ADDR     0x31
#define VL53L1X_THIRD_ADDR    0x32

// ============================================================
//  3.  UART2 — OpenMV Camera (UART0 stays on USB-C debug)
// ============================================================
#define CAMERA_RX           17
#define CAMERA_TX           18
#define CAMERA_BAUD         115200

// ============================================================
//  4.  Motor driver — BTS7960
// ============================================================
#define MOTOR_R_EN          38
#define MOTOR_L_EN          39
#define MOTOR_R_PWM         40    // LEDC Ch0 → forward
#define MOTOR_L_PWM         41    // LEDC Ch1 → reverse

// ============================================================
//  5.  Steering servo
// ============================================================
#define SERVO_PIN           42

// ============================================================
//  6.  E-Stop + LEDs
// ============================================================
#define ESTOP_PIN           21
#define LED_PIN             2
#define RGB_LED_PIN         48

// ============================================================
//  7.  Servo calibration (degrees + microseconds)
// ============================================================
#define SERVO_CENTER        90
#define SERVO_MAX_RIGHT     135
#define SERVO_MAX_LEFT      45
// 2026-06-11 bench recal (servo-cal target): travel is now symmetric,
// 350 µs each side. The old LEFT=1050 under-used the left lock by 50 µs —
// part of the "turns weaker one way" complaint, on top of corner weights.
#define SERVO_CENTER_US     1350
#define SERVO_LEFT_US       1000
#define SERVO_RIGHT_US      1700

// ============================================================
//  9.  Odometry — AS5600 12-bit
//
//   Wheel D=47mm → circumference=147.65mm=14.765cm
//   4096 ticks/rev ÷ 14.765 cm ≈ 277.4 ticks/cm
// ============================================================
#define WHEEL_DIAMETER_MM   47.0f
#define WHEEL_CIRC_MM       (WHEEL_DIAMETER_MM * 3.14159265f)
#define AS5600_RESOLUTION   4096
#define AS5600_HALF_RES     2048
#define TICKS_PER_REV       AS5600_RESOLUTION
#define TICKS_PER_CM        (float)(TICKS_PER_REV / (WHEEL_CIRC_MM / 10.0f))

// ============================================================
// 10.  VL53L1X distance thresholds (mm)
//     VL53L1X long mode reaches ~4 m; we operate in MEDIUM mode
//     (50 ms budget, ~3 m practical range, 20 Hz).
// ============================================================
#define TOF_EMERGENCY_MM    120
#define TOF_SLOW_DOWN_MM    400
#define TOF_SIDE_TARGET_MM  100
#define TOF_PARALLEL_TOL_MM 20
#define TOF_MAX_VALID_MM    3500     // reject reads above this (out-of-range)
#define TOF_INVALID_MM      9999     // sentinel for "no valid VL53L1X reading"

// NOTE: v11 leftovers removed in the 2026-06 audit — motor-speed names
// (MOTOR_MAX_SPEED…), PID/lap defaults (PID_*_DEFAULT, TARGET_LAPS,
// LAP_DEGREES, FINISH_ZONE_CM, GYRO_KP_DEFAULT) and the old timeout/loop
// block (CAMERA_TIMEOUT, LOOP_INTERVAL, SPEED_RAMP_STEP, BLIND_TURN_*,
// ENCODER_TURN_TICKS, …) all lived here AND were redefined (or superseded)
// in wro_config_v13.h. The v13 firmware uses the wro_config_v13.h versions
// (OPEN_MAX_PWM/OBS_MAX_PWM, TARGET_LAPS_RACE, GYRO_LAP_DEG, LOOP_INTERVAL_MS,
// SPEED_RAMP_STEP, ENC_FAIL_LIMIT_V13, …). The only remaining references to
// the old names are in src/esp32/legacy/, which keep their own copies.
