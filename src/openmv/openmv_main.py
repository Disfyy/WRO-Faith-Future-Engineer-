import sensor
import image
import time
from pyb import UART, LED

# ============================================================
# WRO Future Engineers 2026 — OpenMV H7 Plus Vision v3.1
# ============================================================
# Sends compact frame to ESP32-S3 over UART3 at 115200 8N1.
#
# Frame format (must match docs/guides/WRO_OpenMV_UART_Protocol.md v3.1):
#   RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
#
# ModeFlag (bitwise):
#   bit 0 (1) = orange line visible (CW indicator)
#   bit 1 (2) = blue   line visible (CCW indicator)
#   bit 2 (4) = magenta parking-lot block visible
#   bit 3 (8) = RESERVED — wall distance is owned by ESP32 ToF sensors
#               in v13 firmware. Camera always emits 0 here.
#
# ExtraTag:
#   X-position of magenta block (-80..79, = cx-80) when bit 2 is set, else 0.
#
# References to WRO 2026 Future Engineers Game Rules
# (docs/rules/WRO-2026-Future-Engineers-Self-Driving-Cars-General-Rules.md):
#   13.19  Traffic-sign pillars: 50 x 50 x 100 mm
#   13.21  Red pillar:    RGB (238, 39, 55)
#   13.22  Green pillar:  RGB (68, 214, 44)
#   13.25  Parking-lot limitation: 200 x 20 x 100 mm
#   13.27  Magenta block:  RGB (255, 0, 255)
#   13.9   Track lines: 20 mm thick.
#          Orange CMYK (0, 60, 100, 0)  ~= RGB (255, 102,   0)
#          Blue   CMYK (100, 80, 0, 0)  ~= RGB (  0,  51, 255)
# ============================================================

# --- Sensor ---------------------------------------------------
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QQVGA)         # 160 x 120
sensor.skip_frames(time=2000)              # let auto-gain/AWB settle...
sensor.set_auto_gain(False)                # ...then lock for stable LAB
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, exposure_us=10000)

# --- UART -----------------------------------------------------
# UART(3) is the H7 Plus's hardware UART exposed on the camera-facing
# connector. If it can't be constructed (wrong port number on a different
# OpenMV board, peripheral busy, etc.) we'd otherwise hang silently with no
# packets reaching the ESP32 and no visible reason. Blink the red LED fast
# instead so the failure is obvious on the field.
led_red   = LED(1)
led_green = LED(2)
try:
    uart = UART(3, 115200, timeout_char=1000)
except Exception:
    while True:
        led_red.on();  time.sleep_ms(150)
        led_red.off(); time.sleep_ms(150)

# --- Image geometry ------------------------------------------
IMG_W    = 160
IMG_H    = 120
CENTER_X = IMG_W // 2                       # 80

# ============================================================
# REAL-WORLD DIMENSIONS (from WRO rules — DO NOT change)
# ============================================================
PILLAR_HEIGHT_CM     = 10.0                 # rule 13.19 (100 mm)
PARK_BLOCK_HEIGHT_CM = 10.0                 # rule 13.25 (100 mm)

# ============================================================
# CAMERA INTRINSICS — TUNE ON TRACK
# ------------------------------------------------------------
# Pinhole model:  distance_cm = FOCAL_PIX * real_height_cm / blob_h_px
#
# Calibration recipe (do once on the actual chassis with the actual lens):
#   1) Place a red pillar at exactly 50 cm from the camera.
#   2) Read the printed "R D:..." overlay or blob.h() in OpenMV IDE.
#   3) FOCAL_PIX = (50 * blob.h()) / 10        # = 50 cm * px / 10 cm
#
# Default 120 assumes the stock 2.8 mm M12 lens (~70 deg HFOV) on H7 Plus
# at QQVGA. With a wider/narrower lens the value changes proportionally.
# ============================================================
FOCAL_PIX = 120

# ============================================================
# LAB COLOUR THRESHOLDS — TUNE ON TRACK
# ------------------------------------------------------------
# Tools -> Machine Vision -> Threshold Editor in OpenMV IDE.
# Starting ranges below are derived from the rule-given RGB values
# (sRGB -> CIELAB, D65) widened ~+/-15 for lighting tolerance.
# ============================================================
# Red pillar      RGB(238, 39, 55)   ->  L~52  a~+70  b~+46
THRESHOLD_RED     = (35, 70,  30,  90,  10,  70)
# Green pillar    RGB( 68,214, 44)   ->  L~76  a~-65  b~+67
THRESHOLD_GREEN   = (55, 90, -90, -25,  20,  80)
# Orange line     RGB(255,102,  0)   ->  L~66  a~+45  b~+73
THRESHOLD_ORANGE  = (45, 85,  20,  70,  30,  90)
# Blue line       RGB(  0, 51,255)   ->  L~32  a~+71  b~-105
THRESHOLD_BLUE    = (15, 50,  10,  80, -128, -30)
# Magenta park    RGB(255,  0,255)   ->  L~60  a~+98  b~-60
THRESHOLD_MAGENTA = (40, 80,  55, 100, -90, -30)

# --- Regions of Interest (x, y, w, h) ------------------------
# Pillars stand on the floor, ~10 cm tall. Whole height usually fits
# inside the upper 90% of the frame; widen vs. v3.0 so very-close
# pillars (whose tops near the top edge, bottoms near the bottom edge)
# are not clipped.
ROI_PILLARS  = (0,   0, IMG_W, 110)
# Floor lines — only ever visible near the bottom of the frame.
ROI_LINES    = (0,  80, IMG_W,  40)
# Magenta parking-lot blocks: short and wide, mid-lower band.
ROI_MAGENTA  = (0,  40, IMG_W,  80)

# --- Minimum blob sizes (px) ---------------------------------
MIN_PIX_PILLAR  = 50
MIN_PIX_LINE    = 100
MIN_PIX_MAGENTA = 80

clock = time.clock()


# ============================================================
def distance_cm(blob, real_height_cm):
    """Pinhole-model distance estimate. Uses blob height because:
      - rules guarantee object height (100 mm) — width can vary by view angle,
      - height is more robust under partial side-occlusion."""
    h = blob.h()
    if h < 2:
        return 999
    d = int((FOCAL_PIX * real_height_cm) / h)
    return min(max(d, 0), 999)


def calc_checksum(s):
    cs = 0
    for ch in s:
        cs ^= ord(ch)
    return cs & 0xFF


def find_dominant(blob_list):
    """Largest blob by pixel area = best signal. Robust under partial
    occlusion since width and height can both shrink, but pixel count
    still ranks the closest/most-visible target highest."""
    if not blob_list:
        return None
    best = blob_list[0]
    for b in blob_list[1:]:
        if b.pixels() > best.pixels():
            best = b
    return best


# ============================================================
# MAIN LOOP
# ============================================================
while True:
    clock.tick()
    img = sensor.snapshot()

    # Default outputs (6 protocol fields)
    red_x    = 0;   red_dist   = 999
    green_x  = 0;   green_dist = 999
    mode_flag = 0;  extra_tag  = 0

    # ---------- 1. RED PILLARS (rule 13.21) ----------
    red_blobs = img.find_blobs([THRESHOLD_RED],
                               roi=ROI_PILLARS,
                               pixels_threshold=MIN_PIX_PILLAR,
                               area_threshold=MIN_PIX_PILLAR,
                               merge=True)
    best_red = find_dominant(red_blobs)
    if best_red:
        red_x    = best_red.cx() - CENTER_X
        red_dist = distance_cm(best_red, PILLAR_HEIGHT_CM)
        img.draw_rectangle(best_red.rect(), color=(255, 50, 50))
        img.draw_cross(best_red.cx(), best_red.cy(), color=(255, 50, 50))
        img.draw_string(best_red.x(), best_red.y() - 10,
                        "R D:%d" % red_dist, color=(255, 50, 50), scale=1)

    # ---------- 2. GREEN PILLARS (rule 13.22) ----------
    green_blobs = img.find_blobs([THRESHOLD_GREEN],
                                 roi=ROI_PILLARS,
                                 pixels_threshold=MIN_PIX_PILLAR,
                                 area_threshold=MIN_PIX_PILLAR,
                                 merge=True)
    best_green = find_dominant(green_blobs)
    if best_green:
        green_x    = best_green.cx() - CENTER_X
        green_dist = distance_cm(best_green, PILLAR_HEIGHT_CM)
        img.draw_rectangle(best_green.rect(), color=(50, 255, 50))
        img.draw_cross(best_green.cx(), best_green.cy(), color=(50, 255, 50))
        img.draw_string(best_green.x(), best_green.y() - 10,
                        "G D:%d" % green_dist, color=(50, 255, 50), scale=1)

    # ---------- 3. ORANGE / BLUE FLOOR LINES (rule 13.9) ----------
    if img.find_blobs([THRESHOLD_ORANGE],
                      roi=ROI_LINES,
                      pixels_threshold=MIN_PIX_LINE,
                      merge=True):
        mode_flag |= 1                       # bit 0 = orange (CW)

    if img.find_blobs([THRESHOLD_BLUE],
                      roi=ROI_LINES,
                      pixels_threshold=MIN_PIX_LINE,
                      merge=True):
        mode_flag |= 2                       # bit 1 = blue (CCW)

    # ---------- 4. MAGENTA PARKING BLOCK (rule 13.27) ----------
    magenta_blobs = img.find_blobs([THRESHOLD_MAGENTA],
                                   roi=ROI_MAGENTA,
                                   pixels_threshold=MIN_PIX_MAGENTA,
                                   merge=True)
    best_magenta = find_dominant(magenta_blobs)
    if best_magenta:
        mode_flag |= 4                       # bit 2 = magenta visible
        extra_tag  = best_magenta.cx() - CENTER_X
        img.draw_rectangle(best_magenta.rect(), color=(255, 0, 255))
        img.draw_string(best_magenta.x(), best_magenta.y() - 10,
                        "MAG", color=(255, 0, 255), scale=1)

    # ---------- 5. UART OUTPUT ----------
    data_str = "%d,%d,%d,%d,%d,%d" % (red_x, red_dist,
                                      green_x, green_dist,
                                      mode_flag, extra_tag)
    uart.write("%s*%02X\n" % (data_str, calc_checksum(data_str)))

    # LED feedback
    led_red.on()  if (best_red or best_green) else led_red.off()
    led_green.on() if (mode_flag & 4)         else led_green.off()

    time.sleep_ms(10)                        # cap at ~50 Hz to match doc
