# Water Level Gauge — design record (rev A)

Owner hardware: **ESP32-WROOM-32 DevKitC V1, 38-pin, CP2102** + 38-pin "ESP32S" expansion board.
Enclosure: 3D-printed, **magnet-mounted on top of** the tank lid (not inside it — see §2).

Code is a plain **Arduino sketch** (`arduino/water_level_gauge/water_level_gauge.ino`), no external
libraries. Diagrams: [`img/01-wiring.svg`](img/01-wiring.svg) wiring map,
[`img/02-mounting.svg`](img/02-mounting.svg) geometry to scale,
[`img/03-power.svg`](img/03-power.svg) battery/solar tree.

## 1. Tank as measured

| Item | Value |
|---|---|
| Internal footprint | 10 ft × 10 ft = 9.29 m² (304.8 × 304.8 cm) |
| Water depth (4 ft, lid gap excluded) | **121.9 cm** |
| Gap from full water line to lid | **15.2 cm (6")** |
| Full volume | **11.33 m³ = 11 327 L ≈ 2 990 US gal** |
| Volume per 1 cm of level | **92.9 L** |
| Volume per 1 in of level | 236 L |
| Hydrostatic head at floor | 119.6 mbar (0.120 bar / 1.73 psi) |

The UI reports cm, %, and litres. Litres = `level_cm × 92.9`, and it is a **config value**, not
a constant — a concrete tank's interior is never exactly 10×10 and there is usually a silt bed.

## 2. Three things that would have killed the "magnet it inside the lid" plan

**2.1 RF — a steel lid is a Faraday cage.**
Electronics mounted on the *inside* of a metal lid, inside a rebar-reinforced concrete tank, get
essentially no 2.4 GHz signal. There is no firmware fix for this. The box must live **on top of the
lid**, outside the tank. Magnets hold at least as well on the outside of a steel lid, so this costs
nothing except one drilled hole for the sensor cable.

**2.2 Condensation — a dev board does not survive 100 % RH.**
An occupied water tank runs near saturation day and night; inside a closed box under a steel lid the
dew point sits *above* the PCB temperature for hours. A bare DevKitC there corrodes and
shorts within months. So: box outside and vented (labyrinth vents, not sealed), desiccant cartridge,
conformal coat on the assembled PCB, and **no potting of the ESP32 antenna area**.

**2.3 Ultrasonic blind zone — 15.2 cm is too little headroom.**
Face-to-surface distance at full level, by mounting:

| Mount | face → full level | verdict |
|---|---|---|
| probe under lid, magnetic | **15.2 cm** | fails — JSN-SR04T rated from **20 cm**, A02YYUW from 25 cm; with swell, dew and a 3D-printed rim you need ≥40 cm |
| box on lid, 120 mm tall | 27.2 cm | marginal, no margin for wave/swell or the rim echo |
| **box on lid + 250 mm chimney** | **52.2 cm** | ✅ correct; 174 cm to the floor when empty |

Sensor spec (JSN-SR04T V2/V3, waterproof probe): **3.0–5.5 V supply**, range **20–450 cm** (some
listings 20–600 cm), beam ≈5–8° half angle, ≈15 mA while pinging. It can technically run off 3.3 V,
which would remove the level divider — but transmit margin drops with supply voltage, so run it at
5 V and keep the divider. Diagram: [`img/02-mounting.svg`](img/02-mounting.svg) shows the cone at both
water extremes: Ø≈13 cm at the full line, Ø≈36 cm at the floor — both clear the lid gap and land in
the middle of the tank, so there is no wall echo.

**Chosen geometry:** the box sits on top of the lid; the ultrasonic transducer is at the top of the
box looking down through a 60 mm hole. A 3D-printed 250 mm chimney above the lid (not a long narrow
tube *into* the tank — a tube that long gives a strong wall echo that the sensor reports instead of
the water) puts the face 52 cm above the full water line and 174 cm above the floor. The hole rim is
~3 cm from the face, i.e. inside the blind zone, so its echo is ignored by design.

```
        ☀ sun shade (printed louvre, keeps Li-ion out of 60 °C)
     ┌──────────────┐   ← ESP32 + antenna in the TOP third,
     │  ESP32  📡   │     ≥80 mm above the lid, away from magnets/cell
     │   ●  probe   │   ← transducer faces down, 10° tilt so dew runs off
     ├─┬──┬─────┬──┬┤   ← Neodymium N45 magnets ×4 on the underside of the base
     │ │  │     │  ││   ══════════ steel lid ══════════   60 mm hole
     └─┴──┴─────┴──┴┘        │  250 mm printed chimney, no tube in the tank
   ═══════════════════════   │
      15.2 cm air gap        ▼      ← 52.2 cm face→surface at FULL
   ~~~~~~~~ full level ~~~~~~~~~~~~
            │  max 121.9 cm of water
   _________▼_________ tank floor
```

## 3. Sensor decision (still your call — the code supports both)

| Option | Accuracy | Idle current | Maintenance | Notes |
|---|---|---|---|---|
| **A. Ultrasonic, chimney mount** ← what the phase-0 bench plan rehearses (**build 1**, `SENSOR_TYPE 1`, the video's method) | ±5–10 mm | 3.5 mA, power-gated to ~0 | face dew, wasps build nests in the chimney | no contact with water, cheap. **The 2026-11-16 site survey rules it out for this tank** — concrete body, metal lid, unventable, everything inside wet (`04-parts-list.md` §10). Rehearse with it; do not ship it here |
| C. Hydrostatic: **BMP280 tube + BME280 box** + vent tube to floor (**build 2, the code default**) | ±10–20 mm | ~1 µA | PTFE membrane tip fouls in drinking water; the tube must stay dry | no blind zone; **the tube-side chip must be a BMP280** — 121.9 cm adds ≈120 hPa → ≈1133 hPa, past the BME280's 1100 hPa ceiling; the baro offset (`o`) needs re-zeroing after any weather swing. `04-parts-list.md` §10.2 rates this the cost-and-accuracy winner for this site: ₹937, ±1–2 cm |
| B. Reed ladder, 8 floats in 40 mm stilling well | 15 cm steps | ~0 | float fouling | dead simple, coarse |
| D. Submersible hydrostatic probe on GPIO33 — **build 3, not compiled yet** | 1 % of full scale: ±7 cm on a 0–10 PSI part; ±1–2 cm only at 0–2 m span | needs an analog read, and a 24 V rail for 4–20 mA parts | probe 50–100 mm off the floor; the cable's vent hole must stay dry | `04-parts-list.md` §10. No firmware support: `SENSOR_TYPE` takes 1 and 2 only, `PIN_LVL_ANA` is declared and unused. Everything above the sensor — filter, `Rd`, page, CSV, OLED, alarms, app — is already sensor-agnostic, so build 3 is ~40 lines of driver |

A and C are compiled into the sketch: `#define SENSOR_TYPE` near the top of
`arduino/water_level_gauge/water_level_gauge.ino` — `SENSOR_ULTRASONIC` is 1 (A), `SENSOR_BARO_DUAL`
is 2 (C). The **code** defaults to C because it is the only one that needs no chimney; compile A with
`-DSENSOR_TYPE=1`. **Which of them this house gets stopped being a code question at the site survey**
(`04-parts-list.md` §10, 2026-11-16): a concrete overhead tank with a metal lid that cannot be sealed
rules A out outright, makes C the phase-1 population choice, and leaves D as the "I refuse to own a
tube" fallback. Keep the code default and the field build straight in your head when you flash — the
default is not automatically the right build.
Reject any ultrasonic reading closer than the configured blind-zone constant — that is the
difference between a plausible gauge and one that reads "tank is full" forever.

## 4. Power

Tree: [`img/03-power.svg`](img/03-power.svg).

| Rail | Source | Why |
|---|---|---|
| 5 V | **boost** from the 1S battery (TPS61023, 18 µA IQ; cheap alt MT3608 module) | the battery is 3.0–4.2 V and the ultrasonic module wants 4.5–5.5 V, so a boost — not a buck-boost — is enough |
| 3.3 V | the DevKit's own AMS1117 is fine | honest maths: 22 mA average × (5 − 3.3) V ≈ **40 mW**, warm not hot. The 0.4 W figure is the *WiFi TX burst* instant, not the average. Feeding the `3V3` pin from a separate buck is an efficiency option worth ~10–15 mA, not a requirement. |
| Sensor rail | 5 V through a high-side load switch (TPS22919) | gates the 15 mA module between reads; also stops back-powering through I²C pull-ups, the classic "deep sleep is 8 mA not 20 µA" bug |

Battery/current reality with **your** board (the DevKit's CP2102 + power LED add ~5–10 mA that a bare
module would not have):

| Mode | Avg current | 1 × 18650 (3 Ah) | 2 × 18650 (6 Ah) | panel needed |
|---|---|---|---|---|
| always-on WiFi, modem sleep **off** | 60–95 mA | 1.3–2 d | 2.6–5 d | 3–4 W |
| **always-on WiFi, modem sleep on, 30 s sampling** | **20–25 mA** | **5–6 d** | **11–12 d** | **≈1 W (use 2–6 W)** |
| light sleep between samples | 4–8 mA | 2–4 weeks | 4–8 weeks | 0.3 W |
| deep sleep, wake per sample | 50–100 µA — **not reachable on a DevKitC** | — | — | <0.1 W |

The deep-sleep row is a trap worth knowing before you design around it: on a DevKitC the AMS1117,
CP2102 and pull-ups keep drawing **≈0.8–2 mA** even with EN low, so "deep sleep" buys you days, not
months. Sub-100 µA needs the phase‑2 board where those parts are switched off or left off the BOM.

Panel sizing: load = 3.7 V × 22 mA ≈ 0.081 W ≈ **2 Wh/day**. At 3.5 peak-sun-hours and 70 % charging
efficiency a **2 W panel already covers it**; a 6 W panel gives ≈10× margin and survives weeks of rain.

If you fit the optional OLED ([`05-blynk-and-display.md`](05-blynk-and-display.md)), add roughly
**3–8 mA while it is lit** — with idle blanking (default 120 s, and a routine sample does not wake
it) that stays a fraction of the average, but it is the first thing to remove if battery life comes
up short. That figure is datasheet-class, **not** measured on our panel; the measured draw of a lit
SSD1306 in this box is still open.

Recommendation for phase 1: **2 × 18650 in parallel (1 S, no balancer needed) + 2–6 W/6 V panel +
CN3791 charge controller** (CN3791 takes 6–28 V in, so the panel connects straight to it; TP4056 needs
a regulated 5 V and is the wrong part for a 6 V panel). CN3791's `TEMP` pin is driven from the box's
own temperature sensor so charging is suspended below 0 °C — charging a freezing cell is how you
destroy one outdoors. A dark printed box in full sun reaches 60 °C, so the cell and the charger go in
the shaded lower compartment under the louvre shade, and 470 µF bulk on the 5 V rail plus 100 µF at
the ultrasonic keeps the boost converter out of current limit during WiFi TX bursts.

## 5. Phases

| Phase | Output | Gate to proceed |
|---|---|---|
| 0 | firmware for your DevKitC: sensor read + filter + calibration + CSV log + tiny test page + RSSI log — plus the optional local OLED/buzzer/button and the optional Blynk app + push, both compile-time switches (`05-blynk-and-display.md`) | 24 h of clean logs on a bucket, then 24 h on the real lid |
| 1 | KiCad schematic: power/charge/battery, MCU interface, populate-one-of A/B/C, BMP280 (tube) + BME280 (box) + DS18B20 | ERC clean, BOM with LCSC numbers — see [`04-parts-list.md`](04-parts-list.md) §5 |
| 2 | 2-layer PCB, gerbers/drill/PnP/STEP/PDF | boards arrive, flash phase-0 firmware unchanged |
| 3 | real web UI, calibration UI, history CSV, OTA, captive portal, mDNS, auth, MQTT / Home Assistant bridge | mounted and forgotten |

Phase 2 has two variants: **2a** a small interface+power daughter board that keeps your DevKitC
plugged in (recommended: keeps CP2102 USB flashing, no RF re-layout, no antenna risk) or **2b** a
custom board with a bare ESP32-WROOM-32 module footprint (smaller, cheaper per unit, but the PCB
antenna keep-out must be respected or the whole thing fails on the roof).

The alarm half of phase 3 landed early: thresholds, hysteresis, a local buzzer, an OLED banner and
phone push are already in the phase-0 firmware, and they are **local-first** — they work with no
internet and no Blynk account. What is deliberately *not* in any phase: switching a pump. Read-only by
decision, see `README.md`.
