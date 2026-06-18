// ============================================================================
//  Pixy2 / Pixy2.1 — TILT-ADJUSTABLE COVER + CLEVIS BRACKET
//  WRO Future Engineers 2026 · Team Faith
//
//  A box COVER encloses the Pixy2 board (lens window in front, top access for
//  the button, open back for board insertion + the 10-pin ribbon). The cover
//  hangs on a CLEVIS HINGE at its bottom-front: a barrel on the cover sits
//  between two ears on an L-BRACKET, pinned by one M3 screw. Loosen the M3,
//  swing the cover to the downtilt you want, tighten to friction-lock.
//  => adjustable replacement for the fixed-15deg models/Pixy2_mount.
//
//  Two printable parts:
//    - COVER   (SHOW = "cover")    : the camera box + hinge barrel
//    - BRACKET (SHOW = "bracket")  : the L-foot + clevis ears
//  Assemble with 1x M3x20 + nyloc (pivot) and 3x M2.5 self-tap (board->bosses).
//
//  OFFICIAL Pixy2 dims (docs.pixycam.com/wiki/v2:dimensions):
//    PCB 38.25 x 42 mm ; holes (corner origin) (5.1,31.3)(16,38.8)(22.4,38.8) ;
//    lens centre (19,5).
//
//  FRAME (assembly): +X forward (lens), +Y left, +Z up, chassis top at Z=0.
//  >>> MEASURE: PCB_T, lens-barrel length, connector depth before printing. <<<
// ============================================================================

/* [What to render] */
SHOW = "assembly";   // "assembly" | "cover" | "bracket"
SHOW_GHOSTS = false; // assembly only: draw the Pixy board + lens cone
$fn = 48;

/* [Camera — Pixy2 / Pixy2.1, OFFICIAL] */
PCB_W = 38.25; // width  -> cover Y
PCB_H = 42.0;  // height -> cover Z
PCB_T = 1.6;   // thickness                              — VERIFY
LENS_BARREL = 9;   // how far the lens sticks out the board front — VERIFY
CONN_DEPTH  = 11;  // how far the 10-pin connector sticks out the back — VERIFY

// holes + lens, board-corner origin -> board-centre (then board-x->Y, board-y->Z)
HOLE1=[5.1,31.3]; HOLE2=[16.0,38.8]; HOLE3=[22.4,38.8]; LENSXY=[19.0,5.0];

/* [Cover box] */
CLR   = 1.5;   // clearance around the board
WALL  = 2.2;   // shell wall thickness
BOSS_STANDOFF = 4.0;  // board front sits this far behind the front wall inner face
BOSS_D  = 6.0;
PILOT_D = 2.1; // M2.5 self-tap pilot
WIN_HALF = 8.0; // lens window half-size (clears the 14 mm lens barrel)
TOP_SLOT_W = 10; TOP_SLOT_L = 5;   // button access slot on the top wall
BACK_FRAME = 4.0; // width of the frame left around the open back

/* [Clevis hinge] */
BARREL_R   = 5.0;  // hinge barrel radius (bigger = more friction-lock torque)
BARREL_L   = 16.0; // barrel length along Y (sits between the bracket ears)
PIN_D      = 3.4;  // M3 clearance through barrel + ears
EAR_T      = 3.5;  // bracket ear thickness
EAR_GAP    = 0.5;  // clearance each side between barrel end and ear
PIVOT_Z    = 30.0; // pivot-axis height above the chassis (assembly)
PIVOT_X    = 0.0;  // pivot-axis fwd/back position (assembly)

/* [L-bracket foot] */
FOOT_T      = 4.0;
FOOT_FRONT  = 5;    // foot reaches this far FORWARD of the arm
FOOT_BACK   = 34;   // foot reaches this far BACK of the arm (stability + screws)
FOOT_WID    = BARREL_L + 2*(EAR_GAP + EAR_T) + 6;
SCREW_D     = 3.4;  // M3 chassis clearance
SCREW_HEAD_D= 6.6;  // countersink
SCREW_X1    = -11;  // chassis screw positions (both BEHIND the arm at x=0)
SCREW_X2    = -27;

/* [Assembly pose] */
TILT_DEG = 15;  // demo downtilt for the assembly render (real angle is adjustable)

// ---------------------------------------------------------------------------
//  Derived
// ---------------------------------------------------------------------------
CX=PCB_W/2; CY=PCB_H/2;
hY=[HOLE1[0]-CX, HOLE2[0]-CX, HOLE3[0]-CX];   // hole Y (cover frame)
hZ=[HOLE1[1]-CY, HOLE2[1]-CY, HOLE3[1]-CY];   // hole Z
lensY = LENSXY[0]-CX;  lensZ = LENSXY[1]-CY;  // ~ (0, -16)

IN_W = PCB_W + 2*CLR;  OUT_W = IN_W + 2*WALL;     // cover Y span
IN_H = PCB_H + 2*CLR;  OUT_H = IN_H + 2*WALL;     // cover Z span
DEPTH = WALL + BOSS_STANDOFF + PCB_T + CONN_DEPTH + 2;  // cover X span (open back at x=0)
FRONT_IN = DEPTH - WALL;                 // front wall inner face (x)
BOARD_FRONT_X = FRONT_IN - BOSS_STANDOFF;// board front (lens) face x
BARREL_X = DEPTH - BARREL_R - 1;         // barrel sits near the front, under the box

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
module rrect(l,w,r){ hull() for(sx=[-1,1],sy=[-1,1]) translate([sx*(l/2-r),sy*(w/2-r)]) circle(r=r); }

// ---------------------------------------------------------------------------
//  COVER — built in its own frame: back-opening plane at x=0, front wall at
//  x=DEPTH, lens looks +X, board centred on Y/Z=0, hinge barrel under the box.
// ---------------------------------------------------------------------------
module cover() {
    difference() {
        union() {
            // 5-sided shell (open back at x=0)
            difference() {
                translate([0,-OUT_W/2,-OUT_H/2]) cube([DEPTH, OUT_W, OUT_H]);
                // hollow interior
                translate([-1,-IN_W/2,-IN_H/2]) cube([FRONT_IN+1, IN_W, IN_H]);
            }
            // 3 board bosses rising from the front wall inner face toward -X
            for(i=[0:2])
                translate([BOARD_FRONT_X, hY[i], hZ[i]])
                    rotate([0,90,0]) cylinder(d=BOSS_D, h=BOSS_STANDOFF);
            // hinge barrel under the box (axis along Y), webbed to the bottom
            translate([BARREL_X, 0, -OUT_H/2])
                rotate([90,0,0]) cylinder(d=2*BARREL_R, h=BARREL_L, center=true);
            // web connecting barrel to the box bottom
            hull(){
                translate([BARREL_X,0,-OUT_H/2]) rotate([90,0,0]) cylinder(d=2*BARREL_R, h=BARREL_L, center=true);
                translate([BARREL_X-BARREL_R, -BARREL_L/2, -OUT_H/2]) cube([2*BARREL_R, BARREL_L, 1]);
            }
        }
        // lens window in the front wall (centred on the lens)
        translate([FRONT_IN+WALL/2, lensY, lensZ])
            cube([WALL+2, 2*WIN_HALF, 2*WIN_HALF], center=true);
        // top button-access slot (over the board top edge)
        translate([BOARD_FRONT_X, 0, OUT_H/2-WALL/2])
            cube([TOP_SLOT_L, TOP_SLOT_W, WALL+2], center=true);
        // board-mount pilots — drilled INTO each boss from the front-wall side
        // (blind: they do not pierce the exterior front face)
        for(i=[0:2])
            translate([FRONT_IN+0.5, hY[i], hZ[i]]) rotate([0,-90,0])
                cylinder(d=PILOT_D, h=BOSS_STANDOFF+2);
        // pin hole through the barrel
        translate([BARREL_X,0,-OUT_H/2]) rotate([90,0,0]) cylinder(d=PIN_D, h=BARREL_L+2, center=true);
        // cable relief at the bottom-back edge
        translate([0,-TOP_SLOT_W,-OUT_H/2-1]) cube([WALL+2, 2*TOP_SLOT_W, WALL+2]);
    }
}

// ---------------------------------------------------------------------------
//  BRACKET — L-foot (flat on chassis) + clevis ears that straddle the barrel.
//  Built so the pivot axis is at (x=PIVOT_X, z=PIVOT_Z), foot bottom at z=0.
// ---------------------------------------------------------------------------
module bracket() {
    earY = BARREL_L/2 + EAR_GAP + EAR_T/2;   // ear centre offset in Y
    difference() {
        union() {
            // foot (extends back from the arm at x=0)
            translate([PIVOT_X + (FOOT_FRONT-FOOT_BACK)/2, 0, FOOT_T/2])
                cube([FOOT_FRONT+FOOT_BACK, FOOT_WID, FOOT_T], center=true);
            // two clevis ears rising to the pivot
            for(s=[-1,1])
                translate([0, s*earY, 0])
                    hull(){
                        translate([PIVOT_X,0,PIVOT_Z]) rotate([90,0,0]) cylinder(d=2*BARREL_R+3, h=EAR_T, center=true);
                        translate([PIVOT_X-BARREL_R-1.5, -EAR_T/2, FOOT_T-0.01]) cube([2*BARREL_R+3, EAR_T, 1]);
                    }
        }
        // pivot hole through both ears
        translate([PIVOT_X,0,PIVOT_Z]) rotate([90,0,0]) cylinder(d=PIN_D, h=FOOT_WID+2, center=true);
        // chassis countersunk screws in the foot (both behind the arm)
        for(sxx=[SCREW_X1, SCREW_X2])
            translate([PIVOT_X + sxx, 0, -1]){
                cylinder(d=SCREW_D, h=FOOT_T+2);
                translate([0,0,FOOT_T-1.6+1]) cylinder(d1=SCREW_D, d2=SCREW_HEAD_D, h=1.6);
            }
    }
}

// ---------------------------------------------------------------------------
//  Pose the cover onto the pivot for the assembly view
// ---------------------------------------------------------------------------
module place_cover() {
    translate([PIVOT_X,0,PIVOT_Z])
        rotate(TILT_DEG,[0,1,0])
        translate([-BARREL_X,0, OUT_H/2])   // bring barrel centre to origin
        children();
}

module ghost_pixy() {
    place_cover() {
        color([0.15,0.45,0.25,0.85])
            translate([BOARD_FRONT_X-PCB_T,-PCB_W/2,-PCB_H/2]) cube([PCB_T,PCB_W,PCB_H]);
        color([0.1,0.1,0.1,0.95])
            translate([BOARD_FRONT_X,lensY,lensZ]) rotate([0,90,0]) cylinder(d=14,h=LENS_BARREL);
        color([1,0.85,0.1,0.18])
            translate([BOARD_FRONT_X+LENS_BARREL,lensY,lensZ]) rotate([0,90,0]) cylinder(d1=6,d2=120,h=150);
    }
}

// ---------------------------------------------------------------------------
//  Render
// ---------------------------------------------------------------------------
if (SHOW == "cover") {
    cover();
} else if (SHOW == "bracket") {
    bracket();
} else {
    bracket();
    place_cover() cover();
    if (SHOW_GHOSTS) ghost_pixy();
}
