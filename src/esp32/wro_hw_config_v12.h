#pragma once
/*
 * WRO Future Engineers — Hardware Configuration v12
 * ESP32-S3-DevKitC-1 (N8R8) + AS5048A (SPI) + TFMini-S (UART)
 *
 * ARCHITECTURE:
 *   I2C  (Wire,  GPIO8/9):    ICM-20948 IMU only  ← zero conflicts!
 *   SPI  (HSPI,  GPIO11-14):  2× AS5048A encoders (CS per encoder)
 *   UART0 (native USB-C):     Debug / telemetry   ← freed by USB-OTG!
 *   UART1 (GPIO15/16):        TFMini-S front
 *   UART2 (GPIO17/18):        OpenMV H7 Plus camera
 *
 * Team Faith | WRO Future Engineers 2026 | v12.0
 */

// ============================================================
//  1.  I2C — IMU only (one clean bus, no mux needed)
// ============================================================
#define I2C_SDA             8
#define I2C_SCL             9
#define ICM20948_ADDRESS    0x68   // AD0 → GND

// ============================================================
//  2.  SPI — AS5048A × 2 (HSPI bus)
// ============================================================
#define SPI_MOSI           11     // Tie to 3.3V — AS5048A doesn't use MOSI for reads
#define SPI_MISO           13
#define SPI_SCK            12
#define ENC_LEFT_CS        10     // Chip Select — left encoder
#define ENC_RIGHT_CS       14     // Chip Select — right encoder
#define SPI_SPEED_HZ       1000000  // 1 MHz safe (AS5048A max 10 MHz)

// AS5048A constants
#define AS5048A_ANGLE_REG  0x3FFF
#define AS5048A_DIAG_REG   0x3FFD
#define AS5048A_RESOLUTION 16384   // 14-bit = 16384 ticks/rev
#define AS5048A_HALF_RES   8192    // For wrap-around handling

// ============================================================
//  3.  UART1 — TFMini-S front
// ============================================================
#define TFMINI_FRONT_RX    15
#define TFMINI_FRONT_TX    16
#define TFMINI_BAUD        115200

// ============================================================
//  4.  UART2 — OpenMV Camera
// ============================================================
#define CAMERA_RX          17
#define CAMERA_TX          18
#define CAMERA_BAUD        115200

// ============================================================
//  5.  TFMini-S side (optional SW serial)
// ============================================================
#define TFMINI_SIDE_RX     47
#define TFMINI_SIDE_TX     48

// ============================================================
//  6.  Motor driver — BTS7960
// ============================================================
#define MOTOR_R_EN         38
#define MOTOR_L_EN         39
#define MOTOR_R_PWM        40     // LEDC Ch0 → forward
#define MOTOR_L_PWM        41     // LEDC Ch1 → reverse

// ============================================================
//  7.  Steering servo
// ============================================================
#define SERVO_PIN          42

// ============================================================
//  8.  E-Stop + LED
// ============================================================
#define ESTOP_PIN          21
#define LED_PIN            2
#define RGB_LED_PIN        48

// ============================================================
//  9.  Servo calibration (microseconds)
// ============================================================
#define SERVO_CENTER       90
#define SERVO_MAX_RIGHT    135
#define SERVO_MAX_LEFT     45
#define SERVO_CENTER_US    1500
#define SERVO_LEFT_US      1000
#define SERVO_RIGHT_US     2000

// ============================================================
// 10.  Motor speed
// ============================================================
#define MOTOR_MAX_SPEED    150
#define MOTOR_TURN_FAST    120
#define MOTOR_TURN_SLOW    80
#define MOTOR_MIN_SPEED    35

// ============================================================
// 11.  Odometry — recalculated for AS5048A 14-bit
//
//   Wheel D=47mm → circumference=147.65mm=14.765cm
//   16384 ticks/rev ÷ 14.765 cm ≈ 1109.6 ticks/cm
//   (was 277 ticks/cm with AS5600 12-bit — 4× improvement)
// ============================================================
#define WHEEL_DIAMETER_MM  47.0f
#define WHEEL_CIRC_MM      (WHEEL_DIAMETER_MM * 3.14159265f)
#define TICKS_PER_REV      AS5048A_RESOLUTION
#define TICKS_PER_CM       (float)(TICKS_PER_REV / (WHEEL_CIRC_MM / 10.0f))

// ============================================================
// 12.  TFMini-S distance thresholds (mm)
// ============================================================
#define TOF_EMERGENCY_MM     120
#define TOF_SLOW_DOWN_MM     400
#define TOF_SIDE_TARGET_MM   100
#define TOF_PARALLEL_TOL_MM  20

// ============================================================
// 13.  Race & PID defaults
// ============================================================
#define TARGET_LAPS        3
#define LAP_DEGREES        360.0f
#define FINISH_ZONE_CM     100

#define PID_KP_DEFAULT     0.50f
#define PID_KI_DEFAULT     0.001f
#define PID_KD_DEFAULT     0.10f
#define GYRO_KP_DEFAULT    1.20f

// ============================================================
// 14.  Timeouts & loop
// ============================================================
#define CAMERA_TIMEOUT           500
#define CAMERA_OFFLINE_STOP_MS   3000
#define PRINT_INTERVAL           200
#define LOOP_INTERVAL            10
#define GYRO_CALIB_SAMPLES       300
#define FINISH_BLINK_MS          250
#define ESTOP_DEBOUNCE_MS        20
#define MAX_IMU_DT_SEC           0.05f
#define ENCODER_FAIL_STOP        50
#define SPEED_RAMP_STEP          8
#define BLIND_TURN_TIMEOUT       3000
#define BLIND_TURN_ANGLE         85.0f
#define ENCODER_TURN_TICKS       3000
