/* ============================================================================
 *  TANK WATER LEVEL GAUGE — phase 0 bring-up sketch  (Arduino IDE)
 *
 *  Board : ESP32-WROOM-32 DevKitC V1, 38-pin (CP2102)  →  in the Arduino IDE pick
 *          "DOIT ESP32 DEVKIT V1"   (or "ESP32 Dev Module")
 *  Core  : "esp32 by Espressif Systems"  (any recent version; 2.0.x / 3.x both OK)
 *
 *  NO EXTERNAL LIBRARIES NEEDED.  Only what ships with the ESP32 core is used:
 *  Wire, WiFi, WebServer, Preferences(NVS).  BME280/BMP280, the DS18B20 and the
 *  SSD1306 OLED are driven directly, so there is nothing you have to install in
 *  the Library Manager just to build this.
 *
 *  OPTIONAL cloud/app layer: the "Blynk" library adds a phone dashboard and push
 *  notifications, same shape as the video everyone asks about
 *  (https://youtu.be/9geREeE13jc — "IoT Based Water Level Monitoring using ESP32,
 *  Blynk & Ultrasonic Sensor", Tech StudyCell). It is strictly additive and it
 *  fails safe: set ENABLE_BLYNK 0, or leave the token blank, or fit no OLED, and
 *  this is still the same LAN-only gauge with the same CSV log. Before you put a
 *  real token in, read docs/05-blynk-and-display.md: quota maths, and why the
 *  token is a password.
 *
 *  WHAT THIS PHASE PROVES (before any PCB exists):
 *    1. build 2 (default): the pressure difference between the tube sensor and the
 *       box sensor tracks the water level, the tube stays dry, and the tank air
 *       space is properly vented.  Send 'r' — it prints both absolute pressures.
 *    2. the filtered level value is stable over 24 h (CSV log)
 *    3. there is usable WiFi with the box ON TOP of the metal lid  (send 'w')
 *
 *  Serial: 115200.  Commands:  r s c w p e f a o z m i b x l u j d D k  (see handleSerial())
 *  Web   : http://<ip>/            test page        http://<ip>/api/state  JSON
 *  OLED  : optional 0.96" SSD1306 128x64 on the same I2C bus as the pressure sensors
 *          (0x3C or 0x3D; the pressure sensors are at 0x76 / 0x77 — no clash)
 *  If WiFi credentials are wrong/empty it opens AP "WaterLevel-Setup" / "waterlevel".
 *
 *  ⚠ WIRING WARNING (see docs/img/01-wiring.svg):
 *    - The ESP32 is NOT 5 V tolerant. The module's ECHO line must go through the
 *      2 kΩ / 3.3 kΩ divider → 3.00 V.  Feeding 5 V into GPIO26 kills the pin.
 *    - Do NOT wire anything to the pins printed D6 D7 D8 D9 D10 D11 — those are the
 *      SPI flash lines. They are broken out on this board and they look usable.
 * ==========================================================================*/

/* ======================= 0. WHAT IS SWITCHED ON ==========================
 * Compile-time switches. Both stay compiled-in by default but switch themselves
 * OFF at runtime when the hardware/credentials they need are missing, so leaving
 * them at 1 can never brick the build.
 *     arduino-cli lib install "Blynk"      (Library Manager -> "Blynk", Blynk Inc)
 * ------------------------------------------------------------------------*/
#ifndef ENABLE_OLED
#define ENABLE_OLED   1        // SSD1306 128x64 on I2C — driver is wlg_display.h, no library
#endif
#ifndef ENABLE_BLYNK
#define ENABLE_BLYNK  1        // Blynk IoT cloud + phone app — needs the Blynk library
#endif

/* ---- paste these from Blynk Console -> your device -> Info -> Firmware -----
 * Blynk IoT 2.x will not compile without TEMPLATE_ID and TEMPLATE_NAME, and the
 * token is a credential: whoever holds it can read AND write this device. Keep a
 * filled-in token out of git, and re-issue it if this sketch ever goes public.   */
#define BLYNK_TEMPLATE_ID   ""                  // e.g. "TMPLAB12CD34"
#define BLYNK_TEMPLATE_NAME "Water Tank Gauge"
#define BLYNK_AUTH_TOKEN    ""                  // the 32-character device token
#define BLYNK_SERVER        ""                  // "" = blynk.cloud, else "192.168.1.50"
#define BLYNK_SERVER_PORT   80                  // cloud uses 80; self-hosted usually 8080

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
/* change these, then upload. Everything else is derived.                    */

#define SENSOR_ULTRASONIC  1        // build 1: waterproof ultrasonic + printed chimney
#define SENSOR_BARO_DUAL   2        // build 2: hydrostatic differential, vented tube to the floor
#ifndef SENSOR_TYPE                 // you can also set this from arduino-cli with
#define SENSOR_TYPE        SENSOR_BARO_DUAL   // --build-property compiler.cpp.extra_flags=-DSENSOR_TYPE=1
#endif

/* build 2 part choice — this is not a detail, see docs/03-hydrostatic.md:
 *   TUBE side must be a BMP280: its measuring range is 300…1200 hPa. The 121.9 cm
 *     water column adds ≈120 hPa, so at sea level that sensor sees ≈1133 hPa, which is
 *     PAST the BME280's 1100 hPa top end. A BME280 there reads clipped/non-linear.
 *   BOX (reference) side can be a BMP280 or a BME280. A BME280 also logs %RH, which is
 *     how you watch the condensation risk in the box — that is what kills these sensors.
 *   Both share the I2C bus: strap one to 0x76 and one to 0x77 (SD0/AD0 pin). */

/* pin map — verified against the 38-pin DevKitC/NodeMCU-32S pin order */
#define PIN_TRIG     25             // out  → ultrasonic TRIG
#define PIN_ECHO     26             // in   ← ultrasonic ECHO  (through the divider!)
#define PIN_SDA      21             // I2C SDA (default pin on this core)
#define PIN_SCL      22             // I2C SCL
#define PIN_ONEWIRE  27             // DS18B20 data, 4.7 kΩ pull-up to 3V3
#define PIN_LVL_ANA  33             // ADC1_CH5 — analog-output sensor option, unused for type 1
#define PIN_VBAT     36             // ADC1_CH0 — battery sense, 1 MΩ/1 MΩ divider, INPUT ONLY pin
#define PIN_LED      18             // heartbeat LED + 330 Ω  (GPIO2 is a strapping pin — avoided)
#define PIN_BUZZ     17             // piezo alarm via S8050 transistor — the LOCAL half of the alarm
#define PIN_BTN      23             // button to GND: wakes the OLED, silences the buzzer
#define PIN_FLOW     19             // optional flow-sensor reed contact (dry-run detection later)

/* sensor physics / filtering */
#define BLIND_ZONE_CM      35.0f    // echoes closer than this are rejected (see design doc §2.3)
#define FILTER_N           15       // median window
#define MAX_JUMP_CM        12.0f    // bigger single-sample step than this needs confirming
#define CONFIRM_COUNT      3        // consecutive reads needed to accept such a step
#define HOLD_SAMPLES       20       // keep last good value for this many reads

/* defaults, all overridable from the serial console and stored in NVS */
const char   DEF_SSID[]   = "";
const float  DEF_D_EMPTY  = 174.0f;  // raw distance, empty tank, at the chimney height
const float  DEF_D_FULL   = 52.2f;   // raw distance at the full water line
const float  DEF_AREA_CM2 = 92903.0f;// 10 ft × 10 ft ⇒ 92.9 L per cm of level
const float  DEF_MAX_LEVEL_CM = 121.9f;   // build 2 full scale = the water depth (4 ft)
const uint16_t DEF_SAMPLE_S = 60;    // bring-up rate; for the real deployment: 'i 900' (15 min)

/* Alarms — the video's "beep locally and buzz the phone" behaviour. The free Blynk
 * plan allows 5 events per template, so low / high / fault / recovered is the whole
 * budget. Hysteresis and a cooldown are what keep the phone quiet: a level parked
 * exactly on the threshold would otherwise notify over and over. */
const float    DEF_LOW_CM     = 24.0f;   // ~20 % of 121.9 cm. The app slider (V11) moves it
const float    DEF_HIGH_CM    = 120.0f;  // ~2 cm of margin below the overflow (V12)
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
#define BLYNK_SLOW_S          3600       // temperatures / pressures / RSSI: hourly
#define BLYNK_FREE_MSGS       100000L    // Blynk free plan device messages per calendar month

/* ======================= 2. TYPES AND GLOBALS ============================= */
struct Config {
  char    ssid[33];  char pass[65];
  float   dEmptyCm, dFullCm, areaCm2, baroOffsetCm, maxLevelCm;
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
} flt;

float airT = NAN, airRH = NAN, airPa = NAN, waterT = NAN, vbat = NAN;
float pRefPa = NAN, pTubePa = NAN;   // build 2: the two absolute pressures, in Pa

Preferences prefs;
WebServer   server(80);

/* Alarm state — declared here so the OLED and the transport can both read it.
 * The state machine that writes it is section 10. */
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

/* ======================= 3. BME280 DRIVER (raw I2C, no library) =========== */
/* Addresses: 0x76 (SD0→GND, most breakouts) and 0x77 (SD0→3V3).             */
#define BME_ADDR_A 0x76u          // in the box: air T / RH / absolute P
#define BME_ADDR_B 0x77u          // only for SENSOR_TYPE 2: tube to the tank floor

uint8_t  bmeA = BME_ADDR_A, bmeB = BME_ADDR_B;
bool     haveA = false, haveB = false;

struct BmeCal {
  uint16_t T1, P1, H1u;
  int16_t  T2, T3, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11;
  int16_t  H2, H4, H5;
  uint8_t  H3;
  int8_t   H6;
} cal;                         // working copy — bmeReadAll() loads the right slot first
BmeCal   calSlot[2];           // each sensor has its OWN trim words. if both sensors share one
                               // set of trims the difference is off by ~1 hPa = ~1 cm of water,
                               // and in the hydrostatic build that difference IS the measurement.
bool     isBme[2];             // true = BME280 (has humidity), false = BMP280
#define SLOT(a) ((a) == BME_ADDR_A ? 0 : 1)
int32_t  tFine = 0;              // shared by the compensation functions

uint8_t  i2cRead(uint8_t a, uint8_t r) {
  Wire.beginTransmission(a); Wire.write(r);
  if (Wire.endTransmission() != 0) return 0;
  if (Wire.requestFrom((int)a, 1) != 1) return 0;
  return Wire.read();
}
uint16_t i2cRead16LE(uint8_t a, uint8_t r) {
  Wire.beginTransmission(a); Wire.write(r);
  if (Wire.endTransmission() != 0) return 0;
  if (Wire.requestFrom((int)a, 2) != 2) return 0;
  uint8_t l = Wire.read(), h = Wire.read();
  return (uint16_t)(l | (h << 8));
}
void i2cWrite(uint8_t a, uint8_t r, uint8_t v) {
  Wire.beginTransmission(a); Wire.write(r); Wire.write(v); Wire.endTransmission();
}

bool bmeInit(uint8_t a) {
  uint8_t id = i2cRead(a, 0xD0);
  isBme[SLOT(a)] = (id == 0x60 || id == 0x61 || id == 0x62);   // BME280
  bool isBmp280  = (id == 0x58);                               // BMP280 — the tube-side part
  if (!isBme[SLOT(a)] && !isBmp280) return false;
  i2cWrite(a, 0xE0, isBmp280 ? 0xB6 : 0x00);                   // soft reset command differs by part
  delay(10);
  /* --- trim words (Bosch datasheet §4.2.2) --- */
  cal.T1 = i2cRead16LE(a, 0x88);
  cal.T2 = (int16_t)i2cRead16LE(a, 0x8A);
  cal.T3 = (int16_t)i2cRead16LE(a, 0x8C);
  cal.P1 = i2cRead16LE(a, 0x8E);
  cal.P2 = (int16_t)i2cRead16LE(a, 0x90);
  cal.P3 = (int16_t)i2cRead16LE(a, 0x92);
  cal.P4 = (int16_t)i2cRead16LE(a, 0x94);
  cal.P5 = (int16_t)i2cRead16LE(a, 0x96);
  cal.P6 = (int16_t)i2cRead16LE(a, 0x98);
  cal.P7 = (int16_t)i2cRead16LE(a, 0x9A);
  cal.P8 = (int16_t)i2cRead16LE(a, 0x9C);
  cal.P9 = (int16_t)i2cRead16LE(a, 0x9E);
  cal.P10= (int16_t)i2cRead16LE(a, 0xA0);
  cal.P11= (int16_t)i2cRead16LE(a, 0xA2);
  if (isBme[SLOT(a)]) {                           // a BMP280 has no humidity block at all
    cal.H1u = i2cRead(a, 0xA1);
    cal.H2  = (int16_t)i2cRead16LE(a, 0xE1);
    cal.H3  = i2cRead(a, 0xE3);
    cal.H4  = (int16_t)(((int16_t)i2cRead(a, 0xE4) << 4) | ( i2cRead(a, 0xE5) & 0x0F));
    cal.H5  = (int16_t)(((int16_t)i2cRead(a, 0xE6) << 4) | ( i2cRead(a, 0xE5) >> 4));
    cal.H6  = (int8_t) i2cRead(a, 0xE7);
    i2cWrite(a, 0xF2, 0x05);                      // ctrl_hum: humidity oversampling ×16
  }
  /* normal mode, P×16 T×2 (H×16 on a BME280), standby 500 ms, IIR off-filter 4.
     P×16 gives ≈0.02 hPa RMS ≈ 0.2 mm of water — far finer than the mechanics allow. */
  i2cWrite(a, 0xF4, 0xB7);                        // ctrl_meas: osrs_p=101 osrs_t=011 mode=normal
  i2cWrite(a, 0xF5, 0xA0);                        // config: standby 500 ms, iir 4
  calSlot[SLOT(a)] = cal;                         // keep this sensor's own trim words
  return true;
}
/* Bosch compensation, returns x0.01 °C */
int32_t bmeCompTemp(int32_t adcT) {
  int32_t v1 = ((((adcT >> 3) - ((int32_t)cal.T1 << 1))) * (int32_t)cal.T2) >> 11;
  int32_t v2 = (((((adcT >> 4) - (int32_t)cal.T1) * ((adcT >> 4) - (int32_t)cal.T1)) >> 12)
                * (int32_t)cal.T3) >> 14;
  tFine = v1 + v2;
  return (tFine * 5 + 128) >> 8;
}
/* Q24.8 fixed point, divided by 256 at the end → Pa.
   Mirrors the Bosch int64 compensation exactly. If you ever see sea-level
   pressure reported as ~26 million, this /256 is the line to look at. */
float bmeCompPress(int32_t adcP) {
  int64_t  v1, v2;
  uint32_t p;
  v1 = (int64_t)tFine - 128000;
  v2 = v1 * v1 * (int64_t)cal.P6;
  v2 = v2 + ((v1 * (int64_t)cal.P5) << 17);
  v2 = v2 + (((int64_t)cal.P4) << 35);
  v1 = ((v1 << 1) * (int64_t)cal.P2) >> 11;
  v1 = ((((int64_t)1 << 47) + v1)) * (int64_t)cal.P3 >> 33;
  if (v1 == 0 || v2 == 0) return NAN;
  p = 1048576 - adcP;
  p = (uint32_t)((((int64_t)p << 31) - v2) * 3125 / v1);
  v1 = (((int64_t)cal.P9) * (p >> 13) * (p >> 13)) >> 25;
  v2 = (((int64_t)cal.P8) * (int64_t)p) >> 19;
  p  = (uint32_t)((int64_t)p + v1 + v2 + (((int64_t)cal.P7) << 4));
  return (float)p / 256.0f;
}
/* returns x1000 %RH — Bosch float-free compensation, written with named
   intermediates because the one-expression version is impossible to audit. */
uint32_t bmeCompHumi(int32_t adcH) {
  int32_t v = tFine - 76800;
  int32_t a = adcH << 14;
  a = a - ((int32_t)cal.H4 << 20);
  a = a - ((int32_t)cal.H5 * v);
  a = (a + 16384) >> 15;
  int32_t b = (v * (int32_t)cal.H6) >> 10;
  int32_t c = ((v * (int32_t)cal.H3) >> 11) + 32768;
  b = ((b * c) >> 10) + 2097152;
  b = (b * (int32_t)cal.H2 + 8192) >> 14;
  v = a * b;
  v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)cal.H1u) >> 4);
  if (v < 0) v = 0;
  if (v > 419430400L) v = 419430400L;
  return (uint32_t)(v >> 12);
}
void bmeReadAll(uint8_t a, float &T, float &RH, float &Pa) {
  T = NAN; RH = NAN; Pa = NAN;
  cal = calSlot[SLOT(a)];                 // this sensor's trims, never the other sensor's
  int32_t p_raw = ((int32_t)i2cRead(a,0xF7) << 12) | ((int32_t)i2cRead(a,0xF8) << 4) | (i2cRead(a,0xF9) >> 4);
  int32_t t_raw = ((int32_t)i2cRead(a,0xFA) << 12) | ((int32_t)i2cRead(a,0xFB) << 4) | (i2cRead(a,0xFC) >> 4);
  int32_t h_raw = ((int32_t)i2cRead(a,0xFD) << 8) | i2cRead(a,0xFE);
  float t = bmeCompTemp(t_raw) / 100.0f;
  float p = bmeCompPress(p_raw);
  float h = isBme[SLOT(a)] ? bmeCompHumi(h_raw) / 1000.0f : NAN;
  T = t; Pa = p; RH = (isnan(h) || h < 0 || h > 100) ? NAN : h;
}

/* ======================= 4. DS18B20 (bit-bang, no library) =============== */
/* CRC-8 is computed inline in readWaterTemp() over the 9-byte scratchpad */
void owReset() {
  noInterrupts();
  pinMode(PIN_ONEWIRE, OUTPUT); digitalWrite(PIN_ONEWIRE, LOW);
  delayMicroseconds(480);
  pinMode(PIN_ONEWIRE, INPUT);
  uint32_t t0 = micros();
  while (digitalRead(PIN_ONEWIRE) == HIGH && micros() - t0 < 100) ;
  interrupts();
  delayMicroseconds(120);
}
void owBit(uint8_t b) {
  noInterrupts();
  pinMode(PIN_ONEWIRE, OUTPUT); digitalWrite(PIN_ONEWIRE, LOW);
  delayMicroseconds(b ? 6 : 60);
  pinMode(PIN_ONEWIRE, INPUT);
  interrupts();
  delayMicroseconds(b ? 64 : 10);
}
uint8_t owReadBit() {
  noInterrupts();
  pinMode(PIN_ONEWIRE, OUTPUT); digitalWrite(PIN_ONEWIRE, LOW);
  delayMicroseconds(2);
  pinMode(PIN_ONEWIRE, INPUT);
  delayMicroseconds(12);
  uint8_t v = digitalRead(PIN_ONEWIRE);
  interrupts();
  delayMicroseconds(50);
  return v;
}
uint8_t owReadByte() { uint8_t v = 0; for (uint8_t i = 0; i < 8; i++) v |= (owReadBit() << i); return v; }

float readWaterTemp() {
  owReset();
  if (!owReadBit()) return NAN;                 // no presence pulse
  owBit(1); owBit(1); owBit(0); owBit(0);       // 0xCC  skip ROM
  owBit(0); owBit(1); owBit(0); owBit(0);       // 0x44  convert T, non-blocking
  delay(780);                                   // 12-bit conversion = 750 ms
  owReset();
  if (!owReadBit()) return NAN;
  owBit(1); owBit(1); owBit(0); owBit(0);       // skip ROM
  owBit(1); owBit(1); owBit(1); owBit(1);       // 0xBE  read scratchpad
  uint8_t sp[9]; for (uint8_t i = 0; i < 9; i++) sp[i] = owReadByte();
  uint8_t crc = 0;
  for (uint8_t i = 0; i < 8; i++) {             // CRC-8 over the first 8 bytes
    crc ^= sp[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 1) ? (uint8_t)((crc >> 1) ^ 0x8C) : (crc >> 1);
  }
  if (crc != sp[8]) return NAN;                 // corrupted scratchpad — ignore
  int16_t raw = (int16_t)(sp[0] | (sp[1] << 8));
  if (raw == 0 || raw == -1) return NAN;        // unconnected / sensing error
  return raw * 0.0625f;
}

/* ======================= 5. DISTANCE AND LEVEL =========================== */
float speedOfSoundMmPerUs(float tC) {                  // dry air approximation
  float t = isnan(tC) ? 25.0f : tC;
  return (331.3f + 0.606f * t) / 1000.0f;              // mm per µs
}
bool pingRaw(float &cm) {                              // JSN-SR04T / HC-SR04 protocol
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(12);
  digitalWrite(PIN_TRIG, LOW);
  uint32_t us = pulseIn(PIN_ECHO, HIGH, 30000);        // 30 ms ceiling ≈ 5.2 m
  if (us == 0) return false;                           // no echo at all
  cm = (0.5f * (float)us * speedOfSoundMmPerUs(airT)) / 10.0f;
  return cm > BLIND_ZONE_CM;                           // face dew / wall echo gate
}
float waterDensity(float tC) {                  // kg/m³; linear interpolation of the standard table
  const float TC[] = {0,5,10,15,20,25,30,35,40,45,50};
  const float DC[] = {999.84f,999.97f,999.70f,999.10f,998.21f,997.05f,
                      995.65f,994.04f,992.22f,990.21f,988.04f};
  if (isnan(tC)) tC = 25.0f;
  if (tC < 0.0f)  tC = 0.0f;
  if (tC > 50.0f) tC = 50.0f;
  int i = (int)(tC / 5.0f); if (i > 9) i = 9;
  return DC[i] + ((tC - TC[i]) / 5.0f) * (DC[i + 1] - DC[i]);
}
bool baroRaw(float &cm) {                       // SENSOR_TYPE == SENSOR_BARO_DUAL
  if (!haveA) return false;
  float T, RH, Pa;
  bmeReadAll(bmeA, T, RH, Pa);                  // box-side reference (also gives box climate)
  airT = T; airRH = RH; airPa = Pa; pRefPa = Pa;
  if (haveB) bmeReadAll(bmeB, T, RH, Pa);
  pTubePa = haveB ? Pa : airPa;
  float dpPa = pTubePa - pRefPa;                // ≈9.78 Pa per cm of water at 25 °C
  if (!haveB || isnan(dpPa)) return false;
  if (pTubePa > 120000.0f)                      // beyond the BMP280's 1200 hPa measuring range
    Serial.println(F("!! tube-side pressure above 1200 hPa — over range: water in the tube?"));
  float rho = waterDensity(isnan(waterT) ? airT : waterT);
  cm = dpPa * 100.0f / (rho * 9.7803f) + cfg.baroOffsetCm;
  return cm > -5.0f && cm < cfg.maxLevelCm + 15.0f;
}
bool readRaw(float &cm) {
#if SENSOR_TYPE == 1
  return pingRaw(cm);
#else
  return baroRaw(cm);
#endif
}
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
  float raw = NAN;
  bool valid = readRaw(raw);
  flt.totalCount++; if (!valid) flt.badCount++;
  float dist = filterUpdate(raw, valid);
  bool ok = !isnan(dist);
  rd.whenMs = millis(); rd.valid = ok;
  if (ok) {
#if SENSOR_TYPE == 1
    float span = cfg.dEmptyCm - cfg.dFullCm;
    rd.distCm  = dist;
    rd.levelCm = constrain(cfg.dEmptyCm - dist, 0.0f, span);
    rd.pct     = constrain(rd.levelCm / span * 100.0f, 0.0f, 100.0f);
#else
    rd.distCm  = NAN;
    rd.levelCm = dist;
    rd.pct     = constrain(rd.levelCm / cfg.maxLevelCm * 100.0f, 0.0f, 100.0f);
#endif
    rd.litres  = rd.levelCm * cfg.areaCm2 / 1000.0f;   // cm × cm² = cm³, ÷1000 = litres
  }
  evaluateAlarms();   // thresholds, hysteresis, notification cooldown
  outputsUpdate();    // OLED + app. Both are no-ops when switched off or unconfigured.
  if (verbose) {
    Serial.printf("%lu,%.1f,%s,%.1f,%.1f,%.2f,%.0f,%.1f,%.1f,%.2f,%ld,%.2f,%.2f,%lu,%lu\n",
      (unsigned long)(millis() / 1000), isnan(raw) ? -1.0f : raw, valid ? "ok" : "bad",
      isnan(dist) ? -1.0f : dist, ok ? rd.levelCm : -1.0f, ok ? rd.pct : -1.0f,
      ok ? rd.litres : -1.0f, isnan(airT) ? -99.0f : airT, isnan(waterT) ? -99.0f : waterT,
      isnan(vbat) ? -99.0f : vbat, (long)(WiFi.isConnected() ? WiFi.RSSI() : 0),
      isnan(pRefPa) ? -1.0f : pRefPa / 100.0f, isnan(pTubePa) ? -1.0f : pTubePa / 100.0f,
      (unsigned long)flt.badCount, (unsigned long)flt.totalCount);
  }
}

/* ======================= 6. BATTERY ===================================== */
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

/* ======================= 7. WHAT THE BOX SHOWS ========================== */
/* One status string feeds the OLED, the app's text stream and the web page.      */
const char *statusText() {
  static char s[24];
  if (SENSOR_TYPE == SENSOR_BARO_DUAL && !haveB)     { strcpy(s, "TUBE SENSOR MISSING"); return s; }
  if (!rd.valid && flt.totalCount > 5 && flt.badCount * 3 >= flt.totalCount)
                                                      { strcpy(s, "NO VALID READS"); return s; }
  if (!rd.valid)                                      { strcpy(s, "NO READING"); return s; }
  if (pTubePa > 120000.0f)                            { strcpy(s, "TUBE OVER RANGE"); return s; }
  if (alrm.high)                                      { strcpy(s, "HIGH - OVERFLOW"); return s; }
  if (alrm.low)                                       { strcpy(s, "LOW LEVEL"); return s; }
  if (flt.totalCount > 8 && flt.badCount * 4 > flt.totalCount)
                                                      { strcpy(s, "MANY BAD READS"); return s; }
  strcpy(s, "OK");
  return s;
}

#if ENABLE_OLED
uint32_t oledWakeMs = 0, oledTDraw = 0, oledTProbe = 0;
bool     oledAwake = true, oledDirty = true;

/* 128x64 is small: the level is a 2x-scaled number, everything else is 5x7 text,
 * and a reversed band is the only way to shout. Two decimals belong in the app. */
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

  if (isnan(airT)) snprintf(b, sizeof b, "AIR --");
  else if (isnan(airRH)) snprintf(b, sizeof b, "AIR %.0fC", (double)airT);
  else snprintf(b, sizeof b, "AIR %.0fC %.0f%%", (double)airT, (double)airRH);
  oledText(0, 39, b, 1);                                // box climate = the condensation risk,
  if (isnan(waterT)) oledTextRight(OLED_W - 1, 39, "H2O --", 1);   // and air T drives sound speed
  else { snprintf(b, sizeof b, "H2O %.0fC", (double)waterT); oledTextRight(OLED_W - 1, 39, b, 1); }

#if SENSOR_TYPE == 1
  if (isnan(rd.distCm)) snprintf(b, sizeof b, "ECHO --");
  else snprintf(b, sizeof b, "ECHO %.1fcm", (double)rd.distCm);
#else
  if (isnan(pTubePa) || isnan(pRefPa)) snprintf(b, sizeof b, "DP --");
  else snprintf(b, sizeof b, "DP %+.1fhPa", (double)((pTubePa - pRefPa) / 100.0f));
#endif
  oledText(0, 48, b, 1);
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
    Serial.printf("oled: SSD1306 at 0x%02X on the shared I2C bus\n", oledAddr);
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
  /* OLED_IDLE_S 0 = never blank (mains-powered box, the video's arrangement). Any
   * number above 0 is the honest default for a battery/solar build and for a panel
   * that would otherwise burn its own digits into the glass. */
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

/* ======================= 8. CONFIG IN NVS ================================ */
void cfgDefaults() {
  strncpy(cfg.ssid, DEF_SSID, 32); cfg.ssid[32] = 0;
  cfg.pass[0] = 0;
  cfg.dEmptyCm = DEF_D_EMPTY; cfg.dFullCm = DEF_D_FULL;
  cfg.areaCm2 = DEF_AREA_CM2;  cfg.baroOffsetCm = 0.0f;
  cfg.maxLevelCm = DEF_MAX_LEVEL_CM; cfg.sampleSec = DEF_SAMPLE_S;
  cfg.lowCm = DEF_LOW_CM; cfg.highCm = DEF_HIGH_CM; cfg.mute = 0; cfg.oledFlip = 0;
}
void cfgLoad() {
  cfgDefaults();
  prefs.begin("wlg", true);
  strncpy(cfg.ssid, prefs.getString("ssid", cfg.ssid).c_str(), 32);
  strncpy(cfg.pass, prefs.getString("pass", cfg.pass).c_str(), 64);
  cfg.dEmptyCm     = prefs.getFloat("dEmpty", cfg.dEmptyCm);
  cfg.dFullCm      = prefs.getFloat("dFull",  cfg.dFullCm);
  cfg.areaCm2      = prefs.getFloat("area",   cfg.areaCm2);
  cfg.baroOffsetCm = prefs.getFloat("bOff",   cfg.baroOffsetCm);
  cfg.maxLevelCm   = prefs.getFloat("maxCm",  cfg.maxLevelCm);
  cfg.sampleSec    = (uint16_t)prefs.getUShort("sample", cfg.sampleSec);
  cfg.lowCm        = prefs.getFloat("lowCm",  cfg.lowCm);
  cfg.highCm       = prefs.getFloat("highCm", cfg.highCm);
  cfg.mute         = prefs.getUChar("mute",     cfg.mute);
  cfg.oledFlip     = prefs.getUChar("flip",     cfg.oledFlip);
  prefs.end();
}
void cfgSave() {
  prefs.begin("wlg", false);
  prefs.putString("ssid", cfg.ssid);  prefs.putString("pass", cfg.pass);
  prefs.putFloat("dEmpty", cfg.dEmptyCm); prefs.putFloat("dFull", cfg.dFullCm);
  prefs.putFloat("area", cfg.areaCm2);    prefs.putUShort("sample", cfg.sampleSec);
  prefs.putFloat("bOff", cfg.baroOffsetCm); prefs.putFloat("maxCm", cfg.maxLevelCm);
  prefs.putFloat("lowCm", cfg.lowCm);  prefs.putFloat("highCm", cfg.highCm);
  prefs.putUChar("mute", cfg.mute);    prefs.putUChar("flip", cfg.oledFlip);
  prefs.end();
}

/* ======================= 9. BLYNK TRANSPORT (optional) ================== */
/* The sketch already owns WiFi — STA with an AP fallback and its own retry — so this
 * uses config()/connect()/run() and never Blynk.begin(auth, ssid, pass), which would
 * block in a while-loop forever and take the radio away from the local web server.
 *
 * Virtual pins (Console -> Template -> Datastream). Blynk's own V0..V31 macros come
 * from the library, and BLYNK_WRITE(Vn) dispatches by the pin NUMBER, so the handler
 * below for V11 really is the one that fires for datastream 11.
 *   V0 level_cm V1 level_pct V2 litres V3 dist_cm V4 vbat V8 status   <- every sample
 *   V5 air_t  V6 air_rh  V7 water_t  V9 rssi  V14 p_ref  V15 p_tube
 *   V16 p_diff                                                <- hourly (BLYNK_SLOW_S)
 *   V10 sample_s  V11 low_cm  V12 high_cm  V13 mute          <- written BY THE APP
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
  if (v < 0 || v > cfg.maxLevelCm) { Serial.println(F("blynk V11: out of range, ignored")); return; }
  cfg.lowCm = v; cfgSave();
  Serial.printf("blynk V11: low alarm = %.1f cm\n", cfg.lowCm);
}
BLYNK_WRITE(V12) {
  float v = (float)param.asDouble();
  if (v < 0 || v > cfg.maxLevelCm) { Serial.println(F("blynk V12: out of range, ignored")); return; }
  cfg.highCm = v; cfgSave();
  Serial.printf("blynk V12: high alarm = %.1f cm\n", cfg.highCm);
}
BLYNK_WRITE(V13) { cfg.mute = param.asInt() ? 1 : 0; cfgSave();
                   Serial.printf("blynk V13: buzzer %s\n", cfg.mute ? "muted" : "armed"); }

void blynkSetup() {
  if (!blynkConfigured()) {
    Serial.println(F("blynk: OFF — paste BLYNK_TEMPLATE_ID and BLYNK_AUTH_TOKEN from Console -> Info"));
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
 * the slow set once an hour. See docs/05 for the messages/month table. */
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
    Blynk.virtualWrite(V5,  bnum(airT, -99));
    Blynk.virtualWrite(V6,  bnum(airRH, -99));
    Blynk.virtualWrite(V7,  bnum(waterT, -99));
    Blynk.virtualWrite(V9,  (int)(WiFi.isConnected() ? WiFi.RSSI() : 0));
    Blynk.virtualWrite(V14, bnum(isnan(pRefPa)  ? NAN : pRefPa  / 100.0f, -1));
    Blynk.virtualWrite(V15, bnum(isnan(pTubePa) ? NAN : pTubePa / 100.0f, -1));
    Blynk.virtualWrite(V16, bnum((isnan(pTubePa) || isnan(pRefPa)) ? NAN : (pTubePa - pRefPa) / 100.0f, -1));
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

/* ======================= 10. ALARMS, BUZZER, BUTTON ===================== */
/* Two local outputs and one remote one. The local buzzer works with no internet at
 * all, which is the point: an alarm you can only hear through somebody else's cloud
 * is not an alarm. */
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

  bool faultNow = (SENSOR_TYPE == SENSOR_BARO_DUAL && !haveB) ||
                  !rd.valid ||
                  (pTubePa > 120000.0f);                  // water in the tube / over range
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
 * (>1.5 s): toggle the mute latch, so a 3 a.m. beep can be switched off without a phone. */
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

/* ======================= 11. WIFI + WEB ================================== */
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
<title>Tank level — phase 0</title>
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
<h2>Water tank — phase 0 test</h2>
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
                  ['Level', r.level_cm.toFixed(1)+' cm of '+r.max_cm.toFixed(0)],
                  ['Sensor distance', r.dist_cm.toFixed(1)+' cm'],
                  ['Tube − box', (r.p_tube_hpa-r.p_ref_hpa).toFixed(2)+' hPa'],
                  ['Air', r.air_t.toFixed(1)+' °C, '+r.air_rh.toFixed(0)+' %RH'],
                  ['Water', r.water_t.toFixed(1)+' °C'],
                  ['Battery', r.vbat.toFixed(2)+' V ('+r.vpct+' %)'],
                  ['WiFi', r.rssi+' dBm'],
                  ['Good / total samples', (r.n-r.bad)+' / '+r.n],
                  ['Alarm at', r.low_cm.toFixed(0)+' cm low / '+r.high_cm.toFixed(0)+' cm high'+(r.mute?' (buzzer muted)':'')],
                  ['Uptime', (r.uptime/60).toFixed(0)+' min']];
    if(r.bad/r.n > 0.05 && r.n > 8) rows.push(['⚠ rejected echoes','see serial log']);
    t.innerHTML = rows.map(x=>{
      const c = (x[0]=='Status' && r.status!='OK') ? ' class=warn' : '';
      return '<tr><td'+c+'>'+x[0]+'</td><td'+c+'>'+x[1]+'</td></tr>';
    }).join('');
  }catch(e){ lv.textContent='no data'; }
}
up(); setInterval(up,2000);
</script>)rawl";

String jsonState() {
  String j; j.reserve(720);
  j  = "{"
    "\"valid\":"     + String(rd.valid ? 1 : 0) +
    ",\"level_cm\":" + String(isnan(rd.levelCm) ? -1 : rd.levelCm, 1) +
    ",\"pct\":"      + String(isnan(rd.pct) ? -1 : rd.pct, 1) +
    ",\"litres\":"   + String(isnan(rd.litres) ? -1 : rd.litres, 0) +
    ",\"dist_cm\":"  + String(isnan(rd.distCm) ? -1 : rd.distCm, 1) +
    ",\"air_t\":"    + String(isnan(airT) ? -99 : airT, 1) +
    ",\"air_rh\":"   + String(isnan(airRH) ? -99 : airRH, 1) +
    ",\"air_p\":"    + String(isnan(airPa) ? -99 : airPa / 100.0f, 1) +
    ",\"water_t\":"  + String(isnan(waterT) ? -99 : waterT, 1) +
    ",\"vbat\":"     + String(isnan(vbat) ? -99 : vbat, 2) +
    ",\"vpct\":"     + String(vbatPct(vbat), 0) +
    ",\"rssi\":"     + String(WiFi.isConnected() ? WiFi.RSSI() : 0) +
    ",\"uptime\":"   + String(millis() / 1000) +
    ",\"bad\":"      + String(flt.badCount) +
    ",\"n\":"        + String(flt.totalCount) +
    ",\"sensor\":"   + String(SENSOR_TYPE) +
    ",\"p_ref_hpa\": "+ String(isnan(pRefPa) ? -1 : pRefPa / 100.0f, 2) +
    ",\"p_tube_hpa\":"+ String(isnan(pTubePa) ? -1 : pTubePa / 100.0f, 2) +
    ",\"max_cm\":"   + String(cfg.maxLevelCm, 1) +
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

/* ======================= 12. SERIAL COMMANDS ============================ */
void help() {
  Serial.println(F(
    "commands:\n"
    "  r            one raw sensor read\n"
    "  s            JSON status\n"
    "  c            take one sample now + print CSV\n"
    "  w            WiFi scan (RSSI test)      w <ssid>  set SSID\n"
    "  p <pass>     set WiFi password\n"
    "  e <cm>       set empty distance        f <cm>  set full distance  (build 1)\n"
    "  a <cm2>      set tank area             m <cm>  full-scale depth   (build 2: m 121.9)\n"
    "  o <cm>       baro zero offset          z       zero the tube reading right now\n"
    "  i <sec>      set sample interval       b       battery\n"
    "  l <cm>       low alarm level           u <cm>  high alarm level   j  mute/unmute buzzer\n"
    "  d            redraw the OLED now       D       flip the OLED 180 degrees\n"
    "  k            Blynk link status + one test push + one test event\n"
    "             (k is not compiled at all when ENABLE_BLYNK is 0)\n"
    "  x            erase NVS and reboot"));
}
void handleSerial() {
  static String buf;
  while (Serial.available()) buf += (char)Serial.read();
  int nl = buf.indexOf('\n'); if (nl < 0) return;
  String cmd = buf.substring(0, nl); buf.remove(0, nl + 1);
  cmd.trim(); if (cmd.length() == 0) return;
  char c = cmd[0];
  if      (c == 'r') { float raw; bool v = readRaw(raw);
                       Serial.printf("raw = %.1f cm  in-range = %d  air_t = %.1f °C\n",
                                     isnan(raw) ? -1.0f : raw, v, isnan(airT) ? 25.0f : airT);
                       Serial.printf("     box %.2f hPa   tube %.2f hPa   Δ %.2f hPa  (1 hPa ≈ 1.03 cm water at 25 °C)\n",
                                     isnan(pRefPa) ? -1.0f : pRefPa / 100.0f,
                                     isnan(pTubePa) ? -1.0f : pTubePa / 100.0f,
                                     (isnan(pTubePa) || isnan(pRefPa)) ? 0.0f : (pTubePa - pRefPa) / 100.0f); }
  else if (c == 's') Serial.println(jsonState());
  else if (c == 'c') takeSample(true);
  else if (c == 'w') { if (cmd.length() > 2) { cmd.substring(2).toCharArray(cfg.ssid, 33);
                                              cfgSave();
                                              Serial.printf("ssid = '%s'  (now: p <password>)\n", cfg.ssid); }
                       else wifiScanPrint(); }
  else if (c == 'p') { cmd.substring(2).toCharArray(cfg.pass, 65); cfgSave();
                       Serial.println(F("password saved")); }
  else if (c == 'e') { cfg.dEmptyCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("d_empty = %.1f cm\n", cfg.dEmptyCm); }
  else if (c == 'f') { cfg.dFullCm  = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("d_full  = %.1f cm\n", cfg.dFullCm); }
  else if (c == 'a') { cfg.areaCm2  = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("area = %.0f cm²  → %.1f L per cm\n",
                                     cfg.areaCm2, cfg.areaCm2 / 1000.0f); }
  else if (c == 'o') { cfg.baroOffsetCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("baro offset = %.2f cm\n", cfg.baroOffsetCm); }
  else if (c == 'm') { cfg.maxLevelCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("full-scale depth = %.1f cm\n", cfg.maxLevelCm); }
  else if (c == 'z') { float zr; bool zv = baroRaw(zr);
                       if (!zv) Serial.println(F("z: no valid tube reading — check 0x76/0x77 straps and the tube"));
                       else { cfg.baroOffsetCm = -zr; cfgSave();
                              Serial.printf("zeroed, offset = %.2f cm — do this with the tube open to air\n", cfg.baroOffsetCm); } }
  else if (c == 'i') { cfg.sampleSec = cmd.substring(1).toInt(); cfgSave();
                       Serial.printf("sample interval = %u s\n", cfg.sampleSec); }
  else if (c == 'b') Serial.printf("vbat = %.3f V  ≈ %.0f %%\n", vbat, vbatPct(vbat));
  else if (c == 'l') { cfg.lowCm  = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("low alarm  = %.1f cm  (%.0f %% of %.1f cm)",
                                     cfg.lowCm, cfg.lowCm / cfg.maxLevelCm * 100.0f, cfg.maxLevelCm);
                       evaluateAlarms(); }
  else if (c == 'u') { cfg.highCm = cmd.substring(1).toFloat(); cfgSave();
                       Serial.printf("high alarm = %.1f cm  (%.0f %% of %.1f cm)",
                                     cfg.highCm, cfg.highCm / cfg.maxLevelCm * 100.0f, cfg.maxLevelCm);
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

/* ======================= 13. SETUP / LOOP ============================== */
void setup() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_FLOW, INPUT_PULLUP);

  Serial.begin(115200);
  delay(400);
  Serial.println(F("\n=== TANK WATER LEVEL GAUGE — phase 0 ==="));
  Serial.printf("sensor build: %s | blind-zone gate: %.0f cm | filter: median of %d\n",
                (SENSOR_TYPE == 1) ? "ULTRASONIC" : "BARO-DUAL", BLIND_ZONE_CM, FILTER_N);
  Serial.printf("outputs: oled=%d | blynk=%s | web=http://<ip>/\n", ENABLE_OLED,
                BLYNK_ON ? (BLYNK_AUTH_TOKEN[0] ? "configured" : "compiled in, token empty -> off")
                         : "ENABLE_BLYNK=0");

  cfgLoad();
  Serial.printf("nvs: ssid='%s' max_level=%.1f cm  d_empty=%.1f d_full=%.1f  area=%.0f cm² (%.1f L/cm)  every %u s\n",
                cfg.ssid, cfg.maxLevelCm, cfg.dEmptyCm, cfg.dFullCm, cfg.areaCm2,
                cfg.areaCm2 / 1000.0f, cfg.sampleSec);
  Serial.printf("nvs: alarms low<%.1f cm high>%.1f cm  hysteresis %.1f cm  buzzer %s\n",
                cfg.lowCm, cfg.highCm, ALARM_HYST_CM, cfg.mute ? "muted" : "armed");

  Wire.begin(PIN_SDA, PIN_SCL);
  haveA = bmeInit(bmeA);
  if (!haveA) {                       // try the other I2C address (SD0 strap)
    uint8_t other = (bmeA == 0x76) ? 0x77 : 0x76;
    if (bmeInit(other)) { bmeA = other; haveA = true; }
  }
  bmeB = (bmeA == 0x76) ? 0x77 : 0x76;
  haveB = bmeInit(bmeB);
  Serial.printf("I2C pressure: box=0x%02X found=%d %s   tube=0x%02X found=%d %s\n", bmeA, haveA,
                haveA ? (isBme[SLOT(bmeA)] ? "(BME280)" : "(BMP280)") : "",
                bmeB, haveB, haveB ? (isBme[SLOT(bmeB)] ? "(BME280)" : "(BMP280)") : "");
  if (SENSOR_TYPE == SENSOR_BARO_DUAL && !haveB)
    Serial.println(F("!! build 2 needs TWO I2C pressure sensors (0x76 and 0x77). only one found."));

  oledSetup();

  wifiConnect();
  httpRoutes();
  blynkSetup();
  Serial.println(F("csv header: ts_s,raw_cm,valid,filt_cm,level_cm,pct,litres,air_t,water_t,vbat,rssi,p_ref_hPa,p_tube_hPa,bad,n"));
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

  if (now - tHb > 1500) { tHb = now; digitalWrite(PIN_LED, !digitalRead(PIN_LED)); }

  if (now - tSample >= (uint32_t)cfg.sampleSec * 1000UL) {
    tSample = now;
    waterT = readWaterTemp();            // build 2: baroRaw() refreshes box T/RH/P itself
    if (SENSOR_TYPE == SENSOR_ULTRASONIC && haveA) { float T, RH, Pa; bmeReadAll(bmeA, T, RH, Pa);
                 airT = T; airRH = RH; airPa = Pa; }
    takeSample(true);                    // -> evaluateAlarms() + outputsUpdate()
    if (flt.totalCount > 8 && flt.badCount * 4 > flt.totalCount)
      Serial.println(F("!! >25% of samples rejected — build 2: water in the tube, blocked tank vent, or the wrong I2C addresses"));
  }

  if (now - tVbat > 60000) { tVbat = now; vbat = readVbat(); }
  if (now - tScan > 600000) { tScan = now; if (WiFi.isConnected()) wifiScanPrint(); }
  if (!WiFi.isConnected() && WiFi.getMode() == WIFI_STA && now > 30000) {
    static uint32_t tRetry = 0;
    if (now - tRetry > 20000) { tRetry = now; Serial.println(F("wifi: retrying")); WiFi.disconnect(); WiFi.begin(cfg.ssid, cfg.pass); }
  }
  delay(10);
}
