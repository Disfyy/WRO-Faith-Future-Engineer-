#pragma once
/*
 * Telemetry — periodic Serial dump + live PID tuning command parser.
 *
 * Output every TELEMETRY_INTERVAL_MS:
 *   T=12345 ST=RUN_O LAP=2 YAW=-89.4 TGT=-90 DST=L+12340/R+12290 \
 *   TF=412 CAM=R(45,80)/G(-,-) PWM=110 ST_US=1532 K=0.45/0.001/0.30
 *
 * Live commands (newline-terminated over USB Serial):
 *   P0.45  /  I0.001  /  D0.30  /  G1.20  — PID gains
 *   S+ / S-                                 — bump max speed ±5
 *   ?                                        — full state dump
 *   !                                        — software E-Stop request
 */

#include <stdint.h>

void  tlm_init();
void  tlm_update_periodic(int race_state, int corner_state, int lap,
                          int signed_pwm, int steer_us);

// Returns true if a software estop was requested this tick.
bool  tlm_consume_software_estop();
