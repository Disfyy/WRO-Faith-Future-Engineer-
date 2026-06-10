// ============================================================================
//  Magnet Centering Adapter — Parametric (v1.0)
//  WRO Future Engineers 2026 • Team Faith
//
//  Problem: A magnet glued straight to a wheel face drifts off-center as the
//  glue creeps under vibration. If the magnet is off-center by >0.25mm, the
//  AS5600 reports non-linear angle (wobble in serial output during slow spin).
//
//  Solution: A small printed disc with a precision pocket for the magnet.
//  Glue the disc to the wheel center (or hex-hub end). Pocket walls keep
//  the magnet concentric forever.
//
//  PRINT SETTINGS:
//    Material: PETG or PLA (low load on this part)
//    Layer height: 0.12 mm (matters here — pocket must be precise)
//    Infill: 100% (small part, give it strength)
//    No supports.
//    Print 2 (one for each wheel).
// ============================================================================

/* [Diametric magnet — measure yours] */
MAGNET_D       = 4.0;   // our magnet: 4mm cylindrical diametric (datasheet nominal is 6mm)
MAGNET_T       = 2.0;   // magnet thickness
MAGNET_FIT     = 0.30;  // clearance added (0.10 = tight, 0.20 = loose-glue, 0.30 = drop-in for glue)

/* [Adapter disc] */
DISC_OD        = 12.0;  // overall outer diameter — fits over wheel center
DISC_T         = 3.5;   // total disc thickness
POCKET_DEPTH   = MAGNET_T + 0.2;  // pocket holds magnet flush + a touch deep

/* [Centering pilot (optional)] */
// If your HSP wheel has a 5mm hex hole or center pin, set this to 5.1 for a
// snug slip-fit that centers the adapter on the rotation axis automatically.
// Set to 0 to disable (flat-bottom adapter, glue-only).
PILOT_D        = 0;     // 5.1 for HSP 94182 hex-hub centering, 0 for glue-only
PILOT_DEPTH    = 2.0;

/* [Knurled grip ring (optional — improves glue bond surface area)] */
KNURL          = true;
KNURL_TEETH    = 24;
KNURL_DEPTH    = 0.4;

$fn = 96;

// ============================================================================
//  MAIN BODY
// ============================================================================
module adapter() {
    difference() {
        // Outer disc
        union() {
            cylinder(d=DISC_OD, h=DISC_T);
            if (KNURL) knurl_ring();
        }

        // Magnet pocket (top face)
        translate([0, 0, DISC_T - POCKET_DEPTH])
            cylinder(d=MAGNET_D + MAGNET_FIT, h=POCKET_DEPTH + 0.1);

        // Optional pilot hole (bottom face) for centering on hex hub
        if (PILOT_D > 0)
            translate([0, 0, -0.1])
                cylinder(d=PILOT_D, h=PILOT_DEPTH + 0.1);

        // Tiny chamfer on pocket entry — helps magnet drop in
        translate([0, 0, DISC_T - 0.4])
            cylinder(d1=MAGNET_D + MAGNET_FIT, d2=MAGNET_D + MAGNET_FIT + 0.6, h=0.4);
    }
}

module knurl_ring() {
    // Grip ridges must STRADDLE the disc OD: embedded ~0.6 mm into the body so
    // the union is a solid overlap (watertight), and protruding KNURL_DEPTH past
    // the OD as the actual grip texture. Teeth that merely touch the OD tangent
    // produce zero-width slivers → non-manifold edges that some slicers choke on.
    embed = 0.6;                                    // solid overlap into the disc
    width = embed + KNURL_DEPTH;                     // total radial extent of tooth
    cx    = DISC_OD/2 - embed/2 + KNURL_DEPTH/2;     // peak lands at OD/2 + KNURL_DEPTH
    for (i = [0:KNURL_TEETH-1]) {
        angle = i * 360 / KNURL_TEETH;
        rotate([0, 0, angle])
            translate([cx, 0, DISC_T/2])
            cube([width, 0.8, DISC_T - 0.6], center=true);
    }
}

// ============================================================================
//  PREVIEW with ghost magnet
// ============================================================================
SHOW_MAGNET = true;

module ghost_magnet() {
    color([0.6, 0.2, 0.2, 0.7])
        translate([0, 0, DISC_T - MAGNET_T])
        cylinder(d=MAGNET_D, h=MAGNET_T);
    // North/south marker (visual reminder — diametric, not axial)
    color([0.9, 0.9, 0.9])
        translate([0, 0, DISC_T - MAGNET_T/2])
        cube([MAGNET_D + 0.5, 0.3, 0.1], center=true);
}

// ============================================================================
adapter();
// $preview is true in F5/PNG preview, false during F6/STL render — so the ghost
// magnet is shown for visualization but NEVER baked into the exported STL.
if (SHOW_MAGNET && $preview) ghost_magnet();

// ============================================================================
//  USAGE
//  1. Print 2 adapters (one per rear wheel).
//  2. Drop magnet into pocket — should be snug. If too tight, reprint with
//     MAGNET_FIT = 0.20. If too loose, MAGNET_FIT = 0.10.
//  3. Verify magnet is DIAMETRICALLY magnetized: a steel paperclip should
//     attract to the SIDE of the magnet (not the face). If it attracts to
//     the face, you have an axial magnet — wrong type — replace it.
//  4. Glue adapter to wheel hub center with cyanoacrylate (super glue).
//     Press for 30 sec, cure 1 hour before driving.
//  5. With wheel mounted, spin by hand and watch AS5600 raw angle on serial:
//     should sweep 0..4095 smoothly. If it wobbles back-and-forth around
//     the true rotation, adapter is off-center — peel off and redo.
// ============================================================================
