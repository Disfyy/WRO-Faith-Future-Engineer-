# 🔧 WRO Future Engineers — Full Robot Assembly Guide

> **Team Faith | v13 Hardware (May 2026)**
>
> This document covers building the robot **from unboxing to first run**. Every step has verification points (✅), photo cues 📸, and time estimates ⏱️. **Don't move on until the current step passes.**
>
> **v13 hardware (May 2026):** ESP32-S3-DevKitC-1 N8R8 main board + 2× AS5600 encoders on dual native I2C + 2× VL53L1X distance sensors with XSHUT-based runtime address remapping + ICM-20948 IMU + OpenMV H7 Plus camera + BTS7960 motor driver + JX PDI-6221MG servo. **No TCA9548A I2C multiplexer.** Authoritative references for v13:
> - Pin map: [`docs/guides/WRO_Wiring_Map_v13.md`](WRO_Wiring_Map_v13.md)
> - v12 → v13 migration: [`docs/strategy/WRO_Migration_v12_to_v13.md`](../strategy/WRO_Migration_v12_to_v13.md)
> - Active firmware: `src/esp32/wro_v13_main.cpp` (target 11)
> - I2C scanner: `src/esp32/scan_i2c_v13.cpp` (target 2, scans both buses)

---

## Stage navigation

| # | Stage | Difficulty | ⏱️ Time |
|:-:|------|:---------:|:--------:|
| 1 | [Chassis prep](#stage-1-chassis-prep) | 🟢 | 2h |
| 2 | [Power chain](#stage-2-power-chain) | 🟡 | 3-4h |
| 3 | [Motor and BTS7960 driver](#stage-3-motor-and-bts7960-driver) | 🟢 | 1-2h |
| 4 | [Steering servo](#stage-4-steering-servo) | 🟢 | 1h |
| 5 | [ESP32-S3 main board](#stage-5-esp32-s3-main-board) | 🟢 | 1h |
| 6 | [Dual I2C buses](#stage-6-dual-i2c-buses) | 🟡 | 2h |
| 7 | [AS5600 encoders](#stage-7-as5600-encoders) | 🔴 | 3-4h |
| 8 | [IMU (ICM-20948)](#stage-8-imu-icm-20948) | 🟡 | 2h |
| 9 | [OpenMV H7 Plus camera](#stage-9-openmv-h7-plus-camera) | 🟡 | 2h |
| 10 | [VL53L1X ToF sensors](#stage-10-vl53l1x-tof-sensors) | 🟡 | 2h |
| 11 | [E-Stop button](#stage-11-e-stop-button) | 🟢 | 30m |
| 12 | [Status LED](#stage-12-status-led) | 🟢 | 15m |
| 13 | [Final wire dressing](#stage-13-final-wire-dressing) | 🟡 | 2-3h |
| 14 | [Firmware flashing](#stage-14-firmware-flashing) | 🟡 | 2h |
| 15 | [Calibration and first run](#stage-15-calibration-and-first-run) | 🔴 | 4-6h |

---

## Tools and consumables

### Required tools

| Tool | Purpose |
|------|---------|
| 25-40 W soldering iron + Sn60/Pb40 | Connectors, caps, resistors |
| Phillips PH1/PH2 screwdriver | HSP chassis, sensor mounts |
| Hex keys 1.5/2/2.5 mm | HSP chassis, pinion, encoders |
| Digital multimeter | Voltage, continuity, short-circuit checks — **every stage** |
| Caliper (0.1 mm resolution) | Wheel diameter for odometry |
| Wire cutters + stripper | Wires |
| Heat gun (or lighter) | Heat-shrink tubing |
| Anti-static tweezers | SMD parts, AS5600 magnets |

### Consumables

- Heat-shrink tubing assortment (3 / 5 / 8 mm)
- Sn60/Pb40 1 mm solder, ~1 m
- Flux (rosin or LTI-120), 1 unit
- Cable ties 100 mm — 30+
- Double-sided 3M VHB tape
- Dupont male/female jumpers — 40
- Insulating tape (emergency)
- Super glue (AS5600 magnet fixation)
- Hot glue, 3-4 sticks
- 3 mm foam, 5×5 cm (IMU vibration isolation)

---

## BOM check before you start

> ⚠️ **STOP** — verify every component before you start. Realizing a part is missing on stage 10 burns a whole day.

### Mechanics
- [ ] HSP 94182 1/16 4WD chassis (assembled: wheels + suspension + 380 motor)
- [ ] 16T M0.6 pinion, Ø2.3 mm bore (verify pitch — if stock is M0.48, source M0.6)
- [ ] JX PDI-6221MG servo (with included horns)

### Electronics — controllers
- [ ] **ESP32-S3-DevKitC-1 N8R8** (USB-C, native USB-OTG, all-bidirectional GPIOs)
- [ ] OpenMV Cam H7 Plus (with camera module + USB cable)

### Electronics — sensors and drivers
- [ ] BTS7960 43A motor driver (8-pin connector)
- [ ] GY-ICM20948V2 IMU (verify silkscreen says **ICM-20948**, not MPU-6050)
- [ ] AS5600 magnetic encoder × 2 (each kit includes a Ø6 mm diametrically magnetized magnet)
- [ ] VL53L1X ToF × 2 (Pololu or Adafruit breakout)

### Power
- [ ] Li-ion / LiPo 7.4 V 2000 mAh, T-plug (charged!)
- [ ] LM2596 DC-DC step-down × 2
- [ ] T-plug → XT60 adapter + spare XT60 pair
- [ ] KCD3 toggle switch × 1 (only one — WRO Rule 9.10)
- [ ] 0.1 µF ceramic caps (104) × 10
- [ ] 4.7 kΩ resistors × 4 (I2C pull-ups, two per bus)
- [ ] 220 Ω resistor × 1 (LED)
- [ ] 10 kΩ resistor × 1 (E-Stop pull-up, optional — internal pull-up usually sufficient)

---

## Stage 1: Chassis prep

> ⏱️ ~2 hours | 🟢 Easy

### 1.1. Strip the unwanted parts

The HSP 94182 ships with a stock receiver, ESC, and shell — all of these come off:

1. Pop the body shell off (4 corner clips, pull up).
2. Locate and unplug the stock ESC (3 wires to the motor + 2 power leads).
3. Unplug the stock receiver (servo connector to ESC).
4. **Carefully** remove both modules.
5. Keep on the chassis: suspension, 380 motor, servo bracket, wheels, drive shafts.

> ⛔ **Do NOT remove** the steering rods or suspension arms. Their geometry is set at the factory; reassembling without photos costs 2 hours.

📸 *Photograph the chassis before and after stripping — useful for the `v-photos/` folder.*

### 1.2. Pinion swap (if needed)

Verify the stock pinion pitch. If it's M0.48 or M0.5, swap to M0.6:

1. Loosen the pinion grub screw (1.5 mm hex).
2. Pull the pinion off the motor shaft. If it sticks, heat with a hot-air gun for 10 s (50-60 °C) and try again, or use a small puller.
3. Slide the **16T M0.6 Ø2.3 mm** pinion onto the shaft.
4. Tighten the grub screw onto the **flat** of the shaft.
5. **Backlash check:** slip a sheet of paper between pinion and spur, rotate, and pull the paper out. Aim for ~0.1 mm.

| Symptom | Cause | Fix |
|---------|-------|-----|
| Gears whining | Backlash too tight | Move motor 0.1 mm out |
| Gears clicking | Backlash too loose | Move motor in |
| Vibration at high RPM | Grub screw not on the flat | Reposition on the flat |

### 1.3. Mount the JX PDI-6221MG servo

1. Insert the servo into the HSP servo bay (rectangular cutout).
2. Secure with 4 screws from the servo's kit (use included rubber grommets if any).
3. **Servo horn — critical:** with **power off**, manually rotate the servo to its mechanical center, then fit the horn so the front wheels point straight, and tighten the center screw.
4. Connect the steering tie rod to the horn.

### 1.4. Wheel diameter for odometry

This is **critical** for accurate distance counting:

1. Remove one wheel.
2. Caliper the **outer** diameter of the tire (squeeze lightly to simulate load).
3. Record: **D = ___ mm**.
4. Compute ticks/cm:
```
Circumference C = π × D mm
TICKS_PER_CM = 4096 / (C / 10)

Example: D = 47 mm -> C = 147.7 mm -> 4096 / 14.77 ≈ 277 ticks/cm
```
5. Update `TICKS_PER_CM` in `src/esp32/wro_hw_config_v13.h` if your wheels differ from the default 47 mm.

✅ **Stage 1 checklist:**
- [ ] Chassis is clean — ESC, receiver, body removed
- [ ] Pinion installed; gears are quiet
- [ ] Servo installed; horn at mechanical center; wheels straight
- [ ] Wheel diameter measured: D = ___ mm, TICKS_PER_CM = ___
- [ ] 📸 Clean-chassis photo taken

---

## Stage 2: Power chain

> ⏱️ 3-4 hours | 🟡 Medium difficulty
>
> ⛔ **Polarity!** Reversing + and − on the LiPo = **instant** electronics meltdown. Verify every wire with a multimeter **before** plugging it in.

### 2.1. Power architecture (WRO 2026 compliant)

> **WRO Rule 9.10:** "Only ONE switch is allowed to power on the vehicle."

```
LiPo 7.4V ──T-plug──> T->XT60 adapter ──> XT60
                                          │
                                  KCD3 (the only switch)
                                          │
                            ┌─────────────┼─────────────┐
                            │             │             │
                            v             v             v
                      BTS7960 (VCC)   LM2596 #1     LM2596 #2
                      7.4V direct    -> 5.00V       -> 3.30V
                      (motor)       (ESP32, servo)  (sensors)
```

**Loads per rail:**

| Rail | Voltage | Loads | Peak current |
|------|:-------:|-------|:------------:|
| **Motor** | 7.4 V | BTS7960 → 380 motor | ~8 A peak |
| **5 V** | 5.00 V | ESP32-S3 VIN, JX servo, OpenMV VIN, VL53L1X VIN | ~3 A |
| **3.3 V** | 3.30 V | ICM-20948, AS5600 ×2 | ~0.2 A |

> Note: ESP32-S3 has its own onboard 3.3 V regulator — it powers the I2C-bus pull-ups and the AS5600s directly from the 3.3 V pin. The external LM2596 #2 is mostly redundant in v13; you can omit it if the breakout boards have onboard LDOs.

### 2.2. Set up LM2596 BEFORE plugging in any electronics

> ⛔ Set the voltage **first**, then connect the load. If LM2596 is at 12 V from the factory, the ESP32 dies.

**LM2596 #1 → 5 V:**
1. Connect IN+ / IN− to the battery via temporary alligator clips.
2. Power on.
3. Multimeter on OUT+/OUT−.
4. Slowly turn the trim pot until it reads **5.00 V ± 0.05 V**.
5. Verify under load: 100 Ω resistor (50 mA) — voltage should not sag.
6. Power off, mark the regulator with "5V".

**LM2596 #2 → 3.3 V** (optional with onboard breakout LDOs):
1. Same procedure, target **3.30 V ± 0.05 V**.
2. Mark "3.3V".

### 2.3. Solder the power wiring

1. Cut 14 AWG: red 30 cm + black 30 cm.
2. Strip 5 mm on every end.
3. Solder XT60 to the wires from the battery (red → +, black → −).
4. From the XT60 **red (+)**, run through KCD3 and split into 3:
   - → BTS7960 B+ (14 AWG, motor current!)
   - → LM2596 #1 IN+ (22 AWG OK)
   - → LM2596 #2 IN+ (22 AWG OK, if used)
5. **Black (−)** from XT60 splits into 3:
   - → BTS7960 B−
   - → LM2596 #1 IN−
   - → LM2596 #2 IN−
6. Heat-shrink every joint (5 mm).

### 2.4. Motor EMI filtering

> The 380 motor produces strong EMI. Without filtering, I2C hangs, UART drops frames, the IMU drifts.

**3× 0.1 µF ceramic caps on the motor:**
```
        0.1µF
  (+)────┤├────(−)        between motor terminals
  (+)────┤├────(case)      + to motor case
  (−)────┤├────(case)      − to motor case
```

Plus:
- 0.1 µF on BTS7960 VCC-GND (right at the board)
- 0.1 µF on LM2596 #1 OUT-GND
- 0.1 µF on LM2596 #2 OUT-GND (if used)
- **470 µF electrolytic** on the 5 V rail (catches motor-start sag)

### 2.5. Verify with a multimeter (KCD3 ON)

| # | Test | Expected | If wrong |
|:-:|------|:--------:|----------|
| 1 | LM2596 #1 OUT | 5.00 V ±0.05 | Re-trim |
| 2 | LM2596 #2 OUT | 3.30 V ±0.05 | Re-trim |
| 3 | Short test 7.4 V+ ↔ GND | open (∞) | ⛔ Find and remove the short |
| 4 | Short test 5 V+ ↔ GND | open | ⛔ |
| 5 | Short test 3.3 V+ ↔ GND | open | ⛔ |
| 6 | KCD3 OFF → battery V | 0 V on every rail | Switch is bad |

✅ **Stage 2 checklist:**
- [ ] LM2596 #1: 5.00 V ± 0.05 V (marked)
- [ ] LM2596 #2: 3.30 V ± 0.05 V (marked) or omitted with onboard LDOs
- [ ] KCD3 is the only switch and gates everything
- [ ] Caps installed: 3 on the motor + 1 on BTS7960 + 1 per LM2596 + 1× 470 µF electrolytic on 5 V
- [ ] All joints heat-shrunk
- [ ] No shorts — multimeter confirms
- [ ] 📸 Power harness photo

---

## Stage 3: Motor and BTS7960 driver

> ⏱️ 1-2 hours | 🟢 Easy

### 3.1. BTS7960 wiring (v13 pin map)

| BTS7960 pin | Goes to | Wire | Note |
|-------------|---------|:----:|------|
| **B+** | 7.4 V from KCD3 (+) | 14 AWG red | Motor power |
| **B−** | 7.4 V from KCD3 (−) | 14 AWG black | Motor ground |
| **M+** | Motor + terminal | 14 AWG | |
| **M−** | Motor − terminal | 14 AWG | |
| **VCC** (logic) | 5 V from LM2596 #1 | Dupont | |
| **GND** (logic) | Common GND | Dupont | ⚠️ star-grounded |
| **R_EN** | ESP32-S3 **GPIO 38** | Dupont | HIGH = enabled |
| **L_EN** | ESP32-S3 **GPIO 39** | Dupont | HIGH = enabled |
| **RPWM** | ESP32-S3 **GPIO 40** | Dupont | LEDC Ch0 — forward |
| **LPWM** | ESP32-S3 **GPIO 41** | Dupont | LEDC Ch1 — reverse |

> R_EN and L_EN must be HIGH for the driver to run. The firmware does that in `setup()`. If the motor doesn't spin, multimeter these two first — should read 3.3 V.

### 3.2. Direction test

1. Wire everything per the table.
2. Flash the bench-test firmware (target 10).
3. Send the `f` (forward) command in Serial Monitor.
4. If the wheels spin **backwards**, swap M+ and M− at the BTS7960.

✅ **Stage 3 checklist:**
- [ ] Motor turns the right way on "forward"
- [ ] BTS7960 isn't hot at idle (warm = OK; hot = ⛔)
- [ ] Motor caps installed (stage 2)

---

## Stage 4: Steering servo

> ⏱️ ~1 hour | 🟢 Easy

### 4.1. JX PDI-6221MG wiring (v13)

| Servo wire | Goes to | Color |
|:----------:|---------|:-----:|
| **+** (power) | 5 V from LM2596 #1 | Red |
| **−** (ground) | Common GND | Brown |
| **Signal** | ESP32-S3 **GPIO 42** | Orange |

> ⛔ **Do NOT** power the servo from the ESP32-S3's 3.3 V pin. The PDI-6221MG draws up to **2.5 A** under load and will fry the onboard regulator instantly.

### 4.2. Center calibration

Use the dedicated sketch [`sketches/servo_calibrate/servo_calibrate.ino`](../../sketches/servo_calibrate/servo_calibrate.ino) — it walks you through fine-tuning center / left / right with `+` `-` keys and prints the values to paste into `wro_hw_config_v13.h`.

### 4.3. Range test

After calibration the servo should swing left ↔ right without buzzing or binding. If it stalls at an end-stop, increase `SERVO_MARGIN_US` in `wro_config_v13.h` (default 60 µs).

✅ **Stage 4 checklist:**
- [ ] Servo centered (wheels straight at SERVO_CENTER_US)
- [ ] Full range left to right is smooth
- [ ] Servo runs from 5 V (LM2596), not from the ESP32-S3
- [ ] Tie rod connected to the horn

---

## Stage 5: ESP32-S3 main board

> ⏱️ ~1 hour | 🟢 Easy

### 5.1. Mount

1. Drop the ESP32-S3-DevKitC-1 into a breadboard or sockets via header pins.
2. Mount the breadboard to the chassis with 3M VHB.
3. Keep the **USB-C port accessible** for flashing and debug.

> ⚠️ Keep the ESP32-S3 **at least 5 cm from the motor and BTS7960**. Motor EMI corrupts I2C and UART otherwise.

### 5.2. Star ground (critical!)

The most common failure cause is no common ground. All grounds meet at **one point** on the breadboard:

```
                   ┌── ESP32-S3 GND
                   ├── BTS7960 logic GND
common GND ────────┼── LM2596 #1 GND
(one node)         ├── LM2596 #2 GND
                   ├── OpenMV GND
                   └── every sensor GND
```

> ⛔ **Do NOT** chain GND through the components in series. That creates ground loops; I2C will fail intermittently.

### 5.3. Power the ESP32-S3

| ESP32-S3 pin | Connection |
|:------------:|------------|
| **VIN** (or 5V) | 5 V from LM2596 #1 |
| **GND** | Common GND |

> ⛔ **Do NOT power VIN and USB at the same time.** When debugging via USB-C, disconnect VIN.

### 5.4. What can/can't go through the breadboard

The breadboard is a signal/distribution bus, not a power bus. OK to route:
- GPIO signals (PWM, UART, I2C)
- Sensor 3.3 V supplies
- The single star-GND node

Do NOT route:
- Motor power (7.4 V → BTS7960)
- Servo 5 V under load (up to 2.5 A)
- Any line >0.5 A

Recommendation: run the servo's 5 V on its **own pair** straight from LM2596, with a 470-1000 µF electrolytic close to the servo.

✅ **Stage 5 checklist:**
- [ ] ESP32-S3 powers on at 5 V (status LED present)
- [ ] Serial Monitor talks at 115200 baud
- [ ] Common GND verified (multimeter shows 0 Ω between every GND)
- [ ] ESP32-S3 mounted >5 cm from motor

---

## Stage 6: Dual I2C buses

> ⏱️ ~2 hours | 🟡 Medium

### 6.1. Why two buses (instead of a TCA9548A mux)

The v11 build used a TCA9548A I2C multiplexer to share the bus across two AS5600 encoders (both fixed at `0x36`) and two VL53L1X sensors (both default `0x29`). **The mux burned out and is gone.** v13 takes a different approach:

- **AS5600 conflict** is solved by the bus split — one AS5600 on `Wire` (I2C0), the other on `Wire1` (I2C1). The ESP32-S3 has two native I2C peripherals.
- **VL53L1X conflict** is solved at boot via XSHUT pins: hold every VL53L1X in reset, bring them up one at a time, and rewrite each one's I2C address in RAM.

### 6.2. I2C0 wiring (Wire)

| ESP32-S3 pin | Signal | Devices on this bus |
|:------------:|--------|---------------------|
| **GPIO 8** | SDA | ICM-20948 (0x68) + AS5600 Left (0x36) + VL53L1X Front (0x29 → 0x30) |
| **GPIO 9** | SCL | same |

### 6.3. I2C1 wiring (Wire1)

| ESP32-S3 pin | Signal | Devices on this bus |
|:------------:|--------|---------------------|
| **GPIO 11** | SDA | AS5600 Right (0x36) + VL53L1X Side (0x29 → 0x31) |
| **GPIO 12** | SCL | same |

### 6.4. Pull-ups (mandatory)

> The ESP32-S3 internal pull-ups are weak (~45 kΩ). At 400 kHz with >10 cm wires they're not enough.

Each I2C bus needs **4.7 kΩ pull-ups**:
- **SDA → 3.3 V**
- **SCL → 3.3 V**

Many breakout boards already have these populated. Verify with a meter before adding more — too many in parallel pulls the bus too hard. Aim for ~2.5 kΩ effective.

> Wire length: keep each bus under ~30 cm. If you must go longer, drop `Wire.setClock()` from 400000 to 100000.

### 6.5. Diagnostic — run the dual-bus I2C scanner

1. In `wro_build_target.h`, set `WRO_ACTIVE_TARGET = 2` (`WRO_TARGET_SCAN_I2C`).
2. Flash and open Serial Monitor at 115200.
3. Expected output (BEFORE VL53L1X are remapped — the scanner runs before the main firmware):
```
=== I2C0 (Wire) ===
  0x68 -- ICM-20948 IMU (AD0 -> GND)
  0x36 -- AS5600 magnetic encoder
  0x29 -- VL53L1X (default - pre-remap)

=== I2C1 (Wire1) ===
  0x36 -- AS5600 magnetic encoder
  0x29 -- VL53L1X (default - pre-remap)
```

> **0x70** = TCA9548A mux. If you see that, you're scanning the wrong board (the mux is gone in v13).

### 6.6. Recovery / rollback procedure

If the bus is unstable after two scanner runs, fall back to a minimal config:
1. Power off (KCD3 OFF), wait 10 s.
2. Disconnect the AS5600 / VL53L1X — leave only the ICM-20948 on I2C0.
3. Run the scanner — only `0x68` should appear.
4. Add devices **one at a time** in this order: AS5600 Left, VL53L1X Front (I2C0), AS5600 Right (I2C1), VL53L1X Side (I2C1). Run the scanner after each addition.
5. Whichever device drops other devices off the bus = the one with the wiring fault.

Pass criterion: 3 consecutive scans with stable address lists.

✅ **Stage 6 checklist:**
- [ ] I2C0 scanner finds 0x68 + 0x36 + 0x29
- [ ] I2C1 scanner finds 0x36 + 0x29
- [ ] 4.7 kΩ pull-ups present on both buses
- [ ] No 0x70 (mux is supposed to be gone)
- [ ] Wires under 30 cm

---

## Stage 7: AS5600 encoders

> ⏱️ 3-4 hours | 🔴 Hardest mechanical step

### 7.1. How AS5600 works

AS5600 is a 12-bit magnetic encoder. A diametrically magnetized Ø6 mm magnet glued to a rotating shaft is read via Hall sensors → 4096 positions per revolution.

### 7.2. Magnet mounting

> ⛔ **The hardest mechanical step.** The magnet must sit **directly above the chip center**, with a **0.5-3 mm air gap**. >1 mm offset from center = invalid data.

Per encoder (Left → I2C0, Right → I2C1):

1. Find a shaft that rotates in lock-step with the wheel (half-shaft / drive cup).
2. Glue the Ø6 mm magnet to the **end of the shaft**:
   - Tiny drop of super glue (don't get glue on the face that talks to the chip), or
   - Thin 3M double-sided tape.
3. Mount the AS5600 PCB **facing the magnet**, 1-2 mm gap.
4. **Test:** rotate the wheel by hand; the AS5600 reading should sweep smoothly 0 → 4095 → 0.

### 7.3. Wiring (one per bus)

| AS5600 pin | Goes to (Left enc) | Goes to (Right enc) |
|:----------:|--------------------|--------------------|
| VCC | 3.3 V | 3.3 V |
| GND | Common GND | Common GND |
| SDA | ESP32-S3 **GPIO 8** (I2C0) | ESP32-S3 **GPIO 11** (I2C1) |
| SCL | ESP32-S3 **GPIO 9** (I2C0) | ESP32-S3 **GPIO 12** (I2C1) |
| DIR | GND or floating | GND or floating |

### 7.4. Sanity-check in firmware

Set `WRO_ACTIVE_TARGET = 8` (`WRO_TARGET_TEST_ENCODERS`), flash, and watch Serial. While turning each wheel by hand:
- ✅ raw value sweeps 0 → 500 → 1000 → ... → 4095 → 0 smoothly
- ⛔ jumps >100 ticks per sample = magnet too far away or off-center

### 7.5. Magnet recovery / rollback

If readings are noisy (jumps, periodic -1, sudden odometry resets):
1. Power off.
2. Pull the AS5600 PCB and gently remove the magnet with tweezers.
3. Clean glue residue with isopropyl alcohol.
4. Re-glue the magnet centered, 1-2 mm gap, retest.
5. If the issue stays, swap the Left and Right modules — that proves whether the fault is in the PCB or the mount.

Symptom dictionary:
- jumps >100 / sample → magnet off-center
- periodic dropouts → gap too large
- random `SAFE_STOP` from encoder fail-counter → poor mechanical fixation, magnet shifts under vibration

✅ **Stage 7 checklist:**
- [ ] Left encoder visible at 0x36 on I2C0
- [ ] Right encoder visible at 0x36 on I2C1
- [ ] Left wheel: smooth value change when rotating by hand
- [ ] Right wheel: smooth value change when rotating by hand
- [ ] Magnets are firmly attached (shake the robot — they should not move)

---

## Stage 8: IMU (ICM-20948)

> ⏱️ ~2 hours | 🟡 Medium

### 8.1. GY-ICM20948V2 wiring (v13: AD0 → GND)

| IMU pin | Goes to | Note |
|:-------:|---------|------|
| VCC | 3.3 V | ⚠️ Not 5 V! |
| GND | GND | |
| SDA | ESP32-S3 **GPIO 8** (I2C0) | shares the bus with AS5600 Left + VL53L1X Front |
| SCL | ESP32-S3 **GPIO 9** (I2C0) | |
| AD0 | **GND** | Address = 0x68 |

> ⚠️ AD0 must go to **GND**. The v13 firmware expects **0x68**. (v11 used 0x69 with AD0 = VCC; the convention changed during the v12/v13 work.)

### 8.2. Mounting

1. Mount the IMU **horizontally** (Z axis up).
2. X axis points **forward**.
3. **Vibration isolation (mandatory):** 3 mm foam + 3M tape between IMU and chassis.

> Without isolation the gyro drifts 3-5× faster from motor vibration.

### 8.3. Gyro calibration

Automatic at boot (`imu_calibrate_gyro()`):
- Robot stays still for 3 seconds.
- Averages 300 Z-axis samples.
- Result is `gyroZbias`. If bias > 0.05 you have vibration.

### 8.4. IMU recovery / rollback

If the IMU misbehaves:
1. Verify VCC = 3.3 V, AD0 = GND, address = 0x68.
2. Recalibrate with the robot fully still (wheels off the ground).
3. Disconnect the motor temporarily and recalibrate to isolate vibration.

Roll-back rules:
- `gyroZbias > 0.05` two runs in a row → revisit the foam isolation
- Static yaw drift >2°/30 s → replace the I2C wires and shorten them
- IMU drops off the scanner intermittently → redo Stage 6.6

✅ **Stage 8 checklist:**
- [ ] IMU visible at 0x68 on I2C0
- [ ] `imu_calibrate_gyro()` bias < 0.05
- [ ] Yaw moves smoothly when you rotate the robot by hand
- [ ] IMU is on foam, level, X axis forward

---

## Stage 9: OpenMV H7 Plus camera

> ⏱️ ~2 hours | 🟡 Medium

### 9.1. UART2 wiring (ESP32-S3 ↔ OpenMV)

| OpenMV pin | ESP32-S3 pin | Description |
|:----------:|:------------:|-------------|
| **P4 (TX)** | **GPIO 17 (RX)** | Camera transmits → ESP32-S3 receives |
| **P5 (RX)** | **GPIO 18 (TX)** | ESP32-S3 transmits → camera receives |
| **GND** | **GND** | Common ground |
| **VIN** | **5 V from LM2596 #1** | Camera power |

> ⛔ **TX ↔ RX must be crossed!** TX → TX won't fry anything but no data flows.
> Levels: OpenMV H7 Plus is 3.3 V logic; ESP32-S3 is 3.3 V; **no level shifter needed**.

### 9.2. Mounting

| Parameter | Value | Why |
|-----------|:-----:|-----|
| Camera height above floor | **8-12 cm** | Sees both pillars and lines |
| Downtilt | **10-15°** (12° ideal) | Far pillars + nearby lines |
| Orientation | Lens forward | Direction of travel |
| Fixation | Rigid! 3M + zip tie | Vibration → blob jitter → false detections |

What goes wrong with bad mounting:
- Too horizontal → can't see orange/blue lines on the floor
- Too steep → can't see far pillars (>60 cm)
- Vibration → blobs "jump" → PID jitters the steering

### 9.3. Loading the OpenMV firmware

1. Connect OpenMV to a computer over **USB** (not UART).
2. Open OpenMV IDE.
3. Open `src/openmv/openmv_main.py`.
4. ⛔ Don't save to the camera yet — calibrate the thresholds first (Stage 15).
5. Press ▶ to test — you should see live frame buffer output.

✅ **Stage 9 checklist:**
- [ ] OpenMV IDE sees the camera over USB
- [ ] Frame buffer shows live image
- [ ] UART: anything (even noise) reaches ESP32-S3 Serial Monitor
- [ ] Camera is rigid, no rattle when you tap the chassis
- [ ] 8-12 cm height, ~12° downtilt
- [ ] 📸 Camera mount photo for `v-photos/`

---

## Stage 10: VL53L1X ToF sensors

> ⏱️ ~2 hours | 🟡 Medium

### 10.1. Mounting

**Front sensor (I2C0):**
- Front of the chassis, laser pointing **horizontally forward**
- Height: 3-5 cm above floor
- Sees the wall directly ahead — feeds the corner-detection FSM

**Side sensor (I2C1):**
- Right (or left) side of the chassis, laser pointing perpendicular to travel
- Height: 3-5 cm above floor
- Wall-following on Open Challenge straights

### 10.2. Wiring

Each VL53L1X needs an XSHUT pin so the firmware can hold it in reset during the boot-time address-remap dance.

| VL53L1X pin | Front | Side |
|:-----------:|-------|------|
| VIN | 5 V | 5 V |
| GND | Common GND | Common GND |
| SDA | ESP32-S3 GPIO 8 (I2C0) | ESP32-S3 GPIO 11 (I2C1) |
| SCL | ESP32-S3 GPIO 9 (I2C0) | ESP32-S3 GPIO 12 (I2C1) |
| **XSHUT** | ESP32-S3 **GPIO 15** | ESP32-S3 **GPIO 16** |
| GPIO1 (interrupt) | floating | floating |

The third XSHUT pin (`GPIO 47`) is reserved for an optional 3rd VL53L1X on I2C0.

### 10.3. Library

Arduino IDE → Library Manager → search `VL53L1X` → install **VL53L1X by Pololu**.

### 10.4. Test

Set `WRO_ACTIVE_TARGET = 9` (`WRO_TARGET_TEST_VL53L1X`), flash, open Serial Monitor. Both sensors should report distances in mm. Wave a hand 20 cm away → ~200 mm.

✅ **Stage 10 checklist:**
- [ ] Front and side VL53L1X visible **after the boot-time remap** at 0x30 (front) and 0x31 (side)
- [ ] Distances change smoothly when you wave a hand near each sensor
- [ ] XSHUT pins are wired (not floating)

---

## Stage 11: E-Stop button

> ⏱️ ~30 minutes | 🟢 Easy

### 11.1. Wiring

Simple: a push-button between **GPIO 21** and **GND**:
```
3.3V ──[ESP32-S3 internal pull-up]── GPIO 21 ──[BUTTON]── GND
```
The firmware uses `INPUT_PULLUP`:
- Released → GPIO 21 = HIGH
- Pressed → GPIO 21 = LOW → E-Stop active

> Add a 10 kΩ pull-up to 3.3 V if you want belt-and-braces, but the internal pull-up is normally enough.

### 11.2. How E-Stop behaves in the firmware

| Action | Robot reaction |
|--------|----------------|
| Press in `RS_INIT` | LED arms / fast blink |
| Release in `RS_INIT` | 🏁 **Race starts** — transition to `RS_RUN_*` |
| Press during the race | ⛔ Motor 0, steering centered, `RS_SAFE_STOP` |
| Release during the race | ▶️ Resume to the previous running state |

500 ms boot grace prevents a held button at power-up from arming the robot.

### 11.3. Mode selection (compile-time only — Rule 9.9)

For v13: edit `src/esp32/wro_config_v13.h`:
```cpp
#define OBSTACLE_MODE  0   // 0 = Open Challenge, 1 = Obstacle Challenge
```

✅ **Stage 11 checklist:**
- [ ] Button press → Serial logs the E-Stop transition
- [ ] Release → race starts / resumes
- [ ] Button is reachable from outside the robot
- [ ] Mode is set via `#define`, no physical switches

---

## Stage 12: Status LED

> ⏱️ ~15 minutes | 🟢 Easy

### 12.1. Wiring

```
ESP32-S3 GPIO 2 ──[220 Ω]──[LED anode (+)]──[LED cathode (−)]── GND
```

Use a bright LED (blue or white) — needs to be visible from 2+ meters.

### 12.2. Signal map

| LED behavior | Robot state | Meaning |
|:------------:|-------------|---------|
| Slow blink (~1 Hz) | `RS_INIT` / `RS_WAIT_START` | Press + release E-Stop to start |
| Solid on | `RS_RUN_OPEN` / `RS_RUN_OBS` | Race in progress |
| Fast blink (~4 Hz) | `RS_FINISH` | 🏁 Done after 3 laps |
| Off | `RS_SAFE_STOP` | E-Stop held / fault |

✅ **Stage 12 checklist:**
- [ ] LED on at GPIO 2 = HIGH
- [ ] LED off at GPIO 2 = LOW
- [ ] Visible from 2+ m

---

## Stage 13: Final wire dressing

> ⏱️ 2-3 hours | 🟡 Medium
>
> ⚠️ Sloppy wiring causes 80% of competition failures. Spend the time now.

### 13.1. Routing rules

| Rule | Why |
|------|-----|
| Power and signal wires NOT parallel | EMI from 7.4 V/8 A line couples into I2C |
| Power (LiPo → BTS → motor) on **one** side of the chassis | Isolate noise |
| Signal (I2C, UART, PWM) on the **other** side | Clean signal |
| Cable tie every 5-7 cm | Vibration won't yank connectors |
| Slack on the steering servo wires | Servo turns — wires must not pull |

### 13.2. Vibration-proofing

1. Every Dupont connector — drop of hot glue at the joint.
2. Tug-test every soldered wire.
3. Breadboard fixed with both 3M tape and a cable tie through the mounting holes.

### 13.3. Final continuity check

| # | Test | Expected | If wrong |
|:-:|------|:--------:|----------|
| 1 | Short: 7.4 V+ ↔ GND | open | ⛔ Find and remove |
| 2 | Short: 5 V+ ↔ GND | open | ⛔ |
| 3 | Short: 3.3 V+ ↔ GND | open | ⛔ |
| 4 | All component GNDs | 0 Ω among them | No common GND — fix |
| 5 | SDA ↔ SCL on each bus | not shorted | Wires crossed |

✅ **Stage 13 checklist:**
- [ ] Power and signal wires routed on opposite sides
- [ ] All wires zip-tied
- [ ] No dangling conductors
- [ ] No shorts
- [ ] Dupont connectors hot-glued
- [ ] 📸 Final harness photo

---

## Stage 14: Firmware flashing

> ⏱️ ~2 hours | 🟡 Medium

### 14.1. Arduino IDE setup

1. **ESP32 board package:**
   - File → Preferences → Additional Board Manager URLs:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board Manager → install `esp32` by Espressif (v3.x).
2. **Libraries:**
   - `ESP32Servo`
   - `Adafruit ICM20948`
   - `Adafruit Unified Sensor`
   - `VL53L1X` by Pololu
   - (AS5600 driver is inline in `as5600_dual_i2c.h` — no library install needed)
3. **Board settings:**

| Setting | Value |
|---------|-------|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Enabled** |
| Upload Speed | 921600 |
| Flash Size | 8MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | Default |
| Port | (your ESP32-S3 port) |

### 14.2. Flashing the ESP32-S3

1. Open `src/esp32/` as a sketch in Arduino IDE.
2. Verify `src/esp32/wro_hw_config_v13.h`:
   - `TICKS_PER_CM` = your measured value (Stage 1)
   - `SERVO_*_US` = your measured values (Stage 4 / sketch)
3. Verify `src/esp32/wro_config_v13.h`:
   - `OBSTACLE_MODE` = 0 (Open) or 1 (Obstacle)
   - `HAS_SIDE_TOF` = 1 if side VL53L1X is wired
4. In `src/esp32/wro_build_target.h`, ensure `WRO_ACTIVE_TARGET = WRO_TARGET_V13_MAIN` (= 11).
5. **Compile** (Ctrl+R) — ensure no errors.
6. **Upload** (Ctrl+U).
7. Open Serial Monitor at 115200. Expected boot output:
```
============================================================
 WRO FE 2026 -- Team Faith -- v13.0 main firmware
 Mode: OPEN CHALLENGE
 WiFi: OFF, BT: OFF (Rule 11.10)
============================================================
AS5600 dual I2C: OK
VL53L1X FRONT: OK at 0x30
VL53L1X SIDE:  OK at 0x31
ICM-20948 IMU: OK
Calibrating gyro Z bias............
Gyro Z bias = 0.00234 rad/s
System ready. Press E-Stop to start.
```

### 14.3. Flashing the OpenMV

1. Connect OpenMV via USB.
2. Open OpenMV IDE.
3. Open `src/openmv/openmv_main.py`.
4. **Calibrate thresholds first** (Stage 15.1).
5. Tools → Save Script to OpenMV Cam → save as `main.py`. The camera will run autonomously on next power cycle.

### 14.4. Integration smoke test (mandatory before Stage 15)

Goal: confirm every subsystem works **together** before fine calibration.

1. Power on, wait for `System ready.` in Serial Monitor.
2. Start logic: press + release the E-Stop → state transitions correctly INIT → WAIT_START → RUN.
3. Emergency stop: press during a (wheels-up) run → motor 0, steering centered.
4. Camera: with OpenMV connected, the firmware logs valid camera frames; if you unplug the camera, after `CAM_SILENT_DEGRADE_MS` you should see a warning.
5. Lap counting (dry): turn the chassis by hand through 360°; the gyro lap counter should tick.

Pass: every step works without an unexpected reboot or sensor dropout.
Fail: revisit Stages 6-9 before continuing.

✅ **Stage 14 checklist:**
- [ ] ESP32-S3 flashes without errors
- [ ] Serial Monitor reports "System ready."
- [ ] Gyro calibration `gyroZbias < 0.05`
- [ ] E-Stop press/release reflected in Serial
- [ ] OpenMV IDE sees the camera
- [ ] Smoke test passes

---

## Stage 15: Calibration and first run

> ⏱️ 4-6 hours | 🔴 The most important stage

### 15.1. Color thresholds (OpenMV)

> ⛔ Wrong thresholds → robot doesn't see pillars → crashes → DQ. Calibrate under the **same lighting** as the competition.

For each color:
1. Connect OpenMV via USB and open `openmv_main.py`.
2. Place the robot near the target object.
3. **Tools → Machine Vision → Threshold Editor**.
4. Aim the camera at the target.
5. Drag the L / A / B sliders until only the target shows up in the mask.
6. Record (L_min, L_max, A_min, A_max, B_min, B_max) and paste into the matching `THRESHOLD_*` constant in the script.

| # | Color | Variable | Object |
|:-:|:-----:|----------|--------|
| 1 | 🟠 | `THRESHOLD_ORANGE` | Orange line on the mat |
| 2 | 🔵 | `THRESHOLD_BLUE` | Blue line on the mat |
| 3 | 🔴 | `THRESHOLD_RED` | Red pillar |
| 4 | 🟢 | `THRESHOLD_GREEN` | Green pillar |
| 5 | 🩷 | `THRESHOLD_MAGENTA` | Magenta parking block |
| 6 | ⬛ | `THRESHOLD_WALL` | Black track wall |

### 15.2. Focal-length constant

1. Place a red pillar exactly **20 cm** from the camera.
2. Read raw `blob.w()` in the IDE.
3. `FOCAL_CONST = 20 × blob.w()`.
4. Update the constant in `openmv_main.py`.

### 15.3. Odometry calibration

Test conditions:
- Surface: flat mat / tile, no slope
- Motion: straight line, no steering input
- Distance: 100 cm with start/finish marks

1. Mark the start position of one wheel.
2. Run the robot 100 cm three times.
3. Each run, record `Δ totalDistLeft` from Serial Monitor.
4. `avg = (run1 + run2 + run3) / 3`
5. `TICKS_PER_CM = avg / 100`
6. Update `TICKS_PER_CM` in `wro_hw_config_v13.h`, reflash, repeat once.

Pass: ≤ 2 cm error on a 100 cm run.
Fail: >2 cm → repeat steps 2-6.

### 15.4. First run — on the floor (NOT on the track)

1. Smooth floor (not the track yet).
2. KCD3 ON.
3. LED slow blink → `RS_INIT` ✅
4. Press E-Stop → fast blink → release → solid on = race ✅
5. Press E-Stop → robot stops ✅
6. Release → robot resumes ✅

### 15.5. First lap — Open Challenge

1. `OBSTACLE_MODE 0` → reflash.
2. Place on track (no pillars).
3. Run.
4. Robot should complete **3 laps and stop**.
5. If not, read Serial telemetry to debug.

### 15.6. Lap — Obstacle Challenge

1. `OBSTACLE_MODE 1` → reflash.
2. Place red and green pillars on the track.
3. Run.
4. Red → keep right; green → keep left.
5. 3 laps → parking between magenta blocks.

### 15.7. Live PID tuning

During a run you can change PID gains over USB Serial in real time:

| Command | Example | Effect |
|:-------:|---------|--------|
| `P0.65` | Kp = 0.65 | Sharper steering correction |
| `D0.25` | Kd = 0.25 | More damping (less oscillation) |
| `I0.003` | Ki = 0.003 | More integral (kills steady drift) |
| `G1.50` | Gyro Kp = 1.50 | Stronger heading correction |
| `S+` / `S-` | — | Bump max speed ±5 PWM |
| `?` | — | Full state dump |
| `!` | — | Software E-Stop |

Defaults from `wro_config_v13.h`: `PILLAR_KP=0.45`, `PILLAR_KI=0.001`, `PILLAR_KD=0.30`, `HEADING_KP=12.0`. Tune from there.

✅ **Final checklist:**
- [ ] All 6 colors calibrated in OpenMV
- [ ] FOCAL_CONST recorded: ___
- [ ] TICKS_PER_CM recorded: ___
- [ ] E-Stop works (stop + resume)
- [ ] Open Challenge: 3 laps + stop
- [ ] Obstacle Challenge: pillars + parking
- [ ] LED shows correct states
- [ ] 📸 Successful-run video captured for `video/`

---

## Common problems and fixes

### 🔌 I2C issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Robot freezes after 5-10 s | Motor EMI on I2C | 0.1 µF caps on motor (3 of them) |
| Encoders drop out | Long wires + 400 kHz | Shorter wires, or `Wire.setClock(100000)` |
| Scanner finds nothing | Missing pull-ups | Add 4.7 kΩ on SDA + SCL of each bus |
| IMU bias > 0.1 | Vibration during calibration | Robot still + foam isolation |
| All sensors disappear at once | No common GND | Verify 0 Ω between every GND |

### 🎥 Camera doesn't see colors

| Symptom | Cause | Fix |
|---------|-------|-----|
| All thresholds fail | Auto exposure / white balance still on | `auto_gain=False, auto_whitebal=False` |
| Color "moved" under different lighting | Calibrated under different lighting | Recalibrate on site with Threshold Editor |
| Confuses red and orange | LAB ranges overlap | Tighten ranges in Threshold Editor |
| No UART data | TX/RX not crossed | Swap RX↔TX |

### 🏎️ Robot drives crooked

| Symptom | Cause | Fix |
|---------|-------|-----|
| Drifts left/right | SERVO_CENTER_US off | Re-run `sketches/servo_calibrate` |
| Twitchy steering | Noisy PID derivative | Lower `PILLAR_DERIV_EMA_A` (0.2-0.3) |
| Misses corners | `TURN_TARGET_DEG` too low | Raise to 88-92° |
| Crosses start line late | Lap cooldown too long | Lower `LAP_COOLDOWN_MS` |
| Steering oscillates | Kp too high | Drop with `P0.40` over Serial |

### 🔋 Power

| Symptom | Cause | Fix |
|---------|-------|-----|
| ESP32-S3 reboots when motor starts | 5 V sag | 470 µF on the 5 V rail |
| Servo jitters | LM2596 can't deliver 2 A+ | Verify LM2596 under load |
| Battery sags | Below 6.4 V | LiPo tester with alarm |

---

## 🏆 Competition tips

1. **Print** this guide and bring it to WRO.
2. **Spares:** Dupont jumpers, an extra ESP32-S3, zip ties, 3M tape, insulating tape.
3. **Write down** working color thresholds on paper as a fallback.
4. **Charge** the LiPo to 8.4 V before each round.
5. **Warm up** the robot with one practice lap so the gyro stabilizes.

---

> *"Engineering is the closest thing to magic that exists in the world."*
>
> **— Team Faith**
