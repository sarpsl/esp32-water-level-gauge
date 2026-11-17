# The four figures: how they are made

**The SVG is the source of truth. The PNG is a build product.** Edit the generator, never the SVG by
hand, and never the PNG at all.

| figure | source | shows |
|---|---|---|
| `01-wiring` | `gen-diagrams.py` → `build_01()` | bench wiring, the ECHO divider, the dead pins |
| `02-mounting` | `build_02()` | lid hole, standoff, the 75° cone against the tank dimensions |
| `03-power` | `build_03()` | panel → CN3791 → 1S 18650 → MT3608 → 5 V rail, plus the current budget |
| `04-display-and-app` | `build_04()` | what the OLED shows, what the alarms do, what Blynk adds |

## 1. Regenerate

```sh
cd docs/img
python gen-diagrams.py 01 02 03 04        # or a subset: python gen-diagrams.py 04
```

Every number, pin name and V-pin in the figures is written in `gen-diagrams.py` on purpose: the
firmware (`arduino/water_level_gauge/water_level_gauge.ino`) is the thing the figure has to agree
with, and a figure that is generated is diffable against a firmware change.

## 2. Check the geometry before you render

```sh
python check-diagrams.py                  # all four
python check-diagrams.py 04-display-and-app.svg
```

It measures text with character-width arithmetic (there is no font rasteriser here) and reports:

| code | what it means |
|---|---|
| `OOB` | a label, line or polygon edge sits within 12 px of the canvas edge or outside it |
| `T/T` | two labels overlap |
| `T/L` | a label sits on top of a wire or a drawn shape edge |
| `T/B` | a label that starts inside a box runs past that box's right edge |

It must print **`OK, no geometry problems`** before a render is worth looking at. It cannot tell you
whether a drawing is *clear* — only whether it is broken.

## 3. Render the PNG

This box has **no Inkscape, no rsvg, no ImageMagick**. `/c/WINDOWS/system32/convert` is the Windows
disk-conversion tool, not ImageMagick, and must never be run here. Microsoft Edge renders SVG to PNG
well enough:

```sh
EDGE="/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
rm -rf "$TEMP/edgeprofile"          # a fresh profile every time, or a stale cache serves old art
"$EDGE" --headless=new --disable-gpu --hide-scrollbars \
        --user-data-dir="$TEMP/edgeprofile" \
        --force-device-scale-factor=2 \
        --window-size=1240,1040 \
        --screenshot="$TEMP/out.png" \
        "file:///C:/Temp/esp32-water-level-guage/docs/img/04-display-and-app.svg"
```

`--window-size` must equal the SVG's `viewBox` size, and then the output is exactly **2×** it:

| file | SVG canvas | PNG |
|---|---|---|
| `01-wiring` | 1240 × 1080 | 2480 × 2160 |
| `02-mounting` | 1240 × 900 | 2480 × 1800 |
| `03-power` | 1240 × 980 | 2480 × 1960 |
| `04-display-and-app` | 1240 × 1040 | 2480 × 2080 |

If the PNG comes out `1785 x 965` you left `--force-device-scale-factor=2` out and it captured at
96 DPI. If it comes out taller than that with a white band, the browser added a scrollbar — that is
what `--hide-scrollbars` is for. A PNG in the 100–400 KB range with those dimensions is a drawn page;
a ~45 KB one is a blank or partial page.

Commit the SVG **and** the PNG: the Markdown pages embed the PNG so that GitHub, a phone and a
printed PDF all show the same thing without needing an SVG renderer.
