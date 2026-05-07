#pragma once
/*
 * E-Stop / start button — single GPIO 21, active LOW (INPUT_PULLUP).
 *
 * Behavior:
 *   Before race: press+release = ARM (then RUN on next loop).
 *   During race: held = SAFE_STOP (motor off immediately, NOT through ramp).
 *                released = RESUME previous state (PIDs reset, corner lockout).
 *
 * Boot grace: button presses are ignored for the first ESTOP_BOOT_GRACE_MS
 * after power-on (defends against the button being held during boot).
 *
 * Rule 9.10 / 9.11 compliance: this is the ONE start button.
 */

void  estop_init();
void  estop_update();        // call every loop tick

bool  estop_start_requested();    // edge: true ONCE on press+release before race
bool  estop_held();               // currently pressed (during race → safe stop)
bool  estop_released_after_held();  // edge: true ONCE on release-after-hold

void  estop_consume_start();      // call after honoring start_requested
void  estop_consume_release();    // call after honoring released_after_held

void  estop_set_race_active(bool active);
