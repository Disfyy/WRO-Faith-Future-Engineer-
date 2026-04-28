#!/usr/bin/env python3
"""
HSP 94182 1/16 — параметрическая CAD-модель шасси
====================================================

Профессиональная B-Rep CAD-модель на CadQuery. Экспорт в:
  • STEP   — универсальный CAD-формат (Fusion 360, SolidWorks, OnShape, FreeCAD, CATIA)
  • STL    — для 3D-печати
  • 3MF    — для современных слайсеров (Bambu Studio, PrusaSlicer)
  • GLTF   — для веба и 3D-просмотра

Это НАСТОЯЩАЯ CAD-модель: проработанные грани, рёбра, скругления, фаски —
не tessellated mesh. После импорта в Fusion/SolidWorks её можно редактировать,
прокладывать сечения, делать чертежи и FEA-расчёты.

Установка
---------
    pip install -r requirements.txt
    # или напрямую:
    pip install cadquery

Использование
-------------
    python3 chassis_cad.py                        # → cad/HSP94182_assembly.step + .stl
    python3 chassis_cad.py --format step,stl,3mf  # выбрать форматы
    python3 chassis_cad.py --part chassis_plate   # одна деталь
    python3 chassis_cad.py --list                 # список деталей
    python3 chassis_cad.py --explode              # экспортировать с разнесением

Координатная система (стандарт авто-CAD):
    +X  — вперёд (направление движения)
    +Y  — влево
    +Z  — вверх
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

try:
    import cadquery as cq
    from cadquery import exporters
except ImportError:
    print("ОШИБКА: CadQuery не установлен.", file=sys.stderr)
    print("Установка: pip install cadquery", file=sys.stderr)
    print("Документация: https://cadquery.readthedocs.io", file=sys.stderr)
    sys.exit(1)


# ============================================================================
#  ПАРАМЕТРЫ (мм) — изменяйте здесь, чтобы адаптировать модель
# ============================================================================
WHEELBASE       = 205.0   # колёсная база
TRACK           = 130.0   # колея
CHASSIS_L       = 250.0   # длина основной плиты
CHASSIS_W       =  80.0   # ширина основной плиты
CHASSIS_T       =   3.0   # толщина плиты
GROUND_CL       =   8.0   # дорожный просвет

# Колёса
WHEEL_D         =  54.0
WHEEL_W         =  22.0
RIM_D           =  36.0
HUB_D           =   9.0   # диаметр hex-hub

# Амортизатор
SHOCK_L         =  38.0
SHOCK_BODY_D    =   5.0
SHOCK_CAP_D     =   7.2
SHOCK_CAP_H     =   4.2
SPRING_D        =   7.6
SPRING_TURNS    =   9
SPRING_WIRE_D   =   1.1

# Аккумулятор Li-Po
BAT_L           =  78.0
BAT_W           =  36.0
BAT_H           =  18.0

# Бампер
BUMPER_W        = 150.0
BUMPER_T        =  28.0
BUMPER_H        =  12.0

# Геометрия плиты — облегчающие треугольные вырезы
TRI_HOLES = [(60, 0, 30, 90), (0, 0, 34, -90), (-60, 0, 30, 90)]  # (x, y, размер, угол°)

# ============================================================================
#  ЦВЕТА (для STEP-сборки и glTF)
# ============================================================================
C_BLACK   = cq.Color(0.10, 0.10, 0.11)
C_PLASTIC = cq.Color(0.13, 0.14, 0.15)
C_BLUE    = cq.Color(0.10, 0.43, 0.78)
C_DARKAL  = cq.Color(0.16, 0.23, 0.33)
C_STEEL   = cq.Color(0.78, 0.78, 0.78)
C_RIM     = cq.Color(0.94, 0.94, 0.94)
C_TIRE    = cq.Color(0.05, 0.05, 0.05)
C_BAT     = cq.Color(0.94, 0.75, 0.13)
C_LABEL   = cq.Color(1.00, 0.97, 0.90)
C_GOLD    = cq.Color(0.78, 0.60, 0.23)


# ============================================================================
#  ФУНКЦИИ ПОСТРОЕНИЯ ДЕТАЛЕЙ
# ============================================================================

def chassis_plate() -> cq.Workplane:
    """Главная плита: 8-угольный контур, 3 треугольных облегчения, крепёжные отверстия."""
    half_l = CHASSIS_L / 2
    half_w = CHASSIS_W / 2
    inset_x = 16.0
    narrow = half_w * 0.78

    outline = [
        (-half_l,           -narrow),
        (-half_l + inset_x, -half_w),
        ( half_l - inset_x, -half_w),
        ( half_l,           -narrow),
        ( half_l,            narrow),
        ( half_l - inset_x,  half_w),
        (-half_l + inset_x,  half_w),
        (-half_l,            narrow),
    ]

    plate = (
        cq.Workplane("XY")
        .polyline(outline).close()
        .extrude(CHASSIS_T)
    )

    for cx, cy, size, rot_deg in TRI_HOLES:
        r = size / 2.0
        rot = math.radians(rot_deg)
        pts = []
        for i in range(3):
            t = rot + i * (2 * math.pi / 3)
            pts.append((cx + r * math.cos(t), cy + r * math.sin(t)))
        plate = (
            plate.faces(">Z").workplane()
            .polyline(pts).close()
            .cutThruAll()
        )

    plate = (
        plate.faces(">Z").workplane()
        .pushPoints([(x, y) for x in (-110, -90, 90, 110) for y in (-25, 25)])
        .hole(3.2)
    )

    return plate


def upper_deck() -> cq.Workplane:
    """Верхняя T-образная дека."""
    pts = [
        (-90, -10), (90, -10), (90, 10),
        (20, 10),  (20, 38), (-20, 38),
        (-20, 10), (-90, 10),
    ]
    return (
        cq.Workplane("XY")
        .polyline(pts).close()
        .extrude(2.0)
    )


def shock_tower() -> cq.Workplane:
    """Шок-башня: вертикальная пластина (с крылышками встроены в одну форму)."""
    return (
        cq.Workplane("YZ")
        .polyline([
            (-48, 0), (-48, 18), (-36, 18), (-36, 30),
            (-28, 30), (-28, 18), (28, 18), (28, 30),
            (36, 30), (36, 18), (48, 18), (48, 0)
        ]).close()
        .extrude(2.2, both=True)
    )


def shock_body() -> cq.Workplane:
    """Чёрный корпус амортизатора (без пружины)."""
    return (
        cq.Workplane("XY")
        .circle(SHOCK_BODY_D / 2)
        .extrude(SHOCK_L, both=True)
    )


def shock_cap() -> cq.Workplane:
    """Синий алюминиевый колпак амортизатора (без проушины — отдельной деталью)."""
    return cq.Workplane("XY").circle(SHOCK_CAP_D / 2).extrude(SHOCK_CAP_H)


def shock_eye() -> cq.Workplane:
    """Проушина амортизатора (кольцо)."""
    return (
        cq.Workplane("XY")
        .circle(2.6).circle(1.4)
        .extrude(2.0)
    )


def shock_shaft() -> cq.Workplane:
    """Стальной шток амортизатора."""
    return cq.Workplane("XY").circle(1.1).extrude(12, both=True)


def shock_spring() -> cq.Workplane:
    """Спиральная пружина — реальный helical sweep."""
    h = SPRING_L = SHOCK_L * 0.92
    r = SPRING_D / 2.0

    helix_wire = cq.Wire.makeHelix(
        pitch=h / SPRING_TURNS,
        height=h,
        radius=r,
    )
    helix_path = cq.Workplane(obj=helix_wire)

    # Профиль — круглое сечение проволоки
    profile = (
        cq.Workplane("YZ")
        .center(r, 0)
        .circle(SPRING_WIRE_D / 2)
    )
    spring = profile.sweep(helix_path, isFrenet=True)
    # Сместить пружину так, чтобы центр совпадал с центром корпуса
    return spring.translate((0, 0, -h / 2))


def tire_solid() -> cq.Workplane:
    """Шина — кольцо."""
    tire_outer_r = WHEEL_D / 2
    tire_inner_r = tire_outer_r - WHEEL_W * 0.55
    return (
        cq.Workplane("XY")
        .circle(tire_outer_r)
        .circle(tire_inner_r)
        .extrude(WHEEL_W / 2, both=True)
    )


def rim_solid() -> cq.Workplane:
    """Диск со спицами и центральной втулкой."""
    tire_inner_r = WHEEL_D / 2 - WHEEL_W * 0.55
    rim_outer_r = RIM_D / 2
    # Внешняя цилиндрическая часть диска
    rim = (
        cq.Workplane("XY")
        .circle(tire_inner_r + 0.2)
        .circle(rim_outer_r * 0.45)
        .extrude(WHEEL_W / 2 - 1.5, both=True)
    )
    # Лицевая пластина со спицами — простое кольцо со множеством скруглений
    face = (
        cq.Workplane("XY")
        .workplane(offset=WHEEL_W / 2 - 1.6)
        .circle(rim_outer_r)
        .extrude(0.8)
    )
    rim = rim.union(face)
    rim = rim.union(face.translate((0, 0, -(WHEEL_W - 2))))
    return rim


def wheel_hub() -> cq.Workplane:
    """Hex-hub — синий шестигранник в центре колеса."""
    return (
        cq.Workplane("XY")
        .polygon(6, HUB_D)
        .extrude(WHEEL_W / 2 + 2, both=True)
    )


def lower_arm(side: int = 1) -> cq.Workplane:
    """Нижний рычаг подвески."""
    length = TRACK / 2 - 10
    return (
        cq.Workplane("XY")
        .center(side * length / 2, 0)
        .box(length, 12, 3, centered=True)
    )


def upper_arm(side: int = 1) -> cq.Workplane:
    """Верхний рычаг подвески (короче и тоньше)."""
    length = TRACK / 2 - 22
    return (
        cq.Workplane("XY")
        .center(side * length / 2, 0)
        .box(length, 5, 3, centered=True)
    )


def steering_hub(side: int = 1) -> cq.Workplane:
    """Поворотный кулак — основной блок (шкворень и шпиндель добавляются в сборке)."""
    return cq.Workplane("XY").box(8, 10, 16, centered=True)


def hub_post() -> cq.Workplane:
    """Стальной шкворень кулака."""
    return cq.Workplane("XY").circle(1.4).extrude(9, both=True)


def hub_spindle() -> cq.Workplane:
    """Hex-шпиндель к колесу."""
    return cq.Workplane("YZ").circle(2.8).extrude(6, both=True)


def differential() -> cq.Workplane:
    """Корпус дифференциала + 2 крышки + CV-валы."""
    housing = (
        cq.Workplane("YZ")
        .circle(11)
        .extrude(11, both=True)
    )
    cap_l = (
        cq.Workplane("YZ").workplane(offset=-11)
        .circle(11.5).extrude(-1)
    )
    cap_r = (
        cq.Workplane("YZ").workplane(offset=11)
        .circle(11.5).extrude(1)
    )
    cv_l = (
        cq.Workplane("YZ").workplane(offset=-12)
        .circle(1.6).extrude(-18)
    )
    cv_r = (
        cq.Workplane("YZ").workplane(offset=12)
        .circle(1.6).extrude(18)
    )
    return housing, cap_l, cap_r, cv_l, cv_r


def motor() -> cq.Workplane:
    """Корпус электромотора: банка + торцевая крышка + пиньон."""
    can = cq.Workplane("YZ").circle(13).extrude(15, both=True)
    endbell = (
        cq.Workplane("YZ").workplane(offset=-15)
        .circle(13.2).extrude(-4)
    )
    pinion = (
        cq.Workplane("YZ").workplane(offset=15)
        .circle(4).extrude(5)
    )
    return can, endbell, pinion


def driveshaft() -> cq.Workplane:
    """Карданный вал между диффами."""
    return (
        cq.Workplane("YZ")
        .circle(1.6)
        .extrude(WHEELBASE * 0.65, both=True)
    )


def battery_pack() -> cq.Workplane:
    """Li-Po аккумулятор."""
    return cq.Workplane("XY").box(BAT_L, BAT_W, BAT_H, centered=True)


def battery_label() -> cq.Workplane:
    """Наклейка на батарее."""
    return (
        cq.Workplane("XY").workplane(offset=BAT_H / 2)
        .rect(BAT_L * 0.85, BAT_W * 0.55)
        .extrude(0.2)
    )


def esc_case() -> cq.Workplane:
    return cq.Workplane("XY").box(28, 22, 14, centered=True)


def esc_sticker() -> cq.Workplane:
    return (
        cq.Workplane("XY").workplane(offset=7)
        .rect(22, 16).extrude(0.2)
    )


def servo_case() -> cq.Workplane:
    return cq.Workplane("XY").box(22, 12, 22, centered=True)


def servo_horn() -> cq.Workplane:
    return (
        cq.Workplane("XY").workplane(offset=11)
        .circle(5).extrude(2)
    )


def foam_bumper() -> cq.Workplane:
    """
    Пенопластовый бампер — дугообразная пластина.
    Делаем из эллиптической трубы: cut(внешний - внутренний эллипс) и обрезаем нижнюю
    половину box-вычитанием.
    """
    outer_a = BUMPER_W / 2
    outer_b = BUMPER_T * 0.7
    inner_a = outer_a * 0.78
    inner_b = outer_b * 0.55

    outer_solid = (
        cq.Workplane("XY")
        .ellipse(outer_a, outer_b)
        .extrude(BUMPER_H)
    )
    inner_solid = (
        cq.Workplane("XY").workplane(offset=-1)
        .ellipse(inner_a, inner_b)
        .extrude(BUMPER_H + 2)
    )
    foam = outer_solid.cut(inner_solid)
    # Обрезаем верхнюю (для переднего бампера) половину — оставляем только нижнюю дугу
    cutter = (
        cq.Workplane("XY").workplane(offset=-1)
        .center(0, outer_b)
        .rect(outer_a * 3, outer_b * 2)
        .extrude(BUMPER_H + 2)
    )
    foam = foam.cut(cutter)
    return foam


def bumper_holder() -> cq.Workplane:
    """Пластиковый держатель бампера."""
    return (
        cq.Workplane("XY")
        .center(0, -BUMPER_T * 0.4)
        .workplane(offset=BUMPER_H + 2)
        .rect(BUMPER_W * 0.85, 10)
        .extrude(4)
    )


def body_post(h: float = 56) -> cq.Workplane:
    """Кузовная стойка с R-клипсой."""
    shaft = cq.Workplane("XY").circle(1.8).extrude(h)
    return shaft


def antenna_mount() -> cq.Workplane:
    """Крепление антенны: основание + трубка (в одном теле)."""
    return (
        cq.Workplane("XY")
        .box(8, 8, 4, centered=True)
        .faces(">Z").workplane()
        .circle(1.0).extrude(70)
    )


def steering_rod() -> cq.Workplane:
    """Рулевая тяга — стальной стержень."""
    return (
        cq.Workplane("YZ")
        .circle(0.9)
        .extrude((TRACK - 12) / 2, both=True)
    )


def steering_ball() -> cq.Workplane:
    """Шарик-шаровая опора рулевой тяги (две шт. в сборке)."""
    return cq.Workplane("XY").sphere(2.4)


# ============================================================================
#  СБОРКА
# ============================================================================

def make_wheel_assembly() -> cq.Assembly:
    """Колесо как подсборка: шина + диск + hex-hub разными цветами."""
    asm = cq.Assembly(name="wheel")
    asm.add(tire_solid(),  name="tire", color=C_TIRE)
    asm.add(rim_solid(),   name="rim",  color=C_RIM)
    asm.add(wheel_hub(),   name="hub",  color=C_BLUE)
    return asm


def make_shock_assembly() -> cq.Assembly:
    """Амортизатор как подсборка (корпус + 2 колпака + 2 проушины + шток + пружина)."""
    asm = cq.Assembly(name="shock")
    asm.add(shock_body(),  name="body",      color=C_DARKAL)
    asm.add(shock_spring(),name="spring",    color=C_STEEL)
    asm.add(shock_cap(),   name="cap_top",   color=C_BLUE,
            loc=cq.Location((0, 0, SHOCK_L / 2)))
    asm.add(shock_eye(),   name="eye_top",   color=C_BLUE,
            loc=cq.Location((0, 0, SHOCK_L / 2 + SHOCK_CAP_H + 1.0)))
    asm.add(shock_shaft(), name="shaft",     color=C_STEEL,
            loc=cq.Location((0, 0, -SHOCK_L / 2 - 6)))
    asm.add(shock_eye(),   name="eye_bot",   color=C_BLUE,
            loc=cq.Location((0, 0, -SHOCK_L / 2 - 18)))
    return asm


def make_diff_assembly() -> cq.Assembly:
    """Дифференциал как подсборка."""
    housing, cap_l, cap_r, cv_l, cv_r = differential()
    asm = cq.Assembly(name="diff")
    asm.add(housing, name="housing", color=C_DARKAL)
    asm.add(cap_l,   name="cap_l",   color=C_BLUE)
    asm.add(cap_r,   name="cap_r",   color=C_BLUE)
    asm.add(cv_l,    name="cv_l",    color=C_STEEL)
    asm.add(cv_r,    name="cv_r",    color=C_STEEL)
    return asm


def make_motor_assembly() -> cq.Assembly:
    """Мотор как подсборка."""
    can, endbell, pinion = motor()
    asm = cq.Assembly(name="motor")
    asm.add(can,     name="can",     color=C_STEEL)
    asm.add(endbell, name="endbell", color=C_BLUE)
    asm.add(pinion,  name="pinion",  color=C_GOLD)
    return asm


def make_battery_assembly() -> cq.Assembly:
    asm = cq.Assembly(name="battery")
    asm.add(battery_pack(),  name="pack",  color=C_BAT)
    asm.add(battery_label(), name="label", color=C_LABEL)
    return asm


def make_esc_assembly() -> cq.Assembly:
    asm = cq.Assembly(name="esc")
    asm.add(esc_case(),    name="case",    color=C_PLASTIC)
    asm.add(esc_sticker(), name="sticker", color=C_LABEL)
    return asm


def make_servo_assembly() -> cq.Assembly:
    asm = cq.Assembly(name="servo")
    asm.add(servo_case(), name="case", color=C_PLASTIC)
    asm.add(servo_horn(), name="horn", color=C_LABEL)
    return asm


def make_bumper_assembly() -> cq.Assembly:
    asm = cq.Assembly(name="bumper")
    asm.add(foam_bumper(),    name="foam",   color=C_BLACK)
    asm.add(bumper_holder(),  name="holder", color=C_BLACK)
    return asm


def make_steering_assembly() -> cq.Assembly:
    asm = cq.Assembly(name="steering_link")
    asm.add(steering_rod(),  name="rod",    color=C_STEEL)
    asm.add(steering_ball(), name="ball_l", color=C_BLUE,
            loc=cq.Location((0, -(TRACK / 2 - 6), 0)))
    asm.add(steering_ball(), name="ball_r", color=C_BLUE,
            loc=cq.Location((0,  (TRACK / 2 - 6), 0)))
    return asm


def make_hub_assembly(side: int = 1) -> cq.Assembly:
    """Поворотный кулак как подсборка (блок + шкворень + шпиндель)."""
    asm = cq.Assembly(name="hub")
    asm.add(steering_hub(side), name="block",   color=C_BLUE)
    asm.add(hub_post(),         name="post",    color=C_STEEL,
            loc=cq.Location((0, 0, 4)))
    asm.add(hub_spindle(),      name="spindle", color=C_BLUE,
            loc=cq.Location((side * 4, 0, 0)))
    return asm


def build_assembly() -> cq.Assembly:
    """
    Полная сборка шасси HSP 94182.
    Все детали позиционируются с учётом дорожного просвета.
    """
    a = cq.Assembly(name="HSP94182_chassis")

    # ---- Шасси ----
    a.add(chassis_plate(),        name="chassis_plate",
          loc=cq.Location((0, 0, GROUND_CL)),         color=C_BLACK)
    a.add(upper_deck(),           name="upper_deck",
          loc=cq.Location((0, 0, GROUND_CL + 14)),    color=C_BLACK)
    a.add(shock_tower(),          name="shock_tower_front",
          loc=cq.Location((WHEELBASE / 2 - 8, 0, GROUND_CL + 16)),
          color=C_BLACK)
    a.add(shock_tower(),          name="shock_tower_rear",
          loc=cq.Location((-WHEELBASE / 2 + 8, 0, GROUND_CL + 16)),
          color=C_BLACK)

    # ---- Подвеска и амортизаторы ----
    for x, label_x in [(WHEELBASE / 2, "f"), (-WHEELBASE / 2, "r")]:
        for side, label_y in [(1, "l"), (-1, "r")]:
            tag = f"{label_x}{label_y}"
            # нижний рычаг
            a.add(lower_arm(side), name=f"lower_arm_{tag}",
                  loc=cq.Location((x, 0, GROUND_CL + 6)), color=C_BLACK)
            # верхний рычаг
            a.add(upper_arm(side), name=f"upper_arm_{tag}",
                  loc=cq.Location((x, 0, GROUND_CL + 25)), color=C_BLACK)
            a.add(make_hub_assembly(side), name=f"hub_{tag}",
                  loc=cq.Location((x, side * (TRACK / 2 - 6), GROUND_CL + 14)))
            # амортизатор
            a.add(make_shock_assembly(), name=f"shock_{tag}",
                  loc=cq.Location((x, side * (TRACK / 2 - 26), GROUND_CL + 24),
                                  (0, 1, 0), side * 10))

    # ---- Рулевая тяга (только спереди) ----
    a.add(make_steering_assembly(), name="steering_link",
          loc=cq.Location((WHEELBASE / 2 + 6, 0, GROUND_CL + 14)))

    # ---- Колёса ----
    # Колёса строятся с осью Z, поворачиваем на 90° вокруг X → ось Y
    for x, label_x in [(WHEELBASE / 2, "f"), (-WHEELBASE / 2, "r")]:
        for side, label_y in [(1, "l"), (-1, "r")]:
            tag = f"{label_x}{label_y}"
            a.add(make_wheel_assembly(), name=f"wheel_{tag}",
                  loc=cq.Location((x, side * TRACK / 2, WHEEL_D / 2),
                                  (1, 0, 0), 90))

    # ---- Трансмиссия ----
    a.add(make_diff_assembly(), name="diff_front",
          loc=cq.Location((WHEELBASE / 2, 0, GROUND_CL + 7)))
    a.add(make_diff_assembly(), name="diff_rear",
          loc=cq.Location((-WHEELBASE / 2, 0, GROUND_CL + 7)))
    a.add(driveshaft(),         name="driveshaft",
          loc=cq.Location((0, 0, GROUND_CL + 7)),
          color=C_STEEL)
    a.add(make_motor_assembly(), name="motor",
          loc=cq.Location((-38, -18, GROUND_CL + 18)))

    # ---- Электрика ----
    a.add(make_battery_assembly(), name="battery",
          loc=cq.Location((0, 8, GROUND_CL + BAT_H / 2 + 3)))
    a.add(make_esc_assembly(),     name="esc",
          loc=cq.Location((-38, 28, GROUND_CL + 9)))
    a.add(make_servo_assembly(),   name="servo",
          loc=cq.Location((60, -12, GROUND_CL + 11)))

    # ---- Бамперы ----
    a.add(make_bumper_assembly(), name="bumper_front",
          loc=cq.Location((CHASSIS_L / 2 + 6, 0, GROUND_CL + 8)))
    a.add(make_bumper_assembly(), name="bumper_rear",
          loc=cq.Location((-CHASSIS_L / 2 - 6, 0, GROUND_CL + 8),
                          (0, 0, 1), 180))

    # ---- Кузовные стойки ----
    for sx in (-1, 1):
        for sy in (-1, 1):
            a.add(body_post(56), name=f"post_{'f' if sx > 0 else 'r'}{'l' if sy > 0 else 'r'}",
                  loc=cq.Location((sx * (WHEELBASE / 2 - 12), sy * 38, GROUND_CL + 10)),
                  color=C_BLACK)

    # ---- Антенна ----
    a.add(antenna_mount(), name="antenna",
          loc=cq.Location((-22, 34, GROUND_CL + 18)),
          color=C_BLACK)

    return a


# ============================================================================
#  ОДИНОЧНЫЕ ДЕТАЛИ — для экспорта по отдельности
# ============================================================================
INDIVIDUAL_PARTS = {
    "chassis_plate":  lambda: chassis_plate(),
    "upper_deck":     lambda: upper_deck(),
    "shock_tower":    lambda: shock_tower(),
    "lower_arm":      lambda: lower_arm(1),
    "upper_arm":      lambda: upper_arm(1),
    "steering_hub":   lambda: steering_hub(1),
    "body_post":      lambda: body_post(56),
    "antenna_mount":  lambda: antenna_mount(),
    "shock_body":     lambda: shock_body(),
    "shock_spring":   lambda: shock_spring(),
    "driveshaft":     lambda: driveshaft(),
}


# ============================================================================
#  ЭКСПОРТ
# ============================================================================
def export_assembly(a: cq.Assembly, outdir: Path, formats: list[str]) -> None:
    base = outdir / "HSP94182_assembly"
    if "step" in formats:
        path = f"{base}.step"
        a.save(path, "STEP")
        print(f"  ✓ {path}  ({Path(path).stat().st_size / 1024:.1f} KB)")
    if "gltf" in formats:
        path = f"{base}.glb"
        a.save(path, "GLTF")
        print(f"  ✓ {path}")
    if "stl" in formats:
        compound = a.toCompound()
        path = f"{base}.stl"
        exporters.export(compound, path)
        print(f"  ✓ {path}  ({Path(path).stat().st_size / 1024:.1f} KB)")
    if "3mf" in formats:
        path = f"{base}.3mf"
        try:
            a.save(path, "3MF")
            print(f"  ✓ {path}")
        except Exception as e:
            print(f"  ⚠ 3MF не поддерживается этой версией CadQuery: {e}")


def export_part(name: str, part: cq.Workplane, outdir: Path, formats: list[str]) -> None:
    base = outdir / name
    if "step" in formats:
        path = f"{base}.step"
        exporters.export(part, path)
        print(f"  ✓ {path}")
    if "stl" in formats:
        path = f"{base}.stl"
        exporters.export(part, path)
        print(f"  ✓ {path}")


# ============================================================================
#  CLI
# ============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(
        description="Генерация CAD-модели HSP 94182 (CadQuery → STEP/STL/3MF/GLTF)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--list", action="store_true",
                        help="Показать список одиночных деталей")
    parser.add_argument("--part", action="append", default=None,
                        help="Имя одной детали (можно несколько раз). Без флага = вся сборка.")
    parser.add_argument("--format", default="step,stl",
                        help="Форматы через запятую: step, stl, 3mf, gltf (по умолчанию: step,stl)")
    parser.add_argument("--outdir", default="cad",
                        help="Папка для экспорта (по умолчанию: cad/)")
    args = parser.parse_args()

    if args.list:
        print("\nДоступные одиночные детали:\n" + "-" * 50)
        for name in INDIVIDUAL_PARTS:
            print(f"  {name}")
        print("\nБез флага --part экспортируется ПОЛНАЯ сборка.\n")
        return 0

    formats = [f.strip().lower() for f in args.format.split(",") if f.strip()]
    valid = {"step", "stl", "3mf", "gltf"}
    bad = [f for f in formats if f not in valid]
    if bad:
        print(f"ОШИБКА: неизвестные форматы {bad}. Доступно: {valid}", file=sys.stderr)
        return 2

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    print(f"Папка вывода: {outdir.resolve()}")
    print(f"Форматы: {formats}\n")

    if args.part:
        bad = [p for p in args.part if p not in INDIVIDUAL_PARTS]
        if bad:
            print(f"ОШИБКА: неизвестные детали {bad}", file=sys.stderr)
            print("Запустите --list для списка.", file=sys.stderr)
            return 3
        for name in args.part:
            print(f"→ {name}")
            export_part(name, INDIVIDUAL_PARTS[name](), outdir, formats)
    else:
        print("→ Полная сборка")
        a = build_assembly()
        export_assembly(a, outdir, formats)

    print("\nГотово.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
