// ============================================================================
//  HSP 94182 1/16 — параметрическая 3D-модель шасси
//  WRO Future Engineers • открытый формат, любая команда может повторить
//  Откройте файл в OpenSCAD (https://openscad.org), нажмите F5 для предпросмотра,
//  F6 — для рендера, и File → Export → STL/OBJ/3MF.
// ============================================================================

/* [Главные размеры (мм)] */
WHEELBASE   = 205;   // расстояние между осями
TRACK       = 130;   // колея (между центрами колёс)
CHASSIS_L   = 250;   // длина основной плиты
CHASSIS_W   = 80;    // ширина основной плиты
CHASSIS_T   = 3;     // толщина плиты
GROUND_CL   = 8;     // дорожный просвет

/* [Колёса] */
WHEEL_D     = 54;    // внешний диаметр шины
WHEEL_W     = 22;    // ширина шины
RIM_D       = 36;    // диаметр диска

/* [Амортизатор] */
SHOCK_L     = 38;
SHOCK_BODY_D= 5;
SPRING_D    = 7.6;
SPRING_TURNS= 9;

/* [Аккумулятор] */
BAT_L       = 78;
BAT_W       = 36;
BAT_H       = 18;

/* [Бампер] */
BUMPER_W    = 150;
BUMPER_T    = 28;
BUMPER_H    = 12;

/* [Качество (segments)] */
$fn         = 64;

// ============================================================================
//  ЦВЕТА
// ============================================================================
COL_BLACK   = [0.10, 0.10, 0.11];
COL_PLASTIC = [0.13, 0.14, 0.15];
COL_BLUE    = [0.10, 0.43, 0.78];
COL_DARKAL  = [0.16, 0.23, 0.33];
COL_STEEL   = [0.78, 0.78, 0.78];
COL_RIM     = [0.94, 0.94, 0.94];
COL_TIRE    = [0.05, 0.05, 0.05];
COL_BAT     = [0.94, 0.75, 0.13];
COL_LABEL   = [1.00, 0.97, 0.90];
COL_GOLD    = [0.78, 0.60, 0.23];

// ============================================================================
//  УТИЛИТЫ
// ============================================================================
module rounded_box(L, W, H, r=1.5) {
    minkowski() {
        cube([L - 2*r, W - 2*r, max(H - 2*r, 0.01)], center=true);
        sphere(r=r);
    }
}

module hex_prism(d, h) {
    cylinder(d=d, h=h, $fn=6);
}

// ============================================================================
//  ШАССИ — нижняя плита с треугольными вырезами облегчения
// ============================================================================
module chassis_plate() {
    color(COL_BLACK)
    linear_extrude(height=CHASSIS_T, convexity=8)
    difference() {
        // Контур плиты — со скошенными углами
        polygon(points=[
            [-CHASSIS_L/2,         -CHASSIS_W/2 * 0.78],
            [-CHASSIS_L/2 + 16,    -CHASSIS_W/2],
            [ CHASSIS_L/2 - 16,    -CHASSIS_W/2],
            [ CHASSIS_L/2,         -CHASSIS_W/2 * 0.78],
            [ CHASSIS_L/2,          CHASSIS_W/2 * 0.78],
            [ CHASSIS_L/2 - 16,     CHASSIS_W/2],
            [-CHASSIS_L/2 + 16,     CHASSIS_W/2],
            [-CHASSIS_L/2,          CHASSIS_W/2 * 0.78]
        ]);
        // Три облегчающих треугольника
        translate([ 60, 0]) rotate([0,0,90])  triangle_hole(30);
        translate([  0, 0]) rotate([0,0,-90]) triangle_hole(34);
        translate([-60, 0]) rotate([0,0,90])  triangle_hole(30);
        // Крепёжные отверстия
        for (x = [-110, -90, 90, 110])
            for (y = [-25, 25])
                translate([x, y]) circle(r=1.6);
    }
}

module triangle_hole(size) {
    polygon(points=[
        [size/2 * cos(0),   size/2 * sin(0)],
        [size/2 * cos(120), size/2 * sin(120)],
        [size/2 * cos(240), size/2 * sin(240)]
    ]);
}

module side_rail(side=1) {
    color(COL_BLACK)
    translate([0, side * (CHASSIS_W/2 + 1), CHASSIS_T + 4])
        cube([180, 4, 6], center=true);
}

module upper_deck() {
    color(COL_BLACK)
    translate([0, 0, CHASSIS_T + 14])
    linear_extrude(height=2)
    union() {
        translate([0, 0]) square([180, 20], center=true);
        translate([0, 24]) square([40, 28], center=true);
    }
}

// ============================================================================
//  АМОРТИЗАТОР: алюминиевый колпак, корпус, шток, пружина
// ============================================================================
module shock() {
    // Корпус
    color(COL_DARKAL) cylinder(d=SHOCK_BODY_D, h=SHOCK_L, center=true);
    // Колпак
    color(COL_BLUE) translate([0,0,SHOCK_L/2 + 1.2])
        cylinder(d=7.2, h=4.2, center=true);
    // Верхняя проушина
    color(COL_BLUE) translate([0,0,SHOCK_L/2 + 4.6])
        rotate([90,0,0]) ring(r=2.2, t=0.8);
    // Шток
    color(COL_STEEL) translate([0,0,-SHOCK_L/2 - 6])
        cylinder(d=2.2, h=12, center=true);
    // Нижняя проушина
    color(COL_BLUE) translate([0,0,-SHOCK_L/2 - 12])
        rotate([90,0,0]) ring(r=2.2, t=0.8);
    // Пружина (приближаем серией витков)
    color(COL_STEEL) spring(d=SPRING_D, h=SHOCK_L*0.9, turns=SPRING_TURNS, wire=1.1);
}

module ring(r, t) {
    rotate_extrude() translate([r, 0]) circle(r=t);
}

module spring(d=8, h=30, turns=10, wire=1.0, segs=200) {
    step_h = h / segs;
    step_a = (turns * 360) / segs;
    for (i = [0:segs-1]) {
        a = i * step_a;
        z = -h/2 + i * step_h;
        translate([cos(a) * d/2, sin(a) * d/2, z])
            sphere(r=wire);
    }
}

// ============================================================================
//  КОЛЕСО: шина (тор), боковины, диск со спицами, hex-hub
// ============================================================================
module wheel() {
    // Шина — тор
    color(COL_TIRE)
        rotate([0, 90, 0])
        rotate_extrude(convexity=10)
        translate([(WHEEL_D - WHEEL_W*0.55)/2, 0])
        circle(d=WHEEL_W*0.55);

    // Цилиндрическая боковина для замыкания
    color(COL_TIRE)
        rotate([0, 90, 0])
        cylinder(d=WHEEL_D - 2, h=WHEEL_W, center=true);

    // Диск (рим)
    color(COL_RIM) {
        rotate([0, 90, 0])
            cylinder(d=RIM_D, h=WHEEL_W - 4, center=true);
        // Спицы
        for (a = [0:36:359])
            rotate([a, 90, 0])
                translate([0, 0, 0])
                cube([1.0, 3, RIM_D - 6], center=true);
    }

    // Hex-hub
    color(COL_BLUE) {
        for (s = [-1, 1])
            translate([s * (WHEEL_W/2 + 1), 0, 0])
                rotate([0, 90, 0])
                hex_prism(d=9, h=2);
    }
}

// ============================================================================
//  ПОДВЕСКА: рычаги, кулаки, шок-башни
// ============================================================================
module suspension_arm(length=28, w=12, side=1) {
    color(COL_BLACK)
        translate([side * length/2, 0, 0])
        cube([length, w, 3], center=true);
    color(COL_BLUE)
        translate([side * length, 0, 0])
        rotate([0, 90, 0])
        cylinder(d=4.4, h=6, center=true);
}

module steering_hub(side=1) {
    color(COL_BLUE)
        cube([8, 10, 16], center=true);
    color(COL_STEEL)
        translate([0, 0, 4])
        cylinder(d=2.8, h=18, center=true);
    color(COL_BLUE)
        translate([side * 8, 0, 0])
        rotate([0, 90, 0])
        cylinder(d=5.6, h=12, center=true);
}

module shock_tower(z) {
    color(COL_BLACK)
    translate([0, z, CHASSIS_T + 16]) {
        cube([2.2, 96, 22], center=true);
        for (sx = [-1, 1])
            translate([0.5, sx * 32, 3])
                cube([2.2, 8, 28], center=true);
    }
}

// ============================================================================
//  ТРАНСМИССИЯ
// ============================================================================
module differential(z) {
    translate([0, z, CHASSIS_T + 7]) {
        color(COL_DARKAL)
            rotate([0, 90, 0])
            cylinder(d=22, h=22, center=true);
        color(COL_BLUE)
            for (s = [-1, 1])
                translate([s * 11, 0, 0])
                rotate([0, 90, 0])
                cylinder(d=23, h=2, center=true);
        color(COL_STEEL)
            for (s = [-1, 1])
                translate([s * 30, 0, 0])
                rotate([0, 90, 0])
                cylinder(d=3.2, h=36, center=true);
    }
}

module motor() {
    translate([-18, -38, CHASSIS_T + 18]) {
        color(COL_STEEL) rotate([90,0,0]) cylinder(d=26, h=30, center=true);
        color(COL_BLUE)  translate([0,16,0]) rotate([90,0,0]) cylinder(d=26.4, h=4, center=true);
        color(COL_GOLD)  translate([0,-17.5,0]) rotate([90,0,0]) cylinder(d=8, h=5, center=true);
    }
}

module driveshaft() {
    color(COL_STEEL)
        translate([0, 0, CHASSIS_T + 7])
        rotate([90, 0, 0])
        cylinder(d=3.2, h=130, center=true);
}

// ============================================================================
//  ЭЛЕКТРИКА: аккумулятор, ESC, серво
// ============================================================================
module battery() {
    translate([8, 0, CHASSIS_T + BAT_H/2 + 3]) {
        color(COL_BAT) cube([BAT_L, BAT_W, BAT_H], center=true);
        color(COL_LABEL)
            translate([0, 0, BAT_H/2 + 0.15])
            cube([BAT_L*0.85, BAT_W*0.55, 0.2], center=true);
        color(COL_BLACK)
            for (sx = [-1, 1])
                translate([sx * (BAT_L/2 - 6), BAT_W/2 + 14, 0])
                rotate([90, 0, 0])
                cylinder(d=2.4, h=28, center=true);
    }
}

module esc() {
    color(COL_PLASTIC)
        translate([28, -38, CHASSIS_T + 9])
        cube([28, 22, 14], center=true);
}

module servo() {
    color(COL_PLASTIC)
        translate([-12, 60, CHASSIS_T + 11])
        cube([22, 12, 22], center=true);
}

// ============================================================================
//  БАМПЕРЫ И КУЗОВНЫЕ СТОЙКИ
// ============================================================================
module foam_bumper() {
    color(COL_BLACK) {
        // Дугообразная форма — выдавленная полудуга
        difference() {
            scale([BUMPER_W/2, BUMPER_T*0.7, 1])
                cylinder(d=2, h=BUMPER_H);
            translate([0, BUMPER_T*0.7, -1])
                cube([BUMPER_W*1.2, BUMPER_T*1.4, BUMPER_H + 2], center=true);
            scale([BUMPER_W/2 * 0.75, BUMPER_T*0.32, 1])
                cylinder(d=2, h=BUMPER_H + 1);
        }
        // Держатель сверху
        translate([0, 0, BUMPER_H/2 + 4])
            cube([BUMPER_W * 0.85, 10, 4], center=true);
    }
}

module body_post(h=56) {
    color(COL_BLACK)
        translate([0, 0, h/2])
        cylinder(d=3.6, h=h, center=true);
    color(COL_STEEL)
        translate([0, 0, h - 4])
        rotate([90, 0, 0])
        ring(r=2.6, t=0.5);
}

module antenna_mount() {
    color(COL_BLACK) {
        cube([8, 8, 4], center=true);
        translate([0, 0, 35]) cylinder(d=2.0, h=70, center=true);
    }
}

module steering_link() {
    color(COL_STEEL)
        rotate([0, 90, 0])
        cylinder(d=1.8, h=TRACK - 12, center=true);
    color(COL_BLUE)
        for (sx = [-1, 1])
            translate([sx * (TRACK/2 - 6), 0, 0])
            sphere(d=4.8);
}

// ============================================================================
//  СБОРКА
// ============================================================================
module assemble() {
    // ----- Шасси -----
    translate([0, 0, GROUND_CL]) chassis_plate();
    translate([0, 0, GROUND_CL]) side_rail( 1);
    translate([0, 0, GROUND_CL]) side_rail(-1);
    translate([0, 0, GROUND_CL]) upper_deck();

    // ----- Шок-башни -----
    translate([0, 0, GROUND_CL]) shock_tower( WHEELBASE/2 - 8);
    translate([0, 0, GROUND_CL]) shock_tower(-WHEELBASE/2 + 8);

    // ----- Подвеска и амортизаторы -----
    for (zy = [WHEELBASE/2, -WHEELBASE/2])
        for (sx = [-1, 1]) {
            translate([0, zy, GROUND_CL + 6])
                suspension_arm(length=TRACK/2 - 10, w=12, side=sx);
            translate([0, zy, GROUND_CL + 25])
                suspension_arm(length=TRACK/2 - 22, w=5, side=sx);
            translate([sx * (TRACK/2 - 6), zy, GROUND_CL + 14])
                steering_hub(side=sx);
            translate([sx * (TRACK/2 - 26), zy, GROUND_CL + 24])
                rotate([0, sx * -10, 0]) shock();
        }

    // ----- Рулевая тяга -----
    translate([0, WHEELBASE/2 + 6, GROUND_CL + 14]) steering_link();

    // ----- Колёса -----
    for (zy = [WHEELBASE/2, -WHEELBASE/2])
        for (sx = [-1, 1])
            translate([sx * TRACK/2, zy, WHEEL_D/2]) wheel();

    // ----- Трансмиссия -----
    differential( WHEELBASE/2);
    differential(-WHEELBASE/2);
    translate([0, 0, GROUND_CL]) driveshaft();
    translate([0, 0, GROUND_CL]) motor();

    // ----- Электрика -----
    translate([0, 0, GROUND_CL]) battery();
    translate([0, 0, GROUND_CL]) esc();
    translate([0, 0, GROUND_CL]) servo();

    // ----- Бамперы -----
    translate([0, CHASSIS_L/2 + 6, GROUND_CL + 8]) rotate([0,0,180]) foam_bumper();
    translate([0,-CHASSIS_L/2 - 6, GROUND_CL + 8])                  foam_bumper();

    // ----- Кузовные стойки -----
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([sx * 38, sy * (WHEELBASE/2 - 12), GROUND_CL + 10])
            body_post(56);

    // ----- Антенна -----
    translate([34, -22, GROUND_CL + 18]) antenna_mount();
}

// ============================================================================
//  ЗАПУСК
// ============================================================================
assemble();
