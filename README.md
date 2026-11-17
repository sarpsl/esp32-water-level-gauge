# ESP32 water level gauge — 10 ft × 10 ft × 4 ft overhead tank

A read-only tank gauge: an **SR04M-family waterproof ultrasonic module** looking down at the
water from above the lid, an ESP32, a 0.96" OLED on the box, a local buzzer with a stop button,
and an optional [Blynk](https://blynk.cloud) phone dashboard with push notifications. It reports
**cm, % and litres** and it alarms when the tank is low or full.

Built around the same feature set as Tech StudyCell's
[*IoT Based Water Level Indicator using Ultrasonic Sensor*](https://iotcircuithub.com/iot-based-water-level-indicator/)
([video](https://youtu.be/9geREeE13jc)) — same module family, same GPIO numbers — with the
filtering, calibration model, failure diagnosis, battery/solar power and alarm discipline this
installation actually needs.

> **This is an instrument, not a control.** It reads and it alarms. It must never switch a pump
> or a valve. Pump/valve interlocks belong in the pump's own float circuit, and no part of this
> design is a safety device.

**rev B (ultrasonic) replaced rev A (differential-pressure/hydrostatic) completely.** No
pressure sensor, no tube, no chimney, no submersible probe, no build-switching `SENSOR_TYPE`.
The change and the reasoning are in [docs/06-changelog.md](docs/06-changelog.md).

---

## What it measures

| | |
|---|---|
| tank | 304.8 × 304.8 × 121.9 cm internal (10 ft × 10 ft × 4 ft), 92 903 cm² of area |
| water depth / span | **121.9 cm**, i.e. **92.9 L per cm**, **11 327 L (11.33 m³)** full |
| how | distance from the probe face down: 30.2 cm at the full line → 152.1 cm at an empty floor |
| accuracy | datasheet ±10 mm, plus a temperature-dependent scale error of ≈0.27 cm/°C at full scale |
| blind zone | the module cannot read closer than 20 cm; the firmware gate is 25 cm and the mount puts the full line at 30.2 cm |
| sample rate | default 60 s for bring-up, `i 900` (15 min) for deployment |

## Pin map

Adopted from the reference project so you can follow its schematic with this repo. Full wiring:
[docs/01-wiring-phase0.md](docs/01-wiring-phase0.md), picture [docs/img/01-wiring.svg](docs/img/01-wiring.svg).

| signal | GPIO | note |
|---|---|---|
| ultrasonic `TRIG` | **27** | 12 µs pulse; module needs ≥ 10 µs |
| ultrasonic `ECHO` | **26** | **must pass a 2 kΩ / 3.3 kΩ divider** → 3.00 V. The ESP32 is not 5 V tolerant |
| OLED `SDA` / `SCL` | **21 / 22** | SSD1306 at 0x3C/0x3D, no external library |
| stop / wake button | **12** | `INPUT_PULLUP`, button to GND. **Strapping pin: never hold it while the board resets** (GPIO23 is the safe alternative) |
| buzzer | **13** | 220 Ω → BC547/S8050 → 5 V piezo. Works with no internet |
| indicator LED | **14** | 220 Ω → LED. Heartbeat, **solid on when full** |
| battery / rail sense | **36** | 1 MΩ / 1 MΩ divider; GPIO36 is input-only |

Never wire anything to the pins printed **D6 D7 D8 D9 D10 D11** — those are the SPI flash lines,
broken out on the DevKitC and they look usable.

## Build and flash

```
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 arduino/water_level_gauge
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p COM6 arduino/water_level_gauge
arduino-cli monitor -p COM6 -c baudrate=115200
```

No libraries are needed to build the gauge — the OLED driver lives in `wlg_display.h`. The
optional app layer needs the `Blynk` library (`arduino-cli lib install "Blynk"`) plus a token in
the sketch; with no token it compiles and runs as a LAN-only gauge. Verified build:
**1 000 767 bytes (76 %) flash, 50 652 bytes RAM, no warnings.**

Then: `w <ssid>` · `p <password>` · reset · `e <empty distance>` · `f <full distance>` ·
`v` to check span and litres-per-cm. `h` lists every command
(`r s c w p e f a g t i v b l u j d D k x`), and the board also serves a test page at
`http://<ip>/` with JSON at `/api/state`.

## The alarm

Low at ≤ 24 cm (≈20 %), high at ≥ 110 cm (≈12 cm below the overflow), both movable from the app.
Hysteresis 2 cm and a 30 min notification cooldown, so a level parked on a threshold cannot spam
the phone. Locally: a 250 ms beep every 9 s, a short button press snoozes it, holding 1.5 s latches
the mute. A sensor fault (3 consecutive unusable pings) raises the same buzzer and a `sensor_fault`
push, while the display keeps showing the last good level with a `NO ECHO` / `STALE` label rather
than inventing a zero.

## Docs

| | |
|---|---|
| [00-design.md](docs/00-design.md) | the design record: requirements, sensor spec, geometry, calibration model, pin map, power budget, Blynk mapping. The contract everything else has to agree with |
| [01-wiring-phase0.md](docs/01-wiring-phase0.md) | bench wiring, the echo divider, the two pin traps, bring-up order |
| [02-phase0-test-plan.md](docs/02-phase0-test-plan.md) | 15 tests with expected numbers and pass criteria, including the 24 h lid test |
| [03-ultrasonic.md](docs/03-ultrasonic.md) | how the SR04M works: modes and `R19`, blind zone, 75° beam arithmetic, what it cannot see, temperature error budget |
| [04-parts-list.md](docs/04-parts-list.md) | BOM with Indian links and verified prices, plus what rev A retired |
| [05-blynk-and-display.md](docs/05-blynk-and-display.md) | Blynk template, datastreams, events, quota maths, OLED reference, troubleshooting |
| [06-changelog.md](docs/06-changelog.md) | rev A → rev B: what was removed and why, and the "no remnants" checklist |
| [docs/img/](docs/img/) | wiring, mounting section, power tree, display + app (SVG + PNG) |

## Before you touch anything

1. **5 V into GPIO26 kills the pin.** The module's echo is 5 V when the module is powered at 5 V —
   the divider is two resistors and it is not optional. Powering the module from 3V3 avoids the
   divider but costs acoustic margin.
2. **Do not hold the button during reset.** GPIO12 is `MTDI`; a held button leaves the board stuck
   in the wrong boot mode until you let go.
3. **`R19` on the module must be unpopulated** — that selects mode 1, the trigger/echo interface
   this firmware speaks. A fitted resistor makes the module look dead.
4. **Nothing may screen the aperture.** A flyscreen stops 40 kHz cold. Use a cowl.
5. **The full line must sit more than 25 cm below the probe face.** That single rule sizes the
   mount: 150 mm of standoff above the lid, through a 100 mm hole.
