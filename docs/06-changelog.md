# 06 — Changelog: rev A (hydrostatic) → rev B (SR04M ultrasonic)

Rewritten **2026-11-18**. This is the record of what was replaced, what it cost, and what rev B
gives up. §9 is the authority on where the old design may still be named: mostly here, and only in
retired context elsewhere, so the grep list that proves it lives at the bottom of this file.

---

## 1. What changed, at a glance

| | rev A | rev B |
|---|---|---|
| sensing principle | differential pressure: a vented tube down to the tank floor + two barometric ICs subtracting box pressure from tube pressure | one **SR04M-family waterproof ultrasonic module** looking down at the water from above the lid |
| parts in the water | a tube open to the water, and (as an option) a submersible transmitter | **nothing** — the probe stays dry, above the water |
| what sets full scale | full-scale depth (`maxLevelCm`) and the sensor's pressure range | the calibrated span `d_empty − d_full` (121.9 cm here) |
| temperature's job | water temperature → water **density**, for litres | air temperature → **sound speed**, for distance. Configured with `t <°C>`; there is no sensor for it |
| compile-time sensor switch | `SENSOR_TYPE` 1 = ultrasonic (JSN-SR04T), 2 = BME280 + BMP280 differential (default), plus a "build 3" pressure-transmitter option | **no sensor switch at all.** Only `ENABLE_OLED` and `ENABLE_BLYNK`, which are output options |
| mounting | 250 mm printed chimney, 60 mm bore, 1 mm mesh, louvre shade, face 52.2 cm above the full line | 100 mm lid hole, 150 mm probe clearance, cowl, **no screen of any kind** |
| reference project | none of its own — rev A was designed from first principles | Tech StudyCell / IoT CircuitHub [*IoT Based Water Level Indicator using Ultrasonic Sensor*](https://iotcircuithub.com/iot-based-water-level-indicator/) ([video](https://youtu.be/9geREeE13jc)) — same module family, same GPIO numbers |
| firmware size | (rev A build, not comparable) | **1 000 783 B flash (76 %), 50 652 B RAM (15 %)**, Blynk layer compiled in |

The trigger for the rewrite was a decision, not a failure report: the SR04M-family module and the
reference project above became the requirement, and a gauge with one sensor, one build and nothing
in the water is easier to install, service and reason about than a differential-pressure rig with
two barometric ICs, a tube, a chimney and a compensation model. Everything else in this changelog
follows from that one decision.

---

## 2. Firmware

### 2.1 Deleted, not commented out

| removed | what it was | replaced by |
|---|---|---|
| `SENSOR_TYPE`, `SENSOR_ULTRASONIC`, `SENSOR_BARO_DUAL` | build switching | one build |
| ≈150 lines of raw-I²C BME280/BMP280 drivers (`baroRaw()`, chip-ID probes at `0x76`/`0x77`) | the two pressure ICs | one `pingRaw()` |
| `pRefPa`, `pTubePa`, `baroOffsetCm`, `waterDensity()`, `sampleValid()` | the differential model and the density correction it fed | `level = clamp(d_empty − d, 0 … span)` |
| 1-Wire / DS18B20 driver, `OW_PIN 13`, `ONEWIRE`, `ONEWIRE_CRC16`, `sensor_t` | water temperature, which existed only to correct water density | `t <°C>` config for sound speed; **GPIO13 is now the buzzer** |
| `PIN_LVL_ANA` (GPIO33 submersible analog input), reed-ladder option | rev A "build 3" alternatives | deleted — not marked "future" |
| `m <cm>` (measurement mode), `o <cm>` (baro offset), `z` (zero-diff), `a` (baro offset alias) | pressure calibration | `e`/`f` (distances), `g <cm>` (gate), `t <°C>` (sound speed) |
| `p_diff`/`p_ref`/`p_tube` CSV columns, `AIR …%`/`H2O …°C`/`DP …hPa` OLED rows | the pressure readout | `echo_us`, `reason`, `air_t`, and the `ECHO`/`SPAN`/`GATE` rows |
| `FAULT_STREAK 6` with a pressure-specific rule ("`TUBE OVER RANGE` never fires the fault") | pressure fault logic | `FAULT_STREAK 3` on consecutive hard rejects, with near-rejects (dew, surface-in-gate) kept separate |

`R_OK/R_NEAR/…` in the new reason enum is worth one line of its own: the first version used
`R_OK` and the build failed with *expected identifier before numeric constant*, because
`R_OK` is already a POSIX `access()` mode constant inside the ESP32 core. The enum is now
`URS_OK / URS_NEAR / URS_FAR / URS_NOECHO / URS_JUMP`.

### 2.2 New in rev B

- **SR04M mode 1 timing done properly**: 12 µs trigger (module wants ≥ 10 µs), `pulseIn` timeout
  **45 ms** so the module's own ~40 ms no-echo cutoff always fires first, and a **60 ms minimum
  ping gap** enforced inside `pingRaw()` so the CLI, `loop()` and the web page cannot double-ping —
  the module wants > 50 ms and HC-SR04's 25 ms does not qualify.
- **Rejection reasons** (`too close`, `no echo`, `out of range`, `jump rejected`) carried with every
  sample into the log line, the OLED status row, the web JSON and the fault logic.
- **A gate, not just a blind zone**: the module is blind to 20 cm, firmware rejects under
  `BLIND_GATE_CM 25.0` (test plan T4b proves the margin).
- **Jump confirmation**: median-of-15, a step larger than 12 cm needs 3 consecutive confirmations,
  then the new level is accepted — so a real pump delivery is followed in about 4 samples instead of
  being smeared, while a single wild echo cannot move the number at all.
- **Dew-on-face diagnosis**: a rejected near read of 2–8 cm says `DEW ON FACE` instead of being
  counted as a fault.
- **Local alarm half**: buzzer on GPIO13 driven by `tone()`, LED on GPIO14 (solid on = full),
  stop/snooze/mute button on GPIO12, 30 min notification cooldown, NVS mute latch.
- **LAN web page + JSON** at `/`, `/api/state`, `/api/log`, `/api/scan`, plus the log line
  `ts_s,echo_us,raw_cm,reason,filt_cm,level_cm,pct,litres,air_t,gate_cm,vbat,rssi,bad,n`.
- **Blynk as an additive layer** with an explicit message-quota policy (§5.5 of
  [05-blynk-and-display.md](05-blynk-and-display.md)).

---

## 3. Documents

| rev A | rev B |
|---|---|
| `03-hydrostatic.md` — tube physics, bubbler videos, sealed-column error tables, the 1133 hPa range problem, rain-pulse and wasp-nest failure modes | **deleted** (`git rm`). Replaced by [`03-ultrasonic.md`](03-ultrasonic.md): module protocol and the `R19` mode strap, blind zone, 75° cone arithmetic, foam/dew/wave error budget, temperature, mounting |
| `00-design.md` (differential-pressure contract, `§3.1` failure modes, chimney geometry) | rewritten as the rev B contract; §3 explains the mounting change from rev A and §8 is the no-remnants checklist |
| `01-wiring-phase0.md`, `02-phase0-test-plan.md`, `04-parts-list.md`, `05-blynk-and-display.md`, `README.md` | rewritten around the ultrasonic build. Line-number references into the sketch (`.ino:NNN`) were dropped in favour of **function names** (`pingRaw()`, `filterUpdate()`, `statusText()`, `buzzerService()`), because every one of them went stale the day the display and app code landed |
| — | this file |
| — | [`img/README.md`](img/README.md): how the four figures are generated, checked and rendered on this machine (there is no Inkscape and no ImageMagick here; `system32\convert` is the Windows disk tool and must never be run) |
| `img/gen-mounting.py` (one-off, rev A mounting art) | **deleted** (`git rm`) |
| — | `img/gen-diagrams.py` — one generator, `build_01()`…`build_04()`, all four figures from one set of primitives |
| — | `img/check-diagrams.py` — geometry linter for the SVGs (see §7) |

---

## 4. Bill of materials

**Out:** BME280 breakout, BMP280 module (the tube side needed a BMP280 because a 121.9 cm column adds
≈120 hPa, past a BME280's 1100 hPa ceiling — one part number rev A was unusually emphatic about),
DS18B20 in a steel probe, 4.7 kΩ 1-Wire pull-up, silicone/PVC tube + PTFE vent membrane, 1 mm mesh
over the chimney, the 250 mm chimney print, and every part of the submersible-transmitter and
reed-ladder options.

**In:** the SR04M-family module (+1 spare), 2 kΩ/3.3 kΩ echo divider, 220 Ω buzzer and LED
resistors, a BC547/S8050 + 1N4148 + piezo buzzer + green LED, a tactile button, 100 µF at the
module, a 100 mm hole saw, a 75–110 mm stub pipe for the 150 mm standoff, and a cowl **instead of a
screen**.

The **power chain is unchanged** (CN3791 + 5 W/6 V panel + 2 × 18650 + MT3608 + fuse) and its
verified prices carry over into [04-parts-list.md](04-parts-list.md) §4.

---

## 5. If you already built rev A hardware

1. Move the box off the chimney; cut the **100 mm** hole, fit the **150 mm** stub, mount the probe
   face up inside it, cowl over the top. Remove the mesh — it blocks 40 kHz.
2. Wire the module **GND first, then VCC** to the 5 V rail; ECHO through 2 kΩ/3.3 kΩ into GPIO26,
   TRIG to GPIO27. Check pad **`R19` is unpopulated**.
3. Free the pins the pressure build owned: **GPIO13 changes from 1-Wire to the buzzer**, and the
   second I²C device at `0x76`/`0x77` is gone, leaving the OLED alone on GPIO21/22.
4. Flash rev B, `x` to clear NVS (rev A stored a baro offset and a full-scale depth that mean
   nothing now), then `w`/`p`, `t <°C>`, `e`, `f`, `v`.
5. In the Blynk template: **delete** V5 (`air_t`), V6 (`air_rh`), V7 (`water_t`) and V16
   (`p_diff_hpa`); rename V14 → `bad_pct`, V15 → `uptime_h`; add the four events
   `level_low`, `level_high`, `sensor_fault`, `level_ok`. Retire rather than renumber, so existing
   dashboards keep working.

---

## 6. What rev B gives up (say it out loud)

- **No humidity/dew warning.** The BME280 used to report box humidity and warn about condensation
  before it became a problem. Now dew simply *happens*, on the probe face, and shows up as
  `DEW ON FACE` / `too close` rejections after the fact.
- **No water temperature**, so litres are volume × 1 kg/L with no density correction. For a
  domestic water tank that is a rounding error; it is only honest to say so rather than claim the
  number is temperature-compensated.
- **A blind zone at exactly the interesting end.** The full line must sit > 25 cm below the face,
  which is what forces the 150 mm standoff and the 100 mm hole. The pressure build had no minimum
  level at all.
- **Scale error is now environmental and manual.** Sound speed moves 0.177 %/°C, so ≈0.27 cm/°C at
  152 cm and ≈4 cm of level error over a 15 °C swing, and the air temperature is a value you type
  (`t`), not one you measure. Adding a temperature sensor is the obvious future improvement —
  see [03-ultrasonic.md §5](03-ultrasonic.md).
- **Wide cone, awkward lid.** 75° means a 46 cm footprint at the full line, so no lid clutter inside
  20 cm and **no screen ever**. The pressure build's 60 mm chimney is not a shape this sensor can
  live in — a narrow pipe over a wide cone is a resonant clutter chamber.
- **A flat, clean surface is now part of the measurement.** Foam, scum, ripples and splashes are
  rejected as `no echo` rather than integrated into a wrong number — safer, but it means the gauge
  can go stale precisely when the surface is disturbed, which is when you most want a reading.

---

## 7. Figures

The four figures are **generated, not drawn**: `docs/img/gen-diagrams.py` writes the SVG, and the
PNG is a render of it. That is a rev B change in kind as well as content — rev A's figures were
hand-tuned XML, which is exactly how a diagram ends up still showing a sensor the sketch stopped
using. The generator puts every pin number, V-pin and dimension in one Python file that diffs against
the sketch, so a firmware change and the figure that contradicts it show up in the same review.

Content fixes made after the first rev B render, each of them a mismatch between the picture and the
code:

| figure | was wrong | now |
|---|---|---|
| `04` | OLED mock showed `ECHO 1778.9` (microseconds) and `BAT 71%`, and a `W7823A` hostname that the firmware never prints | the mock prints exactly what `oledDraw()` prints: `ECHO 33.8`, `SPAN 121.9`, `GATE 25.0 T30C`, `4 bad`, `BAT 4.02V 82%`, RSSI right-aligned |
| `04` | mock read 118 cm / 96.8 % and then said `LOW LEVEL` | `FULL - OVERFLOW`, which is what `statusText()` returns at 118 cm with `high_cm` 110 |
| `04` | Blynk panel said **12 streams** | **13**: V0 V1 V2 V3 V4 V8 V9 read, V10 V11 V12 V13 written by the app, V14 V15 hourly diagnostics |
| `04` | alarm table invented statuses (`overflow`, `dew_on_face`, `battery_low`) and an event count that did not match | the table lists only the ten strings `statusText()` really returns, and the four event codes in §2 |
| `04` | app mock showed `12.4 V` on V4 | `4.02 V`, the rail the 1 M / 1 M divider on GPIO36 actually measures |
| `04` | `/api/log` described as a CSV ring buffer | it forces one sample now and prints it; the ring is 240 lines but there is no endpoint to dump it (stated in `05-blynk-and-display.md` §6) |
| `03` | CN3791 captioned "CV 8.4 V" — two-series-cell wording, while the whole BOM is 1 S | "CV set to 4.20 V, 1 S", matching `01-wiring-phase0.md` §5 and the parts list |
| `03` | current table invented "deep sleep, ping only 14 mA" | the rows are the ones `00-design.md` §7 commits to: 20-25 mA modem-sleep working point, 60-95 mA with modem sleep off, 4-8 mA light sleep on the phase-2 board, < 8 mA module, ≈350 mA WiFi TX peak |
| `03` | no mention of the gated sensor rail | one legend line: the phase-1 PCB keeps a TPS22919 high-side switch on the module rail to kill its 2 mA standby; the bench build hard-wires it |
| `02` | carried rev A history ("the 60 mm chimney…") in a build instruction | reworded into a present-tense rule: keep the aperture clear and wide. Figures carry instructions, history goes in this file |

**How they are checked without eyes.** This session cannot look at a PNG, and there is no font
rasteriser on the box, so `docs/img/check-diagrams.py` measures the SVG instead: character-width
arithmetic (`0.60 × font-size` for the proportional face, `0.64` for `Consolas`) over the text
elements, then reports

- `OOB` — anything inside 12 px of the canvas edge or outside it,
- `T/T` — two labels overlapping,
- `T/L` — a label sitting on a wire or a shape outline,
- `T/B` — a label that starts inside a box and runs past its right edge.

All four pass clean. That catches the mechanical failures — labels colliding, text over a wire,
art running off the page — and it does not and cannot tell you whether the drawing reads well.
**Every figure still needs a human look before it goes on a wall.**

Rendering: Microsoft Edge headless, `--window-size` equal to the SVG's `viewBox` and
`--force-device-scale-factor=2`, so each PNG comes out exactly 2× (2480 × 2160 / 1800 / 1960 / 2080).
A fresh `--user-data-dir` per run and `--hide-scrollbars` are not optional: without them Edge gives
you a 96 DPI capture or a page with a white band where a scrollbar was. The full command line and the
dimension table are in [`img/README.md`](img/README.md).

---

## 8. Verification state of this rewrite

**Publishing pass (2026-11-18, on the way to a public repo):** the repo gained a licence and a credit
trail, and credentials moved out of the tracked source.

| change | why |
|---|---|
| `LICENSE` (MIT) + `THIRD_PARTY_NOTICES.md` | say in one place what is ours, what is Adafruit's (the 5×7 glyph table in `wlg_font.h`, BSD-licensed, its notice now reproduced inside the file — the 475 bytes were verified byte-identical to `glcdfont.c` v1.13.0 over characters 32…126), what you install but we do not ship (Blynk Apache-2.0, ESP32 core LGPL-2.1), and how the datasheets and the Tech StudyCell build are **cited rather than copied** |
| `wlg_secrets.h` (git-ignored) + `wlg_secrets.example.h` | the Blynk token is a password, and the sketch used to tell you to paste it into a tracked file. It now lives in a file git will not stage; `#if __has_include("wlg_secrets.h")` picks it up, so not having it is the LAN-only build this repo is published in. Measured: 1 000 783 B without it, 1 008 271 B with a test token |
| datasheet citations, `00-design.md` §2 and `03-ultrasonic.md` §10 | name the document and its revision (AJ-SR04M 200925A, JSN-SR04M-2 200427), keep no PDF and no mirrored link in the repo; the local scratch path that used to be cited there is gone |

**Verified:** the sketch compiles clean for `esp32:esp32:esp32doit-devkit-v1` (Arduino CLI 1.6.0,
esp32 core 3.3.12, Blynk 1.4.0) — 1 000 783 B flash / 50 652 B RAM, and 1 000 019 B / 50 500 B with
`-DENABLE_BLYNK=0`; Blynk appears in the `Used library` list; and the grep list in §9 returns hits
only where it says they are allowed — `README.md`, `00-design.md`, `03-ultrasonic.md`,
`04-parts-list.md` and this file — plus the two false positives §9 calls out (a font bitmap byte
`0x77` in `wlg_font.h`, and the `p_diff` retirement comment in the sketch). Nothing survives in the
firmware logic, the figures, or the two how-to-build documents.

**Not verified:** nothing has been flashed or run. Every runtime claim in the docs — the 45 ms
timeout interaction, the 3-sample jump confirmation, the 9 s beep cadence, the quota table, the
0.5–1 % pump-pulse behaviour — is design arithmetic or datasheet behaviour, not measurement.
[02-phase0-test-plan.md](02-phase0-test-plan.md) exists to turn each of those into a number, and
T8/T9/T11 are the ones that will decide whether this gauge is trusted on the roof.

**Process note:** this rewrite was attempted with four parallel documentation/diagram sub-agents
(`docs-wiring`, `docs-sensor-parts`, `docs-app`, `diagrams`, plus one retry). All five exited with
code 1 without writing a single file, so the documentation and diagrams above were written in the
main session instead. Nothing in this repo depends on that having worked.

---

## 9. No-remnants grep list

Run this from the repo root (`git bash`):

```sh
TERM_LIST="SENSOR_BARO_DUAL SENSOR_TYPE baroRaw waterDensity pRefPa pTubePa baroOffsetCm
BME280 BMP280 0x76 0x77 DS18B20 ONEWIRE PIN_LVL_ANA TUBE chimney submersible
hydrostatic bubbler p_diff hPa mbar 1133 119.2 52.2 maxLevelCm"

for t in $TERM_LIST; do
  hits=$(grep -rl --include='*.md' --include='*.ino' --include='*.h' \
                    --include='*.svg' --include='*.py' "$t" .)
  test -n "$hits" && printf '%-16s %s\n' "$t" $(echo $hits | tr '\n' ' ')
done
```

Zero hits required in the **build artefacts** — `arduino/`, `docs/img/` (the SVGs and the
generators), `01-wiring-phase0.md` and `02-phase0-test-plan.md`: those tell someone what to connect
and what to measure today, and a stale part name in them costs an afternoon. Where a term survives
elsewhere, it survives **only** inside a sentence that says it is retired, in these five places:

| file | allowed context |
|---|---|
| `README.md` | one paragraph: rev B replaced rev A's hydrostatic design, and what is gone |
| `00-design.md` | §3 (the mounting change), §6 (what rev A was), §8 (this checklist) |
| `03-ultrasonic.md` | line 5 ("which is gone from the project") and §7's note on why rev A's 250 mm collar cannot work over a 75° cone |
| `04-parts-list.md` | the "retired from rev A — do not order these" table |
| `06-changelog.md` | this file, everywhere |

Two grep hits are false positives and are not signs of old code: `0x77` in `wlg_font.h` is a bitmap
byte of the 5×7 digit font, and `p_diff` in `water_level_gauge.ino` is the comment that tells anyone
with an old Blynk template which four datastreams to delete (V5 air, V6 RH, V7 water temp, V16
differential pressure).

Re-run the list after a licence/attribution pass: `LICENSE` and `THIRD_PARTY_NOTICES.md` must add
**zero** hits, and as of 2026-11-18 they do.
