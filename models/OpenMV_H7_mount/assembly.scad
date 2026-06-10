// ============================================================================
//  OpenMV Cam H7 Plus — INSTALLATION ASSEMBLY  (visual only, do NOT print)
//  WRO Future Engineers 2026 · Team Faith
//
//  Shows the camera mount + a ghost OpenMV board + lens field-of-view cone
//  sitting on the FRONT of the electronics platform (the 165 x 105 x 70 mm
//  box that is mounted on the HSP94182 chassis). Use this to sanity-check
//  placement and aim in OpenSCAD's preview (F5).
//
//  The printable part lives in openmv_h7_mount.scad — export THAT for slicing.
//
//  Frame: +X forward, +Y left, +Z up. Platform TOP face is at Z = 0.
// ============================================================================

use <openmv_h7_mount.scad>

// ---- platform box (the cube the camera installs on) ------------------------
BOX_L = 165;   // length (X, fore-aft)
BOX_W = 105;   // width  (Y)
BOX_H = 70;    // height (Z)

MOUNT_X = 60;  // how far forward of box centre the mount sits (lens near front edge)

// ---- platform box: top face at Z = 0, centred in XY ------------------------
color([0.28, 0.30, 0.34])
    translate([0, 0, -BOX_H])
        linear_extrude(BOX_H)
            offset(r = 3) square([BOX_L - 6, BOX_W - 6], center = true);

// front-edge marker line (where the box ends and the track opens up)
color([0.9, 0.4, 0.2])
    translate([BOX_L/2, 0, 0.2])
        cube([1.2, BOX_W, 0.6], center = true);

// ---- mount (printable part) + ghost camera + FOV ---------------------------
translate([MOUNT_X, 0, 0]) {
    color([0.85, 0.75, 0.15]) openmv_h7_mount();
    ghost_camera();
}
