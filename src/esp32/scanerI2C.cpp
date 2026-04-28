/*
 * I2C Scanner через TCA9548A для WRO Future Engineers
 * Сканирует все 8 каналов мультиплексора и выводит найденные устройства.
 *
 * Использование:
 *   1. Загрузить на ESP32 через Arduino IDE
 *   2. Открыть Serial Monitor (115200 бод)
 *   3. Проверить что все датчики видны на ожидаемых каналах
 *
 * Ожидаемый результат:
 *   Канал [0]: 0x69 — ICM-20948 (IMU)
 *   Канал [1]: 0x36 — AS5600 (энкодер)
 *   Канал [2]: 0x36 — AS5600 (энкодер)
 *   Канал [3]: 0x29 — VL53L1X (ToF фронт) [если подключен]
 *   Канал [4]: 0x29 — VL53L1X (ToF бок)   [если подключен]
 */

#include "wro_build_target.h"

#if WRO_ACTIVE_TARGET == WRO_TARGET_SCAN_I2C

#include <Wire.h>

#define TCA_ADDRESS 0x70
#define I2C_SDA 21
#define I2C_SCL 22

struct KnownDevice {
  uint8_t address;
  const char *name;
};

KnownDevice knownDevices[] = {
    {0x29, "VL53L1X (ToF)"},
    {0x36, "AS5600 (энкодер)"},
    {0x68, "ICM-20948 (AD0=GND)"},
    {0x69, "ICM-20948 (AD0=VCC)"},
    {0x70, "TCA9548A (мультиплексор)"},
};
const int numKnown = sizeof(knownDevices) / sizeof(knownDevices[0]);

const char *lookupDevice(uint8_t addr) {
  for (int i = 0; i < numKnown; i++) {
    if (knownDevices[i].address == addr)
      return knownDevices[i].name;
  }
  return "неизвестное устройство";
}

bool tcaSelect(uint8_t channel) {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << channel);
  return (Wire.endTransmission() == 0);
}

void tcaDisable() {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║   WRO I2C Scanner (TCA9548A)          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  Serial.println();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // 100 кГц для надёжного сканирования

  // Проверка самого TCA9548A
  Wire.beginTransmission(TCA_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("❌ ОШИБКА: TCA9548A (0x70) НЕ НАЙДЕН!");
    Serial.println("   Проверьте:");
    Serial.println("   - SDA подключен к GPIO21");
    Serial.println("   - SCL подключен к GPIO22");
    Serial.println("   - Питание 3.3V на TCA9548A");
    Serial.println("   - Pull-up 4.7kОм на SDA и SCL");
    Serial.println("   - A0, A1, A2 подтянуты к GND");
    while (true)
      delay(1000);
  }
  Serial.println("✅ TCA9548A (0x70): НАЙДЕН\n");

  // Ожидаемая конфигурация
  struct ExpectedEntry {
    uint8_t channel;
    uint8_t address;
    const char *role;
  };
  ExpectedEntry expected[] = {
      {0, 0x69, "ICM-20948 (IMU)"},
      {1, 0x36, "AS5600 (энкодер ЛЕВЫЙ)"},
      {2, 0x36, "AS5600 (энкодер ПРАВЫЙ)"},
      {3, 0x29, "VL53L1X (ToF ФРОНТ)"},
      {4, 0x29, "VL53L1X (ToF БОК)"},
  };
  int numExpected = 5;
  int found = 0;
  int missing = 0;

  // Сканируем каждый канал
  for (uint8_t ch = 0; ch < 8; ch++) {
    if (!tcaSelect(ch)) {
      Serial.print("Канал [");
      Serial.print(ch);
      Serial.println("]: ОШИБКА ПЕРЕКЛЮЧЕНИЯ");
      continue;
    }

    int devicesOnChannel = 0;
    Serial.print("Канал [");
    Serial.print(ch);
    Serial.print("]: ");

    for (uint8_t addr = 1; addr < 127; addr++) {
      if (addr == TCA_ADDRESS)
        continue; // Пропустить сам TCA
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        if (devicesOnChannel > 0)
          Serial.print(", ");
        Serial.print("0x");
        if (addr < 16)
          Serial.print("0");
        Serial.print(addr, HEX);
        Serial.print(" — ");
        Serial.print(lookupDevice(addr));
        devicesOnChannel++;
      }
    }

    if (devicesOnChannel == 0) {
      Serial.print("пусто");
    }
    Serial.println();

    tcaDisable();
    delay(10);
  }

  // Проверка ожидаемых устройств
  Serial.println("\n─── ПРОВЕРКА КОНФИГУРАЦИИ ───");
  for (int i = 0; i < numExpected; i++) {
    tcaSelect(expected[i].channel);

    Wire.beginTransmission(expected[i].address);
    bool ok = (Wire.endTransmission() == 0);

    Serial.print("  CH");
    Serial.print(expected[i].channel);
    Serial.print(" → ");
    Serial.print(expected[i].role);
    Serial.print(": ");
    if (ok) {
      Serial.println("✅ OK");
      found++;
    } else {
      if (expected[i].channel >= 3) {
        Serial.println("⚠️  НЕ НАЙДЕН (VL53L1X опциональный)");
      } else {
        Serial.println("❌ НЕ НАЙДЕН!");
        missing++;
      }
    }

    tcaDisable();
    delay(10);
  }

  Serial.println("\n─── ИТОГ ───");
  Serial.print("Найдено: ");
  Serial.print(found);
  Serial.print("/");
  Serial.println(numExpected);

  if (missing == 0) {
    Serial.println("✅ СТАТУС: ВСЕ ОБЯЗАТЕЛЬНЫЕ ДАТЧИКИ В НОРМЕ");
  } else {
    Serial.print("❌ СТАТУС: ОТСУТСТВУЮТ ");
    Serial.print(missing);
    Serial.println(" ОБЯЗАТЕЛЬНЫХ ДАТЧИКА!");
    Serial.println("   Проверьте проводку и питание.");
  }

  Serial.println("\n(Сканирование завершено. Перезагрузите для повтора.)");
}

void loop() {
  // Ничего не делаем — однократный скан
  delay(1000);
}

#endif // WRO_ACTIVE_TARGET == WRO_TARGET_SCAN_I2C