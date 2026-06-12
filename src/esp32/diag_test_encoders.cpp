#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS

/*
 * WRO v13 — Test 2× AS5600 on dual I2C (ESP32-S3)
 *
 * Wiring:
 *   Left  encoder → I2C0 (Wire):  SDA=GPIO8,  SCL=GPIO9
 *   Right encoder → I2C1 (Wire1): SDA=GPIO3,  SCL=GPIO4
 *
 * Both encoders use the same fixed I2C address (0x36); the dual-bus split
 * is what avoids a collision (no TCA9548A needed).
 *
 * What this target checks, in order:
 *   1. CONNECTION — I2C ACK on both buses
 *   2. MAGNET     — STATUS register (MD/ML/MH) + AGC + magnitude
 *   3. STABILITY  — 15 s full-speed read soak. Keep the wheels STILL and
 *                   wiggle the encoder cables + connectors during the soak;
 *                   every I2C error is counted → STABLE/UNSTABLE verdict.
 *   4. ODOMETRY   — live raw/deg/cm display (spin wheels by hand)
 *
 * Serial: 115200 baud
 * Live commands:
 *   s   re-run the stability soak (wiggle one connector at a time!)
 *   m   re-print magnet health
 *   z   zero odometry totals + error counters
 *   h   help
 *
 * 1 revolution = 4096 ticks. 1 cm ≈ 277 ticks (47 mm wheel).
 */

#include "wro_hw_config_v13.h"
#include "as5600_dual_i2c.h"

#define SOAK_DURATION_MS  15000UL
#define JITTER_WARN_LSB   8        // p-p angle noise while wheel is still

int  prevLeft  = -1, prevRight  = -1;
long totalLeft = 0,  totalRight = 0;

unsigned long liveErrL = 0, liveErrR = 0;    // I2C errors seen in live loop

struct StabilityStats {
  unsigned long reads;
  unsigned long errors;
  unsigned int  maxConsec;       // worst burst of back-to-back errors
  unsigned int  curConsec;
  int refRaw;                    // first good angle — jitter reference
  int jitterMin, jitterMax;      // wrap-aware delta envelope vs refRaw
};

void statsInit(StabilityStats &s) {
  s.reads = s.errors = 0;
  s.maxConsec = s.curConsec = 0;
  s.refRaw = -1;
  s.jitterMin = AS5600_RESOLUTION;
  s.jitterMax = -AS5600_RESOLUTION;
}

void statsUpdate(StabilityStats &s, int raw) {
  s.reads++;
  if (raw < 0) {
    s.errors++;
    s.curConsec++;
    if (s.curConsec > s.maxConsec) s.maxConsec = s.curConsec;
    return;
  }
  s.curConsec = 0;
  if (s.refRaw < 0) { s.refRaw = raw; return; }
  int d = raw - s.refRaw;
  if (d >  AS5600_HALF_RES) d -= AS5600_RESOLUTION;
  if (d < -AS5600_HALF_RES) d += AS5600_RESOLUTION;
  if (d < s.jitterMin) s.jitterMin = d;
  if (d > s.jitterMax) s.jitterMax = d;
}

void updateOdometry(int rawL, int rawR) {
  if (prevLeft < 0) { prevLeft = rawL; prevRight = rawR; return; }
  int dL = rawL - prevLeft;
  int dR = rawR - prevRight;
  if (dL >  AS5600_HALF_RES) dL -= AS5600_RESOLUTION;
  if (dL < -AS5600_HALF_RES) dL += AS5600_RESOLUTION;
  if (dR >  AS5600_HALF_RES) dR -= AS5600_RESOLUTION;
  if (dR < -AS5600_HALF_RES) dR += AS5600_RESOLUTION;
  totalLeft  += dL;
  totalRight += dR;
  prevLeft  = rawL;
  prevRight = rawR;
}

// ─── 2. Magnet health ─────────────────────────────────────────
void printMagnetHealth(const char *name, TwoWire &bus) {
  Serial.print("  "); Serial.print(name); Serial.print(": ");

  int st = as5600_read_status(bus);
  if (st < 0) {
    Serial.println("I2C ERROR — no ACK. Check SDA/SCL/3V3/GND on this bus!");
    return;
  }
  bool md = st & AS5600_STATUS_MD;
  bool ml = st & AS5600_STATUS_ML;
  bool mh = st & AS5600_STATUS_MH;
  int agc = as5600_read_agc(bus);
  int mag = as5600_read_magnitude(bus);

  if      (!md) Serial.print("NO MAGNET detected — wrong magnet type (must be diametric) or gap >3 mm");
  else if (ml)  Serial.print("magnet TOO WEAK — air gap too big, move magnet closer");
  else if (mh)  Serial.print("magnet TOO STRONG — air gap too small, add spacing");
  else          Serial.print("magnet OK");

  Serial.print("  [AGC="); Serial.print(agc);
  Serial.print(" ideal~64 @3.3V, magnitude="); Serial.print(mag);
  Serial.println("]");
}

void printAllMagnetHealth() {
  Serial.println("\n=== MAGNET HEALTH (AS5600 STATUS register) ===");
  printMagnetHealth("LEFT ", Wire);
  printMagnetHealth("RIGHT", Wire1);
  Serial.println();
}

// ─── 3. Connection stability soak ─────────────────────────────
void printSoakReport(const char *name, const StabilityStats &s) {
  Serial.print("  "); Serial.print(name); Serial.print(": ");
  Serial.print(s.reads);  Serial.print(" reads, ");
  Serial.print(s.errors); Serial.print(" errors");
  float rate = (s.reads > 0) ? 100.0f * s.errors / s.reads : 0.0f;
  Serial.print(" ("); Serial.print(rate, 2); Serial.print("%)");
  Serial.print(", worst burst "); Serial.print(s.maxConsec);

  int jitter = (s.jitterMax >= s.jitterMin) ? (s.jitterMax - s.jitterMin) : 0;
  Serial.print(", jitter "); Serial.print(jitter); Serial.println(" LSB p-p");

  Serial.print("         verdict: ");
  if (s.errors == 0)
    Serial.println("STABLE");
  else if (rate >= 0.5f || s.maxConsec >= 3)
    Serial.println("UNSTABLE — reseat/resolder this side's connector and wires!");
  else
    Serial.println("MARGINAL — a few drops; re-run 's' wiggling ONLY this side's cable");

  if (jitter > JITTER_WARN_LSB)
    Serial.println("         note: angle jitter high while still — check magnet gap 0.5-3 mm + centering");
}

void runStabilitySoak() {
  Serial.println("\n=== CONNECTION STABILITY SOAK (15 s) ===");
  Serial.println("Keep the wheels STILL.");
  Serial.println("WIGGLE the encoder cables and connectors during the soak —");
  Serial.println("a loose crimp/joint shows up as error bursts.");
  Serial.print("starting in 3 s ");
  for (int i = 0; i < 3; i++) { delay(1000); Serial.print("."); }
  Serial.println(" GO");

  StabilityStats sL, sR;
  statsInit(sL);
  statsInit(sR);

  unsigned long t0 = millis();
  unsigned long lastTick = t0;
  while (millis() - t0 < SOAK_DURATION_MS) {
    statsUpdate(sL, readEncoderLeft());
    statsUpdate(sR, readEncoderRight());
    if (millis() - lastTick >= 1000) { Serial.print("."); lastTick += 1000; }
  }
  Serial.println(" done\n");

  printSoakReport("LEFT ", sL);
  printSoakReport("RIGHT", sR);
  Serial.println();
}

// ─── Live loop helpers ────────────────────────────────────────
void printLiveHelp() {
  Serial.println("\n  s = stability soak | m = magnet health | z = zero totals | h = help\n");
  Serial.println("  LEFT (raw / deg / cm)           RIGHT (raw / deg / cm)        I2C errs");
  Serial.println("  ─────────────────────────────   ─────────────────────────────  ────────");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WRO v13 AS5600 DUAL-I2C ENCODER TEST ===");
  Serial.print("I2C0 (Left):  SDA=GPIO"); Serial.print(I2C0_SDA);
  Serial.print(" SCL=GPIO"); Serial.println(I2C0_SCL);
  Serial.print("I2C1 (Right): SDA=GPIO"); Serial.print(I2C1_SDA);
  Serial.print(" SCL=GPIO"); Serial.println(I2C1_SCL);

  // 1. CONNECTION
  bool ok = as5600_init();
  Serial.print("init both encoders: "); Serial.println(ok ? "OK" : "FAIL");

  int tL = readEncoderLeft();
  int tR = readEncoderRight();
  Serial.print("Left  AS5600: ");  Serial.println(tL >= 0 ? "FOUND" : "NOT FOUND — check wiring!");
  Serial.print("Right AS5600: ");  Serial.println(tR >= 0 ? "FOUND" : "NOT FOUND — check wiring!");
  Serial.print("Ticks per cm: ");  Serial.println(TICKS_PER_CM, 1);

  // 2. MAGNET
  printAllMagnetHealth();

  // 3. STABILITY
  runStabilitySoak();

  // 4. ODOMETRY (live)
  Serial.println("Live odometry — spin wheels by hand.");
  printLiveHelp();
}

void loop() {
  // live serial commands
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 's': runStabilitySoak();    printLiveHelp(); break;
      case 'm': printAllMagnetHealth(); printLiveHelp(); break;
      case 'z':
        totalLeft = totalRight = 0;
        liveErrL  = liveErrR  = 0;
        prevLeft  = prevRight = -1;
        Serial.println("\n  totals zeroed\n");
        break;
      case 'h': printLiveHelp(); break;
      default: break;   // ignore CR/LF and unknown keys
    }
  }

  int rawL = readEncoderLeft();
  int rawR = readEncoderRight();
  if (rawL < 0) liveErrL++;
  if (rawR < 0) liveErrR++;
  if (rawL >= 0 && rawR >= 0) updateOdometry(rawL, rawR);

  Serial.print("  ");
  if (rawL >= 0) {
    Serial.print(rawL); Serial.print(" / ");
    Serial.print(rawL * 360.0f / AS5600_RESOLUTION, 1); Serial.print("° / ");
    Serial.print(totalLeft / TICKS_PER_CM, 1); Serial.print("cm");
  } else { Serial.print("ERROR"); }

  Serial.print("          ");

  if (rawR >= 0) {
    Serial.print(rawR); Serial.print(" / ");
    Serial.print(rawR * 360.0f / AS5600_RESOLUTION, 1); Serial.print("° / ");
    Serial.print(totalRight / TICKS_PER_CM, 1); Serial.print("cm");
  } else { Serial.print("ERROR"); }

  Serial.print("    L="); Serial.print(liveErrL);
  Serial.print(" R=");    Serial.print(liveErrR);
  Serial.println();
  delay(200);
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_ENCODERS
