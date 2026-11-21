# External code review — esp32-water-level-gauge (rev B)

> **Status 2026-11-20: fixed.** B1–B6 are all addressed in the working tree; the matching doc
> updates and the fix log are in `docs/06-changelog.md` §10. Unaddressed remainders: S1
> (`URS_JUMP` never emitted), S3 (dropped notifications), S4 (no alarm re-notify), S5 (periodic
> scan churn), S6 (no wall-clock timestamps) — logged as enhancement candidates, not phase-0 bugs.

**Reviewed:** 2026-11-20 · reviewer: pi coding agent (read-only review; no files were modified to produce it)

**Scope:** `arduino/water_level_gauge/` (`water_level_gauge.ino` 1 137 lines, `wlg_display.h`,
`wlg_font.h`, `wlg_secrets.example.h`), all of `docs/`, `README.md`, `LICENSE`,
`THIRD_PARTY_NOTICES.md`, git history. Claims in the docs were verified against the toolchain
installed on this machine: **arduino-cli 1.6.0, esp32 core 3.3.12, Blynk library 1.4.0**
(sources read where behaviour was in question).

**Reproduction:** `arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1` reproduces the
documented result exactly — **1 000 783 B flash (76 %), 50 652 B RAM, zero warnings.**
The build-size and no-warnings claims in `README.md` / `02-phase0-test-plan.md` are accurate.

---

## Verdict

**Concept A · documentation A+ · execution B+.**

An unusually well-engineered hobby project: the design docs are a contract, not marketing;
the failure-mode tables, the rev A→B "no remnants" grep checklist, the Blynk quota arithmetic,
and the credit/licensing trail (including the Adafruit font carve-out) are better than most
commercial embedded repos.

However there are **four real defects plus two robustness gaps**, two of which are
field-killers for a solar-powered box on a roof: the OLED will never be found on ESP32
core 3.x, and a power blip while the router reboots strands the device in AP mode until
someone presses reset. Both appear only *after* install — exactly where the current phase-0
test coverage does not reach.

---

## Bugs (evidence-backed, ranked)

### B1 — `Wire.begin()` is never called: the OLED will never be found on ESP32 core 3.x

`setup()` never calls `Wire.begin()`, and `wlg_display.h` probes I2C at boot
(`oledInit()` → `i2cProbe()`).

- On core **2.x** this survived because `TwoWire::beginTransmission()` lazily initialized the
  bus.
- In the installed core **3.3.12** (`packages/esp32/hardware/esp32/3.3.12/libraries/Wire/src/Wire.cpp`):
  the constructor sets `txBuffer(NULL)` (Wire.cpp:41–54); the buffer is allocated only via
  `allocateWireBuffer()` from the `begin()` path (Wire.cpp:144–157, 249); and
  `endTransmission()` returns error 4 with *"NULL TX buffer pointer"* (Wire.cpp:453–456).

So `i2cProbe()` fails forever → the boot banner reports
`oled: nothing answering at 0x3C/0x3D` even with a healthy panel, and the 10-minute re-probe
never helps. Side effect: `PIN_SDA`/`PIN_SCL` are decorative — the pins are never passed to Wire.

**Fix:** `Wire.begin(PIN_SDA, PIN_SCL);` in `setup()` before `oledSetup()`.

### B2 — "hold last-good for 20 samples" is unreachable; stale readings live forever

In `filterUpdate()`, an invalid ping leaves the ring untouched — but `medianOf()` still computes
over the stale ring. The median of frozen data equals `flt.good`, so the *within-MAX_JUMP* branch
runs and **re-arms `flt.hold = HOLD_SAMPLES` on every invalid sample**. Consequences:

- `rd.valid` never goes false once the filter has 3 samples; the `HOLD_SAMPLES` counter is dead
  code after warm-up.
- `NO READING` (after 20 stale samples) can never appear — the gauge holds the old level
  **indefinitely**, and `blynkPush()` keeps streaming `rd.pct` as if live.
- `statusText()` shows `DEW ON FACE` / `NO ECHO` / `OUT OF RANGE` / `NEAR/BLIND` only when
  `!rd.valid`; after warm-up that branch is unreachable, so the diagnostic vocabulary promised in
  `05-blynk-and-display.md` §3.1 collapses to `OK / LOW LEVEL / FULL - OVERFLOW / SENSOR FAULT /
  MANY BAD READS` in real use.
- **The `STALE` label — promised in `README.md`, test plan T10 and doc 05's status table — does
  not exist in the code at all.** T10 as written will fail its own expectation (`NO ECHO` vs the
  actual `SENSOR FAULT`).

Fault *detection* still works (via `flt.streakHard`), so this is display/status honesty, not
alarm loss — but the docs describe a mechanism the firmware cannot perform.

**Fix:** in `filterUpdate`, decide staleness from *samples since the last valid ping*, not from a
median of the frozen ring; emit a `STALE` status (or correct the three docs that promise it).

### B3 — AP fallback is a trapdoor; a power blip while the router reboots strands the device

`wifiConnect()` falls back to `WiFi.mode(WIFI_AP)`. The loop's 20 s STA retry is gated on
`WiFi.getMode() == WIFI_STA` — so once AP fallback fires, **the STA is never retried until a
manual reset**.

Real scenario: a grid blip reboots the ESP32 (ready in ~1 s) while the router takes 60 s to come
back → AP fallback → Blynk permanently silent, the LAN page unreachable from the house, and the
buzzer gamely beeping stale data — until someone climbs to the roof and presses reset. The retry
code exists; the mode check neuters it.

**Fix:** retry with `WiFi.mode(WIFI_STA)` before `WiFi.begin()`, or use a `WIFI_AP_STA` fallback
with an AP timeout.

### B4 — Latent crash (fires the moment B3 is fixed naively): `Blynk.connect()` without `Blynk.config()`

`blynkSetup()` returns early — *without* calling `Blynk.config()` — if WiFi is down at boot
("will try once the link is up" — but nobody calls `config()` later). `blynkService()` then calls
`Blynk.connect(4000)` raw.

In the installed Blynk 1.4.0 library: `authkey` is initialized to **NULL**
(`src/Blynk/BlynkProtocol.h:35`) and only `begin(auth)` sets it (line 118); the run loop then does
`sendCmd(BLYNK_CMD_HW_LOGIN, 1, authkey, strlen(authkey))` (line 228) → `strlen(NULL)` →
Guru Meditation bootloop on ESP32.

Today this is masked by B3: after AP fallback, `WiFi.isConnected()` never becomes true, so the
bare `connect()` is never reached. Fixing B3 alone converts a silent failure into a crash loop —
**B3 and B4 must ship as one change.**

**Fix:** a `blynkNeedsConfig` flag — call `Blynk.config(...)` from `blynkService()` before the
first `connect()`.

### B5 — Serial numeric commands validate nothing; Blynk's counterparts validate everything

- `i` with no/bad argument → `sampleSec = 0` → the loop samples **every iteration** (~60 ms-paced
  continuous pinging + Blynk V1 spam → quota incineration).
- `e` / `f` with no argument → distance 0 → negative span, garbage levels. The boot guard in
  `cfgLoad()` (span < 20 → fallback) only catches it *after reboot* — the test plan's own guard
  test (T6) requires a reset to confirm it.
- `u` with no argument → high alarm fires instantly.
- `a`, `g`, `t`, `l` — same pattern.

Meanwhile Blynk V10–V12 range-check properly (`5..7200`, `0..span`). **Fix:** reuse the same
guards in the serial console.

### B6 — A sensor dead from first boot never raises the fault alarm

The warm-up guard in `evaluateAlarms()` bails (`else return;`) forever until the *first valid*
reading. A cold install with an unplugged module (or `R19` populated — the docs' own #1 gotcha)
shows `NO ECHO` on the OLED but **never buzzes and never pushes**.

**Fix:** judge faults after N total pings regardless of validity, once `totalCount` passes a
grace window (the guard's stated purpose — silence on reboots — is preserved).

---

## Smaller items / doc↔code drift

| # | item | where |
|---|---|---|
| S1 | `URS_JUMP` ("jump rejected") exists in the enum and `reasonText()`, and the changelog claims rejection reasons are carried into the log — but **no code path ever emits it**; jump rejection is invisible in the CSV (`filt_cm` just refuses to move). | `water_level_gauge.ino` |
| S2 | `vbat` measurement node is ambiguous: "from the rail" (`01-wiring` §) vs T15's "never under 3.4 V" which only makes sense at the **battery**. Measure the boosted 5 V rail and `vbatPct()` pins at 100 %. One sentence settling "sense the battery side, before the boost" matters for the phase-1 PCB silkscreen. | `docs/01-wiring-phase0.md`, `docs/02` T15 |
| S3 | Missed notifications are dropped, not queued: `sentLow` is latched even when `blynkNotify()` prints "not linked" — a low-tank event during a net outage is never delivered after recovery. | `setLowAlarm()` / `blynkNotify()` |
| S4 | No re-notification while an alarm *persists*. Anti-spam is right, but a tank still low 8 h later never re-pings. Worth a deliberate decision (e.g. one reminder per 12 h). | `evaluateAlarms()` |
| S5 | The every-10-min `wifiScanPrint()` blocks `WiFi.scanNetworks()` for seconds and briefly drops STA — churn against the Blynk link every 10 min in deployment. Gate it behind a bring-up flag. | `loop()` |
| S6 | CSV timestamps are `millis()`-based; syncing `time()` (Blynk can provide it) would make the 24 h logs much easier to analyze. | CSV / T11 |
| S7 | V15 `uptime_h` resets at the 49.7-day `millis()` wrap (cosmetic; unsigned diffs elsewhere are rollover-safe). | `blynkPush()` |
| S8 | `l` / `u` printf is missing `\n`, leaving an incomplete console line (cosmetic). | `handleSerial()` |
| S9 | Unauthenticated web server + fixed AP password `waterlevel` — acknowledged for phase 3; fine for phase 0, but note `/api/log` and `/api/scan` are also unauthenticated on the LAN. | `httpRoutes()` |
| S10 | Test plan T10 expects status `NO ECHO` during the fault; the code shows `SENSOR FAULT` (because B2 keeps `rd.valid` true). The test as written fails its own expectation. | `docs/02` T10 |

---

## What is genuinely good (keep as the project's signature)

- **Electrical judgment:** the echo divider (the reference build drives 5 V into a
  non-5V-tolerant ESP32 — this project fixes it and says so explicitly), GPIO12 strapping warnings
  everywhere, "never touch D6–D11", GND-before-VCC order.
- **Sensor-perspective correctness:** `pulseIn` ceiling at 45 ms so the module's own 40 ms timeout
  fires first (`NO_ECHO_US 39500` is exactly right), 60 ms min ping gap enforced centrally inside
  `pingRaw()`, gate at 25 cm above the module's 20 cm blind zone, reject-don't-smooth, and
  beam-cone arithmetic proving no wall echo. This is the physics most blog builds get wrong.
- **Failure taxonomy with distinct reasons** (`near / no-echo / far`) carried through CSV, web JSON
  and OLED — genuinely useful when it's 40 °C on the roof (see B2 for why this is currently
  neutered).
- **Alarm discipline:** hysteresis + cooldown + local-first buzzer + warm-up guard.
  *"An alarm you can only hear through somebody else's cloud is not an alarm"* — correct principle,
  correctly implemented.
- **Secrets hygiene:** git-ignored `wlg_secrets.h` + `__has_include`, WiFi credentials in NVS only,
  token-as-password framing with rotation instructions — and a *measured* explanation of why the
  Blynk include must not hide behind `__has_include` (Arduino library discovery). Hard-won
  knowledge, written down.
- **Zero-dependency OLED driver** that is competent (charge pump, 32-byte I²C chunks, page-major
  buffer matching the panel, software flip) — works once B1 lands.
- **Documentation process:** design record as contract, phased gates, test plan with expected
  numbers and failure triage, changelog with rationale, BOM with price sanity-checks, no-remnants
  grep list. Reproducible build claims that reproduce exactly.
- **Clean git history** with self-explanatory, scoped commits; honest licensing carve-out for
  `wlg_font.h` (Adafruit BSD stays in-file).

---

## Recommended next steps (priority order)

1. **B1:** `Wire.begin(PIN_SDA, PIN_SCL);` — one line, un-breaks the OLED on core 3.x.
2. **B2:** rework the hold/stale logic; emit `STALE` or correct the three docs that promise it.
3. **B3 + B4 together:** STA retry after AP fallback *and* `Blynk.config()` before first
   `connect()`. Then add a **T16: boot the ESP32 before the router** to the test plan — the
   current 15 tests do not cover this ordering anywhere.
4. **B5:** range-validate the serial console (`e f a g i t l u`), reusing the Blynk guards.
5. **B6:** fault alarm must not require a first valid reading forever.
6. CI: a tiny GitHub Actions `arduino-cli compile` job — the repo already documents the exact
   FQBN and expected sizes; freeze the "76 %, zero warnings" promise against future edits.
7. Extract `filterUpdate()` / calibration maths / `vbatPct()` into a pure header and unit-test it
   natively (the host-side logic does not need an ESP32).
8. Phase-1 PCB: adopt the docs' own $2 I²C temperature-sensor suggestion for sound-speed
   compensation — it also buys box temperature for dew forecasting, the #1 reject cause at night.

---

*Reviewed at commit `5fb1db6` ("docs: fix the one path that spelled the folder name, and name the
repo"), working tree clean. No files in the repository were modified to produce this review.*
