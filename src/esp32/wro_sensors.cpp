#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_sensors.h"
#include "vl53l1x_dual.h"   // owned ONLY by this .cpp

void sens_init()       { vl53_initAll(); }
void sens_update()     { vl53_readAll(); }

int  sens_tf_front_mm()        { return tfFront.distMM; }
bool sens_tf_front_ok()        { return tfFront.ok && tfFront.distMM > 0 && tfFront.distMM < 9999; }
int  sens_tf_front_strength()  { return tfFront.strength; }
unsigned long sens_tf_front_last_ms() { return tfFront.lastRead; }

int  sens_tf_side_mm()         { return tfSide.distMM; }
bool sens_tf_side_ok()         { return tfSide.ok && tfSide.distMM > 0 && tfSide.distMM < 9999; }

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
