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

// Lateral arm vertical depth (Z height). Bending stiffness ~ depth^3, so
// going from 4mm to 10mm gives ~16x more rigidity. Don't make taller than
// the gap between top deck and suspension top — measure first.
ARM_H             = 10;

// Triangular gusset depth (kills first-mode resonance)
GUSSET_DEPTH      = 14;
GUSSET_T          = 3;

/* [Stability enhancements (v1.1 — convert cantilever into a truss)] */

// Diagonal strut from sensor mount back down to the anchor.
// This is the single biggest rigidity improvement — turns a flexing
// cantilever into a rigid triangle. Enable unless it interferes with
// something on your specific car.
STRUT_ENABLE      = true;
STRUT_W           = 4;        // strut width (X — along car length)
STRUT_T           = 3;        // strut thickness (perpendicular to strut axis)

// Mirror gusset on the OPPOSITE side of the anchor-to-riser corner.
// With gussets on both sides the riser becomes truly rigid in torsion.
DOUBLE_GUSSET     = true;

// Back-buttress on the sensor mount — small horizontal rib that prevents
// the sensor wall from rocking under cable pull or shake.
BACK_BUTTRESS     = true;
BUTTRESS_DEPTH    = 8;

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
//  Lateral arm uses ARM_H for vertical depth (bending stiffness scales with
//  depth cubed — 4mm -> 10mm is ~16x stiffer).
// ============================================================================
module cantilever_arm() {
    // Vertical section (anchor top to axle height)
    translate([-BRACKET_T/2, ANCHOR_T, -ANCHOR_H/2])
        cube([BRACKET_T, BRACKET_T, ARM_RISE + ANCHOR_H/2 + ARM_H/2]);

    // Horizontal section (deep section in Z for bending rigidity)
    hull() {
        translate([-BRACKET_T/2, ANCHOR_T, ARM_RISE - ARM_H/2])
            cube([BRACKET_T, BRACKET_T, ARM_H]);
        translate([-BRACKET_T/2, ANCHOR_T + ARM_REACH - SENSOR_TO_WALL - BRACKET_T,
                   ARM_RISE - ARM_H/2])
            cube([BRACKET_T, BRACKET_T, ARM_H]);
    }
}

// ============================================================================
//  GUSSET — triangulated reinforcement between anchor and arm.
//  With DOUBLE_GUSSET=true a mirror copy on the opposite X side makes the
//  riser rigid in torsion (not just bending).
// ============================================================================
// One vertical YZ-plane gusset (triangular fillet) placed at the inside
// corner where the lateral arm meets the riser. Polygon uses
// (-Z_global, Y_global) so rotate([0,90,0]) makes it a vertical plate
// extruded along X.
module gusset_yz(z_corner, y_corner, y_far, z_far, thickness) {
    translate([-thickness/2, 0, 0])
        rotate([0, 90, 0])
        linear_extrude(height = thickness)
        polygon(points=[
            [-z_corner, y_corner],      // right-angle vertex (inside corner)
            [-z_far,    y_corner],      // along riser (vertical leg)
            [-z_corner, y_far]          // along arm (horizontal leg)
        ]);
}

module gusset() {
    // Lower fillet — below the arm, supporting the bottom of the riser-to-arm
    // corner. Right angle at the inside corner (front of riser, bottom of arm).
    riser_front = ANCHOR_T + BRACKET_T - 0.5;       // 0.5mm into riser for merge
    arm_bot     = ARM_RISE - ARM_H/2;
    gusset_yz(
        z_corner = arm_bot,
        y_corner = riser_front,
        y_far    = riser_front + GUSSET_DEPTH,
        z_far    = arm_bot - GUSSET_DEPTH,
        thickness= GUSSET_T
    );

    if (DOUBLE_GUSSET) {
        // Upper fillet — above the arm, mirror of the lower one
        arm_top = ARM_RISE + ARM_H/2;
        gusset_yz(
            z_corner = arm_top,
            y_corner = riser_front,
            y_far    = riser_front + GUSSET_DEPTH,
            z_far    = arm_top + GUSSET_DEPTH,
            thickness= GUSSET_T
        );
    }
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
//  DIAGONAL STRUT — converts the cantilever into a triangulated truss.
//  Goes from the top of the sensor mount BACK and DOWN to the bottom of the
//  anchor flange, in the same XZ plane as the main arm. The free-cantilever
//  bending load on the sensor mount becomes axial tension in the strut,
//  which any decent material handles ~50x more efficiently than bending.
// ============================================================================
module diagonal_strut() {
    // Top end embeds in the sensor wall; bottom end embeds in the RISER (not
    // the anchor face). Routing through the riser keeps the M3 screw holes
    // clear and avoids an ugly 1mm protrusion behind the anchor.
    mount_x      = ANCHOR_T + ARM_REACH - SENSOR_TO_WALL;
    sensor_top_z = ARM_RISE + PCB_H/2;             // near top of PCB area
    riser_bot_z  = -ANCHOR_H/2 + STRUT_T/2 + 2;    // 2mm above anchor bottom

    hull() {
        // Top end — passes all the way through the sensor wall
        translate([-STRUT_W/2, mount_x - BRACKET_T - 1, sensor_top_z - STRUT_T/2])
            cube([STRUT_W, BRACKET_T + 2, STRUT_T]);
        // Bottom end — passes all the way through the riser (Y = ANCHOR_T..ANCHOR_T+BRACKET_T)
        translate([-STRUT_W/2, ANCHOR_T - 1, riser_bot_z - STRUT_T/2])
            cube([STRUT_W, BRACKET_T + 2, STRUT_T]);
    }
}

// ============================================================================
//  BACK BUTTRESSES — small triangular gussets that fill the upper and lower
//  corners where the wall meets the arm. The wall sticks above and below the
//  arm by ~7 mm on each side (because PCB_H > ARM_H), and those free
//  extensions flex under cable pull or vibration. Buttresses anchor them.
// ============================================================================
module back_buttress_one(z_arm, z_wall) {
    // Triangle in YZ plane, thickness BRACKET_T in X.
    // Polygon defined in local (X, Y) where local X = -Z_global and local Y = Y_global,
    // then linear_extrude along local Z, then rotate so local Z aligns with global X.
    mount_y = ANCHOR_T + ARM_REACH - SENSOR_TO_WALL - BRACKET_T;
    translate([-BRACKET_T/2, 0, 0])
        rotate([0, 90, 0])
        linear_extrude(height = BRACKET_T)
        polygon(points=[
            [-z_arm,  mount_y],                   // arm bottom/top at wall (right angle)
            [-z_wall, mount_y],                   // wall edge (along wall back)
            [-z_arm,  mount_y - BUTTRESS_DEPTH]   // along arm bottom/top
        ]);
}

module back_buttress() {
    arm_bot  = ARM_RISE - ARM_H/2;
    arm_top  = ARM_RISE + ARM_H/2;
    wall_bot = ARM_RISE - (PCB_H + 6)/2 + 1;   // 1mm above true wall bottom
    wall_top = ARM_RISE + (PCB_H + 6)/2 - 1;   // 1mm below true wall top
    back_buttress_one(arm_bot, wall_bot);      // lower buttress
    back_buttress_one(arm_top, wall_top);      // upper buttress
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
        if (STRUT_ENABLE) diagonal_strut();
        if (BACK_BUTTRESS) back_buttress();
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
