# Schemes — wiring diagrams & system schematics

This folder is the visual reference for the WRO Future Engineers vehicle.
Everything here describes the **v13 hardware revision** (ESP32-S3-DevKitC-1 + 2× AS5600 on dual native I²C + 2× VL53L1X with XSHUT runtime address remap, **no TCA9548A mux**).

The authoritative long-form pin reference lives at
[`../docs/guides/WRO_Wiring_Map_v13.md`](../docs/guides/WRO_Wiring_Map_v13.md).
The files here summarise that reference visually.

---

## Reading order

If you're new to the project (or a judge orienting yourself from the file tree), read in this order:

1. **[`WRO_Wiring_Map.md`](WRO_Wiring_Map.md)** — at-a-glance pin map, I²C address table, power rails, pre-flight checks.
2. **[`05_mechanical_layout.md`](05_mechanical_layout.md)** — where things physically sit on the chassis and how to route the bundles.
3. **[`WRO_Full_System_Diagram.md`](WRO_Full_System_Diagram.md)** — master block-diagram overview (with embedded Mermaid).
4. The four **focused views** below — one subsystem at a time.
5. **[`WRO_Detailed_Wiring_Diagram.mmd`](WRO_Detailed_Wiring_Diagram.mmd)** — wire-by-wire schematic with decoupling caps and every ground return.

---

## File index

### Top-level overview

| File | Type | What it covers |
|---|---|---|
| [`WRO_Wiring_Map.md`](WRO_Wiring_Map.md) | Markdown · ASCII tables | Pin map, I²C addresses, power rails, UART, cross-revision history |
| [`WRO_Full_System_Diagram.md`](WRO_Full_System_Diagram.md) | Markdown + embedded Mermaid | Master block diagram with prose context |
| [`WRO_Full_System_Diagram.mmd`](WRO_Full_System_Diagram.mmd) | Standalone Mermaid | Same diagram, source-only (for rendering tools) |
| [`WRO_Detailed_Wiring_Diagram.mmd`](WRO_Detailed_Wiring_Diagram.mmd) | Standalone Mermaid | Wire-by-wire detail with caps and grounds |
| [`05_mechanical_layout.md`](05_mechanical_layout.md) | Markdown · ASCII art | Top-down chassis layout + wire-routing zones |

### Focused subsystem views (Mermaid `.mmd`)

| File | Subsystem | Why look here |
|---|---|---|
| [`01_power_distribution.mmd`](01_power_distribution.mmd) | Power rails | When you're checking VBAT / +5 V / +3.3 V and decoupling |
| [`02_signal_control.mmd`](02_signal_control.mmd) | UART / PWM / safety | When you're wiring the camera, BTS7960, servo, or E-Stop |
| [`03_i2c_buses.mmd`](03_i2c_buses.mmd) | Dual I²C + XSHUT | When you're debugging a sensor or the address-remap dance |
| [`04_fsm_dataflow.mmd`](04_fsm_dataflow.mmd) | Firmware FSM + data flow | When you're reasoning about what state the firmware is in |

### Rendered images (regenerable)

The `renders/` subfolder holds PNG and SVG exports of every `.mmd` source above. They are **generated artefacts** — never hand-edit them. Re-run the render command below whenever a source changes.

---

## Re-rendering

### Mermaid CLI (recommended)

From inside the `schemes/` folder:

```bash
mkdir -p renders
for src in \
  WRO_Full_System_Diagram.mmd \
  WRO_Detailed_Wiring_Diagram.mmd \
  01_power_distribution.mmd \
  02_signal_control.mmd \
  03_i2c_buses.mmd \
  04_fsm_dataflow.mmd
do
  base="${src%.mmd}"
  npx -y @mermaid-js/mermaid-cli -i "$src" -o "renders/${base}.svg"
  npx -y @mermaid-js/mermaid-cli -i "$src" -o "renders/${base}.png" -w 2400
done
```

### Mermaid Live Editor

Paste the contents of any `.mmd` file into [mermaid.live](https://mermaid.live/) and export PNG/SVG. Drop the result into `renders/` with the matching base name.

---

## House style for new diagrams

If you add or modify a `.mmd` file, keep the colour conventions consistent:

| `classDef` | Hex fill / stroke | Used for |
|---|---|---|
| `brain` | `#fff8e1` / `#f9a825` | ESP32-S3 |
| `cam` | `#fff3e0` / `#ef6c00` | OpenMV |
| `act` | `#fce4ec` / `#ad1457` | Actuators (motor, servo, H-bridge) |
| `sens` | `#e8f5e9` / `#2e7d32` | I²C / UART sensors |
| `pwr5` | `#ffebee` / `#c62828` | +VBAT / +5 V |
| `pwr3` | `#fff3e0` / `#e65100` | +3.3 V |
| `pass` | `#f3e5f5` / `#6a1b9a` | Passives (caps, pull-ups) |
| `io` | `#e3f2fd` / `#1565c0` | Safety / status I/O |
| `star` | `#212121` fill / `#ffeb3b` stroke | Star ground |

Arrow conventions:

- `==>` thick — high-current / power
- `-->` regular — signal
- `-.-` dotted — ground return or "consumes" annotation
- `<-->` bidirectional — UART, bus link

---

## What changed in this redesign

Compared to the previous version of this folder:

- Master diagram now uses bus-style nodes (`+5 V`, `+3.3 V`, `+VBAT`) so power lines aren't N-to-N spaghetti.
- Star ground collapsed to a single visual hub (no per-subgraph duplicates).
- Split into **four focused views** + one detailed wire-by-wire view + one ASCII chassis layout. Each fits in your head individually.
- Added a **firmware FSM + data-flow diagram** ([`04_fsm_dataflow.mmd`](04_fsm_dataflow.mmd)) — first non-electrical diagram in the folder.
- Renders moved to `renders/` so source and exports don't get confused.
- Added a colour/arrow legend you can reuse when adding new diagrams.
