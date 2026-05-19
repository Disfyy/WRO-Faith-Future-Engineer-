// ============================================================================
//  AS5600 Encoder Bracket — Parametric (v1.0)
//  WRO Future Engineers 2026 • Team Faith
//  Chassis: HSP 94182 (1/16) • Sensor: AS5600 purple breakout (~22x18mm)
//
//  OPEN IN OPENSCAD: https://openscad.org
//  F5 = preview  •  F6 = render  •  File > Export > STL
//
//  PRINT SETTINGS (recommended):
//    Material: PETG (stiffer than PLA, won't creep under vibration)
//    Layer height: 0.2 mm
//    Infill: 60% (rigidity matters more than weight here)
//    Walls: 4 perimeters (kills flex)
//    Print orientation: anchor face flat on bed (strongest layer direction
//    perpendicular to the cantilever bending load)
//    NO supports needed if oriented correctly.
// ============================================================================

/* [Measure these on your robot — units mm] */

// Distance from chassis side face to wheel INNER face
ARM_REACH         = 28;

// Vertical rise from chassis-side anchor to axle (wheel rotation) centerline
ARM_RISE          = 16;

// Air gap nominal (0.5 .. 3.0 — keep around 1.5)
AIR_GAP           = 1.5;

/* [AS5600 purple breakout — verify with calipers] */

// PCB outline
PCB_W             = 22;       // width across the long axis (where holes are)
PCB_H             = 18;       // height (short axis)
PCB_T             = 1.6;      // PCB thickness (clearance pocket only)

// M2 mounting hole spacing on PCB (center-to-center, long axis)
PCB_HOLE_SPACING  = 17;

// AS5600 chip center offset from PCB long-axis center (0 if chip is centered)
CHIP_OFFSET_X     = 0;

// AS5600 chip face height above PCB top (most modules: ~1.0)
CHIP_HEIGHT       = 1.0;

/* [Anchor (chassis-side mounting flange)] */

// Chassis-side anchor: 2 M3 holes vertically stacked
ANCHOR_HOLE_DY    = 14;       // vertical spacing between the two M3 anchor holes
ANCHOR_W          = 18;       // width of anchor flange (along car length)
ANCHOR_H          = 22;       // height of anchor flange (vertical)
ANCHOR_T          = 4;        // anchor flange thickness

/* [Bracket body] */

// Main bracket plate thickness — DO NOT go below 3mm
BRACKET_T         = 4;

// Triangular gusset depth (kills first-mode resonance)
GUSSET_DEPTH      = 14;
GUSSET_T          = 3;

/* [Hole sizes — slight clearance over nominal] */
M2_HOLE           = 2.4;      // tight clearance for M2 screw
M3_HOLE           = 3.4;      // tight clearance for M3 screw
SLOT_RANGE        = 4;        // VERTICAL slot length — lets you slide the PCB
                              // up/down ±2mm to align AS5600 chip with axle
                              // centerline. AIR GAP is tuned separately via
                              // M2 standoff length (see README Step 5c).

/* [Magnet clearance (visual only — not load-bearing)] */
MAGNET_D          = 6.2;      // diametric magnet diameter (with clearance)
MAGNET_T          = 2.6;      // magnet thickness

$fn = 72;

// ============================================================================
//  CALCULATED CONSTANTS
// ============================================================================
// PCB sits parallel to wheel face. AS5600 chip is on the PCB face that points
// toward the wheel. The chip face must be AIR_GAP from the magnet face.
// Bracket "sensor mount" is the wall on the BACK of the PCB.
// Distance from sensor-mount wall to wheel face = AIR_GAP + CHIP_HEIGHT + PCB_T

SENSOR_TO_WALL = AIR_GAP + CHIP_HEIGHT + PCB_T;  // back-of-PCB to wheel face

// ============================================================================
//  ANCHOR FLANGE (vertical plate against chassis side)
// ============================================================================
module anchor_flange() {
    difference() {
        // Body
        translate([-ANCHOR_W/2, 0, -ANCHOR_H/2])
            cube([ANCHOR_W, ANCHOR_T, ANCHOR_H]);

        // Two M3 mounting holes, vertically stacked
        for (dz = [-ANCHOR_HOLE_DY/2, ANCHOR_HOLE_DY/2])
            translate([0, -1, dz])
                rotate([-90, 0, 0])
                cylinder(d=M3_HOLE, h=ANCHOR_T + 2);

        // Counterbore relief (optional — for socket-head M3, depth 2mm)
        for (dz = [-ANCHOR_HOLE_DY/2, ANCHOR_HOLE_DY/2])
            translate([0, ANCHOR_T - 2, dz])
                rotate([-90, 0, 0])
                cylinder(d=6.0, h=2.5);
    }
}

// ============================================================================
//  CANTILEVER ARM — rises from anchor top to axle height, then turns laterally
// ============================================================================
module cantilever_arm() {
    // Vertical section (anchor top to axle height)
    translate([-BRACKET_T/2, ANCHOR_T, -ANCHOR_H/2])
        cube([BRACKET_T, BRACKET_T, ARM_RISE + ANCHOR_H/2 + BRACKET_T/2]);

    // Horizontal section (axle height extending laterally to wheel face)
    // Width tapers from full at the corner to reduced at the end
    hull() {
        translate([-BRACKET_T/2, ANCHOR_T, ARM_RISE - BRACKET_T/2])
            cube([BRACKET_T, BRACKET_T, BRACKET_T]);
        translate([-BRACKET_T/2, ANCHOR_T + ARM_REACH - SENSOR_TO_WALL - BRACKET_T,
                   ARM_RISE - BRACKET_T/2])
            cube([BRACKET_T, BRACKET_T, BRACKET_T]);
    }
}

// ============================================================================
//  GUSSET — triangulated reinforcement between anchor and arm
//  This is the single most important detail for stopping shake.
// ============================================================================
module gusset() {
    // Triangular gusset on the back side of the anchor/vertical-arm joint
    translate([-GUSSET_T/2, ANCHOR_T, -ANCHOR_H/2 + 1])
        rotate([0, 0, 0])
        linear_extrude(height = ARM_RISE + ANCHOR_H/2 - 2)
        polygon(points=[[0,0],[GUSSET_T, 0],[GUSSET_T, GUSSET_DEPTH]]);

    // Second gusset on the lateral corner (top of vertical, start of horizontal)
    translate([-GUSSET_T/2, ANCHOR_T, ARM_RISE - BRACKET_T/2])
        linear_extrude(height = BRACKET_T)
        polygon(points=[[0,0],[GUSSET_DEPTH, 0],[0, GUSSET_DEPTH]]);
}

// ============================================================================
//  SENSOR MOUNT — vertical wall at end of cantilever, holds AS5600 PCB
//  PCB attaches with 2 M2 screws + standoffs.
//  Slots are VERTICAL — they let you align the AS5600 chip with the axle
//  centerline (chip must be centered over the rotating magnet within 0.25mm).
//  Air gap (the perpendicular distance to the magnet) is set by choosing the
//  standoff length between the bracket wall and the PCB.
// ============================================================================
module sensor_mount() {
    mount_x = ANCHOR_T + ARM_REACH - SENSOR_TO_WALL;

    difference() {
        // Wall body — sized slightly larger than PCB for capture
        translate([-(PCB_W + 6)/2, mount_x - BRACKET_T, ARM_RISE - (PCB_H + 6)/2])
            cube([PCB_W + 6, BRACKET_T, PCB_H + 6]);

        // Two slotted M2 holes for PCB attachment
        // Slot runs along Y (perpendicular to wall — the air-gap direction)
        for (dx = [-PCB_HOLE_SPACING/2, PCB_HOLE_SPACING/2])
            translate([dx, mount_x - BRACKET_T - 1, ARM_RISE])
                rotate([-90, 0, 0])
                hull() {
                    translate([0, -SLOT_RANGE/2, 0])
                        cylinder(d=M2_HOLE, h=BRACKET_T + 2);
                    translate([0, SLOT_RANGE/2, 0])
                        cylinder(d=M2_HOLE, h=BRACKET_T + 2);
                }

        // Center window for AS5600 chip clearance (saves plastic, lets you see chip)
        translate([CHIP_OFFSET_X, mount_x - BRACKET_T - 1, ARM_RISE])
            rotate([-90, 0, 0])
            cylinder(d=8, h=BRACKET_T + 2);
    }
}

// ============================================================================
//  ASSEMBLED BRACKET
// ============================================================================
module bracket() {
    color([0.15, 0.55, 0.85]) {
        anchor_flange();
        cantilever_arm();
        gusset();
        sensor_mount();
    }
}

// ============================================================================
//  REFERENCE GHOSTS (preview only — F5; remove for STL export by setting SHOW_GHOSTS=false)
// ============================================================================
SHOW_GHOSTS = true;

module ghost_pcb() {
    mount_x = ANCHOR_T + ARM_REACH - SENSOR_TO_WALL;
    color([0.4, 0.2, 0.4, 0.5])
        translate([-PCB_W/2, mount_x, ARM_RISE - PCB_H/2])
        cube([PCB_W, PCB_T, PCB_H]);
    // AS5600 chip on outer face
    color([0.1, 0.1, 0.1, 0.7])
        translate([-3, mount_x + PCB_T, ARM_RISE - 3])
        cube([6, CHIP_HEIGHT, 6]);
}

module ghost_magnet() {
    mount_x = ANCHOR_T + ARM_REACH;
    color([0.7, 0.1, 0.1, 0.5])
        translate([0, mount_x, ARM_RISE])
        rotate([-90, 0, 0])
        cylinder(d=MAGNET_D, h=MAGNET_T);
}

module ghost_chassis() {
    color([0.2, 0.2, 0.2, 0.3])
        translate([-30, -3, -ANCHOR_H/2 - 5])
        cube([60, 3, ANCHOR_H + 10]);
}

// ============================================================================
//  MAIN
// ============================================================================
bracket();

if (SHOW_GHOSTS) {
    ghost_pcb();
    ghost_magnet();
    ghost_chassis();
}

// ============================================================================
//  PRINT ORIENTATION HELPER
//  When exporting STL for printing, ROTATE the bracket so the anchor flange
//  lies flat on the print bed. This makes the cantilever bending load
//  perpendicular to the layer direction = much stronger.
//
//    Slicer rotation: rotate -90° around the X axis (anchor face down).
// ============================================================================
