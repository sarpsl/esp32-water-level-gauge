import math, pathlib

S = 1.6  # px per cm
W, H = 1240, 862

# tank geometry in cm
inner_w, depth, gap, chim_h, box_h, face_above_full = 304.8, 121.9, 15.2, 25.0, 12.0, 52.2
blind, beam_half_deg = 35.0, 6.0

# canvas anchors
X0 = 300.0                    # inner left wall
YF = 470.0                    # full water line
CX = X0 + inner_w * S / 2     # tank centre x
Yfloor = YF + depth * S
Ylid_bot = YF - gap * S
Ylid_top = Ylid_bot - 8 * S
Ychim_top = Ylid_top - chim_h * S
Yface = Ychim_top - box_h * S
wall = 20 * S
floor_t = 30 * S

MONO = dict(md=12, sm=11.5, d=12.5, dd=12, lbl=13, mod=14, bad=13, ttl=22, sub=13)

def esc(t):
    return t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

class SVG:
    def __init__(s): s.o = []
    def t(s, x, y, txt, cls='d', anchor='start'):
        s.o.append(f'<text x="{x:.0f}" y="{y:.0f}" class="{cls}" text-anchor="{anchor}">{esc(txt)}</text>')
    def ln(s, x1, y1, x2, y2, stroke='#0b5cad', w=1.4, dash=''):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        s.o.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{w}"{d}/>')
    def rect(s, x, y, w, h, fill, stroke=None, sw=1.2, rx=0):
        st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ''
        r = f' rx="{rx}"' if rx else ''
        s.o.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{fill}"{st}{r}/>')
    def path(s, d, fill='none', stroke='none', w=1.4, dash=''):
        dd = f' stroke-dasharray="{dash}"' if dash else ''
        s.o.append(f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{w}"{dd}/>')

g = SVG()

# ---------- concrete, water, lid ----------
g.rect(X0 - wall, YF, wall, Yfloor - YF + floor_t, '#b9b4ad', '#6f6a63')          # left wall + floor corner
g.rect(X0 + inner_w * S, YF, wall, Yfloor - YF + floor_t, '#b9b4ad', '#6f6a63')   # right wall
g.rect(X0 - wall, Yfloor, inner_w * S + 2 * wall, floor_t, '#b9b4ad', '#6f6a63')  # floor
g.rect(X0, YF, inner_w * S, Yfloor - YF, '#bfe0f2', '#5b93b5')                    # water
wav = f'M{X0:.0f} {YF:.1f} '
step = inner_w * S / 12
for i in range(12):
    wav += f'q {step/4:.1f} {-5 if i % 2 == 0 else 5} {step/2:.1f} 0 t {step/2:.1f} 0 '
g.path(wav, stroke='#5b93b5')
g.rect(X0 - wall - 12, Ylid_top, inner_w * S + 2 * wall + 24, Ylid_bot - Ylid_top, '#5d6672', '#333')  # lid
g.ln(X0 - wall - 12, Yfloor + floor_t, X0 + inner_w * S + wall + 12, Yfloor + floor_t, '#888', 1.0)     # ground

# ---------- chimney + box (true scale) ----------
bore = 6 * S
g.rect(CX - bore / 2 - 3, Ychim_top, bore + 6, Ylid_top - Ychim_top, '#f7e7c6', '#8a6a2f', 1.4)
g.rect(CX - 8 * S, Yface, 16 * S, Ychim_top - Yface, '#f2d9a8', '#8a6a2f', 1.6)
g.rect(CX - 11, Yface - 4, 22, 4, '#0b7d3f')                       # transducer face

# OLED window on the box's shaded side wall — real panel is 2.7 × 1.4 cm active area,
# drawn about 2× over size because at 1.6 px/cm it would be 4 px tall.
g.rect(CX - 8 * S - 7, Yface + 0.6 * S, 7, 11, '#101418', '#0b7d3f', 1.2, rx=1)
g.ln(CX - 8 * S - 7, Yface + 6, CX - 8 * S - 96, Yface - 34, '#0b5cad', 1.2)
g.t(CX - 8 * S - 100, Yface - 38, 'OLED window on the shaded side', 'lbl', 'end')
g.t(CX - 8 * S - 100, Yface - 22, '0.96" SSD1306, same I²C bus', 'sm', 'end')
g.t(CX - 8 * S - 100, Yface - 8, 'sunlight washes it out — face it', 'sm', 'end')
g.t(CX - 8 * S - 100, Yface + 6, 'away from the sun, or blank it', 'sm', 'end')

# ---------- beam cone + blind zone ----------
r_floor = math.tan(math.radians(beam_half_deg)) * (Yfloor - Yface)
r_full = math.tan(math.radians(beam_half_deg)) * (YF - Yface)
r_blind = math.tan(math.radians(beam_half_deg)) * (blind * S)
g.path(f'M{CX-2:.0f} {Yface:.0f} L{CX-r_floor:.0f} {Yfloor:.0f} L{CX+r_floor:.0f} {Yfloor:.0f} L{CX+2:.0f} {Yface:.0f} Z',
       fill='#cfe9f7', stroke='#5b93b5', w=1.0, dash='4 3')
g.path(f'M{CX-2:.0f} {Yface:.0f} L{CX-r_blind:.0f} {Yface+blind*S:.0f} L{CX+r_blind:.0f} {Yface+blind*S:.0f} L{CX+2:.0f} {Yface:.0f} Z',
       fill='url(#hz)', stroke='none')

# wrong sensor position (magnet under the lid)
g.rect(CX - 13, Ylid_bot, 26, 5, '#c0392b')
g.ln(CX, Ylid_bot, CX, Ylid_bot + 14, '#c0392b', 1.4, '3 2')

# ---------- dimensions ----------
def vdim(x, y1, y2, label, sub=None, col='#0b5cad'):
    g.ln(x, y1, x, y2, col)
    g.ln(x - 5, y1, x + 5, y1, col); g.ln(x - 5, y2, x + 5, y2, col)
    g.t(x - 8, (y1 + y2) / 2, label, 'dd', 'end')
    if sub: g.t(x - 8, (y1 + y2) / 2 + 16, sub, 'sm', 'end')
def hdim(y, x1, x2, label, sub=None, col='#0b5cad'):
    g.ln(x1, y, x2, y, col)
    g.ln(x1, y - 5, x1, y + 5, col); g.ln(x2, y - 5, x2, y + 5, col)
    g.t((x1 + x2) / 2, y + 18, label, 'dd', 'middle')
    if sub: g.t((x1 + x2) / 2, y + 34, sub, 'sm', 'middle')

vdim(245, YF, Yfloor, f'{depth:.1f} cm', 'water depth (4 ft)')
vdim(215, Ylid_top, YF, f'{gap:.1f} cm', 'lid gap (6 in)')
vdim(200, Yface, YF, f'{face_above_full:.1f} cm', 'face → full line', col='#0b7d3f')
vdim(762, Yface, Yfloor, f'{face_above_full+depth:.1f} cm', 'face → floor = d_empty')
vdim(470, Ychim_top, Ylid_top, f'{chim_h:.0f} cm', 'chimney Ø60 mm', col='#8a6a2f')
hdim(Yfloor + floor_t + 26, X0, X0 + inner_w * S, f'{inner_w:.1f} cm (10 ft)', 'inner width — same as the depth, so 92.9 L per cm of level')

# ---------- callouts on the drawing ----------
g.t(CX, Yface - 16, 'ultrasonic face at the top of the box', 'dd', 'middle')
g.ln(CX + 60, Yface - 20, CX + 150, Yface - 60, '#0b5cad', 1.2)
g.t(CX + 154, Yface - 64, 'printed box on TOP of the lid,', 'lbl'); g.t(CX + 154, Yface - 48, 'not inside the tank', 'lbl')
g.t(CX + r_blind + 14, Yface + blind * S / 2, 'blind zone ≈35 cm', 'bad'); g.t(CX + r_blind + 14, Yface + blind * S / 2 + 16, 'firmware rejects these', 'sm')
g.t(CX + r_full + 12, YF - 6, f'spot Ø {2*r_full/S:.0f} cm at the full line', 'sm')
g.t(CX + r_floor + 12, Yfloor - 10, f'spot Ø {2*r_floor/S:.0f} cm at the floor', 'sm')
g.t(CX, Yfloor - 26, 'water 121.9 cm deep · 11 327 L full · 92.9 L per cm', 'lbl', 'middle')
g.ln(CX - 15, Ylid_bot + 3, CX - 152, Ylid_bot + 18, '#c0392b', 1.2)
g.t(CX - 156, Ylid_bot + 22, '✗ magnet under the lid:', 'bad', 'end')
g.t(CX - 156, Ylid_bot + 38, '15.2 cm from the water — blind zone', 'sm', 'end')


# ---------- right panels ----------
def panel(x, y, w, h, fill, stroke, title, lines, titlecls='lbl'):
    g.rect(x, y, w, h, fill, stroke, 1.6, 8)
    g.t(x + 16, y + 24, title, titlecls)
    yy = y + 46
    for txt, cls in lines:
        g.t(x + 16, yy, txt, cls); yy += 19 if cls != 'sm' else 17

PX, PW = 930, 280
panel(PX, 84, PW, 236, '#fff4f4', '#a30000', '✗ not under the lid', [
 ('15.2 cm face-to-water is inside', 'md'),
 ('the blind zone, so the sensor', 'md'),
 ('returns nothing or noise.', 'sm'),
 ('a metal lid plus rebar concrete', 'md'),
 ('walls form a partial Faraday cage', 'sm'),
 ('— WiFi dies inside the tank.', 'sm'),
 ('≈100 % RH inside: condensation', 'md'),
 ('cycles through a printed box and', 'sm'),
 ('corrodes a bare DevKitC in weeks.', 'sm'),
 ('no service access, no desiccant', 'md'),
 ('change, no room for an antenna.', 'sm'),
], 'bad')
panel(PX, 336, PW, 250, '#f4f8ff', '#0b5cad', 'mounting checklist', [
 ('□ face ≥ 40 cm above full line', 'md'),
 ('□ cone clears the 6 in lid gap', 'md'),
 ('□ RSSI ≥ −75 dBm, lid closed', 'md'),
 ('□ box in shade, vents down', 'md'),
 ('□ antenna not shadowed by the lid', 'md'),
 ('□ chimney bore ≥ 60 mm, smooth', 'md'),
 ('□ OLED out of direct sun', 'md'),
 ('', 'sm'),
 ('beam spread ≈5–8° half angle:', 'md'),
 ('the cone clears the lid gap and', 'sm'),
 ('lands in the middle of the tank,', 'sm'),
 ('so there is no wall echo.', 'sm'),
])
panel(PX, 592, PW, 234, '#fffdf3', '#8a6a2f', 'calibration values', [
 ('d_empty = 174.0 cm', 'md'),
 ('d_full  = 52.2 cm', 'md'),
 ('area    = 92903 cm²', 'md'),
 ('level = d_empty − distance', 'md'),
 ('0…121.9 cm → 0…11 327 L', 'md'),
 ('alarm defaults: low 24 cm', 'md'),
 ('(≈20 %), high 120 cm', 'md'),
 ('', 'sm'),
 ('measure with the serial e / f', 'sm'),
 ('commands: the chimney height you', 'sm'),
 ('actually print decides both.', 'sm'),
])

# ---------- header ----------
body = '\n'.join(g.o)
head = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans, Arial, sans-serif">
<defs><style>
 .ttl{{font-size:22px;font-weight:700;fill:#111}} .sub{{font-size:13px;fill:#555}}
 .mod{{font-size:14px;font-weight:700;fill:#111}} .lbl{{font-size:13px;fill:#222}}
 .d,.md,.sm,.dd,.bad{{font-size:12px;font-family:DejaVu Sans Mono,monospace;fill:#222}}
 .md{{font-size:12px}} .sm{{font-size:11.5px;fill:#666}} .dd{{font-size:12px;font-weight:700;fill:#0b5cad}}
 .bad{{font-size:13px;font-weight:700;fill:#a30000}}
</style>
<pattern id="hz" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
 <line x1="0" y1="0" x2="0" y2="8" stroke="#c0392b" stroke-width="1.6"/></pattern>
</defs>
<rect width="{W}" height="{H}" fill="#fff"/>
<text x="28" y="36" class="ttl">Mounting geometry — the numbers that decide whether this sensor works</text>
<text x="28" y="58" class="sub">Cross-section, scale 1 cm = {S} px (the box and chimney are drawn true size). Distances are transducer face to the calm centre of the water surface.</text>
'''
svg = head + body + '\n</svg>\n'
pathlib.Path('C:/temp/esp32-water-level-guage/docs/img/02-mounting.svg').write_text(svg, encoding='utf-8')
print('written', len(svg.splitlines()), 'lines')
print('geometry: face y=%.1f  lid %.1f..%.1f  floor %.1f' % (Yface, Ylid_top, Ylid_bot, Yfloor))
