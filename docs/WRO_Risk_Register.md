# WRO Risk Register

| ID | Risk | Impact | Likelihood | Detection | Mitigation | Owner |
|----|------|--------|------------|-----------|------------|-------|
| R1 | Camera frame loss | High | Medium | UART timeout warnings | Cable fixation, protocol validation, safe stop | Vision |
| R2 | I2C noise / dropout | High | Medium | Scanner mismatch, encoder alarms | Short I2C wires, solid GND, retries | Electronics |
| R3 | E-Stop hardware failure | Critical | Low | Preflight E-Stop test fail | Dual checks each run, button replacement policy | Safety |
| R4 | IMU drift | Medium | Medium | Wrong lap increments | Recalibration, vibration isolation, dt clamp | Controls |
| R5 | Wheel slip at high speed | High | Medium | Unstable line following | Speed ramping, turn speed limits, tire setup | Mechanics |
| R6 | Brownout under load | High | Medium | Random resets | Battery health checks, power distribution improvements | Electronics |
| R7 | Connector loosening | Medium | Medium | Intermittent failures | Strain relief, locking connectors, preflight inspection | Hardware |

## Update rule
- Review before every competition session.
- Add new risks discovered in testing.
- Track mitigation status weekly.
