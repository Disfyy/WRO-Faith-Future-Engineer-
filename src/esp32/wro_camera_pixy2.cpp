#include "wro_build_target.h"
#include "wro_config_v13.h"
// Pixy2/2.1 binary backend (interim camera after the OpenMV OV5640 module
// was damaged on 2026-06-12). Fills the same g_cam struct as wro_camera.cpp,
// so the FSM, CAM_SILENT failsafes and telemetry need no changes.
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN && CAMERA_BACKEND == CAMERA_BACKEND_PIXY2

#include <Arduino.h>
#include <HardwareSerial.h>
#include "wro_camera.h"

/*
 * Pixy2 UART protocol (docs.pixycam.com, "porting guide"):
 *   request : ae c1 20 02 <sigmap> <maxBlocks>          (getBlocks, no csum)
 *   response: af c1 21 <payLen> <csumL> <csumH> <payload>
 *   block   : sig(2) x(2) y(2) w(2) h(2) angle(2) index(1) age(1) = 14 B, LE
 *   x: 0..315, y: 0..207. Checksum = 16-bit sum of payload bytes.
 *   An error/busy response arrives as type 0x01/0x03 — ignored, re-polled.
 *
 * Wiring (Pixy 10-pin IDC, pin 1 marked on the connector):
 *   Pixy pin 4 (UART TX, 3.3V) → ESP32-S3 GPIO17 (CAMERA_RX)
 *   Pixy pin 1 (UART RX)       ← ESP32-S3 GPIO18 (CAMERA_TX)
 *   Pixy pin 2 (5V)            ← 5V buck   |   Pixy pin 6 (GND) ↔ GND
 *
 * PixyMon (one-time setup): teach signatures 1..5 in the order documented
 * in wro_config_v13.h §12, set Interface → UART @ 115200, save to flash.
 *
 * Mapping to the OpenMV-era g_cam fields:
 *   x 0..315 → redX/greenX/extraTag -80..79   (×160/316, recentered)
 *   block h  → dist_cm = PIXY_FOCAL_PIX * 10 / h   (same pinhole recipe)
 *   orange/blue/magenta sigs gated by Y bands replicating the OpenMV ROIs.
 */

CameraData g_cam = {};

static HardwareSerial CamSerial(2);

enum { SIG_RED = 1, SIG_GREEN = 2, SIG_ORANGE = 3, SIG_BLUE = 4, SIG_MAGENTA = 5 };

#define PIXY_MAX_BLOCKS  8
#define PIXY_BLOCK_BYTES 14

// Receive state machine
enum RxState : uint8_t { RX_SYNC1, RX_SYNC2, RX_TYPE, RX_LEN, RX_CSUM_L, RX_CSUM_H, RX_PAYLOAD };
static RxState  rxState = RX_SYNC1;
static uint8_t  respType = 0;
static uint8_t  payLen = 0, payPos = 0;
static uint16_t respCsum = 0;
static uint8_t  payload[PIXY_MAX_BLOCKS * PIXY_BLOCK_BYTES];

static bool     awaiting   = false;
static uint32_t lastReqMs  = 0;

void cam_init() {
  CamSerial.begin(CAMERA_BAUD, SERIAL_8N1, CAMERA_RX, CAMERA_TX);
  g_cam.redDist    = 999;
  g_cam.greenDist  = 999;
  g_cam.online     = false;
  g_cam.lastSeenMs = 0;
  g_cam.lastValidMs = 0;
  rxState  = RX_SYNC1;
  awaiting = false;
  Serial.println("Camera UART2 ready — Pixy2 backend (115200, RX=17, TX=18)");
}

static void sendGetBlocks() {
  // sigmap 0x1F = signatures 1..5
  const uint8_t req[6] = { 0xae, 0xc1, 0x20, 0x02, 0x1F, PIXY_MAX_BLOCKS };
  CamSerial.write(req, sizeof(req));
  awaiting  = true;
  lastReqMs = millis();
  rxState   = RX_SYNC1;
}

static inline uint16_t le16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline int mapX(int pixyX) {           // 0..315 → -80..79
  int v = (int)(((long)pixyX * 160L) / 316L) - 80;
  return v < -80 ? -80 : (v > 79 ? 79 : v);
}

// A complete, checksum-valid blocks payload → fill g_cam.
static void applyBlocks() {
  // Largest block per signature (same "dominant blob" rule as the OpenMV script)
  uint32_t bestArea[6] = { 0, 0, 0, 0, 0, 0 };
  int      bestX[6] = { 0 }, bestY[6] = { 0 }, bestH[6] = { 0 };

  for (uint8_t off = 0; off + PIXY_BLOCK_BYTES <= payLen; off += PIXY_BLOCK_BYTES) {
    const uint8_t *b = payload + off;
    uint16_t sig = le16(b);
    if (sig < SIG_RED || sig > SIG_MAGENTA) continue;     // ignore color codes
    int x = le16(b + 2), y = le16(b + 4);
    int w = le16(b + 6), h = le16(b + 8);
    uint32_t area = (uint32_t)w * (uint32_t)h;
    if (area > bestArea[sig]) {
      bestArea[sig] = area; bestX[sig] = x; bestY[sig] = y; bestH[sig] = h;
    }
  }

  int redX = 0, redDist = 999, greenX = 0, greenDist = 999;
  int modeFlag = 0, extraTag = 0;

  if (bestArea[SIG_RED] >= PIXY_MIN_AREA_PILLAR && bestH[SIG_RED] >= 2) {
    redX = mapX(bestX[SIG_RED]);
    int d = PIXY_FOCAL_PIX * 10 / bestH[SIG_RED];
    redDist = (d < 0) ? 0 : (d > CAM_DIST_MAX_CM ? 999 : d);
  }
  if (bestArea[SIG_GREEN] >= PIXY_MIN_AREA_PILLAR && bestH[SIG_GREEN] >= 2) {
    greenX = mapX(bestX[SIG_GREEN]);
    int d = PIXY_FOCAL_PIX * 10 / bestH[SIG_GREEN];
    greenDist = (d < 0) ? 0 : (d > CAM_DIST_MAX_CM ? 999 : d);
  }
  if (bestArea[SIG_ORANGE] >= PIXY_MIN_AREA_LINE && bestY[SIG_ORANGE] >= PIXY_LINE_Y_MIN)
    modeFlag |= CAM_FLAG_ORANGE;
  if (bestArea[SIG_BLUE] >= PIXY_MIN_AREA_LINE && bestY[SIG_BLUE] >= PIXY_LINE_Y_MIN)
    modeFlag |= CAM_FLAG_BLUE;
  if (bestArea[SIG_MAGENTA] >= PIXY_MIN_AREA_MAGENTA && bestY[SIG_MAGENTA] >= PIXY_MAGENTA_Y_MIN) {
    modeFlag |= CAM_FLAG_MAGENTA;
    extraTag = mapX(bestX[SIG_MAGENTA]);
  }

  g_cam.redX        = redX;
  g_cam.redDist     = redDist;
  g_cam.greenX      = greenX;
  g_cam.greenDist   = greenDist;
  g_cam.modeFlag    = modeFlag & 0x0F;
  g_cam.extraTag    = extraTag;
  g_cam.lastValidMs = millis();
  g_cam.framesOk++;
}

static void handleResponse() {
  awaiting = false;
  if (respType != 0x21) {                       // busy / error frame
    g_cam.framesRejected++;
    return;
  }
  uint16_t sum = 0;
  for (uint8_t i = 0; i < payLen; i++) sum += payload[i];
  if (sum != respCsum) {
    g_cam.framesBadCs++;
    return;
  }
  applyBlocks();                                // payLen == 0 is a valid
}                                               // "nothing seen" frame

void cam_update() {
  while (CamSerial.available()) {
    uint8_t c = (uint8_t)CamSerial.read();
    g_cam.lastSeenMs = millis();

    switch (rxState) {
      case RX_SYNC1:
        if (c == 0xaf) rxState = RX_SYNC2;
        break;
      case RX_SYNC2:
        rxState = (c == 0xc1) ? RX_TYPE : (c == 0xaf ? RX_SYNC2 : RX_SYNC1);
        break;
      case RX_TYPE:
        respType = c;
        rxState = RX_LEN;
        break;
      case RX_LEN:
        payLen = c;
        if (payLen > sizeof(payload) ||
            (respType == 0x21 && payLen % PIXY_BLOCK_BYTES != 0)) {
          rxState = RX_SYNC1;                   // garbage length — resync
        } else {
          payPos = 0;
          rxState = RX_CSUM_L;
        }
        break;
      case RX_CSUM_L:
        respCsum = c;
        rxState = RX_CSUM_H;
        break;
      case RX_CSUM_H:
        respCsum |= (uint16_t)c << 8;
        if (payLen == 0) { handleResponse(); rxState = RX_SYNC1; }
        else rxState = RX_PAYLOAD;
        break;
      case RX_PAYLOAD:
        payload[payPos++] = c;
        if (payPos >= payLen) { handleResponse(); rxState = RX_SYNC1; }
        break;
    }
  }

  uint32_t now = millis();
  if (awaiting && now - lastReqMs > PIXY_RESP_TIMEOUT_MS) {
    awaiting = false;                           // response lost — re-poll
    rxState = RX_SYNC1;
  }
  if (!awaiting && now - lastReqMs >= PIXY_POLL_MS) {
    sendGetBlocks();
  }

  unsigned long sinceValid = now - g_cam.lastValidMs;
  g_cam.online = (g_cam.lastValidMs != 0) && (sinceValid <= CAM_SILENT_DEGRADE_MS);
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN && CAMERA_BACKEND == CAMERA_BACKEND_PIXY2
