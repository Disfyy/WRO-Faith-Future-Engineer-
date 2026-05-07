# WRO Migration Guide: v11 → v12

**Team Faith | WRO Future Engineers 2026 | May 2026**

---

## What's changing

| | v11 | v12 |
|-|-----|-----|
| MCU | ESP32 DevKitC V4 | ESP32-S3-DevKitC-1 N8R8 |
| Encoders | AS5600 (I2C 12-bit) | AS5048A (SPI 14-bit) |
| Distance | VL53L1X (I2C 4m) | TFMini-S (UART 12m) |
| I2C devices | 5 | 1 (IMU only) |
| Odometry | 277 ticks/cm | 1110 ticks/cm |

---

## Step 1 — Hardware

### Encoders
- [ ] Remove AS5600 breakout boards
- [ ] Mount AS5048A in same position (reuse magnets — same form factor)
- [ ] Gap: 0.5–1.5 mm from sensor chip
- [ ] Wire: MISO→GPIO13, SCK→GPIO12, MOSI→3.3V, CS_L→GPIO10, CS_R→GPIO14
- [ ] Power: 3.3V + GND

### TFMini-S
- [ ] Remove VL53L1X sensors
- [ ] Mount TFMini-S front facing forward, laser at 3–5 cm height
- [ ] Wire front: TX→GPIO15, RX→GPIO16
- [ ] **Power: 5V required (not 3.3V!)** + GND

### MCU
- [ ] Swap ESP32 DevKitC V4 → ESP32-S3-DevKitC-1 N8R8
- [ ] Rewire all pins per `WRO_Wiring_Map_v12.md`

---

## Step 2 — Arduino IDE

- Board: **ESP32S3 Dev Module**
- USB CDC on Boot: **Enabled**
- Flash Size: **8MB**
- PSRAM: **OPI PSRAM**
- Upload Speed: 921600

---

## Step 3 — Code changes in eps323.cpp

### 3.1 Add new includes at top
```cpp
#include <SPI.h>
#include "wro_hw_config_v12.h"
#include "as5048a_spi.h"
#include "tfmini_s.h"
```

### 3.2 Remove old includes/defines
```cpp
// DELETE these:
#include <VL53L1X.h>
#define TCA_ADDRESS 0x70
#define AS5600_ADDRESS 0x36
// All old #define GPIO pin numbers (now in wro_hw_config_v12.h)
```

### 3.3 Remove dual I2C bus
```cpp
// DELETE:
Wire1.begin(25, 26);

// DELETE functions:
TwoWire& busFor(uint8_t device) { ... }
bool tcaSelect(uint8_t ch) { ... }
bool tcaDisable() { ... }
```

### 3.4 Update I2C init in setup()
```cpp
// OLD:
Wire.begin(21, 22);
Wire1.begin(25, 26);

// NEW:
Wire.begin(I2C_SDA, I2C_SCL);   // GPIO 8, 9
Wire.setClock(400000);
// Wire1 gone — not needed!
```

### 3.5 Update encoder reads in loop()
```cpp
// OLD:
encLeft  = readEncoder(BUS_ENC_LEFT);
encRight = readEncoder(BUS_ENC_RIGHT);

// NEW (add SPI init in setup() first):
as5048a_init();  // in setup()

encLeft  = readEncoderLeft();   // in loop()
encRight = readEncoderRight();
```

### 3.6 Update odometry wrap-around (CRITICAL)
```cpp
// OLD (12-bit AS5600):
if (dL >  2048) dL -= 4096;
if (dL < -2048) dL += 4096;

// NEW (14-bit AS5048A):
if (dL >  AS5048A_HALF_RES) dL -= AS5048A_RESOLUTION;
if (dL < -AS5048A_HALF_RES) dL += AS5048A_RESOLUTION;
// Same for dR
```

### 3.7 Replace VL53L1X with TFMini-S
```cpp
// OLD in setup():
initToF();

// NEW:
tfmini_initAll();   // in setup()

// OLD in loop():
readToF();
// using distFrontMM, distSideMM

// NEW in loop():
tfmini_readAll();
int distFrontMM = tfFront.distMM;
int distSideMM  = tfSide.distMM;
```

### 3.8 Update camera UART pins
```cpp
// OLD:
Serial2.begin(115200, SERIAL_8N1, 16, 17);

// NEW:
Serial2.begin(CAMERA_BAUD, SERIAL_8N1, CAMERA_RX, CAMERA_TX);
// CAMERA_RX=17, CAMERA_TX=18 (defined in wro_hw_config_v12.h)
```

---

## Step 4 — Verification order

1. `WRO_ACTIVE_TARGET = 2` → I2C scan → should find ONLY 0x68
2. `WRO_ACTIVE_TARGET = 8` → Encoder test → spin wheels, verify ticks
3. `WRO_ACTIVE_TARGET = 9` → TFMini test → verify distances
4. `WRO_ACTIVE_TARGET = 10` → Bench test → all sensors + motor + servo
5. `WRO_ACTIVE_TARGET = 1` → Full race firmware

---

## Rollback plan

Keep v11 hardware in spare box. To revert at competition:
1. Swap ESP32-S3 → ESP32 DevKitC V4
2. Reconnect AS5600 encoders (dual I2C)
3. Reconnect VL53L1X sensors (XSHUT gymnastics)
4. Upload git tag `v11-competition-ready`
5. Time estimate: ~30 min
