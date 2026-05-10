# Obstacle Management

How Team Faith handles the WRO Future Engineers **Obstacle Challenge**:
the strategy, the perception pipeline, and the FSM that turns red/green
pillars and a magenta parking block into safe, lawful manoeuvres.

---

## 1. The Obstacle Challenge in one paragraph

The arena is a 3 m × 3 m square track of corridors with four 90° corners.
The robot must complete **3 laps** while obeying coloured pillars: pass
**red on the right**, **green on the left**. After the third lap, in
the final corridor, the robot must perform a **parallel-parking manoeuvre**
between two magenta blocks. Direction (CW or CCW) is fixed at start by the
colour of the line under the start position (orange = CW, blue = CCW).
Mode selection is compile-time only (Rule 9.9): no physical switches.

## 2. High-level strategy

We treat Obstacle as **Open Challenge with a steering-target offset**:
the same heading-hold + corner FSM that drives Open is reused, and the
pillar logic only modifies the steering setpoint while a pillar is in
view. This keeps the firmware small and avoids two parallel control
codepaths fighting each other.

```
              ┌─────────────────┐
              │  WAIT_START     │  E-Stop press+release arms the run
              └────────┬────────┘
                       ▼
              ┌─────────────────┐
              │  RUN_OBSTACLE   │  heading-hold + pillar PID + corner FSM
              └────────┬────────┘
                       ▼
              ┌─────────────────┐
              │   TURN_90       │  IMU-driven, front-ToF gated
              └────────┬────────┘
                       ▼
                ... ×3 laps ...
                       ▼
              ┌─────────────────┐
              │   PARKING       │  approach → align → 3-phase reverse → final
              └────────┬────────┘
                       ▼
              ┌─────────────────┐
              │   FINISH        │  brake, stop, hold position
              └─────────────────┘

       SAFE_STOP runs in parallel; entered on E-Stop held or fault.
```

Top FSM: [`src/esp32/wro_race_fsm.cpp`](../../src/esp32/wro_race_fsm.cpp).
Obstacle behaviour: [`src/esp32/wro_behavior_obstacle.cpp`](../../src/esp32/wro_behavior_obstacle.cpp).

## 3. Perception pipeline

The OpenMV camera produces an ASCII v3 frame every ~20 ms. Inside the
ESP32-S3, a single read function in
[`src/esp32/wro_camera.cpp`](../../src/esp32/wro_camera.cpp) parses the frame
into a struct that the obstacle behaviour consumes:

| Field | Meaning |
|---|---|
| `redX` | Red-pillar centroid offset from image centre, in pixels (-80..+80) |
| `redDist` | Estimated red-pillar distance in cm, or 999 if absent |
| `greenX`, `greenDist` | Same, for green pillars |
| `modeFlag` | Bitfield: orange line / blue line / magenta block visibility |
| `extraTag` | X-position of magenta block when bit 2 is set |

Frames with bad checksum are dropped. Frames with implausible jumps
(distance > 80 cm change vs the previous valid frame) are dropped *once*
and then accepted on confirmation.

## 4. Pillar handling — the dynamic-offset PID

The setpoint of the steering PID is **shifted** when an active pillar is in
view, rather than turning around the pillar. Concretely:

- **Red pillar present** → PID setpoint += +60 px → robot drifts left until
  the visual centre of the pillar is offset 60 px to the right. Net effect:
  pass it on the right.
- **Green pillar present** → PID setpoint += -60 px → robot drifts right.
  Net effect: pass it on the left.
- **Both colours present:** the *near* colour controls the active offset.
  The *far* colour adds a small pre-position term (gain 0.4×) so the robot
  begins biasing in the right direction before it commits.
- **Pillar gone:** the offset returns to zero on the next loop and the
  PID integrator is reset to avoid carry-over wind-up from a manoeuvre
  that is now finished.

PID gains: `KP = 0.45`, `KI = 0.001`, `KD = 0.30` with EMA-filtered
derivative (`alpha = 0.30`). Tunables in
[`src/esp32/wro_config_v13.h`](../../src/esp32/wro_config_v13.h),
implementation in
[`src/esp32/wro_pid.h`](../../src/esp32/wro_pid.h).

The dynamic-offset approach is robust under partial occlusion and avoids
the discrete state explosion of a "swerve left / swerve right / pass /
return" sub-FSM.

## 5. Cornering interaction

Pillar logic and corner logic coexist by giving corners **priority**:
when the front VL53L1X reports < 350 mm for 3 frames, the FSM jumps to
`TURN_90` regardless of pillar state. PIDs are reset on corner exit so the
robot does not chase a stale pillar offset into the next corridor.

Corner exit is by **IMU yaw delta** (80°), never by camera. See
[`other/mobility-management/README.md`](../mobility-management/README.md#6-cornering-fsm)
for the full corner FSM.

## 6. Lap counting

Two independent counters provide redundancy:

- **Primary — gyro 360° accumulator.** The IMU yaw integrator wraps every
  full revolution; we increment on each wrap with hysteresis to avoid
  double-counting on small back-rotations.
- **Secondary — line crossings.** Each orange/blue line bit transition (on
  → off) bumps a counter, gated by a 1.5 s cooldown so a single line under
  the camera for several frames counts once.

The race FSM uses `max(gyroLaps, lineLaps)` and only exits `RUN_OBSTACLE`
to `TURN_90` *or* `PARKING` when both agree to within ±1.

## 7. Parallel parking (after lap 3)

Implemented in [`src/esp32/wro_park.cpp`](../../src/esp32/wro_park.cpp).

```
APPROACH  →  ALIGN  →  REVERSE_PHASE_1  →  REVERSE_PHASE_2  →  FINAL
```

| Phase | What we do |
|---|---|
| `APPROACH` | Drive forward at low PWM until the magenta block is centred (`abs(extraTag) < 15`) and within ~30 cm. |
| `ALIGN` | Continue 1 wheel-base past the block so the rear of the robot is aligned with the gap. |
| `REVERSE_PHASE_1` | Swing servo full lock toward the gap, drive in reverse until rear ToF reads ~10 cm to the rear block. |
| `REVERSE_PHASE_2` | Counter-steer to opposite full lock, continue reverse until front ToF clears the front block. |
| `FINAL` | Centre servo, low forward to settle, brake to FINISH. |

The phase machine aborts to `FINISH` if the camera goes silent (XOR fail
> 5 frames) — a parking miss is preferable to driving blind into the
arena edge.

## 8. Risk and rule-compliance summary

| Risk / rule | Mitigation |
|---|---|
| Wrong-side pass (rule violation) | Dynamic-offset sign is gated by colour bit; cannot be wrong if camera frame is valid. |
| Drift past corner exit | Yaw delta exit + heading-snap to 90° resets the absolute heading reference each corner. |
| Lap miscount | Two independent counters (gyro + camera) with consensus required. |
| Camera dropout in parking | FSM aborts to FINISH rather than driving blind. |
| Mode switch (rule 9.9) | Mode is a compile-time `#define OBSTACLE_MODE 1` — no physical switches. |
| E-Stop response | < 50 ms PWM-to-zero plus PID reset; documented in [`other/power-and-sense-management/`](../power-and-sense-management/) §3. |

Full rule mapping:
[`docs/strategy/WRO_Rule_Compliance_Matrix.md`](../../docs/strategy/WRO_Rule_Compliance_Matrix.md).

## See also

- OpenMV vision source: [`src/openmv/openmv_main.py`](../../src/openmv/openmv_main.py)
- UART protocol spec: [`docs/guides/WRO_OpenMV_UART_Protocol.md`](../../docs/guides/WRO_OpenMV_UART_Protocol.md)
- Race FSM: [`src/esp32/wro_race_fsm.cpp`](../../src/esp32/wro_race_fsm.cpp)
- Track test cases (each scenario with expected behaviour):
  [`docs/strategy/WRO_Track_Test_Cases.md`](../../docs/strategy/WRO_Track_Test_Cases.md)
