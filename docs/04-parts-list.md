# 04 — Parts list, with Indian links

Prices are **₹, tax included, as pulled on 2026-11-16**. Amazon.in buy-box prices were read off the
product pages; anything marked *typical* I could **not** verify today, so treat it as a budget number,
not a quote. Stock and prices on Amazon India move weekly — re-check before ordering.

> **Function names, not line numbers.** The OLED / Blynk / alarm code added ~250 lines to the sketch, which
> broke every `.ino:NNN` reference in this file and in `03-hydrostatic.md`; they are gone as of 2026-11-17.
> The named function is quoted instead — `pingRaw()`, `sampleValid()`, `baroRaw()`, `statusText()`,
> `buzzerService()` — and it is stable because those names are what you search for in the `.ino`.

> **Order for the build you are actually going to flash.** The sketch defaults to
> `SENSOR_TYPE = SENSOR_BARO_DUAL` (**build 2, hydrostatic**); build 1 needs `-DSENSOR_TYPE=1` at compile
> time. That changes the order: build 1 = one ultrasonic
> module, build 2 = two baro modules **of different part numbers** (see §8 — this one is a hard
> requirement, not a preference).

Legend: ✅ = buy it · ➖ = only if you picked that build · ♻ = probably already own it

> **Added after the site survey:** the tank is a house overhead tank (concrete body, metal lid, cannot be
> sealed, everything in it is wet). That rules build 1 out outright and adds **build 3, submersible probe**
> — links, verified prices and the range-vs-accuracy math are in **§10**. Read §10 before ordering sensors.

---

## 1. The sensor — pick one (§8.1 explains why build 2 needs two different chips)

| part | what it is for | qty | price | link |
|---|---|---|---|---|
| ✅ **JSN-SR04T** waterproof integrated ultrasonic | **build 1** — the only non-contact option; sealed transducer, no probe in the water | 1 + 1 spare | **₹899** | [BITSKY INDIA, amazon.in](https://www.amazon.in/BITSKY-INDIA-Waterproof-Ultrasonic-Transducer/dp/B0FFNFVJGV) |
| ✅ **BMP280** module (GY-BMP280), I²C | **build 2, TUBE side** — measuring range **300…1200 hPa** is the whole point | 1 (+1 spare, they die if water enters the tube) | **₹158** | [Anu Electronics, amazon.in](https://www.amazon.in/Electronics-BMP280-Barometric-Atmospheric-Compatible/dp/B0H559HHXV) |
| ✅ **BME280** breakout, I²C | **build 2, BOX side** — reference pressure **plus %RH**, which the sketch needs for the dew warning | 1 | **₹779** | [SmartElex breakout, amazon.in](https://www.amazon.in/SmartElex-BME280-Atmospheric-Pressure-Breakout/dp/B0G2LPGXBT) |
| ✅ **DS18B20** in stainless probe, waterproof | water temperature: density for the litres figure + frost warning | 1 | **₹146** | [waterproof probe, amazon.in](https://www.amazon.in/Waterproof-Temperature-Environmental-Monitoring-Electronics/dp/B0HB3WB631) |
| ♻ ESP32-WROOM-32 DevKitC V1 (38-pin) | you own it; one spare removes the "did I magic-smoke it?" variable | 0–1 | ₹509–699 *typical* | [search: esp32 devkit v1](https://www.amazon.in/s?k=esp32+devkit+v1) |
| ➖ **HC-SR04** (bare, non-waterproof) | **bench stand-in only.** `pingRaw()`'s own comment says "JSN-SR04T / HC-SR04 protocol" — same Trig/Echo timing, same pins, same 2 kΩ/3.3 kΩ echo divider, **zero code change**. Buy it if you want to rehearse tests 1/2/5 and the `e`/`f` calibration for ₹200 instead of ₹899. Never on the tank: bare PCB + open 40 kHz cones + two-element TX/RX geometry | 0–1 | **₹200** ("Electronic spices", 4.1★, 200+ bought/mo, seen on the search page) | [amazon.in hc-sr04](https://www.amazon.in/hc-sr04/s?k=hc-sr04) · [roboway.in](https://roboway.in/shop/ultrasonic-distance-sensor-hc-sr04/) |

Cheap multi-packs look like a bargain and are not: the 2-pcs JSN-SR04T listing runs **₹2,853**
([B08T5XF6Y9](https://www.amazon.in/Integrated-Ultrasonic-Transducer-Waterproof-Compatible/dp/B08T5XF6Y9))
and the 4-pcs GY-BME280 runs **₹4,878**
([B0DQCX62RL](https://www.amazon.in/Atmospheric-Pressure-Temperature-Humidity-GY-BME280/dp/B0DQCX62RL)) —
both imported stock at 3× the domestic price. Buy single pieces from the India-listed sellers above.

## 2. Passives and bench parts (₹300–500 total)

One **1/4 W metal-film 1% assortment** covers every value the schematic uses — no need to buy singles.
[Aniann 1280-pc kit, amazon.in](https://www.amazon.in/Aniann-Resistor-Resistors-Assortment-Experiments/dp/B07L851T3V) · *typical ₹400–700* · [search](https://www.amazon.in/s?k=metal+film+resistor+assortment+1%25)

| value | qty | what it does |
|---|---|---|
| **2 kΩ + 3.3 kΩ** | 1 each | ECHO divider → **3.00 V** into GPIO26. Non-negotiable on build 1: 5 V on that pin kills the chip |
| **4.7 kΩ** | 1 | DS18B20 1-Wire pull-up |
| **10 kΩ** | 2 | I²C pull-ups (many breakouts already have them — check before adding) |
| **1 kΩ** | 1 | S8050 base resistor for the buzzer |
| **330 Ω** | 1 | heartbeat LED (GPIO18) |
| **1 MΩ ×2** | 2 | battery divider into GPIO36 (≈1.9 µA idle) |
| **100 nF X7R** | 3 | decoupling at the ESP32, at the baro modules, at the ultrasonic ("kills ringing") |
| **100 µF** 10 V+ | 1 | across the ultrasonic module's supply |
| **470 µF** 10 V+ | 1 | 5 V bulk — keeps the boost converter out of current limit during WiFi TX bursts |
| **S8050** (or BC547/2N2222) NPN | 1+2 spares | buzzer driver | [search: bc547 to-92](https://www.amazon.in/s?k=bc547+transistor+to-92) *typical ₹10–20 each* |
| Active buzzer 5 V, or piezo + driver | 1 | overflow alarm — the sketch drives it with `tone()` from `buzzerService()`: 250 ms every 9 s | [search](https://www.amazon.in/s?k=active+buzzer+5v+module) *typical ₹30–80* |
| **0.96" SSD1306 I²C OLED, 128×64** (optional, mounted on the box) | 1 | the level where the tank actually is. 4 wires, shares I²C with the baro chips, **no library and no new GPIO**. Cheapest option in the whole project and the one the phone app does not replace | [search](https://www.amazon.in/s?k=0.96+oled+i2c+ssd1306+128x64) · price **NOT verified this session** — do not order on this line, look it up. Panel warnings (sunlight, heat, burn-in) in [`05-blynk-and-display.md`](05-blynk-and-display.md) §2 |
| 1N4148 / **SS34** Schottky | 2 + 2 | flywheel/anti-backfeed; SS34 on the battery/boost path | [search](https://www.amazon.in/s?k=ss34+schottky+diode) *typical ₹10 each* |
| Protoboard 7×9 cm or 830-hole breadboard | 1 | phase-0 assembly | [search](https://www.amazon.in/s?k=perfboard+prototype+pcb) *typical ₹80–150* |
| Male-female jumper wires, 120-pc | 1 | phase-0 assembly | [search](https://www.amazon.in/s?k=jumper+wires+kit) *typical ₹120–200* |
| 4-core cable, **3–5 m** (twist T+E for the ultrasonic; 3-core for the DS18B20 drop) | 1 roll | the runs that decide whether the readings are noisy | IndiaMART / local electrical shop *typical ₹15–30/m* |

## 3. Phase 1 power (skip if you are only running phase 0 on USB)

| part | spec | qty | price | link |
|---|---|---|---|---|
| ✅ **CN3791 MPPT solar charge module**, 1S li-ion | takes panel input well above 6 V — this is the part `docs/00-design.md:117` picks; **TP4056 is the wrong part** for a 6 V panel | 1 | **₹459** (M.R.P. ₹919) | [amazon.in B0GP5TFBP5](https://www.amazon.in/Charger-Lithium-ion-Battery-Charging-Controller/dp/B0GP5TFBP5) |
| ✅ Solar panel **5 W / 6 V** | load is 3.7 V × 22 mA ≈ 0.081 W ≈ **2 Wh/day**; a 5 W panel ≈ 10× margin over 3.5 peak-sun-hours at 70 % charge efficiency | 1 | **₹1,819** (M.R.P. ₹2,599) | [WAAREE 5 W 6 V, amazon.in](https://www.amazon.in/WAAREE-Solar-Panel-Pack-12V/dp/B0DB8CMB1R) |
| ✅ **MT3608** boost module | 18650 (3.0–4.2 V) → 5 V for the DevKit's VIN and the ultrasonic's 4.5–5.5 V. TPS61023 is the low-IQ choice for the phase-1 PCB; MT3608 is the honest bench part | 2 | **₹99** each | [iCSTORE, amazon.in](https://www.amazon.in/Adjustable-converter-Regulator-Transformer-Controller/dp/B08LTLV74H) |
| ✅ **18650 cells**, named marking, flat-top: Samsung INR18650-30Q / Molicel P26A / Lishen | 2 × parallel = 6 Ah → 11–12 days at 20–25 mA always-on WiFi | 2 (+2) | ₹350–700 *typical* | [Flipkart: Samsung INR18650-30Q 3000 mAh](https://www.flipkart.com/microusb-samsung-inr18650-30q-3000mah-3-7v-rechargeable-lithium-battery/p/itm5944aa50d62a5) · [search](https://www.amazon.in/18650-battery-3000mah/s?k=18650+battery+3000mah) |
| ✅ 18650 single-cell holder with leads | 2 holders paralleled (1 S, no balancer) | 2 | **₹122** each | [CentIoT, amazon.in](https://www.amazon.in/CentIoT-Lithium-Battery-Plastic-Retention/dp/B08DMQVRV8) |
| ➖ TP4056 + protection, USB-C | bench charging only, while you wait for the CN3791 | 1 | ₹80–150 *typical* | [amazon.in B0GJS6QT6Q](https://www.amazon.in/Lithium-Battery-Charging-Protection-Charger/dp/B0GJS6QT6Q) |
| ✅ Fuse holder + **1 A** slow-blow, and a 1 A polyfuse | one boost module shorting is how these boxes burn | 1 set | ₹40–80 *typical* | [search](https://www.amazon.in/s?k=inline+fuse+holder+12v) |

## 4. Mechanical — the part people forget until the week it goes on the tank

| part | why | price | link / where |
|---|---|---|---|
| Enclosure, IP65, **≥200×150×100 mm** | two compartments: electronics up top, cell + charger in the shaded lower half | **₹170** for 150×110×70 (seen in results, page not verified) · the 200×150×100 uxcell is **₹7,090** — skip it | [search: ip65 junction box](https://www.amazon.in/ip65-junction-box/s?k=ip65+junction+box) · IndiaMART is cheaper for the big box |
| **Cable glands** M16/M20, IP68 ×4–6 | every penetrations is a dew path into the box | **₹4,977** for a 20-pack on Amazon ([B0BZ3N2Z8R](https://www.amazon.in/Waterproof-Nylon-Cable-Glands-Suitable/dp/B0BZ3N2Z8R)) — imported nonsense. **₹10–25 each at any electrical shop** | local hardware / IndiaMART |
| **Neodymium N45 discs 20×5 mm ×4** | holds the box to the steel lid; keep them ≥80 mm from the antenna | ₹200–500 *typical* | [search: 20mm neodymium magnet](https://www.amazon.in/neodymium-magnets-20mm/s?k=neodymium+magnets+20mm) |
| **Silicone/PVC tube, 4–6 mm ID, 3 m** | **build 2 only**: vented air column from the box down to the floor. Real drop is ≈1.5 m (interior 137.1 cm + the box sits above the lid), so buy 3 m for routing and service slack — see §7 | Amazon has only imports: uxcell 3 m **₹1,320** ([B01ESBKN9E](https://www.amazon.in/Generic-Silicone-Vacuum-Translucent-Water/dp/B01ESBKN9E)) — **aquarium/fish shop: ₹20–50/m** | local pet shop, IndiaMART |
| Hydrophobic **PTFE vent membrane** | vents the tube to atmosphere without letting dust/insects in | 50 mm × 50 sheets **₹5,658** ([Deschem](https://www.amazon.in/Deschem-Hydrophobic-Membrane-Filter-Polytetrafluoroethylene/dp/B07TDWVC24)) — 50 sheets you will never use | alternative: 0.2–0.45 µm syringe filter from a lab supplier, or a downward tube loop + foam sleeve |
| **1 mm mesh** over the chimney top | the design record already predicts wasps nesting in the chimney (`docs/00-design.md:81`) | ₹50–150 | hardware shop |
| Silica gel desiccant, reusable 250–500 g | the box has a 60 mm hole straight down into a water tank; this is the dew defence | ₹150–350 *typical* | [search](https://www.amazon.in/s?k=silica+gel+desiccant+reusable) |
| Chimney + louvre shade, **3D print** (PETG or ASA — the box hits 60 °C in sun; PLA creases) | puts the face 52.2 cm above the full line (`docs/img/02-mounting.svg`) | ₹200–500 at a local print service | IndiaMART / local "3D printing services near me" |
| Silicone sealant + isopropyl | gland sealing, and cleaning the lid before the magnets go on | ₹150 | hardware shop |

## 5. What Amazon India does not carry

For the phase-1/2 PCB you need the bare ICs, not modules. Order those from **LCSC** (ships to India,
DHL, customs on you) or **Digi-Key India / Mouser India**, or from the Kolkata/DEL IC traders:

- `TPS61023` boost (18 µA Iq) — the design's chosen part, MT3608 is a bench substitute only
- `TPS22919` high-side load switch — gates the 15 mA ultrasonic rail between reads; without it the
  "deep sleep is 8 mA not 20 µA" bug arrives through the I²C pull-ups
- bare `BMP280` / `BME280` / `DS18B20`, `ESP32-WROOM-32` module (phase 2b only)
- Indian module sellers worth checking when Amazon is out of stock: **robu.in, robojam.in,
  thinkrobotics.com, evibits.com, electropeak.com, sourceready.in** — their pages block automated
  fetching, so I could not price them today; they routinely carry JSN-SR04T / BME280 / CN3791 cheaper
  than the Amazon import listings above.

## 6. Ballpark totals

| what | ₹ |
|---|---|
| Phase 0, **build 2** (default): BMP280 + BME280 + DS18B20 + passives + box + tube + glands | **≈ ₹1,800–2,300** |
| Phase 0, **build 1**: JSN-SR04T + DS18B20 + passives + box + print + magnets + mesh | **≈ ₹1,900–2,600** |
| Phase 0, **populate both** (the safe option until test 4 is done) | **≈ ₹2,900–3,700** |
| Phase 0, **build 3** — 0-10 PSI at ₹3,754 (±7 cm, not recommended) | **≈ ₹4,300–4,800** |
| Phase 0, **build 3** — 0-2 m 4-20 mA transmitter + 125 Ω + 24 V boost (±2 cm) | **≈ ₹10,000–11,500** |
| Phase 0, **float reed ladder** (8 floats, stilling well, 15 cm steps) | **≈ ₹1,500–2,000** |
| Phase 1 power: CN3791 + 5 W panel + 2 cells + 2 holders + 2× MT3608 + fuse + caps | **≈ ₹3,300–4,300** |
| Everything above | **≈ ₹6,200–8,000** |

## 7. Two numbers to measure before you order

1. **Lid → router distance and what is in between** (test plan Q2). It decides whether the antenna
   stays inside a closed plastic box or whether the box goes in the clear with a longer cable run.
2. **Interior height to the water and the tube drop.** Interior is 137.1 cm (121.9 water + 15.2 gap);
   the tube has to reach the floor with slack, so ~1.5 m minimum, 3 m to be able to route and service it.

## 8. Gotchas that change what you buy

1. **Build 2 needs a BMP280 on the tube side, not a second BME280.** The 121.9 cm column adds ≈120 hPa,
   so the tube sensor sees ≈1133 hPa — past the BME280's 1100 hPa ceiling, inside the BMP280's 1200 hPa
   range (`BME_ADDR_A`/`BME_ADDR_B` in the sketch, and `statusText()` raises `TUBE OVER RANGE` above
   1200 hPa). `README.md` and `docs/00-design.md` §3 now state this correctly; if you have older notes
   saying "dual BME280", cross them out — it is the one part number in this project that is not
   interchangeable.
2. **18650s from Amazon/Flipkart are frequently mislabelled.** Buy a cell with a printed marking
   (INR18650-30Q, P26A, LR18650…) and a seller you can name; unmarked pink/green cells are not 3000 mAh.
3. **"12 V" on the CN3791 module is not a problem** with a 6 V panel: a 6 V module's open-circuit
   voltage is ≈9 V, comfortably above 4.2 V + headroom. Check the listing's input range before paying.
4. **JSN-SR04T blind zone is 20–25 cm**, and the design's gate is 35 cm (`BLIND_ZONE_CM`) — if the tank
   ever runs nearly full, the reading is rejected rather than wrong, which is the behaviour you want.
5. **The display is the part that fails visually, not electrically.** An SSD1306 in a roof-space box is
   hard to read in direct sun, its contrast drifts with temperature, and a static image burns in. It costs
   almost nothing (₹ price above, unverified), adds 3–8 mA while lit, and the sketch blanks it after 120 s
   for exactly that reason. If the tank sits in full sun, budget for the panel anyway and plan to read the
   phone instead — the app is the display that works at noon.
6. **The phone app costs ₹0 and has a quota, not a price.** Blynk's free plan is 100 000 device
   messages/month/device, and a 60 s sample rate spends about half of that. Nothing to buy, but it decides
   the deployment sample interval — see [`05-blynk-and-display.md`](05-blynk-and-display.md) §5.5.
7. **Magnets only work if the lid is steel.** The design assumes it. If the lid is concrete or GRP, the
   mount changes to weights/drill-and-bolt, and that changes the mechanical order.
8. **Flat-top vs button-top cells** must match the holder you buy.

## 9. Do not order yet

`docs/02-phase0-test-plan.md` Q1 (which sensor) is still unanswered and the sketch disagrees with the
README about the default. If you answer it, I'll cut this list down to one build and put the LCSC part
numbers next to the ICs for the KiCad BOM (`docs/00-design.md:130`).

---

## 10. Build 3 — submersible hydrostatic probe (added 2026-11-16 after the site survey)

The installation is a **house overhead tank: concrete body, metal lid, unventable, everything inside it
is wet**. That kills build 1 (dew on the ultrasonic face, `docs/02-phase0-test-plan.md:57`) and makes
build 2's dry-tube rule a standing maintenance duty. Build 3 puts only a potted probe + vented cable in
the water, which is what industrial overhead-tank level measurement actually uses.

### 10.1 Verified Amazon.in stock (2026-11-16)

| part | range | output | supply | **1 % FS = level error** | price | link |
|---|---|---|---|---|---|---|
| **Walfront G1/4 transducer**, 0-10 PSI | 0.69 bar ≈ **7.0 m water** | **0.5–4.5 V** ratiometric | 5 V | **±7.0 cm** | **₹3,754** (MRP ₹7,202), 4.2★ (42) | [B07KJHRPLG](https://www.amazon.in/Pressure-Transducer-0-5-4-5V-Transmitter-0-10PSI/dp/B07KJHRPLG) |
| **Oumefar G1/4 "submersible level transmitter"**, 0-10 PSI | 7.0 m | 0.5–4.5 V | 5 V | **±7.0 cm** | **₹3,382–3,391** (MRP ₹4,499), 1.0★ (1) | [B08JZ8MCR2](https://www.amazon.in/Stainless-Pressure-Transducer-Submersible-Transmitter/dp/B08JZ8MCR2) |
| **ASHATA throw-in**, 0-5 m | 5 m | **4–20 mA** | 24 V | ±5.0 cm | **₹5,587–5,597** (MRP ₹7,195) | [B07XPN9WNL](https://www.amazon.in/Water-Level-Sensor-Throw-Transmitter/dp/B07XPN9WNL) |
| **Tbest TL-136**, 0-1 m / **0-2 m** | 1 m / 2 m | **4–20 mA** | 24 V | **±1.0 / ±2.0 cm** | **₹7,789** / **₹9,685** | [0-1 m: B07WDK2PRN… same family](https://www.amazon.in/Submersible-Pressure-Sensors-Transmitter-Detector/dp/B0C448MLYY) |
| LOOM waterproof submersible | see listing | 4–20 mA | 24 V | n/a | **₹4,691** | [B0DKF4DXQV](https://www.amazon.in/LOOM-Waterproof-Submersible-Transducer-Transmitter/dp/B0DKF4DXQV) |
| Ved Automation (IndiaMART), 0-100 m, SS316 | 100 m | 4–20 mA | 24 V | ±100 cm — absurd for a 1.22 m tank | **₹10,000** | [IndiaMART 2860114221433](https://www.indiamart.com/proddetail/submersible-hydrostatic-level-transmitter-2860114221433.html) |

Common to all: G1/4 BSP thread, stainless body, 3-wire (red = V+, black = 0 V, green/white = signal),
1 % FS linearity, cable **not** extended by the vendor — buy 2–3 m of shielded 3-core separately.

### 10.2 The number that decides it: range vs your 121.9 cm column

121.9 cm of water = **0.119 bar = 1.73 PSI**. Amazon India's cheapest transducers start at **0-10 PSI
(7 m)** — nine times more span than the tank has. Accuracy is quoted as % of full scale, so:

| option | sensor cost | 1 % FS error | notes |
|---|---|---|---|
| **build 2: BMP280 tube + BME280 box** | **₹937** | **±1–2 cm** (BMP280 ±1 hPa ≈ 1.03 cm water) | 4× cheaper and 4× more accurate than any ₹3.4k transmitter; costs you the dry-tube duty |
| build 3, ₹3.4–3.8k, 0-10 PSI | ₹3,754 | **±7 cm** | right interface, wrong range for a 4 ft tank |
| build 3, ₹9.7k, 0-2 m, 4-20 mA | ₹9,685 | ±2 cm | needs a 24 V rail |
| float reed ladder, 8 × ₹130 | ~₹1,000 | 15 cm steps (= 1,394 L/step) | zero calibration, zero electronics in water |

**Conclusion for this site: build 2 is the accuracy-and-cost winner (₹937, ±1–2 cm); build 3 is what you
buy when you refuse to own a tube at all.** A 0-10 PSI part at ₹3,754 is a downgrade in reading quality,
and a part that reads 100 cm when the tank is at 107 cm is a gauge you will stop trusting in a month.
The correct build-3 purchase is a **0–2 m (0.2 bar) level transmitter**, which in India means the 4-20 mA
₹8–10k class or an IndiaMART quote — I could not verify anything below ₹3,382 on Amazon.in, and
robu.in / flyrobo / electropeak / rees52 all blocked automated access (403 / 404 / login-wall / 429).

### 10.3 Interface, per output type

**0.5–4.5 V ratiometric (the ₹3.4–3.8k parts)** — power it from the **5 V rail**, then divide into
GPIO33 (`PIN_LVL_ANA`, ADC1_CH5 — WiFi-safe, per the `ADC1_CH5` note on the `PIN_LVL_ANA` define itself):

- **2.2 kΩ top / 4.7 kΩ bottom** (ratio 0.681): 0.5 V → 0.34 V, 4.5 V → **3.07 V** at the pin. Same
  divider the battery measurement already uses, and it keeps the pin under the ≈3.1 V full-scale of the
  3100 mV attenuation (1 count ≈ 0.805 mV → 0.17 cm/count).
- 1 kΩ series + **100 nF** across the bottom resistor (RC corner 1.6 kHz — kills WiFi TX noise), 100 nF at the pin.
- Ratiometric means the reading tracks supply error: if the 5 V rail sags 2 %, the level reads 2 % low.
  Power the transmitter from the same rail the ADC references, or read `vbat` and scale.

**4–20 mA (the ₹5.6–9.7k parts)** — do **not** use 250 Ω: 20 mA × 250 Ω = 5.0 V, which divides to 3.4 V
and clips. Use **125 Ω (0.1 % — two 249 Ω in parallel)** → 0.5–2.5 V straight into GPIO33, no divider,
and the extra ~0.5 V of headroom makes the 4-20 mA loop the more robust signal over a 3 m cable.
Needs a 24 V rail: XL6009/MT3608 boost from the battery (₹99) or a mains 24 V wall wart.
Burn: 20 mA × 24 V = 0.5 W continuous — four times the whole ESP32 budget, so build 3 on battery is not
a low-power option, and it is the reason to use `i 300` and expect USB power.

**Probe mounting, whichever you buy:** sit the probe **50–100 mm off the floor** on a small stand or inside
a 100 mm UPVC cup open at the bottom, so silt and the tank's annual sludge do not sit on the diaphragm;
set that standoff as the datum (`m <cm>` full-scale + `z` zero). The **vent hole in the cable jacket** must
stay dry, pointing down, inside the box — if water gets into that capillary the reading is dead, same
failure as build 2's wet tube.

### 10.4 Firmware status

`SENSOR_TYPE` supports 1 and 2 only. The sketch declares `PIN_LVL_ANA` (GPIO33) as an "analog-output
sensor option" and **nothing reads it** — `analogReadMilliVolts()` appears once, for the battery.
Build 3 needs ~40 lines: median-of-8 + IIR into the existing `Rd` struct, a validity window
(`sampleValid()` already bounds to `maxLevelCm + 15`), and two calibration commands
(zero at a known depth, span at a second), reusing `m`/`z`/`o` semantics. Everything downstream —
`pct`, `litres`, `level_cm`, the web page, `/api/state`, the CSV, the buzzer, the OLED and the phone
push — is already sensor-agnostic and needs no change: the alarm state machine and the display read
`level_cm`/`statusText()`, never the sensor. Build 3 is a sensor driver, not a re-do of the box.
