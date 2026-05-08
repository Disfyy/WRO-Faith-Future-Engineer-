#pragma once
/*
 * Sensor bridge — single-owner wrapper around vl53l1x_dual.h.
 *
 * Why this exists:
 *   vl53l1x_dual.h declares `static VL53L1Sensor tfFront;` at file scope, so
 *   every .cpp that includes it gets its own copy. With multi-file
 *   architecture we need exactly ONE owner. This module is that owner;
 *   everyone else uses the accessors below.
 *
 * Same applies if you later wrap as5600_dual_i2c.h or other "static-in-header"
 * drivers.
 *
 * The accessor names (sens_tf_*) are kept identical to the v12 TFMini-S
 * bridge so the rest of the firmware needs no changes.
 */

void  sens_init();        // boots VL53L1X front (+ side if HAS_SIDE_TOF)
void  sens_update();      // call every loop tick

int   sens_tf_front_mm();
bool  sens_tf_front_ok();
int   sens_tf_front_strength();
unsigned long sens_tf_front_last_ms();

int   sens_tf_side_mm();
bool  sens_tf_side_ok();
