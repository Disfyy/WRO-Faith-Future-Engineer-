#pragma once
/*
 * Obstacle Challenge behavior — pillar avoidance via camera + heading fallback.
 *
 *   Red pillar → keep on right (setpoint = +PILLAR_OFFSET_PX)
 *   Green pillar → keep on left (setpoint = -PILLAR_OFFSET_PX)
 *   Both visible → prioritize closer; pre-position from far one (0.4×)
 *   Neither visible → blend toward heading-hold over PILLAR_BLEND_OUT_MS
 *
 * Safety override: when tfFront < PILLAR_SAFETY_FRONT_MM, force speed=0
 * for 100 ms regardless of pillar PID — prevents driving into corners.
 *
 * The corner FSM (wro_corner) runs independently and overrides this when
 * a real wall is in front; pillar PID never tries to drive a turn.
 */

#include "wro_camera.h"

void obs_init();
void obs_reset();
void obs_set_target_heading(float deg);

// Returns: brake-now bool. If true, caller forces speed=0 this tick.
bool obs_update(float yaw_deg, int tf_front_mm, bool tf_front_ok,
                const CameraData &cam);

extern int  g_obs_steer_us;
extern int  g_obs_speed_pwm;
extern int  g_obs_active_pillar;     // 0=none, 3=red, 4=green (matches v11 convention)
