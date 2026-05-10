#pragma once
/*
 * Camera UART parser — OpenMV H7 Plus → ESP32-S3 over UART2 (GPIO 17/18)
 *
 * Frame format (UART v3, 115200 8N1):
 *   RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
 *   - 5 commas, 6 fields, 1-byte XOR checksum (hex, 2 chars after '*')
 *   - Frame rate ~50 Hz
 *
 * ModeFlag bits (per docs/WRO_OpenMV_UART_Protocol.md):
 *   bit0: orange line  (CW indicator)
 *   bit1: blue   line  (CCW indicator)
 *   bit2: magenta parking block visible
 *   bit3: RESERVED — wall detection moved to VL53L1X in v13.
 *         Camera always emits 0; ESP32 ignores this bit.
 *
 * Sanity: distance values are clamped to [0, CAM_DIST_MAX_CM]; a frame-to-frame
 *         jump > CAM_DIST_JUMP_MAX_CM is rejected (defends against XOR collisions).
 */

#include <stdint.h>

struct CameraData {
  int  redX;       // -160..160; 0 = not seen
  int  redDist;    // 0..999 cm; 999 = not seen
  int  greenX;
  int  greenDist;
  int  modeFlag;   // 4-bit
  int  extraTag;
  bool online;
  unsigned long lastSeenMs;
  unsigned long lastValidMs;
  uint32_t framesOk;
  uint32_t framesBadCs;
  uint32_t framesRejected;
};

#define CAM_FLAG_ORANGE   1
#define CAM_FLAG_BLUE     2
#define CAM_FLAG_MAGENTA  4
// bit 3 is reserved (legacy wall detection); camera emits 0 in v13.

void cam_init();
void cam_update();   // call every loop tick — non-blocking byte poll

extern CameraData g_cam;
