# HSP 94182 — 3D chassis model (WRO Future Engineers)

Parametric 3D model of the **HSP 94182** chassis (1/16 scale, on-road electric touring) for the **World Robot Olympiad — Future Engineers** team documentation.

The package is structured so that **any other team can reproduce the model** on any OS, without paid software. All sources are open and parametric.

---

## Folder layout

```
HSP94182_3D/
├── viewer.html         Interactive 3D viewer (opens in any browser)
├── chassis_cad.py      True CAD model in CadQuery -> STEP / STL / 3MF
├── chassis.scad        Parametric model in OpenSCAD (alternative)
├── generate_stl.py     Batch STL exporter from the OpenSCAD model
├── requirements.txt    Python deps (for chassis_cad.py)
├── README.md           This file
├── cad/                (created by chassis_cad.py) generated .STEP / .STL
└── stl/                (created by generate_stl.py) STL from the OpenSCAD model
```

### Which model file should you use

| File              | Type              | When to use                                                          |
|-------------------|-------------------|----------------------------------------------------------------------|
| `chassis_cad.py`  | **True CAD**      | Engineering documentation; import into Fusion 360 / SolidWorks / OnShape; FEA; drawings |
| `chassis.scad`    | Parametric script | Quick prototyping, easy dimension tweaks                              |
| `viewer.html`     | Web viewer        | Presentations, demos, GLB export for AR/VR                            |

---

## Quick start — 4 ways

### 1. Generate a real CAD file (.STEP) — recommended for WRO

**STEP** is the universal CAD interchange format and opens in **Fusion 360, SolidWorks, OnShape, FreeCAD, CATIA, Inventor, Solid Edge, KOMPAS-3D**, and any other CAD package. STEP carries the parametric B-Rep geometry (faces, edges, vertices), not a tessellated mesh — so the model can be edited in your CAD tool as a real solid.

```bash
# 1. Install CadQuery (one time)
pip install -r requirements.txt

# 2. Generate the full assembly -> cad/HSP94182_assembly.step
python3 chassis_cad.py

# 3. Or a single part
python3 chassis_cad.py --part chassis_plate --format step

# 4. List all available parts
python3 chassis_cad.py --list
```

**Importing STEP into popular CAD tools:**

| Program          | How to import                                              |
|------------------|------------------------------------------------------------|
| **Fusion 360**   | File -> Insert -> Insert Derive / Upload                    |
| **SolidWorks**   | File -> Open -> select .step                                |
| **OnShape**      | + -> Import -> upload .step                                 |
| **FreeCAD**      | File -> Open -> .step (free, cross-platform)                |
| **KOMPAS-3D**    | File -> Open -> STEP (ASCII / binary supported)             |
| **Blender**      | Plugin "STEPper", or convert .step -> .obj via FreeCAD      |

### 2. Just look at the model (no install)

Double-click **`viewer.html`** — it runs in any modern browser (Chrome / Safari / Firefox / Edge). Features:

- rotate (LMB), pan (RMB), zoom (wheel)
- toggle layers: chassis, suspension, wheels, drivetrain, electronics, body
- preset views: isometric / top / side / front
- export to:
  - **`.GLB`** — Blender, Unity, Unreal, AR/VR
  - **`.STL`** — 3D-printing
  - **`.PNG`** — snapshot for the engineering journal

> ⚠️ The first run needs internet access (Three.js loads from a CDN). After that, the same tab works offline.

### 3. Edit parameters in OpenSCAD

[OpenSCAD](https://openscad.org/downloads.html) is a free cross-platform script-based CAD. Open **`chassis.scad`** and edit the parameters at the top:

```scad
WHEELBASE   = 205;   // distance between axles
TRACK       = 130;   // track width
WHEEL_D     = 54;    // wheel diameter
BAT_L       = 78;    // battery length
...
```

Press **F5** (preview) or **F6** (full render). `File -> Export -> STL` saves the mesh.

### 4. Batch-generate STL

```bash
# In a terminal, inside HSP94182_3D/:
python3 generate_stl.py            # generate ALL parts
python3 generate_stl.py --list     # list available parts
python3 generate_stl.py --part 01_chassis_plate --quality high
```

The result lands in `stl/`, ready for any slicer (**Cura, PrusaSlicer, Bambu Studio**).

> The script needs OpenSCAD installed. It auto-locates the binary on macOS / Linux / Windows.

---

## CAD model (`chassis_cad.py`) — details

**CadQuery** ([cadquery.readthedocs.io](https://cadquery.readthedocs.io)) is a professional Python library for parametric CAD on top of **OpenCascade** (the same kernel FreeCAD uses). It outputs **B-Rep** solid models that, after STEP export, open in any modern CAD system as fully editable solids.

### Available export formats

| Format | Extension     | Use case |
|--------|---------------|----------|
| **STEP** | `.step` / `.stp` | Universal CAD — Fusion 360, SolidWorks, OnShape, FreeCAD, CATIA |
| **STL**  | `.stl`           | 3D-printing (mesh), slicer import |
| **3MF**  | `.3mf`           | Modern format for Bambu Studio / PrusaSlicer (with colors) |
| **GLTF** | `.glb`           | AR/VR, web viewing, Blender |

```bash
# All four formats at once
python3 chassis_cad.py --format step,stl,3mf,gltf

# STEP only, into a custom directory
python3 chassis_cad.py --format step --outdir ~/Documents/WRO_CAD/
```

### Parts in the CAD model

You can export the full assembly or any single part:

| Name            | Description                                        |
|-----------------|----------------------------------------------------|
| `chassis_plate` | Main plate with three triangular weight-relief cutouts |
| `upper_deck`    | T-shaped upper deck                                |
| `shock_tower`   | Shock tower (front and rear share geometry)        |
| `lower_arm`     | Lower suspension arm                               |
| `upper_arm`     | Upper suspension arm                               |
| `steering_hub`  | Steering knuckle (no kingpin)                      |
| `body_post`     | Body mounting post                                 |
| `antenna_mount` | Antenna mount                                      |
| `shock_body`    | Shock absorber body                                |
| `shock_spring`  | Shock spring (helical sweep)                       |
| `driveshaft`    | Drive shaft / cardan                               |

### Editing CAD parameters

All key dimensions live at the top of `chassis_cad.py`:

```python
WHEELBASE       = 205.0   # wheelbase (mm)
TRACK           = 130.0   # track (mm)
CHASSIS_L       = 250.0   # main plate length
CHASSIS_W       =  80.0   # main plate width
WHEEL_D         =  54.0   # wheel diameter
SPRING_TURNS    =   9     # spring turns
BAT_L           =  78.0   # battery length
...
```

Change the value -> rerun the script -> get a fresh STEP. Every part recomputes automatically.

### Coordinate frame (standard automotive CAD)

```
   +Z (up)
    │
    │   +X (forward, direction of travel)
    │  /
    │ /
    └────── +Y (left, when sitting in the driver seat)
```

---

## HSP 94182 chassis specification

| Parameter             | Value                          |
|-----------------------|--------------------------------|
| Scale                 | 1 : 16                         |
| Type                  | On-road touring (flat chassis) |
| Overall length        | ≈ 273 mm                       |
| Width                 | ≈ 155 mm                       |
| Height                | ≈ 78 mm                        |
| Wheelbase             | 205 mm                         |
| Track                 | 130 mm                         |
| Ground clearance      | ≈ 8 mm                         |
| Wheels                | ⌀ 54 mm × 22 mm                |
| Drive                 | 4WD (cardan between diffs)     |
| Differentials         | 2 (front + rear)               |
| Shock absorbers       | 4 (oil, adjustable)            |
| Battery               | Li-Po 7.4 V, ≈ 78 × 36 × 18 mm |

---

## Parts list

| Group         | Parts                                                                  |
|---------------|------------------------------------------------------------------------|
| **Chassis**   | main plate, upper T-deck, side rails, shock towers (×2)                |
| **Suspension**| lower arms (×4), upper arms (×4), steering hubs (×4), shocks with springs (×4), tie rod |
| **Wheels**    | tires with tread (×4), 10-spoke rims (×4), hex hubs (×4)               |
| **Drivetrain**| differentials (×2), cardan shaft, motor with pinion gear               |
| **Electronics**| Li-Po battery, ESC, steering servo                                    |
| **Body**      | front/rear bumpers, body posts (×4), antenna mount                     |

---

## Colors and materials (matching the original)

| Model color    | Real material                  | Where it's used |
|----------------|--------------------------------|-----------------|
| Matte black    | ABS plastic                    | Chassis, arms, bumpers, towers |
| Glossy blue    | 6061-T6 anodized aluminum      | Hubs, shock caps, hex hubs, diff covers |
| Silver         | Steel / stainless              | Screws, shafts, springs, R-clips |
| White          | Painted plastic                | Wheel rims |
| Yellow         | Li-Po battery shrink           | Battery |

---

## Resizing the model for your robot

In `chassis.scad`, all key parameters live at the top of the file. For example, to lift the ground clearance:

```scad
GROUND_CL = 12;   // was 8, +4 mm
```

Or to fit larger wheels:

```scad
WHEEL_D = 62;     // up from 54
WHEEL_W = 26;     // up from 22
```

Save -> press **F5** in OpenSCAD and the geometry rebuilds automatically.

---

## 3D-printing recommendations

These settings have been validated on 0.4 mm-nozzle FDM printers:

| Part               | Material  | Layer | Infill | Supports | Orientation |
|--------------------|-----------|-------|--------|----------|-------------|
| `01_chassis_plate` | PETG/ABS  | 0.20  | 50%    | none     | flat        |
| `02_upper_deck`    | PETG      | 0.20  | 40%    | none     | flat        |
| `03_shock_tower`   | PETG/PC   | 0.16  | 60%    | none     | vertical    |
| `04_lower_arm`     | PETG      | 0.16  | 80%    | none     | flat        |
| `05_upper_arm`     | PETG      | 0.16  | 80%    | none     | flat        |
| `06_steering_hub`  | PETG/PC   | 0.12  | 100%   | none     | vertical    |
| `07_body_post`     | PETG      | 0.20  | 60%    | none     | vertical    |
| `08_foam_bumper`   | TPU 95A   | 0.20  | 30%    | none     | flat        |
| `09_antenna_mount` | PETG      | 0.20  | 50%    | none     | flat        |

For moving parts (arms, hubs), prefer **PC (polycarbonate)** or **PETG-CF** — they survive impacts better.

---

## Compliance with WRO Future Engineers rules

This package is intentionally built as **open engineering documentation**:

- ✅ All sources are text (`.scad`, `.html`, `.py`, `.md`) — readable in any editor
- ✅ No paid software — OpenSCAD, Python, any browser are free
- ✅ Parametric — other teams can adapt to their constraints
- ✅ Reproducible — identical output on macOS / Linux / Windows
- ✅ Git-friendly — fits naturally into the engineering journal

---

## Useful commands

```bash
# Open the viewer (macOS / Linux / Windows)
open viewer.html       # macOS
xdg-open viewer.html   # Linux
start viewer.html      # Windows

# === CAD MODEL (recommended for WRO) ===
pip install -r requirements.txt    # one time
python3 chassis_cad.py             # -> cad/HSP94182_assembly.step
python3 chassis_cad.py --list      # list of single parts
python3 chassis_cad.py --part chassis_plate --format step,stl

# === OPENSCAD MODEL (alternative) ===
python3 generate_stl.py                                  # all STLs
python3 generate_stl.py --part 01_chassis_plate --quality ultra
python3 generate_stl.py --list
```

---

## Version history

| Version | Date       | Changes                                                       |
|---------|------------|---------------------------------------------------------------|
| 1.0     | 2026-04-27 | First publication: full assembly, STL/GLB export              |
| 2.0     | 2026-04-27 | Added the real CAD model (CadQuery → STEP)                    |

---

## License

Files are released under **CC BY 4.0** — free to use, modify, and publish with attribution. Suitable for WRO documentation.

---

## Team contact

> Fill in before submitting the engineering journal:

- Team: ___________________________
- Region: __________________________
- Year: 2026
- Category: WRO Future Engineers
