# AS5600 Encoder Bracket — Team Faith

Bracket + magnet-adapter set that fixes the shaking-encoder problem on the
WRO 2026 car. Three fabrication paths in one package — pick whichever your
team has tools for.

---

## Files in this folder

| File | What it is | Tool needed |
|------|-----------|-------------|
| `AS5600_bracket.scad` | Parametric 3D bracket source | OpenSCAD (free) |
| `magnet_adapter.scad` | Small centering disc for the magnet | OpenSCAD |
| `drawing.md` | Dimensioned drawing for hand-cut aluminium | Drill + vise |
| `README.md` | This file — assembly + verification |  |

---

## Why the encoder data was wrong

The AS5600 is a magnetic encoder. It reports a wrong angle when **any of these**
go out of spec:

1. **Air gap** between chip and magnet is not 0.5–3.0 mm
2. **Magnet polarity** is wrong (axial instead of diametric)
3. **Magnet off-centre** by >0.25 mm from the rotation axis
4. **Bracket flexes** under motor torque → gap changes dynamically while driving
5. **I²C wires** running parallel to motor wires picking up BTS7960 switching noise

Items 3, 4 and 5 are what almost certainly caused your "data is wrong" symptom.
The new bracket + adapter design fixes 3 and 4. You fix 5 by routing.

---

## Step 1 — Diagnose first (before building anything)

Run this BEFORE you fabricate. It tells you whether you need just the bracket,
or also a new magnet.

### 1a. Magnet polarity check

Hold a steel paperclip near the magnet:
- **Attracts to the SIDE** (edge) of the magnet → ✅ diametric — keep it
- **Attracts to the FACE** (flat top) → ❌ axial — replace with diametric

Diametric magnets are in your v12 shopping list — order 6×2.5 mm cylindrical
diametric type if you have any doubt.

### 1b. Magnet centering check (with current setup)

1. Flash build target 8 (`test_encoders.cpp`)
2. Open serial monitor at 115200 baud
3. Spin one wheel **very slowly** by hand — one full revolution in 5 seconds
4. Watch the raw angle output:
   - **Monotonic 0 → 4095 sweep** ✅ → magnet centered well enough
   - **Wobbles back and forth** (e.g. 100 → 150 → 130 → 200 → 180 → 250…) ❌
     → magnet off-centre → install the magnet adapter (Step 3)

### 1c. Air gap check

Use a feeler gauge or measure with calipers:
- Target: **1.5 mm** between AS5600 chip face and magnet face
- Acceptable: 0.5 – 3.0 mm
- Out of spec → bracket positioning is wrong → install new bracket (Step 4)

### 1d. I²C noise check

If 1a, 1b, 1c all pass but the data still glitches **only when motor runs**:
- Re-route SDA/SCL twisted pair away from the BTS7960 power wires
- Add ferrite beads on the I²C lines near the ESP32-S3
- Check 4.7 kΩ pull-ups are present on both Wire and Wire1 buses

---

## Step 2 — Measure your car (CRITICAL before printing)

Open `AS5600_bracket.scad` in OpenSCAD. The top of the file has 3 variables
you MUST verify match your car:

```scad
ARM_REACH = 28;   // distance: chassis side face → wheel INNER face
ARM_RISE  = 16;   // vertical: chassis-side anchor → axle centerline
AIR_GAP   = 1.5;  // sensor chip face → magnet face (target)
```

**How to measure:**

- **ARM_REACH** — with the car on its wheels, measure horizontally from the
  edge of the chassis plate to the inside face of the wheel hub.
  Use a small ruler or calipers. Typical HSP 94182 1/16: **24–30 mm**.

- **ARM_RISE** — measure vertically from the top of the chassis plate (where
  the bracket will bolt) up to the axle centerline (the imaginary line that
  goes through the wheel rotation centre). Typical HSP 94182: **14–18 mm**.

- **AIR_GAP** — keep at 1.5 mm. The slot on the sensor mount lets you tune
  this ±2 mm physically without re-printing.

Also verify these match your AS5600 board:

```scad
PCB_W            = 22;   // long axis of the purple breakout
PCB_H            = 18;   // short axis
PCB_HOLE_SPACING = 17;   // distance between the 2 M2 mounting holes
```

If your breakout is different, edit the numbers and re-export STL.

---

## Step 3 — Print the magnet adapter (do this first)

The adapter ensures the magnet stays centered on the rotation axis. Fixing
this often improves data quality more than fixing the bracket.

1. Open `magnet_adapter.scad` in OpenSCAD
2. Verify `MAGNET_D = 6.0` matches your magnet (measure with calipers)
3. Press **F6** to render, then **File → Export → Export as STL**
4. Slice with these settings:
   - Layer height: **0.12 mm** (precision pocket)
   - Infill: 100%
   - Walls: 4
   - No supports
5. Print **2 adapters** (one per rear wheel)
6. Drop a magnet into the pocket — should be snug, not loose, not forced.
   If too tight → reprint with `MAGNET_FIT = 0.20`
   If too loose → reprint with `MAGNET_FIT = 0.10`
7. **Verify diametric** (see Step 1a above)
8. Glue adapter to wheel hub centre with thin cyanoacrylate (super glue).
   - Clean wheel surface with isopropyl alcohol first
   - Apply a thin layer of glue to the flat bottom of the adapter
   - Press onto wheel centre, hold for 30 sec
   - Cure 1 hour before any driving

---

## Stability features (v1.1) — what makes this bracket actually rigid

A cantilever — even with one gusset — flexes under vibration. To stop the
shake we converted the cantilever into a **triangulated truss**. Four things
work together:

1. **Diagonal strut** (`STRUT_ENABLE=true` by default). Runs from the top of
   the sensor mount down to the bottom of the riser, in the same plane as
   the arm. The cantilever bending load on the sensor end becomes axial
   tension/compression in the strut — that's ~50× more efficient than
   bending a flat plate. **Single biggest rigidity improvement.**
2. **Deeper lateral arm** (`ARM_H=10`, was `BRACKET_T=4`). Bending stiffness
   scales with depth³ — going from 4 mm to 10 mm gives ~16× more rigidity
   for the same arm length.
3. **Double gussets** (`DOUBLE_GUSSET=true` by default). Triangular fillets
   on BOTH sides of the riser-to-arm corner — above the arm and below it.
   Together they make the corner rigid in torsion as well as bending.
4. **Back buttresses** (`BACK_BUTTRESS=true` by default). Small wedges that
   anchor the top and bottom edges of the sensor wall back to the lateral
   arm. The wall is taller than the arm by ~7 mm on each side; without
   buttresses those extensions would flap. With them, the wall is rigid.

If you find the bracket prints too bulky or interferes with something on
your specific car, you can disable individual features by editing the top
of `AS5600_bracket.scad` and setting `STRUT_ENABLE=false` etc. But the
default settings are what fixes the shake.

---

## Step 4 — Print or fabricate the bracket

### Option A — 3D print (recommended)

A pre-rendered `AS5600_bracket.stl` is already in this folder, sized for
the default measurements. If your car matches the defaults you can slice
this directly. Otherwise:

1. Open `AS5600_bracket.scad` in OpenSCAD
2. Verify `ARM_REACH`, `ARM_RISE`, `PCB_HOLE_SPACING` match your measurements
3. Set `SHOW_GHOSTS = false` at the bottom of the file (hides reference PCB/magnet)
4. **F6** to render, **File → Export → Export as STL**
5. In your slicer, **rotate the bracket so the anchor flange lies flat on the bed.**
   This makes the cantilever bending load perpendicular to layers = strongest.
6. Slice with:
   - Material: **PETG** (not PLA — PLA creeps under sustained load)
   - Layer height: 0.2 mm
   - Infill: **60%**
   - Walls: **4 perimeters**
   - No supports needed
7. Print **2 brackets** (one per rear wheel — mirror not needed, design is symmetric).
   Print time ~45 min per bracket.

### Option B — Hand-cut from aluminium

Follow `drawing.md` for the flat blank, fold lines, and gusset.
Use 2 mm aluminium sheet for best stiffness. 3 mm acrylic is OK as fallback
but will crack at the folds — score-and-snap with heat-bending instead.

### Option C — Laser-cut acrylic / plywood sandwich

Use the bracket SCAD to extract a top-view silhouette (project to 2D in
OpenSCAD), then cut 3 layers of 3 mm acrylic and stack with M2 standoffs.
This option is *less rigid* than 3D-print or aluminium — only use if it's
all you have access to.

---

## Step 5 — Install

### 5a. Mount bracket to chassis
1. Position the bracket against the side of the chassis at the rear-axle area
2. The 2× M3 holes in the anchor flange align with existing chassis holes
   (or drill new ones — chassis is plastic, easy)
3. M3 × 8 mm socket-head screws, thread into chassis tabs or use M3 nuts on
   the back
4. **Tighten firmly** — loose anchor screws are the #1 cause of bracket shake
5. Repeat on the other side

### 5b. Mount AS5600 PCB to bracket
1. Drop the purple AS5600 board into the sensor mount with the AS5600 chip
   facing OUT (toward the wheel — i.e. toward the magnet)
2. M2 × 8 mm screws go from the chassis side, through the slotted wall,
   through the PCB, with an M2 nut on the wheel side
3. **Leave the screws loose** for now — vertical alignment comes next

### 5c. Align chip vertically with axle centerline
The slots in the bracket wall let you slide the PCB up/down ±2 mm. The
AS5600 chip MUST sit directly over the wheel rotation axis.

1. With the wheel on the car and bracket installed, look from above
2. Slide PCB until the AS5600 chip (the small black square on the PCB) is
   centered over the wheel hub centerline (where the magnet is)
3. Tighten the 2× M2 nuts firmly

### 5d. Verify / fine-tune air gap
The air gap is set at PRINT time by the `AIR_GAP` parameter in the SCAD
(default 1.5 mm). After mounting:

1. Use a feeler gauge or calipers to measure actual air gap
2. If gap is **too small** (<1.0 mm): add an M2 washer between PCB and wall
   (each washer adds ~0.4 mm of gap)
3. If gap is **too large** (>2.5 mm): re-print the bracket with smaller
   `AIR_GAP` value (e.g. 1.0 mm)
4. Target final gap: **1.4–1.8 mm**

### 5e. Wire it up
- Wire and Wire1 buses per `wro_hw_config_v12.h`:
  - Left encoder: I²C bus 0 (GPIO 8/9) → 0x36
  - Right encoder: I²C bus 1 (GPIO 3/4) → 0x36
- Use **twisted pair** for SDA + GND, separate twisted pair for SCL + 3V3
- Route AS5600 cables **away from motor power wires** — at least 30 mm
  separation, or cross at 90° angles only
- Confirm 4.7 kΩ pull-ups present on both buses

---

## Step 6 — Verify (in this order — do not skip)

### 6a. I²C scan — confirm both AS5600s are seen
Flash build target 2 (`scanerI2C.cpp`). Expected output:

```
Wire (bus 0): 0x36 AS5600 left, 0x68 ICM-20948, 0x30 VL53L1X front
Wire1 (bus 1): 0x36 AS5600 right, 0x29 VL53L1X side
```

If 0x36 missing on either bus → wiring fault or board not powered. Fix
before continuing.

### 6b. Hand-spin smoothness — confirm magnet centering
Flash target 8 (`test_encoders.cpp`). Open serial at 115200. Spin each wheel
slowly by hand, one full revolution.

- Raw angle sweeps 0 → 4095 monotonically (no back-and-forth) ✅
- Wraps cleanly at 4095 → 0 ✅
- No "stuck" zones where the angle stops updating ✅

Failure modes:
- Wobble (non-monotonic) → magnet not centered → re-glue adapter
- Stuck at 0 or 4095 → air gap too big (magnet too weak) → tighten gap
- Stuck random value → air gap too small / magnet polarity wrong → re-check Step 1a

### 6c. Magnet quality — read MAGNITUDE register
The AS5600 has a built-in magnitude register that says how strong it sees
the magnetic field. Add this to your test loop:

```cpp
uint16_t magnitude = readAS5600Register(0x1B);  // MAGNITUDE register
Serial.printf("Magnitude: %d (target 1000-2500)\n", magnitude);
```

- 1000–2500 stable ✅
- <500 → gap too big or weak magnet
- >3500 → gap too small, magnet too close (saturation)
- Magnitude fluctuates >300 while wheel spins → BRACKET STILL FLEXING

### 6d. Bench test — wheels off ground at full motor speed
Flash target 10 (`bench_test_v12.cpp`). Lift wheels clear of ground.
Run motor at MOTOR_MAX_SPEED = 150 for 10 seconds. Log raw ticks at 100 Hz.

Expected: smooth monotonic increase, no jumps >50 counts per sample.
If you see jumps → bracket is still flexing under load → check anchor screw
tightness, add another gusset, or switch to aluminium bracket.

### 6e. Floor test — measured straight line
Drive the car **2.0 m** in a straight line on the floor (use a tape measure).
Convert encoder ticks to distance:

```
distance_cm = ticks / 277.4
```

(Where 277.4 comes from `TICKS_PER_CM` in `wro_hw_config_v12.h`)

- Within ±2% (1.96 – 2.04 m equivalent) ✅
- Larger error → wheel circumference miscalibrated (real wheel ≠ 47 mm OD)
  → measure actual wheel diameter and update `TICKS_PER_CM`

### 6f. Lap test — full WRO track
3 laps on actual WRO obstacle track. Watch left vs right encoder distance.
Difference should track only with steering arc (turns add to outer wheel,
subtract from inner). Erratic asymmetry = mechanical problem (slipping
magnet, loose bracket, gear lash if you went Solution 2).

---

## Troubleshooting cheat-sheet

| Symptom | Likely cause | Fix |
|--------|------|-----|
| Bracket physically wobbles when you wiggle it with your fingers | Loose M3 anchor screws OR gusset missing/loose | Tighten M3s; re-bond gusset |
| Angle jumps wildly only when motor runs | Bracket flexes under torque | Add second gusset, or switch to aluminium |
| Angle jumps wildly even when stationary, just from touching the wire | I²C wire flaky | Re-solder header pins on AS5600 board |
| MAGNITUDE register reads <500 | Air gap too large or weak magnet | Reduce gap; check magnet is N52 not N35 |
| MAGNITUDE register reads >3500 | Magnet too close → saturation | Increase gap toward 2 mm |
| Sweep wobbles back-and-forth when spinning wheel slowly | Magnet off-centre | Re-glue with adapter (Step 3) |
| Both encoders read 0xFFFF (4095) always | Magnet polarity wrong (axial) | Replace with diametric magnet |
| One encoder fine, other dead | Wiring fault on one bus | Re-check Wire vs Wire1 connections |
| Sweep is jumpy but only at high speed | Sample rate too low for high RPM | Reduce LOOP_INTERVAL in config |

---

## What this design deliberately does NOT do

- **No fan-mounted vibration damping** — adding rubber under the bracket
  would just give it another resonance mode. Rigid all the way is correct.
- **No spring-loaded sensor** — fixed gap is more reliable than spring-pressed.
- **No magnetic shielding** — the AS5600 only cares about the rotating
  magnet, not stray fields from motor (those are at much higher frequencies
  and aren't strong enough to confuse a 12-bit angle).
- **No side anti-rotation pin** — if your bracket needs a pin to stay
  oriented, your screws are too loose. Tighten the screws.

---

## Open issues / future work

- If after all this the data is *still* glitchy under motor load, the
  fallback is Solution 2 (idler-pinion gear-drive encoder). That's an
  entirely different mechanical design and requires precision bearing
  machining — held in reserve only if Solution 1 fails despite proper
  bracket + adapter + wiring.
- Consider upgrading from 47 mm `TICKS_PER_CM` calibration to the actual
  measured wheel diameter after install. The HSP 94182 chassis model
  in `models/HSP94182_3D/chassis.scad` lists `WHEEL_D = 54` mm. Either
  the firmware or the SCAD is out of date — measure the real wheel and
  fix whichever is wrong.
