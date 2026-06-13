// ============================================================================
//  Pixy2 / Pixy2.1 — Camera Mount   (front-of-platform, fixed downtilt)
//  WRO Future Engineers 2026 · Team Faith
//
//  Drop-in replacement for openmv_h7_mount.scad after the OpenMV sensor module
//  was damaged (2026-06-12). Reuses the SAME base footprint, the SAME 15 deg
//  downtilt, and matches the OpenMV lens height so PIXY_FOCAL_PIX barely shifts.
//
//  Capture style: BOLT-THROUGH. The Pixy2 board (38.25 x 42 mm) bolts onto 3
//  printed standoff bosses with M2.5 self-tapping screws, using Pixy2's OWN
//  three mounting holes. Screws enter from the FRONT (lens side); all 3 holes
//  sit high on the board, the lens sits low, so the heads never touch the
//  optical path. The 10-pin ribbon hangs in the air gap behind the board and
//  exits through a slot in the bottom edge of the backplate.
//
//  OFFICIAL Pixy2 dimensions (docs.pixycam.com/wiki/v2:dimensions):
//      PCB        : 38.25 wide x 42 tall mm
//      Holes (3)  : 3.0 mm dia, board-corner-origin coords:
//                     (5.1, 31.3) (16.0, 38.8) (22.4, 38.8)
//      Lens center: (19.0, 5.0)
//  Pixy ships with 4-40 screws; M2.5 also passes the 3 mm holes cleanly.
//
//  COORDINATE FRAME (matches chassis.scad / openmv_h7_mount.scad):
//      +X = forward / direction of travel   (lens looks this way + down)
//      +Y = left
//      +Z = up      (base bottom sits at Z = 0 on the platform top)
//
//  >>> MEASURE YOUR BOARD FIRST <<<  Verify PCB_T, the connector block size,
//  and your built OpenMV mount's lens height (LENS_TARGET_Z) with calipers
//  before printing — same rule as the AS5600 bracket and the OpenMV mount.
//
//  Export: set SHOW_GHOSTS = false, press F6, File -> Export -> STL.
// ============================================================================

/* [Camera — Pixy2 / Pixy2.1, OFFICIAL dims] */
PCB_W = 38.25;  // board width  (horizontal in the mount)        — official
PCB_H = 42.0;   // board height (up the slope)                   — official
PCB_T = 1.6;    // PCB thickness                                 — VERIFY w/ calipers

// Mounting holes + lens, given in board-corner coords (official), origin at the
// board's bottom-left corner. Converted to board-CENTRE coords below.
HOLE1 = [ 5.1, 31.3];
HOLE2 = [16.0, 38.8];
HOLE3 = [22.4, 38.8];
LENS  = [19.0,  5.0];

/* [Aim + height — keep equal to the OpenMV mount] */
TILT_DEG      = 15;   // lens downtilt below horizontal (10..30 typical)
LENS_TARGET_Z = 40;   // world height of the lens centre, mm. = OpenMV lens height.
                      // MEASURE your built OpenMV mount and match this number.

/* [Bolt-through capture] */
STANDOFF  = 10.0; // gap board-back -> backplate. Must exceed the 10-pin
                  // connector's protrusion so the ribbon hangs free. Lower
                  // (~4) only if you fit a low-profile/right-angle header.
BOSS_D    = 6.0;  // boss outer diameter (stiffness)
PILOT_D   = 2.1;  // M2.5 self-tapping pilot. Use 3.4 if HEATSET = true.
HEATSET   = false; // true = widen pilot to take an M2.5 heat-set insert
BACK_T    = 3.5;  // backplate thickness
MARGIN    = 2.5;  // backplate extends this far beyond the board outline
BOTTOM_LIP = true; // small shelf under the board's bottom edge (gravity rest)
LIP_H     = 2.0;   // how far the shelf overhangs onto the board face
CABLE_W   = 14;    // width of the ribbon slot in the bottom edge

/* [Optional connector through-window — only if you shorten STANDOFF] */
CONN_WINDOW = false; // true = cut a rectangular window for the IDC connector
CONN_W      = 18;    // window width   — MEASURE the connector footprint
CONN_H      = 10;    // window height  — MEASURE
CONN_OFFY   = -4;    // window centre offset from board centre, up(+)/down(-)

/* [Pedestal + base — same footprint as openmv_h7_mount.scad] */
BASE_T      = 4.0;  // base plate thickness
BASE_MARGIN = 5.0;  // base extends this far beyond the backplate (left/right)
BASE_FRONT  = 6.0;  // base reaches this far FORWARD of the board centre
BASE_BACK   = 40.0; // base reaches this far BACK of the board centre
SCREW_D      = 3.4; // M3 clearance hole (chassis screws)
SCREW_HEAD_D = 6.6; // M3 countersink head diameter
SCREW_INSET  = 6.0; // screw centre distance from base edge

/* [Display] */
SHOW_GHOSTS = false;  // true = draw the Pixy board + lens cone + connector (DO NOT export)
$fn = 64;

// ---------------------------------------------------------------------------
//  Derived
// ---------------------------------------------------------------------------
CX = PCB_W/2;  CY = PCB_H/2;                 // board centre (corner-origin)
// hole + lens positions relative to board CENTRE (local board frame)
h1 = [HOLE1[0]-CX, HOLE1[1]-CY];
h2 = [HOLE2[0]-CX, HOLE2[1]-CY];
h3 = [HOLE3[0]-CX, HOLE3[1]-CY];
HOLES = [h1, h2, h3];
lens_xy = [LENS[0]-CX, LENS[1]-CY];          // ~ (0, -16): lens low, holes high

OUTER_W = PCB_W + 2*MARGIN;                  // backplate outline (local X)
OUTER_H = PCB_H + 2*MARGIN;                  // backplate outline (local Y)
PILOT   = HEATSET ? 3.4 : PILOT_D;

// place the board so the LENS lands at LENS_TARGET_Z.
// In place(): local +Y -> world +Z, so lens world Z ~= LIFT + lens_xy[1].
LIFT     = LENS_TARGET_Z - lens_xy[1];       // = 40 - (-16) = 56 by default
BASE_LEN = BASE_FRONT + BASE_BACK;
BASE_WID = OUTER_W + 2*BASE_MARGIN;
BASE_CX  = (BASE_FRONT - BASE_BACK)/2;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
module rrect(l, w, r) {                       // 2D rounded rectangle, centred
    hull() for (sx = [-1, 1], sy = [-1, 1])
        translate([sx*(l/2 - r), sy*(w/2 - r)]) circle(r = r);
}

// Place children from board-local frame into final pose:
//   local +Z (lens) -> forward + down ; local +Y (board top) -> up ;
//   local +X (board width) -> left.   (identical transform to OpenMV mount)
module place() {
    translate([0, 0, LIFT])
        rotate(TILT_DEG, [0, 1, 0])   // tilt lens downward
        rotate(120, [1, 1, 1])        // stand board up, width axis horizontal
        children();
}

// ---------------------------------------------------------------------------
//  Capture (built in local frame: board in XY at z=[STANDOFF, STANDOFF+PCB_T],
//  backplate at z=[-BACK_T, 0], lens toward +Z)
// ---------------------------------------------------------------------------
module capture() {
    difference() {
        union() {
            translate([0, 0, -BACK_T])
                linear_extrude(BACK_T) rrect(OUTER_W, OUTER_H, 2.5);
            for (h = HOLES)
                translate([h[0], h[1], 0]) cylinder(d = BOSS_D, h = STANDOFF);
            if (BOTTOM_LIP) {
                lipw = min(CABLE_W*2.2, PCB_W*0.7);
                // wall rising from backplate at the bottom edge
                translate([0, -CY + 1, 0])
                    linear_extrude(STANDOFF)
                        translate([-lipw/2, 0]) square([lipw, MARGIN]);
                // overhang lip catching the board face
                translate([0, -CY + 1, STANDOFF + PCB_T])
                    linear_extrude(LIP_H)
                        translate([-lipw/2, -1]) square([lipw, MARGIN + 1]);
            }
        }
        for (h = HOLES)
            translate([h[0], h[1], -BACK_T - 1])
                cylinder(d = PILOT, h = STANDOFF + PCB_T + BACK_T + 2);
        translate([0, -OUTER_H/2, (-BACK_T)/2])
            cube([CABLE_W, 2*MARGIN + 2, BACK_T + 2], center = true);
        if (CONN_WINDOW)
            translate([0, CONN_OFFY, -BACK_T - 1])
                cube([CONN_W, CONN_H, BACK_T + 2], center = true);
    }
}

// ---------------------------------------------------------------------------
//  Base plate (flat underside for VHB tape + countersunk M3 holes)
// ---------------------------------------------------------------------------
module base_plate() {
    difference() {
        translate([BASE_CX, 0, 0])
            linear_extrude(BASE_T) rrect(BASE_LEN, BASE_WID, 3);
        for (sx = [-1, 1], sy = [-1, 1])
            translate([BASE_CX + sx*(BASE_LEN/2 - SCREW_INSET),
                       sy*(BASE_WID/2 - SCREW_INSET), -1]) {
                cylinder(d = SCREW_D, h = BASE_T + 2);
                translate([0, 0, BASE_T - 1.6 + 1])
                    cylinder(d1 = SCREW_D, d2 = SCREW_HEAD_D, h = 1.6);
            }
        // chamfer the front edge so it never clips the downward field of view
        translate([BASE_CX + BASE_LEN/2, 0, BASE_T])
            rotate([0, 45, 0]) cube([6, BASE_WID + 2, 6], center = true);
    }
}

// ---------------------------------------------------------------------------
//  Support ramp — clean hull from a foot on the base up to the backplate
// ---------------------------------------------------------------------------
module support() {
    hull() {
        translate([-BASE_BACK*0.45, 0, BASE_T - 0.01])
            linear_extrude(0.02) rrect(BASE_BACK*0.7, OUTER_W*0.92, 3);
        place() translate([0, 0, -BACK_T + 0.01])
            linear_extrude(0.02) rrect(OUTER_W*0.92, OUTER_H*0.92, 2.5);
    }
}

// ---------------------------------------------------------------------------
//  Ghosts (visual only — never exported)
// ---------------------------------------------------------------------------
module ghost_camera() {
    place() {
        // PCB
        color([0.15, 0.45, 0.25, 0.85])
            translate([0, 0, STANDOFF]) linear_extrude(PCB_T) rrect(PCB_W, PCB_H, 2);
        // M12 lens barrel at the official lens position, pointing +Z
        color([0.1, 0.1, 0.1, 0.95])
            translate([lens_xy[0], lens_xy[1], STANDOFF + PCB_T]) cylinder(d = 14, h = 9);
        // FOV cone out of the lens
        color([1.0, 0.85, 0.1, 0.18])
            translate([lens_xy[0], lens_xy[1], STANDOFF + PCB_T + 9])
                cylinder(d1 = 6, d2 = 120, h = 150);
        // 10-pin IDC connector block on the board BACK (toward backplate)
        color([0.2, 0.2, 0.25, 0.9])
            translate([0, CONN_OFFY, STANDOFF - 8.5]) cube([18, 10, 8.5], center = false);
        // screw heads on the front face (should clear the lens)
        color([0.6, 0.6, 0.65, 0.95])
            for (h = HOLES)
                translate([h[0], h[1], STANDOFF + PCB_T]) cylinder(d = 5, h = 2);
    }
}

// ---------------------------------------------------------------------------
//  Assembly of the printable part
// ---------------------------------------------------------------------------
module pixy2_mount() {
    difference() {
        union() {
            base_plate();
            support();
            place() capture();
        }
        // guarantee a flat bottom on the platform (trim anything below Z=0)
        translate([0, 0, -50]) cube([500, 500, 100], center = true);
    }
}

// ---------------------------------------------------------------------------
//  Render
// ---------------------------------------------------------------------------
pixy2_mount();
if (SHOW_GHOSTS) ghost_camera();
