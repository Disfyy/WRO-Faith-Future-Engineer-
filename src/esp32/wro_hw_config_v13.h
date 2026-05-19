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
 *   I2C1 (Wire1, GPIO 11/12):  AS5600 Right (0x36) +
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
#define I2C1_SDA            11
#define I2C1_SCL            12
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
#define SERVO_CENTER_US     1500
#define SERVO_LEFT_US       1000
#define SERVO_RIGHT_US      2000

// ============================================================
//  8.  Motor speed
// ============================================================
#define MOTOR_MAX_SPEED     150
#define MOTOR_TURN_FAST     120
#define MOTOR_TURN_SLOW     80
#define MOTOR_MIN_SPEED     35

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

// ============================================================
// 11.  Race & PID defaults
// ============================================================
#define TARGET_LAPS         3
#define LAP_DEGREES         360.0f
#define FINISH_ZONE_CM      100

#define PID_KP_DEFAULT      0.50f
#define PID_KI_DEFAULT      0.001f
#define PID_KD_DEFAULT      0.10f
#define GYRO_KP_DEFAULT     1.20f

// ============================================================
// 12.  Timeouts & loop
// ============================================================
#define CAMERA_TIMEOUT          500
#define CAMERA_OFFLINE_STOP_MS  3000
#define PRINT_INTERVAL          200
#define LOOP_INTERVAL           10
#define GYRO_CALIB_SAMPLES      300
#define FINISH_BLINK_MS         250
#define ESTOP_DEBOUNCE_MS       20
#define MAX_IMU_DT_SEC          0.05f
#define ENCODER_FAIL_STOP       50
#define SPEED_RAMP_STEP         8
#define BLIND_TURN_TIMEOUT      3000
#define BLIND_TURN_ANGLE        85.0f
#define ENCODER_TURN_TICKS      3000
