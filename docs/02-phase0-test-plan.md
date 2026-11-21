# Phase 0 test plan — SR04M ultrasonic (rev B)

What to run, in what order, and what "pass" means. Bench tests are marked **(bench)** and can
be done at a desk with no tank; **(tank)** needs the real thing. Every test says what to send
and what to expect back — if the number you get is different, that is the result, write it
down, and read §7 before assuming the code is wrong.

Compile and flash from this repo:

```
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 arduino/water_level_gauge
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p COM6 arduino/water_level_gauge
arduino-cli monitor -p COM6 -c baudrate=115200
```

Verified on this machine: 1 002 307 bytes (76 %) of flash, 50 652 bytes of RAM, no warnings,
with the Blynk layer compiled in and no `wlg_secrets.h` present. Commands available in the sketch
are listed by `h`:
`r s c w p e f a g t i v b l u j d D k x`.

---

## T1 — power-on, nothing attached **(bench)**

USB only, no module, no OLED, no buzzer. Reset the board.

- **expect:** the banner, then the config lines, then `oled: nothing answering at 0x3C/0x3D`,
  `blynk: OFF`, `wifi:` either connected or the AP fallback. Then `no echo` samples.
- **fail:** a reboot loop every few seconds. That is a brown-out — check the USB cable and the
  port's current before blaming anything else.
- Note the `!! d_full … margin` warning if it appears: it means the stored `d_full` is within
  5 cm of the 25 cm gate, i.e. the mount would go blind near full.

## T2 — echo divider voltage **(bench)**

Multimeter on the ESP32 side of the divider (GPIO26 to GND) while the module pings — set the
sample interval to `i 5` so there is a ping every 5 s.

| check | expect |
|---|---|
| idle, no ping | 0 V |
| during a ping | ≈ **3.0 V**, briefly |
| with a 5 V module supply and **no** divider | 5 V → **undo that immediately**, GPIO26 is not 5 V tolerant |

Also measure module VCC at the module pins while pinging: it must not dip more than a few
hundred mV. A visible dip means missing local capacitance (100 µF + 100 nF at the module) —
this is the usual cause of a board that reboots when it pings.

## T3 — raw ping at known distances **(bench)**

Tape a flat board (a hardcover book, A4 laminate, anything ≥ 30 cm across) under the probe at
measured distances. Send `r` at each:

| face → target | expect `r` to report | verdict |
|---|---|---|
| 30 cm | `30.x cm [ok]` | pass if within ±1 cm |
| 60 cm | `60.x cm [ok]` | ±1 cm |
| 100 cm | `100.x cm [ok]` | ±1 cm |
| 150 cm | `150.x cm [ok]` | ±1.5 cm (datasheet: ±10 mm plus temperature error) |
| 250 cm | `250 cm [ok]` if the module's rated range covers it | JSN-SR04M-2: 6 m, AJ-SR04M: 8 m |

The `r` reply also prints the pulse width in µs and the arithmetic: 150 cm at 30 °C is about
**8590 µs**. Check one by hand: `µs / 57.5 ≈ cm` at 25 °C — that is the datasheet's own
formula, and if your number is off by a constant factor, the temperature setting `t` is wrong.

- **fail:** `no echo` at 30 cm on a bench. Work through §8 of
  [03-ultrasonic.md](03-ultrasonic.md) — the usual cause is `R19` populated (the module is not
  in mode 1) or TRIG/ECHO swapped. The reply prints those four checks; read it.

## T4 — the blind area is real **(bench)**

Aim at something at **10 cm**, then **18 cm**, then **30 cm**.

- **expect:** at 10 and 18 cm, `r` reports `too close` and shows the small measured value
  (`lastNearCm` in the near case). At 30 cm, `ok`.
- This is not a fault — it proves the 20 cm blind zone, and that the firmware's 25 cm gate
  throws those reads away instead of averaging them into the level.
- **fail:** a plausible-looking distance at 10 cm. Something is wrong with the gate setting:
  check `g` prints 25.0.

## T5 — no-echo timeout **(bench)**

Aim the probe at a wall 8 m away, or at a surface tilted 45° so the sound misses the probe.

- **expect:** `no echo`, and the `r` line shows the pulse width as 0 or ≥ 39500 µs. That is the
  module's own 40 ms timeout. The firmware waits 45 ms in `pulseIn()` on purpose: shorter would
  invent a spurious `no echo` on a legitimate far reading.

## T6 — calibration walk **(bench or tank)**

With the module at the height it will actually sit at, measure with a tape and enter:

```
e 152.1        (as empty as the tank ever gets — measure it, do not type the drawing)
f 30.2         (at the real overflow / full line)
v              (prints d_empty, d_full, span, gate, L/cm)
```

- **expect:** `span = 121.9 cm`, `92.9 L per cm`, `11327 L at full`.
- Send `c` and read the CSV line: `raw_cm` and `filt_cm` should agree with what `r` said, and
  `level_cm` should be `152.1 − filt_cm`.
- **Guard test:** type a nonsense pair (`e 40`, `f 35` → span 5 cm). The second command must be
  refused outright — `refused: span must be at least 20 cm` — and nothing stored. Send `i`, `u`
  with no argument and `l 999` too: each must answer `Nothing changed` — thresholds and intervals
  are range-checked like Blynk V10–V12, so a bare `i` must never become “sample every loop”.
  Reset anyway: the boot banner must also refuse a bad stored calibration and fall back to the
  defaults. If anything accepts a 5 cm span or a missing number, the validation is broken.

## T7 — filter and jump rejection **(bench)**

Wave your hand through the beam for a few seconds while `c` runs at `i 5`; then put a book at a
fixed distance again.

- **expect:** the hand gives scattered rejects (`too close`), `filt_cm` does **not** chase them,
  and when the book returns the value recovers within 3 samples. A single wild reading must
  never move the displayed level — `MAX_JUMP_CM 12` with 3-sample confirmation.

## T8 — temperature sensitivity **(bench)**

Sound speed moves 0.177 % per °C: **0.27 cm per °C at 152 cm.**

```
r                    (note the distance at today's t)
t 15   →  r          (expect about 4 cm shorter raw distance at the same physical setup)
t 30   →  r          (back where you started)
```

- **expect:** roughly ±4 cm between `t 15` and `t 30` at a 152 cm path, matching the
  arithmetic in [03-ultrasonic.md §6](03-ultrasonic.md). This is the size of the error you are
  accepting by having no temperature sensor — it is also why `e` and `f` are both calibrated at
  the tank on the same day.

## T9 — dew and foam **(tank)**

Breathe on the probe face, or wipe it with a damp cloth, then send `r` and `c`.

- **expect:** status becomes `DEW ON FACE` (a near read of 2–8 cm), level held, no false alarm.
- Foam: pour water into a bucket under the probe so the surface is frothy. Expect `no echo`
  rejects rather than a wrong-but-plausible number. That is the behaviour the design wants: the
  gauge would rather admit it cannot see than print a fiction.

## T10 — fault injection **(bench)**

Unplug the module and watch 4 sample cycles at `i 5`.

- **expect:** after **3 consecutive** `no echo` / `out of range` pings the gauge raises a sensor
  fault: status `NO ECHO`, the buzzer joins the 9 s cycle, the OLED status band inverts, and with
  Blynk configured a `sensor_fault` event is sent. Meanwhile the displayed level holds the last
  good value (up to 20 samples) rather than dropping to zero — a stale number with a `STALE` /
  `NO ECHO` label is honest; a false zero is not.
- Plug it back: the fault clears and a `level_ok` event follows.

## T11 — 24 h stability on the lid **(tank)**

The test that decides whether a PCB is worth designing. Mount the box on the closed lid, probe
in its hole, `i 60`, and capture a day of CSV:

```
arduino-cli monitor -p COM6 -c baudrate=115200 | tee wlg-day1.csv
```

CSV columns, exactly as printed:

```
ts_s,echo_us,raw_cm,reason,filt_cm,level_cm,pct,litres,air_t,gate_cm,vbat,rssi,bad,n
```

Strip the banner lines, then:

```
awk -F, 'NF>=14 {print $5}' wlg-day1.csv | sort -n | uniq -c | tail   # spread of filt_cm
awk -F, 'NF>=14 {n++; if($4!="ok") b++} END{printf "%.1f%% rejected\n", b*100/n}' wlg-day1.csv
```

- **pass:** ≥ 95 % of pings accepted, filtered distance within **±2 cm** across the whole day,
  no gaps longer than a few minutes, no reboot (uptime restarting).
- If rejection is above 5 % at night and near zero by day → condensation on the face (T9). If it
  tracks the afternoon sun → temperature (T8) and/or a box heating in the sun.

## T12 — WiFi through a closed metal lid **(tank)**

`w` prints every network with RSSI; the `/api/scan` route does the same over HTTP. Do the survey
with the box **closed and sitting on the shut lid**, which is where it will live.

- **pass:** ≥ −80 dBm. Record the numbers for the enclosure design; the antenna is a PCB trace
  against a metal lid, and this is the test that says whether the antenna has to stand clear.

## T13 — alarm end-to-end **(bench)**

```
l 121      force the low threshold above the current level (default span is 121.9 cm)
```

- `l 999` is now **refused** by design — serial thresholds are range-checked 0…span, the same guard
  Blynk V11/V12 apply. If a 999 goes through, the validation is broken.

- **expect:** within 9 s a beep (250 ms at 1.8 kHz for low, 2.6 kHz for high/fault), the LED
  behaviour per the design (heartbeat normally, solid on for the high alarm), the OLED status
  band inverted, and — if Blynk is configured — a `level_low` push on the phone.
- Button: **short press** snoozes the buzzer for one 9 s period and wakes the panel;
  **hold 1.5 s** latches the mute (`j` does the same from the console, and it is stored in NVS).
- `u 0` for the high alarm, then `l 24`, `u 110` to restore the defaults. Check the recovery
  push (`level_ok`) arrives too — a stuck alarm that never clears is a bug, not a feature.
- Cooldown check: with Blynk on, an alarm that bounces across the threshold must not push more
  than once per 30 min (`ALARM_COOLDOWN_S`), and must not re-arm until it has cleared by 2 cm
  (`ALARM_HYST_CM`).

## T14 — OLED in daylight and idle blanking **(bench)**

Set up in sunlight next to the window and read the panel from 1 m.

- rows as implemented: big level + `cm`, % and litres top-right, level bar, `BAT x.xxV xx%` +
  RSSI, `ECHO x.x` + `SPAN x.x`, `GATE x.x TxnC` + `N bad`, centred status line.
- **expect:** readable at arm's length in shade; in direct sun any 0.96" SSD1306 washes out —
  raise `OLED_CONTRAST` (0x8F default) and accept that the panel is a convenience, the phone is
  the real display.
- Panel must blank after `OLED_IDLE_S` (120 s) and come back on a button press. Unblanked panels
  in a hot roof box burn their own digits into the glass within a year.

## T15 — power over a week **(tank)**

Run from 1S 18650 + charge controller + panel, logging `b` (or the `vbat` column) daily.

- **pass:** `vbat` never under 3.4 V, no reboot in the log, no brown-out marker after a ping.
- Also try the **3V3 module-supply variant** (no divider, §2 of
  [01-wiring-phase0.md](01-wiring-phase0.md)) at the tank and log it side by side: that is the
  data you need before committing a PCB to a 5 V rail and a divider.

---

## 6. Recording results

Fill this in as you go — it is the input to the PCB decision.

| test | date | result | notes |
|---|---|---|---|
| T3 150 cm accuracy | | cm measured: | |
| T4 blind zone | | rejected at 10/18 cm: | |
| T11 24 h spread | | ±cm, % rejected: | |
| T12 RSSI, lid shut | | dBm: | |
| T15 week on battery | | min vbat: | |

## 7. Before calling anything a bug

| symptom | check first |
|---|---|
| `no echo` on the bench | `R19` unpopulated (mode 1), TRIG↔ECHO not swapped, module VCC really 5 V, GND bonded |
| distances wrong by a constant factor | `t` air temperature, `g` gate, module supply 5 V vs 3V3, divider fitted |
| reboots when it pings | local capacitance at the module, USB cable, brown-out |
| level wanders slowly | afternoon sun on the box, T8 arithmetic — expected, not a fault |
| rejects spike at night | condensation on the face, T9 |
| alarm never reaches the phone | the event code must exist in the Blynk template; `blynk: event sent` only means it left the device — [05-blynk-and-display.md](05-blynk-and-display.md) |
| push notifications keep repeating | hysteresis and cooldown settings, T13 |
