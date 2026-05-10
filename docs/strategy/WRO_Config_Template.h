#pragma once

// ===== Robot Configuration Template =====
// Copy values into your main firmware after validation.

// Servo
#define CFG_SERVO_CENTER      90
#define CFG_SERVO_MAX_RIGHT  135
#define CFG_SERVO_MAX_LEFT    45

// Motor limits
#define CFG_MOTOR_MAX_SPEED  150
#define CFG_MOTOR_TURN_FAST  120
#define CFG_MOTOR_TURN_SLOW   80
#define CFG_MOTOR_MIN_SPEED   35

// Control gains
#define CFG_PID_KP          0.50f
#define CFG_PID_KI          0.001f
#define CFG_PID_KD          0.10f
#define CFG_GYRO_KP         1.20f

// Timeouts and loop
#define CFG_CAMERA_TIMEOUT_MS  500
#define CFG_LOOP_INTERVAL_MS    10

// Blind turn / odometry
#define CFG_ENCODER_TURN_TICKS 3000

// Magnetometer bias (calibrate per robot)
#define CFG_MAG_BIAS_X       0.0f
#define CFG_MAG_BIAS_Y       0.0f

// Race
#define CFG_TARGET_LAPS          3
#define CFG_LAP_DEGREES      360.0f

// Safety
#define CFG_ESTOP_DEBOUNCE_MS    20
#define CFG_FINISH_BLINK_MS     500
