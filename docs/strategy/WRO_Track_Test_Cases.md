# WRO Track Test Cases

## How to run this file

- Execute test cases in order from TC-01 to TC-08.
- If any safety-critical test fails, stop and fix before continuing.
- Record every result in `docs/logs/WRO_Test_Log.csv`.

## TC-01 Stand Safety Test

Goal:
- Verify no unexpected motion at startup.

Steps:
1. Place robot on stand (wheels off ground).
2. Power on and wait for ready state.
3. Observe steering and motor output for 10 seconds.

Pass criteria:
- Steering moves to center and remains stable.
- Motor output remains zero.
- No uncontrolled wheel rotation.

Fail action:
- Block floor tests.
- Inspect startup state logic and motor enable pins.

## TC-02 E-Stop Reaction

Goal:
- Verify emergency stop and safe resume behavior.

Steps:
1. Run robot at low speed on safe surface.
2. Press E-Stop.
3. Confirm stop behavior.
4. Release E-Stop and verify resume behavior.

Pass criteria:
- Press E-Stop -> motor = 0 and steering = center.
- Release E-Stop -> controlled resume only, no sudden jump.

Fail action:
- Block all race runs.
- Recheck GPIO32 wiring, debounce, and `safeStop()` path.

## TC-03 Camera Timeout Stop

Goal:
- Verify fail-safe behavior on camera data loss.

Steps:
1. Start run with valid camera stream.
2. Disconnect UART camera line (or power off camera).
3. Observe serial logs and robot behavior.

Pass criteria:
- Camera timeout warning appears after packet loss.
- If offline persists, robot enters SAFE_STOP.
- No uncontrolled movement while camera is offline.

Fail action:
- Block race runs.
- Verify UART wiring and camera power stability.

## TC-04 Encoder Loss Handling

Goal:
- Verify lockout on persistent encoder failure.

Steps:
1. Start run at low speed.
2. Disconnect one encoder channel (CH1 or CH2).
3. Observe alarms and robot behavior.

Pass criteria:
- Encoder-loss alarm appears.
- Persistent failure triggers SAFE_STOP.
- Motion remains blocked until sensor path is restored.

Fail action:
- Apply rollback procedure from Stage 7.5 in assembly guide.

## TC-05 Lap Counting Stability

Goal:
- Verify robust lap counting without extra increments.

Steps:
1. Run 3 laps in CW direction.
2. Reset and run 3 laps in CCW direction.
3. Compare line-based and gyro-based counters in logs.

Pass criteria:
- Final lap count is exactly 3 in both directions.
- No extra lap jumps.
- Finish transition happens only after valid lap completion.

Fail action:
- Recalibrate line thresholds.
- Recheck cooldown and line-confirm logic.

## TC-06 Recovery Behavior

Goal:
- Verify smooth recovery after short non-critical fault.

Steps:
1. Induce short disturbance (temporary partial view loss or short sensor glitch).
2. Restore normal conditions.
3. Observe steering and speed transition.

Pass criteria:
- Robot recovers control without unsafe jerk.
- No uncommanded acceleration spike.
- Track direction remains consistent.

Fail action:
- Tune PID/damping and verify sensor noise filtering.

## TC-07 Start Procedure Compliance (WRO 9.6-9.14)

Goal:
- Verify start sequence matches official rules.

Steps:
1. Place robot in start zone switched off.
2. Switch on with one power switch.
3. Confirm waiting state before start.
4. On judge-like command, press one start button.

Pass criteria:
- Robot is switched off before placement.
- Exactly one switch is used for power on.
- Exactly one start button triggers motion.
- Motion starts only after start button press.

Fail action:
- Fix startup logic or hardware interface before competition.

## TC-08 Finish Behavior Compliance (WRO 9.24.2 / 9.24.4)

Goal:
- Verify legal end-of-round behavior for both challenges.

Steps:
1. Open Challenge run: complete 3 laps.
2. Obstacle Challenge run: complete 3 laps and parking sequence.
3. Observe finish conditions and robot stop.

Pass criteria:
- Open: after 3 complete laps, robot stops autonomously in finish section.
- Obstacle: after 3 complete laps, robot stops in valid end state (correct section or parking lot).
- No manual touch used to force finish.

Fail action:
- Recheck lap completion logic and finish-state transitions.

## Record for each test

Required fields:
- Date/time
- Firmware version
- Challenge mode (Open/Obstacle)
- Track condition and direction (CW/CCW)
- Battery voltage (start/end)
- Result: PASS/FAIL
- Notes and follow-up action
