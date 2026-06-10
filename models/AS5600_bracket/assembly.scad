// ============================================================================
//  AS5600 Encoder Assembly — Bracket + Magnet Adapter
//  WRO Future Engineers 2026 · Team Faith
//
//  This file is a VISUAL ASSEMBLY ONLY. It imports the two real parts —
//    AS5600_bracket.scad   (chassis-side, blue)
//    magnet_adapter.scad   (wheel-side, dark grey)
//  — and places them in the relative position they sit in once installed
//  on the car. The yellow disc between the chip and the magnet is the
//  operating AIR_GAP (1.5 mm). Bracket is stationary; adapter rotates
//  with the wheel.
//
//  DO NOT EXPORT THIS TO STL FOR PRINTING. Export each source file
//  individually for slicing. This file is for sanity-checking alignment
//  in OpenSCAD's preview (F5).
// ============================================================================

use <AS5600_bracket.scad>
use <magnet_adapter.scad>

// ----------------------------------------------------------------------------
//  Constants — kept in sync with the source files. If you change them in
//  the bracket / adapter SCAD, mirror the change here so the assembly stays
//  aligned.
// ----------------------------------------------------------------------------
ANCHOR_T    = 4;     // bracket anchor flange thickness (Y)
ARM_REACH   = 28;    // chassis face -> wheel-side magnet plane (Y)
ARM_RISE    = 16;    // chassis bottom anchor center -> axle centerline (Z)
DISC_T      = 3.5;   // magnet adapter disc thickness
PCB_W       = 22;
PCB_H       = 18;
PCB_T       = 1.6;
CHIP_HEIGHT = 1.0;
AIR_GAP     = 1.5;

// The bracket places the chip face here; the magnet face must sit AIR_GAP
// further out. The original bracket "ghost_magnet" puts the magnet at
// Y = ANCHOR_T + ARM_REACH, so that's also where our real adapter must
// project its magnet pocket opening.
MAGNET_FACE_Y = ANCHOR_T + ARM_REACH;                          // = 32
SENSOR_TO_WALL = AIR_GAP + CHIP_HEIGHT + PCB_T;                // = 4.1
SENSOR_WALL_Y  = ANCHOR_T + ARM_REACH - SENSOR_TO_WALL;        // PCB back face
CHIP_FACE_Y    = SENSOR_WALL_Y + PCB_T + CHIP_HEIGHT;          // = 30.5

// ----------------------------------------------------------------------------
//  1) Bracket — at origin, chassis-side
// ----------------------------------------------------------------------------
bracket();

// ----------------------------------------------------------------------------
//  2) Magnet adapter — wheel-side, mounted on the rotation axis.
//
//  Adapter native frame:
//      disc lies on the XY plane (Z = 0 .. DISC_T)
//      magnet pocket opens toward +Z
//      magnet axis = Z axis
//
//  We need:
//      magnet axis  -> bracket Y axis (along chassis-to-wheel direction)
//      pocket opens toward -Y (back toward the AS5600 chip)
//      pocket face at Y = MAGNET_FACE_Y
//      axis crosses (X=0, Z=ARM_RISE)
//
//  rotate([90,0,0]) maps +Z -> -Y. After that, the pocket opening sits at
//  Y = -DISC_T, so we translate by (0, MAGNET_FACE_Y + DISC_T, ARM_RISE).
// ----------------------------------------------------------------------------
color([0.35, 0.35, 0.35])
    translate([0, MAGNET_FACE_Y + DISC_T, ARM_RISE])
    rotate([90, 0, 0])
    adapter();

// ----------------------------------------------------------------------------
//  3) Reference geometry (visual context only — not part of either print)
// ----------------------------------------------------------------------------

// PCB — sits on the front face of the sensor mount wall
color([0.45, 0.25, 0.5, 0.55])
    translate([-PCB_W/2, SENSOR_WALL_Y, ARM_RISE - PCB_H/2])
    cube([PCB_W, PCB_T, PCB_H]);

// AS5600 chip — sits on the wheel-facing side of the PCB
color([0.1, 0.1, 0.1, 0.9])
    translate([-3, SENSOR_WALL_Y + PCB_T, ARM_RISE - 3])
    cube([6, CHIP_HEIGHT, 6]);

// Air gap (yellow translucent disc) — what the encoder actually reads through
color([1.0, 0.85, 0.1, 0.45])
    translate([0, CHIP_FACE_Y, ARM_RISE])
    rotate([-90, 0, 0])
    cylinder(d=8, h=AIR_GAP, $fn=72);

// Wheel inner-face plane (transparent grey) — where the adapter glues on.
// NOTE: bracket places the magnet 4 mm beyond ARM_REACH, so on a real HSP
// 94182 wheel the adapter must sit inside a hub recess OR be glued onto a
// hub feature that protrudes ~4 mm past the inner face. Verify with calipers.
color([0.5, 0.5, 0.5, 0.15])
    translate([0, ARM_REACH, ARM_RISE])
    rotate([-90, 0, 0])
    cylinder(d=54, h=0.5, $fn=96);   // WHEEL_D = 54 mm (chassis.scad)
