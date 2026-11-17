# ESP32 water level gauge — 10 ft × 10 ft × 4 ft concrete tank

Reports tank level over WiFi: **cm, %, litres**. Read-only monitoring (no pump control).
Phase 0 runs on your existing **ESP32-WROOM-32 DevKitC (38-pin, CP2102)**; phase 1–2 put it on a PCB.
Optional and additive: a **0.96" OLED on the box**, a **buzzer + button** for a local alarm, and the
**Blynk phone app** with push notifications — all off/on by two `#define`s, none of them needed to
measure, log or serve the page. Details: [`docs/05-blynk-and-display.md`](docs/05-blynk-and-display.md).

## Where everything is

| file | what |
|---|---|
| `arduino/water_level_gauge/water_level_gauge.ino` | **the firmware to flash now** — plain Arduino sketch; the core build needs **no libraries at all** (Blynk only if you turn it on) |
| `arduino/water_level_gauge/wlg_display.h` | inline SSD1306 128×64 I²C OLED driver — no Adafruit, 1 kB frame buffer |
| `arduino/water_level_gauge/wlg_font.h` | 5×7 ASCII font, extracted from Adafruit_GFX `glcdfont.c` (public-domain glyph set) |
| `docs/00-design.md` | design record: tank numbers, why magnet-under-the-lid fails, sensor/power options, phases |
| `docs/01-wiring-phase0.md` | pin map, echo divider, power, serial commands |
| `docs/02-phase0-test-plan.md` | the six bench/field tests that gate the PCB design — sensor, RSSI, 24 h log, dew, display + local alarm, phone app |
| `docs/03-hydrostatic.md` | **how build 2's tube works** — the physics, the sealed-column errors, calibration, and verified video links |
| `docs/04-parts-list.md` | **parts list with ₹ prices and Indian links** (Amazon.in etc.), what Amazon doesn't carry, totals, **the 2026-11-16 site survey** (concrete tank, metal lid, unventable) and **build 3, submersible probe** — surveyed and priced, not compiled |
| `docs/05-blynk-and-display.md` | **the OLED, the local alarm and the phone app** — compile switches and measured flash/RAM, Blynk template/datastreams/events, message quota, what has and has not been tested on hardware |
| `docs/img/01-wiring.svg` / `.png` | **wiring diagram** — every pin used and unused, divider drawn out |
| `docs/img/02-mounting.svg` / `.png` | **mounting geometry to scale** — beam cone, blind zone, all the distances |
| `docs/img/03-power.svg` / `.png` | **power tree** — 18650 + solar, with current/battery-life table |
| `docs/img/04-display-and-app.svg` / `.png` | **what the box shows and what the phone gets** — the real 128×64 field map row by row, the 17 app streams, the message budget |
| `docs/img/gen-mounting.py` | regenerates the mounting diagram (edit distances in cm, re-run) |

## Flash it (5 minutes)

1. Arduino IDE → Boards Manager → install **“esp32 by Espressif Systems”**.
2. Open the `.ino`, board = **DOIT ESP32 DEVKIT V1** (or ESP32 Dev Module, 38-pin flash), pick the COM port.
   The core build needs **no libraries at all** — the OLED driver and its font are in the project folder.
   Two switches at the top: `ENABLE_OLED` (1 = the panel is drawn by code in `wlg_display.h`) and
   `ENABLE_BLYNK` (1 = **requires** the Blynk library, `arduino-cli lib install "Blynk"`; set it to 0 if
   you do not want the phone app). Leaving `BLYNK_AUTH_TOKEN` empty turns the app off at runtime — the
   compiler then dead-strips the whole cloud path (verified: the string `blynk.cloud` is absent from that
   binary), so an empty token costs nothing, but it also means the shipped `.bin` has no Blynk client in
   it until you paste a token. Blynk's real cost when configured is ~8.3 kB flash. Set `ENABLE_BLYNK 0`
   to drop the dependency and the library requirement entirely. Sensor choice is `SENSOR_TYPE`: the code
   default is **2, hydrostatic**;
   ultrasonic needs `-DSENSOR_TYPE=1`. What each build is and which one this house gets:
   "Decision that gates the KiCad schematic", below.
3. **Build 2 (`SENSOR_TYPE` default): hydrostatic, BMP280 in the tube + BME280 in the box.** This is what
   the 2026-11-16 site survey picks for this house, and what the KiCad schematic will be drawn around.
   **Build 1 (ultrasonic) is the bench rehearsal — the video's method, still what tests 1/2/5 of
   `docs/02-phase0-test-plan.md` exercise, ruled out for this tank. Compile it with `-DSENSOR_TYPE=1`**
   (arduino-cli) or change the `#define` (Arduino IDE).
   GPIO25→T, divider→GPIO26, VIN→V, GND→G.
   The echo **must** go through the 2 kΩ/3.3 kΩ divider — 5 V into GPIO26 kills the chip.
4. Serial Monitor 115200 → `w <yourssid>` then `p <yourpass>` then `x`.
5. Calibration from the tank: `e 174` `f 52.2` `a 92903` `i 300` (those distances are the build-1 example;
   build 2 calibrates with `m 121.9` and `z`). Then `c` for one sample, `s` for JSON. For the real
   deployment use `i 900` — 60 s sampling spends 43 % of the free Blynk message quota, 900 s spends 17 %
   (`docs/05-blynk-and-display.md` §5.5).
6. Point a browser at the printed IP: live page, `/api/state`, `/api/log`.

### Optional extras (skip them and everything above is unchanged)

- **OLED on the box**: 4 wires — VCC→3V3, GND, SDA→GPIO21, SCL→GPIO22, i.e. the *same* two signal
  wires as the pressure sensor (`0x3C`/`0x3D` next to `0x76`/`0x77`). Nothing to install, nothing to
  configure; it is auto-probed at boot. Serial `d` redraws, `D` flips it 180°.
- **Local alarm**: buzzer on GPIO17 + button on GPIO23, thresholds with serial `l <cm>` / `u <cm>`,
  `j` mutes. Works with no internet at all.
- **Phone app + push** (Blynk): `arduino-cli lib install "Blynk"`, then paste your
  `BLYNK_TEMPLATE_ID` / `BLYNK_TEMPLATE_NAME` / `BLYNK_AUTH_TOKEN`. Empty token = the feature stays
  off and behaves exactly as if it were not compiled in. Console streams/events and the message
  quota maths: [`docs/05-blynk-and-display.md`](docs/05-blynk-and-display.md).

## Tank constants

304.8 × 304.8 cm inside, water depth 121.9 cm, 6 in (15.2 cm) gap to the lid.
Full = **11 327 L**, **92.9 L per cm**, head at the floor 120 mbar (0.12 bar).

## Decision that gates the KiCad schematic

**Changed on 2026-11-16 by the site survey** (`docs/04-parts-list.md` §10): the tank is a house overhead
tank — concrete body, metal lid, cannot be sealed, everything inside it is wet.

- **Build 1 — ultrasonic, 250 mm printed chimney on the lid** (face 52.2 cm above the full line, 174 cm
  above the floor): the method in the reference video, and still what the bench tests rehearse on a
  bucket, but **ruled out for this tank** — dew on the face in a wet box, and a lid that will not take a
  sealed 100 mm hole.
- **Build 2 — hydrostatic, vented tube to the floor: the phase-1 population choice.** ₹937 and ±1–2 cm:
  cheaper and about four times more accurate than any ₹3.4k transmitter. **BMP280 on the tube side
  (300–1200 hPa), BME280 on the box side** — a BME280 in the tube clips at 1100 hPa. Costs you one
  standing duty: the tube must stay dry.
- **Build 3 — submersible probe on GPIO33**: what you buy if you refuse to own a tube. Priced and
  dimensioned in `docs/04-parts-list.md` §10, **not compiled** — `SENSOR_TYPE` takes 1 and 2 only. The
  ~40 lines it needs are a sensor driver only: the filter, the page, the CSV, the OLED, the alarms and
  the app all read `level_cm`, never the sensor.

The sketch contains builds 1 and 2; one `#define` switches. Parts and links: `docs/04-parts-list.md`.

Four questions to answer before I open the schematic are at the bottom of
`docs/02-phase0-test-plan.md` (sensor choice, router distance/obstacles, sample interval, parts you own).
The order list is already written: `docs/04-parts-list.md` — but it is waiting on the sensor choice
(build 1 ultrasonic vs build 2 hydrostatic), because build 2 needs a **BMP280** on the tube side, not a
second BME280.
