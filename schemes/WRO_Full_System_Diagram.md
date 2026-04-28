# WRO Full Structured System Diagram

This diagram includes all main modules from the assembly guide:
- Power chain: LiPo, KCD3, LM2596 #1 (5V), LM2596 #2 (3.3V)
- Controller and comms: ESP32, OpenMV UART
- Actuators: BTS7960 + DC motor, steering servo
- Safety and status: E-STOP, status LED
- I2C bus: TCA9548A + pull-ups + all sensor channels
- Grounding: star ground point

```mermaid
flowchart LR

  %% =========================
  %% POWER
  %% =========================
  subgraph PWR[Power Chain - single main switch]
    BATT[LiPo 7.4V Battery]
    SW[KCD3 Main Switch]
    LM5[LM2596 #1 -> 5.00V Rail]
    LM33[LM2596 #2 -> 3.30V Rail]
    BP[BTS7960 Power Input B+ B-]

    BATT -->|+7.4V| SW
    SW -->|+7.4V| BP
    SW -->|+7.4V| LM5
    SW -->|+7.4V| LM33
  end

  %% =========================
  %% CONTROL
  %% =========================
  subgraph CTRL[Controller and Safety]
    ESP[ESP32 DevKitC V4]
    OPMV[OpenMV H7 Plus]
    ESTOP[E-STOP Button / GPIO32 INPUT_PULLUP]
    LED[Status LED / GPIO2 + 220R]

    OPMV -->|P4 TX -> GPIO16 RX| ESP
    ESP -->|GPIO17 TX -> P5 RX| OPMV
    ESTOP -->|Active LOW| ESP
    ESP -->|GPIO2| LED
  end

  %% =========================
  %% ACTUATORS
  %% =========================
  subgraph ACT[Drive and Steering]
    BTS[BTS7960 Logic Pins]
    MOTOR[DC Motor 380]
    SERVO[JX PDI-6221MG Servo]
    MCAPS[3x 0.1uF motor EMI capacitors]
    C5[470uF on 5V rail]

    BTS -->|M+ M-| MOTOR
    MCAPS -.across terminals and case.- MOTOR
  end

  %% =========================
  %% I2C
  %% =========================
  subgraph I2C[I2C Topology via TCA9548A]
    PUSDA[4.7k pull-up SDA -> 3.3V]
    PUSCL[4.7k pull-up SCL -> 3.3V]
    TCA[TCA9548A @ 0x70]

    IMU[CH0: ICM-20948 @ 0x69]
    ASL[CH1: AS5600 Left @ 0x36]
    ASR[CH2: AS5600 Right @ 0x36]
    TOFF[CH3: VL53L1X Front @ 0x29 optional]
    TOFS[CH4: VL53L1X Side @ 0x29 optional]
    EMPTY[CH5-CH7: Reserved]

    PUSDA --> TCA
    PUSCL --> TCA
    TCA --> IMU
    TCA --> ASL
    TCA --> ASR
    TCA --> TOFF
    TCA --> TOFS
    TCA --> EMPTY
  end

  %% =========================
  %% SIGNAL CONTROL LINES
  %% =========================
  ESP -->|GPIO19 R_EN| BTS
  ESP -->|GPIO23 L_EN| BTS
  ESP -->|GPIO5 RPWM| BTS
  ESP -->|GPIO14 LPWM| BTS
  ESP -->|GPIO18 PWM| SERVO

  ESP -->|GPIO21 SDA| PUSDA
  ESP -->|GPIO22 SCL| PUSCL

  %% =========================
  %% POWER DISTRIBUTION
  %% =========================
  LM5 -->|+5V| ESP
  LM5 -->|+5V| OPMV
  LM5 -->|+5V high current| SERVO
  LM5 --> C5

  LM33 -->|+3.3V| TCA
  LM33 -->|+3.3V| IMU
  LM33 -->|+3.3V| ASL
  LM33 -->|+3.3V| ASR
  LM33 -->|+3.3V| TOFF
  LM33 -->|+3.3V| TOFS

  %% =========================
  %% STAR GROUND
  %% =========================
  SG[(Star Ground Point)]

  BATT -->|7.4V- B-| SG
  BP -->|B-| SG
  LM5 -->|VIN- GND| SG
  LM33 -->|VIN- GND| SG
  BTS -->|Logic GND| SG
  ESP -->|5V- GND| SG
  OPMV -->|5V- GND| SG
  SERVO -->|5V- GND| SG
  TCA -->|3.3V- GND| SG
  IMU -->|3.3V- GND| SG
  ASL -->|3.3V- GND| SG
  ASR -->|3.3V- GND| SG
  TOFF -->|3.3V- GND| SG
  TOFS -->|3.3V- GND| SG
  ESTOP -.to GND when pressed.- SG
  LED -->|Cathode / GND| SG
```

## Notes
- Do not route motor power through a breadboard.
- Keep I2C wiring short and away from motor power lines.
- Optional ToF modules are shown and can be disabled in firmware.
