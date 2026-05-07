#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_TFMINI

/*
 * WRO v12 — Test TFMini-S LiDAR on ESP32-S3
 *
 * UART1: RX=GPIO15, TX=GPIO16 → TFMini-S front
 *
 * Serial: 115200 baud
 * IMPORTANT: TFMini-S requires 5V power (NOT 3.3V!)
 */

#include "wro_hw_config_v12.h"
#include "tfmini_s.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v12 TFMini-S TEST ===");
  Serial.print("UART1: RX=GPIO"); Serial.print(TFMINI_FRONT_RX);
  Serial.print("  TX=GPIO"); Serial.println(TFMINI_FRONT_TX);
  Serial.println("POWER: TFMini-S needs 5V — NOT 3.3V!");
  Serial.println();

  tfmini_initAll();

  Serial.println();
  Serial.println("  dist(cm)  dist(mm)  strength  status");
  Serial.println("  ─────────────────────────────────────");
}

void loop() {
  tfmini_readAll();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 200) return;
  lastPrint = millis();

  if (!tfFront.ok) { Serial.println("  FRONT: NOT CONNECTED"); return; }

  Serial.print("  ");
  if (tfFront.distCM >= 0) {
    Serial.print(tfFront.distCM);   Serial.print("cm\t\t");
    Serial.print(tfFront.distMM);   Serial.print("mm\t\t");
    Serial.print(tfFront.strength); Serial.print("\t\t");
    if      (tfFront.distMM < TOF_EMERGENCY_MM) Serial.print("EMERGENCY!");
    else if (tfFront.distMM < TOF_SLOW_DOWN_MM) Serial.print("CLOSE");
    else                                         Serial.print("OK");
  } else {
    Serial.print("NO TARGET\t-\t\t");
    Serial.print(tfFront.strength);
    Serial.print("\t\tWEAK SIGNAL");
  }
  Serial.println();
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_TFMINI
