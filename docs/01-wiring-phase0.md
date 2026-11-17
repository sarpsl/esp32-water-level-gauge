# 01 — Phase 0 wiring and Arduino sketch

Code: `arduino/water_level_gauge/water_level_gauge.ino` — **plain Arduino sketch. The core build needs
no libraries at all**: the OLED driver and its font are in the project (`wlg_display.h`,
`wlg_font.h`). The only exception is the optional phone app — if `ENABLE_BLYNK` is 1 you must install
the Blynk library, or set it to 0; the build fails loudly rather than quietly shipping a gauge that
never posts. Details: [`05-blynk-and-display.md`](05-blynk-and-display.md) §1.
Arduino IDE: Boards Manager → install **“esp32 by Espressif Systems”**, then select **DOIT ESP32 DEVKIT V1**
(or *ESP32 Dev Module*, Flash 38 pin). Open the `.ino`, pick the COM port, Upload. Serial Monitor 115200.

Visual: [`img/01-wiring.svg`](img/01-wiring.svg) / [`img/01-wiring.png`](img/01-wiring.png) — full pin map with
the echo divider drawn out. [`img/03-power.svg`](img/03-power.svg) is the phase‑1 power tree.

## Pin map (verified against the DevKitC V1 / NodeMCU‑32S 38‑pin layout)

Left column, top→bottom: `3V3 EN 36 39 34 35 32 33 25 26 27 14 12 GND 13 D9 D10 D11 VIN`
Right column, top→bottom: `GND 23 22 1 3 21 GND 19 18 5 17 16 4 0 2 15 D8 D7 D6`

| GPIO | Net | Dir | Connects |
|---|---|---|---|
| 25 | `TRIG` | out 3.3 V | JSN‑SR04T **T** — 3.3 V pulse triggers the 5 V module fine |
| 26 | `ECHO` | in 3.3 V | JSN‑SR04T **E** **through the 2 kΩ/3.3 kΩ divider** |
| 21 | `SDA` | I²C | BME280 **and** the optional OLED share this wire — `0x76`/`0x77` for the BME280/BMP280, `0x3C`/`0x3D` for the SSD1306; the modules carry their own pull-ups, add none |
| 22 | `SCL` | I²C | as above (`Wire.begin(21,22)` runs once, everything shares that one bus) |
| 27 | `ONEWIRE` | 1‑Wire | DS18B20 DQ + 4.7 kΩ pull‑up |
| 36 | `VBAT` | ADC1_CH0, input only | battery divider 1 MΩ/1 MΩ (phase 1; skip for now) |
| 33 | `LVL_ANA` | ADC1_CH5 | spare — analog sensor option only |
| 18 | `LED_HB` | out + 330 Ω | heartbeat LED |
| 23 | `BTN` | in, pull‑up | service button — short press wakes the OLED and snoozes the buzzer one period; **hold 1.5 s** toggles the mute (saved) |
| 17 | `BUZZ` | out | piezo via S8050 (optional) — 250 ms tone every 9 s while an alarm is on and unmuted: 1.8 kHz low, 2.6 kHz high/fault |
| 19 | `FLOW` | in, pull‑up | reed contact on a pipe (optional) |
| VIN | 5 V in | power | USB 5 V now; boost converter later. **The ultrasonic module runs off VIN, not 3V3** |
| 3V3 | 3.3 V out | power | BME280, DS18B20 pull‑ups |
| GND | — | power | common with every module |

Pins deliberately left empty: 32 (ADC2 while WiFi is up), 34/35/39 (spare ADC1), 16 (strapping),
4 (boot log), 0 (boot mode), 2 (strapping + the carrier board LED), 15 (boot log), 12 (flash voltage), 14.
Never wire: **1, 3** (CP2102 serial console) and **6, 7, 8, 9, 10, 11** — the DevKitC breaks the SPI‑flash
lines out on both sides of the 38‑pin board. Connecting anything there stops the board from booting.

## Echo divider — mandatory

```
ECHO ──┬──[2 kΩ]──┬──[3.3 kΩ]──┬── GND      5 V × 3.3/5.3 = 3.00 V
       │          └→ GPIO26                  abs max on an ESP32 pin = 3.6 V
      [100 nF]───────┘           GPIO26 goes HIGH for the whole echo width, so no AC coupling
```
Feeding 5 V into GPIO26 destroys the pin and usually the whole chip. Do the divider first, then wire power.

## Sensor module wiring (JSN‑SR04T V3, 4‑wire)

| module wire | colour | to |
|---|---|---|
| V | red | VIN / 5 V rail |
| G | black | GND |
| T | yellow | GPIO25 |
| E | white | output of the divider above |

Twist T+E together, keep that pair under 2 m, and put 100 µF across V+G at the module.

## Power

Bench: USB‑C → VIN, the board's AMS1117 makes 3V3. Average board current with WiFi connected is
20–25 mA, so the LDO dissipates ≈40 mW — fine on the bench and fine in a ventilated box. It only
gets hot if you sit *right on top of it* inside a sealed foam‑lined box. Final build:
`img/03-power.svg` — solar → 18650 charger → 2× 18650 parallel → boost 5 V → VIN.

## Carrier board caveat

Those 38‑pin “ESP32S” carrier boards usually wire an onboard LED to **GPIO 2** and a button to
**GPIO 0** — both strapping pins. That is why the sketch uses **GPIO 18** for heartbeat and **GPIO 23**
for the button. Check your board's silkscreen against the diagram before powering up. If the carrier
board's layout differs from the DevKitC column order above, re-check: it is the silkscreen label that
matters, not the physical position.

## Optional OLED on the box — 4 wires, no new GPIO, no library

| OLED pin | to |
|---|---|
| VCC | 3V3 — these modules are 3.3 V I²C parts, do **not** feed VIN |
| GND | GND |
| SDA | GPIO21 (the BME280's SDA — same two wires) |
| SCL | GPIO22 |

Auto-probed at boot (`0x3C`, then `0x3D`). If nothing answers, boot prints
`oled: nothing answering at 0x3C/0x3D — display code stays idle`, everything else is unaffected, and
the bus is re-probed every 10 min, so fitting a panel later needs no re-flash. Serial `d` redraws,
`D` flips 180° (saved). **In a roof box an SSD1306 is unreadable in direct sun and burns in** — put
the window on the shaded side and leave the 120 s blanking on
([`05-blynk-and-display.md`](05-blynk-and-display.md) §2, and the mounting figure).

## Serial commands (115200 8N1)

```
r  s  c  w  p  e  f  a  o  z  m  i  b  x  l  u  j  d  D  k  h

r one raw sensor read                        s JSON status only
w (bare) WiFi scan / RSSI test               w <ssid>  set SSID        p <pass>  set password
e <cm> empty distance   f <cm> full distance   (build 1: ultrasonic)
a <cm2> tank area (92903)                    m <cm> full-scale depth   (build 2: m 121.9)
o <cm> baro zero offset                      z zero the tube reading right now
i <sec> sample interval                      b battery voltage
l <cm> LOW alarm level (default 24)          u <cm> OVERFLOW alarm level (default 120)
j buzzer mute on/off                         x erase NVS and reboot
h this help
d redraw the OLED now                        D flip the OLED 180°, saved
k Blynk link state + one push + one test event
```

`d`/`D` are not compiled at all with `ENABLE_OLED 0`; `k` is not compiled at all with
`ENABLE_BLYNK 0`. This is the firmware's own `h` output, so if this table ever disagrees with the
sketch, the sketch wins.

## Alarms — what the box does on its own

No internet needed for any of this; the phone push is a copy of the same decision, not the source of it.

| what | value | where it lives |
|---|---|---|
| low level sets the alarm | `DEF_LOW_CM` 24 cm (≈20 % of the tank) | change with `l <cm>`, saved |
| overflow level | `DEF_HIGH_CM` 120 cm | change with `u <cm>`, saved |
| hysteresis | 2 cm — a low alarm sets at `low` and only clears at `low + 2` | `ALARM_HYST_CM` |
| notification cooldown | 30 min per event | `ALARM_COOLDOWN_S` |
| buzzer | 250 ms every 9 s; 1.8 kHz low, 2.6 kHz high/fault | `BUZZ_PERIOD_MS`, `BUZZ_ON_MS` |
| mute | survives a reboot; `j`, a 1.5 s button hold, or the app switch | NVS key `mute` |

A fault (`TUBE SENSOR MISSING`, `NO VALID READS`, `TUBE OVER RANGE`) is a **separate** alarm with its
own event and its own pitch, and nothing is declared faulty until the first valid filtered reading
exists — a reboot cannot cry wolf. The logic is `evaluateAlarms()`; the beep is `buzzerService()`;
the screen banner and the app text use the same `statusText()` word as `/api/state`’s `status` key.

## Web (phase 0 test page only)

`http://<ip>/` live page · `/api/state` JSON · `/api/log` force a sample · `/api/scan` WiFi scan.
`/api/state` now also carries `status`, `al_low`/`al_high`/`al_fault`, `low_cm`, `high_cm`, `mute`, so
a page or dashboard of your own can show what the box is alarmed about. The CSV log schema is
unchanged — status/alarm fields are deliberately not in the CSV, so old parsers keep working.
If WiFi credentials are wrong or empty it opens AP **WaterLevel-Setup** / **waterlevel** on 192.168.4.1.
