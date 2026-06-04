#!/usr/bin/env python3
"""
Generate hardware/tanita_sniffer_breadboard.svg

A Fritzing-style *physical* wiring drawing of the passive SPI tap: ESP32-S3
DevKitC on the left, a mini breadboard with the four series resistors in the
middle, and the TANITA CN1 handset connector (7-pin) on the right. Color-coded
jumpers match hardware/README.md.

Renders inline on GitHub / any browser. Run:
    python3 hardware/gen_breadboard_svg.py
"""

import os

W, H = 1040, 660

# signal -> wire colour (illustrative; confirm real CN1 colours on your unit)
COL = {
    "SCK":  "#e8b500",   # yellow
    "CS":   "#ff8c1a",   # orange
    "SIMO": "#2b9d976",  # (fixed below) green
    "SOMI": "#2f7fd0",   # blue
    "GND":  "#222222",   # black
    "VCC":  "#d83232",   # red
    "NC":   "#9aa0a6",   # grey
}
COL["SIMO"] = "#2e9e5b"  # green

svg = []


def el(s):
    svg.append(s)


def rrect(x, y, w, h, rx, fill, stroke="#333", sw=1.5, extra=""):
    el(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" '
       f'fill="{fill}" stroke="{stroke}" stroke-width="{sw}" {extra}/>')


def text(x, y, s, size=14, fill="#111", anchor="start", weight="normal", mono=False):
    fam = "monospace" if mono else "sans-serif"
    el(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" '
       f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{s}</text>')


def hole(cx, cy, r=3.2, fill="#3a3a3a"):
    el(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}"/>')


def wire(pts, color, wdth=4.5):
    d = " ".join(f"{x},{y}" for x, y in pts)
    el(f'<polyline points="{d}" fill="none" stroke="{color}" '
       f'stroke-width="{wdth}" stroke-linecap="round" stroke-linejoin="round"/>')


def pad(cx, cy, color):
    el(f'<circle cx="{cx}" cy="{cy}" r="6" fill="#d9c27a" stroke="#7a6a2a" stroke-width="1.5"/>')
    el(f'<circle cx="{cx}" cy="{cy}" r="2.4" fill="{color}"/>')


def resistor(cx, cy, color):
    """horizontal 100R resistor (brown-black-brown-gold) centred at cx,cy."""
    bw, bh = 56, 16
    el(f'<rect x="{cx-bw/2}" y="{cy-bh/2}" width="{bw}" height="{bh}" rx="7" '
       f'fill="#d9c9a3" stroke="#9a8a5a" stroke-width="1.2"/>')
    bands = ["#5a3a1a", "#111111", "#5a3a1a", "#c9a227"]
    for i, c in enumerate(bands):
        bx = cx - 16 + i * 9
        el(f'<rect x="{bx}" y="{cy-bh/2}" width="3.5" height="{bh}" fill="{c}"/>')
    # leads
    el(f'<line x1="{cx-bw/2-14}" y1="{cy}" x2="{cx-bw/2}" y2="{cy}" stroke="#999" stroke-width="2.5"/>')
    el(f'<line x1="{cx+bw/2}" y1="{cy}" x2="{cx+bw/2+14}" y2="{cy}" stroke="#999" stroke-width="2.5"/>')


def build():
    el(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
       f'font-family="sans-serif">')
    rrect(0, 0, W, H, 0, "#fbfbf8", stroke="none")
    text(W/2, 34, "TANITA SPI Protocol Grabber — breadboard wiring (passive tap)",
         20, "#111", "middle", "bold")
    text(W/2, 56, "ESP32-S3 listens only · original SD PCB stays connected · GND must be common",
         13, "#555", "middle")

    # ---- y positions shared across ESP32 / resistor / CN1 for the 4 signals
    sig_y = {"SCK": 300, "CS": 348, "SIMO": 396, "SOMI": 444}

    # ============ ESP32-S3 DevKitC (left) ============
    ex, ey, ew, eh = 70, 150, 210, 380
    rrect(ex, ey, ew, eh, 12, "#1f6f4a", "#0d3a26", 2)        # green PCB
    rrect(ex + ew/2 - 28, ey - 16, 56, 22, 5, "#9aa0a6")     # USB-C shield
    text(ex + ew/2, ey - 1, "USB-C", 11, "#222", "middle")
    text(ex + ew/2, ey + 26, "ESP32-S3", 18, "#fff", "middle", "bold")
    text(ex + ew/2, ey + 46, "DevKitC", 13, "#cfe6da", "middle")

    # right-edge header pins we use (top -> bottom)
    esp_pins = [
        ("5V",     220, COL["VCC"]),
        ("GND",    260, COL["GND"]),
        ("GPIO12", sig_y["SCK"],  COL["SCK"]),
        ("GPIO10", sig_y["CS"],   COL["CS"]),
        ("GPIO11", sig_y["SIMO"], COL["SIMO"]),
        ("GPIO13", sig_y["SOMI"], COL["SOMI"]),
    ]
    px = ex + ew  # right edge x
    esp_xy = {}
    for name, y, c in esp_pins:
        pad(px, y, c)
        text(px - 12, y + 4, name, 12, "#fff", "end", mono=True)
        esp_xy[name] = (px, y)

    # ============ mini breadboard (middle) ============
    bx, by, bw, bh = 470, 250, 200, 230
    rrect(bx, by, bw, bh, 10, "#eee8d8", "#cfc6ad", 2)
    # center channel
    el(f'<rect x="{bx}" y="{by+bh/2-8}" width="{bw}" height="16" fill="#ded6bf"/>')
    # tie-point holes (rows above/below channel, a few columns)
    for col in range(6):
        hx = bx + 24 + col * 30
        for row in range(5):
            hole(hx, by + 24 + row * 16)
            hole(hx, by + bh - 24 - row * 16)
    text(bx + bw/2, by - 8, "breadboard (R1–R4)", 12, "#777", "middle")

    # ============ CN1 handset connector (right) ============
    cx, cy, cw, ch = 820, 240, 120, 260
    rrect(cx, cy, cw, ch, 8, "#f2f2ef", "#b9b9b0", 2)
    text(cx + cw/2, cy - 10, "CN1 (handset)", 13, "#333", "middle", "bold")
    text(cx + cw/2, cy - 26, "7-pin JST", 11, "#888", "middle")
    cn1 = [
        ("SCK",  sig_y["SCK"],  COL["SCK"]),
        ("CS",   sig_y["CS"],   COL["CS"]),
        ("SIMO", sig_y["SIMO"], COL["SIMO"]),
        ("SOMI", sig_y["SOMI"], COL["SOMI"]),
        ("GND",  484,           COL["GND"]),
        ("VCC",  516,           COL["VCC"]),
        ("NC",   548,           COL["NC"]),
    ]
    cn1_xy = {}
    # connector body shorter than wires; extend body to hold all 7
    for name, y, c in cn1:
        # pin housing slot
        el(f'<rect x="{cx}" y="{y-7}" width="20" height="14" fill="#d9d9d2" stroke="#a9a9a0"/>')
        text(cx + 30, y + 4, name, 12, "#222", "start", mono=True)
        cn1_xy[name] = (cx, y)  # wire enters at left edge of connector

    # ============ resistors on the breadboard ============
    res_cx = bx + bw/2
    for sig in ("SCK", "CS", "SIMO", "SOMI"):
        resistor(res_cx, sig_y[sig], COL[sig])

    # ============ wires ============
    # signals: ESP32 GPIO -> R left lead ; R right lead -> CN1 pin
    rl = res_cx - 56/2 - 14   # resistor left lead x
    rr = res_cx + 56/2 + 14   # resistor right lead x
    for sig, gpio in (("SCK", "GPIO12"), ("CS", "GPIO10"),
                      ("SIMO", "GPIO11"), ("SOMI", "GPIO13")):
        y = sig_y[sig]
        gx, gy = esp_xy[gpio]
        wire([(gx, gy), (rl, y)], COL[sig])             # ESP32 -> R
        wire([(rr, y), (cn1_xy[sig][0], y)], COL[sig])  # R -> CN1

    # GND: ESP32 GND -> down -> CN1 GND (no resistor)
    gx, gy = esp_xy["GND"]
    wy = cn1_xy["GND"][1]
    wire([(gx, gy), (gx + 30, gy), (gx + 30, wy), (cn1_xy["GND"][0], wy)], COL["GND"])

    # 5V: ESP32 5V -> short stub + USB note (NOT to CN1)
    vx, vy = esp_xy["5V"]
    wire([(vx, vy), (vx + 26, vy)], COL["VCC"], 4)
    text(vx + 32, vy + 4, "(from USB only)", 11, "#a33", "start")

    # VCC on CN1: do-not-connect marker
    vcx, vcy = cn1_xy["VCC"]
    el(f'<line x1="{vcx-26}" y1="{vcy-10}" x2="{vcx-6}" y2="{vcy+10}" stroke="#d83232" stroke-width="3"/>')
    el(f'<line x1="{vcx-26}" y1="{vcy+10}" x2="{vcx-6}" y2="{vcy-10}" stroke="#d83232" stroke-width="3"/>')
    text(vcx - 34, vcy + 4, "DO NOT", 10, "#d83232", "end", weight="bold")

    # ============ legend / caveat ============
    ly = 580
    rrect(70, ly, W - 140, 60, 8, "#fff", "#ddd", 1)
    text(86, ly + 22, "Legend:", 13, "#111", weight="bold")
    items = [("SCK", "SCK→GPIO12"), ("CS", "CS→GPIO10"),
             ("SIMO", "SIMO→GPIO11"), ("SOMI", "SOMI→GPIO13"),
             ("GND", "GND common"), ("VCC", "VCC: skip")]
    x = 158
    for sig, label in items:
        el(f'<line x1="{x}" y1="{ly+18}" x2="{x+20}" y2="{ly+18}" '
           f'stroke="{COL[sig]}" stroke-width="5" stroke-linecap="round"/>')
        text(x + 26, ly + 22, label, 10.5, "#333", mono=True)
        x += 140
    text(86, ly + 46,
         "Wire colours are illustrative — confirm the real CN1 wire/pin order on your unit (see README). "
         "R1–R4 = 100 Ω series (optional).",
         11, "#777")

    el('</svg>')

    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "tanita_sniffer_breadboard.svg")
    with open(path, "w") as f:
        f.write("\n".join(svg))
    print(f"wrote {path}  ({len(svg)} elements)")


if __name__ == "__main__":
    build()
