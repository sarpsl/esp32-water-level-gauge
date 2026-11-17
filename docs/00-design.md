# Water Level Gauge — design record (rev B, SR04M ultrasonic)

**Rev B, 2026-11-17: the sensing principle is now ultrasonic only — an SR04M-family waterproof
module.** The hydrostatic/tube build (rev A "build 2", BME280+BMP280 and a vented tube to the tank
floor) is gone from the firmware, the parts list and the diagrams, together with the submersible-probe
option and the DS18B20 water-temperature sensor. §8 lists exactly what was removed, so you can check
nothing is left behind.

Owner hardware: **ESP32-WROOM-32 DevKitC V1, 38-pin, CP2102** + 38-pin "ESP32S" expansion board.
Reference build this project follows: *IoT Based Water Level Indicator using Ultrasonic Sensor*
(Tech StudyCell, Dec 2022) — ESP32 + SR04M/HC-SR04 + 0.96" I²C OLED + Blynk + buzzer + button:
<https://iotcircuithub.com/iot-based-water-level-indicator/> (video: <https://youtu.be/9geREeE13jc>).
We keep that project's **pin map, part choices and alarm behaviour**, and we keep this repo's
calibration model, filter, CSV log, LAN JSON page, echo divider and read-only rule.

Code is a plain **Arduino sketch** (`arduino/water_level_gauge/water_level_gauge.ino`); the core build
needs no libraries. Diagrams: [`img/01-wiring.svg`](img/01-wiring.svg) wiring map,
[`img/02-mounting.svg`](img/02-mounting.svg) mounting geometry to scale,
[`img/03-power.svg`](img/03-power.svg) power tree,
[`img/04-display-and-app.svg`](img/04-display-and-app.svg) OLED rows + app streams.

## 1. Tank as measured (unchanged since the 2026-11-16 survey)

| Item | Value |
|---|---|
| Internal footprint | 10 ft × 10 ft = 9.29 m² (304.8 × 304.8 cm) |
| Water depth (4 ft, lid gap excluded) | **121.9 cm** |
| Gap from full water line to lid underside | **15.2 cm (6")** |
| Full volume | **11.33 m³ = 11 327 L ≈ 2 990 US gal** |
| Volume per 1 cm of level | **92.9 L** |
| Body | reinforced concrete, **metal lid**, not sealable, everything inside the lid is wet |

The UI reports cm, %, and litres. Litres = `level_cm × 92.9`, and the area is a **config value**
(serial `a <cm2>`), not a constant — a concrete tank's interior is never exactly 10×10 and there is
usually a silt bed on the floor.

## 2. The sensor: SR04M family waterproof ultrasonic module

Sold in India as **SR04M / AJ-SR04M / SR04M-2 / JSN-SR04M-2** (same family, module + separate
encapsulated waterproof probe on a fixed cable). Two datasheets are quoted throughout this repo; both
are in the sources list at the end of §9.

| Spec | Value | Source |
|---|---|---|
| Supply | **3.0–5.5 VDC** (works on a 3.3 V rail, at reduced margin) | JSN-SR04M-2 datasheet |
| Working current | **< 8 mA** (JSN-SR04M-2); mode 1 min. 2.5 mA, standby **< 2 mA** (AJ-SR04M) | both |
| Blind area (dead zone) | **20 cm** — the module will not report anything closer | both |
| Max range | 6 m (JSN-SR04M-2) / 8 m (AJ-SR04M table); Indian listings quote 4.5 m. **This tank's worst case is 1.52 m**, so range is never the limit here | both |
| Accuracy / resolution | ±10 mm at long range, 1 mm resolution | JSN-SR04M-2 |
| Measuring (beam) angle | **75° cone** — wide, see §3 | JSN-SR04M-2 |
| Frequency | 40 kHz, 8-cycle burst | both |
| Trigger | TTL high **≥ 10 µs** on TRIG/RX | both |
| Echo | high width = round-trip time; **if no echo returns, ECHO drops after 40 ms** | both |
| Suggested cycle | **> 50 ms** between triggers | JSN-SR04M-2 |
| Arithmetic | `cm = µs / 57.5` (datasheet uses 348 m/s) | both |
| Target | needs a smooth reflector **≥ 0.5 m²** — a rippling/foamy surface fails this | JSN-SR04M-2 |
| Operating temp | −20 … +70 °C | JSN-SR04M-2 |
| Board size | 42 × 29 × 12 mm | JSN-SR04M-2 |
| Working modes | five, selected by the resistor at pad **R19** — see §2.1 | AJ-SR04M manual |

**Wiring order note from the datasheet, and it is not pedantry:** with a mains-referenced supply,
connect **GND to the module first**, then VCC. The module is unhappy the other way round.

### 2.1 The five modes (only mode 1 is used here)

| R19 on the module | Mode | Standby | Low-power | Blind | Max |
|---|---|---|---|---|---|
| **open circuit** | **1 — HR-04/HC-SR04-compatible trigger+echo ← this build** | < 2 mA | — | 20 cm | 8 m |
| 300 kΩ | 2 — low-power pulse | < 2 mA | < 40 µA | 20 cm | 8 m |
| 120 kΩ | 3 — automatic serial (9600, self-cycling) | < 2 mA | — | 20 cm | 8 m |
| 47 kΩ | 4 — serial, trigger by writing `0x01` | < 2 mA | < 20 µA | 20 cm | 8 m |
| 0 Ω | 5 — ASCII printout | < 2 mA | < 20 µA | 20 cm | 8 m |

Firmware speaks mode 1 (pulse) on GPIO26/27 — the same protocol as the reference project and as the
HC-SR04. Modes 2 and 4 are the two to revisit in phase 3 if battery life comes up short: they are the
same measurement at 20–40 µA instead of 2.5 mA, and mode 4 trades the echo wire for one UART pin and a
4-byte checksummed frame in mm. **Nothing in this firmware drives the serial modes**; if you buy a
module that a previous owner converted (R19 bridged), it will answer no echo at all — check the pad.

### 2.2 Why ultrasonic suits this tank

- Nothing electronic goes into the water: only the **encapsulated probe**, in the wet air space under
  the lid. That is what makes ultrasonic workable in a tank the 2026-11-16 survey called "unventable,
  everything inside wet" — the electronics and the battery stay in a dry vented box **on top** of the
  lid, and the one wet part has no exposed conductors.
- No consumables, no tube to keep dry, no membrane to foul, nothing to calibrate against pressure.
- Costs ₹400–700 in India versus ₹937 for the old two-barometer tube build.

The honest costs are in §4.3 (dew on the face, insects, and the 20 cm blind zone) and they are the
reason the geometry below is not "glue the sensor to the lid".

## 3. Mounting geometry — what changed from rev A, and why

Rev A's chimney (250 mm tall, 60 mm bore, face 52.2 cm above the full line) was dimensioned for the
**JSN-SR04T's 5–8° cone**, which threads a narrow tube cleanly. The SR04M-2's beam is a **75° cone**.
Down a 250 mm × 60 mm collar that beam slams into the wall within a few centimetres and rattles to the
bottom, and the collar's bottom rim sits at 25 cm — *closer than the water*, so the module reports the
rim. The chimney is therefore **gone**; the geometry is now a wide, short clearance:

```
      ☀ sun shade                ← ESP32 + antenna in the top third of the box,
   ┌───────────────┐                 ≥ 80 mm above the lid, away from the cell
   │  ESP32  📡    │
   │  OLED ☐  🔊   │              ← the box: dry, vented labyrinth, desiccant
   ├───────────────┤
   │  ● probe      │              ← probe face 150 mm above the lid underside,
   ╘═╤═══════════╤═╛                 pointing straight down, 5–10° tilt so dew runs off
  ═══╪═ ███████ ═╪═══  metal lid, 100 mm hole cut in it
     └─┐       ┌─┘
   15.2 cm air gap at FULL
  ~~~~~~~~~~~~~~~~ full water line      ← face → surface = 30.2 cm
             │
             │   max 121.9 cm of water
  ___________▼__________  tank floor    ← face → floor = 152.1 cm
```

| Quantity | Value | Check |
|---|---|---|
| face → lid underside | **15.0 cm** | every echo off the lid plane and the hole rim lands at 15 cm, i.e. **inside the 20 cm blind window the module blanks itself** — the clutter is rejected by hardware, not by code |
| face → full water line | **30.2 cm** | ≥ 25 cm (the reference project's rule) and 10 cm of margin over the 20 cm blind zone for swell, rain splash and face dew |
| face → tank floor (empty) | **152.1 cm** | ≤ 450 cm, no problem |
| beam diameter at the full line | 2·30.2·tan 37.5° ≈ **46 cm** | clears the 100 mm lid hole with room to spare |
| beam diameter at the floor | 2·152.1·tan 37.5° ≈ **233 cm** | lands wholly inside the 304.8 cm floor, so there is **no wall echo**, and the footprint (4.3 m²) beats the datasheet's 0.5 m² minimum |
| water depth measurable | **121.9 cm** = the whole tank | the top of the tank is *not* clipped, unlike a lid-flush mount |

If the face were flush under the lid, the face→full distance would be **15.2 cm — inside the blind
zone**: the gauge would go blind exactly when the tank is nearly full, which is the one moment you
want it to work. So the box bottom (and the probe) stand 150 mm proud of the lid, on a printed collar
over the hole. That single 150 mm is what makes an SR04M usable on this tank.

**Do not put a screen or mesh over the aperture.** At 40 kHz (8.6 mm wavelength) a 1 mm flyscreen is
close to an acoustic wall: it eats the ping. Keep the weather off with a rain cap and a downward-facing
gap, and accept the annual job of wiping the face (§4.3).

## 4. The measurement chain, and the numbers it has to beat

`ping → raw cm → blind gate → median-of-15 → level, %, litres → OLED + app + CSV + alarm`

**4.1 Level from distance.** The tank is measured from the top, so everything is distance-based, and
the two calibration constants are the two *distances*, exactly as in the reference sketch
(`emptyTankDistance` / `fullTankDistance`):

```
span  = d_empty − d_full                      // 152.1 − 30.2 = 121.9 cm of water
level = clamp(d_empty − d, 0 … span)
pct   = level / span × 100
litres= level × area_cm² / 1000               // 92.9 L per cm here
```

Set them with `e <cm>` and `f <cm>`; both are stored in NVS. `e`/`f` are also the reason the raw
distance is logged: if the tank is measured on a hot afternoon, the sound speed at calibration time is
baked into those two numbers (see §4.2).

**4.2 Sound speed and temperature.** `v = 331.3 + 0.606·T_air` m/s → **0.18 % per °C**, i.e. at
152 cm one °C is 0.27 cm. The firmware multiplies the measured pulse width by a speed derived from a
**single config value** (`t <°C>`, default 30 °C, stored in NVS) because this build has no temperature
sensor: the DS18B20 and the box BME280 went away with the hydrostatic build.

| situation | error on level |
|---|---|
| calibrated at the tank, reading at the same air temperature | ≈ 0 (calibration cancels the sound speed) |
| 30 °C calibration, 15 °C reading (Jan night) | −6 cm on a full-scale read, −2.4 % of the tank |
| 30 °C calibration, 45 °C reading (May afternoon, closed box) | +6 cm |

±5 cm is ±460 L on this tank. Set `t` once per season, or fix it to your annual average and accept the
seasonal wander. **Not verified:** the two datasheets do not state whether the module applies its own
internal temperature compensation in mode 1; the older JSN-SR04T datasheets claim a −10…+60 °C
compensation range. Test 6 of the test plan measures it (move a hot/cold bucket, watch `r`) instead of
assuming either answer. The cheapest fix, if the wander turns out to matter, is one $2 I²C temperature
sensor on the existing bus — the code already takes the air temperature as a config value, so the
change is a driver plus one assignment.

**4.3 Failure modes, and what the firmware does about each.** This is the whole reason the phase-0
test plan exists.

| failure | what you see | firmware behaviour |
|---|---|---|
| **dew / a water film on the face** | echo at a few cm, or nothing; jittery | rejected by the blind gate; counted in `bad`; status `DEW ON FACE`; the module's own LED still flashes, which tells you the trigger got there |
| **surface closer than 20 cm** (tank over-full) | `NEAR/BLIND` reads | reported as a distinct reason, **not** faked as "100 %"; the high alarm normally fires first, at 110 cm |
| no echo at all (surface steeply rippled, foam, algae mat) | 40 ms timeout | reason `NO ECHO`, held last-good value for 20 samples, then `NO READING` |
| silt bed / sloped floor when empty | `e` reads 148 instead of 152 | harmless: it shifts 0 % by a few cm. Set `e` with the tank *actually empty*, not from the drawing |
| wall echo | — | cannot happen: the beam footprint at the floor (233 cm) is inside the tank (304.8 cm) |
| lid/hole clutter | echo at 15 cm | inside the module's own 20 cm blanking window |
| insects nesting on the face | slow drift toward small distances | annual inspection; there is no sensor-side fix that does not cost you the ping |
| a 15 s WiFi outage | readings continue | logging and the local buzzer never stop; Blynk reconnects with a bounded backoff |

**4.4 Filtering.** Median-of-15 over the sample stream (not a moving average), `MAX_JUMP_CM 12` with a
3-sample confirmation, and a 20-sample hold of the last good value. Ripples and one bad echo burst
therefore cannot move the reported level; a real fill or drain is accepted after 3 samples. A ping at
least 60 ms after the previous one is enforced in code, because the module needs > 50 ms between
triggers and the serial console, the web page and the timer can all ask for a sample.

## 5. Pin map (adopted from the reference project — and where we deliberately differ)

The reference project's GPIO numbers are used as published, so a reader can follow the article's
schematic and PCB with this repo. Two differences are deliberate and marked.

| Function | GPIO | DevKitC silkscreen | Notes |
|---|---|---|---|
| ultrasonic **TRIG** | **27** | D27 | 3.3 V, 12 µs high pulse; TTL input ≥ 10 µs is met comfortably |
| ultrasonic **ECHO** | **26** | D26 | **through the 2 kΩ/3.3 kΩ divider** — difference #1, see below |
| I²C **SDA** (OLED) | 21 | D21 | same as the reference project |
| I²C **SCL** (OLED) | 22 | D22 | same as the reference project |
| **button** (silence / wake) | 12 | D12 | to GND, `INPUT_PULLUP` — **strapping pin, read the hazard below** |
| **buzzer** drive | 13 | D13 | → 220 Ω → BC547/S8050 → 5 V piezo, same as the reference project |
| **LED** (full + heartbeat) | 14 | D14 | → 220 Ω → 5 mm LED to GND |
| battery/5 V-rail sense | 36 | SN | ADC1_CH0, 1 MΩ/1 MΩ divider, **input-only pin**, never an output |
| not used on purpose | 25, 23, 17, 18, 19, 2, 0, 5, 15, 4, 13-related, 34/35/39 | — | free for phase 2; **D6–D11 are SPI flash lines, never usable** |

**Difference #1 — the echo divider, which the reference circuit does not have.** The reference part
list is two 220 Ω resistors and nothing else, i.e. ECHO goes straight into D26. That is fine on an
Arduino UNO (5 V logic) and **not fine on an ESP32**: a 5 V-swinging ECHO into a 3.3 V pin is an
absolute-maximum violation and kills the pin. This build powers the module at 5 V (best acoustic
margin) and puts **2 kΩ in series from ECHO, 3.3 kΩ from the pin to GND → 3.00 V** on GPIO26.
Alternative that also works and drops the divider: power the module from **3V3** (allowed: 3.0–5.5 V) —
then its echo is 3.3 V and the pin is safe. Range drops; on a 1.52 m worst case it still has ~3× margin.
Pick one, do not do neither. Diagram: [`img/01-wiring.svg`](img/01-wiring.svg).

**Difference #2 — the LED has two jobs.** In the reference build the green LED means "tank full". Here
GPIO14 blinks once per 1.5 s as a heartbeat and goes **solid on for the high (full/overflow) alarm**,
so the box tells you it is alive *and* it shouts visually. (If you are following the reference PCB,
that is the only behavioural difference you will notice.)

**The GPIO12 hazard, inherited from the reference design, stated plainly:** GPIO12 is MTDI, a strapping
pin sampled at reset; at power-on the ROM decides the flash I/O supply from it, and a LOW there selects
1.8 V VDD_SDIO and the module will not boot. The button wires GPIO12 to GND while pressed. So
**do not hold the button while plugging in / resetting** — the board sits dead until you let go and
power-cycle. Nothing is damaged. If you want a button that is safe to hold during power-on, move it to
GPIO23 (free) and leave the buzzer on 13; the code keeps every pin in a `#define` at the top of the
sketch so that is a one-line change.

Other electrical notes: keep the TRIG/ECHO runs under ~30 cm of wire, put 100 nF + 100 µF across the
module's supply pins at the module (the 8-cycle burst is a few mA but the cable is the antenna), and
**do not cut or extend the probe's own cable** — the vendor sells it at a fixed length and the return
path is part of the design.

## 6. Outputs and where the data goes

| Output | Detail |
|---|---|
| **OLED** 0.96" SSD1306 128×64, I²C `0x3C`/`0x3D` | hand-rolled driver in `wlg_display.h` (no Adafruit), 1 kB frame buffer, blanked after 120 s, `OLED_IDLE_S 0` for a mains-powered box. Row map: [`img/04-display-and-app.svg`](img/04-display-and-app.svg) |
| **buzzer** GPIO13 | local, works with no internet and no Blynk account: 250 ms every 9 s; 1.8 kHz low-level, 2.6 kHz high/fault. `j` mutes, the button short-press snoozes, 1.5 s hold toggles mute |
| **LED** GPIO14 | heartbeat, solid on when full |
| **Blynk IoT** (optional, compile-time `ENABLE_BLYNK`) | same template the user already has; streams and events in §6.1 and [`05-blynk-and-display.md`](05-blynk-and-display.md) |
| **LAN web server** (unchanged, always on) | `/` test page, `/api/state` JSON, `/api/log` force a sample, `/api/scan` RSSI. AP fallback `WaterLevel-Setup` / `waterlevel` when WiFi is wrong |
| **serial console** 115200 | `r s c w p e f a i b l u j t g d D k x h` — see the sketch's `help()` |

### 6.1 Blynk virtual pins — one map, and four streams retired

The user's Blynk template already exists with V0…V16 and four event codes, so the mapping stays
stable and the hydrostatic streams are **retired rather than renumbered**:

| stream | type | now |
|---|---|---|
| V0 level_cm, V1 level_pct, V2 litres, V3 dist_cm, V4 vbat, V8 status | every sample | kept |
| V9 rssi, V14 bad_pct, V15 uptime_h | hourly | V9 kept; **V14 and V15 repurposed** from `p_ref`/`p_tube` to diagnostics |
| V10 sample_s, V11 low_cm, V12 high_cm, V13 mute | written **by the app** | kept, unchanged |
| **V5 air_t, V6 air_rh, V7 water_t, V16 p_diff** | — | **retired: delete these four datastreams in the template** (nothing writes them any more; a gauge widget parked at −1 is a retired stream, not a fault) |
| events `level_low`, `level_high`, `sensor_fault`, `level_ok` | — | kept, same codes |

Message budget is unchanged and still the reason the hourly set exists: free plan = 100 000 device
messages/month, six fast streams at a 60 s sample rate is ~259 200 — over quota. `i 900` (15 min) plus
the `BLYNK_THROTTLE_S 300` rule brings it to ~17 % of quota. Table in `05-blynk-and-display.md`.

## 7. Power

Tree: [`img/03-power.svg`](img/03-power.svg). Unchanged in architecture, and the module got cheaper:
the SR04M draws **< 8 mA while pinging, < 2 mA standby** (rev A's JSN-SR04T was ~15 mA), so the gated
5 V sensor rail is now about saving 2 mA, not 15.

| Rail | Source | Why |
|---|---|---|
| 5 V | boost from the 1S battery (TPS61023, 18 µA IQ; cheap alt MT3608 module) | the module wants 4.5–5.5 V for full transmit margin, the battery is 3.0–4.2 V, so a boost is enough |
| 3.3 V | the DevKit's own AMS1117 | honest maths at 22 mA average ≈ 40 mW — warm, not hot |
| sensor rail | 5 V through a high-side load switch (TPS22919) | kills the module's 2 mA standby between reads, and stops back-powering through the I²C pull-ups — the classic "deep sleep is 8 mA, not 20 µA" bug |
| bench build | USB-C (5 V) straight in | everything in the test plan except the battery rows runs from USB. The reference project's own options are 5 V DC or its PM01 AC-DC module — same answer, more headroom, and no solar parts to buy while you are still proving the sensor |

Battery/current reality with a DevKitC (CP2102 + power LED add 5–10 mA a bare module would not have):

| Mode | Avg current | 1 × 18650 (3 Ah) | 2 × 18650 (6 Ah) | panel |
|---|---|---|---|---|
| always-on WiFi, modem sleep off | 60–95 mA | 1.3–2 d | 2.6–5 d | 3–4 W |
| **always-on WiFi, modem sleep on, 30 s sampling** | **20–25 mA** | **5–6 d** | **11–12 d** | **≈1 W, fit 2–6 W** |
| light sleep between samples | 4–8 mA | 2–4 weeks | 4–8 weeks | 0.3 W |
| deep sleep, wake per sample | 50–100 µA — **not reachable on a DevKitC** | — | — | < 0.1 W |

The deep-sleep row is a trap: on a DevKitC the AMS1117, CP2102 and pull-ups still draw 0.8–2 mA with
EN low. Sub-100 µA needs the phase-2 board (and the SR04M's mode 2, §2.1). Panel sizing: 3.7 V ×
22 mA ≈ 2 Wh/day; at 3.5 peak-sun-hours and 70 % charging efficiency a 2 W panel covers it, 6 W
survives a monsoon week. OLED lit adds 3–8 mA while awake — first thing to trim if battery life comes
up short, and the idle blanking (120 s) is what keeps that a fraction of the average.

18650 + CN3791 + 6 V panel recommendation unchanged; keep the cell and charger in the shaded lower
compartment (a dark box in full sun reaches 60 °C, and CN3791's TEMP pin must suspend charging below
0 °C). 470 µF bulk on 5 V plus 100 µF at the ultrasonic keeps the boost out of current limit during
WiFi TX bursts.

## 8. Removed in rev B — the "no remnants" checklist

Nothing in this repo refers to the old pressure-sensing design any more. Grep for all of these and get
zero hits outside this section and `docs/06-changelog.md`:

`SENSOR_BARO_DUAL`, `SENSOR_TYPE`, `baroRaw`, `waterDensity`, `pRefPa`, `pTubePa`, `baroOffsetCm`,
`BME280`, `BMP280`, `0x76`, `0x77`, `DS18B20`, `ONEWIRE`, `PIN_LVL_ANA`, `TUBE`, `chimney`,
`submersible`, `hydrostatic`, `bubbler`, `1133 hPa`, `1200 hPa`, `1100 hPa`, `119.2 hPa`,
`119.6 mbar`, `250 mm chimney`, `52.2`, `174`, `52.2 cm`.

| removed | what replaced it |
|---|---|
| `SENSOR_TYPE` build switching (1 ultrasonic / 2 baro-dual) and `#define SENSOR_ULTRASONIC/SENSOR_BARO_DUAL` | one build. The compile switches that remain are `ENABLE_OLED` and `ENABLE_BLYNK`, which are output options, not sensor options |
| BME280/BMP280 raw-I²C drivers (≈150 lines), the vented tube, the sealed-column correction maths | one `pingRaw()` against the SR04M |
| DS18B20 1-Wire driver and water temperature (only ever existed to correct water *density*) | air temperature as a config value (`t <°C>`) for sound speed |
| submersible-probe option (`PIN_LVL_ANA`, GPIO33) and the reed-ladder option | deleted, not marked "future" |
| baro calibration commands `o <cm>` / `z` | `t <degC>` (sound speed) and `g <cm>` (blind gate), plus `e`/`f` which were always the ultrasonic pair |
| `maxLevelCm` / full-scale depth config and `m <cm>` command | the span is derived: `d_empty − d_full` |
| CSV columns `p_ref_hPa`, `p_tube_hPa` | `air_t` (the config value actually used), `echo_reason` |
| OLED rows `AIR …°C …%` and `H2O …°C`, DP row | `ECHO` distance, `GATE`/`SPAN` diagnostics row |
| Blynk V5/V6/V7/V16 | retired; V14/V15 repurposed to `bad_pct` / `uptime_h` |
| 250 mm printed chimney, 60 mm bore | 100 mm lid hole + 150 mm probe clearance, sized for the 75° cone |
| `docs/03-hydrostatic.md` (tube physics, bubbler videos, sealed-column error tables) | `docs/03-ultrasonic.md` (module protocol, blind zone, mounting, error budget, ultrasonic videos) |

## 9. Phases

| Phase | Output | Gate to proceed |
|---|---|---|
| 0 | this firmware on your DevKitC: SR04M read + blind gate + median filter + `e`/`f` calibration + CSV log + LAN test page + RSSI log, plus the optional OLED / buzzer / button and the optional Blynk app + push | the six tests in [`02-phase0-test-plan.md`](02-phase0-test-plan.md) pass on a bucket, then 24 h on the real lid |
| 1 | KiCad schematic: power/charge/battery, MCU interface, the SR04M 4-pin header, OLED header, buzzer/LED/button | ERC clean, BOM with LCSC/Farnell numbers — [`04-parts-list.md`](04-parts-list.md) |
| 2 | 2-layer PCB, gerbers/drill/PnP/STEP/PDF | boards arrive, flash phase-0 firmware unchanged |
| 3 | real web UI, calibration UI, history CSV, OTA, captive portal, mDNS, auth, MQTT / Home Assistant bridge; SR04M mode 2/4 for battery life | mounted and forgotten |

Phase 2 variants: **2a** a small interface+power daughter board that keeps the DevKitC plugged in
(recommended: USB flashing keeps working, no RF re-layout, no antenna risk) or **2b** a custom board
with a bare ESP32-WROOM-32 footprint (smaller and cheaper per unit, but the PCB antenna keep-out must
be respected or the thing fails on the roof).

Read-only by decision, same as the reference project's "no relay" note: nothing in any phase switches
a pump or a valve. What phase-0 firmware already does locally — thresholds, hysteresis, buzzer, OLED
banner, phone push — works with no internet at all.