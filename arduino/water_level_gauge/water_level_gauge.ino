/* ============================================================================
 *  TANK WATER LEVEL GAUGE — phase 0 bring-up sketch  (Arduino IDE)   rev B
 *
 *  Sensor: **SR04M-family waterproof ultrasonic module** (sold in India as SR04M,
 *          SR04M-2, AJ-SR04M, JSN-SR04M-2 — one family). Module electronics in the
 *          dry box, encapsulated waterproof probe on its own fixed cable looking
 *          down into the tank from 150 mm above the lid.
 *  Board : ESP32-WROOM-32 DevKitC V1, 38-pin (CP2102) → in the Arduino IDE pick
 *          "DOIT ESP32 DEVKIT V1"  (or "ESP32 Dev Module")
 *  Core  : "esp32 by Espressif Systems" (2.0.x or 3.x both build this)
 *
 *  NO EXTERNAL LIBRARIES NEEDED for the core build. Only what ships with the ESP32
 *  core: Wire, WiFi, WebServer, Preferences (NVS). The SSD1306 OLED is driven from
 *  wlg_display.h in this folder, so there is nothing to install just to build it.
 *
 *  OPTIONAL app layer: the "Blynk" library adds the phone dashboard + push, same
 *  shape as the reference build this project follows ("IoT Based Water Level
 *  Indicator using Ultrasonic Sensor", Tech StudyCell, https://youtu.be/9geREeE13jc,
 *  write-up https://iotcircuithub.com/iot-based-water-level-indicator/). It is
 *  strictly additive and fails safe: ENABLE_BLYNK 0, or no wlg_secrets.h, or no OLED
 *  fitted, and this is still the same LAN-only gauge with the same CSV log and the
 *  same LOCAL buzzer. Before filling in wlg_secrets.h read docs/05-blynk-and-display.md
 *  (quota maths, and why the token is a password).
 *
 *  WHAT THIS PHASE PROVES (before any PCB exists):
 *    1. the module answers on TRIG/ECHO in mode 1 (R19 open) and the median-filtered
 *       distance is stable over 24 h — send 'r' for one raw ping with the pulse width
 *    2. the mounting gives ≥ 25 cm from the face to the FULL water line, so the 20 cm
 *       blind zone never swallows the top of the tank (docs/03-ultrasonic.md)
 *    3. there is usable WiFi with the box ON TOP of the metal lid — send 'w'
 *
 *  Serial: 115200.  Commands: r s c w p e f a g t i b l u j d D k v x h  (see help())
 *  Web   : http://<ip>/            test page      http://<ip>/api/state   JSON
 *  OLED  : optional 0.96" SSD1306 128x64 on I2C 0x3C/0x3D (auto-probed at boot)
 *  Wrong/empty WiFi credentials → opens AP "WaterLevel-Setup" / "waterlevel".
 *
 *  ⚠ WIRING WARNING (see docs/img/01-wiring.svg):
 *    - The ESP32 is NOT 5 V tolerant. The module's ECHO must go through the
 *      2 kΩ / 3.3 kΩ divider → 3.00 V on GPIO26. Feeding 5 V into that pin kills it.
 *      (Alternative: run the module from 3V3 instead of 5 V, then no divider is needed.)
 *    - GPIO12 is the button AND a strapping pin (MTDI): do not hold the button while
 *      the board resets, or it will not boot until you let go. docs/00-design.md §5.
 *    - Do NOT wire anything to the pins printed D6 D7 D8 D9 D10 D11 — those are the
 *      SPI flash lines. They are broken out on this board and they look usable.
 *    - Connect GND to the module before VCC (the datasheet asks for that order, and
 *      it matters with a mains-referenced supply).
 *
 *  LICENCE / CREDIT
 *    SPDX-License-Identifier: MIT — see LICENSE at the top of this repo.
 *    One exception: wlg_font.h carries glyph data from Adafruit_GFX (glcdfont.c,
 *    v1.13.0) under Adafruit's BSD notice, which stays in that file.
 *    The GPIO numbers, the part choices and the "> 25 cm above the full line" rule
 *    follow the Tech StudyCell / IoT CircuitHub build cited above. No code, schematic,
 *    PCB layout, image or text from that project is in here — everything below was
 *    written for this installation. Full credit trail: THIRD_PARTY_NOTICES.md.
 * ==========================================================================*/

/* ======================= 0. WHAT IS SWITCHED ON ==========================
 * Compile-time switches. Both stay compiled-in by default but switch themselves
 * OFF at runtime when the hardware/credentials they need are missing, so leaving
 * them at 1 can never brick the build. There is NO sensor-type switch any more:
 * this project is the SR04M build, full stop.
 *     arduino-cli lib install "Blynk"      (Library Manager -> "Blynk", Blynk Inc)
 * ------------------------------------------------------------------------*/
#ifndef ENABLE_OLED
#define ENABLE_OLED   1        // SSD1306 128x64 on I2C — driver is wlg_display.h, no library
#endif
#ifndef ENABLE_BLYNK
#define ENABLE_BLYNK  1        // Blynk IoT cloud + phone app — needs the Blynk library
#endif

/* ---- credentials live in wlg_secrets.h, which git ignores ------------------
 * Copy wlg_secrets.example.h -> wlg_secrets.h (this folder) and fill it in; this
 * #if __has_include picks it up automatically and the lines below then become
 * defaults instead of the values actually used. No file, no cloud: the sketch
 * still builds and runs as a LAN-only gauge, which is the state this repo is
 * published in.
 *
 * Why a separate file instead of pasting the token here: a token committed to a
 * public repo cannot be unpublished, even after a later commit deletes it. The
 * token is a password — whoever holds it can read AND write this device — and if
 * one ever reaches git history, rotate it in Blynk Console (Device -> Info).
 *
 * (__has_include is safe for a quoted include of a file in the sketch folder:
 * the compiler resolves it relative to this .ino. It is NOT safe for a *library*
 * include — see the note on #include <BlynkSimpleEsp32.h> below.)             */
#if __has_include("wlg_secrets.h")
#include "wlg_secrets.h"
#endif

/* Blynk IoT 2.x will not compile without TEMPLATE_ID and TEMPLATE_NAME. */
#ifndef BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_ID   ""                  // e.g. "TMPLAB12CD34"
#endif
#ifndef BLYNK_TEMPLATE_NAME
#define BLYNK_TEMPLATE_NAME "Water Tank Gauge"
#endif
#ifndef BLYNK_AUTH_TOKEN
#define BLYNK_AUTH_TOKEN    ""                  // the 32-character device token
#endif
#ifndef BLYNK_SERVER
#define BLYNK_SERVER        ""                  // "" = blynk.cloud, else "192.168.1.50"
#endif
#ifndef BLYNK_SERVER_PORT
#define BLYNK_SERVER_PORT   80                  // cloud uses 80; self-hosted usually 8080
#endif

/* Event codes must match what you create in Console -> Template -> Events, one per
   line, or the push simply never arrives (the cloud answers "event not found"). */
#define BLYNK_EV_LOW      "level_low"
#define BLYNK_EV_HIGH     "level_high"
#define BLYNK_EV_FAULT    "sensor_fault"
#define BLYNK_EV_RECOVER  "level_ok"

#if ENABLE_BLYNK
/* Deliberately NOT wrapped in __has_include(): Arduino's library discovery scans the
 * sketch text for #include lines, and an include hidden behind __has_include() is never
 * added to the build — the guard silently evaluates false and the whole cloud layer
 * compiles out even when the library IS installed. Measured on this machine: with a token
 * pasted, the plain #if ENABLE_BLYNK form pulls 8336 B of Blynk code and puts the literal
 * "blynk.cloud" into the .bin; a build without the library has neither. So a missing library
 * is a loud compile error, not a silent no-op. Fix it
 * either way:   arduino-cli lib install "Blynk"      or      #define ENABLE_BLYNK 0
 */
#include <BlynkSimpleEsp32.h>
#endif

/* BLYNK_ON = compiled in. It is still OFF at runtime while BLYNK_AUTH_TOKEN is "". */
#if ENABLE_BLYNK
#define BLYNK_ON 1
#else
#define BLYNK_ON 0
#endif

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#if ENABLE_OLED
#include "wlg_display.h"        // SSD1306 driver + the 5x7 font in wlg_font.h
#endif

/* ======================= 1. USER SETTINGS ================================= */
/* Pin map adopted from the reference project (Tech StudyCell) so you can follow its
 * schematic with this repo: ultrasonic on D26/D27, OLED on D21/D22, button D12,
 * buzzer D13, LED D14. Only the echo divider and the LED's second job differ. */
#define PIN_TRIG     27             // out → module TRIG/RX. 12 µs high (module wants ≥ 10 µs)
#define PIN_ECHO     26             // in  ← module ECHO/TX **through the 2k/3k3 divider**
#define PIN_SDA      21             // I2C SDA (OLED) — same as the reference project
#define PIN_SCL      22             // I2C SCL (OLED)
#define PIN_BTN      12             // button to GND, INPUT_PULLUP. STRAPPING PIN — see header warning
#define PIN_BUZZ     13             // → 220 Ω → BC547/S8050 base; the LOCAL half of the alarm
#define PIN_LED      14             // → 220 Ω → LED. heartbeat + solid on for the full alarm
#define PIN_VBAT     36             // ADC1_CH0 — battery/5 V rail sense, 1 MΩ/1 MΩ, INPUT-ONLY pin

/* ---- module physics and the filter. Read docs/03-ultrasonic.md first. ----
 * The module's own blind area is 20 cm; the gate below is deliberately higher, so
 * that swell, rain splash and a film of dew on the face cannot masquerade as water.
 * Anything the module reports inside the gate is thrown away, not smoothed. */
#define BLIND_GATE_CM      25.0f    // rejects echoes closer than this (module blind 20 cm)
#define OVER_RANGE_CM      15.0f    // a read further away than d_empty + this is not water
#define MAX_ECHO_US        45000    // pulseIn ceiling. The module drops ECHO by itself at 40 ms,
                                    // so 45 ms is the honest ceiling: 30 ms would cut off a
                                    // legitimate far read and invent a "no echo"
#define NO_ECHO_US         39500    // a pulse this long IS the module's 40 ms timeout
#define MIN_PING_GAP_MS    60       // the module wants > 50 ms between triggers; the console,
                                    // the web page and the timer can all ask at once
#define FILTER_N           15       // median window
#define MAX_JUMP_CM        12.0f    // bigger single-sample step than this needs confirming
#define CONFIRM_COUNT      3        // consecutive reads needed to accept such a step
#define HOLD_SAMPLES       20       // keep last good value for this many reads
#define FAULT_STREAK       3        // this many pings with no usable echo -> sensor fault alarm

/* Defaults, all overridable from the serial console and stored in NVS.
 * The two distances are the whole calibration, and they are DISTANCES, not levels —
 * same two constants the reference sketch calls emptyTankDistance / fullTankDistance.
 * Face 150 mm above the lid underside, lid gap 15.2 cm, water depth 121.9 cm:
 *   d_full  = 15.0 + 15.2                     =  30.2 cm   (face → full water line)
 *   d_empty = 30.2 + 121.9                    = 152.1 cm   (face → tank floor)
 * Set them at the tank, not from the drawing: 'e' with the tank actually empty and 'f'
 * at the overflow line. Both then bake in the sound speed at calibration time. */
const char   DEF_SSID[]     = "";
const float  DEF_D_EMPTY    = 152.1f;    // raw face→floor distance, empty tank
const float  DEF_D_FULL     = 30.2f;     // raw face→surface distance at the full line
const float  DEF_AREA_CM2   = 92903.0f;  // 10 ft × 10 ft ⇒ 92.9 L per cm of level
const float  DEF_AIR_T_C    = 30.0f;     // drives sound speed; no sensor in this build, 't' sets it
const uint16_t DEF_SAMPLE_S = 60;        // bring-up rate; for the real deployment: 'i 900' (15 min)

/* Alarms — the reference build's "beep locally and buzz the phone" behaviour. The free
 * Blynk plan allows 5 events per template, so low / high / fault / recovered is the
 * whole budget. Hysteresis and a cooldown are what keep the phone quiet: a level parked
 * exactly on the threshold would otherwise notify over and over.
 * The high alarm sits 12 cm below the full line, i.e. at a distance of 42 cm — far
 * outside the blind zone, so the alarm never depends on a read the module cannot make. */
const float    DEF_LOW_CM     = 24.0f;   // ~20 % of 121.9 cm. The app slider (V11) moves it
const float    DEF_HIGH_CM    = 110.0f;  // ~12 cm below the overflow (V12)
#define ALARM_HYST_CM         2.0f       // an alarm only CLEARS 2 cm the other way
#define ALARM_COOLDOWN_S      1800       // max one notification per event per 30 min
#define BUZZ_PERIOD_MS        9000       // beep every 9 s while an alarm is unmuted
#define BUZZ_ON_MS            250
#define OLED_IDLE_S           120        // blank the panel after this (3-8 mA + burn-in)
#define OLED_REDRAW_MS        2000
/* Blynk free plan: 100 000 device messages per calendar month. messages/month =
 * samples/month x streams_per_sample, so a 60 s sample rate with 5 streams is
 * ~216 000 — over quota on its own. Hence the two throttles below; the table of
 * what each interval actually costs is in docs/05-blynk-and-display.md. */
#define BLYNK_THROTTLE_S      300        // sampling faster than this -> only V1 (+ changed V8)
#define BLYNK_SLOW_S          3600       // diagnostics + RSSI: hourly
#define BLYNK_FREE_MSGS       100000L    // Blynk free plan device messages per calendar month

#define FW_REV "B ultrasonic SR04M"

/* ======================= 2. TYPES AND GLOBALS ============================= */
/* Why a ping was thrown away. Four distinct causes, kept apart on purpose: they have
 * different fixes (wipe the face / re-cut the mount / check the R19 pad / look at the
 * surface), and a gauge that lumps them into "bad" teaches you nothing. */
enum Reason : uint8_t { URS_OK = 0, URS_NEAR, URS_FAR, URS_NOECHO, URS_JUMP };
const char *reasonText(uint8_t r) {
  switch (r) { case URS_OK:     return "ok";
               case URS_NEAR:   return "too close";
               case URS_FAR:    return "out of range";
               case URS_NOECHO: return "no echo";
               default:       return "jump rejected"; }
}

struct Config {
  char    ssid[33];  char pass[65];
  float   dEmptyCm, dFullCm, areaCm2, blindCm, airTC;
  uint16_t sampleSec;
  float   lowCm, highCm;        // alarm thresholds (serial 'l'/'u', app sliders V11/V12)
  uint8_t mute;                 // 1 = buzzer silenced ('j', the button, or app V13)
  uint8_t oledFlip;             // 1 = panel mounted upside down ('D')
} cfg;

struct Reading {
  float  distCm, levelCm, pct, litres;
  bool   valid;
  uint32_t whenMs;
} rd;

struct Filter {
  float  ring[FILTER_N];
  uint8_t idx = 0, count = 0;
  float  good = NAN, lastRaw = NAN;
  int    confirm = 0, hold = 0;
  uint32_t badCount = 0, totalCount = 0;
  uint32_t nNear = 0, nFar = 0, nNoEcho = 0;   // why, separately — SINCE the last accepted ping
  uint8_t  streakBad = 0, streakHard = 0;      // rejected pings in a row; "hard" = no echo / far
} flt;

uint32_t lastPingMs = 0;         // module needs > 50 ms between triggers
uint32_t lastEchoUs = 0;         // round-trip time of the most recent ping, accepted or not
uint8_t  lastReason = URS_NOECHO;  // verdict of the most recent ping
float    lastNearCm = NAN;       // what a rejected near read measured (dew ≈ 2-8 cm)
float    vbat = NAN;

Preferences prefs;
WebServer   server(80);

/* Alarm state — declared here so the OLED and the transport can both read it.
 * The state machine that writes it is section 9. */
struct Alarms {
  bool     low = false, high = false, fault = false;
  bool     sentLow = false, sentHigh = false, sentFault = false;  // notified, not yet cleared
  uint32_t tLow = 0, tHigh = 0, tFault = 0;   // when each notification was last sent
  uint32_t tBeep = 0;
} alrm;

/* one status string for OLED + app + web page, so the three can never disagree */
const char *statusText();
/* called at the end of every sample: pushes to the app and marks the screen dirty */
void outputsUpdate();
void evaluateAlarms();

/* ======================= 3. THE SR04M: TRIG / ECHO ======================= */
/* Mode 1 on this family = plain HC-SR04/HR-04 protocol, selected by leaving the
 * resistor off pad R19 on the module. Sequence, straight from the datasheet:
 *   1. ≥ 10 µs HIGH on TRIG/RX               (we send 12 µs)
 *   2. module fires 8 bursts of 40 kHz, pulls ECHO/TX high
 *   3. ECHO stays high for the round-trip time, then drops
 *   4. if no echo comes back, ECHO drops by itself at 40 ms
 *   5. distance = high-us × 348 m/s / 2  →  the datasheet's own form is uS / 57.5 = cm
 * Nothing else about the module is interesting: no I2C, no calibration bytes, no
 * configuration registers. Five modes exist (R19), only mode 1 is used here. */
float speedOfSoundMmPerUs(float tC) {          // dry air approximation
  float t = isnan(tC) ? 25.0f : tC;
  return (331.3f + 0.606f * t) / 1000.0f;      // mm per µs
}

bool pingRaw(float &cm, uint8_t &reason) {
  /* The module needs > 50 ms between triggers. 'c', /api/log and the sample timer can
   * all ask for a ping within a millisecond of each other, and two triggers 10 ms apart
   * make the module answer with garbage — so the gap is enforced here, once, for every
   * caller rather than left to each caller to remember. */
  uint32_t since = millis() - lastPingMs;
  if (since < MIN_PING_GAP_MS) delay(MIN_PING_GAP_MS - since);

  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(12);
  digitalWrite(PIN_TRIG, LOW);

  uint32_t us = pulseIn(PIN_ECHO, HIGH, MAX_ECHO_US);
  lastPingMs = millis();
  lastEchoUs = us;

  if (us == 0 || us >= NO_ECHO_US) {           // the module's own 40 ms timeout
    reason = URS_NOECHO; return false;           // nothing reflected: rippled surface, foam,
  }                                            // algae mat, face dried out, or > max range
  cm = ((float)us * speedOfSoundMmPerUs(cfg.airTC)) / 20.0f;   // µs·mm/µs ÷2 ÷10 = cm

  if (cm < cfg.blindCm) {                      // inside the gate: not water
    reason = URS_NEAR; lastNearCm = cm; return false;
  }
  if (cm > cfg.dEmptyCm + OVER_RANGE_CM) {     // further than the floor: cannot be water
    reason = URS_FAR; return false;
  }
  reason = URS_OK;
  return true;
}

/* ======================= 4. DISTANCE TO LEVEL ============================ */
/* Everything is distance from the face, top-down — the same two constants the
 * reference sketch uses, and the reason the raw distance is logged next to the level:
 * a level is an opinion about calibration, the distance is what the sensor saw. */
float spanCm() { return cfg.dEmptyCm - cfg.dFullCm; }

float medianOf() {
  float t[FILTER_N]; uint8_t k = 0;
  for (uint8_t i = 0; i < flt.count; i++) t[k++] = flt.ring[(flt.idx + FILTER_N - i) % FILTER_N];
  for (uint8_t i = 1; i < k; i++) { float x = t[i]; int j = i - 1;
    while (j >= 0 && t[j] > x) { t[j + 1] = t[j]; j--; } t[j + 1] = x; }
  return t[k / 2];
}
float filterUpdate(float rawCm, bool valid) {
  if (valid) { flt.ring[flt.idx] = rawCm; flt.idx = (flt.idx + 1) % FILTER_N;
               if (flt.count < FILTER_N) flt.count++; }
  float med = (flt.count >= 3) ? medianOf() : NAN;
  if (isnan(med)) { if (flt.hold > 0) { flt.hold--; return flt.good; } return NAN; }
  if (isnan(flt.good)) { flt.good = med; flt.lastRaw = med; return med; }
  if (fabsf(med - flt.good) > MAX_JUMP_CM) {          // a real fill, or one bad echo burst?
    float side = (med > flt.good) ? 1.0f : -1.0f;
    if (flt.confirm > 0 && (((flt.lastRaw > flt.good) ? 1.0f : -1.0f) == side)) {
      if (++flt.confirm >= CONFIRM_COUNT) { flt.good = med; flt.confirm = 0; }
    } else flt.confirm = 1;
    flt.lastRaw = med;
    if (flt.hold > 0) flt.hold--;
    return flt.good;                                  // hold until confirmed
  }
  flt.good = med; flt.confirm = 0; flt.hold = HOLD_SAMPLES;
  return med;
}

void takeSample(bool verbose) {
  float raw = NAN; uint8_t reason = URS_NOECHO;
  bool valid = pingRaw(raw, reason);
  lastReason = reason;
  flt.totalCount++;
  if (!valid) {
    flt.badCount++; flt.streakBad++;
    if      (reason == URS_NEAR)   flt.nNear++;
    else if (reason == URS_FAR)  { flt.nFar++;    flt.streakHard++; }
    else if (reason == URS_NOECHO) { flt.nNoEcho++; flt.streakHard++; }
  } else {
    /* One accepted ping clears the "why did it fail" counters: they describe the streak
     * that just ended, which is what the status text and the fault alarm both want. */
    flt.streakBad = 0; flt.streakHard = 0;
    flt.nNear = flt.nFar = flt.nNoEcho = 0;
  }
  float dist = filterUpdate(raw, valid);
  bool ok = !isnan(dist);
  rd.whenMs = millis(); rd.valid = ok;
  if (ok) {
    float span = spanCm();
    rd.distCm  = dist;
    rd.levelCm = constrain(cfg.dEmptyCm - dist, 0.0f, span);
    rd.pct     = constrain(rd.levelCm / span * 100.0f, 0.0f, 100.0f);
    rd.litres  = rd.levelCm * cfg.areaCm2 / 1000.0f;   // cm × cm² = cm³, ÷1000 = litres
  }
  evaluateAlarms();   // thresholds, hysteresis, notification cooldown
  outputsUpdate();    // OLED + app. Both are no-ops when switched off or unconfigured.
  if (verbose) {
    Serial.printf("%lu,%lu,%.1f,%s,%.1f,%.1f,%.2f,%.0f,%.1f,%.1f,%.2f,%ld,%lu,%lu\n",
      (unsigned long)(millis() / 1000), (unsigned long)lastEchoUs,
      isnan(raw) ? -1.0f : raw, reasonText(reason),
      isnan(dist) ? -1.0f : dist, ok ? rd.levelCm : -1.0f, ok ? rd.pct : -1.0f,
      ok ? rd.litres : -1.0f, cfg.airTC, cfg.blindCm,
      isnan(vbat) ? -99.0f : vbat, (long)(WiFi.isConnected() ? WiFi.RSSI() : 0),
      (unsigned long)flt.badCount, (unsigned long)flt.totalCount);
  }
}

/* ======================= 5. BATTERY / RAIL =============================== */
float readVbat() {
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) { mv += analogReadMilliVolts(PIN_VBAT); delay(2); }
  return ((mv / 8.0f) * 2.0f) / 1000.0f;              // 1 MΩ / 1 MΩ divider
}
float vbatPct(float v) {
  if (isnan(v)) return NAN;
  const float V[] = {3.30f,3.50f,3.65f,3.80f,3.95f,4.05f,4.15f,4.20f};
  const float P[] = {0, 5, 15, 35, 60, 80, 95, 100};
  if (v <= V[0]) return 0; if (v >= V[7]) return 100;
  for (int i = 0; i < 7; i++) if (v <= V[i + 1])
    return P[i] + (P[i + 1] - P[i]) * (v - V[i]) / (V[i + 1] - V[i]);
  return NAN;
}

/* ======================= 6. WHAT THE BOX SAYS ============================ */
/* One status string feeds the OLED, the app's text stream and the web page. The
 * fault words name the *reason*, because "sensor error" would send you looking at
 * the wrong thing: dew on the face, a surface closer than the gate, and a module that
 * never answered need three different fixes. */
const char *statusText() {
  static char s[24];
  if (!rd.valid) {
    if (flt.totalCount < 3)                          { strcpy(s, "WARMING UP"); return s; }
    /* A film of dew on the transducer reads a couple of cm; a near read at 15-25 cm is the
     * water surface itself climbing into the gate. Same rejection, two different fixes. */
    if (flt.nNear && !flt.nNoEcho && !flt.nFar) {
      if (!isnan(lastNearCm) && lastNearCm < 12.0f)  { strcpy(s, "DEW ON FACE"); return s; }
      else                                           { strcpy(s, "NEAR/BLIND");  return s; }
    }
    if (flt.nFar && !flt.nNoEcho)                    { strcpy(s, "OUT OF RANGE"); return s; }
    if (flt.nNoEcho)                                 { strcpy(s, "NO ECHO");     return s; }
    strcpy(s, "NO READING");                                             return s;
  }
  if (alrm.high)                                     { strcpy(s, "FULL - OVERFLOW"); return s; }
  if (alrm.low)                                      { strcpy(s, "LOW LEVEL");    return s; }
  if (alrm.fault)                                    { strcpy(s, "SENSOR FAULT"); return s; }
  if (flt.totalCount > 8 && flt.badCount * 4 > flt.totalCount)
                                                     { strcpy(s, "MANY BAD READS"); return s; }
  strcpy(s, "OK");
  return s;
}

#if ENABLE_OLED
uint32_t oledWakeMs = 0, oledTDraw = 0, oledTProbe = 0;
bool     oledAwake = true, oledDirty = true;

/* 128x64 is small: the level is a 2x-scaled number, everything else is 5x7 text, and a
 * reversed band is the only way to shout. The two calibration distances and the gate are
 * on the panel on purpose — when a reading looks wrong, the first thing you want to see
 * is what the sensor actually measured and what the gauge was told to ignore. */
void oledDraw() {
  char b[24];
  memset(oledBuf, 0, sizeof oledBuf);

  if (rd.valid) {
    snprintf(b, sizeof b, "%.0f", (double)rd.levelCm);
    int x = oledText(0, 0, b, 2);                       // 10x14 px digits
    oledText(x + 3, 9, "cm");
    snprintf(b, sizeof b, "%.1f%%", (double)rd.pct);
    oledTextRight(OLED_W - 1, 0, b, 1);
    snprintf(b, sizeof b, "%.0f L", (double)rd.litres);
    oledTextRight(OLED_W - 1, 9, b, 1);
    oledBar(0, 18, OLED_W, 10, rd.pct);
  } else {
    oledTextCenter(0, "NO LEVEL", 2);
  }

  if (isnan(vbat)) snprintf(b, sizeof b, "BAT --V");
  else snprintf(b, sizeof b, "BAT %.2fV %.0f%%", (double)vbat, (double)vbatPct(vbat));
  oledText(0, 30, b, 1);
  if (WiFi.isConnected()) { snprintf(b, sizeof b, "%ld dBm", (long)WiFi.RSSI());
                            oledTextRight(OLED_W - 1, 30, b, 1); }
  else                    oledTextRight(OLED_W - 1, 30, "NO WIFI", 1);

  if (isnan(rd.distCm)) snprintf(b, sizeof b, "ECHO --");
  else snprintf(b, sizeof b, "ECHO %.1f", (double)rd.distCm);
  oledText(0, 39, b, 1);                                // what the module measured, right now
  snprintf(b, sizeof b, "SPAN %.1f", (double)spanCm());
  oledTextRight(OLED_W - 1, 39, b, 1);

  snprintf(b, sizeof b, "GATE %.1f T%.0fC", (double)cfg.blindCm, (double)cfg.airTC);
  oledText(0, 48, b, 1);                                // the gate + the speed-of-sair config
  snprintf(b, sizeof b, "%lu bad", (unsigned long)flt.badCount);
  oledTextRight(OLED_W - 1, 48, b, 1);

  oledTextCenter(56, statusText(), 1);
  if (alrm.low || alrm.high || alrm.fault) oledInvertBand(55, 9);
  oledFlush();
}

void oledSetup() {
  if (oledInit()) {
    oledSetFlip(cfg.oledFlip != 0);
    oledSetContrast(OLED_CONTRAST);      // the line to raise if a hot, sunlit panel fades
    Serial.printf("oled: SSD1306 at 0x%02X on I2C %u/%u\n", oledAddr, PIN_SDA, PIN_SCL);
  } else {
    Serial.println(F("oled: nothing answering at 0x3C/0x3D — display code stays idle"));
  }
}
void oledWake() { oledWakeMs = millis(); if (!oledAwake) { oledAwake = true; oledDirty = true; } }
void oledRequestDraw() { oledDirty = true; oledWake(); }
/* A routine sample must NOT un-blank a panel nobody is looking at: mark the image dirty
 * and let oledService() redraw it only if the panel is already awake. */
void oledRefresh() { oledDirty = true; }

void oledService(uint32_t now) {
  if (!oledAddr) {                                  // panel fitted later? retry every 10 min
    if (now - oledTProbe > 600000) { oledTProbe = now; oledSetup(); }
    return;
  }
  /* OLED_IDLE_S 0 = never blank (mains-powered box, the reference build's arrangement). Any
   * number above 0 is the honest default for a battery/solar build and for a panel that
   * would otherwise burn its own digits into the glass. */
  if (OLED_IDLE_S > 0 && oledAwake && now - oledWakeMs > (uint32_t)OLED_IDLE_S * 1000UL) {
    oledAwake = false; oledSleep(false);            // panel off; driver and buffer untouched
    return;
  }
  if (oledAwake && (oledDirty || now - oledTDraw >= OLED_REDRAW_MS)) {
    oledDirty = false; oledTDraw = now; oledDraw();
  }
}
#else  // ENABLE_OLED — the same calls, as nothing
inline void oledSetup() {}
inline void oledWake() {}
inline void oledRequestDraw() {}
inline void oledRefresh() {}
inline void oledService(uint32_t) {}
#endif // ENABLE_OLED

/* ======================= 7. CONFIG IN NVS ================================ */
void cfgDefaults() {
  strncpy(cfg.ssid, DEF_SSID, 32); cfg.ssid[32] = 0;
  cfg.pass[0] = 0;
  cfg.dEmptyCm = DEF_D_EMPTY; cfg.dFullCm = DEF_D_FULL;
  cfg.areaCm2 = DEF_AREA_CM2;  cfg.blindCm = BLIND_GATE_CM;
  cfg.airTC = DEF_AIR_T_C;     cfg.sampleSec = DEF_SAMPLE_S;
  cfg.lowCm = DEF_LOW_CM; cfg.highCm = DEF_HIGH_CM; cfg.mute = 0; cfg.oledFlip = 0;
}
void cfgLoad() {
  cfgDefaults();
  prefs.begin("wlg", true);
  strncpy(cfg.ssid, prefs.getString("ssid", cfg.ssid).c_str(), 32);
  strncpy(cfg.pass, prefs.getString("pass", cfg.pass).c_str(), 64);
  cfg.dEmptyCm  = prefs.getFloat("dEmpty", cfg.dEmptyCm);
  cfg.dFullCm   = prefs.getFloat("dFull",  cfg.dFullCm);
  cfg.areaCm2   = prefs.getFloat("area",   cfg.areaCm2);
  cfg.blindCm   = prefs.getFloat("gate",   cfg.blindCm);
  cfg.airTC     = prefs.getFloat("airT",   cfg.airTC);
  cfg.sampleSec = (uint16_t)prefs.getUShort("sample", cfg.sampleSec);
  cfg.lowCm     = prefs.getFloat("lowCm",  cfg.lowCm);
  cfg.highCm    = prefs.getFloat("highCm", cfg.highCm);
  cfg.mute      = prefs.getUChar("mute",   cfg.mute);
  cfg.oledFlip  = prefs.getUChar("flip",   cfg.oledFlip);
  prefs.end();
  if (spanCm() < 20.0f) {           // a calibration that cannot be right: refuse to invent levels
    Serial.println(F("!! d_empty - d_full is under 20 cm — falling back to the built-in defaults."
                     "   Set them at the tank: 'e <empty distance>' and 'f <full distance>'."));
    cfg.dEmptyCm = DEF_D_EMPTY; cfg.dFullCm = DEF_D_FULL;
  }
}
void cfgSave() {
  prefs.begin("wlg", false);
  prefs.putString("ssid", cfg.ssid);  prefs.putString("pass", cfg.pass);
  prefs.putFloat("dEmpty", cfg.dEmptyCm); prefs.putFloat("dFull", cfg.dFullCm);
  prefs.putFloat("area", cfg.areaCm2);    prefs.putUShort("sample", cfg.sampleSec);
  prefs.putFloat("gate", cfg.blindCm);    prefs.putFloat("airT", cfg.airTC);
  prefs.putFloat("lowCm", cfg.lowCm);  prefs.putFloat("highCm", cfg.highCm);
  prefs.putUChar("mute", cfg.mute);    prefs.putUChar("flip", cfg.oledFlip);
  prefs.end();
}

/* ======================= 8. BLYNK TRANSPORT (optional) ================== */
/* The sketch already owns WiFi — STA with an AP fallback and its own retry — so this
 * uses config()/connect()/run() and never Blynk.begin(auth, ssid, pass), which would
 * block in a while-loop forever and take the radio away from the local web server.
 *
 * Virtual pins (Console -> Template -> Datastream). Blynk's own V0..V31 macros come
 * from the library, and BLYNK_WRITE(Vn) dispatches by the pin NUMBER, so the handler
 * below for V11 really is the one that fires for datastream 11.
 *   V0 level_cm V1 level_pct V2 litres V3 dist_cm V4 vbat V8 status   <- every sample
 *   V9 rssi  V14 bad_pct  V15 uptime_h                               <- hourly
 *   V10 sample_s  V11 low_cm  V12 high_cm  V13 mute                  <- written BY THE APP
 * V5 air_t, V6 air_rh, V7 water_t and V16 p_diff are retired: they carried the pressure
 * build's humidity and barometric streams. Delete those four datastreams in the template
 * rather than renumbering anything — the user's existing template stays valid.
 */
#if BLYNK_ON
bool     blynkLinked = false;
uint32_t tBlynkRetry = 0, tBlynkSlow = 0;
char     lastStatusSent[24] = "";

bool blynkConfigured() { return BLYNK_AUTH_TOKEN[0] != 0 && BLYNK_TEMPLATE_ID[0] != 0; }
bool blynkReady()      { return blynkConfigured() && Blynk.connected(); }
void blynkPush();
void blynkNotify(const char *code, const String &what);

/* NAN would go over the wire as the text "nan" and the widget shows nonsense. */
double bnum(float v, double whenNan) { return isnan(v) ? whenNan : (double)v; }

BLYNK_CONNECTED() {
  blynkLinked = true;
  lastStatusSent[0] = 0;
  // the app is the master of these four: pull their stored values back after a reboot
  Blynk.syncVirtual(V10, V11, V12, V13);
  Serial.println(F("blynk: link up (app values synced back)"));
}
BLYNK_DISCONNECTED() {
  blynkLinked = false;
  Serial.println(F("blynk: link down — the gauge and the CSV keep running regardless"));
}

BLYNK_WRITE(V10) {                       // sample interval, from a Slider
  int v = param.asInt();
  if (v < 5 || v > 7200) { Serial.println(F("blynk V10: out of range, ignored (5..7200 s)")); return; }
  cfg.sampleSec = (uint16_t)v; cfgSave();
  Serial.printf("blynk V10: sample interval = %u s\n", cfg.sampleSec);
}
BLYNK_WRITE(V11) {
  float v = (float)param.asDouble();
  if (v < 0 || v > spanCm()) { Serial.println(F("blynk V11: out of range, ignored")); return; }
  cfg.lowCm = v; cfgSave();
  Serial.printf("blynk V11: low alarm = %.1f cm\n", cfg.lowCm);
}
BLYNK_WRITE(V12) {
  float v = (float)param.asDouble();
  if (v < 0 || v > spanCm()) { Serial.println(F("blynk V12: out of range, ignored")); return; }
  cfg.highCm = v; cfgSave();
  Serial.printf("blynk V12: high alarm = %.1f cm\n", cfg.highCm);
}
BLYNK_WRITE(V13) { cfg.mute = param.asInt() ? 1 : 0; cfgSave();
                   Serial.printf("blynk V13: buzzer %s\n", cfg.mute ? "muted" : "armed"); }

void blynkSetup() {
  if (!blynkConfigured()) {
    Serial.println(F("blynk: OFF — copy wlg_secrets.example.h to wlg_secrets.h and put Console -> Info values in it"));
    Serial.println(F("      the phone app is optional; http://<ip>/ still works with no internet"));
    return;
  }
  if (!WiFi.isConnected()) { Serial.println(F("blynk: no WiFi yet — will try once the link is up")); return; }
  if (BLYNK_SERVER[0]) Blynk.config(BLYNK_AUTH_TOKEN, BLYNK_SERVER, BLYNK_SERVER_PORT);
  else                 Blynk.config(BLYNK_AUTH_TOKEN);          // blynk.cloud:80
  blynkLinked = Blynk.connect(4000);
  Serial.printf("blynk: %s  host=%s port=%u\n", blynkLinked ? "connected" : "not reachable (retrying in the background)",
                BLYNK_SERVER[0] ? BLYNK_SERVER : "blynk.cloud", (unsigned)BLYNK_SERVER_PORT);
  if (blynkLinked) blynkPush();
}

/* Quota policy, in one place: pct always; status only when it CHANGES; the other
 * three fast streams only when we are not sampling faster than BLYNK_THROTTLE_S;
 * the diagnostics set once an hour. See docs/05 for the messages/month table. */
void blynkPush() {
  if (!blynkReady()) return;
  uint32_t now = millis();
  const char *st = statusText();

  Blynk.virtualWrite(V1, bnum(rd.pct, 0));
  if (strcmp(st, lastStatusSent) != 0) {
    strcpy(lastStatusSent, st);
    Blynk.virtualWrite(V8, String(st));
  }
  if (cfg.sampleSec >= BLYNK_THROTTLE_S && rd.valid) {
    Blynk.virtualWrite(V0, bnum(rd.levelCm, 0));
    Blynk.virtualWrite(V2, bnum(rd.litres, 0));
    Blynk.virtualWrite(V4, bnum(vbat, 0));
  } else if (cfg.sampleSec < BLYNK_THROTTLE_S && flt.totalCount == 1) {
    Serial.printf("blynk: sampling every %u s — only V1/V8 go out to stay inside the "
                  "%ld messages/month free quota (send 'i %u' for the full set)\n",
                  cfg.sampleSec, BLYNK_FREE_MSGS, BLYNK_THROTTLE_S);
  }
  if (now - tBlynkSlow >= (uint32_t)BLYNK_SLOW_S * 1000UL) {
    tBlynkSlow = now;
    Blynk.virtualWrite(V9,  (int)(WiFi.isConnected() ? WiFi.RSSI() : 0));
    Blynk.virtualWrite(V14, flt.totalCount ? (double)flt.badCount * 100.0 / (double)flt.totalCount : 0.0);
    Blynk.virtualWrite(V15, (double)(millis() / 3600000UL));
    if (rd.valid) Blynk.virtualWrite(V3, bnum(rd.distCm, 0));
  }
}

void blynkNotify(const char *code, const String &what) {
  if (!blynkReady()) { Serial.printf("blynk: not linked — event '%s' not sent\n", code); return; }
  Blynk.logEvent(code, what);
  Serial.printf("blynk: event '%s' sent\n", code);
}

void blynkService(uint32_t now) {
  if (!blynkConfigured()) return;
  if (!WiFi.isConnected()) return;              // wifiConnect()/loop() own the radio
  if (!Blynk.connected()) {
    if (now - tBlynkRetry > 30000) {
      tBlynkRetry = now;
      blynkLinked = Blynk.connect(4000);        // bounded, not Blynk.begin's endless while()
      Serial.printf("blynk: %s\n", blynkLinked ? "back up" : "still unreachable");
    }
    return;
  }
  Blynk.run();
}
#else  // BLYNK_ON off — the same calls, as nothing
inline void blynkSetup() {}
inline void blynkPush() {}
inline void blynkService(uint32_t) {}
static void blynkNotify(const char *code, const String &) {
  Serial.printf("blynk: compiled out or not configured — event '%s' not sent\n", code);
}
#endif // BLYNK_ON

/* ======================= 9. ALARMS, BUZZER, BUTTON ===================== */
/* Two local outputs and one remote one. The local buzzer works with no internet at
 * all, which is the point: an alarm you can only hear through somebody else's cloud
 * is not an alarm. Same behaviour as the reference build's buzzer + stop button, plus
 * hysteresis and a notification cooldown, which the reference sketch does not have. */
bool alarmAny() { return alrm.low || alrm.high || alrm.fault; }

void setLowAlarm(bool on) {
  if (on == alrm.low) return;
  alrm.low = on;
  if (on) oledWake();   // an alarm has to be visible, not just audible
  if (on) {
    if (millis() - alrm.tLow >= (uint32_t)ALARM_COOLDOWN_S * 1000UL || !alrm.sentLow) {
      alrm.tLow = millis(); alrm.sentLow = true;
      char d[96];
      snprintf(d, sizeof d, "Tank low: %.0f cm (%.0f%%), about %.0f L left. Alarm at %.0f cm.",
               (double)rd.levelCm, (double)rd.pct, (double)rd.litres, (double)cfg.lowCm);
      blynkNotify(BLYNK_EV_LOW, String(d));
    }
  } else if (alrm.sentLow) {
    alrm.sentLow = false;
    char d[80];
    snprintf(d, sizeof d, "Level back to %.0f cm (%.0f%%).", (double)rd.levelCm, (double)rd.pct);
    blynkNotify(BLYNK_EV_RECOVER, String(d));
  }
}
void setHighAlarm(bool on) {
  if (on == alrm.high) return;
  alrm.high = on;
  if (on) oledWake();
  if (on) {
    if (millis() - alrm.tHigh >= (uint32_t)ALARM_COOLDOWN_S * 1000UL || !alrm.sentHigh) {
      alrm.tHigh = millis(); alrm.sentHigh = true;
      char d[96];
      snprintf(d, sizeof d, "Tank high: %.0f cm (%.0f%%) — check the inlet, overflow risk.",
               (double)rd.levelCm, (double)rd.pct);
      blynkNotify(BLYNK_EV_HIGH, String(d));
    }
  } else if (alrm.sentHigh) {
    alrm.sentHigh = false;
    char d[80];
    snprintf(d, sizeof d, "Level back to %.0f cm (%.0f%%).", (double)rd.levelCm, (double)rd.pct);
    blynkNotify(BLYNK_EV_RECOVER, String(d));
  }
}
void setFaultAlarm(bool on) {
  if (on == alrm.fault) return;
  alrm.fault = on;
  if (on) oledWake();
  if (on) {
    if (millis() - alrm.tFault >= (uint32_t)ALARM_COOLDOWN_S * 1000UL || !alrm.sentFault) {
      alrm.tFault = millis(); alrm.sentFault = true;
      char d[96];
      snprintf(d, sizeof d, "Gauge fault: %s. The level shown may be stale.", statusText());
      blynkNotify(BLYNK_EV_FAULT, String(d));
    }
  } else if (alrm.sentFault) {
    alrm.sentFault = false;
    blynkNotify(BLYNK_EV_RECOVER, String(String("Gauge reading again: ") + statusText()));
  }
}

void evaluateAlarms() {
  /* At power-up the median filter needs three good reads before it produces a value,
   * and until then rd.valid is legitimately false. Judging the fault alarm in that
   * window would beep and push on every reboot. */
  static bool warmedUp = false;
  if (!warmedUp) { if (rd.valid) warmedUp = true; else return; }

  /* A ping that came back but landed outside the gate is a sensor fault, not a level:
   * the module can see the surface, it just cannot see it *usefully*. Two sub-cases are
   * faulted here on purpose — no echo at all, and a read further off than the floor.
   * "Too close" is deliberately NOT a fault on its own: if the last good level was already
   * near the top, a near read means the tank is genuinely over the calibration point and
   * the high alarm covers it; if the last good level was low, it is dew on the face, which
   * is a nuisance, not a broken gauge. Both show up in the status text either way. */
  bool faultNow = !rd.valid || flt.streakHard >= FAULT_STREAK;

  if (rd.valid) {                                         // thresholds only judged on good data
    if (!alrm.low  && rd.levelCm <= cfg.lowCm)                 setLowAlarm(true);
    else if (alrm.low  && rd.levelCm >  cfg.lowCm + ALARM_HYST_CM)  setLowAlarm(false);
    if (!alrm.high && rd.levelCm >= cfg.highCm)                setHighAlarm(true);
    else if (alrm.high && rd.levelCm < cfg.highCm - ALARM_HYST_CM) setHighAlarm(false);
  }
  setFaultAlarm(faultNow);
}

void outputsUpdate() {
  oledRefresh();   // no wake: idle blanking has to survive a 30 s sampling rate
  blynkPush();
}

void buzzerService(uint32_t now) {
  if (!alarmAny() || cfg.mute) return;
  if (now - alrm.tBeep >= BUZZ_PERIOD_MS) {
    alrm.tBeep = now;
    tone(PIN_BUZZ, alrm.high || alrm.fault ? 2600 : 1800, BUZZ_ON_MS);
  }
}

/* short press: wake the screen and snooze the buzzer for one period. long press
 * (>1.5 s): toggle the mute latch, so a 3 a.m. beep can be switched off without a phone.
 * This is the reference build's "push button to stop the buzzer", with the mute latch and
 * the debounce added — an undebounced button makes the buzzer stutter and the OLED flicker. */
void buttonService(uint32_t now) {
  static uint8_t  seen = 0;                  // debounced level
  static uint32_t tDown = 0, tEdge = 0;
  static bool     longDone = false;
  uint8_t raw = digitalRead(PIN_BTN) ? 1 : 0;      // 1 = pressed (button pulls to GND)
  if (now - tEdge < 30) return;
  tEdge = now;
  if (raw != seen) {
    seen = raw; longDone = false;
    if (raw) { tDown = now; oledWake(); }
    else if (!longDone) {                          // released before 1.5 s = short press
      alrm.tBeep = now;                            // snooze: silence for one beep period
      oledRequestDraw();
    }
    return;
  }
  if (raw && !longDone && now - tDown > 1500) {
    longDone = true;
    cfg.mute = cfg.mute ? 0 : 1; cfgSave();
    Serial.printf("button: buzzer %s\n", cfg.mute ? "muted" : "armed");
  }
}

/* ======================= 10. WIFI + WEB ================================== */
void wifiConnect() {
  if (cfg.ssid[0] == 0) {
    Serial.println(F("!! no SSID saved yet — send:  w YourWifiName   then:  p yourpassword"));
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);                       // modem sleep ON: keeps average current low
  WiFi.begin(cfg.ssid, cfg.pass);
  Serial.printf("wifi: connecting to '%s'", cfg.ssid);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print('.'); }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\nwifi: connected  ip=%s  rssi=%d dBm  gw=%s\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                  WiFi.gatewayIP().toString().c_str());
  else {
    Serial.println(F("\nwifi: FAILED → opening AP  WaterLevel-Setup / waterlevel  on 192.168.4.1"));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WaterLevel-Setup", "waterlevel");
  }
}
void wifiScanPrint() {
  Serial.println(F("scan: strongest networks — run this with the box closed, ON THE LID"));
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
    Serial.printf("   %-26s %5ld dBm  ch%-2ld\n", WiFi.SSID(i).c_str(), (long)WiFi.RSSI(i), (long)WiFi.channel(i));
  WiFi.scanDelete();
}

const char PAGE[] = R"rawl(
<!doctype html><meta charset=utf8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Tank level — phase 0 (ultrasonic)</title>
<style>
 body{background:#111;color:#eee;font:16px/1.5 system-ui;margin:1.5rem;max-width:34rem}
 .lv{font-size:3.4rem;font-weight:700}.litres{opacity:.7;margin-bottom:.8rem}
 .bar{height:16px;background:#2b2b2b;border-radius:8px;overflow:hidden}
 .bar>i{display:block;height:16px;background:#3ec6a8;border-radius:8px}
 table{width:100%;margin-top:1.2rem;border-collapse:collapse}
 td{padding:.25rem .4rem;border-bottom:1px solid #242424}
 td:last-child{text-align:right;font-variant-numeric:tabular-nums}
 .warn{color:#ffb020}
</style>
<h2>Water tank — phase 0 test · SR04M</h2>
<div class=lv id=lv>-- cm</div><div class=litres id=lt></div>
<div class=bar><i id=bar style="width:0%"></i></div>
<table id=t></table>
<script>
async function up(){
  try{
    const r = await (await fetch('/api/state')).json();
    lv.textContent = r.level_cm.toFixed(0)+' cm';
    lt.textContent = r.pct.toFixed(1)+' %  ·  '+(r.litres/1000).toFixed(2)+' m³  ·  '+Math.round(r.litres)+' L';
    bar.style.width = r.pct.toFixed(0)+'%';
    const rows = [['Status', r.status],
                  ['Level', r.level_cm.toFixed(1)+' cm of '+r.span_cm.toFixed(0)],
                  ['Face → water', r.dist_cm<0?'—':r.dist_cm.toFixed(1)+' cm ('+r.echo_us+' µs)'],
                  ['Echo verdict', r.echo_reason],
                  ['Blind gate', r.gate_cm.toFixed(0)+' cm — module blind zone is 20'],
                  ['Sound speed', r.air_t.toFixed(0)+' °C → '+r.sound_mms.toFixed(3)+' m/s'],
                  ['Calibration', 'empty '+r.d_empty.toFixed(1)+' / full '+r.d_full.toFixed(1)+' cm'],
                  ['Battery', r.vbat.toFixed(2)+' V ('+r.vpct+' %)'],
                  ['WiFi', r.rssi+' dBm'],
                  ['Good / total pings', (r.n-r.bad)+' / '+r.n],
                  ['Alarm at', r.low_cm.toFixed(0)+' cm low / '+r.high_cm.toFixed(0)+' cm high'+(r.mute?' (buzzer muted)':'')],
                  ['Uptime', (r.uptime/60).toFixed(0)+' min']];
    if(r.bad/r.n > 0.05 && r.n > 8) rows.push(['⚠ rejected pings','near '+r.n_near+'  no-echo '+r.n_noecho+'  far '+r.n_far]);
    t.innerHTML = rows.map(x=>{
      const c = (x[0]=='Status' && r.status!='OK') ? ' class=warn' : '';
      return '<tr><td'+c+'>'+x[0]+'</td><td'+c+'>'+x[1]+'</td></tr>';
    }).join('');
  }catch(e){ lv.textContent='no data'; }
}
up(); setInterval(up,2000);
</script>)rawl";

String jsonState() {
  String j; j.reserve(760);
  j  = "{"
    "\"fw\":\""        + String(FW_REV) + "\"," +
    "\"valid\":"     + String(rd.valid ? 1 : 0) +
    ",\"level_cm\":" + String(isnan(rd.levelCm) ? -1 : rd.levelCm, 1) +
    ",\"pct\":"      + String(isnan(rd.pct) ? -1 : rd.pct, 1) +
    ",\"litres\":"   + String(isnan(rd.litres) ? -1 : rd.litres, 0) +
    ",\"dist_cm\":"  + String(isnan(rd.distCm) ? -1 : rd.distCm, 1) +
    ",\"echo_us\":"  + String((long)lastEchoUs) +
    ",\"echo_reason\":\"" + String(reasonText(lastReason)) + "\"" +
    ",\"air_t\":"    + String(cfg.airTC, 1) +
    ",\"sound_mms\":"+ String(speedOfSoundMmPerUs(cfg.airTC) * 1000.0f, 1) +
    ",\"vbat\":"     + String(isnan(vbat) ? -99 : vbat, 2) +
    ",\"vpct\":"     + String(vbatPct(vbat), 0) +
    ",\"rssi\":"     + String(WiFi.isConnected() ? WiFi.RSSI() : 0) +
    ",\"uptime\":"   + String(millis() / 1000) +
    ",\"bad\":"      + String(flt.badCount) +
    ",\"n\":"        + String(flt.totalCount) +
    ",\"n_near\":"   + String(flt.nNear) +
    ",\"n_noecho\":" + String(flt.nNoEcho) +
    ",\"n_far\":"    + String(flt.nFar) +
    ",\"gate_cm\":"  + String(cfg.blindCm, 1) +
    ",\"span_cm\":"  + String(spanCm(), 1) +
    ",\"status\":\"" + String(statusText()) + "\"" +
    ",\"al_low\":"   + String(alrm.low ? 1 : 0) +
    ",\"al_high\":"  + String(alrm.high ? 1 : 0) +
    ",\"al_fault\":" + String(alrm.fault ? 1 : 0) +
    ",\"low_cm\":"   + String(cfg.lowCm, 1) +
    ",\"high_cm\":"  + String(cfg.highCm, 1) +
    ",\"mute\":"     + String(cfg.mute) +
    ",\"d_empty\":"  + String(cfg.dEmptyCm, 1) +
    ",\"d_full\":"   + String(cfg.dFullCm, 1) +
    ",\"area_cm2\":" + String(cfg.areaCm2, 0) +
    ",\"sample_s\":" + String(cfg.sampleSec) +
    "}";
  return j;
}
void httpRoutes() {
  server.on("/",          []() { server.send_P(200, "text/html", PAGE); });
  server.on("/api/state", []() { server.send(200, "application/json", jsonState()); });
  server.on("/api/log",   []() { takeSample(true); server.send(200, "text/plain", "logged"); });
  server.on("/api/scan",  []() {
    int n = WiFi.scanNetworks(); String s;
    for (int i = 0; i < n; i++) s += WiFi.SSID(i) + "," + String(WiFi.RSSI(i)) + "\n";
    WiFi.scanDelete(); server.send(200, "text/plain", s);
  });
  server.begin();
}

/* ======================= 11. SERIAL COMMANDS ============================ */
void help() {
  Serial.println(F(
    "commands:\n"
    "  r            one raw ping: pulse width, distance, verdict\n"
    "  s            JSON status\n"
    "  c            take one sample now + print CSV\n"
    "  w            WiFi scan (RSSI test)      w <ssid>  set SSID\n"
    "  p <pass>     set WiFi password\n"
    "  e <cm>       face distance, EMPTY tank     f <cm>  face distance, FULL line\n"
    "             (both are DISTANCES from the probe face; the span between them is the water)\n"
    "  a <cm2>      set tank area (92903 = 10 ft x 10 ft)\n"
    "  g <cm>       blind gate — reject anything closer (module's own blind zone is 20 cm)\n"
    "  t <degC>     air temperature, drives sound speed — no sensor for this in this build\n"
    "  i <sec>      sample interval                v  print calibration + config\n"
    "  b            battery voltage\n"
    "  l <cm>       low alarm level           u <cm>  high alarm level   j  mute/unmute buzzer\n"
    "  d            redraw the OLED now       D       flip the OLED 180 degrees\n"
    "  k            Blynk link status + one test push + one test event\n"
    "             (k is not compiled at all when ENABLE_BLYNK is 0)\n"
    "  x            erase NVS and reboot"));
}
void printConfig() {
  Serial.printf("calibration: d_empty=%.1f  d_full=%.1f  span=%.1f cm of water  (set with e / f)\n",
                cfg.dEmptyCm, cfg.dFullCm, spanCm());
  Serial.printf("blind gate = %.1f cm (module's own blind area is 20 cm)   air_t = %.0f °C → %.3f m/s\n",
                cfg.blindCm, cfg.airTC, speedOfSoundMmPerUs(cfg.airTC) * 1000.0f);
  Serial.printf("tank area = %.0f cm² → %.1f L per cm → %.0f L at full\n",
                cfg.areaCm2, cfg.areaCm2 / 1000.0f, spanCm() * cfg.areaCm2 / 1000.0f);
  Serial.printf("alarms: low <= %.1f cm  high >= %.1f cm  hysteresis %.1f cm  buzzer %s\n",
                cfg.lowCm, cfg.highCm, ALARM_HYST_CM, cfg.mute ? "muted" : "armed");
  Serial.printf("pings: %lu good / %lu total (near %lu, no-echo %lu, far %lu), every %u s\n",
                (unsigned long)(flt.totalCount - flt.badCount), (unsigned long)flt.totalCount,
                (unsigned long)flt.nNear, (unsigned long)flt.nNoEcho, (unsigned long)flt.nFar,
                cfg.sampleSec);
}
void handleSerial() {
  static String buf;
  while (Serial.available()) buf += (char)Serial.read();
  int nl = buf.indexOf('\n'); if (nl < 0) return;
  String cmd = buf.substring(0, nl); buf.remove(0, nl + 1);
  cmd.trim(); if (cmd.length() == 0) return;
  char c = cmd[0];
  if      (c == 'r') { float raw; uint8_t why;
                       bool v = pingRaw(raw, why);
                       Serial.printf("ping: %lu us  = %.1f cm  [%s]   gate %.1f cm  air_t %.0f °C (%.3f m/s)\n",
                                     (unsigned long)lastEchoUs, isnan(raw) ? -1.0f : raw,
                                     reasonText(why), cfg.blindCm, cfg.airTC,
                                     speedOfSoundMmPerUs(cfg.airTC) * 1000.0f);
                       Serial.printf("     if this is 0 us, check: 5 V on the module, TRIG on %u, ECHO on %u, and R19 open (mode 1)\n",
                                     PIN_TRIG, PIN_ECHO); }
  else if (c == 's') Serial.println(jsonState());
  else if (c == 'c') takeSample(true);
  else if (c == 'v') printConfig();
  else if (c == 'w') { if (cmd.length() > 2) { cmd.substring(2).toCharArray(cfg.ssid, 33);
                                              cfgSave();
                                              Serial.printf("ssid = '%s'  (now: p <password>)\n", cfg.ssid); }
                       else wifiScanPrint(); }
  else if (c == 'p') { cmd.substring(2).toCharArray(cfg.pass, 65); cfgSave();
                       Serial.println(F("password saved")); }
  else if (c == 'e') { cfg.dEmptyCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("d_empty = %.1f cm  → span %.1f cm\n", cfg.dEmptyCm, spanCm()); }
  else if (c == 'f') { cfg.dFullCm  = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("d_full  = %.1f cm  → span %.1f cm\n", cfg.dFullCm, spanCm()); }
  else if (c == 'a') { cfg.areaCm2  = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("area = %.0f cm²  → %.1f L per cm\n",
                                     cfg.areaCm2, cfg.areaCm2 / 1000.0f); }
  else if (c == 'g') { cfg.blindCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("gate = %.1f cm — anything the module reports closer than this is thrown away\n",
                                     cfg.blindCm); }
  else if (c == 't') { cfg.airTC = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("air_t = %.0f °C → sound speed %.3f m/s (%.2f cm error per 15 °C at 150 cm)\n",
                                     cfg.airTC, speedOfSoundMmPerUs(cfg.airTC) * 1000.0f,
                                     0.00177f * 15.0f * cfg.dEmptyCm); }
  else if (c == 'i') { cfg.sampleSec = cmd.substring(1).toInt(); cfgSave();
                       Serial.printf("sample interval = %u s\n", cfg.sampleSec); }
  else if (c == 'b') Serial.printf("vbat = %.3f V  ≈ %.0f %%\n", vbat, vbatPct(vbat));
  else if (c == 'l') { cfg.lowCm  = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("low alarm  = %.1f cm  (%.0f %% of %.1f cm)",
                                     cfg.lowCm, cfg.lowCm / spanCm() * 100.0f, spanCm());
                       evaluateAlarms(); }
  else if (c == 'u') { cfg.highCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("high alarm = %.1f cm  (%.0f %% of %.1f cm)",
                                     cfg.highCm, cfg.highCm / spanCm() * 100.0f, spanCm());
                       evaluateAlarms(); }
  else if (c == 'j') { cfg.mute = cfg.mute ? 0 : 1; cfgSave();
                       Serial.printf("buzzer %s (button: short = snooze, hold 1.5 s = this)\n",
                                     cfg.mute ? "muted" : "armed"); }
#if ENABLE_OLED
  else if (c == 'd') { oledRequestDraw(); Serial.println(F("oled: redrawn")); }
  else if (c == 'D') { cfg.oledFlip = cfg.oledFlip ? 0 : 1; cfgSave(); oledSetFlip(cfg.oledFlip != 0);
                       Serial.printf("oled flip = %d\n", cfg.oledFlip); }
#endif
#if BLYNK_ON
  else if (c == 'k') {
    Serial.printf("blynk: library yes, token %s, template %s, link %s\n",
                  BLYNK_AUTH_TOKEN[0] ? "set" : "EMPTY",
                  BLYNK_TEMPLATE_ID[0] ? BLYNK_TEMPLATE_ID : "EMPTY",
                  Blynk.connected() ? "up" : "down");
    if (!Blynk.connected()) Serial.println(F("   no link: check token, template ID, and that wifiConnect() got an IP"));
    else { blynkPush(); Serial.println(F("   pushed one sample set")); }
    blynkNotify(BLYNK_EV_RECOVER, String("Test notification from the gauge — if this arrived, the event code is right."));
  }
#endif
  else if (c == 'x') { prefs.begin("wlg", false); prefs.clear(); prefs.end();
                       Serial.println(F("NVS cleared — rebooting")); delay(300); ESP.restart(); }
  else if (c == 'h') help();
  else               help();
}

/* ======================= 12. SETUP / LOOP ============================== */
void setup() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);      // D12: internal pull-up, button to GND — as in the reference build
  pinMode(PIN_BUZZ, OUTPUT);

  Serial.begin(115200);
  delay(400);
  Serial.println(F("\n=== TANK WATER LEVEL GAUGE — phase 0, SR04M ultrasonic ==="));
  Serial.printf("pins: TRIG=%u ECHO=%u(divider!) SDA=%u SCL=%u BTN=%u(strapping) BUZZ=%u LED=%u VBAT=%u\n",
                PIN_TRIG, PIN_ECHO, PIN_SDA, PIN_SCL, PIN_BTN, PIN_BUZZ, PIN_LED, PIN_VBAT);
  Serial.printf("outputs: oled=%d | blynk=%s | web=http://<ip>/\n", ENABLE_OLED,
                BLYNK_ON ? (BLYNK_AUTH_TOKEN[0] ? "configured" : "compiled in, token empty -> off")
                         : "ENABLE_BLYNK=0");

  cfgLoad();
  Serial.printf("nvs: ssid='%s' d_empty=%.1f d_full=%.1f span=%.1f cm  area=%.0f cm² (%.1f L/cm)  every %u s\n",
                cfg.ssid, cfg.dEmptyCm, cfg.dFullCm, spanCm(), cfg.areaCm2,
                cfg.areaCm2 / 1000.0f, cfg.sampleSec);
  Serial.printf("nvs: gate=%.1f cm  air_t=%.0f C (%.3f m/s)  alarms low<%.1f high>%.1f hyst %.1f  buzzer %s\n",
                cfg.blindCm, cfg.airTC, speedOfSoundMmPerUs(cfg.airTC) * 1000.0f,
                cfg.lowCm, cfg.highCm, ALARM_HYST_CM, cfg.mute ? "muted" : "armed");
  if (cfg.dFullCm < cfg.blindCm + 5.0f)
    Serial.printf("!! d_full = %.1f cm leaves under 5 cm of margin over the gate (module blind zone is 20 cm).\n"
                  "   The gauge goes blind just when the tank is nearly full, and swell or face dew eat the\n"
                  "   margin. Raise the probe face: docs/03-ultrasonic.md\n", cfg.dFullCm);

  oledSetup();

  wifiConnect();
  httpRoutes();
  blynkSetup();
  Serial.println(F("csv header: ts_s,echo_us,raw_cm,reason,filt_cm,level_cm,pct,litres,air_t,gate_cm,vbat,rssi,bad,n"));
  Serial.println(F("capture it: arduino-cli monitor -p COM6 -c baudrate=115200 | tee wlg.csv"));
  Serial.println(F("                  (then strip the banner lines before analysis)"));
  help();
}

void loop() {
  static uint32_t tSample = 0, tScan = 0, tHb = 0, tVbat = 0;
  uint32_t now = millis();

  server.handleClient();
  handleSerial();
  buttonService(now);          // wake the screen / snooze / mute
  buzzerService(now);          // local half of the alarm: works with no internet at all
  oledService(now);
  blynkService(now);           // Blynk.run() + a bounded reconnect

  /* heartbeat, and solid on for the full/overflow alarm — the reference build's green
   * "tank full" LED, with a second job added so the box also says it is alive */
  if (alrm.high) digitalWrite(PIN_LED, HIGH);
  else if (now - tHb > 1500) { tHb = now; digitalWrite(PIN_LED, !digitalRead(PIN_LED)); }

  if (now - tSample >= (uint32_t)cfg.sampleSec * 1000UL) {
    tSample = now;
    takeSample(true);                    // -> evaluateAlarms() + outputsUpdate()
    if (flt.totalCount > 8 && flt.badCount * 4 > flt.totalCount)
      Serial.println(F("!! >25% of pings rejected — dew or algae on the face, a surface inside the "
                       "gate, a rippling/foamy surface giving no echo, or R19 not open (mode 1)"));
  }

  if (now - tVbat > 60000) { tVbat = now; vbat = readVbat(); }
  if (now - tScan > 600000) { tScan = now; if (WiFi.isConnected()) wifiScanPrint(); }
  if (!WiFi.isConnected() && WiFi.getMode() == WIFI_STA && now > 30000) {
    static uint32_t tRetry = 0;
    if (now - tRetry > 20000) { tRetry = now; Serial.println(F("wifi: retrying")); WiFi.disconnect(); WiFi.begin(cfg.ssid, cfg.pass); }
  }
  delay(10);
}