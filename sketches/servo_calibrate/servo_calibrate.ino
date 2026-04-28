// =====================================================================
//  WRO — Калибровка сервопривода рулевого
// =====================================================================
//
//  ЗАЧЕМ: Сервопривод не всегда центрируется ровно на 90°. Этот скетч
//  помогает найти ТОЧНОЕ значение в микросекундах для:
//    - центра (колёса прямо)
//    - максимального ЛЕВОГО упора
//    - максимального ПРАВОГО упора
//
//  ПОДГОТОВКА:
//    1. Подними шасси, чтобы колёса крутились свободно
//    2. Залей этот скетч
//    3. Открой Serial Monitor 115200 baud
//
//  ПОШАГОВАЯ ИНСТРУКЦИЯ:
//    Шаг 1. Команда `c` — серво на старте 1500мкс. Посмотри на колёса.
//    Шаг 2. Подкручивай `+` (+5мкс) или `-` (-5мкс) пока колёса не будут
//           ИДЕАЛЬНО ПРЯМО. Используй `>` (+50) и `<` (-50) для крупных шагов.
//    Шаг 3. Когда колёса прямо — нажми `C` чтобы сохранить ЦЕНТР.
//    Шаг 4. Кручи `+` пока колёса не упрутся вправо до упора.
//           Нажми `R` — сохранится МАКСИМАЛЬНЫЙ ПРАВЫЙ.
//    Шаг 5. Кручи `-` пока колёса не упрутся влево до упора.
//           Нажми `L` — сохранится МАКСИМАЛЬНЫЙ ЛЕВЫЙ.
//    Шаг 6. Нажми `p` — увидишь итоговые значения для копирования в код.
//
//  ВАЖНО: Не оставляй серво в упоре долго — он перегревается!
//  Если жужжит на упоре — отступи на 50мкс назад.
//
//  КОМАНДЫ:
//    +/-       Точная подстройка (±5 мкс)
//    >/<       Грубая (±50 мкс)
//    c         Показать текущее значение
//    1500      Установить конкретное значение (просто число)
//    sweep     Плавный обход: центр→правый→центр→левый→центр
//    C         Сохранить как ЦЕНТР
//    R         Сохранить как МАКС ПРАВЫЙ
//    L         Сохранить как МАКС ЛЕВЫЙ
//    p         Показать сводку калибровки
//    h         Помощь
// =====================================================================

#include <ESP32Servo.h>

#define SERVO_PIN     27
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2500

Servo steeringServo;
int currentUS = 1500;

int calCenter = 0;
int calRight  = 0;
int calLeft   = 0;

void setUS(int us) {
  us = constrain(us, SERVO_MIN_US, SERVO_MAX_US);
  currentUS = us;
  steeringServo.writeMicroseconds(us);
}

void printPos() {
  Serial.print("  >> серво = ");
  Serial.print(currentUS);
  Serial.println(" мкс");
}

void smoothMove(int from, int to) {
  int step = (to > from) ? 5 : -5;
  for (int us = from; (step > 0) ? (us <= to) : (us >= to); us += step) {
    setUS(us);
    delay(15);
  }
  setUS(to);
}

void runSweep() {
  int c = calCenter > 0 ? calCenter : 1500;
  int r = calRight  > 0 ? calRight  : 2000;
  int l = calLeft   > 0 ? calLeft   : 1000;

  Serial.println("\n--- ПЛАВНЫЙ ОБХОД ---");
  Serial.print("  центр("); Serial.print(c); Serial.println(")...");
  smoothMove(currentUS, c);  delay(500);
  Serial.print("  правый("); Serial.print(r); Serial.println(")...");
  smoothMove(c, r);          delay(500);
  Serial.print("  центр...");
  smoothMove(r, c);          delay(500);
  Serial.print("\n  левый("); Serial.print(l); Serial.println(")...");
  smoothMove(c, l);          delay(500);
  Serial.print("  центр...\n");
  smoothMove(l, c);
  Serial.println("--- ОБХОД ОКОНЧЕН ---\n");
}

void printHelp() {
  Serial.println("\n┌─── КОМАНДЫ ────────────────────────────┐");
  Serial.println("│ +/-      ±5 мкс (точная подстройка)    │");
  Serial.println("│ >/<      ±50 мкс (грубая)              │");
  Serial.println("│ c        Показать текущее              │");
  Serial.println("│ 1500     Установить конкретное число   │");
  Serial.println("│ sweep    Плавный обход (центр→Л→Ц→П→Ц) │");
  Serial.println("│ C        Сохранить как ЦЕНТР           │");
  Serial.println("│ R        Сохранить как МАКС ПРАВЫЙ     │");
  Serial.println("│ L        Сохранить как МАКС ЛЕВЫЙ      │");
  Serial.println("│ p        Показать сводку               │");
  Serial.println("│ h        Эта помощь                    │");
  Serial.println("└────────────────────────────────────────┘\n");
}

void printSummary() {
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║      СВОДКА КАЛИБРОВКИ               ║");
  Serial.println("╠══════════════════════════════════════╣");

  Serial.print("║  ЦЕНТР:       ");
  if (calCenter > 0) { Serial.print(calCenter); Serial.println(" мкс"); }
  else { Serial.println("(не установлен)"); }

  Serial.print("║  МАКС ПРАВЫЙ: ");
  if (calRight > 0) { Serial.print(calRight); Serial.println(" мкс"); }
  else { Serial.println("(не установлен)"); }

  Serial.print("║  МАКС ЛЕВЫЙ:  ");
  if (calLeft > 0) { Serial.print(calLeft); Serial.println(" мкс"); }
  else { Serial.println("(не установлен)"); }

  if (calCenter > 0 && calRight > 0 && calLeft > 0) {
    int rangeR = calRight - calCenter;
    int rangeL = calCenter - calLeft;

    Serial.println("╠══════════════════════════════════════╣");
    Serial.print("║  Ход вправо: +"); Serial.print(rangeR); Serial.println(" мкс");
    Serial.print("║  Ход влево:  -"); Serial.print(rangeL); Serial.println(" мкс");

    if (abs(rangeR - rangeL) > 50) {
      Serial.println("║  Несимметрично! Возьми меньший ход.");
    }

    Serial.println("╠══════════════════════════════════════╣");
    Serial.println("║  Скопируй в eps323.cpp:              ║");
    Serial.println("║                                      ║");
    Serial.print("║  #define SERVO_CENTER_US  "); Serial.println(calCenter);
    Serial.print("║  #define SERVO_RIGHT_US   "); Serial.println(calRight);
    Serial.print("║  #define SERVO_LEFT_US    "); Serial.println(calLeft);
  }

  Serial.println("╚══════════════════════════════════════╝\n");
}

void setup() {
  Serial.begin(115200);
  delay(800);

  steeringServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  setUS(1500);

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  WRO — КАЛИБРОВКА СЕРВОПРИВОДА           ║");
  Serial.println("║  JX PDI-6221MG на GPIO 27                ║");
  Serial.println("║  Старт: 1500 мкс                         ║");
  Serial.println("╚══════════════════════════════════════════╝\n");

  Serial.println("ШАГ 1. Подними шасси, чтобы колёса крутились свободно.");
  Serial.println("ШАГ 2. Подкручивай '+' / '-' пока колёса не будут ПРЯМО.");
  Serial.println("ШАГ 3. Нажми 'C' — сохранить ЦЕНТР.");
  Serial.println("ШАГ 4. Кручи '+' до правого упора, нажми 'R'.");
  Serial.println("ШАГ 5. Кручи '-' до левого упора, нажми 'L'.");
  Serial.println("ШАГ 6. Нажми 'p' — получишь итоговые цифры.\n");

  printHelp();
  Serial.println("Готов. Текущее: 1500 мкс");
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // Если просто число — установить значение
  bool allDigits = true;
  for (size_t i = 0; i < cmd.length(); i++) {
    if (!isDigit(cmd[i])) { allDigits = false; break; }
  }
  if (allDigits) {
    int v = cmd.toInt();
    if (v >= SERVO_MIN_US && v <= SERVO_MAX_US) {
      setUS(v);
      printPos();
    } else {
      Serial.print("  Диапазон 500–2500. Ты ввёл: ");
      Serial.println(v);
    }
    return;
  }

  // Длинные команды
  if (cmd == "sweep") { runSweep(); return; }
  if (cmd == "h" || cmd == "help") { printHelp(); return; }

  // Однобуквенные
  char ch = cmd[0];
  switch (ch) {
    case '+': setUS(currentUS + 5);   printPos(); break;
    case '-': setUS(currentUS - 5);   printPos(); break;
    case '>': setUS(currentUS + 50);  printPos(); break;
    case '<': setUS(currentUS - 50);  printPos(); break;
    case 'c': printPos(); break;

    case 'C':
      calCenter = currentUS;
      Serial.print("\n  *** ЦЕНТР сохранён: ");
      Serial.print(calCenter); Serial.println(" мкс ***\n");
      break;

    case 'R':
      calRight = currentUS;
      Serial.print("\n  *** МАКС ПРАВЫЙ сохранён: ");
      Serial.print(calRight); Serial.println(" мкс ***\n");
      break;

    case 'L':
      calLeft = currentUS;
      Serial.print("\n  *** МАКС ЛЕВЫЙ сохранён: ");
      Serial.print(calLeft); Serial.println(" мкс ***\n");
      break;

    case 'p': case 'P':
      printSummary();
      break;

    default:
      Serial.print("  Неизвестная команда: ");
      Serial.println(cmd);
      Serial.println("  Набери 'h' для справки");
      break;
  }
}

void loop() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() > 0) {
        handleCommand(buf);
        buf = "";
      }
    } else {
      buf += c;
    }
  }
}
