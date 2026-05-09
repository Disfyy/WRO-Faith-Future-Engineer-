# WRO Characteristics Audit

Date: 9 April 2026 — **v11 historical audit, line numbers stale; v13 re-audit pending**
Scope: Compare documented template values with active firmware/runtime values

> ⚠️ **v13 status (May 2026):** This audit was performed against `src/esp32/eps323.cpp` (now `legacy_eps323.cpp`) and the line numbers below no longer point at active code. The active firmware is `src/esp32/wro_v13_main.cpp` plus modular `wro_*.{cpp,h}` files. Tunables now live in `src/esp32/wro_config_v13.h` (gains, thresholds, mode flag) and `src/esp32/wro_hw_config_v13.h` (pin map, encoder/odometry constants). A re-audit against the v13 sources is required.

Compared sources (v11 historical):
- `docs/WRO_Config_Template.h`
- `src/esp32/legacy_eps323.cpp` (was `src/esp32/eps323.cpp`)
- `src/openmv/openmv_main.py`

---

## 1) Summary

- Parameters fully aligned: 10
- Parameters requiring alignment or clarification: 8
- Main mismatch cluster: speed profile and PID defaults

Interpretation:
- Firmware appears intentionally tuned for race mode behavior.
- Template file appears generic and should be versioned per mode/tuning state.

---

## 2) ESP32 configuration comparison

| Parameter | Template value | Firmware value | Status | Anchor |
|-----------|----------------|----------------|--------|--------|
| Servo center | `CFG_SERVO_CENTER = 90` | `SERVO_CENTER = 90` | OK | `docs/WRO_Config_Template.h:7`, `src/esp32/eps323.cpp:54` |
| Servo max right | `135` | `135` | OK | `docs/WRO_Config_Template.h:8`, `src/esp32/eps323.cpp:55` |
| Servo max left | `45` | `45` | OK | `docs/WRO_Config_Template.h:9`, `src/esp32/eps323.cpp:56` |
| Motor max speed | `150` | `OBS=140`, `OPEN=180` | MISMATCH (mode-specific) | `docs/WRO_Config_Template.h:12`, `src/esp32/eps323.cpp:59`, `src/esp32/eps323.cpp:65` |
| Motor turn fast | `120` | `OBS=110`, `OPEN=140` | MISMATCH (mode-specific) | `docs/WRO_Config_Template.h:13`, `src/esp32/eps323.cpp:60`, `src/esp32/eps323.cpp:66` |
| Motor turn slow | `80` | `OBS=75`, `OPEN=95` | MISMATCH (mode-specific) | `docs/WRO_Config_Template.h:14`, `src/esp32/eps323.cpp:61`, `src/esp32/eps323.cpp:67` |
| Motor min speed | `35` | `OBS=35`, `OPEN=40` | PARTIAL MATCH | `docs/WRO_Config_Template.h:15`, `src/esp32/eps323.cpp:62`, `src/esp32/eps323.cpp:68` |
| PID Kp | `0.50` | `0.55` | MISMATCH | `docs/WRO_Config_Template.h:18`, `src/esp32/eps323.cpp:79` |
| PID Ki | `0.001` | `0.002` | MISMATCH | `docs/WRO_Config_Template.h:19`, `src/esp32/eps323.cpp:80` |
| PID Kd | `0.10` | `0.18` | MISMATCH | `docs/WRO_Config_Template.h:20`, `src/esp32/eps323.cpp:81` |
| Gyro Kp | `1.20` | `1.20` | OK | `docs/WRO_Config_Template.h:21`, `src/esp32/eps323.cpp:82` |
| Camera timeout (ms) | `500` | `500` | OK | `docs/WRO_Config_Template.h:24`, `src/esp32/eps323.cpp:139` |
| Loop interval (ms) | `10` | `10` | OK | `docs/WRO_Config_Template.h:25`, `src/esp32/eps323.cpp:142` |
| Encoder turn ticks | `3000` | `3000` | OK | `docs/WRO_Config_Template.h:28`, `src/esp32/eps323.cpp:151` |
| Target laps | `3` | `3` | OK | `docs/WRO_Config_Template.h:35`, `src/esp32/eps323.cpp:160` |
| Lap degrees | `360.0` | `360.0` | OK | `docs/WRO_Config_Template.h:36`, `src/esp32/eps323.cpp:161` |
| E-Stop debounce (ms) | `20` | `20` | OK | `docs/WRO_Config_Template.h:39`, `src/esp32/eps323.cpp:145` |
| Finish blink (ms) | `500` | `250` | MISMATCH | `docs/WRO_Config_Template.h:40`, `src/esp32/eps323.cpp:144` |

---

## 3) OpenMV characteristics baseline

| Parameter | Current value | Status | Anchor |
|-----------|---------------|--------|--------|
| UART | `UART(3, 115200, timeout_char=1000)` | OK | `src/openmv/openmv_main.py:32` |
| Auto gain | `False` | OK | `src/openmv/openmv_main.py:27` |
| Auto white balance | `False` | OK | `src/openmv/openmv_main.py:28` |
| Auto exposure | `False, exposure_us=10000` | OK | `src/openmv/openmv_main.py:29` |
| Focal constant | `2000` | TRACK-CALIBRATION REQUIRED | `src/openmv/openmv_main.py:66` |
| Color thresholds (LAB) | RED/GREEN/ORANGE/BLUE/MAGENTA/WALL set | TRACK-CALIBRATION REQUIRED | `src/openmv/openmv_main.py:40-45` |

---

## 4) Actions required

1. Split `WRO_Config_Template.h` into mode-specific templates (Open vs Obstacle) or annotate as baseline-only.
2. Align documented PID defaults with actual race-tuned firmware values.
3. Align finish blink documentation (`500 ms` vs `250 ms`) to avoid confusion in diagnostics.
4. Keep threshold and focal constant values in test logs per venue lighting.

---

## 5) Verification statement

This audit is document/code level only.
Physical validation still required on hardware for:
- true steering neutrality under load,
- battery sag impact on speed profile,
- final race behavior under official field conditions.
