# 04 — Parts list, with Indian links (rev B, SR04M ultrasonic)

Prices are **₹, tax included, as pulled on 2026-11-16** unless marked *typical* (a budget
number I could not verify) or **unverified** (a part this rev needs that I have not priced
yet). Stock and prices on Amazon India move weekly — re-check before ordering.

> **Function names, not line numbers.** The `.ino:NNN` references in the old version of this
> file broke the moment the display and app code landed, so they are gone. The named function
> is quoted instead — `pingRaw()`, `filterUpdate()`, `statusText()`, `buzzerService()` — and
> that stays true when the sketch changes shape.

> **One build.** rev A offered three sensor technologies (`SENSOR_TYPE` 1/2/3, build 3 added
> later). That switch is gone: this project is the ultrasonic build, so order one list. What
> changed and what was retired is in §9, and the reasoning is in
> [00-design.md](00-design.md) and [06-changelog.md](06-changelog.md).

Legend: ✅ buy it · ♻ probably already own it · ➖ optional / bench-only

---

## 1. The sensor

| part | what it is | qty | price | link |
|---|---|---|---|---|
| ✅ **SR04M / SR04M-2 waterproof ultrasonic module** — the part in the reference project. Same family as **AJ-SR04M** and **JSN-SR04M-2**; separate potted probe on a cable, electronics board in the dry box | the gauge. Non-contact, so nothing sits in drinking water | 1 + **1 spare** | **unverified** — not priced this session. Amazon.in carries it under several seller names; expect roughly ₹900–1,800 | [search: sr04m ultrasonic sensor](https://www.amazon.in/s?k=sr04m+ultrasonic+sensor) · also [robu.in](https://robu.in), [robojam.in](https://robojam.in), [thinkrobotics.com](https://www.thinkrobotics.com), IndiaMART |
| ♻ **JSN-SR04T** (integrated, transducer on the end of the module — no separate probe) | the price data point I *did* verify, same blind zone and protocol, ₹899 from an India-listed seller. Perfectly usable if it is what you can buy; the split-probe SR04M is nicer on a lid because only the small potted probe crosses the hole | 0–1 | **₹899** (verified 2026-11-16) | [BITSKY INDIA, amazon.in](https://www.amazon.in/BITSKY-INDIA-Waterproof-Ultrasonic-Transducer/dp/B0FFNFVJGV) |
| ➖ **HC-SR04** (bare, not waterproof) | **bench rehearsal only.** Mode 1 of the SR04M *is* the HC-SR04 protocol, so the same pins, the same 2 kΩ/3.3 kΩ echo divider and the same `e`/`f` calibration apply with zero code change. Rehearse tests T2–T8 for ₹200 before the real module arrives. Never on the tank: unsealed PCB, open cones, exposed pads | 0–1 | **₹200** (*seen on the search page, 4.1★, 200+ bought/month — treat as approximate*) | [search: hc-sr04](https://www.amazon.in/hc-sr04/s?k=hc-sr04) · [roboway.in](https://roboway.in/shop/ultrasonic-distance-sensor-hc-sr04/) |

**When the module arrives, check three things before it goes on the tank:**

1. **Pad `R19` is unpopulated.** That resistor sets the operating mode; open = mode 1, the
   HC-SR04-compatible trigger/echo this firmware speaks. Something fitted there and the module
   answers on its TX line instead — the failure looks like a dead sensor ([03-ultrasonic.md §2](03-ultrasonic.md)).
2. **Probe cable length.** Face must stand 150 mm above the lid and reach the electronics box.
   Measure, and confirm the probe can be raised for cleaning without disconnecting the box.
3. **Blind area is 20 cm and max range is 6 m (JSN-SR04M-2) / 8 m (AJ-SR04M).** Both are far
   outside this tank's needs (30.2 cm to 152.1 cm), but if a listing claims 2 m you have bought
   a different part.

---

## 2. Board and display

| part | spec | qty | price | link |
|---|---|---|---|---|
| ♻ **ESP32-WROOM-32 DevKitC V1**, 38-pin, CP2102 | the board everything is built around; buy **DOIT ESP32 DEVKIT V1** in the IDE. One spare removes the "did I magic-smoke it?" variable | 0–1 | ₹509–699 *typical* | [search: esp32 devkit v1](https://www.amazon.in/s?k=esp32+devkit+v1) |
| ✅ **0.96" SSD1306 I²C OLED, 128×64**, 4-pin (VCC GND SDA SCL) | the level where the tank actually is. 2 wires (GPIO21/22), no library — the driver is in `wlg_display.h`. Cheapest useful part in the project, and the phone app does not replace it | 1 | **unverified** — do not order on this line, look it up; ₹250–450 is the usual range for this panel in India | [search](https://www.amazon.in/s?k=0.96+oled+i2c+ssd1306+128x64). Sunlight, heat and burn-in notes: [05-blynk-and-display.md §2](05-blynk-and-display.md) |
| ✅ Tactile push button, 6 mm, **or** a panel-mounted switch | short press = wake + snooze, hold = mute. The reference build's "push button to stop the buzzer" | 1–2 | ₹10–30 *typical* | [search](https://www.amazon.in/s?k=tactile+switch+6mm) |

---

## 3. Passives and bench parts (₹300–500 total)

One **1/4 W metal-film 1 % assortment** covers every value the schematic uses.
[Aniann 1280-pc kit, amazon.in](https://www.amazon.in/Aniann-Resistor-Resistors-Assortment-Experiments/dp/B07L851T3V) · *typical ₹400–700* · [search](https://www.amazon.in/s?k=metal+film+resistor+assortment+1%25)

| value | qty | what it does |
|---|---|---|
| **2 kΩ + 3.3 kΩ** | 2 each | **ECHO divider → 3.00 V into GPIO26.** Non-negotiable when the module runs at 5 V: 5 V on that pin kills the chip. (Module on 3V3 instead needs no divider — [01-wiring §2](01-wiring-phase0.md)) |
| **220 Ω** | 2 | buzzer transistor base, and the green LED |
| **1 MΩ ×2** | 2 | battery/5 V-rail divider into GPIO36 (≈1.9 µA idle) |
| **100 nF X7R** | 3 | decoupling: at the ESP32, at the ultrasonic module, at the OLED. The module's 8-cycle 40 kHz burst is a current pulse — without local capacitance the rail dips and the board reboots mid-ping |
| **100 µF** 10 V+ | 1 | directly across the ultrasonic module's VCC/GND |
| **470 µF** 10 V+ | 1 | 5 V bulk — keeps the boost converter out of current limit during WiFi TX bursts |
| **S8050** (or BC547 / 2N2222) NPN | 1 + 2 spares | buzzer driver | *typical ₹10–20 each* · [search: bc547 to-92](https://www.amazon.in/s?k=bc547+transistor+to-92) |
| **1N4148** | 1 | flyback across the piezo buzzer | *typical ₹5–10* |
| Active piezo buzzer, 5 V, 8–12 mm | 1 | the **local** half of the alarm — beeps with no internet at all. Driven by `tone()` from `buzzerService()`: 250 ms every 9 s, 1.8 kHz low / 2.6 kHz high and fault | *typical ₹30–80* · [search](https://www.amazon.in/s?k=active+buzzer+5v+module) |
| Green LED, 5 mm | 1 | heartbeat, **solid on when the tank is full** — the reference build's green "tank full" indicator | *typical ₹5* |
| Protoboard 7×9 cm or 830-hole breadboard | 1 | phase-0 assembly | *typical ₹80–150* |
| Male-female jumpers, 120-pc | 1 | phase-0 assembly | *typical ₹120–200* |
| 4-core cable, 3–5 m | 1 roll | twist TRIG+ECHO as a pair; this is the run that decides whether readings are noisy | local electrical shop *typical ₹15–30/m* |

---

## 4. Power (skip if phase 0 runs from USB only)

| part | spec | qty | price | link |
|---|---|---|---|---|
| ✅ **CN3791 MPPT solar charge module**, 1S Li-ion | takes panel input well above 6 V — the part [00-design.md §7](00-design.md) picks. **TP4056 is the wrong part** for a 6 V panel | 1 | **₹459** (M.R.P. ₹919) | [amazon.in B0GP5TFBP5](https://www.amazon.in/Charger-Lithium-ion-Battery-Charging-Controller/dp/B0GP5TFBP5) |
| ✅ Solar panel **5 W / 6 V** | average load is a few tens of mA; a 5 W panel is roughly 10× the margin over 3.5 peak-sun-hours at 70 % charge efficiency | 1 | **₹1,819** (M.R.P. ₹2,599) | [WAAREE 5 W 6 V, amazon.in](https://www.amazon.in/WAAREE-Solar-Panel-Pack-12V/dp/B0DB8CMB1R) |
| ✅ **MT3608** boost module | 18650 (3.0–4.2 V) → 5 V for the DevKit's VIN **and** the module's 4.5–5.5 V supply. TPS61023 is the low-Iq choice for the phase-1 PCB; MT3608 is the honest bench part | 2 | **₹99** each | [iCSTORE, amazon.in](https://www.amazon.in/Adjustable-converter-Regulator-Transformer-Controller/dp/B08LTLV74H) |
| ✅ **18650 cell**, printed marking: Samsung INR18650-30Q / Molicel P26A / Lishen | 2 × parallel = 6 Ah → 11–12 days at 20–25 mA always-on WiFi | 2 (+2) | ₹350–700 *typical* | [Flipkart: INR18650-30Q 3000 mAh](https://www.flipkart.com/microusb-samsung-inr18650-30q-3000mah-3-7v-rechargeable-lithium-battery/p/itm5944aa50d62a5) · [search](https://www.amazon.in/18650-battery-3000mah/s?k=18650+battery+3000mah) |
| ✅ 18650 single-cell holder with leads | 2 holders paralleled (1 S, no balancer) | 2 | **₹122** each | [CentIoT, amazon.in](https://www.amazon.in/CentIoT-Lithium-Battery-Plastic-Retention/dp/B08DMQVRV8) |
| ➖ TP4056 + protection, USB-C | bench charging only, while the CN3791 is on order | 1 | ₹80–150 *typical* | [amazon.in B0GJS6QT6Q](https://www.amazon.in/Lithium-Battery-Charging-Protection-Charger/dp/B0GJS6QT6Q) |
| ✅ Fuse holder + **1 A** slow-blow, plus a 1 A polyfuse | a boost module shorting is how these boxes burn | 1 set | ₹40–80 *typical* | [search](https://www.amazon.in/s?k=inline+fuse+holder+12v) |

---

## 5. Mechanical — the part people forget until the week it goes on the tank

| part | why | price | where |
|---|---|---|---|
| ✅ **100 mm hole saw / core cutter** | the beam is 46 cm across at the full line and the lid clutter echoes must stay inside the 20 cm blind zone, which needs a real 100 mm hole, not a 25 mm gland grommet | ₹250–700 (bi-metal hole saw) or a diamond core bit at a tool-hire shop | hardware shop / IndiaMART |
| ✅ **Stub pipe for the probe standoff**, PVC/ABS 75–110 mm OD, ~200 mm long | the probe face must stand **150 mm above the lid underside**. This is the part that buys the blind-zone headroom; there is no chimney in rev B and none should be rebuilt | ₹60–150 | plumbing shop |
| ✅ Cable gland, M16/M20, IP68 ×2–4 | probe cable entry and the sensor-to-box run. Every penetration is a dew path into the box | **₹10–25 each at any electrical shop.** The Amazon 20-pack at ₹4,977 ([B0BZ3N2Z8R](https://www.amazon.in/Waterproof-Nylon-Cable-Glands-Suitable/dp/B0BZ3N2Z8R)) is imported nonsense | local hardware |
| ⚠ **NOT a screen or mesh over the aperture** | 40 kHz will not pass a flyscreen — it blocks the ping and the module reports `no echo`. Keep insects out with a cowl over the hole or a foam gasket around the probe body, never across the beam | — | — |
| ✅ Cowl / weather hood over the hole | shades the probe (a hot face means condensation on it later) and keeps birds, wasps and leaves out | 3D-print PETG or ASA — the box hits 60 °C in sun, PLA creases. ₹200–500 at a local print service | IndiaMART / "3D printing services near me" |
| Enclosure, IP65, **≥200×150×100 mm** | two compartments: electronics up top, cell + charger shaded below | ₹170 for 150×110×70 (seen in results, page not verified); the 200×150×100 uxcell at ₹7,090 is silly — use IndiaMART | [search: ip65 junction box](https://www.amazon.in/ip65-junction-box/s?k=ip65+junction+box) |
| **Neodymium N45 discs 20×5 mm ×4** | holds the box to a steel lid; keep them ≥80 mm from the PCB antenna | ₹200–500 *typical* | [search: 20mm neodymium magnet](https://www.amazon.in/neodymium-magnets-20mm/s?k=neodymium+magnets+20mm) |
| Silica gel desiccant, reusable 250–500 g | a hole straight down into a water tank means the box lives in a humid environment; dew on the **probe face** is the single most common cause of rejected pings | ₹150–350 *typical* | [search](https://www.amazon.in/s?k=silica+gel+desiccant+reusable) |
| Silicone sealant + isopropyl alcohol | gland sealing, cleaning the lid before magnets go on, and **cleaning the probe face** at each inspection (dust and cobwebs bias the distance near) | ₹150 | hardware shop |
| Isopropyl + lint-free wipe | the face is the optical element here; wipe it, do not paint it, do not coat it | ₹100 | — |

---

## 6. What Amazon India does not carry

For the phase-1 PCB you need bare parts, not modules — LCSC (ships to India, customs on you),
Digi-Key/Mouser India, or the Kolkata/DEL IC traders:

- `TPS61023` boost (18 µA Iq) — the design's chosen regulator; MT3608 is a bench substitute
- `TPS22919` high-side load switch — gates the module rail between reads, which is what makes a
  low-power rev possible later (mode 2/4 of the module, 20–40 µA standby)
- bare `ESP32-WROOM-32` module (phase 2b only)
- Indian module sellers worth checking when Amazon is out of stock: **robu.in, robojam.in,
  thinkrobotics.com, evibits.com, electropeak.com, sourceready.in** — their pages block
  automated fetching so they could not be priced this session; they routinely carry
  SR04M/JSN-SR04 parts cheaper than the Amazon import listings.

---

## 7. Ballpark totals

| what | ₹ |
|---|---|
| Phase 0 bench rehearsal: HC-SR04 + OLED + passives + breadboard + jumpers | **≈ ₹700–1,100** |
| Phase 0 real build: SR04M-family module + spare + OLED + passives + box + hole saw + stub pipe + glands + cowl + silica gel + magnets | **≈ ₹2,600–3,800** (₹900–1,800 of it is the module, unverified) |
| Phase 1 power: CN3791 + 5 W panel + 2 cells + 2 holders + 2× MT3608 + fuse + caps | **≈ ₹3,300–4,300** |
| Everything | **≈ ₹6,600–8,900** |

---

## 8. Gotchas that change what you buy

1. **`R19` decides whether the module works at all.** Order knowing you may have to check the
   mode strap pad; a module in a serial mode does not answer a TRIG pulse.
2. **Blind area is 20 cm, and the firmware gate is 25 cm.** The full line must therefore sit more
   than 25 cm below the probe face — the reference project says the same thing as "greater than
   25 cm". With a 15.2 cm lid gap, that forces a ~150 mm standoff above the lid, which is why the
   stub pipe is on the list and why the hole is 100 mm.
3. **The echo divider is not optional on a 5 V module supply.** Two resistors, ₹2 of stock, and
   the difference between a working gauge and a dead GPIO26.
4. **18650s from Amazon/Flipkart are frequently mislabelled.** Buy a printed marking
   (INR18650-30Q, P26A, LR18650…) from a seller you can name; unmarked pink/green cells are
   not 3000 mAh.
5. **"12 V" on the CN3791 listing is not a problem** with a 6 V panel: a 6 V module's open-circuit
   voltage is ≈9 V, comfortably above 4.2 V + headroom. Check the input range before paying.
6. **The display fails visually, not electrically.** An SSD1306 in a roof-space box is hard to
   read in direct sun, its contrast drifts with temperature and a static image burns in. It costs
   almost nothing, adds 3–8 mA while lit, and the firmware blanks it after 120 s for exactly that
   reason. Budget for it anyway and plan to read the phone at noon.
7. **The phone app costs ₹0 and has a quota, not a price.** Blynk's free plan is 100 000 device
   messages per month per device; a 60 s sample rate with the full stream set is over quota on its
   own. It decides the deployment sample interval — [05-blynk-and-display.md §5](05-blynk-and-display.md).
8. **Magnets only work if the lid is steel.** The design assumes it. Concrete or GRP lid means
   weights or drill-and-bolt, and that changes the mechanical order.
9. **Buy the probe with a cable you can service.** The face gets dew, dust and cobwebs; if the
   probe cannot be lifted out and wiped without dismounting the box, it will not get cleaned, and
   rejected pings will quietly become the norm.

---

## 9. Retired from rev A — do not order these

| part | rev A used it for | why it is gone |
|---|---|---|
| **BMP280** module | tube-side differential pressure (needed because a 121.9 cm water column adds ≈120 hPa, past a BME280's 1100 hPa ceiling) | the whole hydrostatic principle is gone; there is no tube and no pressure sensor |
| **BME280** breakout | box-side reference pressure + humidity | same, and the dew/humidity warning is gone with it |
| **DS18B20** in a steel probe | water temperature → density for litres | density is no longer derived from a pressure difference; the 1-Wire bus is gone |
| Submersible level transmitter (0–10 PSI / 0–2 m 4–20 mA), 125 Ω, 24 V boost | rev A "build 3" | retired — a part in drinking water, and the calibration was ±7 cm on the cheap variant |
| Float/reed ladder + stilling well | rev A alternative | retired |
| **Silicone/PVC tube, 3–5 m** + PTFE hydrophobic vent membrane | the vented air column of the hydrostatic build | no tube in an ultrasonic build |
| **1 mm mesh** over the chimney top | wasps nesting in the rev A chimney | ⚠ replaced by a cowl — a screen anywhere across the beam blocks 40 kHz entirely |
| **Chimney + louvre shade** (250 mm, put the face 52.2 cm above the full line) | the old sensor's ~15° cone | the SR04M's cone is 75° wide: a narrow pipe would be a resonant clutter chamber. Replaced by the 150 mm stub standoff |
| 4.7 kΩ 1-Wire pull-up, second I²C pull-up pair | DS18B20 and the dual pressure-sensor bus | those buses are gone; the OLED's pull-ups are on its own module |

**Also retired in firmware, not just in the BOM:** `SENSOR_TYPE`, `baroRaw()`, `waterDensity()`,
`sampleValid()`, the `p_ref`/`p_tube`/`p_diff` Blynk streams, the humidity and `DP hPa` OLED rows.
Datastreams **V5, V6, V7 and V16** in the Blynk template now carry nothing — delete them there
rather than renumber anything ([05-blynk-and-display.md §3](05-blynk-and-display.md)).
