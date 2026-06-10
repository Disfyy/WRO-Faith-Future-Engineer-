// ============================================================================
//  AS5600 Diametric Magnet — Dimensional Dummy (v1.0)
//  WRO Future Engineers 2026 • Team Faith
//
//  This is a 1:1 SIZE MODEL of the AS5600 magnet — a plain cylinder at the
//  magnet's real dimensions, nothing more. It is NOT a working magnet: a 3D
//  printer prints plastic, so this prints a non-magnetic blank.
//
//  What it's good for:
//    • Test-fit the pocket in magnet_adapter.scad WITHOUT risking the real
//      magnet (the real ones are easy to lose/crack — see shopping list).
//    • Check the 1.5 mm air gap / clearances in the printed assembly.
//    • A stand-in spacer while a real magnet is on order.
//
//  Magnet spec (matches magnet_adapter.scad — our actual magnet):
//    Diametrically-magnetised neodymium disc, 4.0 mm dia x 2.0 mm thick,
//    N35-N52. Note: this is SMALLER than the AS5600 datasheet nominal (6 mm),
//    so the usable air gap is tighter — run ~0.5-1.0 mm and check the AGC
//    register reads mid-range (not pinned), not the 1.5 mm a 6 mm magnet allows.
//    Sources:
//      ams AS5600 datasheet (Mouser): AMS_AS5600_Datasheet_EN.PDF
//      SimpleFOC community: "What diameter magnet works best with AS5xxx"
//
//  PRINT SETTINGS:
//    Material: PLA or PETG (it's just a fit gauge)
//    Layer height: 0.12 mm (small part — finer layers fit the pocket better)
//    Infill: 100%
//    No supports.
//    Print 2 (one fit-check per wheel adapter).
// ============================================================================

/* [Diametric magnet — measure yours, then match the adapter] */
MAGNET_D       = 4.0;   // our magnet: 4 mm cylindrical diametric (datasheet nominal is 6 mm)
MAGNET_T       = 2.0;   // magnet thickness

/* [Pole indicator — optional visual only, off by default ("nothing more")] */
// A real diametric magnet has N/S split across the DIAMETER (the side), not
// the flat faces. Set true to engrave a thin line across the top marking that
// axis — handy as a build reminder. It does NOT change the printed size.
SHOW_POLE_LINE = false;
POLE_LINE_W    = 0.4;   // engraved line width
POLE_LINE_D    = 0.3;   // engraved line depth

$fn = 96;

// ============================================================================
//  MODEL
// ============================================================================
module magnet_dummy() {
    difference() {
        // The magnet, exactly to size: 6.0 dia x 2.5 thick.
        cylinder(d=MAGNET_D, h=MAGNET_T);

        // Optional engraved pole-axis line across the top face.
        if (SHOW_POLE_LINE)
            translate([0, 0, MAGNET_T - POLE_LINE_D])
                cube([MAGNET_D + 1, POLE_LINE_W, POLE_LINE_D + 0.1], center=true);
    }
}

magnet_dummy();

// ============================================================================
//  USAGE
//  1. Print 1-2 of these.
//  2. Drop into the magnet_adapter.scad pocket. Snug = good. If it won't seat,
//     bump MAGNET_FIT in magnet_adapter.scad to 0.20 and reprint the adapter;
//     if it rattles, drop MAGNET_FIT to 0.10.
//  3. Once the fit is right, swap in the REAL diametric magnet for the car.
// ============================================================================
