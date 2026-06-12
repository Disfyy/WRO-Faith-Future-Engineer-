#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_telemetry_wifi.h"

#if WIFI_TELEMETRY

#include <WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP   udp;
static IPAddress bcastIp(192, 168, 4, 255);   // softAP subnet broadcast
static bool      wifiUp = false;

void tlm_wifi_init() {
  // softAP() blocks for a few hundred ms — must run before the task
  // watchdog is armed (200 ms timeout would trip mid-init).
  WiFi.mode(WIFI_AP);
  wifiUp = WiFi.softAP(WIFI_TLM_SSID, WIFI_TLM_PASS, WIFI_TLM_CHANNEL);
  if (wifiUp) {
    udp.begin(WIFI_TLM_PORT);
    Serial.printf("WiFi telemetry: AP \"%s\" up, IP %s, UDP port %d\n",
                  WIFI_TLM_SSID, WiFi.softAPIP().toString().c_str(),
                  (int)WIFI_TLM_PORT);
    Serial.println("## TESTING BUILD - set WIFI_TELEMETRY 0 before competition (Rule 11.10) ##");
  } else {
    Serial.println("WARN: WiFi telemetry AP failed to start - USB serial unaffected");
  }
}

static void sendTo(const IPAddress &ip, const char *line, size_t len) {
  udp.beginPacket(ip, WIFI_TLM_PORT);
  udp.write((const uint8_t *)line, len);
  udp.endPacket();
}

void tlm_wifi_send(const char *line) {
  if (!wifiUp) return;
  size_t len = strlen(line);
  // Broadcast frames are unACKed and dropped by stations in WiFi power
  // save (a laptop receives the first line then dozes). Unicast frames
  // are ACKed/retried by the MAC layer, so ALSO send to the first DHCP
  // leases the softAP hands out (.2-.4 covers every realistic listener).
  sendTo(bcastIp, line, len);
  if (WiFi.softAPgetStationNum() > 0) {
    sendTo(IPAddress(192, 168, 4, 2), line, len);
    sendTo(IPAddress(192, 168, 4, 3), line, len);
    sendTo(IPAddress(192, 168, 4, 4), line, len);
  }
}

#else   // WIFI_TELEMETRY == 0 — competition build: radio never touched

void tlm_wifi_init() {}
void tlm_wifi_send(const char *) { (void)0; }

#endif  // WIFI_TELEMETRY

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
