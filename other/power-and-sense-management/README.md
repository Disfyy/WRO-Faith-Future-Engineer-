# Power and Sense Management

How Team Faith powers the robot and how it perceives its environment.
This document covers the battery and power-distribution strategy plus the
sensor stack: each sensor's role, why it was chosen, and how its data
flows into the firmware.

---

## 1. Power architecture

| Stage | Component | Output | Feeds |
|---|---|---|---|
| Source | 2S/3S LiPo (7.4 V / 11.1 V) | Raw battery rail | Motor driver, buck input |
| Buck #1 | 5 V step-down (≥ 3 A) | 5.0 V logic | ESP32-S3, OpenMV camera |
| Buck #2 | 5 V step-down (≥ 5 A) | 5.0 V servo | JX PDI-6221MG steering servo |
| 3.3 V | On-board ESP32-S3 LDO | 3.3 V | I2C devices (IMU, AS5600 ×2, VL53L1X ×2) |
| Motor | BTS7960 H-bridge | PWM-modulated battery rail | RS-540 brushed DC motor |

We deliberately split logic and servo onto **separate 5 V buck converters**.
The JX PDI-6221MG can pull 1.5+ A briefly when stalled; sharing one buck
with the ESP32-S3 caused brown-outs during early v11 testing.

The motor driver is fed directly from the battery rail (not the buck), so
motor current never crosses logic ground.

### Common-ground discipline
- ESP32-S3, OpenMV, BTS7960, and both bucks share a single ground star at
  the BTS7960 GND pad — this matters because the OpenMV's UART signal
  references the ESP32 GND through this path.
- I2C pull-ups (4.7 kΩ) are referenced to the 3.3 V rail.

## 2. Sensor stack

| Sensor | Bus / pin | Address | Role |
|---|---|---|---|
| **OpenMV H7 Plus** camera | UART2 (RX 17, TX 18) @ 115 200 | — | Colour blob tracking: orange/blue lines, red/green pillars, magenta parking blocks |
| **ICM-20948** 9-DoF IMU | Wire (I2C0, GPIO 8/9) | 0x68 | Yaw integrator, lap counting, corner exit |
| **AS5600** magnetic encoder ×2 | Wire (left) / Wire1 (right) | 0x36 each | Wheel odometry @ 12-bit (4096 ticks/rev) |
| **VL53L1X** ToF distance ×2 | Wire (front) / Wire1 (side) | 0x30 / 0x29 | Wall + corner detection (range up to ~3 m) |

### 2.1 The address-conflict problem (and our solution)

Both AS5600s ship at I2C address `0x36` and both VL53L1Xs ship at `0x29`.
The original v11 design used a TCA9548A 8-channel I2C mux to give each
device a unique virtual bus — but the mux failed during practice runs.

Our v13 solution uses **two strategies in parallel** instead of the mux:

- **Two native I2C peripherals on the ESP32-S3.** The S3 exposes both
  `Wire` (I2C0) and `Wire1` (I2C1) as independent hardware peripherals.
  We put one AS5600 on each bus — collision avoided by hardware separation.
  Driver: [`src/esp32/as5600_dual_i2c.h`](../../src/esp32/as5600_dual_i2c.h).
- **Runtime XSHUT address remap for the VL53L1X pair.** At boot, both
  VL53L1Xs are held in reset via XSHUT (GPIO 15 front, GPIO 16 side). We
  release the front sensor first, talk to it at the default `0x29`, and
  reprogram it to `0x30`. Then we release the side sensor, which comes up
  at `0x29` on a different bus where there's now no conflict. Driver:
  [`src/esp32/vl53l1x_dual.h`](../../src/esp32/vl53l1x_dual.h).

This is documented end-to-end in
[`docs/strategy/WRO_Migration_v12_to_v13.md`](../../docs/strategy/WRO_Migration_v12_to_v13.md).

### 2.2 Camera (OpenMV H7 Plus)

The OpenMV runs an independent MicroPython program
([`src/openmv/openmv_main.py`](../../src/openmv/openmv_main.py)) that:

1. Captures QQVGA (160×120) frames at ~50 Hz.
2. Finds the largest blob in each of 5 LAB-colour ROIs (red pillar, green
   pillar, orange line, blue line, magenta park-block).
3. Estimates pillar/block distance from blob height using a calibrated
   pinhole model (`distance_cm = FOCAL_PIX * real_height_cm / blob_h_px`).
4. Packs the result into a compact ASCII v3 frame and sends it on UART
   at 115 200 baud with an XOR checksum:

   ```
   RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
   ```

   Spec: [`docs/guides/WRO_OpenMV_UART_Protocol.md`](../../docs/guides/WRO_OpenMV_UART_Protocol.md).

The ESP32-S3 reads frames in `wro_camera.{h,cpp}` and rejects any frame with
a bad checksum or an implausible distance jump.

### 2.3 IMU (ICM-20948)

We use the gyroscope's Z-axis as the primary heading source, integrated
each loop iteration. The accelerometer + magnetometer are available but
not fused in v13 (the snap-to-90°-after-corner heuristic absorbs the
bounded drift over a 3-lap run).

Calibration: the static gyro offset is measured at boot during a 500 ms
stillness window before the start signal. See
[`src/esp32/wro_imu.cpp`](../../src/esp32/wro_imu.cpp).

### 2.4 Encoders (AS5600 ×2)

The AS5600 reports a 12-bit absolute angle (0–4095) of a diametrally-magnetised
disc magnet glued to each rear axle. The driver tracks signed deltas across
the 0/4095 wrap-around and accumulates them into a `int32_t` tick counter
per wheel.

- **Resolution:** ~277 ticks/cm (D = 47 mm wheel).
- **Use:** finish-zone odometry (post-3-lap distance budget) and slip
  monitoring during the run. *Not* used as a turn-exit condition.

### 2.5 ToF distance (VL53L1X ×2)

Each VL53L1X is configured for **Medium** ranging mode — ~1.3 m at full
ambient, 3 m+ in the controlled WRO arena lighting. Internally we sample
at 50 ms timing budgets, polled from the main loop.

- **Front (Wire @ 0x30):** corner trigger and pre-emptive braking.
- **Side (Wire1 @ 0x29):** wall-follow assist for narrow Open Challenge
  corridors when the line camera is partially occluded.

The driver applies a single-frame jump-defence filter (any reading more
than 800 mm different from the previous valid reading is discarded once).

## 3. Safety and resilience

- **Hardware E-Stop** on GPIO 21 (INPUT_PULLUP). Press+release before the
  start = ARM/START. Held during a run = `SAFE_STOP` (motor PWM driven to
  zero in under 50 ms). Released = `RESUME` with PIDs reset. Implementation
  [`src/esp32/wro_estop.cpp`](../../src/esp32/wro_estop.cpp).
- **500 ms boot grace** prevents the E-Stop from firing during the LiPo
  power-up transient.
- **All sensor reads guarded.** A single failed read does not crash the
  loop; it is logged and the previous valid reading is reused for one
  iteration before falling through to a SAFE_STOP if the dropout persists.

## See also

- Full v13 wiring map: [`docs/guides/WRO_Wiring_Map_v13.md`](../../docs/guides/WRO_Wiring_Map_v13.md)
- I2C / power schemas: [`schemes/`](../../schemes/) (Mermaid + rendered PNG/SVG)
- Migration story: [`docs/strategy/WRO_Migration_v12_to_v13.md`](../../docs/strategy/WRO_Migration_v12_to_v13.md)
- Risk register (incl. battery / power risks):
  [`docs/strategy/WRO_Risk_Register.md`](../../docs/strategy/WRO_Risk_Register.md)
