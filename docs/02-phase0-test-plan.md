# 02 — Phase 0 bench and field tests

Code: `arduino/water_level_gauge/water_level_gauge.ino` (Arduino IDE, board **DOIT ESP32 DEVKIT V1**).
Wiring: [`../docs/01-wiring-phase0.md`](01-wiring-phase0.md) · diagram [`img/01-wiring.svg`](img/01-wiring.svg) ·
geometry [`img/02-mounting.svg`](img/02-mounting.svg) · display + app: [`05-blynk-and-display.md`](05-blynk-and-display.md)
and [`img/04-display-and-app.svg`](img/04-display-and-app.svg)

```
serial 115200:  r  raw read     s  JSON     c  sample + CSV     w  WiFi scan
                w <ssid>  p <pass>          e <cm>  f <cm>  a <cm2>  m <cm>  o <cm>  z
                i <sec>  b  x  h            l <cm>  u <cm>  j        d  D      k
```

`h` prints the firmware's own list — trust it over anything typed here. `d`/`D` only exist with
`ENABLE_OLED 1`, `k` only with `ENABLE_BLYNK 1`.

## Test 1 — does the sensor see the water at all? (bucket, 10 min)

Bucket ≥ 20 cm wide. Set the empty distance to the bucket surface distance first (`e 40`), then compare
the reported raw distance with a tape measure.

| face → water | expected raw | if you get |
|---|---|---|
| 15 cm | **rejected** (blind zone 35 cm) | a number → the blind-zone constant is wrong for your module |
| 25 cm | rejected or wild | normal for JSN‑SR04T |
| 52 cm | 52 ± 1 cm | correct |
| 100 cm | 100 ± 1.5 cm | correct |

Then `r` ten times with the bucket lid half on: you are looking for the echo of the lid/rim instead of
the water (a reading that snaps to a fixed value is the giveaway).

## Test 2 — RSSI through the lid (15 min)

Run `w` (scan) at the router, then at each candidate spot with the **box closed and the lid down**:

1. inside the tank, magnet under the lid (expected: −85 dBm or nothing — this is the proof you need
   before abandoning the original plan)
2. on top of the lid (this is the real case)
3. on top of the lid, chimney fitted (checks the chimney does not shield the antenna)

Record the RSSI the sketch prints in the CSV (`rssi` column). **−75 dBm or better is required** for
always‑on WiFi; below that the ESP32 retransmits and average current climbs.

## Test 3 — 24 h log on the real tank

Set `e`/`f` from the actual chimney height (design values for the 250 mm chimney: `e 174`, `f 52.2`,
`a 92903`, `i 30`), let it run overnight, then fetch `GET /api/log` and save the CSV.
Send me: the tail of the CSV, the `bad/n` counters, RSSI range, and any `WARN` lines.

What kills a gauge in the field, in order of likelihood:
1. dew/frost on the transducer face → drifting or dead readings (look for slow 1–3 cm ramps overnight)
2. wall/rim echo → reading snaps to a constant
3. sunlight on the box → the sensor's own temperature drift
4. WiFi dropout → check `bad` counter climbing in bursts, RSSI swings

## Test 4 — dew on the face

Leave the chimney pointed down for a night. If the face beads, the fix is (a) 10° tilt so water runs
off, (b) a hydrophobic coating, or (c) switch to the hydrostatic build
(`#define SENSOR_TYPE SENSOR_BARO_DUAL` + **BMP280 on the tube side, BME280 on the box side** + a vented
(tube to the floor — a BME280 in the tube clips at 1100 hPa, see [`04-parts-list.md`](04-parts-list.md) §8).
[`03-hydrostatic.md`](03-hydrostatic.md) §3 quantifies two effects the wiring diagram does not show: the trapped
air column reads **~11 % low**, and its temperature adds **~1 cm per °C** of box temperature swing.
Do **not** ship a design
that fails this test.

## Test 5 — the display and the local alarm, with the internet switched off (20 min)

Nothing in this test needs a router, a Blynk account, or water in the tank.

1. **Panel absent** (or not yet wired): boot must print `oled: nothing answering at 0x3C/0x3D — display
   code stays idle` and everything else must behave as before. Wire the panel while it is running and
   wait ≤10 min: it must appear without a reboot. If it never does, check VCC is on **3V3**, not VIN.
2. **Panel present**: `d` redraws. `D` flips 180° and stays flipped through a reboot (NVS key `flip`).
   The panel must go blank after 120 s **even with sampling running** — that is the burn-in and
   sunlight protection, and it is the difference between 3–8 mA and nothing. A short button press, or
   `d`, wakes it.
3. **Force a low alarm while watching**: `l 130` with the reported level above it. Within one sample
   the status band must invert and the buzzer must give 250 ms every 9 s at ~1.8 kHz. Then `l 1` — the
   alarm must clear once the level is more than 2 cm above the threshold (hysteresis), not chatter.
4. **Mute must persist**: `j` (or a 1.5 s button hold) silences it; reboot; it must still be silenced.
   A short press must **not** mute — it only snoozes one period.
5. **Cut the WAN / forget the WiFi**: the buzzer, the OLED banner and `http://<ip>/` must all keep
   working. If the alarm only works when the app works, it is not an alarm, it is a demo.

## Test 6 — the phone app: does a notification actually arrive (10 min)

1. Token left empty: boot says `blynk: OFF — paste BLYNK_TEMPLATE_ID and BLYNK_AUTH_TOKEN from
   Console -> Info`, and `k` says there is no link. The gauge must be otherwise normal.
2. Token set, template created with the 17 datastreams and the 4 events from
   [`05-blynk-and-display.md`](05-blynk-and-display.md) §5: `k` must print `link up`, push one sample
   set, and fire the test event. **A push must land on the phone within a few seconds.** If the serial
   says `event 'level_ok' sent` and nothing lands on the phone, the event code in the Console is
   wrong — the sketch cannot see that, so check the Console's event history.
3. Move the app's `V11` slider past `max_cm` and back: the serial must print
   `blynk V11: out of range, ignored`, then accept the valid value, and the value must survive a reboot.
4. Set `i 900` for deployment and confirm the throttle notice is absent; `i 60` and confirm it appears
   (`only V1/V8 go out to stay inside the … quota`). At 60 s the app is deliberately thin — that is
   the 100 000 msg/month free ceiling doing its job, not a bug.

## Gate to start phase 1 (KiCad)

All four pass, **or** test 1 fails at 52 cm, in which case the board design changes sensor section and I
design the hydrostatic option as primary instead.

Answers I need from you to open the schematic (the four questions in the plan):
1. chimney mount (build 1) or hydrostatic (build 2) — or "populate both, decide after phase 0"
2. distance and obstacles from the tank lid to the router (for the antenna/box placement note)
3. sample interval for the real deployment (30 s, 1 min, 5 min?) — this also sets the phone app's
   message spend: 900 s is ~17 % of the free Blynk quota, 60 s is ~43 %
   ([`05-blynk-and-display.md`](05-blynk-and-display.md) §5.5)
4. do you already own the BMP280 + BME280, JSN‑SR04T V3, DS18B20, 18650s, solar panel? The order list is
already written — [`04-parts-list.md`](04-parts-list.md), ₹ prices with Indian links (2026-11-16); LCSC or
Digi-Key India are needed only for the bare ICs on the phase-1 board.
