#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_estop.h"

static unsigned long bootMs           = 0;
static bool          raceActive       = false;

static bool          rawLow           = false;
static bool          stableLow        = false;
static unsigned long edgeChangeMs     = 0;

static bool          armed            = false;     // press observed, waiting for release
static bool          startRequested   = false;     // single-shot edge before race
static bool          releasedAfterHold = false;    // single-shot edge during race

void estop_init() {
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  bootMs = millis();
}

void estop_set_race_active(bool active) { raceActive = active; }

void estop_update() {
  unsigned long now = millis();
  bool reading = (digitalRead(ESTOP_PIN) == LOW);

  // Boot grace
  if (now - bootMs < ESTOP_BOOT_GRACE_MS) {
    rawLow = reading;
    stableLow = false;
    return;
  }

  // Debounce
  if (reading != rawLow) {
    rawLow = reading;
    edgeChangeMs = now;
  }
  if (now - edgeChangeMs >= (unsigned long)ESTOP_DEBOUNCE_MS_V13) {
    if (rawLow != stableLow) {
      bool wasLow = stableLow;
      stableLow = rawLow;

      if (!raceActive) {
        // Pre-race: arm on press, fire startRequested on release.
        if (!wasLow && stableLow) {
          armed = true;
        } else if (wasLow && !stableLow && armed) {
          armed = false;
          startRequested = true;
        }
      } else {
        // During race: rising edge after a held → release-after-hold event.
        if (wasLow && !stableLow) {
          releasedAfterHold = true;
        }
      }
    }
  }
}

bool estop_start_requested()    { return startRequested; }
bool estop_held()               { return stableLow; }
bool estop_released_after_held(){ return releasedAfterHold; }

void estop_consume_start()   { startRequested = false; }
void estop_consume_release() { releasedAfterHold = false; }

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
