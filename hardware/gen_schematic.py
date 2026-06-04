#!/usr/bin/env python3
"""
Generate hardware/tanita_sniffer.kicad_sch  (KiCad 8 schematic).

The schematic is emitted programmatically so every pin tip, wire stub and net
label coordinate is computed from one convention instead of hand-placed -- that
keeps the connectivity correct and the file structurally valid.

Topology: a PASSIVE parallel tap. The original SD-Card PCB stays connected to
the handset; this board only listens.

    CN1 (handset link)        series 100R          ESP32-S3
    -------------------       -----------       ---------------
    SCK  --- SCK_TAP --[R1]-- SCK       ------- GPIO12
    CS   --- CS_TAP  --[R2]-- CS_n      ------- GPIO10
    SIMO --- SIMO_TAP--[R3]-- SIMO_MOSI ------- GPIO11   (SPI2 listen)
    SOMI --- SOMI_TAP--[R4]-- SOMI_MISO ------- GPIO13   (SPI3 listen)
    GND  ------------------- GND --------------- GND      (REQUIRED common)
    VCC  --- (DNC) handset 3V3, do NOT connect
    NC   --- (unused)

Run:  python3 hardware/gen_schematic.py
"""

import os

ROOT_UUID = "00000000-0000-0000-0000-0000000000aa"
_uid = 0


def uid():
    global _uid
    _uid += 1
    return f"00000000-0000-0000-0000-{_uid:012d}"


FONT = "(effects (font (size 1.27 1.27)))"


# ---- library symbol definitions -----------------------------------------

def lib_symbols():
    s = []
    s.append('  (lib_symbols')

    # 7-pin connector; pins exit to the RIGHT (angle 180 -> tip on right).
    pins7 = ["SCK", "CS", "SIMO", "SOMI", "GND", "VCC", "NC"]
    s.append('    (symbol "tanita:Handset_CN1" (pin_names (offset 1.016))'
             ' (in_bom yes) (on_board yes)')
    s.append(f'      (property "Reference" "J" (at -5.08 12.7 0) {FONT})')
    s.append(f'      (property "Value" "Handset_CN1" (at -5.08 -12.7 0) {FONT})')
    s.append(f'      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    s.append(f'      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    s.append('      (symbol "Handset_CN1_0_1"')
    s.append('        (rectangle (start -5.08 10.16) (end 5.08 -10.16)'
             ' (stroke (width 0.254) (type default)) (fill (type background))))')
    s.append('      (symbol "Handset_CN1_1_1"')
    for i, nm in enumerate(pins7):
        y = 7.62 - i * 2.54
        s.append(f'        (pin passive line (at 7.62 {y:g} 180) (length 2.54)'
                 f' (name "{nm}" {FONT}) (number "{i+1}" {FONT}))')
    s.append('      ))')

    # ESP32-S3 module; relevant pins exit to the LEFT (angle 0 -> tip on left).
    espins = [("GPIO12", "SCK_in"), ("GPIO10", "CS_in"),
              ("GPIO11", "SIMO_in"), ("GPIO13", "SOMI_in"),
              ("GND", "GND"), ("5V", "USB_5V")]
    s.append('    (symbol "tanita:ESP32S3" (pin_names (offset 1.016))'
             ' (in_bom yes) (on_board yes)')
    s.append(f'      (property "Reference" "U" (at -2.54 12.7 0) {FONT})')
    s.append(f'      (property "Value" "ESP32-S3" (at -2.54 -12.7 0) {FONT})')
    s.append(f'      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    s.append(f'      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    s.append('      (symbol "ESP32S3_0_1"')
    s.append('        (rectangle (start -7.62 10.16) (end 7.62 -10.16)'
             ' (stroke (width 0.254) (type default)) (fill (type background))))')
    s.append('      (symbol "ESP32S3_1_1"')
    for i, (nm, _) in enumerate(espins):
        y = 7.62 - i * 2.54
        s.append(f'        (pin {("power_in" if nm in ("GND","5V") else "input")} line'
                 f' (at -10.16 {y:g} 0) (length 2.54)'
                 f' (name "{nm}" {FONT}) (number "{i+1}" {FONT}))')
    s.append('      ))')

    # Horizontal resistor: pin1 left tip, pin2 right tip.
    s.append('    (symbol "tanita:R_H" (pin_numbers (hide yes)) (pin_names (offset 0) (hide yes))'
             ' (in_bom yes) (on_board yes)')
    s.append(f'      (property "Reference" "R" (at 0 2.286 0) {FONT})')
    s.append(f'      (property "Value" "100" (at 0 -2.286 0) {FONT})')
    s.append(f'      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    s.append(f'      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    s.append('      (symbol "R_H_0_1"')
    s.append('        (rectangle (start -2.54 1.016) (end 2.54 -1.016)'
             ' (stroke (width 0.254) (type default)) (fill (type none))))')
    s.append('      (symbol "R_H_1_1"')
    s.append(f'        (pin passive line (at -5.08 0 0) (length 2.54)'
             f' (name "~" {FONT}) (number "1" {FONT}))')
    s.append(f'        (pin passive line (at 5.08 0 180) (length 2.54)'
             f' (name "~" {FONT}) (number "2" {FONT}))')
    s.append('      ))')

    s.append('  )')
    return "\n".join(s)


# ---- helpers for instances, wires, labels --------------------------------

parts = []     # symbol instances
wires = []     # (x1,y1,x2,y2)
labels = []    # (text, x, y, justify)
texts = []     # (text, x, y)


def place(lib, ref, val, x, y, pins_abs):
    """pins_abs: list of (number, abs_x, abs_y) for the placed instance."""
    u = uid()
    lines = [f'  (symbol (lib_id "{lib}") (at {x:g} {y:g} 0) (unit 1)'
             ' (in_bom yes) (on_board yes) (dnp no)',
             f'    (uuid "{u}")',
             f'    (property "Reference" "{ref}" (at {x:g} {y-13:g} 0) {FONT})',
             f'    (property "Value" "{val}" (at {x:g} {y+13:g} 0) {FONT})']
    for num, _, _ in pins_abs:
        lines.append(f'    (pin "{num}" (uuid "{uid()}"))')
    lines.append(f'    (instances (project "tanita_sniffer"'
                 f' (path "/{ROOT_UUID}" (reference "{ref}") (unit 1))))')
    lines.append('  )')
    parts.append("\n".join(lines))


def stub_label(tip_x, tip_y, direction, net):
    """Draw a 2.54 stub from a pin tip and label the net at the stub end."""
    dx = 5.08 * (1 if direction == "right" else -1)
    end_x = tip_x + dx
    wires.append((tip_x, tip_y, end_x, tip_y))
    just = "left" if direction == "right" else "right"
    labels.append((net, end_x, tip_y, just))


def main():
    # --- CN1 handset connector at (60,90); pins exit right (tip = x+7.62) ---
    jx, jy = 60, 90
    j_nets = ["SCK_TAP", "CS_TAP", "SIMO_TAP", "SOMI_TAP", "GND", "VCC_HANDSET_DNC", "NC"]
    j_pins = []
    for i in range(7):
        # KiCad mirrors library Y (up) onto schematic Y (down): abs_y = oy - local_y
        tx, ty = jx + 7.62, jy - (7.62 - i * 2.54)
        j_pins.append((str(i + 1), tx, ty))
        if j_nets[i] != "NC":
            stub_label(tx, ty, "right", j_nets[i])
    place("tanita:Handset_CN1", "J1", "Handset_CN1", jx, jy, j_pins)

    # --- ESP32-S3 at (200,90); pins exit left (tip = x-10.16) ---
    ux, uy = 200, 90
    u_nets = ["SCK", "CS_n", "SIMO_MOSI", "SOMI_MISO", "GND", "USB_5V"]
    u_pins = []
    for i in range(6):
        tx, ty = ux - 10.16, uy - (7.62 - i * 2.54)   # Y mirror, see J1
        u_pins.append((str(i + 1), tx, ty))
        stub_label(tx, ty, "left", u_nets[i])
    place("tanita:ESP32S3", "U1", "ESP32-S3-DevKitC", ux, uy, u_pins)

    # --- series resistors R1..R4 stacked in the middle (x=130) ---
    r_in = ["SCK_TAP", "CS_TAP", "SIMO_TAP", "SOMI_TAP"]
    r_out = ["SCK", "CS_n", "SIMO_MOSI", "SOMI_MISO"]
    for i in range(4):
        rx, ry = 130, 110 - i * 8
        # pin1 tip at rx-5.08 (left), pin2 tip at rx+5.08 (right)
        place("tanita:R_H", f"R{i+1}", "100", rx, ry,
              [("1", rx - 5.08, ry), ("2", rx + 5.08, ry)])
        stub_label(rx - 5.08, ry, "left", r_in[i])
        stub_label(rx + 5.08, ry, "right", r_out[i])

    # --- annotation notes ---
    texts.append(("PASSIVE SPI TAP - listens only, never drives the bus.", 60, 130))
    texts.append(("Do NOT connect handset VCC (net VCC_HANDSET_DNC).", 60, 134))
    texts.append(("Power ESP32 from USB. GND MUST be common with the handset.", 60, 138))
    texts.append(("R1-R4 = optional 100R series protection (0R / wire ok).", 60, 142))
    texts.append(("SPI2 listens SIMO_MOSI (GPIO11); SPI3 listens SOMI_MISO (GPIO13).", 60, 146))

    # --- assemble file ---
    out = []
    out.append('(kicad_sch')
    out.append('  (version 20231120)')
    out.append('  (generator "tanita_gen_schematic")')
    out.append('  (generator_version "8.0")')
    out.append(f'  (uuid "{ROOT_UUID}")')
    out.append('  (paper "A4")')
    out.append('  (title_block')
    out.append('    (title "TANITA SPI Protocol Grabber - passive tap")')
    out.append('    (company "TANITA-WIFI-Retrofit")')
    out.append('    (comment 1 "ESP32-S3 dual SPI slave bus sniffer")')
    out.append('  )')
    out.append(lib_symbols())

    for w in wires:
        out.append(f'  (wire (pts (xy {w[0]:g} {w[1]:g}) (xy {w[2]:g} {w[3]:g}))'
                   f' (stroke (width 0) (type default)) (uuid "{uid()}"))')
    for (txt, x, y, just) in labels:
        out.append(f'  (label "{txt}" (at {x:g} {y:g} 0)'
                   f' (effects (font (size 1.27 1.27)) (justify {just} bottom))'
                   f' (uuid "{uid()}"))')
    for (txt, x, y) in texts:
        out.append(f'  (text "{txt}" (at {x:g} {y:g} 0)'
                   f' (effects (font (size 1.27 1.27)) (justify left)) (uuid "{uid()}"))')

    for p in parts:
        out.append(p)

    out.append('  (sheet_instances (path "/" (page "1")))')
    out.append(')')
    out.append('')

    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "tanita_sniffer.kicad_sch")
    with open(path, "w") as f:
        f.write("\n".join(out))

    # paren-balance sanity check
    txt = "\n".join(out)
    bal = 0
    for ch in txt:
        if ch == '(':
            bal += 1
        elif ch == ')':
            bal -= 1
    assert bal == 0, f"unbalanced parens: {bal}"
    print(f"wrote {path}  ({len(out)} lines, parens balanced)")


if __name__ == "__main__":
    main()
