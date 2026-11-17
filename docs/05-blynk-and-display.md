# 05 — The display, the local alarm and the phone app

What this adds: an OLED on the box, a buzzer plus button on the box, and the Blynk IoT app
(dashboard + push notifications). What it does **not** add: any way to switch a pump, a valve or
anything else. Same read-only promise as [`00-design.md`](00-design.md) §1 — the app writes
thresholds, a sample interval and a buzzer mute, nothing else, and the gauge keeps measuring,
logging and serving its own web page when the internet is dead.

Figure: [`img/04-display-and-app.svg`](img/04-display-and-app.svg) / [`.png`](img/04-display-and-app.png)
— the real 128×64 layout row by row, the app streams, and the message budget.
Wiring: [`01-wiring-phase0.md`](01-wiring-phase0.md) (the OLED and the piezo are on that pin map).

Code: `arduino/water_level_gauge/water_level_gauge.ino` + `wlg_display.h` + `wlg_font.h`.
Functions worth knowing: `statusText()`, `oledDraw()`, `oledService()`, `evaluateAlarms()`,
`outputsUpdate()`, `buzzerService()`, `buttonService()`, `blynkPush()`, `blynkService()`.

---

## 1. Two switches, and what each one costs

| switch | default | what it gates |
|---|---|---|
| `ENABLE_OLED` | 1 | the SSD1306 driver, `d`/`D` commands, screen blanking |
| `ENABLE_BLYNK` | 1 | the Blynk include, `k` command, `BLYNK_*` defines, app streams |
| `SENSOR_TYPE` | 2 (baro-dual) | 1 = ultrasonic only — unchanged, see `00-design.md` |

Both are `#ifndef`-guarded, so you can set them from the build instead of editing the sketch:

```
arduino-cli compile --fqbn esp32:esp32:esp32 --build-property compiler.cpp.extra_flags="-DENABLE_BLYNK=0" water_level_gauge.ino
```
In the Arduino IDE, edit the two `#define` lines near the top of the `.ino`.

Measured on esp32 core **3.3.12**, FQBN `esp32:esp32:esp32` (WROOM-32: 1 310 720 B flash,
327 680 B RAM), `--clean`, 2026-11-16 and re-measured 2026-11-17:

| variant | flash | static RAM |
|---|---|---|
| default (build 2, OLED + Blynk compiled in, **token left empty**) | **1 001 215 B (76 %)** | **50 780 B (15 %)** |
| `-DSENSOR_TYPE=1` (build 1) | 1 001 547 | 50 780 |
| `-DENABLE_BLYNK=0` | 1 000 463 | 50 628 |
| `-DENABLE_OLED=0` | 997 843 | 49 740 |
| both off (the phase-0 build) | 997 099 | 49 588 |
| **default, but with a real template ID + token pasted** | **1 008 799 B (77 %)** | 50 820 |

So **the OLED driver costs 3 372 B flash + 1 040 B RAM**, 1 024 B of which is the 128×64 frame buffer.
**Blynk's real cost is the last row minus the `ENABLE_BLYNK=0` row: 8 336 B flash + 192 B RAM** — not
the 752 B that the default and `ENABLE_BLYNK=0` rows differ by. With `BLYNK_AUTH_TOKEN ""` the compiler
proves `blynkConfigured()` is false and dead-strips the cloud path: the literal `blynk.cloud` is absent
from the empty-token binary and appears once in the token-pasted one. Two things follow: leaving the
token empty really is close to free, and any flash figure quoted for "the Blynk build" depends on
whether a token has been pasted yet. Flash left after the worst row above: **≈302 kB**.

### 1.1 The Blynk library, and one trap that wasted an hour

```
arduino-cli lib install "Blynk"        # pulls Blynk 1.4.0 + BlynkNcpDriver 0.7.0
```

The sketch includes Blynk with a **plain** conditional:

```cpp
#if ENABLE_BLYNK
#include <BlynkSimpleEsp32.h>
#endif
```

It must stay that way. An `#include` hidden inside `#if defined(__has_include(...))` — the usual
trick for optional features — is **invisible to Arduino's library scanner**: the scanner parses the
source text, not the preprocessor, so the library is never added to the build. Measured on this machine
with a token pasted (so the cloud code is live) — same core, same flags:

| how Blynk is included | flash | does the cloud code exist? |
|---|---|---|
| plain `#if ENABLE_BLYNK` | 1 008 799 B | yes — the literal `blynk.cloud` appears once in the `.bin` |
| `-DENABLE_BLYNK=0` (nothing included) | 1 000 463 B | no — no `blynk` string, no transport object |

So Blynk's true cost is the difference: **8 336 B flash, 192 B RAM**. An `__has_include`-guarded include
sits in the second row while *looking* like the first, which is why the guard is not used here. The way
to check it yourself, without hardware:

```bash
python -c "d=open('build/…/water_level_gauge.ino.bin','rb').read(); print(d.count(b'blynk.cloud'))"
```

Zero means the cloud layer is not in the image, whatever the sketch appears to say.

The current design is deliberately fail-loud: `ENABLE_BLYNK 1` + no library = a compile error
(`BlynkSimpleEsp32.h: No such file or directory`). Two ways out, both intended:
install the library, or set `ENABLE_BLYNK 0`.

Upstream noise to ignore: Blynk 1.4.0 emits one warning on ESP32 core 3.3.x —
`Blynk/src/BlynkApiArduino.h:69:39: warning: 'mem_dyn' may be used uninitialized
[-Wmaybe-uninitialized]`. Not ours, harmless, do not "fix" it in the sketch.

---

## 2. The display: 4 wires, no new GPIO, no library

| OLED pin | to |
|---|---|
| VCC | **3V3** (not VIN — these modules are 3.3 V parts on I²C) |
| GND | GND |
| SDA | GPIO21 — the same wire as the pressure sensor |
| SCL | GPIO22 — the same wire as the pressure sensor |

Address map on that pair of wires:

| device | address | probed by |
|---|---|---|
| SSD1306 OLED | `0x3C`, else `0x3D` | `oledSetup()` at boot, and every 10 min if nothing answered |
| BMP280 / BME280 | `0x76`, else `0x77` | `bmeInit()` (unchanged) |

Three modules, two wires, no address conflict. The bus pull-ups are already on both modules.
`Wire.begin(PIN_SDA, PIN_SCL)` happens once in `setup()`; the display driver reuses that bus.

The driver is `wlg_display.h` (about 200 lines: init, page-addressed frame buffer, blit, 5×7 font,
text/bar/invert helpers, sleep). `wlg_font.h` is the 5×7 ASCII glyph set, extracted mechanically
from `Adafruit_GFX/glcdfont.c` — glyph data is the decades-old public-domain character-ROM pattern
set, Adafruit_GFX's code is BSD-3-Clause. **No Adafruit libraries are needed.** That is why the
"core builds with nothing but the ESP32 core" claim still holds — it only breaks for Blynk (§1.1),
and Blynk is opt-in and clearly reported.

No panel fitted, or a dead one: boot prints `oled: nothing answering at 0x3C/0x3D — display code
stays idle`, the sketch continues normally, and `oledService()` re-probes the bus every 600 s, so a
panel fitted later shows up without a reboot.

Knobs (in `wlg_display.h` and near the top of the sketch):

| knob | default | why you would touch it |
|---|---|---|
| `OLED_CONTRAST` | `0x8F` | a sun-heated panel fades; raise towards `0xFF` |
| `OLED_VCOMH` | `0x40` | faint-rows fix on a marginal panel, rarely |
| `OLED_IDLE_S` | 120 | blank after this; **`0` = never blank** (mains-powered box) |
| `OLED_REDRAW_MS` | 2000 | refresh period while awake |
| `cfg.oledFlip` | 0 | `D` flips 180°, saved in NVS key `flip` |

Honest warnings about a panel in a roof box, learned from the class of part rather than from our
own panel (we have not flashed one yet — §8):

- A routine sample marks the image dirty but does **not** wake a blanked panel (`oledRefresh()`);
  only a button press, serial `d`, or an alarm starting wakes it. Without that rule, a 30 s sampling
  rate would keep the panel lit forever and `OLED_IDLE_S` would be fiction.

- an SSD1306 is **unreadable in direct sun**. Contrast ratio collapses; twisting the contrast does
  not save it. Face the window away from the sun (see the mounting figure), or run without it.
- heat and UV kill these panels years before they kill an ESP32.
- static digits burn in. The big level number is static for hours. This is the real reason the
  default is to blank the panel after 120 s, not a power-obsession.
- a lit panel is about **3–8 mA** with text on it (all-white full-on is 2–3× that); a blanked panel
  (cmd `0xAE`) draws ~0. The driver has no reset or power pin wired, so blanking is done by
  sleeping the panel, not by cutting its power.

---

## 3. What the screen shows, and why those fields

From `oledDraw()`, y coordinates in the code's own 128×64 space:

| y | content | why it is there |
|---|---|---|
| 0 | level in **big 2× digits** + `cm`, and right-aligned `pct %` / litres | the one number you want at 3 m |
| 18 | bar, 0…100 % of `maxLevelCm` | level trend readable at a glance |
| 30 | `BAT x.xxV nn%` … `−nn dBm`, or `NO WIFI` | battery is what fails first; RSSI tells you whether the antenna placement is right before you glue the box down |
| 39 | box `AIR xC nn%` … water `H2O xC` | box RH is the condensation risk (see `03-hydrostatic.md`), air T is what the ultrasonic speed-of-sound correction uses |
| 48 | what the sensor saw: `ECHO xx.xcm` or `DP +x.xxhPa` … `n bad` | the raw evidence, and the count of rejected reads — a bare sensor lies *slowly*, the counter shows it |
| 56 | one status word, centred | the summary, and the alarm banner |

An active alarm **inverts the status band** (`oledInvertBand`) — inverting is the only way to shout
on 128×64 pixels, and it works even if the buzzer is muted.

The status word comes from `statusText()`, and it is the *same* string used by the OLED, the LAN web
page (`/api/state` → `status`), the Blynk status stream `V8` and the notification text. That is
deliberate: one word, four places, no drift. Exact strings:

| status | meaning |
|---|---|
| `OK` | normal |
| `LOW LEVEL` | level below `cfg.lowCm` |
| `HIGH - OVERFLOW` | level above `cfg.highCm` |
| `TUBE OVER RANGE` | build 2: the tube-side absolute pressure reads above 1200 hPa (`pTubePa > 120000` in `statusText()`) — at the top of this tank the true figure is ≈1133 hPa, so a BME280 in the tube has already clipped (its ceiling is 1100 hPa) and the part there must be a **BMP280**: `04-parts-list.md` §8 |
| `TUBE SENSOR MISSING` | build 2 tube-side sensor absent |
| `NO VALID READS` | every read rejected (blind zone, bad mounting) |
| `NO READING` | nothing valid yet since boot |
| `MANY BAD READS` | the filter is working but the raw stream is mostly junk |

The CSV log schema is **unchanged** by this work — `status` and the alarm fields are in
`/api/state` and in the app, not in the CSV, so old parsers keep working.

---

## 4. The local half: thresholds, buzzer, button (works with no internet)

| constant | value | note |
|---|---|---|
| `DEF_LOW_CM` | 24.0 | ≈20 % of the 121.9 cm tank; change with serial `l <cm>` or app slider `V11`, saved to NVS; exercised by test 5 in `02-phase0-test-plan.md` |
| `DEF_HIGH_CM` | 120.0 | 1.9 cm below the tank top |
| `ALARM_HYST_CM` | 2.0 | an alarm only *clears* 2 cm the other way — a falling level must reach `low − 2` to set, `low + 2` to clear |
| `ALARM_COOLDOWN_S` | 1800 | at most one notification per event per 30 min |
| `BUZZ_PERIOD_MS` / `BUZZ_ON_MS` | 9000 / 250 | one 250 ms beep every 9 s while an alarm is on and unmuted |
| beep pitch | 1 800 Hz low, 2 600 Hz high/fault | you can tell them apart through a wall |

`evaluateAlarms()` sets three independent states (`alrm.low`, `alrm.high`, `alrm.fault`) and
`outputsUpdate()` pushes them to the buzzer, the screen banner and the app (an alarm starting also
wakes a blanked panel — the banner has to be seen, not just heard). The fault state has a
**warm-up guard**: nothing is declared faulty until the first valid filtered reading exists, so a
reboot at 3 a.m. cannot cry wolf about a missing sensor before the median filter has anything to
work with.

Moving the thresholds — all four settings are written to NVS immediately and read back at boot:

| way | what | NVS key |
|---|---|---|
| serial `l <cm>` / `u <cm>` | low / overflow level | `lowCm`, `highCm` |
| app sliders `V11` / `V12` | same, 0…`maxLevelCm` enforced | same |
| serial `j`, app `V13`, or the button | buzzer mute | `mute` |
| serial `i <sec>`, app `V10` | sample interval, 5…7200 s | `sample` |

Button (GPIO23 to GND, internal pull-up, 30 ms debounce in `buttonService()`):

- **short press**: wake the panel + snooze the buzzer for one more beep period.
- **hold ≥1.5 s**: toggle the mute. Long press rather than short, so you cannot mute the alarm by
  leaning on the box.

Buzzer wiring is unchanged from `01-wiring-phase0.md` (GPIO17 → 1 kΩ → S8050 → piezo). It is driven
with `tone()`, i.e. it is an active-piezo-on-a-transistor, not a latched alarm.

---

## 5. Blynk: template, streams, events, and the quota that stops a gauge dead

### 5.1 Setup, in order

1. `arduino-cli lib install "Blynk"` (or Library Manager → "Blynk" by Blynk Inc.).
2. Blynk Console (<https://console.blynk.io>) → create a template → **Datastreams** → add the 17
   streams in §5.2 (V0…V16; write-type for V10…V13) → **Events** → add the 4 events in §5.3 with
   the codes exactly as written → **Device** → Add → copy the three values.
3. Paste them into the sketch:

```cpp
#define BLYNK_TEMPLATE_ID   "TMPL........"      // Console -> Device -> Info
#define BLYNK_TEMPLATE_NAME "Water Tank Gauge"  // must match the template name, character for character
#define BLYNK_AUTH_TOKEN    "................................"   // 32 chars
#define BLYNK_SERVER        ""                  // "" = blynk.cloud; else e.g. "192.168.1.50"
#define BLYNK_SERVER_PORT   80
```

4. Flash. With no token, boot prints `blynk: OFF — paste BLYNK_TEMPLATE_ID and BLYNK_AUTH_TOKEN
   from Console -> Info` followed by `the phone app is optional; http://<ip>/ still works with no
   internet`. With a token and WiFi already up you get `blynk: connected  host=blynk.cloud port=80`,
   or `blynk: not reachable (retrying in the background)`; with a token but no WiFi link yet,
   `blynk: no WiFi yet — will try once the link is up`.

The token is a **password that controls your device**: anyone who has it can write V10…V13. Keep it
out of git (add it to a local override, or type it in per build), and rotate it in Console if it
leaks. The stock cloud endpoint is port **80 plain TCP** — on a trusted home LAN that is what the
stock Blynk examples use too, but it is not a secret channel; Blynk's SSL variant
(`BlynkSimpleEsp32_SSL.h`) is a one-line change if your LAN is not trusted.

### 5.2 The 17 datastreams (free template allows 50)

| pin | name | type | direction | sent |
|---|---|---|---|---|
| V0 | `level_cm` | Double | read | when sampling ≥ `BLYNK_THROTTLE_S` |
| V1 | `level_pct` | Double | read | **every sample** |
| V2 | `litres` | Double | read | ≥ throttle |
| V3 | `dist_cm` | Double | read | hourly, build 1 |
| V4 | `vbat` | Double | read | ≥ throttle |
| V5 | `air_t` | Double | read | hourly |
| V6 | `air_rh` | Double | read | hourly |
| V7 | `water_t` | Double | read | hourly |
| V8 | `status` | **Text** | read | on change only |
| V9 | `rssi` | Integer | read | hourly |
| V10 | `sample_s` | Integer 5…7200 | **write** | app → device |
| V11 | `low_cm` | Double 0…max | **write** | app → device |
| V12 | `high_cm` | Double 0…max | **write** | app → device |
| V13 | `mute` | Integer 0/1 | **write** | app → device |
| V14 | `p_ref_hpa` | Double | read | hourly, build 2 |
| V15 | `p_tube_hpa` | Double | read | hourly, build 2 |
| V16 | `p_diff_hpa` | Double | read | hourly, build 2 |

`BLYNK_CONNECTED()` calls `Blynk.syncVirtual(V10, V11, V12, V13)` — only the four writable streams,
not `syncAll()`, because every synced value is a metered message. After a reboot the **app values win
over the sketch defaults**. The four handlers validate their ranges and reject nonsense out loud:

```
blynk V10: out of range, ignored (5..7200 s)
blynk V11: low alarm = 30.0 cm
```

### 5.3 The 4 events (free template allows 5)

Create these with exactly these codes; the firmware sends them by code
(`Blynk.logEvent(code, text)`), and the text lands in the push notification:

| code | when | the text the firmware sends |
|---|---|---|
| `level_low` | level falls below `low_cm` | `Tank low: 18 cm (15 %), about 1430 L left. Alarm at 24 cm.` |
| `level_high` | level rises above `high_cm` | `Tank high: 121 cm (99 %) — check the inlet, overflow risk.` |
| `sensor_fault` | sensor missing / over range / no valid reads | `Gauge fault: TUBE SENSOR MISSING. The level shown may be stale.` |
| `level_ok` | an alarm cleared | `Level back to 42 cm (34 %).` or, after a fault, `Gauge reading again: OK.` |

A limitation to know before debugging: `blynkNotify()` prints `blynk: event 'level_low' sent` — that
only means the library accepted the call. The firmware **cannot** tell you whether the Console found
that event code. If the serial says sent but no push arrives, the usual cause is an event code that
does not exist in the template (or a typo/case difference), and the place to check is the Console's
event history, not the sketch. `k` sends a deliberately obvious test event —
`Test notification from the gauge — if this arrived, the event code is right.` — which is the fastest
way to separate "my template is wrong" from "my gauge is wrong".

### 5.4 How it connects, and why not `Blynk.begin()`

The sketch owns WiFi: STA with retries, and the `WaterLevel-Setup` access point fallback
(`01-wiring-phase0.md`). `Blynk.begin(auth, ssid, pass)` would take the radio over, block for up to
minutes when the internet is down, and fight that fallback. So:

```cpp
Blynk.config(BLYNK_AUTH_TOKEN /*, host, port */);   // no WiFi touch
Blynk.connect(4000);                                 // bounded attempt, only when WiFi is up
Blynk.run();                                         // every loop, when linked
```

`blynkService()` keeps that to a budget: one retry every 30 s while disconnected, 4 s timeout, no
`delay()` anywhere, `Blynk.run()` skipped entirely when not linked. With no token in the sketch
Blynk is compiled but inert, and the box behaves exactly as it did before this feature existed.
`k` on serial prints the link state, forces one `V1` push and fires one test event — the fastest way
to prove the app side is wired up.

### 5.5 Message quota — the number that can stop a gauge dead

Free plan, read from <https://docs.blynk.io/en/blynk.console/limits> on 2026-11-16 (plans change,
re-check before scaling):

| limit | value |
|---|---|
| device messages | 100 000 / month |
| push + email from one automation | 20 / hour |
| datastreams per template | 50 |
| events per template | 5 |
| templates / devices per free org | 10 / 5 |
| `/external/api/data/get` | **10 calls / day / device** |

That last row kills any third-party dashboard that polls; push your own values instead.

`messages/month = samples/month × streams_per_sample`, so the firmware spends on purpose:

| what | how often |
|---|---|
| `V1` level_pct | every sample |
| `V8` status | only on change |
| `V0` `V2` `V4` (the fast set) | every sample, **only if the sample interval ≥ `BLYNK_THROTTLE_S` (300 s)** |
| `V3 V5 V6 V7 V9 V14 V15 V16` (the slow set, 9 streams) | every `BLYNK_SLOW_S` (3600 s) |

Measured against the quota (30-day month; the arithmetic is shown because it is the number that decides
the deployment interval):

| sample interval | samples/month | what goes out | messages/month | % of the free quota |
|---|---|---|---|---|
| 60 s | 43 200 | throttled: `V1` + changed `V8` only | **≈43 300** | 43 % |
| 300 s | 8 640 | 8 640 × 4 fast + 720 h × 8 slow | **≈40 300** | 40 % |
| **900 s (deployment value)** | 2 880 | 2 880 × 4 fast + 720 h × 8 slow | **≈17 300** | 17 % |

Bring-up at `i 60` is survivable precisely because of the throttle: the fast set is dropped and only
`V1` + changed `V8` go out. The sketch says so once, so nobody mistakes it for a bug:

```
blynk: sampling every 60 s — only V1/V8 go out to stay inside the 100000 messages/month free quota
       (send 'i 300' for the full set)
```

(wrapped here; in `blynkPush()` it is one `Serial.printf`, printed once per boot, not once per sample)

Push and email delivery is a **separate** budget (20 per hour per automation on free), and the
firmware's own `ALARM_COOLDOWN_S` 1800 keeps a flapping level from spending it: the worst case is one
notification per event per 30 min.

---

## 6. Serial commands, the current full set

```
r  s  c  w  p  e  f  a  o  z  m  i  b  x  l  u  j  d  D  k  h

l <cm>   low alarm level                   u <cm>  high (overflow) alarm level
j        mute/unmute the buzzer            d  redraw the OLED now        (ENABLE_OLED)
D        flip the OLED 180°, saved         k  Blynk status + one push + one test event  (ENABLE_BLYNK)
m <cm>   full-scale depth (build 2: m 121.9)      z  zero the tube reading right now
```

`h` prints the firmware's own list; if this table ever disagrees with it, the sketch wins.

`d`/`D` are not compiled at all at `ENABLE_OLED 0` — typing them falls through to the help text. Same for
`k` at `ENABLE_BLYNK 0`: the `k` branch does not exist, so you get the help list. What does print in that
configuration is the stub in `blynkNotify()`, i.e. when an alarm actually tries to notify the cloud:

```
blynk: compiled out or not configured — event 'level_ok' not sent
```

With Blynk compiled in, `k` answers with one line of state and then acts:

```
blynk: library yes, token set, template TMPLAB12CD34, link up
   pushed one sample set
blynk: event 'level_ok' sent
```

`token EMPTY` / `template EMPTY` mean the defines were never pasted; `link down` is followed by
`   no link: check token, template ID, and that wifiConnect() got an IP`. The test event carries the text
`Test notification from the gauge — if this arrived, the event code is right.`, and `blynk: event '…'
sent` means only that the library accepted the call — see §5.3.

---

## 7. Bench tests, no water needed

Marked **[NOT RUN YET]** below: this code compiles and the symbols link, but it has not been flashed
and driven on hardware in this session. Run these before the box goes on the ladder.

1. **Token empty** → boot prints `blynk: OFF …`, no retries, and `k` answers `token EMPTY, template
   EMPTY, link down`. The LAN page at `http://<ip>/` still works with the WAN cable pulled. After pasting
   a real token, confirm the build actually carries the client: `blynk.cloud` must appear in the `.bin`
   (§1.1) — it is absent from the empty-token image.
2. **No OLED fitted** → `oled: nothing answering at 0x3C/0x3D …`, everything else normal, re-probe
   every 10 min (watch with `c`, then fit a panel and wait 10 min without a reboot).
3. **OLED fitted** → `d` redraws, `D` flips 180° and stays flipped after a reboot, the panel blanks
   after 120 s **even though sampling continues** (see §2), and a button press or `d` wakes it. An
   alarm must wake it by itself.
4. **Low alarm end to end, offline** → `l 130` while the level is ~120 → within one sample: the
   status band inverts, the buzzer beeps 250 ms every 9 s, with no internet involved at all.
   `j` (or a 1.5 s button hold) silences it and the silence survives a reboot.
5. **Hysteresis** → set `l` near the current level, watch the set/clear points: they must be 2 cm
   apart, and the beeping must not chatter.
6. **App side** → `k` → one point appears on `V1`, one push arrives. Then drag the `V11` slider: the
   reject line must appear for out-of-range values, and a valid value must survive a reboot.
7. **Quota** → set `i 900` for the real deployment and confirm the throttle notice is *absent*; set
   `i 60` and confirm it is *present*.

## 8. Verified vs not verified (read this before quoting anything above)

Verified here, by measurement: the six build sizes and RAM figures of §1, re-measured 2026-11-17 after
the include cleanup, including the token-pasted build; the `__has_include` library-discovery trap
(§1.1) — the evidence is the 8 336 B flash delta and the `blynk.cloud` literal being present in the
token-pasted image and absent from both the `ENABLE_BLYNK=0` image and the empty-token default image;
the fact that an empty `BLYNK_AUTH_TOKEN` makes the compiler dead-strip the cloud path (same string
test); the presence of the `BLYNK_WRITE`/`blynkPush` handlers, the 30 keys of `/api/state`, the OLED
draw coordinates (read off `oledDraw()`), the pin map.

Verified from documentation, with source: the Blynk limits table
(<https://docs.blynk.io/en/blynk.console/limits>, read 2026-11-16), the `BLYNK_TEMPLATE_ID` /
`BLYNK_TEMPLATE_NAME` requirement and `Blynk.logEvent()` semantics (Blynk Getting Started / Firmware
API pages), Blynk library 1.4.0 + BlynkNcpDriver 0.7.0 from the installed library.properties.

**Not verified** (do not present as fact):
- any real Blynk account round-trip — no token has been configured or flashed in this session;
  event delivery latency, push arrival and the exact Console wording are from documentation only;
- OLED hardware behaviour (contrast fade, burn-in, 3–8 mA, sun readability) — class-of-part
  knowledge and datasheet-style figures, **not** measured on the panel that will ship; the current
  draw of a lit SSD1306 in our box is unmeasured;
- Blynk free-plan terms after 2026-11-16, and whether Blynk's self-hosted server is maintained —
  do not plan on self-hosting;
- Amazon.in price and stock for a 0.96" SSD1306 module — see `04-parts-list.md`, marked unverified there.
