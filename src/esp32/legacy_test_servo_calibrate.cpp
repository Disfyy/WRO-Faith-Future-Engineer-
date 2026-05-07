/*
 * WRO Future Engineers — Servo Calibration Tool
 *
 * PURPOSE:
 *   Find the true microsecond center of your JX PDI-6221MG steering servo,
 *   measure its mechanical limits, and quantify hysteresis (the position
 *   error when the servo approaches the same angle from different directions).
 *
 * HOW TO USE:
 *   1. Upload to ESP32 with WRO_ACTIVE_TARGET = WRO_TARGET_TEST_SERVO_CAL
 *   2. Open Serial Monitor at 115200 baud.
 *   3. Follow the on-screen menu — it walks you through every step.
 *
 * SERIAL COMMANDS (single character + optional number + Enter):
 *   c            — print current microsecond value
 *   +  / -       — nudge by FINE_STEP   (default  5 µs)
 *   >  / <       — nudge by COARSE_STEP (default 50 µs)
 *   s<number>    — jump to an exact µs value, e.g.  s1520
 *   w            — run full-sweep test (centre → right → centre → left → centre)
 *   h            — run hysteresis test (approach centre from both sides, 10×)
 *   r            — run repeatability test (go to extremes and back, 20 cycles)
 *   t            — quick trim: interactive binary-search for true mechanical centre
 *   p            — print summary of all saved calibration values
 *   ?            — show this help
 *
 * HARDWARE:
 *   Servo signal → GPIO 27, powered from BEC / regulator (NOT the ESP32 pin).
 *   Wheels should be ON THE GROUND or the chassis lifted — either way,
 *   make sure you can see the wheel angle clearly.
 */

#if __has_include("wro_build_target.h")
#include "wro_build_target.h"
#else
#define WRO_TARGET_TEST_SERVO_CAL 7
#define WRO_ACTIVE_TARGET WRO_TARGET_TEST_SERVO_CAL
#endif

#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_SERVO_CAL

#include <Arduino.h>
#include <ESP32Servo.h>

// ── Hardware ────────────────────────────────────────────────
constexpr int SERVO_PIN      = 27;
constexpr int SERVO_MIN_US   = 500;   // absolute safe floor (µs)
constexpr int SERVO_MAX_US   = 2500;  // absolute safe ceiling (µs)

// ── Nudge sizes ─────────────────────────────────────────────
constexpr int FINE_STEP      = 5;     // µs per '+' or '-' press
constexpr int COARSE_STEP    = 50;    // µs per '>' or '<' press

// ── Sweep / test timing ─────────────────────────────────────
constexpr int SWEEP_STEP_US  = 10;    // µs increment during sweep
constexpr int SWEEP_DELAY_MS = 30;    // ms per step (controls sweep speed)
constexpr int SETTLE_MS      = 400;   // time to let the servo physically stop

// ── State ───────────────────────────────────────────────────
Servo steeringServo;
int   currentUS = 1565;               // starting assumption

// Saved calibration values (user fills these in during the session).
int   calCenter   = 0;
int   calMaxRight = 0;
int   calMaxLeft  = 0;
int   hysteresisFromRight = 0;
int   hysteresisFromLeft  = 0;

// ═════════════════════════════════════════════════════════════
//  Helpers
// ═════════════════════════════════════════════════════════════

void setServoUS(int us) {
  us = constrain(us, SERVO_MIN_US, SERVO_MAX_US);
  currentUS = us;
  steeringServo.writeMicroseconds(us);
}

void printPosition() {
  Serial.print("  >> servo = ");
  Serial.print(currentUS);
  Serial.println(" µs");
}

void smoothMove(int fromUS, int toUS) {
  int step = (toUS > fromUS) ? SWEEP_STEP_US : -SWEEP_STEP_US;
  for (int us = fromUS; (step > 0) ? (us <= toUS) : (us >= toUS); us += step) {
    setServoUS(us);
    delay(SWEEP_DELAY_MS);
  }
  setServoUS(toUS);
  delay(SETTLE_MS);
}

// ═════════════════════════════════════════════════════════════
//  FULL SWEEP TEST
// ═════════════════════════════════════════════════════════════
void runSweepTest() {
  int center = (calCenter > 0) ? calCenter : 1565;
  int maxR   = (calMaxRight > 0) ? calMaxRight : 1915;
  int maxL   = (calMaxLeft  > 0) ? calMaxLeft  : 1215;

  Serial.println("\n=== FULL SWEEP TEST ===");
  Serial.println("Moving: centre → full right → centre → full left → centre");

  Serial.print("  Centre ("); Serial.print(center); Serial.println(" µs)...");
  smoothMove(currentUS, center);
  delay(500);

  Serial.print("  Full right ("); Serial.print(maxR); Serial.println(" µs)...");
  smoothMove(center, maxR);
  delay(500);

  Serial.print("  Back to centre ("); Serial.print(center); Serial.println(" µs)...");
  smoothMove(maxR, center);
  delay(500);

  Serial.print("  Full left ("); Serial.print(maxL); Serial.println(" µs)...");
  smoothMove(center, maxL);
  delay(500);

  Serial.print("  Back to centre ("); Serial.print(center); Serial.println(" µs)...");
  smoothMove(maxL, center);

  Serial.println("=== SWEEP COMPLETE ===");
  Serial.println("  Look at the wheel: is it perfectly straight now?");
  Serial.println("  If NOT, use +/- to nudge until it is, then type 't' to save.\n");
}

// ═════════════════════════════════════════════════════════════
//  HYSTERESIS TEST
// ═════════════════════════════════════════════════════════════
void runHysteresisTest() {
  int center = (calCenter > 0) ? calCenter : 1500;
  int offset = 300;  // how far away we go before returning

  Serial.println("\n=== HYSTERESIS TEST (10 cycles) ===");
  Serial.println("  This moves right→centre and left→centre repeatedly.");
  Serial.println("  Watch the wheel at each 'centre' stop: does it land the same?");
  Serial.println();

  for (int i = 0; i < 10; i++) {
    // Approach centre from the RIGHT
    smoothMove(currentUS, center + offset);
    delay(200);
    smoothMove(center + offset, center);
    Serial.print("  Cycle "); Serial.print(i + 1);
    Serial.print("/10 — approached from RIGHT → ");
    Serial.print(center); Serial.println(" µs.  Mark the wheel position.");
    delay(1500);

    // Approach centre from the LEFT
    smoothMove(currentUS, center - offset);
    delay(200);
    smoothMove(center - offset, center);
    Serial.print("  Cycle "); Serial.print(i + 1);
    Serial.print("/10 — approached from LEFT  → ");
    Serial.print(center); Serial.println(" µs.  Compare to previous.");
    delay(1500);
  }

  Serial.println("\n=== HYSTERESIS TEST COMPLETE ===");
  Serial.println("  If the wheel lands at different spots depending on direction,");
  Serial.println("  that's mechanical backlash. Typical: 5-20 µs (0.5-2°).");
  Serial.println("  Note the gap and enter it when prompted.\n");
}

// ═════════════════════════════════════════════════════════════
//  REPEATABILITY TEST
// ═════════════════════════════════════════════════════════════
void runRepeatabilityTest() {
  int center = (calCenter > 0) ? calCenter : 1565;
  int maxR   = (calMaxRight > 0) ? calMaxRight : 1915;
  int maxL   = (calMaxLeft  > 0) ? calMaxLeft  : 1215;

  Serial.println("\n=== REPEATABILITY TEST (20 cycles) ===");
  Serial.println("  Full right → centre → full left → centre, 20 times.");
  Serial.println("  Watch for drift or inconsistency at centre.\n");

  for (int i = 0; i < 20; i++) {
    smoothMove(currentUS, maxR);
    smoothMove(maxR, center);
    delay(300);
    smoothMove(center, maxL);
    smoothMove(maxL, center);
    delay(300);

    Serial.print("  Cycle "); Serial.print(i + 1);
    Serial.println("/20 done.");
  }

  Serial.println("\n=== REPEATABILITY TEST COMPLETE ===\n");
}

// ═════════════════════════════════════════════════════════════
//  INTERACTIVE CENTRE-FINDING (binary search feel)
// ═════════════════════════════════════════════════════════════
void runTrimCentre() {
  Serial.println("\n=== INTERACTIVE CENTRE TRIM ===");
  Serial.println("  Use +/- (5 µs) and >/< (50 µs) until wheels are PERFECTLY straight.");
  Serial.println("  Then type 'x' to save this value as your calibrated centre.\n");

  while (true) {
    if (!Serial.available()) continue;

    char ch = Serial.read();
    switch (ch) {
      case '+': setServoUS(currentUS + FINE_STEP);   printPosition(); break;
      case '-': setServoUS(currentUS - FINE_STEP);   printPosition(); break;
      case '>': setServoUS(currentUS + COARSE_STEP); printPosition(); break;
      case '<': setServoUS(currentUS - COARSE_STEP); printPosition(); break;
      case 'x': case 'X':
        calCenter = currentUS;
        Serial.print("\n  ** CENTRE SAVED: ");
        Serial.print(calCenter);
        Serial.println(" µs **\n");
        return;
      case 'c': printPosition(); break;
      default: break;
    }
  }
}

// ═════════════════════════════════════════════════════════════
//  Print saved calibration summary
// ═════════════════════════════════════════════════════════════
void printCalibrationSummary() {
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║    SERVO CALIBRATION SUMMARY         ║");
  Serial.println("╠══════════════════════════════════════╣");

  Serial.print("║  Centre:     ");
  if (calCenter > 0) { Serial.print(calCenter); Serial.println(" µs"); }
  else Serial.println("(not set)");

  Serial.print("║  Max right:  ");
  if (calMaxRight > 0) { Serial.print(calMaxRight); Serial.println(" µs"); }
  else Serial.println("(not set)");

  Serial.print("║  Max left:   ");
  if (calMaxLeft > 0) { Serial.print(calMaxLeft); Serial.println(" µs"); }
  else Serial.println("(not set)");

  Serial.println("╠══════════════════════════════════════╣");

  if (calCenter > 0 && calMaxRight > 0 && calMaxLeft > 0) {
    int rangeR = calMaxRight - calCenter;
    int rangeL = calCenter - calMaxLeft;

    Serial.print("║  Range right: +");
    Serial.print(rangeR); Serial.println(" µs from centre");
    Serial.print("║  Range left:  -");
    Serial.print(rangeL); Serial.println(" µs from centre");

    if (abs(rangeR - rangeL) > 30) {
      Serial.println("║  ⚠ Asymmetric! Use the smaller range for both sides.");
    }

    Serial.println("╠══════════════════════════════════════╣");
    Serial.println("║  Copy these into eps323.cpp:         ║");
    Serial.println("║                                      ║");
    Serial.print("║  #define SERVO_CENTER_US   "); Serial.println(calCenter);
    Serial.print("║  #define SERVO_RIGHT_US    "); Serial.println(calMaxRight);
    Serial.print("║  #define SERVO_LEFT_US     "); Serial.println(calMaxLeft);

    int angleCentre = map(calCenter, SERVO_MIN_US, SERVO_MAX_US, 0, 180);
    int angleRight  = map(calMaxRight, SERVO_MIN_US, SERVO_MAX_US, 0, 180);
    int angleLeft   = map(calMaxLeft, SERVO_MIN_US, SERVO_MAX_US, 0, 180);
    Serial.println("║                                      ║");
    Serial.println("║  Equivalent angles (ESP32Servo):     ║");
    Serial.print("║  Centre ≈ "); Serial.print(angleCentre); Serial.println("°");
    Serial.print("║  Right  ≈ "); Serial.print(angleRight); Serial.println("°");
    Serial.print("║  Left   ≈ "); Serial.print(angleLeft); Serial.println("°");
  }

  Serial.println("╚══════════════════════════════════════╝\n");
}

// ═════════════════════════════════════════════════════════════
//  Help
// ═════════════════════════════════════════════════════════════
void printHelp() {
  Serial.println("\n┌──────── SERVO CALIBRATION TOOL ────────┐");
  Serial.println("│ c         Show current µs               │");
  Serial.println("│ + / -     Nudge ±5 µs (fine)            │");
  Serial.println("│ > / <     Nudge ±50 µs (coarse)         │");
  Serial.println("│ s<num>    Jump to µs, e.g. s1520        │");
  Serial.println("│ t         Interactive centre trim        │");
  Serial.println("│ w         Full sweep test                │");
  Serial.println("│ h         Hysteresis test (10 cycles)    │");
  Serial.println("│ r         Repeatability test (20 cycles) │");
  Serial.println("│ p         Print calibration summary      │");
  Serial.println("│ L         Save current as max LEFT       │");
  Serial.println("│ R         Save current as max RIGHT      │");
  Serial.println("│ ?         This help                      │");
  Serial.println("└─────────────────────────────────────────┘\n");
}

// ═════════════════════════════════════════════════════════════
//  Setup & Loop
// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);

  steeringServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  setServoUS(1565);

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  WRO SERVO CALIBRATION TOOL              ║");
  Serial.println("║  JX PDI-6221MG on GPIO 27                ║");
  Serial.println("║  Starting at 1565 µs (nominal centre)    ║");
  Serial.println("╚══════════════════════════════════════════╝");
  Serial.println();
  Serial.println("STEP 1: Make sure the servo horn is attached so the");
  Serial.println("        wheels point roughly straight at 1565 µs.");
  Serial.println("STEP 2: Type 't' to start interactive centre trim.");
  Serial.println("STEP 3: Steer to max right, type 'R' to save.");
  Serial.println("STEP 4: Steer to max left, type 'L' to save.");
  Serial.println("STEP 5: Type 'h' to test hysteresis.");
  Serial.println("STEP 6: Type 'p' to see your final values.");
  Serial.println();
  printHelp();
}

void loop() {
  if (!Serial.available()) return;

  char ch = Serial.read();

  switch (ch) {
    case 'c': case 'C':
      printPosition();
      break;

    case '+':
      setServoUS(currentUS + FINE_STEP);
      printPosition();
      break;

    case '-':
      setServoUS(currentUS - FINE_STEP);
      printPosition();
      break;

    case '>':
      setServoUS(currentUS + COARSE_STEP);
      printPosition();
      break;

    case '<':
      setServoUS(currentUS - COARSE_STEP);
      printPosition();
      break;

    case 's': case 'S': {
      int val = Serial.parseInt();
      if (val >= SERVO_MIN_US && val <= SERVO_MAX_US) {
        setServoUS(val);
        printPosition();
      } else {
        Serial.println("  Invalid µs. Range: 500–2500.");
      }
      break;
    }

    case 'w': case 'W':
      runSweepTest();
      break;

    case 'h': case 'H':
      runHysteresisTest();
      break;

    case 'r':
      runRepeatabilityTest();
      break;

    case 'R':
      calMaxRight = currentUS;
      Serial.print("  ** MAX RIGHT SAVED: ");
      Serial.print(calMaxRight);
      Serial.println(" µs **");
      break;

    case 'L':
      calMaxLeft = currentUS;
      Serial.print("  ** MAX LEFT SAVED: ");
      Serial.print(calMaxLeft);
      Serial.println(" µs **");
      break;

    case 't': case 'T':
      runTrimCentre();
      break;

    case 'p': case 'P':
      printCalibrationSummary();
      break;

    case '?':
      printHelp();
      break;

    default:
      break;
  }
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_SERVO_CAL
