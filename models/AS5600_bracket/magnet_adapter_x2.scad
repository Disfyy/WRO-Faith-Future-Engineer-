// ============================================================================
//  AS5600 Magnet Adapter — 2-up Print Plate (v1.0)
//  WRO Future Engineers 2026 • Team Faith
//
//  Ready-to-slice plate: TWO magnet adapters (one per rear wheel) laid out
//  side by side with a gap so the slicer treats them as separate parts.
//
//  Pulls the real adapter module from magnet_adapter.scad via use<>, so any
//  change there (MAGNET_FIT, knurl, pilot) flows through automatically.
//  use<> imports ONLY the module, so the ghost-magnet preview in that file
//  does NOT come along — the pockets export hollow.
//
//  Re-render:  openscad -o magnet_adapter_x2.stl magnet_adapter_x2.scad
//
//  PRINT SETTINGS (same as the single adapter):
//    Material: PETG or PLA   Layer: 0.12 mm   Infill: 100%   No supports.
//    Prints both wheels' adapters in one go.
// ============================================================================

use <magnet_adapter.scad>

COUNT   = 2;    // number of adapters on the plate
SPACING = 18;   // center-to-center, mm (12 mm OD + 6 mm gap between parts)

// Lay them in a row, centered on the origin (tidy preview + bed placement).
for (i = [0 : COUNT - 1])
    translate([i * SPACING - (COUNT - 1) * SPACING / 2, 0, 0])
        adapter();
