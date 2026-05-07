# WRO Full Structured System Diagram (v12)

> **Active hardware revision:** v12 — ESP32-S3-DevKitC-1 + AS5048A SPI encoders + TFMini-S UART distance sensors.
> Full pin reference: [`docs/WRO_Wiring_Map_v12.md`](../docs/WRO_Wiring_Map_v12.md).
> Migration notes: [`docs/WRO_Migration_v11_to_v12.md`](../docs/WRO_Migration_v11_to_v12.md).

## Modules covered
- Power chain: LiPo 2S/3S, KCD3 main switch, 5 V rail, 3.3 V rail (onboard ESP32-S3 regulator)
- Controllers: ESP32-S3 (motion + sensor fusion), OpenMV H7 Plus (vision)
- Actuators: BTS7960 + brushed DC motor, JX PDI-6221MG steering servo
- Safety / status: E-Stop button, status LED, onboard WS2812 RGB
- I2C bus: **single device** (ICM-20948 IMU @ 0x68). No mux.
- SPI HSPI bus: 2× AS5048A magnetic encoders (14-bit)
- UART1: TFMini-S front (115200 baud, 9-byte frames, 12 m range)
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
  %% I2C - IMU only
  %% =========================
  subgraph I2C[I2C Topology - one bus, one device]
    PUSDA[4.7k pull-up SDA -> 3.3V]
    PUSCL[4.7k pull-up SCL -> 3.3V]
    IMU[ICM-20948 @ 0x68]

    PUSDA --> IMU
    PUSCL --> IMU
  end

  %% =========================
  %% SPI HSPI - 2x AS5048A
  %% =========================
  subgraph SPIBUS[SPI HSPI - 2x AS5048A 14-bit]
    ASL[AS5048A Left  / CS=GPIO10]
    ASR[AS5048A Right / CS=GPIO14]
  end

  %% =========================
  %% UART distance + camera
  %% =========================
  subgraph UART[UART Distance + Camera]
    TFF[TFMini-S Front / UART1 12m]
    TFS[TFMini-S Side / UART SW (GPIO47, optional)]
  end

  %% =========================
  %% SIGNAL CONTROL LINES
  %% =========================
  ESP -->|GPIO38 R_EN| BTS
  ESP -->|GPIO39 L_EN| BTS
  ESP -->|GPIO40 R_PWM| BTS
  ESP -->|GPIO41 L_PWM| BTS
  ESP -->|GPIO42 PWM| SERVO

  ESP -->|GPIO8 SDA| PUSDA
  ESP -->|GPIO9 SCL| PUSCL

  ESP -->|GPIO11 MOSI / GPIO12 SCK / GPIO13 MISO| ASL
  ESP -->|GPIO11 MOSI / GPIO12 SCK / GPIO13 MISO| ASR

  ESP -->|UART1 GPIO15 RX, GPIO16 TX 5V| TFF
  ESP -.->|UART SW GPIO47 RX, optional| TFS

  %% =========================
  %% POWER DISTRIBUTION
  %% =========================
  V5 -->|+5V| ESP
  V5 -->|+5V| OPMV
  V5 -->|+5V high current| SERVO
  V5 -->|+5V REQUIRED for TFMini| TFF
  V5 -.->|+5V if TFS wired| TFS
  V5 --> C5

  ESP -->|3.3V from onboard reg| IMU
  ESP -->|3.3V from onboard reg| ASL
  ESP -->|3.3V from onboard reg| ASR

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
  ASL -->|GND| SG
  ASR -->|GND| SG
  TFF -->|GND| SG
  ESTOP -.to GND when pressed.- SG
  LED -->|Cathode / GND| SG
```

## Notes
- Do not route motor power through a breadboard.
- Keep I2C and SPI wiring short and away from motor power lines.
- TFMini-S front sensor **requires 5 V** — do not power from 3.3 V.
- OpenMV camera UART is ASCII v3 (`RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n`). Spec: [`docs/WRO_OpenMV_UART_Protocol.md`](../docs/WRO_OpenMV_UART_Protocol.md).
- The rendered `WRO_Full_System_Diagram.png` / `.svg` in this folder are **stale v11 renders** — regenerate from this `.md`/`.mmd` after merge.
