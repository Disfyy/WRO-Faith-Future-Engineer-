# `src/openmv/` — Vision firmware (OpenMV H7 Plus)

MicroPython firmware running on the OpenMV H7 Plus camera. Detects WRO 2026
field elements (orange/blue lap lines, red/green traffic-sign pillars, magenta
parking block) and streams a compact frame to the ESP32-S3 over UART at
115200 8N1, ~30 Hz.

## Files

| File | Purpose |
|---|---|
| [`openmv_main.py`](openmv_main.py) | Sole script. Deploy to the camera as `main.py` so it auto-runs on power-up. |

## What it sees

Per WRO 2026 Future Engineers rules (cited inline in the script):

| Object | Color | Rule | Real size |
|---|---|---|---|
| Lap line | Orange ≈ RGB(255,102,0) | 13.9 | 20 mm wide |
| Lap line | Blue ≈ RGB(0,51,255) | 13.9 | 20 mm wide |
| Traffic-sign pillar | Red ≈ RGB(238,39,55) | 13.21 | 50×50×100 mm |
| Traffic-sign pillar | Green ≈ RGB(68,214,44) | 13.22 | 50×50×100 mm |
| Parking-lot block | Magenta ≈ RGB(255,0,255) | 13.27 | 200×20×100 mm |

Distances to pillars and the parking block are estimated from blob height
in pixels using the pinhole-camera model (`d = FOCAL_PIX × 10 cm / blob_h_px`).

## UART frame

```
RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
```

Full schema (fields, ranges, bit assignments, XOR checksum):
[`../../docs/guides/WRO_OpenMV_UART_Protocol.md`](../../docs/guides/WRO_OpenMV_UART_Protocol.md).

> **v13 note:** Wall distance (bit 3 of `ModeFlag`) is reserved and always
> zero. The two VL53L1X ToF sensors on the ESP32-S3 own front-distance and
> side-distance — the camera no longer estimates wall range.

## Deploying to the camera

1. Connect the OpenMV H7 Plus over USB to the OpenMV IDE.
2. Run **Tools → Machine Vision → Threshold Editor** under the actual race
   lighting to dial in LAB thresholds for each color (rule 13.21/13.22/13.27
   RGBs are nominal — sensor + lighting drift them).
3. Paste the updated `(L,A,B)` tuples into the `*_THRESHOLD` constants near
   the top of `openmv_main.py`.
4. **File → Save Open Script to OpenMV Cam (As Main Script)**. The camera
   will execute `main.py` on every boot, no host required.

## Hardware wiring (camera side)

| OpenMV pin | Connects to | ESP32-S3 pin |
|---|---|---|
| P4 (UART3 TX) | UART2 RX on ESP32-S3 | GPIO 18 |
| P5 (UART3 RX) | UART2 TX on ESP32-S3 | GPIO 17 |
| 3.3V / GND | Logic supply | shared |

Authoritative wiring map: [`../../docs/guides/WRO_Wiring_Map_v13.md`](../../docs/guides/WRO_Wiring_Map_v13.md).
