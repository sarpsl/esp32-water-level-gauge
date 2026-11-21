# 05 — The display, the local alarm and the phone app (rev B)

Everything in this file is rev B: ultrasonic levels, ultrasonic diagnostics. The pressure-build
streams are gone (§3.2 lists what to delete in your Blynk template, §7 the retired serial
commands).

---

## 1. Two switches, and what each one costs

| switch | what it turns on | measured on this machine, today |
|---|---|---|
| `ENABLE_OLED 1` | the SSD1306 driver in `wlg_display.h` + the font in `wlg_font.h` | no external library at all; ~1 kB of code, a 1 kB framebuffer |
| `ENABLE_BLYNK 1` | the cloud layer: `BlynkSimpleEsp32.h`, 4 `BLYNK_WRITE` handlers, 4 events | **+740 B flash, +152 B RAM** (1 002 307 → 1 001 567 B measured with `--build-property compiler.cpp.extra_flags=-DENABLE_BLYNK=0`, post-fix build) |

Both default to 1 and both **switch themselves off at runtime** when the hardware or credentials
they need are missing, so leaving them at 1 cannot brick a build: no panel at 0x3C → the OLED
calls no-op; empty `BLYNK_AUTH_TOKEN` → the cloud layer stays idle and the boot banner says so.

⚠ Those figures were measured with **no `wlg_secrets.h` present** (blank template ID and token).
Most of Blynk's protocol is templates instantiated around the transport, and the domain literal is
not even present in the binary until a real `BLYNK_TEMPLATE_ID` is compiled in. Expect a meaningfully
bigger build once real credentials are in — measured on this machine at **+7.5 kB flash / +48 B RAM**
with a test template ID and token (1 008 271 B vs 1 000 783 B) — rather than reusing the rev A number
(that build was a different sketch and is quoted nowhere any more).

```
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 arduino/water_level_gauge
```

### 1.1 The Blynk library, and one trap that wasted an hour

Install it with `arduino-cli lib install "Blynk"` (Library Manager → "Blynk", Blynk Inc). The
include in the sketch is a **plain `#include <BlynkSimpleEsp32.h>` inside `#if ENABLE_BLYNK`**, and
that is deliberate. An earlier version guarded it with `__has_include()`, which looks safer and is
not: Arduino's library discovery scans the sketch's `#include` lines, and an include hidden behind
`__has_include()` is never added to the build, so the guard evaluates false and the entire cloud
layer silently compiles out **even when the library is installed**. The symptom is a gauge that
says nothing and pushes nothing while the serial log looks healthy.

So a missing library here is a **loud compile error**, not a quiet no-op. Fix it either way:
install the library, or `#define ENABLE_BLYNK 0`. Do not "improve" the guard.

The compile output is the check — Blynk must appear in the `Used library` list:

```
Used library     Version  Path
Blynk            1.4.0    ...\Documents\Arduino\libraries\Blynk
WiFi             3.3.12   ...
WebServer        3.3.12   ...
```

---

## 2. The display: 4 wires, no new GPIO, no library

| OLED pin | to |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | **GPIO21** |
| SCL | **GPIO22** |

GPIO21/22 are the I²C pair the reference project uses, so the panel and that project's schematic
agree. Nothing else in this build needs the bus — the SR04M speaks TRIG/ECHO, not I²C — so the
panel owns it: probed at **0x3C**, then **0x3D**, at boot. `oledInit()` failing is not an error:
`oledAddr` stays 0 and every drawing call becomes a no-op, and the driver re-probes every 10
minutes so a panel fitted later is picked up without a reboot.

GPIO21/22 have **no internal pull-ups** on the ESP32. Cheap SSD1306 modules carry 4.7 k–10 kΩ
on-board, and it works. A bare SSD1306 breakout with none can need a pair of 4.7 kΩ to 3V3 — the
symptom is the probe finding nothing, or I²C that works when touched and fails when left alone.

Panel limits, learned the expensive way and all of them properties of the panel:

- **Direct sunlight washes out any 0.96" SSD1306.** The phone is the noon display; the panel is
  the "what does the box think right now" display. `OLED_CONTRAST` (default `0x8F`) is the line to
  raise, and it is a knob, not a fix.
- **Contrast drifts with temperature.** Legible at 25 °C in shade, washed out at 55 °C in a roof
  box. Not a bug in the driver.
- **A static image burns in.** That is why the firmware blanks the panel after `OLED_IDLE_S`
  (120 s) and why a routine sample does **not** un-blank it — `oledRefresh()` marks the image
  dirty and `oledService()` only redraws if the panel is already awake. A 30 s sampling rate does
  not keep a panel lit all day.
- VCOMH `0x40` suits 3.3 V-driven modules; `0x20` suits 5 V ones. Those two lines in
  `wlg_display.h` are the panel knobs — geometry never needs touching.

---

## 3. What the screen shows, and why those fields

128 × 64 px, 5×7 font (6 px advance), so a row holds at most 21 characters and the width budget
decides the wording. `oledDraw()` in the sketch is the authority; this is the same layout:

```
 ┌────────────────────────────────┐
 │ 118        96.8%               │  level, 2× digits (10×14 px), + "cm"; % top-right
 │            10962 L             │  litres top-right
 │ ████████████████████░░░░       │  bar, 10 px tall, = pct of span
 │ BAT 4.02V 82%          -54 dBm │  rail voltage + RSSI (or NO WIFI)
 │ ECHO 33.8          SPAN 121.9   │  what the module measured + the calibrated span
 │ GATE 25.0 T30C        4 bad     │  the blind gate + configured air temp + rejects
 │        FULL - OVERFLOW          │  status, centred, band INVERTED if any alarm is on
 └────────────────────────────────┘
```

The rows are not decoration — they are the diagnosis order for a wrong number, in the order you
would check them:

- **`ECHO 33.8`** is the raw distance the module reported, in the same units as the calibration.
  When a level looks wrong, this is the number you compare against a tape measure. A level is an
  opinion about calibration; a distance is what the sensor saw.
- **`SPAN 121.9`** is `d_empty − d_full`. If the span is wrong, every percentage and litre figure
  is wrong in the same way, and the level may still look plausible. `v` on the serial console
  prints the full calibration.
- **`GATE 25.0`** is what the gauge is being told to ignore. If the full line is near this number,
  the gauge is blind at exactly the wrong moment — the boot banner says so too.
- **`T30C`** is the configured air temperature that sets sound speed. There is no temperature
  sensor in this build, so this is a config value, and it is on the screen because it is the
  largest single error source (≈0.27 cm per °C at full scale).
- **`4 bad`** counts rejected pings. Non-zero is normal; growing fast is the story.

Row wording is tight on purpose: `"ECHO 118.3"` + `"SPAN 121.9"` is 120 px of the 128 available,
which is why the fields are not spelled out and why the old `AIR … H2O …` rows from the pressure
build are gone rather than squeezed in.

### 3.1 Status line vocabulary

One string is produced by `statusText()` and shared by the OLED, the app's `V8` stream and the web
page, so the three can never disagree:

| status | meaning | what to do |
|---|---|---|
| `OK` | a good filtered reading, no alarm | — |
| `WARMING UP` | fewer than 3 samples since boot | wait |
| `LOW LEVEL` / `FULL - OVERFLOW` | an alarm is active | the buzzer and the button, §4 |
| `SENSOR FAULT` | 3 consecutive unusable pings — while the level is still held the panel usually shows the **reason word** instead (`NO ECHO`, `OUT OF RANGE`); this is the fallback word when there is no specific reason to name | check face, gate, cable |
| `DEW ON FACE` | a rejected near read of 2–8 cm | wipe the face, desiccate the box |
| `NEAR/BLIND` | a rejected near read of 12–25 cm | the surface is inside the gate — raise the face or lower the full line |
| `NO ECHO` | the module's 40 ms timeout fired | surface too rippled/foamy/algae-covered, or the face is not square to the water, or the module is not in mode 1 |
| `OUT OF RANGE` | a return further off than `d_empty + 15 cm` | wrong calibration, or the beam is looking past the water |
| `MANY BAD READS` | over 25 % rejected | the fault summary in [02 test plan §7](02-phase0-test-plan.md) |
| `STALE` | holding the last good value | the reading is old, not live |
| `NO READING` | the held value expired after 20 bad pings — there is no level to show any more | the sensor fault sequence; module, cable, R19 |

---

## 4. The local half: thresholds, buzzer, button (works with no internet)

The buzzer and the OLED are the half of the alarm that does not care whether Blynk, the router or
the internet exist. That is the whole reason they are on the box: an alarm you can only hear
through somebody else's cloud is not an alarm.

```
low      level ≤ 24.0 cm   (≈20 % of 121.9 cm)      slider V11, serial 'l'
high     level ≥ 110.0 cm  (≈12 cm under the overflow)  slider V12, serial 'u'
hysteresis  2.0 cm          an alarm only CLEARS 2 cm the other way
cooldown    1800 s          at most one notification per event per 30 min
buzzer      250 ms every 9 s: 1.8 kHz low, 2.6 kHz high and fault
LED         heartbeat at 1.5 s; solid ON for the high/overflow alarm
```

Two guards that are easy to get wrong and are already handled:

- **warm-up guard** — nothing is declared faulty until the median filter has produced its first
  reading, so a reboot does not beep and push on its own way.
- **fault streak** — `FAULT_STREAK 3` consecutive `no echo` / `out of range` pings raise the fault;
  a run of `too close` rejections deliberately does **not**, because "the tank is genuinely over the
  full line" and "dew on the face" both look identical to the sensor and only one of them is a fault.
  Meanwhile the displayed level holds the last good value for up to 20 samples with a `STALE` label
  rather than dropping to a believable zero — and once those 20 expire the number goes away
  (`NO READING`): a held level stops pretending to be live.

The button is the reference build's "push button to stop the buzzer", with debounce and a latch
added:

| press | does |
|---|---|
| short (< 1.5 s) | wakes the OLED, **snoozes** the buzzer for one 9 s period |
| hold (> 1.5 s) | toggles the **mute latch** (stored in NVS — a 3 a.m. beep can be switched off without a phone) |

Debounce is not optional: an undebounced button makes the buzzer stutter and the panel flicker, and
an edge-only handler cannot tell a short press from a long one.

⚠ GPIO12 is the `MTDI` strapping pin. Holding the button while the board resets leaves it in the
wrong boot mode — silent until you let go. GPIO23 is the safe alternative (`PIN_BTN`, one line).

---

## 5. Blynk: template, streams, events, and the quota that stops a gauge dead

### 5.1 Setup, in order

1. [blynk.cloud](https://blynk.cloud) → create a **Template** for ESP32, then a **Device** from it.
2. Console → your device → **Info** → copy `BLYNK_TEMPLATE_ID`, and the **Auth token**.
   `BLYNK_TEMPLATE_NAME` is the template's name, exactly as typed. Blynk IoT 2.x will not compile
   without ID and name.
3. Put them in **`arduino/water_level_gauge/wlg_secrets.h`** — copy `wlg_secrets.example.h` and fill
   it in. The sketch pulls that file in with `#if __has_include("wlg_secrets.h")`, so its presence
   switches the cloud layer on and its absence leaves a LAN-only gauge; no editing of the `.ino`
   needed, and it is in `.gitignore` on purpose.
   **The token is a credential**: whoever holds it can read *and* write this device. Never put a real
   token in a tracked file — a credential that reaches a public repo's history stays in the history,
   so rotate it in Console (Device → Info) if one leaks. Do not commit `wlg_secrets.h`; check
   `git status` shows it as ignored.
   The stock cloud endpoint is port **80 plain TCP** — same as the stock examples — so it is not a
   secret channel; the SSL variant (`BlynkSimpleEsp32_SSL.h`) is a one-line change if the LAN is
   not trusted. Self-hosted: set `BLYNK_SERVER` / `BLYNK_SERVER_PORT` in the same file.
4. Create the datastreams (§5.2) and the events (§5.3) in the **Template**, not the device.
5. Flash, then send `k`. That prints the link state, pushes one sample set, and fires a test event —
   if the phone did not buzz, the event code is wrong, not the radio.

### 5.2 The 13 live datastreams (free template allows 50)

| pin | name | type | direction | sent |
|---|---|---|---|---|
| V0 | `level_cm` | Double | read | when the sample interval ≥ `BLYNK_THROTTLE_S` (300 s) |
| V1 | `level_pct` | Double | read | **every sample** — the one stream that never throttles |
| V2 | `litres` | Double | read | ≥ throttle |
| V3 | `dist_cm` | Double | read | hourly (raw face→surface distance) |
| V4 | `vbat` | Double | read | ≥ throttle |
| V8 | `status` | **Text** | read | on change only |
| V9 | `rssi` | Integer | read | hourly |
| V10 | `sample_s` | Integer 5…7200 | **write** | app → device (Slider) |
| V11 | `low_cm` | Double 0…span | **write** | app → device (Slider) |
| V12 | `high_cm` | Double 0…span | **write** | app → device (Slider) |
| V13 | `mute` | Integer 0/1 | **write** | app → device (Switch) |
| V14 | `bad_pct` | Double | read | hourly — % of pings rejected |
| V15 | `uptime_h` | Double | read | hourly — reboots show up here |

`BLYNK_CONNECTED()` calls `Blynk.syncVirtual(V10, V11, V12, V13)` — only the four writable streams,
never `syncAll()`, because every synced value is a metered message. After a reboot **the app's
values win over the sketch defaults**, which is what makes the sliders useful. The handlers validate
ranges and reject nonsense out loud:

```
blynk V11: out of range, ignored
blynk V12: high alarm = 110.0 cm
```

### 5.3 The 4 events (free template allows 5)

Create these codes exactly; `Blynk.logEvent(code, text)` sends them by code and the text lands in
the push:

| code | when | text the firmware sends |
|---|---|---|
| `level_low` | level falls to/below `low_cm` | `Tank low: 18 cm (15 %), about 1430 L left. Alarm at 24 cm.` |
| `level_high` | level rises to/above `high_cm` | `Tank high: 110 cm (90 %) — check the inlet, overflow risk.` |
| `sensor_fault` | 3 consecutive unusable pings | `Gauge fault: NO ECHO. The level shown may be stale.` |
| `level_ok` | any of the above clears | `Level back to 61 cm (50 %).` / `Gauge reading again: OK.` |

If a code does not exist in the template the cloud answers *event not found* and nothing arrives on
the phone, while the serial log still says `blynk: event 'level_low' sent` — that line only means
the request left the device. **Four of the five allowed events are used**, so there is no fifth
event to add without deleting one.

### 5.4 How it connects, and why not `Blynk.begin()`

The sketch already owns WiFi — STA with an AP fallback (`WaterLevel-Setup` / `waterlevel` on
192.168.4.1), its own retry, and a web server on the same radio. `Blynk.begin(auth, ssid, pass)`
would block in an endless `while()` until it connects and take the radio with it. So:

```cpp
Blynk.config(auth);            // or Blynk.config(auth, BLYNK_SERVER, BLYNK_SERVER_PORT)
Blynk.connect(4000);           // bounded — never an endless wait
...
Blynk.run();                   // called from loop(), next to server.handleClient()
if (!Blynk.connected() && now - tRetry > 30000) Blynk.connect(4000);
```

A dead Blynk link costs nothing: the gauge, the OLED, the buzzer and `http://<ip>/` keep working.
The log line is explicit — `blynk: link down — the gauge and the CSV keep running regardless`.

### 5.5 Message quota — the number that can stop a gauge dead

Blynk's free plan is **100 000 device messages per calendar month per device**. A message is one
stream write, so:

```
messages/month = samples/month × streams_per_sample + status_changes + hourly_diagnostics
```

Measured against this firmware's throttle (`BLYNK_THROTTLE_S 300`, `BLYNK_SLOW_S 3600`, 4 hourly
diagnostic writes, ~5 status changes a day), month = 2 630 000 s:

| sample interval | samples/month | streams/sample | ≈ messages/month | % of quota |
|---|---|---|---|---|
| 60 s (bring-up default) | 43 830 | 1 (throttled: V1 only) | **≈ 45 000** | 45 % |
| 300 s | 8 767 | 4 (V0 V1 V2 V4) | **≈ 38 000** | 38 % |
| **900 s — recommended** | 2 922 | 4 | **≈ 15 000** | 15 % |
| 3600 s | 730 | 4 | **≈ 6 000** | 6 % |

Without the throttle — eight streams on every sample at 60 s — that is ≈ 350 000 messages and the
quota is gone in **under ten days**, after which the device is throttled and the dashboard freezes
while the gauge keeps working perfectly. That is the trap: it fails as a *display* problem, not a
sensor problem, and mid-month. So the fast path writes only `V1` (plus `V8` on change), the fuller
set starts at 300 s, and diagnostics go hourly. The sketch says so out loud once, at boot:

```
blynk: sampling every 60 s — only V1/V8 go out to stay inside the 100000 messages/month free quota (send 'i 300' for the full set)
```

Deployment: `i 900`. Bring-up: leave 60 s and accept that you are watching quota, not litres.

---

## 6. The LAN web page (works with no internet at all)

| route | what |
|---|---|
| `/` | dark test page, refreshes every 2 s: status, level, bar, echo distance + reason, gate, sound speed, calibration, battery, RSSI, good/total pings, alarm settings, uptime |
| `/api/state` | JSON, all fields below |
| `/api/log` | forces one sample and returns `logged` (also prints the CSV line) |
| `/api/scan` | WiFi survey: `SSID,RSSI` per line |

```json
{"fw":"B ultrasonic SR04M","valid":1,"level_cm":118.3,"pct":97.0,"litres":10993,"dist_cm":33.8,
 "echo_us":1920,"echo_reason":"ok","air_t":30.0,"sound_mms":349.5,"vbat":4.02,"vpct":82,
 "rssi":-54,"uptime":86400,"bad":4,"n":1440,"n_near":2,"n_noecho":1,"n_far":1,"gate_cm":25.0,
 "span_cm":121.9,"status":"OK","al_low":0,"al_high":1,"al_fault":0,"low_cm":24.0,"high_cm":110.0,
 "mute":0,"d_empty":152.1,"d_full":30.2,"area_cm2":92903,"sample_s":900}
```

`echo_reason` is the verdict of the **most recent ping** (`ok`, `too close`, `no echo`,
`out of range`, `jump rejected`); `status` is the human summary. They are different fields on
purpose: the last ping can be fine while the status says `MANY BAD READS`.

---

## 7. Serial commands — the current full set

`h` prints this. Rev A's sensor-specific commands are gone (`m` measurement mode, `z` zero-diff,
`o` onewire read, and the `b`/`a` that meant barometric offsets); letters now mean one thing each.

| command | does |
|---|---|
| `r` | one raw ping: pulse width, distance, verdict, gate, air temp. `no echo` prints the four things to check |
| `s` | the JSON state |
| `c` | take one sample now, print the CSV line |
| `v` | print calibration + config: `d_empty`, `d_full`, span, gate, air temp, L/cm, alarm settings, streak counters |
| `w` | WiFi scan; `w <ssid>` sets the SSID |
| `p <pass>` | set the WiFi password |
| `e <cm>` / `f <cm>` | face distance with the tank empty / full — **the whole calibration** |
| `a <cm2>` | tank area (92903 = 10 ft × 10 ft → 92.9 L/cm) |
| `g <cm>` | blind gate: reject anything closer (module's own blind area is 20 cm) |
| `t <degC>` | air temperature → sound speed. No sensor for this in this build, so it is a config value |
| `i <sec>` | sample interval |
| `b` | battery voltage and % |
| `l <cm>` / `u <cm>` | low / high alarm level (also re-evaluates the alarm immediately) |
| `j` | mute / unmute the buzzer |
| `d` / `D` | redraw the OLED now / flip it 180° |
| `k` | Blynk link status + one test push + one test event (not compiled at all when `ENABLE_BLYNK 0`) |
| `x` | erase NVS and reboot |

The CSV line the plan tells you to log, in this exact order:

```
ts_s,echo_us,raw_cm,reason,filt_cm,level_cm,pct,litres,air_t,gate_cm,vbat,rssi,bad,n
```

Capture: `arduino-cli monitor -p COM6 -c baudrate=115200 | tee wlg.csv`, then strip the banner
lines before analysis.

---

## 8. Bench tests, no water needed

The full plan with pass criteria is [02-phase0-test-plan.md](02-phase0-test-plan.md); these are
the four that specifically exercise the app and display layers:

| send | expect |
|---|---|
| `k` | `blynk: library yes, token set, template TMPL…, link up` + a push that arrives on the phone + one test event. If the link says down: token, template ID, and whether `wifiConnect()` got an IP |
| `l 121` | low alarm: buzzer within 9 s, status band inverted, `level_low` push. Then `l 24` to restore. Thresholds are range-checked 0…span like V11, so on the 121.9 cm default span `l 121` is the forcing value and `l 999` is refused |
| `u 0` | high alarm: LED solid on, `level_high` push. Then `u 110` |
| `j` / button short / button hold | mute state changes, reported on the serial line, and the buzzer actually stops |
| `d`, `D` | redraw, and flip the panel 180° (a panel mounted upside-down in a cowl reads upside-down) |
| pull the module, wait 4 samples | `STALE` for the first two rejects, then `NO ECHO` + `sensor_fault` push, the level held for 20 samples. Plug it back → `level_ok` |

---

## 9. Verified vs not verified

**Verified on this machine today:** the sketch compiles for
`esp32:esp32:esp32doit-devkit-v1` with no warnings — 1 002 307 B flash (76 %), 50 652 B RAM (15 %)
with the Blynk layer compiled in, 1 001 567 B / 50 500 B with `-DENABLE_BLYNK=0`; Blynk 1.4.0 appears
in the `Used library` list; the OLED driver needs no external library; the quota table's arithmetic
is this firmware's own throttle logic plus Blynk's published 100 000-message free allowance.

**Not verified** (do not quote as fact): any runtime behaviour — nothing has been flashed in this
session. The message counts in §5.5 are arithmetic on the throttle rules, not a month of real
traffic. Blynk's plan limits change; re-check them in Console. And the event/push text above is what
the code formats, not what a handset showed.
