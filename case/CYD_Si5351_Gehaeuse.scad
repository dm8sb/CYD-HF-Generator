// =====================================================================
//  Gehäuse für "Cheap Yellow Display" (ESP32-2432S028, USB-C-Version)
//  + CJMCU-5351 (Si5351A) mit 3 direkt aufgelöteten Edge-Mount-SMA-Buchsen
//
//  Maße der CYD aus der STEP-Datei "Cheap Yellow Display by mbrembati"
//  Koordinaten: Ursprung = linke untere Ecke der CYD-Platine,
//               z = 0 ist die Platinen-UNTERSEITE, Display zeigt nach +z.
//
//  Teile:  part = "top"      -> Oberteil (Displayfenster, USB-C-Öffnung)
//          part = "bottom"   -> Unterteil (SMA-Durchbrüche, Si5351-Halter)
//          part = "assembly" -> Vorschau mit Dummy-Platinen
//          part = "explode"  -> Vorschau auseinandergezogen
//          part = "print"    -> beide Teile druckfertig nebeneinander
//
//  Montage CYD (closure = "snap"): CYD aufs Unterteil legen, Oberteil
//  aufdrücken bis alle 4 Rasthaken einrasten. Die CYD wird zwischen den
//  Rohren (unten) und den Domen mit Zentrierstiften (oben) geklemmt.
//  Öffnen: Haken durch die Fenster mit kleinem Schraubendreher eindrücken.
//  (closure = "screw": 4x M2.5x30 Senkkopf von unten durch Rohr -> Dom)
//
//  Montage Si5351 (rail): Platine flach hinter den Führungsschienen ablegen,
//  zur SMA-Wand schieben (Gewinde durch die Bohrungen), bis die SMA-Gehäuse
//  an der Wand anliegen. Außen die SMA-Muttern festziehen.
//  (screw: Platine auf die Schraubdome setzen und verschrauben)
// =====================================================================

part = "print";      // [print, top, bottom, assembly, explode]
bottom_rot = 0;      // [0, 180] nur Vorschau: Unterteil gedreht -> SMA-Buchsen oben bzw. unten

/* ---------- Verschluss ---------- */
closure = "snap";    // "snap" = Rasthaken, schraubenlos | "screw" = 4x M2.5
// 4 Rasthaken, punktsymmetrisch zur Gehäusemitte (Unterteil 180° drehbar):
// je 2 an beiden Längswänden, gespiegelt zur Mitte.
// Die Arme hängen am Oberteil nach unten, laufen in einer Tasche in der Wand
// des Unterteils und rasten unten in ein Fenster ein. Alle Überhänge 45°,
// damit beide Teile ohne Stützmaterial druckbar sind.
snap_dx   = 27.8;    // Abstand der Haken von der Mitte (X)
snap_w    = 10;      // Hakenbreite
snap_t    = 1.6;     // Armdicke am Haken
snap_t0   = 2.2;     // Armdicke an der Wurzel (verjüngter Arm -> Biegung gleichmäßig verteilt)
snap_in   = 0.7;     // so weit liegt der Arm in der Wandstärke (Tasche im Unterteil)
snap_clr  = 0.3;     // Luft Arm <-> Tasche
snap_tip  = 0.2;     // Haken endet so weit vor der Außenfläche
snap_z    = -18;     // Rastkante (oberster Punkt der Rastschräge am Arm)
snap_play = -0.15;   // negativ = Vorspannung: Haken zieht die Hälften spielfrei zusammen
pin_d     = 2.6;     // Zentrierstift im Oberteil-Dom (CYD-Bohrung 3.0)

/* ---------- CYD (aus STEP gemessen) ---------- */
pcb_l   = 85.6;      // X
pcb_w   = 50.0;      // Y
pcb_t   = 1.6;
disp_top = 6.1;      // Oberkante Displayglas über Platinen-Unterseite
holes   = [[3.68, 4.06], [81.93, 4.06], [3.68, 45.93], [81.93, 45.93]];
act_x0 = 11.51; act_x1 = 69.11;      // sichtbare Displayfläche
act_y0 = 4.00;  act_y1 = 47.00;
usbc_y  = 22.92;     // USB-C Mitte in Y (rechte Schmalseite)
usbc_z  = -1.73;     // USB-C Mitte in Z
back_clear = 4.5;    // höchstes Bauteil auf der CYD-Rückseite

/* ---------- Si5351 (CJMCU-5351) – bitte nachmessen! ---------- */
si_l    = 30.0;      // Länge der SMA-Kante (X)
si_w    = 26.0;      // Tiefe (Y)
si_t    = 1.6;
si_x0   = pcb_l/2 - si_l/2;  // mittig im Gehäuse
si_sma  = [5, 15, 25];       // SMA-Mitten ab linker Platinenkante (10 mm Raster)
sma_body = 1.0;      // SMA-Gehäuse steht so weit über die Platinenkante (liegt an der Wand an)
sma_axis_off = 0.7;  // SMA-Achse über Platinen-OBERSEITE (Edge-Mount)
si_mount = "rail";   // "rail" = Schiebeführung (ohne Federteile), "screw" = Schraubdome
si_grip  = 1.0;      // so weit greifen die Führungsschienen über die Platinenkanten (links/rechts)
si_rail_l = 9;       // Länge der Schienen ab der SMA-Wand (<= Gewindelänge, damit man einschieben kann)
// nur für "screw": Bohrungen relativ zur linken HINTEREN Ecke, Schraube M2
si_holes = [[2.5, 2.5], [si_l-2.5, 2.5]];
si_pilot_d = 1.7;

/* ---------- Gehäuse-Parameter ---------- */
wall     = 2.0;
front_t  = 2.0;
floor_t  = 2.0;
clr      = 0.5;      // Luft CYD <-> Wand an den Schmalseiten (X)
clr_y    = 1.5;      // Luft CYD <-> Wand an den Längsseiten (Y), Platz für Steckerkabel
glass_gap = 0.2;
lower_depth = 22;    // Innenraum unter der CYD-Platine
z_split  = -6.0;     // Trennebene Oberteil / Unterteil
corner_r = 3;
win_margin = 0.6;

usbc_open_w = 12.5;
usbc_open_h = 7.0;

sma_d     = 6.8;     // Gewinde 1/4"-36 = 6.35 mm + Luft
sma_slot  = 0.6;     // Langloch in Z als Höhentoleranz
sma_h     = 10;      // Höhe der SMA-Achse über dem Boden (innen)

screw_d    = 2.8;    // Durchgang M2.5
pilot_d    = 2.1;    // Kernloch M2.5
csk_d      = 5.0;
standoff_d = 6.0;
tube_d     = 5.5;

$fn = 48;

/* ---------- abgeleitete Maße ---------- */
in_x0 = -clr;            in_x1 = pcb_l + clr;
in_y0 = -clr_y;          in_y1 = pcb_w + clr_y;
out_x0 = in_x0 - wall;   out_x1 = in_x1 + wall;
out_y0 = in_y0 - wall;   out_y1 = in_y1 + wall;
plate_in  = disp_top + glass_gap;
z_top     = plate_in + front_t;
z_floor   = -lower_depth;
z_bottom  = z_floor - floor_t;
standoff_h = plate_in - pcb_t;
sma_z     = z_floor + sma_h;
si_z      = sma_z - sma_axis_off - si_t;       // Platinen-Unterseite Si5351
si_lift   = si_z - z_floor;
si_y1     = in_y1 - sma_body;                  // SMA-Kante der Platine
si_y0     = si_y1 - si_w;                      // hintere Kante
sma_x     = [for (s = si_sma) si_x0 + s];
snaps = [for (wl = ["ymin", "ymax"], sx = [-1, 1]) [wl, pcb_l/2 + sx*snap_dx, snap_w]];

screw_len = (plate_in - 0.3) - z_bottom;
echo(str("Gehäuse aussen: ", out_x1-out_x0, " x ", out_y1-out_y0, " x ", z_top-z_bottom, " mm"));
if (closure == "screw") echo(str("Schraube M2.5 Senkkopf, max. Länge ca. ", floor(screw_len), " mm"));
echo(str("Si5351 liegt ", si_lift, " mm über dem Boden"));
assert(sma_z + sma_slot/2 + sma_d/2 * sqrt(2) < z_split - 0.8, "SMA-Durchbruch zu nah an der Trennebene");

module rrect(x0, y0, x1, y1, h, r) {
    translate([x0, y0, 0]) linear_extrude(h)
        offset(r) offset(-r) square([x1-x0, y1-y0]);
}
module shell(z0, z1) {
    difference() {
        translate([0,0,z0]) rrect(out_x0, out_y0, out_x1, out_y1, z1-z0, corner_r);
        translate([0,0,z0-1]) rrect(in_x0, in_y0, in_x1, in_y1, z1-z0+2, max(corner_r-wall, 0.5));
    }
}
// Innenecke des Gehäuses, die zu einer CYD-Bohrung gehört (inset = Abstand zur Wand)
function corner_of(h, inset) = [
    h[0] < pcb_l/2 ? in_x0 + inset : in_x1 - inset,
    h[1] < pcb_w/2 ? in_y0 + inset : in_y1 - inset ];

// Dom/Rohr + Steg zur Gehäuseecke
module corner_post(h, d, z0, z1, inset) {
    c = corner_of(h, inset + 1);
    translate([0,0,z0]) hull() {
        translate([h[0], h[1], 0]) cylinder(d = d, h = z1 - z0);
        translate([c[0], c[1], 0]) cylinder(d = 2, h = z1 - z0);
    }
}

/* ======================= OBERTEIL ======================= */
module top() {
    difference() {
        union() {
            shell(z_split, plate_in);
            translate([0,0,plate_in]) rrect(out_x0, out_y0, out_x1, out_y1, front_t, corner_r);
            for (h = holes) corner_post(h, standoff_d, pcb_t, plate_in + 0.01, 0);
            // Zentrierstifte (nur Rastversion): greifen in die CYD-Bohrungen
            if (closure == "snap") for (h = holes) translate([h[0], h[1], pcb_t - 1.3])
                cylinder(d1 = pin_d - 0.6, d2 = pin_d, h = 1.31);
            if (closure == "snap") for (sn = snaps) snap_place(sn) snap_hook_arm(sn[2]);
        }
        hull() {   // Displayfenster mit 45°-Fase nach außen
            translate([act_x0-win_margin, act_y0-win_margin, plate_in-0.01])
                cube([act_x1-act_x0+2*win_margin, act_y1-act_y0+2*win_margin, 0.01]);
            translate([act_x0-win_margin-front_t, act_y0-win_margin-front_t, z_top])
                cube([act_x1-act_x0+2*(win_margin+front_t), act_y1-act_y0+2*(win_margin+front_t), 0.01]);
        }
        if (closure == "screw") for (h = holes) translate([h[0], h[1], pcb_t - 0.1])
            cylinder(d = pilot_d, h = standoff_h + front_t - 1 + 0.1);
        // USB-C: nach unten offene Aussparung (kein Brückenüberhang beim Druck),
        // die Unterkante bildet die Wand des Unterteils
        translate([in_x1 - 1, usbc_y, 0]) rotate([0, 90, 0]) linear_extrude(wall + 2)
            hull() {
                translate([-(usbc_z + usbc_open_h/2) + 2.5, 0]) offset(2.5) square([0.01, usbc_open_w - 5], center = true);
                translate([-(z_split - 1), 0]) square([0.01, usbc_open_w], center = true);
            }
    }
}

/* ======================= UNTERTEIL ======================= */
lip_h = 1.5; lip_t = 1.2; lip_clr = 0.25;

module bottom() {
    difference() {
        union() {
            translate([0,0,z_bottom]) rrect(out_x0, out_y0, out_x1, out_y1, floor_t, corner_r);
            shell(z_floor - 0.01, z_split);
            translate([0,0,z_split - 0.01]) difference() {   // Zentrierrand
                rrect(in_x0+lip_clr, in_y0+lip_clr, in_x1-lip_clr, in_y1-lip_clr, lip_h, 0.5);
                translate([0,0,-1]) rrect(in_x0+lip_clr+lip_t, in_y0+lip_clr+lip_t,
                                          in_x1-lip_clr-lip_t, in_y1-lip_clr-lip_t, lip_h+2, 0.5);
            }
            // 45°-Stütze unter dem Zentrierrand (aus der Wand herauswachsend)
            lip_support();
            // Rohre mit Steg zur Ecke; oberhalb der Trennebene mit Spiel zum Oberteil
            for (h = holes) {
                corner_post(h, tube_d, z_floor - 0.01, z_split, 0);
                corner_post(h, tube_d, z_split - 0.01, 0, lip_clr);
            }
            si_holder();
        }
        if (closure == "snap") for (sn = snaps) snap_place(sn) snap_cutout(sn[2]);
        if (closure == "screw") for (h = holes) translate([h[0], h[1], 0]) {
            translate([0,0,z_bottom-1]) cylinder(d = screw_d, h = -z_bottom + 2);
            translate([0,0,z_bottom-0.01]) cylinder(d1 = csk_d+0.4, d2 = screw_d, h = (csk_d+0.4-screw_d)/2);
        }
        // Rand unter der USB-C-Öffnung aussparen – an BEIDEN Schmalseiten (punktsymmetrisch),
        // damit das Unterteil auch um 180° gedreht aufgesetzt werden kann
        for (r = [0, 180]) translate([pcb_l/2, pcb_w/2, 0]) rotate([0, 0, r]) translate([-pcb_l/2, -pcb_w/2, 0])
            translate([in_x1-3, usbc_y-usbc_open_w/2-1, z_split-0.1]) cube([4, usbc_open_w+2, lip_h+1]);
        // SMA-Durchbrüche: Langloch in Z mit 45°-Tropfenspitze oben (stützfrei)
        for (x = sma_x) translate([x, in_y1 - 1, sma_z]) rotate([-90, 0, 0])
            linear_extrude(wall + 2) sma_teardrop();
    }
}

// Keilring unter dem Zentrierrand: an der Wand 0 mm, oben so breit wie der Rand
module lip_support() {
    d = lip_clr + lip_t + 0.15;
    difference() {
        translate([0,0,z_split - d]) rrect(in_x0 - 0.01, in_y0 - 0.01, in_x1 + 0.01, in_y1 + 0.01, d + 0.01, 1);
        hull() {
            translate([0,0,z_split - d - 0.01]) rrect(in_x0, in_y0, in_x1, in_y1, 0.01, 1);
            translate([0,0,z_split + 0.01]) rrect(in_x0 + d, in_y0 + d, in_x1 - d, in_y1 - d, 0.01, 0.5);
        }
    }
}

// Tropfenform (2D) für die SMA-Bohrung; Spitze zeigt nach -y, nach rotate([-90,0,0]) also nach oben
module sma_teardrop() {
    r = sma_d/2; k = sma_slot/2;
    w = concat(
        [[0, k + r*sqrt(2)]],                                   // Spitze
        [for (a = [45 : -7.5 : 0])    [r*cos(a),  k + r*sin(a)]],
        [for (a = [0 : -7.5 : -180])  [r*cos(a), -k + r*sin(a)]],
        [for (a = [180 : -7.5 : 135]) [r*cos(a),  k + r*sin(a)]]);
    polygon([for (p = w) [p[0], -p[1]]]);
}

/* ---------- Rastverschluss ---------- */
// lokales System: Innenseite der Wand bei y=0, Innenraum in +y, x entlang der Wand
module snap_place(sn) {
    if (sn[0] == "ymin")      translate([sn[1], in_y0, 0]) children();
    else if (sn[0] == "ymax") translate([sn[1], in_y1, 0]) rotate([0,0,180]) children();
    else if (sn[0] == "xmin") translate([in_x0, sn[1], 0]) rotate([0,0,-90]) children();
    else if (sn[0] == "xmax") translate([in_x1, sn[1], 0]) rotate([0,0,90]) children();
}
// Profil (lokal: y = 0 Wandinnenseite, -y nach außen, z absolut)
ya     = -snap_in;                    // Außenseite Arm
yt     = -wall + snap_tip;            // Hakenspitze
hk     = ya - yt;                     // Hakentiefe ab Arm
y_pk   = ya - snap_clr;               // Taschengrund im Unterteil
z_tip  = snap_z - hk;                 // Unterkante der 45°-Rastschräge an der Spitze
z_arm0 = z_tip - 0.4 - 2.0;           // Armende (Einführschräge 2 mm)
z_win0 = z_tip - 0.4 - 2.0 * (hk - snap_clr) / hk - 0.3;   // Fenster-Unterkante

// Federarm am Oberteil
module snap_hook_arm(w) {
    rotate([90, 0, 90]) translate([0, 0, -w/2]) linear_extrude(w) polygon([
        [ya,           z_split + ya + snap_t0],    // im Wandquerschnitt des Oberteils verankert
        [0,            z_split + ya + snap_t0],
        [ya + snap_t0, z_split],          // 45°-Stütze an der Innenseite
        [ya + snap_t,  z_arm0],           // verjüngt bis zum Haken
        [ya,          z_arm0],
        [yt,          z_tip - 0.4],       // Einführschräge
        [yt,          z_tip],
        [ya,          snap_z]             // 45°-Rastschräge
    ]);
}
// Tasche + Fenster + Randaussparung im Unterteil
module snap_cutout(w) {
    c = snap_clr;
    // Tasche in der Wandinnenseite von der Trennebene bis unter den Arm
    translate([-w/2 - c, y_pk, z_arm0 - 0.4]) cube([w + 2*c, -y_pk + 0.01, z_split - z_arm0 + 0.5]);
    // Zentrierrand aussparen
    translate([-w/2 - 1, -0.01, z_split - lip_clr - lip_t - 0.2]) cube([w + 2, ya + snap_t0 + c + 0.3, lip_h + lip_clr + lip_t + 1.2]);
    // Fenster durch die Restwand, Oberkante 45° passend zur Rastschräge
    rotate([90, 0, 90]) translate([0, 0, -w/2 - c]) linear_extrude(w + 2*c) polygon([
        [y_pk + 0.01, z_win0],
        [y_pk + 0.01, snap_z - (ya - y_pk) + snap_play],
        [-wall - 1,   snap_z - (ya + wall + 1) + snap_play],
        [-wall - 1,   z_win0]
    ]);
}

/* ---------- Si5351-Halter ---------- */
module si_holder() {
    c = 0.25;
    translate([0,0,z_floor - 0.01]) {
        if (si_mount == "screw") {
            for (p = si_holes) translate([si_x0 + p[0], si_y0 + p[1], 0]) difference() {
                cylinder(d = 4.5, h = si_lift);
                translate([0,0,1]) cylinder(d = si_pilot_d, h = si_lift);
            }
        } else {
            // Schiebeführung: Platine hinter den Schienen ablegen und Richtung SMA-Wand
            // schieben, bis die SMA-Gehäuse an der Wand anliegen. Außen die SMA-Muttern
            // festziehen – sie ziehen die Platine an die Wand und halten sie in Y.
            // Keine federnden Teile, Unterseite der Schienenlippe 45° (stützfrei).
            for (side = [0, 1]) translate([side ? si_x0 + si_l : si_x0, 0, 0]) mirror([side, 0, 0])
                translate([0, in_y1 - si_rail_l, 0]) rotate([90, 0, 0]) mirror([0, 0, 1])
                    linear_extrude(si_rail_l + 0.01) rail_profile();
            // Auflagen unter der hinteren Platinenhälfte (lang genug für den Schiebeweg)
            for (px = [0, si_l - 5]) translate([si_x0 + px, in_y1 - sma_body - si_w, 0])
                cube([5, si_w - si_rail_l - 1, si_lift]);
        }
    }
}

// Querschnitt einer Schiene (2D: x = quer zur Platine, Platinenkante bei x = 0,
// Platine in +x; y = Höhe ab Boden). Wird so gespiegelt, dass +x zur Platine zeigt.
module rail_profile() {
    c = 0.3;  top = si_lift + si_t;  wb = 2.2;
    u0 = top + 0.2 - c;                       // Unterseite Lippe an der Nutrückwand
    polygon([
        [-c - wb, 0], [si_grip, 0],           // Fuß
        [si_grip, si_lift], [-c, si_lift],    // Auflagefläche unter der Platinenkante
        [-c, u0],                             // Nutrückwand
        [si_grip, u0 + c + si_grip],          // 45°-Unterseite der Lippe
        [si_grip, u0 + c + si_grip + 0.8],
        [-c - wb, u0 + c + si_grip + 0.8 + si_grip + c + wb]   // 45°-Oberseite (Einführhilfe)
    ]);
}

/* ======================= DUMMIES ======================= */
module cyd_dummy() {
    color("gold")  cube([pcb_l, pcb_w, pcb_t]);
    color("white") translate([8.21, 0, pcb_t]) cube([69.2, 50, disp_top - pcb_t]);
    color("black") translate([act_x0, act_y0, disp_top-0.05]) cube([act_x1-act_x0, act_y1-act_y0, 0.1]);
    color("silver") translate([pcb_l-7.5, usbc_y-4.5, -3.46]) cube([7.5, 9, 3.46]);
    color("dimgray") translate([0, 15.8, -3.25]) cube([25.5, 18, 3.25]);
}
module si_dummy() {
    color("purple") translate([si_x0, si_y0, si_z]) cube([si_l, si_w, si_t]);
    for (x = sma_x) color("goldenrod") {
        translate([x - 3.2, si_y1 - 4, sma_z - 3.2]) cube([6.4, 4 + sma_body, 6.4]);   // Körper
        translate([x, in_y1, sma_z]) rotate([-90,0,0]) cylinder(d = 6.35, h = 7);   // Gewinde
    }
}

/* ======================= AUSGABE ======================= */
if (part == "top") {
    translate([0,0,z_top]) rotate([180,0,0]) top();
} else if (part == "bottom") {
    translate([0,0,-z_bottom]) bottom();
} else if (part == "print") {
    translate([0,0,z_top]) rotate([180,0,0]) top();
    translate([0, out_y1 - out_y0 + 8, -z_bottom]) bottom();
} else if (part == "explode") {
    translate([0,0,25]) { top(); cyd_dummy(); }
    rot_bottom() { bottom(); si_dummy(); }
} else {
    top(); cyd_dummy(); rot_bottom() { bottom(); si_dummy(); }
}
module rot_bottom() {
    translate([pcb_l/2, pcb_w/2, 0]) rotate([0, 0, bottom_rot]) translate([-pcb_l/2, -pcb_w/2, 0]) children();
}
