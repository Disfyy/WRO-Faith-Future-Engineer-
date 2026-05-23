# WRO Wiring Map (v13)

> **Active hardware revision:** v13 — ESP32-S3-DevKitC-1 N8R8 + 2× AS5600 (dual native I²C) + 2× VL53L1X (XSHUT runtime address remap).
> The long-form reference lives at [`docs/guides/WRO_Wiring_Map_v13.md`](../docs/guides/WRO_Wiring_Map_v13.md);
> this file is the at-a-glance summary kept next to the diagrams.

---

## 1. ESP32-S3 pin map

### Headline pin assignments

```
┌──────────┬────────────┬──────────────────────────────────────────────┐
│ Function │   GPIO     │ Connection                                   │
├──────────┼────────────┼──────────────────────────────────────────────┤
│ I²C0 SDA │   8        │ ICM-20948 0x68 · AS5600 L 0x36 · VL53L1X F   │
│ I²C0 SCL │   9        │ same bus                                     │
│ I²C1 SDA │  11        │ AS5600 R 0x36 · VL53L1X Side                 │
│ I²C1 SCL │  12        │ same bus                                     │
│ XSHUT F  │  15        │ VL53L1X Front (boot-LOW → release → 0x30)    │
│ XSHUT S  │  16        │ VL53L1X Side  (boot-LOW → release → 0x31)    │
│ XSHUT 3  │  47        │ reserved — optional 3rd VL53L1X              │
│ UART2 RX │  17        │ ← OpenMV TX                                  │
│ UART2 TX │  18        │ → OpenMV RX                                  │
│ BTS R_EN │  38        │ BTS7960 enable A                             │
│ BTS L_EN │  39        │ BTS7960 enable B                             │
│ BTS RPWM │  40        │ forward PWM (LEDC Ch0)                       │
│ BTS LPWM │  41        │ reverse PWM (LEDC Ch1)                       │
│ Servo PWM│  42        │ JX PDI-6221MG (LEDC Ch2 @ 50 Hz)             │
│ E-Stop   │  21        │ button (INPUT_PULLUP, active LOW, HW INT)    │
│ Status LED│  2        │ + 220 Ω → LED → GND                          │
│ RGB LED  │  48        │ onboard WS2812 on the DevKit                 │
└──────────┴────────────┴──────────────────────────────────────────────┘
```

### Strapping / boot pins to keep clear

- **GPIO 0** — boot mode, do not pull externally.
- **GPIO 3** — strapping (JTAG vs USB), leave floating or default.
- **GPIO 45, 46** — strapping for SPI flash voltage; do not use.

---

## 2. I²C address map

```
┌────────────┬───────────┬──────────────────────┬─────────────────┐
│ Bus        │ Address   │ Device               │ Rail            │
├────────────┼───────────┼──────────────────────┼─────────────────┤
│ Wire  (0)  │ 0x68      │ ICM-20948            │ 3.3 V           │
│ Wire  (0)  │ 0x0C      │ AK09916 (mag in IMU) │ 3.3 V (aux bus) │
│ Wire  (0)  │ 0x36      │ AS5600 Left          │ 3.3 V           │
│ Wire  (0)  │ 0x29→0x30 │ VL53L1X Front        │ 5 V (VIN)       │
│ Wire1 (1)  │ 0x36      │ AS5600 Right         │ 3.3 V           │
│ Wire1 (1)  │ 0x29→0x31 │ VL53L1X Side         │ 5 V (VIN)       │
└────────────┴───────────┴──────────────────────┴─────────────────┘
```

The two AS5600 share a fixed factory address (0x36). Splitting the bus
into Wire + Wire1 lets both live without a TCA9548A mux.

The two VL53L1X also share the same default (0x29). We boot with both
XSHUTs LOW, then bring them up one at a time and write each to a unique
runtime address in `vl53l1x_dual.h`.

---

## 3. Power rails

```
┌─────────────┬───────────────────────┬───────────────────────────────────┐
│ Rail        │ Source                │ Loads                             │
├─────────────┼───────────────────────┼───────────────────────────────────┤
│ +VBAT (raw) │ LiPo 2S/3S → KCD3 sw  │ BTS7960 B+, 5 V buck input        │
│ +5 V        │ LM2596 / D-SUN buck   │ ESP32-S3 VIN, OpenMV VIN, servo,  │
│             │ (3 A, 470 µF + 0.1 µF)│ VL53L1X × 2 VIN, BTS7960 logic VCC│
│ +3.3 V      │ ESP32-S3 onboard LDO  │ ICM-20948, AS5600 × 2,            │
│             │                       │ 4.7 kΩ I²C pull-ups (both buses)  │
│ GND         │ Star ground point     │ EVERYTHING returns here           │
└─────────────┴───────────────────────┴───────────────────────────────────┘
```

**Rule of thumb:** if a sensor's datasheet says 2.6–3.6 V, use the 3.3 V
rail. If it says "4.5–5.5 V VIN with onboard LDO," use 5 V. Never mix.

---

## 4. UART protocol (OpenMV → ESP32)

```
GPIO 18 (TX) ──────────▶ OpenMV RX
GPIO 17 (RX) ◀────────── OpenMV TX

Baud: 115200, 8N1, no flow control
Payload (ASCII v3):
  RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
  └──── decimal ints ────┘   └ char ┘ └tag┘ └XOR cs┘
```

Full protocol spec: [`docs/guides/WRO_OpenMV_UART_Protocol.md`](../docs/guides/WRO_OpenMV_UART_Protocol.md).

---

## 5. Cross-revision changes

| Component | v11           | v12 (planned, skipped) | v13 (current)              |
|-----------|---------------|------------------------|----------------------------|
| MCU       | ESP32 DevKitC V4 | ESP32-S3-DevKitC-1 N8R8 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders  | 2× AS5600 + mux  | 2× AS5048A (SPI)        | 2× AS5600 (dual native I²C) |
| Distance  | VL53L1X + mux    | TFMini-S (UART, 12 m)   | VL53L1X (XSHUT remap)      |
| I²C mux   | TCA9548A @ 0x70  | none                    | none — replaced by topology |
| I²C buses | 1 (via mux)      | 1                       | 2 (Wire + Wire1)           |
| Odometry  | 277 ticks/cm     | 1110 ticks/cm           | 277 ticks/cm               |

Step-by-step migration notes: [`docs/strategy/WRO_Migration_v12_to_v13.md`](../docs/strategy/WRO_Migration_v12_to_v13.md).

---

## 6. Pre-flight pin checks

Run these before every bench session:

1. **`SCAN_I2C` build target** — `Wire` shows `0x68, 0x36, 0x30`; `Wire1` shows `0x36, 0x31`.
2. **`TEST_ENCODERS` build target** — both AS5600 sweep 0–4095 cleanly when wheels spin.
3. **`TEST_DISTANCE` build target** — both VL53L1X return sensible mm at 0.1 m and 1 m.
4. **`TEST_SERVO_CAL`** — re-confirm `SERVO_CENTER_US` matches mechanical centre.
5. **E-Stop physical** — press → `SAFE_STOP` instant, release → resume.

If any of these fail, fix wiring before touching firmware.
