import sensor
import image
import time
from pyb import UART, LED

# ============================================================
# WRO Future Engineers — OpenMV Vision v3.0
# ============================================================
# Protocol v3.0:
#   RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*CS\n
#
# ModeFlag (bitwise):
#   bit 0 (1)  = orange line visible (CW)
#   bit 1 (2)  = blue line visible (CCW)
#   bit 2 (4)  = magenta block visible (parking)
#   bit 3 (8)  = black wall close (blind turn)
#
# ExtraTag:
#   If magenta block visible    -> X-position of block (-160..160)
#   Else if wall close          -> distance to wall (cm)
#   Otherwise                   -> 0
# ============================================================

# --- Sensor ---
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QQVGA)    # 160x120
sensor.skip_frames(time=2000)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, exposure_us=10000)

# --- UART ---
uart = UART(3, 115200, timeout_char=1000)
led_red   = LED(1)
led_green = LED(2)

# ============================================================
# THRESHOLDS (LAB). MUST be calibrated on the actual track!
# Tools -> Threshold Editor in OpenMV IDE
# ============================================================
THRESHOLD_RED     = (30, 60, 30, 80, 10, 60)     # Red pillar
THRESHOLD_GREEN   = (30, 70, -60, -20, 10, 50)   # Green pillar
THRESHOLD_ORANGE  = (50, 80, 20, 60, 20, 70)     # Orange line (CW)
THRESHOLD_BLUE    = (20, 50, -20, 20, -60, -20)  # Blue line (CCW)
THRESHOLD_MAGENTA = (30, 60, 20, 60, -40, -10)   # Magenta parking block
THRESHOLD_WALL    = (0, 20, -10, 10, -10, 10)    # Black wall

# --- Frame ---
IMG_W    = 160
IMG_H    = 120
CENTER_X = IMG_W // 2   # 80

# --- ROIs ---
ROI_PILLARS  = (0, 0, IMG_W, 90)    # Pillars in upper 3/4
ROI_LINES    = (0, 40, IMG_W, 80)   # Lines in middle/lower band
ROI_WALL     = (0, 60, IMG_W, 60)   # Wall in lower half
ROI_MAGENTA  = (0, 50, IMG_W, 70)   # Magenta blocks in mid/lower band

# --- Minimum blob sizes ---
MIN_PIX_PILLAR  = 50
MIN_PIX_LINE    = 100
MIN_PIX_WALL    = 180
MIN_PIX_MAGENTA = 60

# --- Focal-length constant ---
# CALIBRATION: place a pillar at 20 cm -> FOCAL = 20 * blob.w()
FOCAL_CONST = 2000

clock = time.clock()

# ============================================================
def get_distance(blob):
    if blob.w() < 2:
        return 999
    return min(int(FOCAL_CONST / blob.w()), 999)

def calc_checksum(s):
    cs = 0
    for ch in s:
        cs ^= ord(ch)
    return cs & 0xFF

def find_closest(blob_list):
    """Closest blob = widest blob."""
    if not blob_list:
        return None
    best = blob_list[0]
    for b in blob_list[1:]:
        if b.w() > best.w():
            best = b
    return best

def find_largest(blob_list):
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

    # --- Result: 6 fields ---
    red_x    = 0;   red_dist   = 999
    green_x  = 0;   green_dist = 999
    mode_flag = 0;  extra_tag  = 0

    # =========================
    # 1. RED PILLARS
    # =========================
    red_blobs = img.find_blobs([THRESHOLD_RED],
                                roi=ROI_PILLARS,
                                pixels_threshold=MIN_PIX_PILLAR,
                                area_threshold=MIN_PIX_PILLAR,
                                merge=True)
    best_red = find_closest(red_blobs)
    if best_red:
        red_x    = best_red.cx() - CENTER_X
        red_dist = get_distance(best_red)
        img.draw_rectangle(best_red.rect(), color=(255, 50, 50))
        img.draw_cross(best_red.cx(), best_red.cy(), color=(255, 50, 50))
        img.draw_string(best_red.x(), best_red.y()-10,
                        "R D:%d" % red_dist, color=(255,50,50), scale=1)

    # =========================
    # 2. GREEN PILLARS
    # =========================
    green_blobs = img.find_blobs([THRESHOLD_GREEN],
                                  roi=ROI_PILLARS,
                                  pixels_threshold=MIN_PIX_PILLAR,
                                  area_threshold=MIN_PIX_PILLAR,
                                  merge=True)
    best_green = find_closest(green_blobs)
    if best_green:
        green_x    = best_green.cx() - CENTER_X
        green_dist = get_distance(best_green)
        img.draw_rectangle(best_green.rect(), color=(50, 255, 50))
        img.draw_cross(best_green.cx(), best_green.cy(), color=(50, 255, 50))
        img.draw_string(best_green.x(), best_green.y()-10,
                        "G D:%d" % green_dist, color=(50,255,50), scale=1)

    # =========================
    # 3. ORANGE / BLUE LINE -> ModeFlag
    # =========================
    orange_blobs = img.find_blobs([THRESHOLD_ORANGE],
                                   roi=ROI_LINES,
                                   pixels_threshold=MIN_PIX_LINE,
                                   merge=True)
    if orange_blobs:
        mode_flag |= 1    # bit 0 = orange (CW)
        b = find_largest(orange_blobs)
        if b:
            img.draw_rectangle(b.rect(), color=(255, 128, 0))

    blue_blobs = img.find_blobs([THRESHOLD_BLUE],
                                 roi=ROI_LINES,
                                 pixels_threshold=MIN_PIX_LINE,
                                 merge=True)
    if blue_blobs:
        mode_flag |= 2    # bit 1 = blue (CCW)
        b = find_largest(blue_blobs)
        if b:
            img.draw_rectangle(b.rect(), color=(0, 128, 255))

    # =========================
    # 4. MAGENTA BLOCK (parking) -> ModeFlag + ExtraTag
    # =========================
    magenta_blobs = img.find_blobs([THRESHOLD_MAGENTA],
                                    roi=ROI_MAGENTA,
                                    pixels_threshold=MIN_PIX_MAGENTA,
                                    merge=True)
    best_magenta = find_closest(magenta_blobs)
    if best_magenta:
        mode_flag |= 4    # bit 2 = magenta
        extra_tag = best_magenta.cx() - CENTER_X
        img.draw_rectangle(best_magenta.rect(), color=(255, 0, 255))
        img.draw_string(best_magenta.x(), best_magenta.y()-10,
                        "MAG", color=(255,0,255), scale=1)

    # =========================
    # 5. BLACK WALL -> ModeFlag + ExtraTag
    # =========================
    if not (mode_flag & 4):  # Skip wall check if magenta is already in extra_tag
        wall_blobs = img.find_blobs([THRESHOLD_WALL],
                                     roi=ROI_WALL,
                                     pixels_threshold=MIN_PIX_WALL,
                                     merge=True)
        if wall_blobs:
            closest_wall = max(wall_blobs, key=lambda b: b.y())
            w_dist = get_distance(closest_wall)
            if w_dist < 40:  # Wall close
                mode_flag |= 8    # bit 3 = wall close
                extra_tag = w_dist
                img.draw_rectangle(closest_wall.rect(), color=(100, 100, 100))

    # =========================
    # 6. UART OUTPUT
    # =========================
    data_str = "%d,%d,%d,%d,%d,%d" % (red_x, red_dist, green_x, green_dist,
                                       mode_flag, extra_tag)
    cs = calc_checksum(data_str)
    uart_msg = "%s*%02X\n" % (data_str, cs)
    uart.write(uart_msg)

    # LED red: any pillar in view
    if best_red or best_green:
        led_red.on()
    else:
        led_red.off()

    # LED green: parking magenta in view
    if mode_flag & 4:
        led_green.on()
    else:
        led_green.off()

    time.sleep_ms(20)
