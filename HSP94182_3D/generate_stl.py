#!/usr/bin/env python3
"""
generate_stl.py — пакетная генерация STL-файлов из chassis.scad

Назначение
----------
Команда WRO Future Engineers: запустите этот скрипт, чтобы из параметрической
OpenSCAD-модели получить готовые STL-файлы — отдельно по каждой детали, готовые
к 3D-печати или импорту в Cura / PrusaSlicer / Bambu Studio.

Зависимости
-----------
• Python 3.8+
• OpenSCAD (https://openscad.org/downloads.html) — устанавливается отдельно.
  Скрипт автоматически найдёт его в стандартных путях для macOS / Linux / Windows.

Использование
-------------
    python3 generate_stl.py                # сгенерировать все детали (по умолчанию)
    python3 generate_stl.py --list         # показать список деталей
    python3 generate_stl.py --part 01_chassis_plate
    python3 generate_stl.py --outdir ./stl_v2 --quality high
"""

from __future__ import annotations
import argparse
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SCAD_FILE = ROOT / "chassis.scad"
DEFAULT_OUTDIR = ROOT / "stl"

# ----------------------------------------------------------------------------
# Список деталей: имя файла → вызов OpenSCAD-модуля
# Можно расширять, добавляя любую деталь, определённую в chassis.scad.
# ----------------------------------------------------------------------------
PARTS: dict[str, dict] = {
    "00_full_assembly": {
        "call": "assemble();",
        "desc": "Полная сборка шасси (для просмотра / референса)",
    },
    "01_chassis_plate": {
        "call": "chassis_plate();",
        "desc": "Главная плита (печатать из ABS / PETG, 3-4 мм)",
    },
    "02_upper_deck": {
        "call": "upper_deck();",
        "desc": "Верхняя T-образная дека",
    },
    "03_shock_tower_front": {
        "call": "shock_tower(0);",
        "desc": "Шок-башня (одинаковая спереди и сзади)",
    },
    "04_lower_arm_left": {
        "call": "suspension_arm(length=55, w=12, side=-1);",
        "desc": "Левый нижний рычаг подвески",
    },
    "04_lower_arm_right": {
        "call": "suspension_arm(length=55, w=12, side=1);",
        "desc": "Правый нижний рычаг подвески",
    },
    "05_upper_arm_left": {
        "call": "suspension_arm(length=43, w=5, side=-1);",
        "desc": "Левый верхний рычаг подвески",
    },
    "05_upper_arm_right": {
        "call": "suspension_arm(length=43, w=5, side=1);",
        "desc": "Правый верхний рычаг подвески",
    },
    "06_steering_hub_left": {
        "call": "steering_hub(side=-1);",
        "desc": "Левый поворотный кулак (передний)",
    },
    "06_steering_hub_right": {
        "call": "steering_hub(side=1);",
        "desc": "Правый поворотный кулак (передний)",
    },
    "07_body_post": {
        "call": "body_post(56);",
        "desc": "Кузовная стойка (4 шт)",
    },
    "08_foam_bumper": {
        "call": "foam_bumper();",
        "desc": "Бампер с пеной (2 шт — перед/зад)",
    },
    "09_antenna_mount": {
        "call": "antenna_mount();",
        "desc": "Крепление антенны",
    },
    "10_wheel": {
        "call": "wheel();",
        "desc": "Колесо в сборе (для референса; печать импрактична)",
    },
    "11_battery_dummy": {
        "call": "battery();",
        "desc": "Габаритный макет аккумулятора",
    },
    "12_motor_dummy": {
        "call": "motor();",
        "desc": "Габаритный макет мотора",
    },
}

# Качество рендера → значение $fn в OpenSCAD
QUALITY_PRESETS = {
    "draft":  32,
    "normal": 64,
    "high":   128,
    "ultra":  256,
}


def find_openscad() -> str | None:
    """Ищет исполняемый файл OpenSCAD в стандартных путях."""
    candidates = [
        "openscad",
        "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD",
        "/usr/bin/openscad",
        "/usr/local/bin/openscad",
        "/opt/homebrew/bin/openscad",
        "C:/Program Files/OpenSCAD/openscad.exe",
        "C:/Program Files (x86)/OpenSCAD/openscad.exe",
    ]
    for c in candidates:
        if shutil.which(c):
            return shutil.which(c)
        p = Path(c)
        if p.exists():
            return str(p)
    return None


def render_part(scad_call: str, output_stl: Path, openscad_bin: str, fn_quality: int) -> tuple[bool, str]:
    """Рендерит одну деталь во временный .scad → STL через OpenSCAD."""
    wrapper = f"""// автогенерация — generate_stl.py
$fn = {fn_quality};
use <{SCAD_FILE.as_posix()}>;
{scad_call}
"""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".scad", delete=False, encoding="utf-8") as tmp:
        tmp.write(wrapper)
        tmp_path = Path(tmp.name)
    try:
        proc = subprocess.run(
            [openscad_bin, "-o", str(output_stl), str(tmp_path)],
            capture_output=True, text=True, timeout=180,
        )
        if proc.returncode != 0:
            return False, proc.stderr.strip() or proc.stdout.strip() or "unknown error"
        return True, ""
    except subprocess.TimeoutExpired:
        return False, "timeout (>180s)"
    finally:
        tmp_path.unlink(missing_ok=True)


def fmt_size(n_bytes: int) -> str:
    if n_bytes < 1024:
        return f"{n_bytes} B"
    if n_bytes < 1024 * 1024:
        return f"{n_bytes / 1024:.1f} KB"
    return f"{n_bytes / 1024 / 1024:.2f} MB"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Генерация STL-файлов из параметрической модели chassis.scad",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--list", action="store_true", help="Показать список деталей и выйти")
    parser.add_argument("--part", action="append", default=None,
                        help="Имя детали (можно несколько раз). По умолчанию — все.")
    parser.add_argument("--outdir", default=str(DEFAULT_OUTDIR),
                        help=f"Папка для STL (по умолчанию: {DEFAULT_OUTDIR})")
    parser.add_argument("--quality", choices=list(QUALITY_PRESETS), default="normal",
                        help="Качество рендера ($fn): draft=32, normal=64, high=128, ultra=256")
    parser.add_argument("--openscad", default=None,
                        help="Путь к OpenSCAD (если не найден автоматически)")
    args = parser.parse_args()

    if args.list:
        print("\nДоступные детали:\n" + "-" * 60)
        for name, info in PARTS.items():
            print(f"  {name:<28} — {info['desc']}")
        print()
        return 0

    if not SCAD_FILE.exists():
        print(f"ОШИБКА: не найден {SCAD_FILE}", file=sys.stderr)
        return 2

    openscad = args.openscad or find_openscad()
    if not openscad:
        print("ОШИБКА: OpenSCAD не найден.", file=sys.stderr)
        print("Скачайте бесплатно: https://openscad.org/downloads.html", file=sys.stderr)
        print("Или укажите путь: --openscad /путь/к/openscad", file=sys.stderr)
        return 3

    print(f"OpenSCAD: {openscad}")
    print(f"SCAD-файл: {SCAD_FILE}")
    print(f"Качество ($fn): {QUALITY_PRESETS[args.quality]}")

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    print(f"Папка вывода: {outdir.resolve()}\n")

    targets = args.part if args.part else list(PARTS.keys())
    bad = [t for t in targets if t not in PARTS]
    if bad:
        print(f"ОШИБКА: неизвестные детали: {bad}", file=sys.stderr)
        print("Запустите --list чтобы увидеть доступные имена.", file=sys.stderr)
        return 4

    fn = QUALITY_PRESETS[args.quality]
    ok, fail = 0, 0
    t0 = time.time()
    for name in targets:
        info = PARTS[name]
        stl_path = outdir / f"{name}.stl"
        print(f"→ {name:<28} ", end="", flush=True)
        success, err = render_part(info["call"], stl_path, openscad, fn)
        if success and stl_path.exists():
            print(f"OK   {fmt_size(stl_path.stat().st_size):>10}   ({info['desc']})")
            ok += 1
        else:
            print(f"FAIL {err[:80]}")
            fail += 1

    dt = time.time() - t0
    print(f"\nГотово: {ok} ОК, {fail} ошибок, время {dt:.1f} с.")
    return 0 if fail == 0 else 5


if __name__ == "__main__":
    sys.exit(main())
