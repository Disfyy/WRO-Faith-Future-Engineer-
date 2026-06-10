// ============================================================================
//  OpenMV Cam H7 Plus — Camera Mount   (front-of-platform, fixed downtilt)
//  WRO Future Engineers 2026 · Team Faith
//
//  Holds an OpenMV Cam H7 Plus on the FLAT TOP of the electronics box
//  (165 x 105 x 70 mm) at the FRONT edge, aimed forward and tilted DOWN by
//  TILT_DEG so the lens sees the track + pillars ahead of the car.
//
//  Capture style: EDGE-CAPTURE CRADLE — the 45 x 36 mm board drops into a
//  pocket and is held by retention lips on the two sides + top edge. The
//  bottom edge is open (slide the board in bottom-first, snap the top under
//  the top lip). No reliance on the board's M2.5 hole spacing (OpenMV does
//  not publish it).
//
//  COORDINATE FRAME (matches chassis.scad):
//      +X = forward / direction of travel   (lens looks this way + down)
//      +Y = left
//      +Z = up      (base bottom sits at Z = 0 on the platform top)
//
//  >>> MEASURE YOUR BOARD FIRST <<<  Verify CAM_L / CAM_W / PCB_T with
//  calipers before printing — same rule as the AS5600 bracket.
//
//  Export: set SHOW_GHOSTS = false, press F6, File -> Export -> STL.
// ============================================================================

/* [Camera — OpenMV Cam H7 Plus] */
CAM_L   = 45.0;   // PCB long axis  (HORIZONTAL in the mount)   — VERIFY w/ calipers
CAM_W   = 36.0;   // PCB short axis (UP the slope)              — VERIFY w/ calipers
PCB_T   = 1.6;    // PCB thickness                              — VERIFY w/ calipers

/* [Aim] */
TILT_DEG = 15;    // lens downtilt below horizontal. Re-print to tune (10..30 typical)

/* [Cradle capture] */
BOARD_FIT = 0.30; // clearance added around the board inside the pocket
WALL      = 2.4;  // pocket wall thickness
BACK_T    = 3.0;  // backplate thickness (board back rests on this)
LIP_GRAB  = 2.0;  // how far retention lips overhang onto the board face
LIP_T     = 1.6;  // lip thickness (the bit sitting over the board)
CABLE_W   = 14;   // width of the slot in the bottom edge for header wires / USB

/* [Pedestal + base] */
PEDESTAL_H  = 10;   // gap from base top to the bottom edge of the board (cable room)
BASE_T      = 4.0;  // base plate thickness
BASE_MARGIN = 5.0;  // base extends this far beyond the cradle (left/right)
BASE_FRONT  = 6.0;  // base reaches this far FORWARD of the board centre (kept short)
BASE_BACK   = 40.0; // base reaches this far BACK of the board centre (support + screws)
SCREW_D      = 3.4; // M3 clearance hole
SCREW_HEAD_D = 6.6; // M3 countersink head diameter
SCREW_INSET  = 6.0; // screw centre distance from base edge

/* [Optional bolt-through holes — only if you measured the M2.5 pattern] */
BOLT_HOLES   = false; // true = add 4x M2.5 holes through the backplate
BOLT_D       = 2.7;   // M2.5 clearance
BOLT_SP_L    = 38;    // hole spacing along board long axis  — MEASURE if you use this
BOLT_SP_W    = 30;    // hole spacing along board short axis — MEASURE if you use this

/* [Display] */
SHOW_GHOSTS = false;  // true = also draw the OpenMV board + lens FOV cone (DO NOT export)
$fn = 64;

// ---------------------------------------------------------------------------
//  Derived
// ---------------------------------------------------------------------------
POCKET_L = CAM_L + 2*BOARD_FIT;          // pocket inner, long axis
POCKET_W = CAM_W + 2*BOARD_FIT;          // pocket inner, short axis
OUTER_L  = POCKET_L + 2*WALL;            // cradle outer, long axis (-> becomes Y span)
OUTER_W  = POCKET_W + 2*WALL;            // cradle outer, short axis
// lift the board centre so its bottom edge clears base + pedestal
LIFT     = BASE_T + PEDESTAL_H + (CAM_W/2)*cos(TILT_DEG) + WALL;
BASE_LEN = BASE_FRONT + BASE_BACK;       // total base length in X
BASE_WID = OUTER_L + 2*BASE_MARGIN;      // base width in Y
BASE_CX  = (BASE_FRONT - BASE_BACK)/2;   // base centre X (base extends back)

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
module rrect(l, w, r) {                   // 2D rounded rectangle, centred
    hull() for (sx = [-1, 1], sy = [-1, 1])
        translate([sx*(l/2 - r), sy*(w/2 - r)]) circle(r = r);
}

// Place children from cradle-local frame into final pose:
//   local +Z (lens) -> forward + down ;  local +Y (board top) -> up & forward
module place() {
    translate([0, 0, LIFT])
        rotate(TILT_DEG, [0, 1, 0])   // tilt lens downward
        rotate(120, [1, 1, 1])        // stand board up, long axis horizontal
        children();
}

// ---------------------------------------------------------------------------
//  Cradle (built in local frame: board in XY, lens toward +Z, back toward -Z)
// ---------------------------------------------------------------------------
module cradle() {
    difference() {
        union() {
            // backplate — board back rests on this
            translate([0, 0, -BACK_T])
                linear_extrude(BACK_T) rrect(OUTER_L, OUTER_W, 2);

            // pocket walls (board sits between them)
            linear_extrude(PCB_T)
                difference() {
                    rrect(OUTER_L, OUTER_W, 2);
                    rrect(POCKET_L, POCKET_W, 1.5);
                }

            // retention lips on TOP + LEFT + RIGHT edges (bottom left open)
            translate([0, 0, PCB_T])
                linear_extrude(LIP_T)
                    difference() {
                        rrect(OUTER_L, OUTER_W, 2);
                        rrect(POCKET_L - 2*LIP_GRAB, POCKET_W - 2*LIP_GRAB, 1.5);
                        // open the bottom lip for board insertion
                        translate([0, -OUTER_W/2])
                            square([OUTER_L + 1, 2*(WALL + LIP_GRAB) + 1], center = true);
                    }
        }
        // cable / USB slot through the bottom edge
        translate([0, -OUTER_W/2, (-BACK_T + PCB_T + LIP_T)/2])
            cube([CABLE_W, 2*WALL + 2, BACK_T + PCB_T + LIP_T + 2], center = true);

        // optional bolt-through holes
        if (BOLT_HOLES)
            for (sx = [-1, 1], sy = [-1, 1])
                translate([sx*BOLT_SP_L/2, sy*BOLT_SP_W/2, -BACK_T - 1])
                    cylinder(d = BOLT_D, h = BACK_T + PCB_T + LIP_T + 2);
    }
}

// ---------------------------------------------------------------------------
//  Base plate (flat underside for VHB tape + countersunk M3 holes)
// ---------------------------------------------------------------------------
module base_plate() {
    difference() {
        translate([BASE_CX, 0, 0])
            linear_extrude(BASE_T) rrect(BASE_LEN, BASE_WID, 3);

        // countersunk M3 holes near the four corners
        for (sx = [-1, 1], sy = [-1, 1])
            translate([BASE_CX + sx*(BASE_LEN/2 - SCREW_INSET),
                       sy*(BASE_WID/2 - SCREW_INSET), -1]) {
                cylinder(d = SCREW_D, h = BASE_T + 2);
                translate([0, 0, BASE_T - 1.6 + 1])    // countersink from top
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
            linear_extrude(0.02) rrect(BASE_BACK*0.7, OUTER_L*0.92, 3);
        place() translate([0, 0, -BACK_T + 0.01])
            linear_extrude(0.02) rrect(OUTER_L*0.92, OUTER_W*0.92, 2);
    }
}

// ---------------------------------------------------------------------------
//  Ghosts (visual only — never exported)
// ---------------------------------------------------------------------------
module ghost_camera() {
    place() {
        // PCB
        color([0.15, 0.45, 0.25, 0.85])
            translate([0, 0, 0]) linear_extrude(PCB_T) rrect(CAM_L, CAM_W, 2);
        // M12 lens barrel (near top-centre), pointing +Z (lens direction)
        color([0.1, 0.1, 0.1, 0.95])
            translate([0, 6, PCB_T]) cylinder(d = 14, h = 9);
        color([0.2, 0.2, 0.2, 0.95])
            translate([0, 6, PCB_T + 9]) cylinder(d = 11, h = 3);
        // FOV cone out of the lens
        color([1.0, 0.85, 0.1, 0.18])
            translate([0, 6, PCB_T + 12]) cylinder(d1 = 6, d2 = 120, h = 150);
    }
}

// ---------------------------------------------------------------------------
//  Assembly of the printable part
// ---------------------------------------------------------------------------
module openmv_h7_mount() {
    difference() {
        union() {
            base_plate();
            support();
            place() cradle();
        }
        // guarantee a flat bottom on the platform (trim anything below Z=0)
        translate([0, 0, -50]) cube([500, 500, 100], center = true);
    }
}

// ---------------------------------------------------------------------------
//  Render
// ---------------------------------------------------------------------------
openmv_h7_mount();
if (SHOW_GHOSTS) ghost_camera();
