# Mobility Management

How Team Faith's robot moves: drivetrain, steering, chassis, and the
control loops that turn sensor data into wheel motion.

---

## 1. Chassis

We built on top of an **HSP 94182** 1/16-scale RC car platform — a 4-wheel
independent-suspension chassis with a brushed motor and Ackermann steering.
This gave us a competition-proven mechanical base so we could focus our
engineering effort on electronics, sensing, and software.

- CAD reference: [`models/HSP94182_3D/`](../../models/HSP94182_3D/)
  ([viewer.html](../../models/HSP94182_3D/viewer.html) for an in-browser preview)
- Wheel diameter: 47 mm
- Track width: ~110 mm
- Wheelbase: ~165 mm

Custom 3D-printed mounts (designed in OpenSCAD, exported as STEP/STL) hold
the ESP32-S3, BTS7960 driver, OpenMV camera, IMU, ToF sensors, and the LiPo
battery. The OpenMV is mounted on an antenna mount looking forward, ~80 mm
above the floor — chosen so 100 mm pillars fit fully inside the QQVGA frame
between roughly 30 cm and 200 cm.

## 2. Drivetrain

| Component | Spec | Why |
|---|---|---|
| Motor | RS-540 brushed DC (HSP stock) | Ample torque, simple H-bridge control |
| ESC / driver | **BTS7960 43A H-Bridge** | Bidirectional, handles motor current with safety margin |
| Reduction | Stock 1/16 RC pinion + spur | Already gives suitable top speed for the 3 m WRO field |
| Wheels | 47 mm diameter rubber | Good grip on the WRO mat |

The BTS7960 is driven from the ESP32-S3 with two LEDC PWM channels
(R_PWM = forward, L_PWM = reverse) plus active-HIGH enable lines. Speed is
expressed as a signed integer in `[-255 .. +255]` and mapped to the two
channels in `wro_v13_main.cpp`'s motor mixer.

Rule 9.6 says the robot's drive system must be electrical and student-built
in the sense of integration; we satisfy this by integrating the off-the-shelf
HSP motor with our custom driver electronics and firmware.

## 3. Steering

| Component | Spec | Notes |
|---|---|---|
| Servo | **JX PDI-6221MG digital** | 20 kg·cm metal-gear, ~0.16 s / 60° |
| Linkage | Stock HSP Ackermann tie-rod | Reused from the donor RC car |
| Range | 45° (max-left) → 90° (centre) → 135° (max-right) | Software-limited to avoid mechanical bind |

The servo runs from a dedicated 5 V buck (separate from logic 5 V) so that
high-stall current dips do not brown out the ESP32-S3. Centre, end-stops
and microsecond mapping are tuned per chassis using the procedure in
[`docs/guides/WRO_Servo_Calibration_Guide.md`](../../docs/guides/WRO_Servo_Calibration_Guide.md)
and the standalone Arduino sketch
[`sketches/servo_calibrate/servo_calibrate.ino`](../../sketches/servo_calibrate/servo_calibrate.ino).

## 4. Speed control

Open Challenge runs at **PWM 80** maximum (steady, low slip), Obstacle
Challenge at **PWM 130** maximum (more aggressive, since the stop-and-turn
budget tolerates a bit more wheel-spin). Cornering — in either mode — drops
to **PWM 70** to keep slip out of the IMU yaw signal that drives turn exit.

Speed targets and ramps are defined as named tunables in
[`src/esp32/wro_config_v13.h`](../../src/esp32/wro_config_v13.h) so they
can be retuned without touching the FSM logic.

## 5. Closed-loop steering

We run two PID-style controllers in parallel, gated by the active mode:

- **Heading-hold (Open Challenge):** snaps to the nearest 90° heading after
  every corner exit, then holds against the IMU yaw with `KP = 12 µs/deg`,
  `KD = 2.0`, `KI = 0` (pure PD; integral was found unnecessary over a 3-min
  run and accumulated drift).
- **Pillar PID (Obstacle Challenge):** setpoint shifts ±60 px relative to
  the camera centre when a pillar of the active colour is in view (red →
  keep right, green → keep left). `KP = 0.45`, `KI = 0.001`, `KD = 0.30`
  with EMA-filtered derivative; integral is reset on colour switch.
  A far-pillar pre-position term at 0.4× gain blends in when both pillar
  colours are simultaneously visible.

The output of either PID is added to the servo centre value in
microseconds, clamped to the calibrated end-stops, and written to the
servo. Implementations: [`src/esp32/wro_pid.h`](../../src/esp32/wro_pid.h),
[`src/esp32/wro_v13_main.cpp`](../../src/esp32/wro_v13_main.cpp).

## 6. Cornering FSM

Cornering is owned by **VL53L1X front + IMU yaw delta**, never the camera
(an early v11 design used camera-gated turn entry and failed when the lens
was partially occluded mid-turn). The corner state is in
[`src/esp32/wro_corner.{h,cpp}`](../../src/esp32/wro_corner.cpp):

1. **Trigger:** front ToF < 350 mm for 3 consecutive frames.
2. **Slow-down:** front ToF < 600 mm starts a speed taper.
3. **Commit:** swing servo to maximum lock in the direction indicated by the
   most recent line colour (orange → CW, blue → CCW).
4. **Exit:** IMU yaw integrator passes 80° of delta from corner-entry yaw.
5. **Settle:** snap heading-hold target to the nearest 90° of the new
   absolute heading.

Encoder ticks during the turn are recorded as telemetry only; they are
*never* used as an exit condition (wheel slip during the turn would lie).

## 7. Engineering trade-offs

- **No I2C multiplexer.** The original v11 build used a TCA9548A to share
  one I2C bus across two AS5600s and two VL53L1Xs. The mux failed during
  practice. v13 drops it entirely and instead uses both native I2C
  peripherals on the ESP32-S3 (one AS5600 per bus) plus runtime XSHUT-based
  address remapping for the VL53L1X pair.
- **Brushed motor over brushless.** The stock HSP brushed motor is over-spec
  for 3 m WRO fields and avoids the BLDC commutation electronics we'd
  otherwise need to debug.
- **Single steering servo.** Rear-wheel drive + Ackermann front steering is
  geometrically appropriate for the corridor widths in the rules.

## See also

- Wiring map: [`docs/guides/WRO_Wiring_Map_v13.md`](../../docs/guides/WRO_Wiring_Map_v13.md)
- Assembly + startup: [`docs/guides/WRO_Robot_Assembly_and_Startup_Guide.md`](../../docs/guides/WRO_Robot_Assembly_and_Startup_Guide.md)
- Servo calibration: [`docs/guides/WRO_Servo_Calibration_Guide.md`](../../docs/guides/WRO_Servo_Calibration_Guide.md)
- Schemes: [`schemes/`](../../schemes/)
