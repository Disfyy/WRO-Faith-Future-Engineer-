# WRO Full Structured System Diagram (v13)

> **Active hardware revision:** v13 — ESP32-S3-DevKitC-1 + 2× AS5600 dual-I2C + 2× VL53L1X (XSHUT-based runtime address remap).
> Full pin reference: [`docs/guides/WRO_Wiring_Map_v13.md`](../docs/guides/WRO_Wiring_Map_v13.md).
> Migration notes: [`docs/strategy/WRO_Migration_v12_to_v13.md`](../docs/strategy/WRO_Migration_v12_to_v13.md).

## Modules covered
- Power chain: LiPo 2S/3S, KCD3 main switch, 5 V rail, 3.3 V rail (onboard ESP32-S3 regulator)
- Controllers: ESP32-S3 (motion + sensor fusion), OpenMV H7 Plus (vision)
- Actuators: BTS7960 + brushed DC motor, JX PDI-6221MG steering servo
- Safety / status: E-Stop button, status LED, onboard WS2812 RGB
- I2C0 (Wire, GPIO 8/9): ICM-20948 (0x68) + AS5600 Left (0x36) + VL53L1X Front (0x29 → 0x30)
- I2C1 (Wire1, GPIO 11/12): AS5600 Right (0x36) + VL53L1X Side (0x29 → 0x31)
- VL53L1X XSHUTs on GPIO 15 / 16 (third reserved on GPIO 47)
- UART2: OpenMV camera (115200 baud, ASCII v3 protocol with XOR checksum)
- Grounding: star ground point

```mermaid
flowchart LR

  %% =========================
  %% POWER
  %% =========================
  subgraph PWR[Power Chain - single main switch]
    BATT[LiPo 2S/3S Battery]
    SW[KCD3 Main Switch]
    V5[+5V Rail / Buck or LM2596]
    BP[BTS7960 Power Input B+ B-]

    BATT -->|+VBAT| SW
    SW -->|+VBAT| BP
    SW -->|+VBAT| V5
  end

  %% =========================
  %% CONTROL
  %% =========================
  subgraph CTRL[Controllers and Safety]
    ESP[ESP32-S3-DevKitC-1 N8R8]
    OPMV[OpenMV H7 Plus]
    ESTOP[E-STOP Button / GPIO21 INPUT_PULLUP]
    LED[Status LED / GPIO2 + 220R]
    RGB[Onboard WS2812 / GPIO48]

    OPMV -->|TX -> GPIO17 RX| ESP
    ESP -->|GPIO18 TX -> RX| OPMV
    ESTOP -->|Active LOW| ESP
    ESP -->|GPIO2| LED
    ESP -->|GPIO48| RGB
  end

  %% =========================
  %% ACTUATORS
  %% =========================
  subgraph ACT[Drive and Steering]
    BTS[BTS7960 Logic Pins]
    MOTOR[Brushed DC Motor 380]
    SERVO[JX PDI-6221MG Servo]
    MCAPS[3x 0.1uF motor EMI capacitors]
    C5[470uF on 5V rail]

    BTS -->|M+ M-| MOTOR
    MCAPS -.across terminals and case.- MOTOR
  end

  %% =========================
  %% I2C0 - IMU + AS5600 L + VL53L1X F
  %% =========================
  subgraph I2C0[I2C0 Wire - GPIO 8/9]
    PUSDA0[4.7k pull-up SDA -> 3.3V]
    PUSCL0[4.7k pull-up SCL -> 3.3V]
    IMU[ICM-20948 @ 0x68]
    AS5600L[AS5600 Left @ 0x36]
    TFFR[VL53L1X Front @ 0x30 / XSHUT GPIO15]

    PUSDA0 --> IMU
    PUSCL0 --> IMU
    PUSDA0 --> AS5600L
    PUSCL0 --> AS5600L
    PUSDA0 --> TFFR
    PUSCL0 --> TFFR
  end

  %% =========================
  %% I2C1 - AS5600 R + VL53L1X S
  %% =========================
  subgraph I2C1[I2C1 Wire1 - GPIO 11/12]
    PUSDA1[4.7k pull-up SDA -> 3.3V]
    PUSCL1[4.7k pull-up SCL -> 3.3V]
    AS5600R[AS5600 Right @ 0x36]
    TFSD[VL53L1X Side @ 0x31 / XSHUT GPIO16]

    PUSDA1 --> AS5600R
    PUSCL1 --> AS5600R
    PUSDA1 --> TFSD
    PUSCL1 --> TFSD
  end

  %% =========================
  %% SIGNAL CONTROL LINES
  %% =========================
  ESP -->|GPIO38 R_EN| BTS
  ESP -->|GPIO39 L_EN| BTS
  ESP -->|GPIO40 R_PWM| BTS
  ESP -->|GPIO41 L_PWM| BTS
  ESP -->|GPIO42 PWM| SERVO

  ESP -->|GPIO8 SDA| PUSDA0
  ESP -->|GPIO9 SCL| PUSCL0
  ESP -->|GPIO11 SDA| PUSDA1
  ESP -->|GPIO12 SCL| PUSCL1
  ESP -->|GPIO15 XSHUT| TFFR
  ESP -->|GPIO16 XSHUT| TFSD

  %% =========================
  %% POWER DISTRIBUTION
  %% =========================
  V5 -->|+5V| ESP
  V5 -->|+5V| OPMV
  V5 -->|+5V high current| SERVO
  V5 -->|+5V VIN| TFFR
  V5 -->|+5V VIN| TFSD
  V5 --> C5

  ESP -->|3.3V from onboard reg| IMU
  ESP -->|3.3V from onboard reg| AS5600L
  ESP -->|3.3V from onboard reg| AS5600R

  %% =========================
  %% STAR GROUND
  %% =========================
  SG[(Star Ground Point)]

  BATT -->|VBAT- B-| SG
  BP -->|B-| SG
  V5 -->|GND| SG
  BTS -->|Logic GND| SG
  ESP -->|GND| SG
  OPMV -->|GND| SG
  SERVO -->|GND| SG
  IMU -->|GND| SG
  AS5600L -->|GND| SG
  AS5600R -->|GND| SG
  TFFR -->|GND| SG
  TFSD -->|GND| SG
  ESTOP -.to GND when pressed.- SG
  LED -->|Cathode / GND| SG
```

## Notes
- Do not route motor power through a breadboard.
- Keep I2C wiring short and away from motor power lines.
- The VL53L1X address-remap dance happens once at boot in `vl53l1x_dual.h` — XSHUTs are held LOW until the firmware is ready to bring each sensor up.
- OpenMV camera UART is ASCII v3 (`RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n`). Spec: [`docs/guides/WRO_OpenMV_UART_Protocol.md`](../docs/guides/WRO_OpenMV_UART_Protocol.md).
- The rendered `WRO_Full_System_Diagram.png` / `.svg` in this folder are **stale renders from earlier revisions** — regenerate from this `.md`/`.mmd` when the bench wiring is finalized.
