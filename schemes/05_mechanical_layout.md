# Mechanical Layout (v13) — top-down view

Approximate top-down placement of the major components on the chassis.
Front of the car is at the top of the diagram. Distances are not to scale —
this is a wire-routing reference, not a CAD drawing.

```
                                   FRONT
                  ┌───────────────────────────────────────┐
                  │                                       │
                  │            ┌───────────────┐          │
                  │            │   VL53L1X     │          │
                  │            │   FRONT       │          │
                  │            │  0x30  XSHUT  │          │
                  │            │   → GPIO 15   │          │
                  │            └───────┬───────┘          │
                  │                    │ I²C0 (5 V VIN)   │
                  │                    │                  │
                  │   ┌────────────────┴─────────────┐    │
                  │   │       OpenMV H7 Plus         │    │
                  │   │  ┌────────┐                  │    │
                  │   │  │  CAM   │  UART2 ─────────▶│    │
                  │   │  └────────┘   GPIO 17/18     │    │
                  │   └──────────────────────────────┘    │
                  │                                       │
                  │   ┌──────────────────────────────┐    │
                  │   │     ESP32-S3-DevKitC-1       │    │
                  │   │           N8R8               │    │
                  │   │                              │    │
                  │   │   I²C0 → GPIO 8/9 ───────────┼────┼── (to right side via I²C0 trunk)
                  │   │   I²C1 → GPIO 11/12 ─────────┼────┼── (to right side via I²C1 trunk)
        LEFT SIDE │   │                              │    │ RIGHT SIDE
       ──────────▶│   │   UART2 → GPIO 17/18         │    │◀──────────
                  │   │   PWM   → GPIO 40/41/42      │    │
                  │   │   EN    → GPIO 38/39         │    │
                  │   │   E-STOP IN  ← GPIO 21       │    │
                  │   └──────────────────────────────┘    │
                  │                                       │
                  │     [ICM-20948]              [VL53L1X SIDE]
                  │      0x68                     0x31  XSHUT
                  │      I²C0 (3V3)               → GPIO 16
                  │                               I²C1 (5 V VIN)
                  │                                       │
                  │   ┌──────────────────────────────┐    │
                  │   │       BTS7960 H-Bridge       │    │
                  │   │     B+/B− from VBAT          │    │
                  │   │     VCC logic from 5 V       │    │
                  │   └──────────┬───────────────────┘    │
                  │              │ M+/M−                  │
                  │   ┌──────────┴────────┐               │
                  │   │     Motor 380     │               │
                  │   │   + 3× 0.1 µF EMI │               │
                  │   └──────────┬────────┘               │
                  │              │                        │
                  │       drive shaft                     │
                  │              │                        │
                  │   [AS5600 L] │           [AS5600 R]   │
                  │    0x36      │            0x36        │
                  │    I²C0      │            I²C1        │
                  │              │                        │
                  │   ┌──────────────────────────────┐    │
                  │   │   JX PDI-6221MG steering     │    │
                  │   │   PWM ← GPIO 42 · 50 Hz      │    │
                  │   └──────────────────────────────┘    │
                  │                                       │
                  │     ┌──────────────┐  ┌────────────┐  │
                  │     │   LiPo 2S/3S │  │ KCD3 main  │  │
                  │     │              │  │   switch   │  │
                  │     └──────┬───────┘  └──────┬─────┘  │
                  │            │ +VBAT (red)     │        │
                  │            └────────┬────────┘        │
                  │                     │ to BTS7960 B+   │
                  │                     │ + 5 V buck IN   │
                  │                                       │
                  │     ┌────────────────────────────┐    │
                  │     │    LM2596 / D-SUN buck     │    │
                  │     │    +5 V · 3 A · 470 µF     │    │
                  │     └────────────────────────────┘    │
                  │                                       │
                  │           ⭐ STAR GND ⭐               │
                  │     (single physical lug; tie         │
                  │      LiPo −, BTS B−, V5 GND,          │
                  │      ESP GND, all sensor GNDs)        │
                  │                                       │
                  └───────────────────────────────────────┘
                                   REAR
```

## Wire-routing zones

| Zone | Run | Notes |
|------|-----|-------|
| Power trunk | `+VBAT` LiPo → KCD3 → buck-IN, and LiPo → BTS B+ | Keep thick (≥18 AWG), one bundle, away from I²C |
| Motor pair | BTS M+ / M− → motor | Twisted pair; 3× 0.1 µF EMI caps soldered at motor terminals |
| I²C0 trunk | GPIO 8/9 + 3V3 + GND + IMU/AS5600-L/VL53L1X-F | Short (<20 cm). Pull-ups near the ESP32, not at the sensor |
| I²C1 trunk | GPIO 11/12 + 3V3 + GND + AS5600-R/VL53L1X-S | Short. Route on the opposite side of the chassis from motor wires |
| UART2 | GPIO 17/18 → OpenMV | Crossed RX/TX. Keep <30 cm |
| Servo PWM | GPIO 42 → servo | Single wire, 5 V + GND in same bundle |
| E-Stop | GPIO 21 → switch → GND | Twisted pair; switch must be physically reachable in any FSM state |
| Star ground | Single lug at chassis center | Every GND return ends here, including LiPo − |

## Things to verify physically

- [ ] Magnet gap on each AS5600 is **0.5–3 mm**, diametric polarity facing the chip.
- [ ] VL53L1X front is unobstructed forward, side sensor unobstructed to its target wall side.
- [ ] OpenMV camera tilt/aim covers the floor strip at ~30 cm.
- [ ] No I²C wire routed parallel to a motor wire for more than a few cm.
- [ ] Servo full-sweep doesn't tug any wire bundle off its connector.
- [ ] E-Stop button is mechanically locked-on until released (latching N.O. or button held).
