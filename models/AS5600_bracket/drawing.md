# AS5600 Encoder Bracket — Hand-Fabrication Drawing

For teams without a 3D printer. Cut from **2mm aluminum sheet** (preferred) or **3mm acrylic** (acceptable fallback).

All dimensions in **millimetres**. Tolerance: ±0.2mm on hole positions, ±0.5mm on outline.

> **Rigidity note:** The 3D-printed `AS5600_bracket.stl` version includes an
> integrated diagonal strut + gussets + buttresses that triangulate the
> structure. This hand-cut version does **not** include the strut — adding
> a folded-sheet diagonal would require complex joinery. To match the 3D
> version's rigidity, cut and rivet a **separate diagonal strap** (Section 4
> below) running from the top of the sensor mount back to the chassis. With
> the diagonal strap the hand-cut version equals the 3D version. Without
> it, expect ~3–5× more deflection under load.

---

## 1. Flat blank — cut this shape, then bend at the fold lines

```
                                      <-- mark "OUT" on this face (faces wheel)
       28                              
   <--------->                          
   +---------+---------+---------+      
   |         |         |         |      
   |    A    |    B    |    C    |      
   |  28 W   |  16 H   |  28 W   |      
   |         |         |         |      
   |         |         |    o    | <-- M2 hole  (slot 4 long)
   |    M3   |         |         |      
   |    o    |         |   24 H  |      
   |         |         |         |      
   |  14 ↕   |         |         |      
   |         |         |         |      
   |    M3   |         |    o    | <-- M2 hole  (slot 4 long)
   |    o    |         |         |      
   |         |         |         |      
   +---------+---------+---------+      
            FOLD 1    FOLD 2            
            (90°)     (90°)             

OVERALL FLAT BLANK: 72 mm long × 28 mm wide (before folding)
```

### Region A — Anchor flange (bolts to chassis side)
- Width: **28 mm** (along car length)
- Height: **22 mm** (vertical)
- 2× **M3 holes**, Ø3.4 mm
- Hole centres: spaced **14 mm** apart vertically, centred on the flange width

### Region B — Vertical riser
- Width: **16 mm** (matches arm rise dimension)
- Height: **22 mm**
- No holes

### Region C — Sensor mount (holds PCB)
- Width: **28 mm**
- Height: **24 mm** (slightly taller than PCB)
- 2× **M2 slots**, Ø2.4 mm with 4 mm slot length running perpendicular to the fold edge
  - Slots centred: **17 mm** apart (matches AS5600 board hole spacing — verify with calipers)
- Optional: Ø8 mm window in centre for AS5600 chip visibility

### Fold lines
- **Fold 1** (Region A → B): 90° toward the inside of the L
- **Fold 2** (Region B → C): 90° back outward — gives the lateral reach

Bending sequence (use a bench vice + hammer + scrap wood):
1. Clamp blank with Fold 1 line at vise jaw edge
2. Hammer down to 90°
3. Repeat for Fold 2 — fold direction opposite to Fold 1
4. Check angles with a square — must be 90° ±2°

---

## 2. Gusset — add for rigidity (CRITICAL)

A flat L-bracket without a gusset will flex. Cut a triangular gusset from the same sheet and rivet/bolt it across Fold 1.

```
   GUSSET — 14 × 14 mm right triangle
   
         |
         |  <-- 14 mm
   +-----+
   |    /
   |   /
   |  /
   | /
   |/
   +
   14 mm
```

- Drill 1× M2 hole near each leg edge (centred 4 mm in from each corner of the right angle)
- Bolt/rivet onto Region A and Region B at Fold 1 corner — sandwich the gusset between bracket and screw head

A second gusset at Fold 2 corner is recommended but optional (less critical because Fold 2 carries less bending load).

---

## 2a. Diagonal strap — the truss element (CRITICAL for rigidity)

This is what the 3D version calls the "diagonal strut". For the hand-cut
version, add it as a separate folded strip riveted in place.

```
   STRAP — 50 × 6 mm strip, 1.5 mm bend at each end

   <---------- 50 mm ----------->
   +-----+--------------------+-----+
   |  o  |                    |  o  |   <-- Ø2.4 mm M2 holes 4 mm in
   +-----+--------------------+-----+
       ↑                          ↑
     bend 90° here              bend 90° here
     (so this 5mm flap          (so this 5mm flap
      bolts to sensor mount      bolts to anchor flange
      Region C)                  Region A)
```

Cut from the same 2mm aluminium sheet (or 1.5mm steel if you have it —
even better in tension).

Bend the two end flaps 90° in the same direction (forming a shallow "[ ").
Bolt one end to **Region A** (anchor flange, near the bottom-front edge)
using an M2 screw + nut, with a new Ø2.4mm hole drilled in Region A
**below** the lower M3 anchor hole. Bolt the other end to **Region C**
(sensor mount, near the upper edge above the chip window) using another
M2 screw + nut.

The strap runs diagonally through open air between anchor and sensor
mount, on the chassis-side of the wheel. It must NOT touch the wheel or
suspension arm — eyeball clearance after install.

Pre-bend test: with the strap in place, push laterally on the sensor
mount with your finger. Deflection should drop by a factor of 3 or more
versus without the strap.

---

## 3. Assembly cross-section (side view, on the car)

```
           magnet on wheel face         OUTER WHEEL
              |  air gap 1.5 mm           |
              v                           |
          +---+                          ROTATES
          |   |                           |
          |[AS5600]<-- PCB
          | sensor  
          | mount (Region C)
          |
          |   <-- vertical riser (Region B)
          |
          |
   =======+   <-- chassis edge (Region A bolted here with 2× M3)
   ###############  chassis floor (rigid)
                            ↓ GROUND
```

---

## 4. Bill of materials (per bracket, ×2 for the car)

| # | Item | Qty | Note |
|---|---|---|---|
| 1 | 2mm aluminium sheet, 80×40 mm blank | 1 | Or 3mm acrylic if no metal access |
| 2 | M3 × 8 mm screws | 2 | For chassis anchor — check your chassis thread depth |
| 3 | M3 nuts (or threaded inserts) | 2 | Skip if chassis has tapped M3 holes |
| 4 | M2 × 6 mm screws | 2 | For sensor PCB attachment |
| 5 | M2 nuts | 2 | On back side of sensor mount |
| 6 | M2 washers | 4 | Spread load on the slotted holes |
| 7 | Small gusset triangle (same material) | 1 | 14×14 mm — see Section 2 |
| 8 | M2 × 4 mm screws (for gusset) | 2 | Or rivets |
| 9 | Diagonal strap, 50×6 mm | 1 | Same material as bracket — see Section 2a |
| 10 | M2 × 6 mm screws (for strap ends) | 2 | One into anchor, one into sensor mount |
| 11 | M2 nuts (for strap) | 2 | On the back side |

---

## 5. Tolerances and fit checks

After fabrication, before mounting on the car:

1. **Square check** — both folds must be 90° ±2°. Adjust with vise if not.
2. **PCB fit** — drop the AS5600 board into the sensor mount with M2 screws loose. Should sit flat with no wobble. Slot must allow ±2 mm of front/back travel.
3. **Air-gap simulation** — hold the bracket up against the wheel with the PCB installed. Visually verify the AS5600 chip surface sits **1.5 mm** from the wheel hub centre (where the magnet will go). Eyeball or use a feeler gauge.
4. **Rigidity test** — clamp the bracket by the anchor flange in a vise. Push laterally on the sensor mount with one finger. The mount should NOT visibly deflect. If it does, the gusset isn't bolted tight or the material is too thin.

Pass all four checks before installing on the car.
