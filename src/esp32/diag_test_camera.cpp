#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_TEST_CAMERA

/*
 * TARGET 12 — OpenMV camera link test (v13)
 *
 * Answers ONE question: is the camera giving signals, and are they valid?
 *
 * Listens on UART2 (GPIO 17 RX / 18 TX, 115200 8N1) for protocol v3.1
 * frames:  RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
 *
 * Prints a 1-second summary with a plain-language VERDICT:
 *   NO BYTES   -> wiring/power/script-not-saved problem
 *   GARBAGE    -> baud mismatch or TX/RX swapped
 *   BAD CHECKSUM -> noise or protocol mismatch
 *   OK         -> frames parsed; shows what the camera currently sees
 *
 * Wheels can stay on the ground — this target never moves the robot.
 */

#include <Arduino.h>
#include "wro_hw_config_v13.h"

static HardwareSerial CamTest(2);

// line assembly
#define LINE_MAX 64
static char     lineBuf[LINE_MAX];
static uint8_t  linePos        = 0;

// stats since boot
static uint32_t bytesTotal     = 0;
static uint32_t framesOk       = 0;
static uint32_t framesBadCs    = 0;
static uint32_t framesBadFmt   = 0;
static unsigned long lastByteMs = 0;

// stats for the current 1 s window
static uint32_t winBytes = 0, winOk = 0, winBadCs = 0, winBadFmt = 0;
static unsigned long lastReportMs = 0;

// last good frame
static int  lastRedX = 0, lastRedDist = 999, lastGreenX = 0, lastGreenDist = 999;
static int  lastMode = 0, lastTag = 0;

// keep one example of a bad line for the report
static char badExample[LINE_MAX] = "";

static void parseLine(const char *line) {
  // split payload*checksum
  const char *star = strchr(line, '*');
  if (!star || strlen(star) < 3) {
    framesBadFmt++; winBadFmt++;
    strncpy(badExample, line, LINE_MAX - 1);
    return;
  }
  // XOR of every payload char must equal the 2 hex digits after '*'
  uint8_t cs = 0;
  for (const char *p = line; p < star; p++) cs ^= (uint8_t)*p;
  uint8_t want = (uint8_t)strtol(star + 1, NULL, 16);
  if (cs != want) {
    framesBadCs++; winBadCs++;
    strncpy(badExample, line, LINE_MAX - 1);
    return;
  }
  int rx, rd, gx, gd, mf, et;
  if (sscanf(line, "%d,%d,%d,%d,%d,%d", &rx, &rd, &gx, &gd, &mf, &et) != 6) {
    framesBadFmt++; winBadFmt++;
    strncpy(badExample, line, LINE_MAX - 1);
    return;
  }
  framesOk++; winOk++;
  lastRedX = rx;  lastRedDist   = rd;
  lastGreenX = gx; lastGreenDist = gd;
  lastMode = mf;  lastTag = et;
}

static void report(unsigned long now) {
  Serial.printf("[CAM] bytes/s=%lu frames/s ok=%lu badCS=%lu badFMT=%lu (total ok=%lu)\n",
                (unsigned long)winBytes, (unsigned long)winOk,
                (unsigned long)winBadCs, (unsigned long)winBadFmt,
                (unsigned long)framesOk);

  // ---- verdict, worst problem first ----
  if (bytesTotal == 0) {
    Serial.println("  VERDICT: NO BYTES EVER - camera silent.");
    Serial.println("    check: OpenMV TX -> ESP32 GPIO17 | OpenMV RX -> GPIO18 | GND-GND");
    Serial.println("    check: camera powered? script saved on camera as main.py?");
  } else if (winBytes == 0) {
    Serial.printf("  VERDICT: WENT SILENT %.1f s ago (was alive before) - camera crashed or cable loose.\n",
                  (now - lastByteMs) / 1000.0f);
  } else if (winOk == 0 && (winBadFmt > 0 || winBadCs > 0)) {
    if (winBadCs >= winBadFmt)
      Serial.println("  VERDICT: BYTES ARRIVE BUT CHECKSUM FAILS - noise on wire or script/protocol mismatch.");
    else
      Serial.println("  VERDICT: GARBAGE - likely wrong baud or TX/RX swapped.");
    Serial.printf("    example bad line: \"%s\"\n", badExample);
  } else if (winOk > 0) {
    Serial.print("  VERDICT: OK - camera alive. Sees: ");
    bool any = false;
    if (lastRedDist   < 999) { Serial.printf("RED x=%+d d=%dcm ",   lastRedX,   lastRedDist);   any = true; }
    if (lastGreenDist < 999) { Serial.printf("GREEN x=%+d d=%dcm ", lastGreenX, lastGreenDist); any = true; }
    if (lastMode & 0x1)      { Serial.print("ORANGE-line ");  any = true; }
    if (lastMode & 0x2)      { Serial.print("BLUE-line ");    any = true; }
    if (lastMode & 0x4)      { Serial.printf("MAGENTA x=%+d ", lastTag); any = true; }
    if (!any) Serial.print("nothing (point it at a pillar/line to verify detection)");
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("============================================");
  Serial.println(" TARGET 12: OpenMV CAMERA LINK TEST (v13)");
  Serial.println(" UART2  RX=GPIO17  TX=GPIO18  115200 8N1");
  Serial.println(" Expecting ~50 frames/s of protocol v3.1");
  Serial.println("============================================");
  CamTest.begin(CAMERA_BAUD, SERIAL_8N1, CAMERA_RX, CAMERA_TX);
  lastReportMs = millis();
}

void loop() {
  unsigned long now = millis();

  while (CamTest.available()) {
    char c = (char)CamTest.read();
    bytesTotal++; winBytes++;
    lastByteMs = now;
    if (c == '\n') {
      lineBuf[linePos] = 0;
      if (linePos > 0) parseLine(lineBuf);
      linePos = 0;
    } else if (c != '\r') {
      if (linePos < LINE_MAX - 1) lineBuf[linePos++] = c;
      else linePos = 0;                       // runaway line: resync
    }
  }

  if (now - lastReportMs >= 1000) {
    lastReportMs = now;
    report(now);
    winBytes = winOk = winBadCs = winBadFmt = 0;
  }
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_TEST_CAMERA
