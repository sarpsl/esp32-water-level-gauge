"""Static geometry checker for the generated SVG diagrams.

    python check-diagrams.py 01-wiring.svg 02-mounting.svg ...

Reports, per file:
  OOB     text or line outside the viewBox
  T/T     two text labels overlapping
  T/L     a text label sitting on top of a wire/outline
  T/B     a text label wider than the box it belongs to

No ImageMagick on this box and the current model cannot see images, so this is
how the diagrams get verified: character-width arithmetic on every label.
"""
import re
import sys
import pathlib

CW = 0.62          # advance width as a fraction of font size (DejaVu Sans)
MONO = 0.602
HERE = pathlib.Path(__file__).resolve().parent

TEXT = re.compile(
    r'<text x="([-\d.]+)" y="([-\d.]+)" font-size="([-\d.]+)"[^>]*?'
    r'(font-weight="bold")?[^>]*?(font-family="[^"]*")?[^>]*?text-anchor="(\w+)"[^>]*>([^<]*)</text>'
)
LINE = re.compile(r'<line x1="([-\d.]+)" y1="([-\d.]+)" x2="([-\d.]+)" y2="([-\d.]+)"')
RECT = re.compile(r'<rect x="([-\d.]+)" y="([-\d.]+)" width="([-\d.]+)" height="([-\d.]+)"')
HEAD = re.compile(r'width="(\d+)" height="(\d+)" viewBox="0 0 (\d+) (\d+)"')


def boxes(svg):
    out = []
    for m in TEXT.finditer(svg):
        x, y, fs = float(m.group(1)), float(m.group(2)), float(m.group(3))
        mono = "mono" in (m.group(5) or "")
        txt = m.group(7)
        w = len(txt) * fs * (MONO if mono else CW)
        anchor = m.group(6)
        x0 = x - w / 2 if anchor == "middle" else (x - w if anchor == "end" else x)
        out.append((x0, y - 0.74 * fs, x0 + w, y + 0.26 * fs, txt))
    return out


def main(names):
    bad = 0
    for name in names:
        svg = (HERE / name).read_text(encoding="utf-8")
        W, H = (int(v) for v in HEAD.search(svg).groups()[:2])
        tb = boxes(svg)
        lines = [tuple(float(v) for v in m.groups()) for m in LINE.finditer(svg)]
        rects = [tuple(float(v) for v in m.groups()) for m in RECT.finditer(svg)]
        print(f"--- {name}  {W}x{H}  {len(tb)} labels, {len(lines)} lines")

        for x0, y0, x1, y1, t in tb:
            if x0 < 8 or x1 > W - 8 or y0 < 6 or y1 > H - 4:
                bad += 1
                print(f"  OOB  {t[:52]!r}  bbox=({x0:.0f},{y0:.0f})-({x1:.0f},{y1:.0f})")

        for i in range(len(tb)):
            for j in range(i + 1, len(tb)):
                a, b = tb[i], tb[j]
                if a[0] < b[2] - 1 and b[0] < a[2] - 1 and a[1] < b[3] - 1 and b[1] < a[3] - 1:
                    bad += 1
                    print(f"  T/T  {a[4][:34]!r} <> {b[4][:34]!r}")

        for x0, y0, x1, y1, t in tb:
            for lx1, ly1, lx2, ly2 in lines:
                lo_x, hi_x = min(lx1, lx2), max(lx1, lx2)
                lo_y, hi_y = min(ly1, ly2), max(ly1, ly2)
                if hi_x - lo_x < 2 and hi_y - lo_y < 2:
                    continue
                if x0 + 1 < hi_x and lo_x < x1 - 1 and y0 + 1 < hi_y and lo_y < y1 - 1:
                    bad += 1
                    print(f"  T/L  {t[:44]!r} on line ({lo_x:.0f},{lo_y:.0f})-"
                          f"({hi_x:.0f},{hi_y:.0f})")

        # labels that START inside a box must not run past its right edge
        for rx, ry, rw, rh in rects:
            if rw < 60 or rh < 40:
                continue
            inside = [t for t in tb
                      if rx + 2 <= t[0] <= rx + rw - 10 and ry - 2 <= t[1] and t[3] <= ry + rh + 2]
            for t in inside:
                if t[2] > rx + rw - 4:
                    bad += 1
                    print(f"  T/B  {t[4][:48]!r} runs past box x={rx:.0f} w={rw:.0f} "
                          f"by {t[2] - (rx + rw - 4):.0f} px")
    print("OK, no geometry problems" if not bad else f"{bad} problems")
    return bad


if __name__ == "__main__":
    sys.exit(1 if main(sys.argv[1:] or sorted(p.name for p in HERE.glob("[0-9][0-9]-*.svg"))) else 0)
