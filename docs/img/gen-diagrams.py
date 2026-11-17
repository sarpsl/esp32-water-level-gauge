#!/usr/bin/env python3
"""Generate the rev B diagrams for the SR04M ultrasonic water-level gauge.

    python gen-diagrams.py [01 02 03 04]        # write SVG
    python check-diagrams.py                    # geometry check
    render each SVG with Edge (see README note) and save as PNG

Everything here is drawn from the firmware and docs, so the pictures and the
text cannot disagree: tank dimensions, pin map, divider ratio, alarm behaviour
and Blynk V-pins all come from the same numbers.

Layout rules learned the hard way (and enforced by check-diagrams.py):
  * labels sit 8 px ABOVE a wire, never on it
  * power nets are tags, not long rails, so wires cannot cross them
  * no text within 10 px of a vertical wire run
  * every string is word-wrapped to the width of the box it lives in
"""
import pathlib
import sys

OUT = pathlib.Path(__file__).resolve().parent

INK, GREY = "#12202e", "#6b7480"
BLUE, RED, GREEN, ORANGE = "#0b5cad", "#8c1c13", "#0a7d3c", "#c28000"
PWR, GND, WIRE = "#b00020", "#20242a", "#20242a"
FILL, OLED_BG, OLED_FG = "#f4f8fb", "#0d1116", "#e8f0f8"
CW = 0.60


class Doc:
    def __init__(self, w, h, title, subtitle):
        self.w, self.h = w, h
        self.o = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="0 0 {w} {h}" font-family="DejaVu Sans, Arial, sans-serif">',
            f'<rect width="{w}" height="{h}" fill="#ffffff"/>',
            f'<text x="40" y="34" font-size="22" font-weight="bold" fill="{INK}">{title}</text>',
        ]
        y = 56
        for line in subtitle:
            for ln in self.wrap(line, self.w - 80, 12.5):
                self.o.append(f'<text x="40" y="{y}" font-size="12.5" fill="{GREY}">{ln}</text>')
                y += 17

    def save(self, name):
        s = "\n".join(self.o) + "\n</svg>\n"
        (OUT / name).write_text(s, encoding="utf-8")
        return f"{name}  {self.w}x{self.h}  {len(self.o)} elements"

    # ---- text -------------------------------------------------------------
    def wrap(self, s, width_px, size):
        out, line = [], ""
        for word in str(s).split():
            cand = (line + " " + word).strip()
            if len(cand) * size * CW > width_px and line:
                out.append(line)
                line = word
            else:
                line = cand
        if line:
            out.append(line)
        return out or [""]

    def txt(self, x, y, t, size=12.5, color=INK, anchor="start", bold=False, mono=False,
            wrap_px=0, lh=None):
        t = str(t)
        if wrap_px:
            t = "\n".join(self.wrap(t, wrap_px, size))
        lh = lh or size + 3
        for i, ln in enumerate(t.split("\n")):
            f = ' font-family="DejaVu Sans Mono, Consolas, monospace"' if mono else ""
            b = ' font-weight="bold"' if bold else ""
            self.o.append(
                f'<text x="{x}" y="{y + i * lh}" font-size="{size}" fill="{color}"{b}{f} '
                f'text-anchor="{anchor}">{ln}</text>'
            )

    # ---- primitives -------------------------------------------------------
    def ln(self, x1, y1, x2, y2, color=WIRE, w=1.6, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.o.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
                      f'stroke-width="{w}" stroke-linecap="round"{d}/>')

    def poly(self, pts, color=WIRE, w=1.6, fill="none", dash=None, close=True):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        p = " ".join(f"{a:.1f},{b:.1f}" for a, b in pts)
        self.o.append(f'<polygon points="{p}{"Z" if close else ""}" fill="{fill}" '
                      f'stroke="{color}" stroke-width="{w}" stroke-linejoin="round"{d}/>')

    def box(self, x, y, w, h, fill=FILL, stroke=INK, sw=1.4, rx=4, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" '
                      f'stroke="{stroke}" stroke-width="{sw}" rx="{rx}"{d}/>')

    def dot(self, x, y, color=INK, r=3):
        self.o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')

    def gnd(self, x, y, label=None, anchor="start"):
        self.ln(x, y, x, y + 12)
        self.ln(x - 11, y + 12, x + 11, y + 12, GND, 2)
        self.ln(x - 7, y + 16, x + 7, y + 16, GND, 2)
        self.ln(x - 3, y + 20, x + 3, y + 20, GND, 2)
        if label:
            self.txt(x + 15, y + 16, label, 10.5, GREY, anchor)

    def resistor(self, x, y, w, h, value, vertical=False, color=INK, below=False):
        """The box only; leads are drawn by the caller."""
        self.box(x, y, w, h, fill="#ffffff", stroke=color, sw=1.5, rx=2)
        if vertical:
            self.txt(x + w + 6, y + h / 2 + 4, value, 12, color, "start", bold=True)
        elif below:
            self.txt(x + w / 2, y + h + 13, value, 12, color, "middle", bold=True)
        else:
            self.txt(x + w / 2, y - 6, value, 12, color, "middle", bold=True)

    def cap(self, x, y, value, color=PWR):
        """Polarised cap hanging from y down to a ground, centred on x."""
        self.ln(x, y, x, y + 14, color)
        self.ln(x - 11, y + 14, x + 11, y + 14, color, 2)
        self.ln(x - 8, y + 19, x + 8, y + 19, color, 2)
        self.ln(x, y + 19, x, y + 30, color)
        self.gnd(x, y + 30)
        self.txt(x + 15, y + 16, value, 11, color)

    def tag(self, x, y, label, color, size=11.5):
        """Net label: a tag box centred on (x, y). Wires stop at its edge."""
        w = len(label) * size * CW + 14
        self.box(x - w / 2, y - 11, w, 22, fill="#ffffff", stroke=color, sw=1.4, rx=4)
        self.txt(x, y + 4, label, size, color, "middle", bold=True)
        return w / 2

    def pin(self, x, y, label, side="left", color=INK, sub=None):
        """Pin stub on a block edge with its name above the wire, never on it."""
        d = -10 if side == "left" else 10
        self.dot(x, y, GND, 2.4)
        a = x + d + (4 if side == "right" else -4)
        self.txt(a, y - 6, label, 12, color, "start" if side == "right" else "end", bold=True)
        if sub:
            self.txt(a, y + 12, sub, 10.5, GREY, "start" if side == "right" else "end")
        return a

    def dim(self, x, y1, y2, big, small, color=INK, anchor="start", dx=8, lbl_dy=0):
        self.ln(x, y1, x, y2, color, 1.3)
        for yy in (y1, y2):
            self.ln(x - 6, yy, x + 6, yy, color, 1.3)
        mid = (y1 + y2) / 2 + lbl_dy
        self.txt(x + dx, mid - 4, big, 11.5, color, anchor, bold=True)
        self.txt(x + dx, mid + 10, small, 11.5, color, anchor)

    def legend(self, x, y, w, lines, title="Read before wiring", size=12.5):
        rows = [self.wrap(ln, w - 40, size) for ln in lines]
        h = 52 + sum(len(r) for r in rows) * (size + 6)
        self.box(x, y, w, h, fill="#fdf6f4", stroke=RED, sw=1.6, rx=6)
        self.txt(x + 14, y + 24, title, 14, RED, bold=True)
        yy = y + 46
        for r in rows:
            for ln in r:
                self.txt(x + 14, yy, ln, size)
                yy += size + 6
        return y + h

    def panel(self, x, y, w, title):
        self.box(x, y, w, 34, fill="#eef4fa", stroke=INK, sw=1.4, rx=6)
        self.txt(x + 14, y + 22, title, 13.5, INK, bold=True)

    def table(self, x, y, rows, col, size=11.5):
        for i, cells in enumerate(rows):
            yy = y + i * 18
            for c, text in enumerate(cells):
                self.txt(x + col[c], yy, text, size, INK if c else GREY, mono=(c == 0))
        return y + len(rows) * 18


# --------------------------------------------------------------------------
# 01 — bench wiring
# --------------------------------------------------------------------------
def build_01():
    d = Doc(1240, 1080, "01 - rev B bench wiring: ESP32 + SR04M-family ultrasonic gauge", [
        "SR04M / SR04M-2 / AJ-SR04M / JSN-SR04M-2. Mode 1 (HC-SR04-compatible TRIG/ECHO) is the "
        "only mode the firmware speaks: pad R19 must stay unpopulated.",
        "GPIO numbers are ESP32-WROOM-32 numbers, matching the reference project "
        "iotcircuithub.com/iot-based-water-level-indicator . The pin columns are a signal map, not "
        "the physical pin order. Nothing here switches a pump or a valve.",
    ])

    # ---- sensor module -----------------------------------------------------
    d.box(60, 220, 240, 190)
    d.txt(74, 246, "SR04M-family module", 14, INK, bold=True)
    d.txt(74, 264, "electronics board, dry box", 11.5, GREY)
    d.txt(74, 396, "mode 1: pad R19 UNPOPULATED", 11.5, RED, bold=True)
    d.tag(110, 150, "+5 V", PWR)
    d.ln(110, 220, 110, 161, PWR)
    d.txt(118, 206, "VCC", 11.5, PWR, bold=True)
    d.ln(110, 410, 110, 430)
    d.gnd(110, 430, "GND before VCC")

    d.ln(300, 300, 560, 300, BLUE)
    d.txt(296, 296, "TRIG", 12, BLUE, "end", bold=True)
    d.txt(306, 292, "12 us burst (10 us minimum)", 11, BLUE)
    d.ln(300, 340, 430, 340, BLUE)
    d.txt(296, 344, "ECHO", 12, BLUE, "end", bold=True)
    d.resistor(430, 332, 56, 16, "2 k")
    d.ln(486, 340, 560, 340, BLUE)
    d.dot(500, 340, BLUE)
    d.ln(500, 340, 500, 372)
    d.resistor(492, 372, 16, 44, "3.3 k", vertical=True)
    d.ln(500, 416, 500, 426)
    d.gnd(500, 426)
    d.txt(300, 470, "divider node = 3.00 V. 5 V straight into GPIO26 destroys the pin.",
          11.5, RED, wrap_px=235)

    # ---- probe -------------------------------------------------------------
    d.box(40, 500, 200, 130, fill="#eaf2ea", stroke=GREEN, sw=1.6, rx=6)
    d.txt(52, 524, "probe: face UP, IP67", 12.5, GREEN, bold=True)
    d.txt(52, 544, "21 mm transducer in the", 11, GREY)
    d.txt(52, 560, "100 mm lid hole", 11, GREY)
    d.txt(52, 576, "face 150 mm above the lid", 11, GREY)
    d.txt(52, 600, "no mesh, no screen over it", 11, RED, bold=True)
    d.txt(400, 520, "1 m 4-wire cable, serviceable:", 11, GREY)
    d.txt(400, 536, "the probe comes out dry", 11, GREY)

    # ---- battery sense -----------------------------------------------------
    d.tag(330, 580, "pack + / 5 V", PWR)
    d.ln(330, 591, 330, 606, PWR)
    d.resistor(322, 606, 16, 44, "1 M", vertical=True)
    d.ln(330, 650, 330, 662)
    d.dot(330, 662)
    d.ln(330, 662, 560, 662)
    d.ln(330, 662, 330, 668)
    d.resistor(322, 668, 16, 44, "1 M", vertical=True)
    d.ln(330, 712, 330, 722)
    d.gnd(330, 722)
    d.txt(380, 716, "2:1 sense: 3.3 V full scale reads 0..6.6 V, doubled in firmware", 11, GREY,
          wrap_px=170)

    # ---- ESP32 -------------------------------------------------------------
    d.box(560, 220, 280, 560, fill="#eef4fa", sw=2)
    d.txt(574, 246, "ESP32-WROOM-32 DevKitC V1", 15, INK, bold=True)
    d.txt(574, 264, "38-pin, CP2102, USB-C", 11.5, GREY)
    d.tag(640, 180, "3V3", ORANGE)
    d.ln(640, 220, 640, 191, ORANGE)
    d.txt(648, 214, "3V3 pin", 11, ORANGE, bold=True)

    d.dot(560, 300, GND, 2.4)
    d.txt(546, 294, "GPIO27", 12, BLUE, "end", bold=True)
    d.dot(560, 340, GND, 2.4)
    d.txt(546, 334, "GPIO26", 12, BLUE, "end", bold=True)
    d.dot(560, 662, GND, 2.4)
    d.txt(546, 656, "GPIO36", 12, BLUE, "end", bold=True)
    d.txt(546, 672, "ADC1, input only", 10.5, GREY, "end")
    d.txt(574, 700, "never wire anything here", 12, RED, bold=True)
    d.txt(574, 718, "D6 D7 D8 D9 D10 D11", 12.5, RED, mono=True, bold=True)
    d.txt(574, 734, "= SPI flash, broken out on this board", 11, RED)
    d.txt(574, 762, "strapping pins 0  2  12  15: boot mode", 11, RED)

    # power and ground on the right edge
    d.ln(840, 260, 870, 260, PWR)
    d.ln(870, 260, 870, 240, PWR)
    d.tag(870, 228, "+5 V", PWR)
    d.txt(844, 254, "VIN", 12, PWR, bold=True)
    d.ln(840, 620, 880, 620)
    d.gnd(880, 620)
    d.txt(844, 614, "GND", 12, GND, bold=True)

    # I2C up the 888/896 corridor into the OLED
    d.ln(840, 340, 888, 340, BLUE)
    d.ln(888, 340, 888, 310, BLUE)
    d.ln(888, 310, 930, 310, BLUE)
    d.txt(844, 334, "SDA 21", 11.5, BLUE, bold=True)
    d.ln(840, 370, 896, 370, BLUE)
    d.ln(896, 370, 896, 340, BLUE)
    d.ln(896, 340, 930, 340, BLUE)
    d.txt(844, 364, "SCL 22", 11.5, BLUE, bold=True)
    d.dot(930, 310, GND, 2.4)
    d.dot(930, 340, GND, 2.4)

    # buzzer on GPIO13 through an NPN
    d.txt(844, 424, "GPIO13", 11.5, BLUE, bold=True)
    d.ln(840, 430, 850, 430, BLUE)
    d.resistor(850, 422, 52, 16, "220", below=True)
    d.ln(902, 430, 920, 430)
    d.ln(920, 412, 920, 448, INK, 2)
    d.ln(920, 420, 952, 402)
    d.ln(920, 440, 952, 458)
    d.poly([(946, 450), (952, 458), (942, 457)], color=INK, w=1.2, fill=INK)
    d.ln(952, 458, 952, 466)
    d.gnd(952, 466)
    d.txt(966, 462, "BC547 / S8050", 11.5, INK, bold=True)
    d.ln(952, 402, 1090, 402)
    d.box(1090, 392, 90, 52, fill="#ffffff", stroke=PWR, sw=1.6, rx=6)
    d.txt(1135, 414, "piezo", 12, PWR, "middle", bold=True)
    d.txt(1135, 430, "buzzer", 11.5, PWR, "middle")
    d.ln(1180, 402, 1192, 402, PWR)
    d.tag(1206, 402, "5 V", PWR)

    # button on GPIO12
    d.txt(844, 484, "GPIO12", 11.5, BLUE, bold=True)
    d.ln(840, 490, 900, 490, BLUE)
    d.box(900, 476, 22, 28, fill="#ffffff", sw=1.4, rx=2)
    d.ln(911, 504, 911, 518)
    d.gnd(911, 518)
    d.txt(940, 494, "tact switch", 11.5, INK)

    # full-tank LED on GPIO14
    d.txt(844, 544, "GPIO14", 11.5, BLUE, bold=True)
    d.ln(840, 550, 850, 550, BLUE)
    d.resistor(850, 542, 52, 16, "220", below=True)
    d.ln(902, 550, 916, 550, GREEN)
    d.poly([(916, 538), (916, 562), (940, 550)], color=GREEN, w=1.8, fill="none")
    d.ln(928, 532, 950, 522, GREEN, 1.4)
    d.ln(932, 540, 954, 530, GREEN, 1.4)
    d.ln(940, 550, 940, 566)
    d.gnd(940, 566)
    d.txt(966, 554, "green LED, anode to the resistor", 11, GREEN)

    # ---- OLED --------------------------------------------------------------
    d.box(930, 250, 270, 120)
    d.txt(944, 274, "0.96 in SSD1306 OLED 128x64", 13.5, INK, bold=True)
    d.txt(944, 292, "I2C 0x3C, driver in wlg_display.h", 11, GREY)
    d.txt(944, 308, "no library, no pull-ups needed", 11, GREY)
    d.tag(1010, 232, "3V3", ORANGE)
    d.ln(1010, 250, 1010, 243, ORANGE)
    d.txt(1018, 248, "VCC", 11.5, ORANGE, bold=True)
    d.ln(960, 370, 960, 384)
    d.gnd(960, 384)
    d.txt(974, 388, "GND", 11.5, GND, bold=True)

    d.legend(40, 810, 1160, [
        "ECHO goes through 2 k in series and 3.3 k to GND, node 3.00 V. Power the module from 3V3 "
        "instead and you can delete both resistors, but you give up acoustic margin.",
        "GPIO 6-11 (silkscreen D6..D11) are the SPI flash lines: they look usable and are not.",
        "Mode 1 needs pad R19 unpopulated. A fitted resistor puts the module in a serial mode that "
        "never answers a TRIG pulse.",
        "Button: internal pull-up, idle HIGH, pressed GND. GPIO12 is a strapping pin, so never hold "
        "it through a reset - GPIO23 is the safe alternative, one line in the sketch (PIN_BTN).",
        "Buzzer: tone() at 1.8 kHz for low level, 2.6 kHz for high or fault, 250 ms every 9 s, "
        "driven by the NPN from GPIO13 with a 220 base resistor and a diode across the buzzer.",
        "LED on GPIO14: heartbeat every 1.5 s, solid on when the tank is full.",
        "Never screen the aperture - a flyscreen blocks 40 kHz entirely. Use the cowl.",
        "This gauge reports. It must never be wired to switch a pump, valve or load.",
    ])
    return d.save("01-wiring.svg")


def build_02():
    d = Doc(1240, 900, "02 - rev B mounting section: 100 mm hole, 150 mm standoff, 75 deg cone", [
        "Tank measured, not guessed: 304.8 x 304.8 x 121.9 cm internal, full line 15.2 cm below the "
        "lid, 92 903 cm2 of area = 92.9 L per cm of level = 11 327 L to the full line.",
        "Every distance below is face to surface, which is exactly what the e and f calibration "
        "commands store.",
    ])
    S, xmid, yface = 1.2, 470, 380
    ylid, yfull = yface + 18.0, yface + 36.2
    yfloor = yfull + 146.3
    half = 152.4 * S

    def hw(y):
        return (y - yface) * 0.767

    d.poly([(xmid - half, yfull), (xmid - half, yfloor), (xmid + half, yfloor), (xmid + half, yfull)],
           color=INK, w=2, fill="none")
    d.box(xmid - half, yfloor, 2 * half, 14, fill="#c9d3de", sw=1.2, rx=4)
    d.txt(xmid - half + 8, yfloor - 8, "tank floor", 11.5, GREY)
    d.box(xmid - half, ylid, 2 * half, 10, fill="#b9c4d0", sw=1.2, rx=4)
    d.txt(xmid - half + 8, ylid - 6, "lid", 11.5, GREY)
    hole = 6.0
    d.box(xmid - hole, ylid, 2 * hole, 10, fill="#ffffff", sw=1.0, rx=4)
    d.ln(xmid - hole - 6, ylid, xmid - hole - 6, yface + 30)
    d.ln(xmid + hole + 6, ylid, xmid + hole + 6, yface + 30)
    d.txt(xmid + 40, yface + 10, "100 mm hole, 150 mm stub pipe", 11.5, INK, wrap_px=140)

    d.poly([(xmid - half + 2, yfull), (xmid + half - 2, yfull), (xmid + half - 2, yfloor - 1),
            (xmid - half + 2, yfloor - 1)], color=INK, w=0, fill="#dceaf6")
    d.ln(xmid - half + 2, yfull, xmid + half - 2, yfull, "#2a7fbf", 2.2)
    d.txt(xmid - half + 10, yfull + 22, "water surface: the reflector", 12, "#1d5f92", bold=True)

    d.box(xmid - 15, yface, 30, 16, fill="#cde6cd", stroke=GREEN, sw=1.8)
    d.txt(xmid, yface - 12, "SR04M probe face, straight down", 12, GREEN, "middle", bold=True)

    d.poly([(xmid, yface), (xmid - hw(yfull), yfull), (xmid + hw(yfull), yfull)],
           color=BLUE, w=1.2, fill="#cfe2f2", dash="6 4")
    d.txt(xmid + hw(yfull) + 14, yfull + 22, "beam 46 cm wide at the full line", 11.5, BLUE)
    d.poly([(xmid, yface), (xmid - hw(yfloor), yfloor), (xmid + hw(yfloor), yfloor)],
           color=GREY, w=1.2, fill="none", dash="6 4")
    d.txt(xmid - hw(yfloor), yfloor + 26, "beam 233 cm wide at an empty floor, still inside the "
                                          "304.8 cm tank", 11.5, GREY, wrap_px=300)

    d.box(xmid - 31, yface, 62, 24, fill="#f6dede", stroke=RED, sw=1.2, dash="4 3")
    d.txt(xmid - 42, yface + 8, "blind 20 cm", 11.5, RED, "end")
    d.txt(xmid - 42, yface + 22, "gate 25 cm", 11.5, RED, "end")

    d.dim(xmid + 250, yface, yfull, "30.2 cm", "face to full line", color=GREEN)
    d.dim(xmid + 390, yface, yfloor, "152.1 cm", "face to empty floor", color=BLUE)
    d.dim(xmid + 545, yfull, yfloor, "121.9 cm", "span = level 100 %")
    d.dim(xmid - half - 47, yface, ylid, "15.0 cm", "face above lid", anchor="end", dx=-8,
          lbl_dy=-9)
    d.dim(xmid - half - 107, ylid, yfull, "15.2 cm", "lid to full line", anchor="end", dx=-8,
          lbl_dy=30)

    d.box(40, yface - 96, 200, 78, fill="#f4f8fb", sw=1.5)
    d.txt(52, yface - 74, "electronics box", 12.5, INK, bold=True)
    d.txt(52, yface - 56, "ESP32, OLED, buzzer,", 11, GREY)
    d.txt(52, yface - 40, "boost, button, LED", 11, GREY)
    d.txt(52, yface - 24, "4-wire cable down the stub", 11, BLUE)

    d.poly([(xmid - 90, yface - 26), (xmid + 90, yface - 26), (xmid + 70, yface - 50),
            (xmid - 70, yface - 50)], color=ORANGE, w=1.6, fill="#fbf1de")
    d.txt(xmid, yface - 62, "cowl: sun, rain, insects. NEVER a screen across the aperture: "
                            "40 kHz will not pass a mesh.", 12, RED, "middle", bold=True, wrap_px=430)

    d.legend(40, 660, 560, [
        "Full line 30.2 cm below the face: 5.2 cm of headroom over the 25 cm gate. That headroom "
        "covers swell, splash and a film on the face; it does not cover an overfilled tank.",
        "Above 30.2 cm the reading is reported FULL and clamps at 121.9 cm: a report, not an interlock.",
        "A narrow pipe, collar or cowl lip close above the cone turns the lid into a resonant clutter chamber. Keep the aperture clear and wide.",
        "Lid clutter reflects at 15-19 cm, inside the 20 cm blind area, so it cannot reach the beam.",
    ], title="Why 100 mm and 150 mm")
    d.legend(640, 660, 560, [
        "Face parallel to the water within a few degrees: the cone must put a return on the "
        "transducer, not on the far wall.",
        "Wipe the face at every site visit - dust and cobwebs bias it near, dew rejects the ping.",
        "Check pad R19 unpopulated before sealing the box.",
        "Probe cable serviceable: the probe must come out without draining the tank.",
    ], title="On the tank")
    return d.save("02-mounting.svg")


# --------------------------------------------------------------------------
# 03 — power
# --------------------------------------------------------------------------
def build_03():
    d = Doc(1240, 980, "03 - rev B power: panel, 1S Li-ion MPPT charge, boost, load", [
        "Everything runs on the 5 V boost rail: ESP32, OLED, piezo buzzer and the SR04M-family "
        "module. There is no 12 V box and no battery sense at the pack unless you add the second "
        "divider documented in 00-design.md.",
    ])

    def block(x, y, w, h, title, lines, stroke=INK, fill=FILL):
        d.box(x, y, w, h, fill=fill, stroke=stroke, sw=1.6, rx=6)
        d.txt(x + 12, y + 24, title, 13, INK, bold=True)
        yy = y + 42
        for ln in lines:
            d.txt(x + 12, yy, ln, 11, GREY)
            yy += 16

    # solar -> charge -> battery -> boost -> load
    block(40, 150, 170, 96, "5 W / 6 V panel", ["open circuit ~7 V", "short circuit ~0.95 A",
                                                "250-300 mA in sun"])
    d.tag(280, 178, "0.5 A fuse", PWR)
    d.ln(210, 178, 245, 178, PWR)
    d.txt(214, 172, "PV +", 11, PWR)
    d.ln(210, 210, 240, 210)
    d.gnd(240, 210, "PV -", "start")
    d.tag(280, 262, "1 A polyfuse", PWR)
    d.txt(280, 296, "panel short-circuit path", 10.5, GREY, "middle")

    block(340, 130, 190, 150, "CN3791 MPPT", ["CV set to 4.20 V, 1S", "CC set by Rs, ~1 A",
                                              "12 V panel OK", "TEMP pin: stop under 0 C",
                                              "red charge, green full"])
    d.ln(315, 178, 340, 178, PWR)
    d.ln(530, 178, 570, 178, PWR)
    d.tag(550, 170, "0.5 A", PWR)

    block(570, 130, 200, 150, "18650 x2 parallel", ["protected cells only", "4.2 V top, 2.8 V cut",
                                                    "true capacity 1.2-3.5 Ah",
                                                    "never mismatched",
                                                    "check with a multimeter"], stroke=PWR)
    d.ln(670, 280, 670, 300)
    d.gnd(670, 300, "pack -")
    d.txt(700, 300, "1 M / 1 M sense to GPIO36", 11, GREY)

    block(830, 130, 190, 150, "MT3608 boost", ["to 5.0 V, trim with the", "25 k trimpot",
                                               "no soft start", "input spikes brown out",
                                               "the ESP32 under Wi-Fi TX"], stroke=PWR)
    d.ln(770, 178, 830, 178, PWR)
    d.ln(1020, 178, 1180, 178, PWR, 2)

    d.tag(1150, 148, "+5 V rail", PWR)
    d.ln(1150, 159, 1150, 178, PWR)
    d.cap(1050, 178, "100 nF")
    d.cap(1120, 178, "470 uF")

    block(1040, 300, 160, 150, "the load", ["ESP32 + OLED", "+ SR04M module", "+ piezo buzzer",
                                            "+ button, LED", "the 5.0 V rail"], stroke=BLUE)
    d.txt(1036, 170, "cap at the load", 11, GREY)
    d.ln(1180, 178, 1180, 300, PWR)

    # bench branch
    d.box(40, 360, 380, 150, fill="#f2f6f2", stroke=GREEN, sw=1.6, rx=6)
    d.txt(54, 386, "bench: never lithium without a current limit", 13, GREEN, bold=True)
    d.txt(54, 410, "USB-C into the DevKit is fine for firmware work, but the CP2102 port cannot "
                   "feed the boost rail: the module and buzzer brown the board out.", 11.5,
          wrap_px=350)
    d.txt(54, 462, "Bench power = lab supply, 4.2 V, current limit 500 mA, into the boost input or "
                   "the battery pads.", 11.5, wrap_px=350)
    d.txt(54, 500, "A TP4056 module (no protection) is a bench charger only, never in the box.",
          11.5, RED, wrap_px=350)

    # budget table
    d.panel(40, 540, 560, "Measured and expected budget")
    rows = [
        ("state", "draw", "where it comes from"),
        ("WiFi on, modem sleep", "20-25 mA", "the working point, 30 s reads"),
        ("modem sleep off, OLED lit", "60-95 mA", "what the idle blanking avoids"),
        ("light sleep between reads", "4-8 mA", "phase-2 board, not a DevKitC"),
        ("SR04M while pinging", "< 8 mA", "module spec, on top of the ESP32"),
        ("WiFi TX burst", "~350 mA peak", "why the bulk cap matters"),
        ("buzzer beat", "+30 mA", "bench, HC-SR04 rehearsal"),
        ("panel into 6 Ah", "~1.5-3 h", "5 W in real sun, MPPT"),
        ("autonomy at 22 mA", "11-12 d", "6 Ah pack, panel doing nothing"),
    ]
    endy = d.table(40, 590, rows, [0, 190, 330])
    d.txt(40, endy + 26, "The 24 h column test in 02-phase0-test-plan.md is what turns these "
                         "targets into numbers.", 11.5, GREY, wrap_px=520)

    y = d.legend(640, 540, 560, [
        "Boost converters have no soft start and no undervoltage lockout worth the name: an empty "
        "pack plus a Wi-Fi TX burst is the classic 'it boots then dies' fault.",
        "The module's transmit burst is acoustic but its current step is electrical: keep the 100 nF "
        "and 100 uF at its own pins, away from the ESP32's.",
        "Li-ion cells that are not genuinely protected, or two cells of different age or brand, are "
        "a fire risk and a mystery bug generator.",
        "470 uF on the 5 V rail is not decoration: it is what keeps the SR04M echo timing valid "
        "during a TX burst.",
        "00-design.md keeps a high-side load switch (TPS22919) on the module's 5 V rail on the "
        "phase-1 PCB, to kill its 2 mA standby between reads. The bench build hard-wires it.",
    ], title="Why the power supply is a sensor problem")
    return d.save("03-power.svg")


# --------------------------------------------------------------------------
# 04 — display, alarms and app
# --------------------------------------------------------------------------
def build_04():
    d = Doc(1240, 1040, "04 - what it shows, what it shouts, what the app adds", [
        "The OLED, buzzer, LED and web page are local-first: with no Blynk token, no Wi-Fi and no "
        "cloud the gauge still measures, displays and alarms.",
        "Blynk is additive, and one rule protects the free plan: V1 (percent) is written every "
        "sample and everything else is throttled.",
    ])

    # ---- the panel --------------------------------------------------------
    x, y, w, h = 40, 130, 520, 262
    d.box(x - 6, y - 6, w + 12, h + 12, fill="#22282e", stroke=INK, sw=1.4, rx=10)
    d.box(x, y, w, h, fill=OLED_BG, stroke="#3a4450", sw=1, rx=2)
    d.txt(x + 26, y + 92, "118", 84, OLED_FG, bold=True, mono=True)
    d.txt(x + 240, y + 92, "cm", 28, OLED_FG, bold=True)
    d.txt(x + w - 20, y + 42, "96.8%", 27, OLED_FG, "end", bold=True)
    d.txt(x + w - 20, y + 78, "10962 L", 21, OLED_FG, "end", bold=True)
    d.box(x + 8, y + 106, w - 16, 16, fill="#2b333b", stroke="#4a5560", sw=1, rx=2)
    d.box(x + 8, y + 106, (w - 16) * 0.968, 16, fill=OLED_FG, stroke=OLED_FG, sw=1, rx=2)
    d.txt(x + 10, y + 142, "BAT 4.02V 82%", 16, OLED_FG, bold=True)
    d.txt(x + w - 10, y + 142, "-54 dBm", 14, OLED_FG, "end", mono=True)
    d.txt(x + 10, y + 172, "ECHO 33.8", 15, OLED_FG, mono=True)
    d.txt(x + w - 10, y + 172, "SPAN 121.9", 15, OLED_FG, "end", mono=True)
    d.txt(x + 10, y + 198, "GATE 25.0 T30C", 15, OLED_FG, mono=True)
    d.txt(x + w - 10, y + 198, "4 bad", 15, OLED_FG, "end", mono=True)
    d.box(x + 4, y + 210, w - 8, 44, fill=OLED_FG, stroke=OLED_FG, sw=1, rx=2)
    d.txt(x + w / 2, y + 240, "FULL - OVERFLOW", 26, OLED_BG, "middle", bold=True)

    d.txt(x, y + h + 40, "Panel rows, 128 x 64, 0.96 in", 13.5, INK, bold=True)
    notes = [
        "big: level in cm, top right: percent then litres",
        "bar: percent of span, 10 px tall on the real panel",
        "BAT rail voltage and percent, then RSSI or NO WIFI",
        "ECHO is the filtered distance the module measured in cm, SPAN the calibrated span, GATE "
        "the blind gate, T30C the configured air temperature that sets sound speed, and the right "
        "field counts rejected pings",
        "bottom band is the status string, inverted while any alarm is on",
        "blanked after OLED_IDLE_S (120 s) idle; the button wakes it",
    ]
    yy = y + h + 62
    for n in notes:
        for ln in d.wrap(n, 520, 11.5):
            d.txt(x, yy, ln, 11.5)
            yy += 16

    # ---- alarms -----------------------------------------------------------
    d.panel(600, 130, 600, "Status strings and local alarms, no cloud involved")
    rows = [
        ("LOW LEVEL", "level < low_cm, default 24 cm"),
        ("FULL - OVERFLOW", "level > high_cm, default 110 cm"),
        ("SENSOR FAULT", "3 hard rejects in a row"),
        ("MANY BAD READS", "over 25% of pings rejected"),
        ("NO ECHO / OUT OF RANGE", "nothing back, or back after 40 ms"),
        ("NEAR-BLIND / DEW ON FACE", "an echo inside the 25 cm gate"),
        ("WARMING UP", "fewer than 3 samples taken yet"),
    ]
    end = d.table(600, 184, [("OLED status line", "what sets it"), ] + rows, [0, 230])
    d.txt(600, 354, "The four Blynk events are level_low, level_high, sensor_fault and level_ok. "
                    "Clearing is hysteretic at 2 cm (ALARM_HYST_CM) and the push is rate limited to "
                    "one per event per 30 min (ALARM_COOLDOWN_S 1800).", 11.5, GREY, wrap_px=590)

    d.panel(600, 400, 600, "What the operator does")
    rows = [
        ("short press", "wakes the OLED, snoozes one 9 s period"),
        ("long press, 1.5 s", "mute latch in NVS; also j or app V13"),
        ("LED, GPIO14", "toggles every 1.5 s, solid on full alarm"),
        ("buzzer, GPIO13", "250 ms every 9 s: 1.8 kHz low, else 2.6 kHz"),
    ]
    end = d.table(600, 454, rows, [0, 170], size=11.5)

    # ---- Blynk ------------------------------------------------------------
    d.panel(40, 580, 560, "Blynk template: 13 streams, 4 events")
    rows = [
        ("V0", "level_cm", "every sample if slow"),
        ("V1", "level_pct", "every sample, always"),
        ("V2", "litres", "every sample if slow"),
        ("V3", "dist_cm", "hourly"),
        ("V4", "vbat", "every sample if slow"),
        ("V8", "status text", "only on change"),
        ("V9", "rssi", "hourly"),
        ("V10", "sample_s", "written by the app"),
        ("V11", "low_cm", "written by the app"),
        ("V12", "high_cm", "written by the app"),
        ("V13", "mute", "written by the app"),
        ("V14", "bad_pct", "hourly"),
        ("V15", "uptime_h", "hourly"),
    ]
    end = d.table(40, 634, rows, [0, 70, 230], size=11.5)
    d.txt(430, 634, "V5 V6 V7 V16 were the air, water and barometric streams of rev A. Delete them "
                    "in the template; do not renumber the rest.", 11.5, RED, wrap_px=150)
    d.txt(430, 730, "Events: level_low, level_high, sensor_fault, level_ok", 11.5, wrap_px=150)
    d.txt(430, 810, "Free plan: 100 000 device messages per month. 300 s sampling is about 38 000, "
                    "60 s about 45 000, 900 s about 15 000.", 11.5, GREY, wrap_px=150)

    # ---- app --------------------------------------------------------------
    d.box(640, 580, 240, 360, fill="#eef4fa", sw=1.6, rx=14)
    d.txt(760, 616, "Water tank", 15, INK, "middle", bold=True)
    d.ln(660, 630, 860, 630, "#cfd8e2", 1)
    d.box(660, 646, 200, 96, fill="#ffffff", stroke="#cfd8e2", sw=1.2, rx=8)
    d.txt(760, 690, "96.8%", 30, BLUE, "middle", bold=True)
    d.txt(760, 714, "level, V1", 11, GREY, "middle")
    d.box(660, 756, 96, 76, fill="#ffffff", stroke="#cfd8e2", sw=1.2, rx=8)
    d.txt(708, 790, "10962", 18, INK, "middle", bold=True)
    d.txt(708, 810, "litres", 11, GREY, "middle")
    d.box(764, 756, 96, 76, fill="#ffffff", stroke="#cfd8e2", sw=1.2, rx=8)
    d.txt(812, 790, "4.02 V", 16, INK, "middle", bold=True)
    d.txt(812, 810, "V4", 11, GREY, "middle")
    d.box(660, 846, 200, 56, fill="#f6dede", stroke=RED, sw=1.2, rx=8)
    d.txt(760, 868, "FULL - OVERFLOW", 14, RED, "middle", bold=True)
    d.txt(760, 886, "push, once per 30 min", 11, RED, "middle")

    # ---- LAN --------------------------------------------------------------
    d.panel(920, 580, 280, "LAN, no cloud")
    d.txt(920, 616, "http://<ip>/  live page, 2 s refresh", 11.5, INK, wrap_px=280)
    d.txt(920, 648, "/api/state  JSON snapshot", 11.5, INK)
    d.txt(920, 672, "/api/log  force one sample now", 11.5, INK)
    d.txt(920, 696, "/api/scan  Wi-Fi survey, SSID,RSSI", 11.5, INK)
    d.txt(920, 730, "Keys include echo_us, echo_reason,", 11.5, GREY, wrap_px=280)
    d.txt(920, 762, "gate_cm, span_cm, n_near, n_noecho,", 11.5, GREY, wrap_px=280)
    d.txt(920, 778, "n_far, bad, dist_cm, sound_mms", 11.5, GREY, wrap_px=280)
    d.txt(920, 812, "Open it on a phone on the same SSID:", 11.5, GREY, wrap_px=280)
    d.txt(920, 828, "no Blynk, no port forwarding.", 11.5, GREY, wrap_px=280)

    d.legend(640, 920, 560, [
        "Blynk costs +748 B flash and +152 B RAM with a blank token: it is deliberately compiled in "
        "and switched off at runtime, so keep the include visible or the library auto-discovery "
        "quietly drops it.",
    ], title="Measured, not guessed")
    return d.save("04-display-and-app.svg")


BUILDERS = {"01": build_01, "02": build_02, "03": build_03, "04": build_04}

if __name__ == "__main__":
    for k in (["01", "02", "03", "04"] if len(sys.argv) < 2 else sys.argv[1:]):
        print(BUILDERS[k]())
