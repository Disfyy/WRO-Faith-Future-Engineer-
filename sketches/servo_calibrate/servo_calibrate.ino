// =====================================================================
//  WRO v13 — Steering servo calibration
// =====================================================================
//
//  Why: the servo's mechanical center rarely lines up perfectly with
//  1500 us. This sketch walks you through finding the exact microsecond
//  values for:
//    - center (wheels straight)
//    - max LEFT lock
//    - max RIGHT lock
//
//  Setup:
//    1. Lift the chassis so the wheels can turn freely.
//    2. Upload this sketch.
//    3. Open Serial Monitor at 115200 baud.
//
//  Procedure:
//    Step 1. Send `c` -- servo starts at 1500 us. Check the wheels.
//    Step 2. Use `+` (+5 us) / `-` (-5 us) for fine adjustment, or
//            `>` (+50 us) / `<` (-50 us) for coarse, until the wheels
//            point PERFECTLY STRAIGHT.
//    Step 3. When straight, press `C` to save as CENTER.
//    Step 4. Crank `+` until the wheels hit the right end-stop. Press
//            `R` to save as MAX RIGHT.
//    Step 5. Crank `-` until the wheels hit the left end-stop. Press
//            `L` to save as MAX LEFT.
//    Step 6. Press `p` for the summary you can paste into the firmware.
//
//  Important: do NOT leave the servo pinned against an end-stop -- it
//  overheats. If it buzzes at the limit, back off ~50 us.
//
//  Commands:
//    +/-       fine step (+/- 5 us)
//    >/<       coarse step (+/- 50 us)
//    c         show current value
//    1500      jump to a literal value (any 4-digit number 500..2500)
//    sweep     smooth sweep: center -> right -> center -> left -> center
//    C         save current as CENTER
//    R         save current as MAX RIGHT
//    L         save current as MAX LEFT
//    p         print calibration summary
//    h         help
// =====================================================================

#include <ESP32Servo.h>

// v13: steering servo on GPIO 42 (was GPIO 27 on v11 hardware)
#define SERVO_PIN     42
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2500

Servo steeringServo;
int currentUS = 1500;

int calCenter = 0;
int calRight  = 0;
int calLeft   = 0;

void setUS(int us) {
  us = constrain(us, SERVO_MIN_US, SERVO_MAX_US);
  currentUS = us;
  steeringServo.writeMicroseconds(us);
}

void printPos() {
  Serial.print("  >> servo = ");
  Serial.print(currentUS);
  Serial.println(" us");
}

void smoothMove(int from, int to) {
  int step = (to > from) ? 5 : -5;
  for (int us = from; (step > 0) ? (us <= to) : (us >= to); us += step) {
    setUS(us);
    delay(15);
  }
  setUS(to);
}

void runSweep() {
  int c = calCenter > 0 ? calCenter : 1500;
  int r = calRight  > 0 ? calRight  : 2000;
  int l = calLeft   > 0 ? calLeft   : 1000;

  Serial.println("\n--- SWEEP ---");
  Serial.print("  center("); Serial.print(c); Serial.println(")...");
  smoothMove(currentUS, c);  delay(500);
  Serial.print("  right(");  Serial.print(r); Serial.println(")...");
  smoothMove(c, r);          delay(500);
  Serial.print("  center..."); Serial.println();
  smoothMove(r, c);          delay(500);
  Serial.print("  left(");   Serial.print(l); Serial.println(")...");
  smoothMove(c, l);          delay(500);
  Serial.println("  center...");
  smoothMove(l, c);
  Serial.println("--- SWEEP DONE ---\n");
}

void printHelp() {
  Serial.println("\n+--- COMMANDS -----------------------------+");
  Serial.println("| +/-      +/- 5 us  (fine)                |");
  Serial.println("| >/<      +/- 50 us (coarse)              |");
  Serial.println("| c        show current                    |");
  Serial.println("| 1500     jump to literal value           |");
  Serial.println("| sweep    smooth sweep C->L->C->R->C      |");
  Serial.println("| C        save as CENTER                  |");
  Serial.println("| R        save as MAX RIGHT               |");
  Serial.println("| L        save as MAX LEFT                |");
  Serial.println("| p        print summary                   |");
  Serial.println("| h        this help                       |");
  Serial.println("+------------------------------------------+\n");
}

void printSummary() {
  Serial.println("\n+======================================+");
  Serial.println("|       CALIBRATION SUMMARY            |");
  Serial.println("+======================================+");

  Serial.print("|  CENTER:     ");
  if (calCenter > 0) { Serial.print(calCenter); Serial.println(" us"); }
  else { Serial.println("(not set)"); }

  Serial.print("|  MAX RIGHT:  ");
  if (calRight > 0) { Serial.print(calRight); Serial.println(" us"); }
  else { Serial.println("(not set)"); }

  Serial.print("|  MAX LEFT:   ");
  if (calLeft > 0) { Serial.print(calLeft); Serial.println(" us"); }
  else { Serial.println("(not set)"); }

  if (calCenter > 0 && calRight > 0 && calLeft > 0) {
    int rangeR = calRight - calCenter;
    int rangeL = calCenter - calLeft;

    Serial.println("+======================================+");
    Serial.print("|  Right travel: +"); Serial.print(rangeR); Serial.println(" us");
    Serial.print("|  Left  travel: -"); Serial.print(rangeL); Serial.println(" us");

    if (abs(rangeR - rangeL) > 50) {
      Serial.println("|  Asymmetric! Use the smaller travel.");
    }

    Serial.println("+======================================+");
    Serial.println("|  Paste into wro_hw_config_v13.h:     |");
    Serial.println("|                                      |");
    Serial.print("|  #define SERVO_CENTER_US  "); Serial.println(calCenter);
    Serial.print("|  #define SERVO_RIGHT_US   "); Serial.println(calRight);
    Serial.print("|  #define SERVO_LEFT_US    "); Serial.println(calLeft);
  }

  Serial.println("+======================================+\n");
}

void setup() {
  Serial.begin(115200);
  delay(800);

  steeringServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  setUS(1500);

  Serial.println("\n\n");
  Serial.println("+==========================================+");
  Serial.println("|  WRO v13 -- STEERING SERVO CALIBRATION   |");
  Serial.println("|  JX PDI-6221MG on GPIO 42                |");
  Serial.println("|  Start: 1500 us                          |");
  Serial.println("+==========================================+\n");

  Serial.println("STEP 1. Lift the chassis so the wheels turn freely.");
  Serial.println("STEP 2. Use '+' / '-' until wheels point STRAIGHT.");
  Serial.println("STEP 3. Press 'C' to save CENTER.");
  Serial.println("STEP 4. Crank '+' to the right end-stop, press 'R'.");
  Serial.println("STEP 5. Crank '-' to the left end-stop, press 'L'.");
  Serial.println("STEP 6. Press 'p' for the final values.\n");

  printHelp();
  Serial.println("Ready. Current: 1500 us");
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // If purely digits -> jump to that value
  bool allDigits = true;
  for (size_t i = 0; i < cmd.length(); i++) {
    if (!isDigit(cmd[i])) { allDigits = false; break; }
  }
  if (allDigits) {
    int v = cmd.toInt();
    if (v >= SERVO_MIN_US && v <= SERVO_MAX_US) {
      setUS(v);
      printPos();
    } else {
      Serial.print("  Range is 500..2500. You entered: ");
      Serial.println(v);
    }
    return;
  }

  // Multi-char commands
  if (cmd == "sweep") { runSweep(); return; }
  if (cmd == "h" || cmd == "help") { printHelp(); return; }

  // Single-char commands
  char ch = cmd[0];
  switch (ch) {
    case '+': setUS(currentUS + 5);   printPos(); break;
    case '-': setUS(currentUS - 5);   printPos(); break;
    case '>': setUS(currentUS + 50);  printPos(); break;
    case '<': setUS(currentUS - 50);  printPos(); break;
    case 'c': printPos(); break;

    case 'C':
      calCenter = currentUS;
      Serial.print("\n  *** CENTER saved: ");
      Serial.print(calCenter); Serial.println(" us ***\n");
      break;

    case 'R':
      calRight = currentUS;
      Serial.print("\n  *** MAX RIGHT saved: ");
      Serial.print(calRight); Serial.println(" us ***\n");
      break;

    case 'L':
      calLeft = currentUS;
      Serial.print("\n  *** MAX LEFT saved: ");
      Serial.print(calLeft); Serial.println(" us ***\n");
      break;

    case 'p': case 'P':
      printSummary();
      break;

    default:
      Serial.print("  Unknown command: ");
      Serial.println(cmd);
      Serial.println("  Type 'h' for help");
      break;
  }
}

void loop() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() > 0) {
        handleCommand(buf);
        buf = "";
      }
    } else {
      buf += c;
    }
  }
}
