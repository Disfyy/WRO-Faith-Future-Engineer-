#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include <HardwareSerial.h>
#include "wro_config_v13.h"
#include "wro_camera.h"

CameraData g_cam = {};

static HardwareSerial CamSerial(2);

#define CAM_BUF_SIZE 96
static char     buf[CAM_BUF_SIZE];
static uint8_t  bufPos = 0;

static int prevRedDist   = 999;
static int prevGreenDist = 999;

void cam_init() {
  CamSerial.begin(CAMERA_BAUD, SERIAL_8N1, CAMERA_RX, CAMERA_TX);
  g_cam.redDist = 999;
  g_cam.greenDist = 999;
  g_cam.online = false;
  g_cam.lastSeenMs = 0;
  g_cam.lastValidMs = 0;
  bufPos = 0;
  Serial.println("Camera UART2 ready (115200, RX=17, TX=18)");
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Parse one complete line (without trailing \n). Returns true if accepted.
static bool parseFrame(const char *line, uint8_t len) {
  if (len < 11) return false;

  const char *star = nullptr;
  for (uint8_t i = 0; i < len; i++) if (line[i] == '*') { star = line + i; break; }
  if (!star || (line + len - star) < 3) return false;

  uint8_t cs = 0;
  for (const char *p = line; p < star; p++) cs ^= (uint8_t)*p;

  int hi = hexNibble(star[1]);
  int lo = hexNibble(star[2]);
  if (hi < 0 || lo < 0) return false;
  uint8_t got = (uint8_t)((hi << 4) | lo);
  if (cs != got) {
    g_cam.framesBadCs++;
    return false;
  }

  int rx, rd, gx, gd, mf, et;
  // sscanf is fine for a 30-50 Hz parser; not in a tight inner loop.
  if (sscanf(line, "%d,%d,%d,%d,%d,%d*", &rx, &rd, &gx, &gd, &mf, &et) != 6) return false;

  if (rd < 0) rd = 0;
  if (gd < 0) gd = 0;
  if (rd > CAM_DIST_MAX_CM) rd = 999;
  if (gd > CAM_DIST_MAX_CM) gd = 999;

  // Reject sudden jumps (XOR collision defense). Allow jumps when transitioning
  // to/from "not seen" (999), since that's a legitimate visibility flip.
  bool redJump   = (prevRedDist   != 999 && rd != 999 && abs(rd - prevRedDist)   > CAM_DIST_JUMP_MAX_CM);
  bool greenJump = (prevGreenDist != 999 && gd != 999 && abs(gd - prevGreenDist) > CAM_DIST_JUMP_MAX_CM);
  if (redJump || greenJump) {
    g_cam.framesRejected++;
    return false;
  }

  prevRedDist   = rd;
  prevGreenDist = gd;

  g_cam.redX       = rx;
  g_cam.redDist    = rd;
  g_cam.greenX     = gx;
  g_cam.greenDist  = gd;
  g_cam.modeFlag   = mf & 0x0F;
  g_cam.extraTag   = et;
  g_cam.lastValidMs = millis();
  g_cam.framesOk++;
  return true;
}

void cam_update() {
  while (CamSerial.available()) {
    char c = (char)CamSerial.read();
    g_cam.lastSeenMs = millis();

    if (c == '\r') continue;
    if (c == '\n') {
      if (bufPos > 0) {
        buf[bufPos] = 0;
        parseFrame(buf, bufPos);
        bufPos = 0;
      }
      continue;
    }
    if (bufPos < CAM_BUF_SIZE - 1) {
      buf[bufPos++] = c;
    } else {
      bufPos = 0;
    }
  }

  unsigned long sinceValid = millis() - g_cam.lastValidMs;
  g_cam.online = (g_cam.lastValidMs != 0) && (sinceValid <= CAM_SILENT_DEGRADE_MS);
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
