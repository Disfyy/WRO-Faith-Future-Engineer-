/*
 * ============================================================
 * WRO Future Engineers 2026 — Team Faith
 * ESP32-CAM EMERGENCY VISION (plan B for the damaged OpenMV)
 * ============================================================
 * Emits the SAME UART v3.1 frame as src/openmv/openmv_main.py:
 *
 *   RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n   115200 8N1
 *
 *   ModeFlag bit0 = orange line, bit1 = blue line,
 *            bit2 = magenta block, bit3 = always 0 (reserved)
 *   ExtraTag = magenta cx-80 when bit2 set, else 0
 *
 * The ESP32-S3 race firmware (wro_v13_main.cpp) needs NO changes:
 * to the robot this board IS the OpenMV.
 *
 * BOARD: AI-Thinker ESP32-CAM (OV2640).
 *   Arduino IDE board: "AI Thinker ESP32-CAM".
 *   Different clone (ESP-EYE, M5, WROVER-KIT)? The camera pin map
 *   below must change — send a board photo before flashing.
 *
 * WIRING TO ROBOT (one-way link, like the OpenMV was):
 *   ESP32-CAM GPIO13 (TX) ──> ESP32-S3 GPIO17 (UART2 RX)
 *   ESP32-CAM GND        <──> robot GND
 *   ESP32-CAM 5V         <─── 5V buck (>=500 mA). NOT from the S3 3V3 pin.
 *   GPIO12 must stay UNCONNECTED (boot strap, sets flash voltage).
 *
 * FLASHING (USB-TTL adapter):
 *   TTL TX -> U0R, TTL RX -> U0T, TTL GND -> GND, 5V -> 5V.
 *   Hold IO0 to GND while resetting = download mode. Release after flash.
 *
 * PIPELINE:
 *   OV2640 YUV422 @ QQVGA 160x120. Per-pixel YCbCr thresholds (Cb/Cr are
 *   lighting-robust, analogous to LAB a/b). 2x2-pixel cell grid, connected
 *   components per colour class, largest blob wins (same find_dominant
 *   semantics as the OpenMV script). Distance from blob height with the
 *   same pinhole recipe: dist_cm = FOCAL_PIX * 10 / blob_h_px.
 *
 * TUNING:
 *   WiFi AP "WRO-CAM" pass "wro2026fe" -> http://192.168.4.1
 *   Live image + per-class pixel counts + threshold sliders + exposure
 *   controls. "Save" persists to flash (NVS) — survives reboot.
 *
 * !!! RACE BUILD: set ENABLE_WIFI_TUNER to 0 and reflash. WRO rules
 * !!! forbid radio links on the vehicle during runs.
 * ============================================================
 */

#define ENABLE_WIFI_TUNER  1     // 0 for competition runs (no radio!)
#define DISABLE_BROWNOUT   0     // last-resort hack if 5V rail is weak

#include "esp_camera.h"
#include <Preferences.h>
#if ENABLE_WIFI_TUNER
  #include <WiFi.h>
  #include <WebServer.h>
#endif
#if DISABLE_BROWNOUT
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif

// --- AI-Thinker ESP32-CAM pin map --------------------------------
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

#define LED_STATUS     33        // onboard red LED, active LOW
#define ROBOT_TX_PIN   13        // -> ESP32-S3 GPIO17 (UART2 RX)
#define ROBOT_RX_PIN   15        // unused, leave unconnected

// --- Image geometry (matches the OpenMV script) -------------------
#define IMG_W      160
#define IMG_H      120
#define CENTER_X    80
#define CELL         2           // 2x2 px cells -> 80x60 grid
#define GW          (IMG_W / CELL)
#define GH          (IMG_H / CELL)
#define CELL_ON      2           // >=2 of 4 px must match to light a cell

// --- Colour classes ------------------------------------------------
enum { C_RED = 0, C_GREEN, C_ORANGE, C_BLUE, C_MAGENTA, N_CLASSES };

// YCbCr thresholds {Ymin,Ymax,Cbmin,Cbmax,Crmin,Crmax}. Defaults derived
// from the rule RGB values (13.21/13.22/13.9/13.27, BT.601) widened for
// tolerance — TUNE ON TRACK via the WiFi page, then Save.
static int16_t thr[N_CLASSES][6] = {
  { 40, 180,  80, 135, 175, 255 },   // red pillar    RGB(238,39,55)
  { 60, 220,  40, 115,  40, 110 },   // green pillar  RGB(68,214,44)
  { 90, 230,  20,  72, 160, 235 },   // orange line   RGB(255,102,0)
  { 20, 160, 165, 255,  40, 115 },   // blue line     RGB(0,51,255)
  { 50, 200, 150, 235, 170, 255 },   // magenta park  RGB(255,0,255)
};

// ROIs as cell rows (2 px per row). Same bands as the OpenMV script:
// pillars y 0..109, lines y 80..119, magenta y 40..119.
static const uint8_t roiY0[N_CLASSES] = { 0, 0, 40, 40, 20 };
static const uint8_t roiY1[N_CLASSES] = { 54, 54, 59, 59, 59 };
static const uint16_t minPix[N_CLASSES] = { 50, 50, 100, 100, 80 };

// --- Camera intrinsics (TUNE: pillar at exactly 50 cm, read h_px on the
// web page, FOCAL_PIX = 50 * h_px / 10) ----------------------------
static int16_t focalPix = 120;
#define PILLAR_HEIGHT_CM 10

// --- Manual camera settings (persisted) ----------------------------
static uint8_t aecOn = 1;        // 1 = auto exposure
static int16_t aecVal = 300;     // manual exposure 0..1200
static uint8_t agcOn = 1;        // 1 = auto gain
static int16_t agcGain = 5;      // manual gain 0..30
static uint8_t awbOn = 1;        // 1 = auto white balance
static int16_t wbMode = 0;       // 0 auto,1 sunny,2 cloudy,3 office,4 home

// --- State ----------------------------------------------------------
static uint8_t cellCnt[N_CLASSES][GH][GW];
static uint8_t vis[GH][GW];
static uint16_t q[GH * GW];      // BFS queue of cell indices (y*GW+x)

struct Blob { bool found; uint16_t pixels; int16_t cx; int16_t h; };
static Blob blobs[N_CLASSES];

static char lastLine[64] = "";
static uint16_t fps = 0;
static bool camOk = false;

Preferences prefs;
HardwareSerial RobotSerial(2);
#if ENABLE_WIFI_TUNER
WebServer server(80);
#endif

// ====================================================================
// Camera
// ====================================================================
static bool cameraInit() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_YUV422;
  c.frame_size   = FRAMESIZE_QQVGA;       // 160x120
  c.fb_count     = 1;
  c.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  c.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  for (int attempt = 1; attempt <= 3; attempt++) {
    esp_err_t err = esp_camera_init(&c);
    if (err == ESP_OK) return true;
    Serial.printf("[CAM] init attempt %d failed: 0x%x\n", attempt, err);
    esp_camera_deinit();
    delay(300);
  }
  return false;
}

static void applyCamSettings() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  s->set_saturation(s, 1);                 // slight boost helps separation
  s->set_whitebal(s, awbOn);
  s->set_wb_mode(s, wbMode);
  s->set_exposure_ctrl(s, aecOn);
  if (!aecOn) s->set_aec_value(s, aecVal);
  s->set_gain_ctrl(s, agcOn);
  if (!agcOn) s->set_agc_gain(s, agcGain);
}

// ====================================================================
// NVS persistence
// ====================================================================
static void loadSettings() {
  prefs.begin("wrocam", true);
  if (prefs.isKey("thr"))
    prefs.getBytes("thr", thr, sizeof(thr));
  focalPix = prefs.getShort("focal", focalPix);
  aecOn   = prefs.getUChar("aecOn", aecOn);
  aecVal  = prefs.getShort("aecVal", aecVal);
  agcOn   = prefs.getUChar("agcOn", agcOn);
  agcGain = prefs.getShort("agcGain", agcGain);
  awbOn   = prefs.getUChar("awbOn", awbOn);
  wbMode  = prefs.getShort("wbMode", wbMode);
  prefs.end();
}

static void saveSettings() {
  prefs.begin("wrocam", false);
  prefs.putBytes("thr", thr, sizeof(thr));
  prefs.putShort("focal", focalPix);
  prefs.putUChar("aecOn", aecOn);
  prefs.putShort("aecVal", aecVal);
  prefs.putUChar("agcOn", agcOn);
  prefs.putShort("agcGain", agcGain);
  prefs.putUChar("awbOn", awbOn);
  prefs.putShort("wbMode", wbMode);
  prefs.end();
}

// ====================================================================
// Vision
// ====================================================================
static inline bool inRange(int y, int cb, int cr, const int16_t *t) {
  return y >= t[0] && y <= t[1] && cb >= t[2] && cb <= t[3] &&
         cr >= t[4] && cr <= t[5];
}

// Classify every pixel into colour classes, accumulate per-cell counts.
// YUV422 layout from OV2640: Y0 U Y1 V (U=Cb, V=Cr shared per pixel pair).
static void buildGrid(const uint8_t *buf) {
  memset(cellCnt, 0, sizeof(cellCnt));
  int px = 0;
  for (int i = 0; i < IMG_W * IMG_H * 2; i += 4) {
    int u = buf[i + 1], v = buf[i + 3];
    int x = px % IMG_W, y = px / IMG_W;
    int gy = y >> 1;
    for (int p = 0; p < 2; p++) {
      int luma = buf[i + p * 2];
      int gx = (x + p) >> 1;
      for (int cls = 0; cls < N_CLASSES; cls++)
        if (inRange(luma, u, v, thr[cls])) cellCnt[cls][gy][gx]++;
    }
    px += 2;
  }
}

// Largest connected blob (4-conn BFS on lit cells) within the class ROI.
static Blob findBlob(int cls) {
  Blob best = { false, 0, 0, 0 };
  memset(vis, 0, sizeof(vis));
  const uint8_t y0 = roiY0[cls], y1 = roiY1[cls];

  for (int sy = y0; sy <= y1; sy++) {
    for (int sx = 0; sx < GW; sx++) {
      if (vis[sy][sx] || cellCnt[cls][sy][sx] < CELL_ON) continue;
      // BFS
      uint16_t head = 0, tail = 0;
      uint16_t pixels = 0;
      int minX = sx, maxX = sx, minY = sy, maxY = sy;
      vis[sy][sx] = 1;
      q[tail++] = sy * GW + sx;
      while (head < tail) {
        int cy = q[head] / GW, cx = q[head] % GW;
        head++;
        pixels += cellCnt[cls][cy][cx];
        if (cx < minX) minX = cx;
        if (cx > maxX) maxX = cx;
        if (cy < minY) minY = cy;
        if (cy > maxY) maxY = cy;
        const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
          int nx = cx + dx[d], ny = cy + dy[d];
          if (nx < 0 || nx >= GW || ny < y0 || ny > y1) continue;
          if (vis[ny][nx] || cellCnt[cls][ny][nx] < CELL_ON) continue;
          vis[ny][nx] = 1;
          q[tail++] = ny * GW + nx;
        }
      }
      if (pixels > best.pixels) {
        best.found  = true;
        best.pixels = pixels;
        best.cx     = (minX + maxX + 1) * CELL / 2;   // centre in px
        best.h      = (maxY - minY + 1) * CELL;       // height in px
      }
    }
  }
  if (best.pixels < minPix[cls]) best.found = false;
  return best;
}

static int distanceCm(const Blob &b) {
  if (!b.found || b.h < 2) return 999;
  int d = (int)focalPix * PILLAR_HEIGHT_CM / b.h;
  return d < 0 ? 0 : (d > 999 ? 999 : d);
}

// ====================================================================
// WiFi tuner
// ====================================================================
#if ENABLE_WIFI_TUNER
static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width">
<title>WRO-CAM</title><style>
body{font-family:sans-serif;background:#111;color:#eee;margin:8px}
fieldset{border:1px solid #444;margin:6px 0}
input[type=range]{width:140px}label{display:inline-block;width:34px}
pre{background:#000;padding:6px;font-size:12px}img{image-rendering:pixelated}
</style></head><body>
<h3>WRO-CAM tuner (ESP32-CAM plan B)</h3>
<img id=im width=320 height=240><pre id=st>...</pre>
<fieldset><legend>camera</legend>
AEC auto <input type=checkbox id=aecOn> exp <input type=range id=aecVal min=0 max=1200>
AGC auto <input type=checkbox id=agcOn> gain <input type=range id=agcGain min=0 max=30>
AWB <input type=checkbox id=awbOn> wb_mode <input type=range id=wbMode min=0 max=4>
focal <input type=number id=focal style=width:60px>
</fieldset><div id=sl></div>
<button onclick="fetch('/save').then(()=>alert('saved'))">SAVE to flash</button>
<script>
const N=['RED','GREEN','ORANGE','BLUE','MAGENTA'],L=['Ymin','Ymax','Cbmin','Cbmax','Crmin','Crmax'];
function mk(t){let h='';for(let c=0;c<5;c++){h+='<fieldset><legend>'+N[c]+'</legend>';
for(let i=0;i<6;i++)h+='<label>'+L[i]+'</label><input type=range min=0 max=255 value='+t[c][i]+
' oninput="fetch(\'/set?c='+c+'&i='+i+'&v=\'+this.value)">'+ (i==2?'<br>':'');h+='</fieldset>'}
document.getElementById('sl').innerHTML=h}
fetch('/get').then(r=>r.json()).then(j=>{mk(j.thr);
for(const k of ['aecVal','agcGain','wbMode','focal'])document.getElementById(k).value=j[k];
for(const k of ['aecOn','agcOn','awbOn'])document.getElementById(k).checked=!!j[k];
for(const k of ['aecOn','agcOn','awbOn','aecVal','agcGain','wbMode'])
 document.getElementById(k).onchange=e=>fetch('/cam?'+k+'='+(e.target.type=='checkbox'?(e.target.checked?1:0):e.target.value));
document.getElementById('focal').onchange=e=>fetch('/focal?v='+e.target.value)});
setInterval(()=>fetch('/state').then(r=>r.text()).then(t=>st.textContent=t),400);
(function f(){im.src='/jpg?'+Date.now();im.onload=()=>setTimeout(f,500);im.onerror=()=>setTimeout(f,1500)})();
</script></body></html>
)HTML";

static void handleState() {
  char b[320];
  snprintf(b, sizeof(b),
    "fps:%u cam:%s\nuart: %s\nRED   px:%u cx:%d h:%d d:%dcm\n"
    "GREEN px:%u cx:%d h:%d d:%dcm\nORANGE px:%u\nBLUE   px:%u\n"
    "MAGENTA px:%u cx:%d",
    fps, camOk ? "OK" : "FAIL", lastLine,
    blobs[C_RED].pixels, blobs[C_RED].cx - CENTER_X, blobs[C_RED].h,
    distanceCm(blobs[C_RED]),
    blobs[C_GREEN].pixels, blobs[C_GREEN].cx - CENTER_X, blobs[C_GREEN].h,
    distanceCm(blobs[C_GREEN]),
    blobs[C_ORANGE].pixels, blobs[C_BLUE].pixels,
    blobs[C_MAGENTA].pixels, blobs[C_MAGENTA].cx - CENTER_X);
  server.send(200, "text/plain", b);
}

static void handleGet() {
  String j = "{\"thr\":[";
  for (int c = 0; c < N_CLASSES; c++) {
    j += '[';
    for (int i = 0; i < 6; i++) { j += String(thr[c][i]); if (i < 5) j += ','; }
    j += ']'; if (c < N_CLASSES - 1) j += ',';
  }
  j += "],\"aecOn\":" + String(aecOn) + ",\"aecVal\":" + String(aecVal) +
       ",\"agcOn\":" + String(agcOn) + ",\"agcGain\":" + String(agcGain) +
       ",\"awbOn\":" + String(awbOn) + ",\"wbMode\":" + String(wbMode) +
       ",\"focal\":" + String(focalPix) + "}";
  server.send(200, "application/json", j);
}

static void handleSet() {
  int c = server.arg("c").toInt(), i = server.arg("i").toInt();
  if (c >= 0 && c < N_CLASSES && i >= 0 && i < 6)
    thr[c][i] = server.arg("v").toInt();
  server.send(200, "text/plain", "ok");
}

static void handleCam() {
  if (server.hasArg("aecOn"))   aecOn   = server.arg("aecOn").toInt();
  if (server.hasArg("aecVal"))  aecVal  = server.arg("aecVal").toInt();
  if (server.hasArg("agcOn"))   agcOn   = server.arg("agcOn").toInt();
  if (server.hasArg("agcGain")) agcGain = server.arg("agcGain").toInt();
  if (server.hasArg("awbOn"))   awbOn   = server.arg("awbOn").toInt();
  if (server.hasArg("wbMode"))  wbMode  = server.arg("wbMode").toInt();
  applyCamSettings();
  server.send(200, "text/plain", "ok");
}

static void handleJpg() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { server.send(503, "text/plain", "no frame"); return; }
  uint8_t *jpg = nullptr; size_t jlen = 0;
  bool ok = fmt2jpg(fb->buf, fb->len, fb->width, fb->height,
                    fb->format, 40, &jpg, &jlen);
  esp_camera_fb_return(fb);
  if (!ok) { server.send(503, "text/plain", "jpg fail"); return; }
  server.send_P(200, "image/jpeg", (const char *)jpg, jlen);
  free(jpg);
}
#endif // ENABLE_WIFI_TUNER

// ====================================================================
static uint8_t calcChecksum(const char *s) {
  uint8_t cs = 0;
  while (*s) cs ^= (uint8_t)*s++;
  return cs;
}

void setup() {
#if DISABLE_BROWNOUT
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, HIGH);            // off (active low)

  Serial.begin(115200);                      // USB-TTL debug log
  RobotSerial.begin(115200, SERIAL_8N1, ROBOT_RX_PIN, ROBOT_TX_PIN);

  loadSettings();
  camOk = cameraInit();
  if (camOk) {
    applyCamSettings();
    Serial.println("[CAM] OV2640 up, QQVGA YUV422");
  } else {
    Serial.println("[CAM] FATAL: camera init failed — check module/power");
  }

#if ENABLE_WIFI_TUNER
  WiFi.softAP("WRO-CAM", "wro2026fe");
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/state", handleState);
  server.on("/get", handleGet);
  server.on("/set", handleSet);
  server.on("/cam", handleCam);
  server.on("/jpg", handleJpg);
  server.on("/focal", []() {
    focalPix = server.arg("v").toInt();
    server.send(200, "text/plain", "ok");
  });
  server.on("/save", []() { saveSettings(); server.send(200, "text/plain", "ok"); });
  server.begin();
  Serial.println("[NET] tuner at http://192.168.4.1 (AP WRO-CAM)");
#endif
}

void loop() {
  static uint32_t frames = 0, tFps = millis();

#if ENABLE_WIFI_TUNER
  server.handleClient();
#endif

  if (!camOk) {                              // fast blink = camera dead.
    digitalWrite(LED_STATUS, (millis() / 150) & 1);
    delay(20);                               // stay silent on UART ->
    return;                                  // robot failsafe handles it
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { delay(5); return; }             // dropped frame: skip, no packet
  buildGrid(fb->buf);
  esp_camera_fb_return(fb);

  for (int c = 0; c < N_CLASSES; c++) blobs[c] = findBlob(c);

  int redX = 0, redDist = 999, greenX = 0, greenDist = 999;
  int modeFlag = 0, extraTag = 0;
  if (blobs[C_RED].found) {
    redX = blobs[C_RED].cx - CENTER_X;
    redDist = distanceCm(blobs[C_RED]);
  }
  if (blobs[C_GREEN].found) {
    greenX = blobs[C_GREEN].cx - CENTER_X;
    greenDist = distanceCm(blobs[C_GREEN]);
  }
  if (blobs[C_ORANGE].found)  modeFlag |= 1;
  if (blobs[C_BLUE].found)    modeFlag |= 2;
  if (blobs[C_MAGENTA].found) {
    modeFlag |= 4;
    extraTag = blobs[C_MAGENTA].cx - CENTER_X;
  }

  char data[48];
  snprintf(data, sizeof(data), "%d,%d,%d,%d,%d,%d",
           redX, redDist, greenX, greenDist, modeFlag, extraTag);
  snprintf(lastLine, sizeof(lastLine), "%s*%02X", data, calcChecksum(data));
  RobotSerial.print(lastLine);
  RobotSerial.print('\n');

  // slow heartbeat blink = vision running
  frames++;
  digitalWrite(LED_STATUS, (frames >> 4) & 1);
  if (millis() - tFps >= 1000) {
    fps = frames; frames = 0; tFps = millis();
  }
}
