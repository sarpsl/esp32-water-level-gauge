# Third-party material, credits and licences

Read this before you re-use anything from this repository. It says exactly what is
ours, what is somebody else's, and what each piece may be used for.

The short version: **every line of firmware, every diagram and every word of the
docs in this repository was written for this build** and is offered under the MIT
licence in [`LICENSE`](LICENSE) — with **one exception**, the 5×7 glyph table in
`wlg_font.h`, which is Adafruit's and stays BSD-licensed. One earlier project
inspired the *idea* and the **pin numbers**; nothing from it was copied.

---

## 1. What is third-party, and in which file

| what | where it touches this repo | licence | vendored here? |
|---|---|---|---|
| **Adafruit_GFX classic 5×7 font** (`glcdfont.c`, v1.13.0) | `arduino/water_level_gauge/wlg_font.h` — the byte table | BSD (text in §3) | **yes** — redistributed under its own terms, notice kept in-file |
| **Blynk library** (`BlynkSimpleEsp32.h`), Blynk Inc. | `#include` in `water_level_gauge.ino` when `ENABLE_BLYNK 1` | Apache-2.0 | no — you install it (`arduino-cli lib install "Blynk"`) |
| **BlynkNcpDriver** (pulled in by Blynk 1.4.x) | link-time only | Apache-2.0 | no |
| **ESP32 Arduino core** (`Arduino.h`, `Wire`, `WiFi`, `WebServer`, `Preferences`), Espressif Systems | the whole sketch | LGPL-2.1 | no — comes from the board package |
| **SSD1306 controller command set**, Solomon Systech datasheet | the init sequence in `wlg_display.h` | n/a — interface facts | no — no datasheet text or figure reproduced |
| **SR04M-family module datasheets** (AJ-SR04M, JSN-SR04M-2) | spec values quoted as numbers in `docs/00-design.md` §2, `docs/03-ultrasonic.md` §1–2 | n/a — specification facts | **no PDF in this repo**, see §5 |
| **Tech StudyCell / IoT CircuitHub reference build** | pin numbers, part choices, one quoted sentence — see §2 | no licence stated → treated as all rights reserved | **no** — nothing from it is reproduced |
| **DejaVu Sans** glyphs | rendered into the four PNGs in `docs/img/` | free, attribution-only (§6) | no font file shipped |
| vendor listings (Amazon.in, Flipkart, Robu.in, Think Robotics, RoboWay, RoboJam) | `docs/04-parts-list.md` links and ₹ prices | n/a — facts and hyperlinks | no page text copied |
| toolchain: `arduino-cli` (GPL-2.0), Microsoft Edge headless, Python 3 | used to build and to screenshot the SVGs | their own | no — tools used, not shipped; they place no licence obligation on this source |

---

## 2. The reference project — what we took and what we did not

*IoT Based Water Level Indicator using Ultrasonic Sensor*, **Tech StudyCell /
IoT CircuitHub** (Dec 2022):
write-up <https://iotcircuithub.com/iot-based-water-level-indicator/>,
video <https://youtu.be/9geREeE13jc>. It is the build this project was checked
against, and it is credited wherever its influence reaches.

**Taken from it** (functional decisions and facts, not expression):

| | |
|---|---|
| the concept | ESP32 + waterproof ultrasonic module + 0.96" I²C OLED + Blynk + buzzer + stop button, for a tank |
| the GPIO numbers | TRIG 27, ECHO 26, SDA 21, SCL 22, button 12, buzzer 13, LED 14 — adopted on purpose so a reader can follow their article with this firmware |
| the > 25 cm rule | their own words: "the minimum distance between the sensor and the full tank water level must be greater than 25cm" (`docs/03-ultrasonic.md` §3) |
| part selection | BC547 + 220 Ω for the buzzer, 5 V piezo, 0.96" SSD1306 panel — commodity parts chosen by many builds |
| two names | `emptyTankDistance` / `fullTankDistance` are named in `docs/03-ultrasonic.md` §7 as *their* constants; our code uses `dEmptyCm` / `dFullCm` |

**Not taken from it** — none of this is in this repository, and none of it may be
copied out of their page into yours:

- their sketch source, in any form (ours is written from scratch, and shares no
  function, no structure and no line of code with it);
- their circuit diagram, PCB layout, Gerber files or photographs;
- any sentence of their article beyond the one quoted line above;
- their Blynk template: our datastream and event naming (`docs/05-blynk-and-display.md`)
  is this project's own.

That page states no licence, so it is treated as **all rights reserved**: facts and
functional choices from it are used, its expression is not. If you want the
original, go to their page — including for the zero-PCB layout they publish, which
this project does **not** use: rev B is a breadboard/enclosure build (phase 0) and
no PCB of ours exists yet.

Where our build deliberately **differs** from theirs — the 2 kΩ/3.3 kΩ echo divider,
the read-only rule, the alarm hysteresis and cooldown, the median filter and
calibration-by-distance, the CSV log and LAN JSON page — the differences are
listed in `docs/00-design.md` §5 and `docs/03-ultrasonic.md` §9.

---

## 3. Adafruit_GFX — the 5×7 font in `wlg_font.h`

`wlg_font.h` contains 475 bytes of glyph data: characters 32…126 taken
mechanically, byte for byte, out of `glcdfont.c` from **Adafruit GFX Library
v1.13.0** (verified byte-identical over that range against the locally installed
library). That file is Adafruit's and carries their notice, reproduced here and in
the header of `wlg_font.h` itself:

```
// This is the 'classic' fixed-space bitmap font for Adafruit_GFX since 1.0.
// See gfxfont.h for newer custom bitmap font info.
```

```
Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

Practical consequence: if you lift `wlg_font.h` into another project, keep that
notice in the file (source redistribution) and reproduce it in whatever
accompanies your build (binary redistribution). The MIT licence in `LICENSE` does
not apply to those 475 bytes.

For the record: the same 5×7 patterns have been floating around AVR/Arduino code
for decades as "public domain" character-ROM shapes, and this data does also
appear in Arduino's own LiquidCrystal examples. We still ship Adafruit's notice,
because the table in this file was taken from their file, and because
"it looks public-domain" is not a position worth defending.

`wlg_display.h` — the SSD1306 driver around it — is original to this project and
uses no Adafruit code. We deliberately did **not** use Adafruit GFX, Adafruit
BusIO or Adafruit_SSD1306 (the reference build does, and also AceButton): the
point of this firmware is that it builds with only the ESP32 core installed.

---

## 4. Libraries you install, which we do not ship

Compiling this sketch pulls in code we did not write and cannot relicense. If you
distribute a compiled `.bin`, you are distributing their object code, so keep their
notices:

| library | author | licence | how it enters the build |
|---|---|---|---|
| `esp32 by Espressif Systems` (Arduino core; `WiFi`, `WebServer`, `Wire`, `Preferences`) | Espressif Systems | LGPL-2.1 | required, board package |
| `Blynk` 1.4.x → `BlynkNcpDriver` | Blynk Inc. | Apache-2.0 | optional, only when `ENABLE_BLYNK 1` |

Apache-2.0 additionally wants you to keep the NOTICE text and state changes.
LGPL-2.1 lets you link it with proprietary code provided users can re-link — with
Arduino that is satisfied by shipping the sources of your sketch, which this repo
does anyway.

---

## 5. Datasheets

The numbers in `docs/00-design.md` §2 and `docs/03-ultrasonic.md` §1–2 (blind area
20 cm, trigger ≥ 10 µs, 40 ms no-echo timeout, 75° cone, ±10 mm, `cm = µs / 57.5`,
the R19 mode table, …) are transcribed **as facts** from these two documents:

| document | revision to ask for |
|---|---|
| `AJ-SR04M` ultrasonic ranging module | `AJ-SR04M-200925A` |
| `JSN-SR04M-2` waterproof ultrasonic ranging amplifier | `JSN-SR04M-2 200427` |

**No datasheet PDF, page image, table graphic or paragraph of datasheet prose is
redistributed in this repository**, and we do not link to somebody else's copy of
them: get the datasheet for the module you actually bought from that seller's
listing. Spec numbers re-stated in our own tables and our own words are our
expression of public facts; the datasheet's own figures and text remain the
manufacturer's.

---

## 6. Fonts used by the figures

The four figures in `docs/img/` are drawn by `gen-diagrams.py` and rendered to PNG
with headless Edge, which lays the text out in **DejaVu Sans** (with `Arial` named
only as a fallback in the SVG `font-family` string — the SVG embeds no font). The
PNGs therefore contain DejaVu glyph outlines:

> DejaVu Fonts, Copyright © DejaVu Fonts Authors. Based on Bitstream Vera fonts,
> trademark of Bitstream, Inc. Free to use, copy and redistribute, including in
> rendered images, provided the font name is not changed and the licence is kept
> with the font itself: <https://dejavu-fonts.github.io/License.html>

No font file is shipped here. The SVGs and `gen-diagrams.py` are ours; the drawing
is our own and is not a reproduction of anyone else's schematic.

---

## 7. Prices, links and shelf reality

`docs/04-parts-list.md` carries ₹ prices and vendor links gathered on **2026-11-16**
(see the "unverified"/"typical" tags). Those are facts about a market on a date, not
copied catalogue prose — listings move weekly, so re-check before ordering. Part
names and numbers are used as identifiers to point at a product, which is nominative
use; no vendor image, description or marketing text is reproduced.

---

## 8. If you fork this

You must:

1. keep the MIT notice for our code and docs,
2. keep the BSD notice inside `wlg_font.h` if you keep that file,
3. keep the credit to Tech StudyCell / IoT CircuitHub if you keep the pin map or the
   > 25 cm rule — not because a GPIO number is copyrightable (it is not) but because
   it is the honest citation trail,
4. not present this as a safety device. It is a gauge. It must never switch a pump or
   a valve, and `docs/00-design.md` §7 says so; you inherit that wording if you fork.
