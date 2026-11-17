# The ultrasonic sensor — how the SR04M works, and what it cannot see

rev B. This is the note to read before you mount anything, before you set a threshold,
and before you believe a number on the OLED. It replaces the rev A note about
differential-pressure (hydrostatic) sensing, which is gone from the project.

Reference build for comparison: *IoT Based Water Level Indicator using Ultrasonic Sensor*,
Tech StudyCell — [write-up](https://iotcircuithub.com/iot-based-water-level-indicator/),
[video](https://youtu.be/9geREeE13jc). It uses the same module family and the same GPIO
numbers; this firmware adds the filtering, the gate, the calibration model and the failure
diagnosis.

---

## 1. What the module is

One part, four names depending on who is selling it: **SR04M, SR04M-2, AJ-SR04M,
JSN-SR04M-2**. Buy whichever the shop has; check the two numbers that matter, max range
and probe cable length.

It is not one sensor but two assemblies on a cable:

- **the electronics board** — 42 × 29 × 12 mm, lives in the dry box, carries the 40 kHz
  driver, the receiver and the mode strap pad `R19`;
- **the transducer probe** — epoxy-encapsulated, weatherproof, on a fixed cable. This is
  the bit that hangs over the water. Because the sensitive parts are potted and the
  electronics stay dry, unlike an HC-SR04, it survives a drinking-water tank: no
  condensation inside the sensor face, no algae in a vent, no corrosion of exposed pins.

| spec | value | source |
|---|---|---|
| supply | 3.0 – 5.5 VDC | datasheet |
| working current | < 8 mA (JSN-SR04M-2); ≈ 2.5 mA per ping, mode 1 (AJ-SR04M) | datasheet |
| **blind area** | **20 cm** — no reading closer than this, ever | datasheet |
| max range | 6 m (JSN-SR04M-2) / 8 m (AJ-SR04M table) | datasheet |
| accuracy | ±10 mm from 1 m to 6 m, 1 mm resolution, ≈ 1 % repeatability | datasheet |
| beam | **75° cone** | datasheet |
| burst | 40 kHz, 8 cycles | datasheet |
| trigger | TTL HIGH, **≥ 10 µs** | datasheet |
| no-echo behaviour | ECHO drops by itself after **40 ms** | datasheet |
| cycle time | wait > 50 ms between triggers | datasheet |
| arithmetic | `cm = µs / 57.5` | datasheet |
| target | smooth, **≥ 0.5 m²** | datasheet |
| temperature | −20 … +70 °C | datasheet |

---

## 2. Protocol: five modes, we use one

`R19` on the electronics board picks the mode by the resistor fitted there:

| mode | R19 | what comes out | standby |
|---|---|---|---|
| **1 — pulse width** | **open (nothing fitted)** | **HC-SR04/HR-04 compatible: TRIG in, ECHO high-time out** | 2.5 mA |
| 2 — low-power pulse | 300 kΩ | same echo pulse, but you must trigger it | 40 µA |
| 3 — auto serial | 120 kΩ | free-running 9600 baud distance frames | 2.5 mA |
| 4 — serial trigger | 47 kΩ | send `0x01`, get 4 bytes back | 20 µA |
| 5 — ASCII | 0 Ω | prints the distance in text | 20 µA |

**This project uses mode 1 only, and that means `R19` must be left unpopulated.**
Check it when the module arrives — some boards ship with something fitted, and the
symptom is confusing: the module answers on its TX line instead of giving you an echo
pulse, and `r` on the serial console reports `no echo` forever.

Modes 2 and 4 are the low-power route for a later battery build (wake the module, take one
reading, sleep). Not implemented here, and deliberately: modes 3–5 change what the cable
carries, and the whole point of rev B is one boring, debuggable interface.

Mode 1 timing, exactly as the firmware drives it:

```
TRIG  ──────┐ ┌──────┐_____________________________________
            └─┘      │  12 µs (datasheet wants ≥ 10 µs)
                     └── fires 8 bursts of 40 kHz ──► sound leaves the face

ECHO  __________________┌───────────────────────────┐______  ← high for the
                        │◄────── round trip ───────►│        round-trip time
                        0                       40 ms max:
                                                no echo → ECHO drops, read rejected
             │◄──── MIN_PING_GAP_MS = 60 ms between pings (datasheet: > 50 ms) ────►│

          distance_cm = high_us × speed_of_sound / 2
                      = high_us / 57.5          at 25 °C, the datasheet's own form
```

Two details worth knowing, both of which the firmware handles and a naive sketch does not:

- **The 40 ms no-echo timeout is a feature.** `pulseIn()` must wait longer than that
  (the firmware waits 45 ms) or you will time out early and invent a "no echo" for a
  perfectly good far reading. 40 ms of round trip is 6.9 m of air, so 45 ms covers the
  module's own stated range.
- **Two pings closer than 50 ms apart make the module answer with garbage.** The serial
  console, the `/api/log` route and the sample timer can all ask for a ping within a
  millisecond of each other, so the gap is enforced once inside `pingRaw()` rather than
  left to every caller to remember.

---

## 3. The blind area, and why the gate is 25 cm not 20 cm

Inside 20 cm the module cannot hear anything: it is still ringing from its own transmit
burst when the echo of a near target would arrive. Every distance below that is
physically unmeasurable, so the top of the tank must stay clear of it by design, not by luck.

The firmware gate is `BLIND_GATE_CM 25.0f`, five centimetres above the module's own limit,
because the tank surface moves:

- swell and wind ripple move the apparent surface up and down by several cm;
- rain falling into an open tank makes a frothy layer at the surface that reflects weakly
  or not at all;
- a film of dew on the probe face reads a couple of cm and looks like a very near target.

Anything the module reports inside the gate is **thrown away, not smoothed**. A filter that
average a blind-zone artefact with real readings produces a level that is wrong in a way you
cannot see. Rejected samples get a reason instead (`too close`, `no echo`, `out of range`),
and the reason is logged beside the number, in the CSV, on the web page and in
`echo_reason` in the JSON.

**Consequence for the mount: the full water line must sit further than 25 cm below the
probe face.** That single rule sizes the whole mounting — see §4. The reference project
states the same rule as "the minimum distance between the sensor and the full tank water
level must be greater than 25 cm".

---

## 4. Beam geometry against this tank

Internal tank: 304.8 × 304.8 × 121.9 cm (10 ft × 10 ft × 4 ft). Full line is 15.2 cm below
the lid. Water depth 121.9 cm, area 92 903 cm², 92.9 L per cm, 11 327 L full.

A 75° cone means the footprint widens at `tan(37.5°) = 0.767` per unit of distance:

| target | distance from face | beam diameter |
|---|---|---|
| full water line | 30.2 cm | 2 × 30.2 × 0.767 = **46 cm** |
| tank floor, empty | 152.1 cm | 2 × 152.1 × 0.767 = **233 cm** |

304.8 cm wide tank, 233 cm beam at its widest → **the beam never touches a wall**, so there
is no spurious wall echo to fight. That is why the probe goes in the middle of the lid and
why a 100 mm hole is enough.

Mounting that falls out of the geometry:

```
        ┌──────────────┐ dry box on the lid
        │  SR04M board │  R19 open, powered from the 5 V rail
        └──────┬───────┘
     ──────────┴────────────  lid, 100 mm hole
          │ probe face │  face stands 150 mm above the lid underside
          └────────────┘
        ↘   30.2 cm to the full line   ↙   75° cone, 46 cm across there
     ═════════════════════════════════  full water line (15.2 cm below lid)
        ↘                             ↙   cone widens to 233 cm at the floor
     ───────────────────────────────────  tank floor, 152.1 cm below the face
```

- **face 150 mm proud of the lid underside** buys the blind zone: 15.0 + 15.2 = 30.2 cm to
  the full line, i.e. 5.2 cm above the 25 cm gate.
- **The 100 mm hole is not cosmetic.** The rim and the box above it reflect; those clutter
  echoes land at about 15 cm, which is inside the 20 cm blind area and therefore invisible.
  Cut the hole smaller and clutter returns creep into the measurable band, where they do harm.
- **No screen, gauze or flyscreen over the aperture.** 40 kHz does not go through a flymesh —
  the screen simply blocks the ping and the module reports no echo. Keep insects out with
  a proper cowl or a foam gasket around the probe body, not across the beam.
- **Face the probe at still, level water**, and keep the fill inlet out of the beam: an
  inlet pouring water makes foam and turbulence exactly where the sensor is looking.
- The datasheet's own target rule (smooth, ≥ 0.5 m²) is comfortably met: even at the full
  line the beam covers 0.17 m² of open water and the tank offers 9.3 m².

Rev A had a 250 mm chimney to defeat the narrow ~15° cone of the old sensor. That chimney is
gone and must not be rebuilt: with a 75° cone it would be a resonant pipe full of clutter
echoes.

---

## 5. What it cannot see, and what the firmware does about it

| condition | what the module reports | firmware response |
|---|---|---|
| surface inside 20 cm of the face | nothing (self-ringing) | impossible by mounting design; gate would reject it anyway |
| surface between 20 and 25 cm | a distance | rejected as `too close`, status `NEAR/BLIND` |
| foam, froth, heavy ripple, Algae mat | weak or no echo | rejected as `no echo`; the gauge holds the last good value |
| dew or a water film on the probe face | 2 – 8 cm | rejected as `too close`, status `DEW ON FACE` |
| dust / cobwebs on the face | slightly near-biased distance | filtered; clean the face at each inspection |
| lid rim, box, cable in the beam | ~15 cm | inside the module's blind area, never measurable |
| probe face not level | scatter, then no echo | `no echo` streaks; re-level the mount |
| tank genuinely empty and clean | 152 cm ± a few cm | valid; that is the `e` calibration point |

Rejections are counted separately and reported, never averaged away:

```
pings: 412 good / 430 total (near 18, no-echo 0, far 0)
```

A sensor fault is raised after `FAULT_STREAK` = 3 consecutive `no echo` / `out of range`
pings, which is what turns a quiet gauge into a beeping one when the probe dies. A streak of
`too close` is deliberately **not** a fault: if the last good level was already high the tank
is genuinely over the calibration point (the high alarm covers it); if it was low it is dew,
which is a nuisance and not a broken instrument.

---

## 6. Temperature: the one real error source

Speed of sound in dry air is `331.3 + 0.606·T` m/s, so it moves **0.177 % per °C**.

```
at 152.1 cm:   0.177 %/°C × 152.1 cm = 0.27 cm per °C
30 °C → 15 °C  2.7 %   →   4.0 cm of level   →   372 L
30 °C → 45 °C  2.6 %   →   3.9 cm of level   →   360 L
```

There is **no temperature sensor in this build** — the sound speed comes from a configured
air temperature (`t <°C>`, default 30 °C, stored in NVS). That is honest for a gauge whose
job is "how much water do I have left, roughly", and it is the one number to revisit if you
want better than ±5 cm.

Note that a *fixed* temperature error is a **linear scale error**, not a random one: it is
zero at `d_full` and worst near empty, so the empty end of the range drifts more than the
full end. It is also partly cancelled by calibrating in place — `e` and `f` are both taken at
the ambient temperature of the day, which bakes that day's speed into both constants.

If a temperature-compensated build is ever wanted, the module does not need to change: read
any 1-Wire or I²C thermometer and pass it to `speedOfSoundMmPerUs()`. That is a rev C idea,
not a reason to fit parts now.

---

## 7. Calibration: two distances, not two levels

Everything in the gauge is a **distance from the probe face, measured downwards** — the same
two constants the reference sketch calls `emptyTankDistance` and `fullTankDistance`. Levels
are derived from them:

```
span  = d_empty − d_full                 152.1 − 30.2 = 121.9 cm of water
level = clamp(d_empty − d,  0 … span)
pct   = level / span × 100
litres= level × 92 903 cm² / 1000
```

Calibrate **at the tank**, with a tape measure, not from a drawing:

```
e <cm>     face → water surface with the tank as empty as it ever gets
f <cm>     face → water surface at the real overflow/full line
```

Both are stored in NVS. If `d_empty − d_full` comes out under 20 cm the firmware refuses the
calibration and falls back to the built-in defaults, because that combination cannot be
right and would silently invent levels.

Why distances rather than levels: the raw distance is what the sensor actually measured, and
it is what you need in order to diagnose a wrong number. A level is an opinion about the
calibration; log the distance next to it (the CSV does exactly this) and a bad calibration
becomes obvious instead of mysterious.

The filter that sits between the two: median of 15, a single-sample jump limit of 12 cm that
needs 3 consecutive confirming reads to be accepted (so one wild echo cannot move the
display), and the last good value held for 20 samples while a fault streak builds.

---

## 8. Bring-up failure modes, in the order they actually happen

1. **`r` gives `no echo` on a bench test.** 90 % of the time: `R19` has a resistor fitted
   (not mode 1), or VCC is not actually 5 V, or TRIG/ECHO are swapped, or the ECHO wire went
   to the wrong GPIO. Send `r` and read the line it prints — it lists those four.
2. **Distances are wrong by a constant factor.** Wrong `t` (air temperature), or a 3.3 V
   module supply while the divider is fitted for a 5 V one, or the echo line back-fed with
   5 V and half-dead GPIO26. Measure the divider output while pinging: about 3.0 V.
3. **Reads appear only sometimes.** Beam not square to the water, surface rippling, probe
   cable swinging in the wind, or pings faster than 60 ms apart.
4. **Numbers wander by ±10 cm and only when the sun is on the box.** Temperature (§6).
5. **The gauge goes blind exactly when the tank is full.** Face-to-full-line under 25 cm —
   the mount is wrong, raise it. The boot banner warns about this (`d_full < gate + 5 cm`).

---

## 9. Why not just use the video's HC-SR04

The reference build uses HC-SR04 because it is the cheapest ultrasonic module in India, and
for a demo on a desk that is fine. For a real water tank it is not:

| | HC-SR04 | SR04M family |
|---|---|---|
| the transducer | bare, unsealed, exposed pins and a hole in the PCB | epoxy-potted, weatherproof |
| condensation | forms inside the open face, then corrodes the pads | cannot — nothing is exposed |
| blind area | ~2 cm (nice on a bench, irrelevant over a tank) | 20 cm |
| max range | ~4 m | 6 m / 8 m |
| beam | ~15° | 75° |
| life over a water tank | months | years |

The blind area is the trade, and it is paid for with 150 mm of standoff above the lid — which
costs nothing and buys the mounting its headroom. Everything else the SR04M gives you is
better for this job, which is why rev B is ultrasonic-only and mode 1 keeps the interface as
simple as the HC-SR04 the video uses.

---

## 10. References

- Module datasheets: `AJ-SR04M` (mode table, R19, ranges, blind area) and `JSN-SR04M-2`
  (working current < 8 mA, trigger ≥ 10 µs, 348 m/s arithmetic, target ≥ 0.5 m²). Local
  copies used while writing this note: `$TEMP/sr04m/aj.txt`, `$TEMP/sr04m/jsn.txt`.
- Seeed SenseCAP SR04M product page — same family, sells the probe with mounting hardware.
- Tech StudyCell, *IoT Based Water Level Indicator using Ultrasonic Sensor*:
  [write-up](https://iotcircuithub.com/iot-based-water-level-indicator/) ·
  [video](https://youtu.be/9geREeE13jc) — GPIO numbering and the > 25 cm rule adopted here.
