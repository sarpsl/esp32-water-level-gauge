# Phase 0 wiring — the breadboard build (rev B, SR04M ultrasonic)

Phase 0 answers three questions before any PCB is designed: does the SR04M answer and stay
stable, is the mounting geometry above the 20 cm blind area, and does WiFi reach through the
metal lid. Nothing here is permanent. Board: **ESP32-WROOM-32 DevKitC V1, 38-pin (CP2102)** —
in the Arduino IDE choose *DOIT ESP32 DEVKIT V1*.

Full design decisions: [00-design.md](00-design.md). Sensor theory and the blind-zone
arithmetic: [03-ultrasonic.md](03-ultrasonic.md). Picture: [img/01-wiring.svg](img/01-wiring.svg).

> **The gauge is an instrument, not a control.** It reads the level and alarms. It must never
> switch a pump or a valve; that interlock belongs in the pump's own float circuit.

---

## 1. The pin map

Rev B adopts the GPIO numbers from the reference project
([iotcircuithub.com](https://iotcircuithub.com/iot-based-water-level-indicator/)), so you can
follow that project's schematic with this repo. Where this build differs, it says so.

| signal | GPIO | to / from | notes |
|---|---|---|---|
| `TRIG` | **27** | → module TRIG/RX | 12 µs HIGH pulse (module wants ≥ 10 µs) |
| `ECHO` | **26** | ← module ECHO/TX | **through the divider in §2 — never 5 V direct** |
| `SDA` | **21** | OLED SDA | matches the reference project |
| `SCL` | **22** | OLED SCL | matches the reference project |
| `BTN` | **12** | push button → GND | `INPUT_PULLUP`, as in the reference build. **Strapping pin — §4** |
| `BUZZ` | **13** | → 220 Ω → transistor base | the local half of the alarm |
| `LED` | **14** | → 220 Ω → LED → GND | heartbeat, solid on when full. The reference build's green "tank full" LED |
| `VBAT` | **36** | ← 1 MΩ / 1 MΩ from the rail | ADC1_CH0, **input-only pin**, no pull-up available |

The reference project's ultrasonic pair is `D26`/`D27`; which of the two is Echo and which is
the trigger is ambiguous in its write-up. This repo fixes it as **ECHO = 26, TRIG = 27** in
`water_level_gauge.ino` — if you follow the video's photo instead and the module never
answers, swap those two wires and send `r`.

Free / deliberately unused: 4, 5, 16, 17, 18, 19, 23, 25, 32, 33, 34, 35, 39. GPIO23 is the
drop-in alternative for the button if you want a button that is safe to hold during a reset.

---

## 2. The echo divider — the one wire that kills a board

The module runs at 3.0–5.5 V. Driven from the 5 V rail its ECHO pulse is **5 V**, and the
ESP32 is not 5 V tolerant: 5 V into GPIO26 damages the pin, sometimes immediately, sometimes
after a few weeks.

```
module ECHO ──┬──[ 2 kΩ ]──┬── GPIO26
              │            │
              │         [ 3.3 kΩ ]
              │            │
             GND          GND
                    3.3/5.0 × 5.0 V ≈ 3.00 V at the pin
```

Use 1 % resistors if you have them; the divider's accuracy does not matter (it is a level
shift, not a measurement) but its presence very much does. The firmware prints a reminder on
every boot: `ECHO=26(divider!)`.

**Alternative that needs no divider:** power the module from the board's **3V3** pin instead
of 5 V. The module works down to 3.0 V, its echo is then 3.3 V and safe on a direct
connection. Cost: acoustic margin — the transmit drive is lower, so far and weak echoes (a
low, rippling water surface) fall off first. On the bench it usually still reads; on the tank
it is the difference between `no echo` and a number. Recommended build: **5 V + divider**.
The 3V3 variant is worth one test in the phase-0 plan precisely so you know what you would be
giving up.

---

## 3. The rest of the bench

| item | wiring |
|---|---|
| **OLED** 0.96" SSD1306 (4-pin: VCC GND SDA SCL) | VCC → 3V3, GND → GND, SDA → 21, SCL → 22. Auto-probed at 0x3C, then 0x3D. Cheap modules carry the I²C pull-ups on-board; a bare SSD1306 breakout without them may need 4.7 kΩ to 3V3 on each line |
| **Buzzer** 5 V piezo, active | GPIO13 → 220 Ω → BC547/S8050 base; emitter → GND; buzzer +5 → 5 V rail, − → collector. Put the buzzer's own internal diode across it if it has one, otherwise a 1N4148 cathode-to-+5 across the terminals. Driving it directly from the GPIO is possible but loud enough to be annoying and loads the pin |
| **LED** green 5 mm | GPIO14 → 220 Ω → LED → GND. Heartbeat at 1.5 s; **solid on** for the full/overflow alarm |
| **Button** tactile | GPIO12 → one leg, other leg → GND. No external resistor: `INPUT_PULLUP` provides it (same as the reference build). Short press = wake the OLED + snooze the buzzer; hold 1.5 s = mute latch |
| **Battery/5 V sense** | rail → 1 MΩ → GPIO36 → 1 MΩ → GND. GPIO36 is input-only, so no internal pull-down trick is available; keep the divider stiff enough that a floating pin cannot drift |
| **Module** | VCC → 5 V, GND → GND, TRIG/RX → 27, ECHO/TX → divider → 26. Probe on its cable, face down |

**Connect GND to the module before VCC.** The datasheet asks for that order and it matters
when the supply is mains-referenced: a signal-first connection can latch current back through
the module's ESD diodes.

Decouple the module where it sits: 100 µF electrolytic + 100 nF ceramic across VCC/GND at the
module. The 8-cycle 40 kHz burst is a real current pulse; without local capacitance the
supply dips and the ESP32 brown-outs — the classic symptom is the board rebooting whenever it
pings, which looks like a software bug.

---

## 4. Two pin traps on this board

**Do not wire anything to the pins printed `D6 D7 D8 D9 D10 D11`.** Those are the SPI flash
lines (CS, SCK, MISO, MOSI, and the flash's power-up pins). The DevKitC breaks them out and
they look perfectly usable. Touching them while the board runs usually just hangs it.

**GPIO12 (`MTDI`) is a strapping pin.** It selects the flash mode at reset and its state at
that moment matters. Because a button to GND holds it low, **holding the button while plugging
in or resetting the board stops it booting** — it will sit silent until you let go. Everything
else about GPIO12 as an input is fine, and the reference build uses it too. If you would
rather not have that trap, move the button to **GPIO23** (change `PIN_BTN`, one line) — the
docs and the diagram note both options.

Also leave alone, for reset reasons: GPIO0 (boot), GPIO2 (boot), GPIO15 (boot), GPIO9/10
(boot + flash).

---

## 5. Power for the bench

Bench: USB from the PC is enough — the module takes under 8 mA while pinging, the OLED 3–8 mA
with the panel awake, the buzzer ~30 mA while beeping, so the whole thing sits under the
500 mA a USB port offers.

Tank: 1S 18650 → CN3791 solar charge controller (Li-ion, set the cutoff to 4.20 V) → 5 V boost
converter (≥ 1 A) → board VIN. The panel and the current table are in
[00-design.md §7](00-design.md) and [img/03-power.svg](img/03-power.svg). Do not run the box
from the battery at 3.7 V directly: the module is fine down to 3.0 V but the DevKitC's regulator
headroom and the OLED's brightness both fall off, and the brown-out risk during a ping goes up.

---

## 6. Bring-up order (do it in this order, it saves an hour)

1. Power up with **nothing** attached except USB. Serial monitor at 115200, board reset — you
   want the banner and no brown-out reboot loop.
2. Fit the OLED. Boot should print `oled: SSD1306 at 0x3C`. Nothing at 0x3C/0x3D means swapped
   SDA/SCL or missing pull-ups.
3. Fit the module, **GND first**, then VCC. Send `r`. You want a pulse width and a distance
   with a hand 30 cm below the face. `no echo` → §8 of [03-ultrasonic.md](03-ultrasonic.md).
4. Send `c`, then set the calibration for the bench: `e 152.1`, `f 30.2` (or the real measured
   distances if you are already at the tank). Send `v` to see span, gate and the L/cm figure.
5. WiFi: `w YourSSID`, `p yourpassword`, reset. Read the IP. Open `http://<ip>/`.
6. Buzzer and LED: send `l 121` to force the low alarm (thresholds are range-checked against the
   span, so `l 999` is refused) — the buzzer beeps every 9 s, the LED
   comes on, the OLED status band inverts. Short press the button (snooze), hold 1.5 s (mute),
   `l 24` to put it back.
7. Optional app layer: copy `wlg_secrets.example.h` → `wlg_secrets.h`, put
   `BLYNK_TEMPLATE_ID` / `BLYNK_AUTH_TOKEN` in it, recompile, send `k`. That file is git-ignored, so
   the token never reaches the repo. Skip it entirely and the gauge still does everything locally. See
   [05-blynk-and-display.md](05-blynk-and-display.md).

---

## 7. What phase 0 must prove before a PCB is designed

| # | thing to prove | how | pass criterion |
|---|---|---|---|
| 1 | the module answers in mode 1 and holds still | 24 h log on the lid (`i 60`, capture the CSV) | ≥ 95 % accepted pings, filtered distance within ±2 cm over the day |
| 2 | the mounting clears the blind zone | measure face → full line with a tape | ≥ 30 cm, and `d_full` > 25 cm in the config |
| 3 | WiFi works with the box on a closed metal lid | `w` scan and RSSI, both lid open and shut | ≥ −80 dBm with the lid shut |
| 4 | power | run from the 18650 + panel for a week | no brown-out reboot in the log, `vbat` never under 3.4 V |
| 5 | the alarm reaches a human | force `l`, watch the buzzer, the OLED and the phone | beep within 9 s, push delivered, snooze and mute both work |

Full procedure and expected numbers: [02-phase0-test-plan.md](02-phase0-test-plan.md).
