/*
 * wro_telemetry_wifi — TESTING-ONLY Wi-Fi mirror of the USB telemetry.
 *
 * Robot opens its own access point (WIFI_TLM_SSID) and broadcasts every
 * telemetry line as one UDP datagram to port WIFI_TLM_PORT. One-way only:
 * no commands are accepted over Wi-Fi, so it cannot drive the robot.
 *
 * Rule 11.10 compliance: WIFI_TELEMETRY 0 in wro_config_v13.h compiles
 * both functions to no-ops and the radio is never initialized.
 *
 * Listen on a laptop joined to the AP:   nc -ul 3333
 */
#pragma once

void tlm_wifi_init();                  // call ONCE in setup(), BEFORE the WDT is armed
void tlm_wifi_send(const char *line);  // non-blocking; safe in the 10 ms loop
