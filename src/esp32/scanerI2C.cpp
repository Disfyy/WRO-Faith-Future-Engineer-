/*
 * WRO Future Engineers - Умный сканер I2C
 * Версия: Финальная
 * Железо: ESP32 DevKit, TCA9548A, AS5600 (x2), ICM-20948
 */

#include <Wire.h>

// ==============================
// ПИНЫ И АДРЕСА
// ==============================
#define I2C_SDA      21
#define I2C_SCL      22
#define TCA_ADDRESS  0x70

// ==============================
// ОЖИДАЕМАЯ КАРТА КАНАЛОВ
// ==============================
// Канал 0 -> ICM-20948 (0x69)
// Канал 1 -> AS5600 левый (0x36)
// Канал 2 -> AS5600 правый (0x36)
// Канал 3 -> VL53L1X левый (0x29)
// Канал 4 -> VL53L1X правый (0x29)
// Каналы 5-7 -> пусто

// ==============================
// ЦЕЛЕВЫЕ АДРЕСА (Targeted Scan)
// ==============================
const uint8_t targetAddresses[] = {0x36, 0x68, 0x69, 0x28, 0x29};
const int     numTargets = sizeof(targetAddresses) / sizeof(targetAddresses[0]);

const int16_t expectedAddressByChannel[8] = {
  0x69,  // CH0: ICM-20948
  0x36,  // CH1: AS5600 left
  0x36,  // CH2: AS5600 right
  0x29,  // CH3: VL53L1X left
  0x29,  // CH4: VL53L1X right
  -1, -1, -1
};

// ==============================
// МУЛЬТИПЛЕКСОР
// ==============================
bool tcaSelect(uint8_t channel) {
  if (channel > 7) return false;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << channel);
  return (Wire.endTransmission() == 0);
}

bool tcaDisable() {
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(0);
  return (Wire.endTransmission() == 0);
}

// ==============================
// ОПРЕДЕЛЕНИЕ ДАТЧИКА ПО АДРЕСУ
// ==============================
String getSensorName(uint8_t address) {
  switch (address) {
    case 0x36: return "AS5600 (энкодер)";
    case 0x68: return "ICM-20948 (IMU, AD0=GND)";
    case 0x69: return "ICM-20948 (IMU, AD0=VCC)";
    case 0x28: return "BNO085 (SA0=GND)";
    case 0x29: return "BNO085 (SA0=VCC)";
    default:   return "неизвестный";
  }
}

// ==============================
// ПРОВЕРКА ОДНОГО АДРЕСА
// ==============================
bool checkAddress(uint8_t address, uint8_t retries = 2) {
  for (uint8_t i = 0; i < retries; i++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) return true;
    delay(1);
  }
  return false;
}

// ==============================
// ИТОГОВЫЙ ОТЧЁТ
// ==============================
void printExpectedMap() {
  Serial.println("\n--- Ожидаемая карта каналов ---");
  Serial.println("  Канал 0 -> ICM-20948  адрес 0x69");
  Serial.println("  Канал 1 -> AS5600     адрес 0x36 (левый)");
  Serial.println("  Канал 2 -> AS5600     адрес 0x36 (правый)");
  Serial.println("  Каналы 3-7 -> пусто");
  Serial.println("--------------------------------");
}

// ==============================
// SETUP
// ==============================
void setup() {
  Serial.begin(115200);

  // Защита от зависания без ноутбука
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000);

  delay(500);
  Serial.println("\n╔═══════════════════════════════╗");
  Serial.println("║   Умный сканер I2C WRO v2.0   ║");
  Serial.println("╚═══════════════════════════════╝");

  // I2C на боевой частоте
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Serial.println("Шина I2C запущена (400 кГц).");

  // Печатаем ожидаемую карту
  printExpectedMap();
}

// ==============================
// LOOP
// ==============================
void loop() {
  Serial.println("\n=== Начало сканирования ===");

  // --------------------------------------------------
  // ШАГ 1: Проверка мультиплексора TCA9548A
  // --------------------------------------------------
  Serial.print("Проверка TCA9548A (0x70)... ");
  if (!checkAddress(TCA_ADDRESS)) {
    Serial.println("НЕ НАЙДЕН!");
    Serial.println("Проверь подключение:");
    Serial.println("  VCC  -> 3.3V");
    Serial.println("  GND  -> GND");
    Serial.println("  SDA  -> GPIO 21");
    Serial.println("  SCL  -> GPIO 22");
    Serial.println("  A0/A1/A2 -> GND (адрес 0x70)");
    delay(3000);
    return;
  }
  Serial.println("OK");

  // --------------------------------------------------
  // ШАГ 2: Сканирование каналов
  // --------------------------------------------------
  int totalFound  = 0;
  int totalErrors = 0;

  for (uint8_t channel = 0; channel < 8; channel++) {
    if (!tcaSelect(channel)) {
      Serial.print("Канал [");
      Serial.print(channel);
      Serial.println("]: ОШИБКА TCA (нет ACK)");
      totalErrors++;
      continue;
    }
    delay(2); // Пауза для физического открытия ключей

    Serial.print("Канал [");
    Serial.print(channel);
    Serial.print("]: ");

    int devicesFound = 0;
    bool expectedFound = false;
    int16_t expectedAddress = expectedAddressByChannel[channel];

    for (int i = 0; i < numTargets; i++) {
      uint8_t address = targetAddresses[i];

      if (checkAddress(address)) {
        Serial.print("0x");
        if (address < 16) Serial.print("0");
        Serial.print(address, HEX);
        Serial.print(" ");
        Serial.print(getSensorName(address));
        if (expectedAddress >= 0 && address == expectedAddress) {
          expectedFound = true;
        }
        if (expectedAddress < 0) {
          Serial.print(" [НЕОЖИДАННО НА ПУСТОМ КАНАЛЕ]");
          totalErrors++;
        }
        Serial.print("  ");
        devicesFound++;
        totalFound++;
      }
    }

    if (devicesFound == 0) {
      Serial.print("пусто");

      if (expectedAddress >= 0) {
        Serial.print("  <- ВНИМАНИЕ: ожидался адрес 0x");
        if (expectedAddress < 16) Serial.print("0");
        Serial.print((uint8_t)expectedAddress, HEX);
        Serial.print(" (");
        Serial.print(getSensorName((uint8_t)expectedAddress));
        Serial.print(")!");
        totalErrors++;
      }
    } else if (expectedAddress >= 0 && !expectedFound) {
      Serial.print(" <- ВНИМАНИЕ: найдено устройство, но не ожидаемый адрес 0x");
      if (expectedAddress < 16) Serial.print("0");
      Serial.print((uint8_t)expectedAddress, HEX);
      totalErrors++;
    }

    Serial.println();
  }

  // Безопасное закрытие всех каналов
  if (!tcaDisable()) {
    Serial.println("WARN: Не удалось закрыть каналы TCA9548A");
    totalErrors++;
  }

  // --------------------------------------------------
  // ШАГ 3: Итоговый отчёт
  // --------------------------------------------------
  Serial.println("\n--- Итог сканирования ---");
  Serial.print("Найдено датчиков: ");
  Serial.println(totalFound);
  Serial.print("Проблем: ");
  Serial.println(totalErrors);

  if (totalErrors == 0 && totalFound >= 3) {
    Serial.println("СТАТУС: ВСЕ ДАТЧИКИ В НОРМЕ — можно заливать основной код!");
  } else if (totalErrors > 0) {
    Serial.println("СТАТУС: ЕСТЬ ПРОБЛЕМЫ — проверь подключение датчиков.");
  } else {
    Serial.println("СТАТУС: Датчиков меньше ожидаемого — проверь каналы 0,1,2.");
  }

  Serial.println("=== Следующее сканирование через 5 сек ===");
  delay(5000);
}