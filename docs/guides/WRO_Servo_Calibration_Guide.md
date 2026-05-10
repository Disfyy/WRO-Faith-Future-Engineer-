# WRO Servo Calibration Guide

**Servo:** JX PDI-6221MG
**MCU:** ESP32-S3-DevKitC-1 N8R8 (v13)
**Pin:** GPIO 42
**Firmware:** [`sketches/servo_calibrate/servo_calibrate.ino`](../sketches/servo_calibrate/servo_calibrate.ino) — standalone Arduino IDE sketch already wired to GPIO 42 with v13 calibration constants.

> The legacy `src/esp32/legacy/legacy_test_servo_calibrate.cpp` (build target 7) was the v11 calibration program on GPIO 27 and is kept as historical reference only. For v13 use the standalone sketch above, which has the same workflow with v13 hardware bindings.

---

## Why calibration matters

When you command the servo to 90° (centre) after it was at 135° (right),
you might notice the wheel doesn't land at *exactly* the same spot it was
when it went to 90° from 45° (left). This is called **hysteresis** and it
has three main causes:

| Cause | What happens | Fix |
|-------|-------------|-----|
| **Gear backlash** | Tiny gaps between gear teeth let the output shaft sit at slightly different positions depending on which direction it arrived from. | Cannot be eliminated, but you can compensate in software. |
| **Linkage play** | Loose ball links, worn servo horn splines, or a long steering rod amplify the backlash. | Tighten or replace ball links. Use a metal servo horn. Minimise linkage length. |
| **Coarse PWM mapping** | `servo.write(90)` converts to 1500 µs, but your servo's true mechanical centre might be 1485 µs or 1520 µs. A 15 µs error is ~1.5° of steering angle. | Calibrate in **microseconds**, not degrees. |

---

## Before you begin — mechanical checks

Do these FIRST. No amount of software calibration fixes bad mechanics.

### 1. Tighten everything

- [ ] Servo horn screw is tight (the screw in the centre of the horn).
- [ ] Servo horn splines have no slop — if they do, replace the horn
      or try a different spline tooth position.
- [ ] Ball links on the steering rod click firmly with no wiggle.
- [ ] Servo is screwed down to the chassis with no flex.
- [ ] Steering rod has no visible bend.

### 2. Set the horn at mechanical centre

1. Power the servo **without** the horn attached.
2. Send 1500 µs (the calibration tool starts there automatically).
3. Now press the horn onto the splines so that the wheels point straight.
4. Tighten the horn screw.

This ensures that the servo's electrical midpoint approximately matches
the mechanical straight-ahead position, reducing the offset you need to
trim later.

---

## Calibration procedure (step by step)

### Step 0: Flash the calibration firmware

Open `sketches/servo_calibrate/servo_calibrate.ino` in the Arduino IDE and upload it as a standalone sketch. Open Serial Monitor at **115200 baud** with "Newline" line ending.

(The sketch is independent of `wro_build_target.h`; it doesn't pull in the rest of the firmware.)

### Step 1: Find true centre

1. The servo starts at 1500 µs.
2. Type **`t`** to enter the interactive trim mode.
3. Look at the front wheels from directly above/ahead.
4. Use **`+`** / **`-`** (±5 µs) to nudge until the wheels are
   *perfectly* straight. Use **`>`** / **`<`** (±50 µs) for big jumps.
5. When satisfied, type **`x`** to save.

> **Tip:** Place a ruler or straight edge against the front wheels to
> judge alignment precisely. Even 1° matters at competition speed.

Write down the value. Example: **1485 µs**.

### Step 2: Find maximum right

1. From centre, press **`>`** repeatedly (or type e.g. `s2000`) to steer
   right until either:
   - The steering linkage hits a mechanical stop, OR
   - The wheel angle reaches ~45° from centre (equivalent to the old
     `SERVO_MAX_RIGHT = 135`).
2. **Do not push past the mechanical stop** — you'll strip the servo gears.
3. Type **`R`** to save this as the right limit.

Write down the value. Example: **1940 µs**.

### Step 3: Find maximum left

Same process in the other direction. Type **`L`** to save.

Write down the value. Example: **1040 µs**.

### Step 4: Measure hysteresis

1. Type **`h`** to run the hysteresis test.
2. This moves the servo right→centre and left→centre, 10 times each.
3. Watch the wheel at each "centre" stop. If it consistently lands
   slightly left when approaching from the right (or vice versa), that's
   your backlash gap.
4. Estimate the gap in µs. Typical for JX PDI-6221MG: **5–15 µs**.

### Step 5: Check repeatability

1. Type **`r`** for 20 full cycles (right → centre → left → centre).
2. The wheel should always return to the same centre position.
   If it drifts over cycles, there's a mechanical issue (horn slipping,
   linkage loosening, servo overheating).

### Step 6: Print and record your values

Type **`p`**. You'll see output like:

```
╔══════════════════════════════════════╗
║    SERVO CALIBRATION SUMMARY         ║
╠══════════════════════════════════════╣
║  Centre:     1485 µs
║  Max right:  1940 µs
║  Max left:   1040 µs
╠══════════════════════════════════════╣
║  Range right: +455 µs from centre
║  Range left:  -445 µs from centre
╠══════════════════════════════════════╣
║  Copy these into wro_hw_config_v13.h:
║
║  #define SERVO_CENTER_US   1485
║  #define SERVO_RIGHT_US    1940
║  #define SERVO_LEFT_US     1040
║
║  Equivalent angles (ESP32Servo):
║  Centre ≈ 88°
║  Right  ≈ 129°
║  Left   ≈ 48°
╚══════════════════════════════════════╝
```

---

## Applying calibration to the main firmware

After calibration, update the µs constants in `src/esp32/wro_hw_config_v13.h` (`SERVO_CENTER_US`, `SERVO_LEFT_US`, `SERVO_RIGHT_US`). The v13 firmware already drives the servo with `writeMicroseconds()` via the `writeSteeringUs()` helper in `wro_v13_main.cpp`, so the only change is updating the three constants and reflashing.

### What to change

**Replace the old degree-based constants:**

```cpp
// OLD — imprecise
#define SERVO_CENTER      90
#define SERVO_MAX_RIGHT   135
#define SERVO_MAX_LEFT    45
```

**With microsecond constants from your calibration:**

```cpp
// NEW — calibrated in µs (run sketches/servo_calibrate, or read from bench_test_v13 target 10)
#define SERVO_CENTER_US    1485   // <- your measured value
#define SERVO_RIGHT_US     1940   // <- your measured value
#define SERVO_LEFT_US      1040   // <- your measured value
```

**Update `setSteering()` to work in microseconds:**

```cpp
void setSteering(int us) {
  us = constrain(us, SERVO_LEFT_US, SERVO_RIGHT_US);
  steeringServo.writeMicroseconds(us);
}
```

**Update every place that writes a steering value.**  
Throughout the code, replace angle calculations with µs calculations.
The simplest approach: keep the rest of the code using a **-100 to +100
abstract steering range** and map it to µs in `setSteering()`:

```cpp
// steerCmd: -100 (full left) ... 0 (centre) ... +100 (full right)
void setSteering(int steerCmd) {
  steerCmd = constrain(steerCmd, -100, 100);
  int us;
  if (steerCmd >= 0)
    us = map(steerCmd, 0, 100, SERVO_CENTER_US, SERVO_RIGHT_US);
  else
    us = map(steerCmd, -100, 0, SERVO_LEFT_US, SERVO_CENTER_US);
  steeringServo.writeMicroseconds(us);
}
```

This decouples the PID output (which uses abstract units) from the
physical servo (which uses calibrated microseconds), so recalibrating
only means changing three `#define` values.

---

## Reducing hysteresis in practice

Even after calibration, some backlash remains. These techniques minimise
its effect during a race:

1. **Overshoot-and-settle**: When returning to centre, briefly command
   a few µs *past* centre in the approach direction, then snap to
   centre. This takes up the gear slack. Adds ~20 ms latency.

2. **Always approach from the same side**: If the servo is at 1700 µs
   (right) and you want centre, move to centre−10 µs first, then to
   centre. This ensures the gears always seat the same way.

3. **Increase servo dead-band tolerance in PID**: If your PID
   oscillates within ±10 µs of the setpoint, it's chasing backlash.
   Add a dead zone: don't update the servo if the new command is within
   ±8 µs of the last written value.

4. **Mechanical**: Replace plastic ball links with metal ones. Use a
   metal servo horn instead of the stock plastic one. Shorter steering
   rod = less amplification of backlash.

---

## Troubleshooting

| Symptom | Likely cause | Action |
|---------|-------------|--------|
| Servo jitters at rest | Electrical noise on signal wire, or the servo is fighting a mechanical bind | Add a 100 nF ceramic cap between servo signal and GND, close to the servo connector. Check for linkage binds. |
| Centre drifts over time | Servo horn slipping on splines | Tighten horn screw. Use thread-lock (blue Loctite) on the screw. |
| Loud buzzing at extreme angles | Linkage hitting a hard stop while the servo tries to push further | Reduce `SERVO_RIGHT_US` / `SERVO_LEFT_US` by 20–50 µs to stay inside the mechanical range. |
| Asymmetric range (right > left or vice versa) | Horn installed one spline tooth off centre | Remove horn, re-seat at 1500 µs, re-calibrate. |
| `setSteering()` has no effect | Servo not attached, wrong pin, or timer conflict with motor PWM | Verify `steeringServo.attach(SERVO_PIN, SERVO_LEFT_US, SERVO_RIGHT_US)` is called with `SERVO_PIN = 42` for v13. ESP32Servo and `ledcAttach` can conflict if they use the same timer — servo defaults to timer 0, motor PWM should use other timers. |

---

## Quick reference

| Parameter | Your value | Date measured |
|-----------|-----------|---------------|
| Centre (µs) | _______ | ____/____/____ |
| Max right (µs) | _______ | ____/____/____ |
| Max left (µs) | _______ | ____/____/____ |
| Hysteresis (µs) | _______ | ____/____/____ |
| Horn position | _______ spline tooth | ____/____/____ |
| Linkage rod length (mm) | _______ | ____/____/____ |
