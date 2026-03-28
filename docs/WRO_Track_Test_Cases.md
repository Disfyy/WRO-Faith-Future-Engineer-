# WRO Track Test Cases

## TC-01 Stand Safety Test
- Goal: verify no unexpected motion at startup
- Steps: power on with wheels lifted
- Pass: steering centers, motor output is zero

## TC-02 E-Stop Reaction
- Goal: immediate stop in motion
- Steps: command motion, press E-Stop
- Pass: motor speed zero + steering center instantly

## TC-03 Camera Timeout Stop
- Goal: fail-safe on vision loss
- Steps: disconnect camera UART while running
- Pass: warning in serial and safe stop

## TC-04 Encoder Loss Handling
- Goal: critical encoder failure lockout
- Steps: disconnect one encoder and run
- Pass: encoder alarm then motion blocked until recovery

## TC-05 Lap Counting Stability
- Goal: verify stable lap increments
- Steps: run 3 laps with consistent direction
- Pass: lap count increments correctly, no extra jumps

## TC-06 Recovery Behavior
- Goal: smooth resume after temporary fault
- Steps: induce short sensor fault then recover
- Pass: robot resumes control without unsafe jump

## Record for each test
- Date/time
- Firmware version
- Track condition
- Battery voltage
- Result: PASS/FAIL
- Notes and follow-up action
