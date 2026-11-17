# 03 — How build 2 (the tube) actually works

Build 2 is `SENSOR_TYPE = SENSOR_BARO_DUAL`, the sketch's compile default, and the build the
2026-11-16 site survey picks for this house (`04-parts-list.md` §10).

> **No `.ino:NNN` line numbers in this file any more** (2026-11-17). The ones that were here pointed at
> the sketch before the OLED / Blynk / alarm code went in (~250 lines added) and had gone wrong. Named
> functions instead: `baroRaw()` does the differential and the over-range warning, `waterDensity()` the
> temperature correction, `sampleValid()` the bounds, `statusText()` the `TUBE OVER RANGE` string, and the
> `z` handler inside `handleSerial()` does the zeroing.

Everything here is unaffected by the display and the phone app: `V14`/`V15`/`V16` in the app are
`p_ref_hPa`, `p_tube_hPa` and `p_diff_hPa` — the same three numbers this document is about, posted
hourly ([`05-blynk-and-display.md`](05-blynk-and-display.md) §5).

A widget gotcha for those three: a value the sensors cannot produce is sent as a **sentinel, not as
"no data"** — −1 for the three pressures, −99 for air/water temperature, 0 for RSSI when WiFi is down.
A gauge widget that displays −1 hPa is telling the truth: there is no tube reading, which in build 2
means exactly the failure this document warns about (water in the tube, or the tube-side chip not
answering at `0x77`).

## 1. The idea in one sentence

**You never measure the water — you measure the air above it.** The water at the tank floor presses on
the air trapped in the tube; that pressure travels up the tube to a barometric sensor sitting dry in the
box, and level = pressure difference ÷ density of water.

```
   box on the lid                     steel lid
  ┌───────────────────┐          ══════╪═══════════
  │ BME280 @0x76      │ ← box air      │   ← the lid is NOT penetrated by anything wet
  │   (reference)     │                │
  │ BMP280 @0x77      │ ← tube air ────┴──╮  sealed tube, 4-6 mm, dry above the water
  └───────────────────┘                    │
                    water vapor, dew, rain │  nothing electronic in here
                        ┌ ─ ─ ─ ─ ─ ─ ─ ─┘
      air space         │ │
   ═══════ water surface╵══╵════════════════ 121.9 cm of water above this point
      │                            │
      │            tube opening ◄──╯  ← open end sits on the tank floor
      └────────────────────────────────
```

Water at depth **h** pushes with `ρ·g·h`. At 25 °C that is **≈9.78 Pa per cm**, so:

| quantity | value for this tank |
|---|---|
| head at the floor, 121.9 cm of water | **11.92 kPa = 119.2 hPa** |
| 1 hPa of pressure | **1.023 cm of water** (what the `c` command prints: `1 hPa ≈ 1.03 cm water at 25 °C`) |
| absolute pressure at the tube sensor, sea level | ≈1013 + 119 = **1132 hPa** |
| BMP280 measuring range | 300–**1200** hPa ✔ |
| BME280 measuring range | 300–**1100** hPa ✘ — it would sit pegged at the top, hence two different chips |

The code does exactly this in `baroRaw()`: `dpPa = pTubePa - pRefPa`, then
`cm = dpPa * 100.0f / (rho * 9.7803f) + baroOffsetCm`, with `rho` looked up from the DS18B20 water
temperature (`waterDensity()`) so the litres figure stays honest as the water warms.

## 2. Why there are two sensors and not one

Because the tube sensor sees **weather + water**, and weather is the bigger number.

- A monsoon low: 1013 → 990 hPa = **23 hPa = 23.5 cm of phantom water level**
- The whole tank: 121.9 cm = **119 hPa**

So a single absolute sensor in the tube would be reading mostly barometric pressure, not water. The
box-side BME280 at 0x76 samples the same weather a few centimetres away, and the subtraction in
`baroRaw()` cancels it. Bonus: that same chip gives the box temperature and %RH that drive the dew
warning in `/api/state` (`air_t`, `air_rh`).

## 3. What the videos do not tell you (and what our own docs did not say until today)

Almost every YouTube video on this technique is a **bubbler**: a pump pushes a small steady stream of air
down the tube, bubbles come out of the open end, and the back-pressure equals the head exactly. Our
build is the **sealed** variant — no pump, the tube top is closed by the BMP280. That difference has two
consequences, and I ran the numbers rather than guessing:

**3.1 Water climbs into the tube and compresses the trapped air → the reading is biased low.**
Equilibrium: `p₀·(T/T₀)·L/(L−x) = p_atm + ρg(h−x)`, where **L** is the trapped air length and **x** how far
the water rises into the tube. For our tank (L = 1.37 m of air, water at 25 °C):

| true level | ideal Δp | water rises into tube by | Δp actually seen | indicated level | error |
|---|---|---|---|---|---|
| 20 cm | 19.6 hPa | 2.3 cm | 17.3 hPa | 17.7 cm | **−11.5 %** |
| 60 cm | 58.7 hPa | 6.7 cm | 52.1 hPa | 53.3 cm | −11.2 % |
| 100 cm | 97.8 hPa | 10.9 cm | 87.2 hPa | 89.1 cm | −10.9 % |
| 121.9 cm | 119.2 hPa | 13.0 cm | 106.4 hPa | 108.9 cm | **−10.7 %** |

So at a true full tank the raw physics says **108.9 cm** — 13 cm low. A shorter air column is a stiffer
air spring and errs less (with L = 0.60 m the same table reads −5.0 %). This is the reason industrial
"wet leg" installations deliberately fill the reference tube with liquid: no air spring, no error — but
then the water reaches the sensor port, and a BMP280's port is not water-proof (it dies, same failure the
`baroRaw()` over-range message warns about).

**3.2 The trapped air's temperature moves the reading.** Same equation, tank held at 121.9 cm, L = 1.37 m:

| tube air temperature | indicated level |
|---|---|
| 20 °C below reference | 101.0 cm |
| reference (calibration point) | 108.9 cm |
| +12 °C | 113.3 cm |
| +24 °C | 117.8 cm |

A vented box on a terrace runs from ~28 °C at night to 55 °C in the afternoon. **That is ~9–25 cm of
driven error at a constant water level**, and it is *not* removable by re-zeroing, because it follows the
box temperature, not the weather. The box-side sensor does measure that temperature (`airT`), so the
firmware **can** correct for it — see §5.

**3.3 Firmware gap.** `baroRaw()` has an **additive** correction only (`cfg.baroOffsetCm`), and `m <cm>`
sets full-scale for the percentage, not a gain. So the −11 % in §3.1 **cannot be calibrated out today**:
a single offset is right at one level and wrong everywhere else. Fixing it needs one of: a gain command,
or the gas-law correction in §5, or a different sensing principle (build 3, or a purged bubbler, §6).

## 4. Keeping the sealed tube alive (the reason build 2 is high-maintenance)

1. **The tube must stay dry above the water line.** Condensation inside it drips onto the BMP280.
   Route it with a **low loop** below the entry so water collects at the loop, keep the tube touching the
   wall/water where it is thermally stable, and put a **silica-gel bead in the top 10 cm**.
2. **The tank air space must be vented** (your overhead tank already is — overflow and lid gap). A sealed
   tank pressurises with temperature and the gauge lies.
3. **Tip fouling:** silt and carbonate scale on the open end. Sit it 50–100 mm off the floor on a stand
   or inside a 100 mm UPVC cup open at the bottom.
4. **the `baroRaw()` over-range message** (`tube-side pressure above 1200 hPa`) means one thing in the field:
   **water has reached the sensor**, or the tank vent is blocked. Not "the tank is overfull".
5. **`r` on the serial port shows both pressures.** If `box` and `tube` differ by more than ~125 hPa at a
   known water level, suspect water in the tube before suspecting the code.

## 5. Bench calibration procedure for build 2 (do this before it goes on the tank)

1. Tube **open to air**, box closed: `z` → zeroes the differential (the `z` handler refuses if no valid
   tube reading).
2. Put the tube's open end in a tall bucket of water and measure the depth with a ruler: 30 cm, 60 cm,
   90 cm, 120 cm. Read `r` at each. Plot indicated vs true. **The slope is your gain error; the shape of
   the curve is §3.1's air spring.** If the curve is not a straight line, the sealed-tube build is not
   fit for this tank and you move to build 3 or a purge.
3. Note the box temperature at calibration. Then warm the box with a hairdryer 10 °C and watch `r`: that
   is §3.2 in 30 seconds, on the bench, before it is on the roof.
4. `m 121.9` sets full scale, `a 92903` sets the tank area, `o <cm>` is the additive offset — record the
   numbers you used in the table in §2 of the test plan.

## 6. Two firmware paths that make build 2 honest

- **`l <cm>` (trapped air column length) + gas-law compensation.** We already measure box air temperature
  and water temperature, so `T_tube = weighted average` is available; the §3.1 equation is ~15 lines and
  turns the −11 % into a computed term instead of a mystery. This is the cheapest correctness win in the
  project.
- **Purge (`build 2b`)**: an aquarium air pump (₹250–400) + a one-way check valve on the tube, driven for
  3–5 s before each sample. It blows the condensate out, keeps the tube at true line pressure while it
  blows, and after the purge the residual pressure settles to the head. That converts build 2 from a
  sealed column into a duty-cycled bubbler — immune to §3.1, §3.2 and condensation. Cost: ~0.5 W when
  running, so this path wants mains power, not the 18650s.

## 7. Verified videos (watch pages checked 2026-11-16)

**The tube principle — sealed vs bubbler. Watch these three first.**

| | video | why this one |
|---|---|---|
| ⭐ | [Tutorial: Measuring liquid level using a bubbler](https://www.youtube.com/watch?v=hs7ewLdVxo0) | hands-on bench build with a tube and a pressure sensor — the closest thing to wiring your own |
| ⭐ | [Bubbler Liquid Level Sensing Lab](https://www.youtube.com/watch?v=qWQgoSmAqkI) | a lab demonstration: you can *see* the bubbles and the back-pressure relationship |
| ⭐ | [Measuring Liquid Level with a Bubbler Tube](https://www.youtube.com/watch?v=vK0UVHHlRQ8) | short and clear on why pressure = depth, independent of tube length and shape |
| | [Bubbler method for level measurement](https://www.youtube.com/watch?v=dA68x95oHP4) | the clean single-diagram explanation |
| | [Bubbler Level Measurement Working Principle](https://www.youtube.com/watch?v=WoOaUJKDMMI) | industrial animation, shows the venturi/ejector version used without electricity at the tank |
| | [Bubbler Type Level Transmitter / Working Principle / Applications](https://www.youtube.com/watch?v=P6RCBAQgNvU) | instrumentation-course treatment, with the failure modes listed |
| | [Bubbler Method for level measurement · InstrumentAcademy](https://www.youtube.com/watch?v=tuWlRZpCNXc) | same topic, slower pace, worked Δp→level arithmetic |
| | [Bubler Level Measurements](https://www.youtube.com/watch?v=7-FW7irHJaU) | quick field demo (hashtag short; low production, but the effect is visible) |

**The pressure→level arithmetic (your 1 hPa ≈ 1.02 cm question).**

| | video | note |
|---|---|---|
| ⭐ | [Sensorics — Hydrostatic Level Sensors: Pressure to Level](https://www.youtube.com/watch?v=96c7hmrEONU) | exactly the conversion build 2 does, including specific gravity |
| | [What is Hydrostatic Level Transmitter? Working Principle Explained](https://www.youtube.com/watch?v=bsSkQsv-jYE) | the direct-diaphragm (submersible) version = **build 3** |
| | [Hydrostatic pressure · How does hydrostatic level measurement work?](https://www.youtube.com/watch?v=hwT5cEmuV7s) | 2-minute principle animation |
| | [What is Hydrostatic Pressure? — Level Measurement Instrumentation](https://www.youtube.com/watch?v=xuBU4uxFjzo) | the ρ·g·h derivation from first principles |

**Closest DIY matches to our two builds.**

| build | video | note |
|---|---|---|
| 2 (baro) | [How to make a Wireless Water Level Indicator using BMP280](https://www.youtube.com/watch?v=fmargmIxNzc) | the same chip we use for level, wirelessly |
| 2 (baro) | [make a pressuremeter / water depth by arduino bmp280](https://www.youtube.com/watch?v=7rXG54g18UE) | BMP280 → water depth arithmetic (Arabic narration, code on screen) |
| 2 (baro) | [How Does a Barometric Altimeter Work? BMP280 Explained](https://www.youtube.com/watch?v=Z8jYic4gm2Y) | useful for the flip side: pressure→height of *air*, which is why weather fools the tube |
| 1 (ultrasonic) | [Smart Water Tank Monitoring Using JSN-SR04T & Arduino](https://www.youtube.com/watch?v=7Awg-TG8Eps) | our exact sensor, tank application |
| 1 (ultrasonic) | [Interfacing Waterproof Ultrasonic Sensor JSN-SR04T with Arduino](https://www.youtube.com/watch?v=KeMevxyaI2g) | wiring and the 5 V/3.3 V question (we solve it with the 2 kΩ/3.3 kΩ divider, `docs/01-wiring-phase0.md`) |
| 1 (ultrasonic) | [Arduino Water level controller using Ultrasonic sensor JSN-SR04T-2.0](https://www.youtube.com/watch?v=_RVH1HyXy3s) | the blind-zone problem, visibly |
| 1 (ultrasonic) | [Measuring Water Level using Arduino and JSN-SR04T](https://www.youtube.com/watch?v=_6FEoRIPKng) | calibration of empty/full distances, i.e. our `e`/`f` commands |
| 1 (ultrasonic) | [medidor de nivel de agua para cisternas y tinacos WIFI (ESP32)](https://www.youtube.com/watch?v=vxnQ0Rbaa8Y) | Spanish, but it is the same job: rooftop tank, ESP32, WiFi — the mounting is worth a look |
| — | [Automatic Water Tank Overflow Control · Pump OFF · ESP32 · Blynk](https://www.youtube.com/watch?v=w_c4EGb0a_s) | **pump control**, which this project deliberately does *not* do — useful only if you change your mind |
| — | [ESP32 Water Level Sensor with ESPHome](https://www.youtube.com/watch?v=-qbqCAZ7mTc) | a different firmware stack (ESPHome); good dashboard ideas, not compatible with this sketch |

Watch for what is **not** in these videos: none of them has a sealed tube on a hot metal box over a
1.22 m tank, which is §3's whole problem.
