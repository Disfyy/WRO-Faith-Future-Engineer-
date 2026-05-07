#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS

/*
 * WRO v12 — Test AS5048A SPI Encoders on ESP32-S3
 *
 * Wiring: SCK=12, MISO=13, MOSI=11(→3.3V)
 *         Left CS=GPIO10, Right CS=GPIO14
 *
 * Serial: 115200 baud
 * Spin wheels by hand — values should change smoothly.
 * 1 revolution = 16384 ticks. 1 cm ≈ 1110 ticks.
 */

#include "wro_hw_config_v12.h"
#include "as5048a_spi.h"

int  prevLeft  = -1, prevRight  = -1;
long totalLeft = 0,  totalRight = 0;

void updateOdometry(int rawL, int rawR) {
  if (prevLeft < 0) { prevLeft = rawL; prevRight = rawR; return; }
  int dL = rawL - prevLeft;
  int dR = rawR - prevRight;
  if (dL >  AS5048A_HALF_RES) dL -= AS5048A_RESOLUTION;
  if (dL < -AS5048A_HALF_RES) dL += AS5048A_RESOLUTION;
  if (dR >  AS5048A_HALF_RES) dR -= AS5048A_RESOLUTION;
  if (dR < -AS5048A_HALF_RES) dR += AS5048A_RESOLUTION;
  totalLeft  += dL;
  totalRight += dR;
  prevLeft  = rawL;
  prevRight = rawR;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v12 AS5048A ENCODER TEST ===");
  Serial.print("SCK=GPIO"); Serial.print(SPI_SCK);
  Serial.print("  MISO=GPIO"); Serial.print(SPI_MISO);
  Serial.print("  CS_L=GPIO"); Serial.print(ENC_LEFT_CS);
  Serial.print("  CS_R=GPIO"); Serial.println(ENC_RIGHT_CS);

  as5048a_init();

  int tL = as5048a_readAngle(ENC_LEFT_CS);
  int tR = as5048a_readAngle(ENC_RIGHT_CS);
  Serial.print("Left  AS5048A: ");  Serial.println(tL >= 0 ? "FOUND" : "NOT FOUND — check wiring!");
  Serial.print("Right AS5048A: ");  Serial.println(tR >= 0 ? "FOUND" : "NOT FOUND — check wiring!");
  Serial.print("Ticks per cm:  ");  Serial.println(TICKS_PER_CM, 1);
  Serial.println();
  Serial.println("  LEFT (raw / deg / cm)           RIGHT (raw / deg / cm)");
  Serial.println("  ─────────────────────────────   ─────────────────────────────");
}

void loop() {
  int rawL = readEncoderLeft();
  int rawR = readEncoderRight();
  if (rawL >= 0 && rawR >= 0) updateOdometry(rawL, rawR);

  Serial.print("  ");
  if (rawL >= 0) {
    Serial.print(rawL); Serial.print(" / ");
    Serial.print(rawL * 360.0f / AS5048A_RESOLUTION, 1); Serial.print("° / ");
    Serial.print(totalLeft / TICKS_PER_CM, 1); Serial.print("cm");
  } else { Serial.print("ERROR"); }

  Serial.print("          ");

  if (rawR >= 0) {
    Serial.print(rawR); Serial.print(" / ");
    Serial.print(rawR * 360.0f / AS5048A_RESOLUTION, 1); Serial.print("° / ");
    Serial.print(totalRight / TICKS_PER_CM, 1); Serial.print("cm");
  } else { Serial.print("ERROR"); }

  Serial.println();
  delay(200);
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS
