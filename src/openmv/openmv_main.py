import sensor, image, time, math
from pyb import UART, LED

# ==========================================
# WRO Future Engineers — OpenMV Vision Code
# ==========================================

# Настройка сенсора
sensor.reset()
sensor.set_pixformat(sensor.RGB565) # Цветной формат
sensor.set_framesize(sensor.QQVGA)  # 160x120 - идеальный баланс скорости и качества
sensor.skip_frames(time = 2000)
sensor.set_auto_gain(False)         # ВЫКЛЮЧИТЬ! Иначе цвета "поплывут" от освещения
sensor.set_auto_whitebal(False)     # ВЫКЛЮЧИТЬ!

# Настройка UART (Baudrate должно совпадать с ESP32: 115200)
# На OpenMV H7 Plus: TX = P4, RX = P5
uart = UART(3, 115200, timeout_char=1000)

led = LED(1) # Красный светодиод для индикации

# ==========================================
# ПОРОГИ ЦВЕТОВ (НАСТРАИВАТЬ НА ТРЕКЕ!!!)
# ==========================================
# Формат: (L Min, L Max, A Min, A Max, B Min, B Max)
THRESHOLD_ORANGE = (50, 80,  20,  60,  20,  70)  # Type 1 (Направление)
THRESHOLD_BLUE   = (20, 50, -20,  20, -60, -20)  # Type 2 (Направление)
THRESHOLD_RED    = (30, 60,  30,  80,  10,  60)  # Type 3 (Препятствие красное)
THRESHOLD_GREEN  = (30, 70, -60, -20,  10,  50)  # Type 4 (Препятствие зеленое)
THRESHOLD_WALL   = ( 0, 20, -10,  10, -10,  10)  # Черный цвет бортов для слепых поворотов

CENTER_X = 80 # Середина экрана для QQVGA (160 / 2)

clock = time.clock()

def get_distance_cm(blob):
    # Фокусное расстояние / ширину пикселей.
    # КОНСТАНТУ 2000 НУЖНО ПОДОБРАТЬ! Поставьте блок на 20см и поменяйте константу, чтобы выдавало 20.
    if blob.w() == 0: return 999
    return int(2000 / blob.w()) 

while(True):
    clock.tick()
    img = sensor.snapshot()
    
    # Дефолтные значения для отправки в ESP
    error_x = 0
    distance = 999
    obj_type = 0 
    
    best_blob = None
    max_pixels = 0
    
    # 1. Поиск важных цветных объектов (Препятствия и метки направления)
    # Ищем объединяя 4 цвета. pixels_threshold отсекает мелкий шум.
    blobs = img.find_blobs([THRESHOLD_ORANGE, THRESHOLD_BLUE, THRESHOLD_RED, THRESHOLD_GREEN], 
                             pixels_threshold=100, area_threshold=100, merge=True)
                             
    for blob in blobs:
        if blob.pixels() > max_pixels:
            best_blob = blob
            max_pixels = blob.pixels()
            
    if best_blob:
        # Если нашли цветной объект, определяем его тип по битовой маске (code)
        if best_blob.code() == 1:   obj_type = 1 # Orange
        elif best_blob.code() == 2: obj_type = 2 # Blue
        elif best_blob.code() == 4: obj_type = 3 # Red
        elif best_blob.code() == 8: obj_type = 4 # Green
        
        # Расчет ошибки для рулежки ESP32
        error_x = best_blob.cx() - CENTER_X
        # Расчет дистанции
        distance = get_distance_cm(best_blob)
        
        # Отрисовка на экране IDE для удобной отладки
        img.draw_rectangle(best_blob.rect(), color=(255, 255, 255))
        img.draw_cross(best_blob.cx(), best_blob.cy())
        img.draw_string(best_blob.x(), best_blob.y() - 10, "T:%d D:%d" % (obj_type, distance), color=(255,255,255))

    else:
        # 2. Если препятствий нет, ищем черные стены (для функции Blind Turn)
        # Ищем черную стену в нижней половине кадра
        wall_blobs = img.find_blobs([THRESHOLD_WALL], roi=(0, 60, 160, 60), pixels_threshold=200, merge=True)
        if wall_blobs:
            # Берем самую низкую стену (ближайшую к нам по Y)
            closest_wall = max(wall_blobs, key=lambda b: b.y())
            distance = get_distance_cm(closest_wall)
            obj_type = 0 # Черная стена не является динамическим препятствием
            error_x = 0
            
            img.draw_rectangle(closest_wall.rect(), color=(255, 0, 0))
            img.draw_string(closest_wall.x(), closest_wall.y() - 10, "WALL D:%d" % distance, color=(255,0,0))
    
    # 3. Отправка данных на ESP32!
    uart_string = "%d,%d,%d\n" % (error_x, distance, obj_type)
    uart.write(uart_string)
    
    # Отладка в консоль (закомментировать на самих соревнованиях для ускорения FPS)
    # print("FPS: %f | TX: %s" % (clock.fps(), uart_string.strip()))
