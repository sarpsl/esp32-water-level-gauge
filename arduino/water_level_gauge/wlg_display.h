/* ============================================================================
 *  wlg_display.h — SSD1306 128×64 I2C OLED driver, no external library.
 *
 *  Drives the 0.96" / 1.3" modules everyone buys (SSD1306 + 4 pin header:
 *  VCC, GND, SDA, SCL) on GPIO21/22 — the same pair the reference project uses.
 *  I2C is not really needed by anything else in this build (the SR04M talks TRIG
 *  and ECHO, not a bus), so the bus exists for the panel alone: 0x3C, or 0x3D on
 *  some panels, auto-probed at boot. GPIO21/22 have no internal pull-ups on the
 *  DevKitC — cheap modules ship the resistors on the board, so this works, but a
 *  bare SSD1306 break-out with none may need 4.7 k to 3V3 on each line.
 *
 *  Why hand-rolled: the sketch's promise is that it compiles with only the
 *  ESP32 core installed. The Adafruit stack (GFX + BusIO + SSD1306) is three
 *  libraries and ~20 kB for one 128×64 panel; this is ~1 kB of flash, a 1 kB
 *  framebuffer in RAM and no allocation.
 *
 *  Frame buffer is page-major, exactly how the panel wants it:
 *      oledBuf[page*128 + x], bit0 = the TOP pixel of that column.
 *  So oledFlush() can stream the buffer straight out with zero conversion.
 * ==========================================================================*/
#ifndef WLG_DISPLAY_H
#define WLG_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include "wlg_font.h"

#define OLED_W        128
#define OLED_PAGES    8            // 8 pages × 8 px  = 64 px tall
#define OLED_BYTES    (OLED_PAGES * OLED_W)

/* Panel knobs. If the screen comes up dim or washed out, these two lines are
   what you change — not the geometry. VCOMH 0x40 suits 3.3 V-driven modules,
   0x20 suits the 5 V ones; contrast 0x8F is a sane starting point. A panel that
   is legible at 25 °C in the shade will look washed out at 55 °C in a roof box:
   that is a property of the panel, not a bug here (see docs/05). */
#define OLED_CONTRAST 0x8F
#define OLED_VCOMH    0x40

static uint8_t  oledAddr = 0;          // 0 = no panel found; every call below then no-ops
static bool     oledOn   = false;      // panel's own display bit (0xAE / 0xAF)
static bool     oledFlip = false;      // mounted the other way up
static uint8_t  oledBuf[OLED_BYTES];
static uint8_t  oledColor = 1;         // 1 set, 0 clear, 2 invert

static const uint8_t OLED_CANDIDATES[] = { 0x3C, 0x3D };

static void oledFlush();   // called by oledInit()/oledClear(), defined below
static void oledClear();   // called by oledInit()


/* ---- low level: command vs data ---------------------------------------- */
/* Bit 7 of the first control byte is Co, bits 6..0 are D/C. Co=0 means "all the
   following bytes are commands". Then SSD1306 wants 0x00 for commands and 0x40
   for RAM data. Getting these two bytes wrong is the #1 reason a hand-rolled
   driver "scans but shows nothing". */
static void oledCmd(uint8_t c) {
  if (!oledAddr) return;
  Wire.beginTransmission(oledAddr);
  Wire.write(0x00); Wire.write(c);
  Wire.endTransmission();
}
static void oledCmd2(uint8_t c, uint8_t v) {
  if (!oledAddr) return;
  Wire.beginTransmission(oledAddr);
  Wire.write(0x00); Wire.write(c); Wire.write(v);
  Wire.endTransmission();
}
static void oledData(const uint8_t *p, uint16_t n) {
  if (!oledAddr) return;
  while (n) {
    uint16_t chunk = (n > 32) ? 32 : n;      // keep each I²C transfer short: the
    Wire.beginTransmission(oledAddr);        // ESP32 Wire buffer is only so big
    Wire.write(0x40);
    for (uint16_t i = 0; i < chunk; i++) Wire.write(p[i]);
    Wire.endTransmission();
    p += chunk; n -= chunk;
  }
}

static bool i2cProbe(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission() == 0;
}

/* Probe 0x3C then 0x3D, then bring the panel up. Addressing mode is set to
   horizontal so the column counter walks 0→127 and rolls over to the next page
   by itself — but we still set page + column 0 before every page, because a
   module that missed a byte mid-transfer then cannot smear the whole screen. */
static bool oledInit() {
  if (oledAddr) return true;
  for (uint8_t i = 0; i < sizeof OLED_CANDIDATES; i++)
    if (i2cProbe(OLED_CANDIDATES[i])) { oledAddr = OLED_CANDIDATES[i]; break; }
  if (!oledAddr) return false;

  oledCmd(0xAE);                    // display OFF while we configure
  oledCmd2(0xD5, 0x80);             // clock: external / default divisor
  oledCmd2(0xA8, 0x3F);             // multiplex ratio = 64 (128×64 panel)
  oledCmd2(0xD3, 0x00);             // no display offset
  oledCmd(0x40);                    // start line 0
  oledCmd2(0x8D, 0x14);             // internal charge pump ON — without this a
                                    // 3V3-powered module stays black
  oledCmd2(0x20, 0x00);             // horizontal addressing mode
  oledCmd(0xA1);                    // segment remap: column 127 → SEG0
  oledCmd(0xC8);                    // COM scan direction: remapped
  oledCmd2(0xDA, 0x12);             // COM pins: alternative (the 128×64 wiring)
  oledCmd(0xA6);                    // normal (not inverted)
  oledCmd(0xA4);                    // output follows RAM, not a test pattern
  oledCmd2(0x81, OLED_CONTRAST);
  oledCmd2(0xD9, 0xF1);             // pre-charge period
  oledCmd2(0xDB, OLED_VCOMH);
  oledCmd(0xAF);                    // display ON
  oledOn = true;
  oledClear();
  return true;
}

static uint8_t revBits(uint8_t v) {
  v = (uint8_t)((v & 0xF0) >> 4 | (v & 0x0F) << 4);
  v = (uint8_t)((v & 0xCC) >> 2 | (v & 0x33) << 2);
  return (uint8_t)((v & 0xAA) >> 1 | (v & 0x55) << 1);
}

static void oledFlush() {
  if (!oledAddr || !oledOn) return;
  uint8_t row[OLED_W];
  for (uint8_t page = 0; page < OLED_PAGES; page++) {
    const uint8_t *src = &oledBuf[page * OLED_W];
    if (oledFlip) {                                  // upside-down mounting
      for (uint8_t i = 0; i < OLED_W; i++) row[i] = revBits(src[OLED_W - 1 - i]);
      src = row;
    }
    oledCmd((uint8_t)(0xB0 | page));                 // page address
    oledCmd(0x00); oledCmd(0x10);                    // column 0 (low, then high)
    oledData(src, OLED_W);
  }
}

/* ---- frame buffer primitives ------------------------------------------- */
static void oledClear() { memset(oledBuf, 0, sizeof oledBuf); oledFlush(); }
static void oledPix(int x, int y) {
  if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_PAGES * 8) return;
  uint16_t i = (uint16_t)(y >> 3) * OLED_W + x;
  uint8_t  m = (uint8_t)(1 << (y & 7));
  if      (oledColor == 2) oledBuf[i] ^=  m;
  else if (oledColor == 1) oledBuf[i] |=  m;
  else                     oledBuf[i] &= ~m;
}
static void oledRect(int x, int y, int w, int h, bool fill) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (fill || i == 0 || j == 0 || i == w - 1 || j == h - 1) oledPix(x + i, y + j);
}
/* 5×7 glyph, scaled by 1 or 2. scale 2 = 10×14 px, used for the big cm number. */
static void oledChar(int x, int y, char ch, uint8_t scale = 1) {
  if (ch < 32 || ch > 126) ch = '?';
  const uint8_t *g = &wlgFont5x7[(ch - 32) * 5];
  for (uint8_t c = 0; c < 5; c++) {
    uint8_t bits = pgm_read_byte(g + c);
    for (uint8_t r = 0; r < 7; r++)
      if (bits & (1 << r))
        for (uint8_t a = 0; a < scale; a++)
          for (uint8_t b = 0; b < scale; b++)
            oledPix(x + c * scale + a, y + r * scale + b);
  }
}
static int oledText(int x, int y, const char *s, uint8_t scale = 1) {
  int adv = 6 * scale;
  while (*s) { oledChar(x, y, *s++, scale); x += adv; }
  return x;
}
static int oledTextWidth(const char *s, uint8_t scale = 1) {
  return (int)strlen(s) * 6 * scale - scale;
}
static void oledTextCenter(int y, const char *s, uint8_t scale = 1) {
  int x = (OLED_W - oledTextWidth(s, scale)) / 2;
  if (x < 0) x = 0;
  oledText(x, y, s, scale);
}
/* Right-aligned text — used so the battery/RSSI fields do not jitter. */
static void oledTextRight(int xRight, int y, const char *s, uint8_t scale = 1) {
  int x = xRight - oledTextWidth(s, scale);
  if (x < 0) x = 0;
  oledText(x, y, s, scale);
}
static void oledBar(int x, int y, int w, int h, float pct) {
  oledRect(x, y, w, h, false);
  int inner = w - 2;
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  int fill = (int)(inner * pct / 100.0f + 0.5f);
  uint8_t keep = oledColor; oledColor = 1;
  for (int j = 1; j < h - 1; j++)
    for (int i = 0; i < fill; i++) oledPix(x + 1 + i, y + j);
  oledColor = keep;
}
/* Invert a whole band, for the alarm banner: the panel is only 128×64, so a
   reversed line is the only way to shout. */
static void oledInvertBand(int y, int h) {
  uint8_t keep = oledColor; oledColor = 2;
  for (int j = 0; j < h; j++) for (int i = 0; i < OLED_W; i++) oledPix(i, y + j);
  oledColor = keep;
}

/* ---- panel power ------------------------------------------------------- */
/* Blanking the panel (not the driver) is worth 3–8 mA and it is the single
   easiest defence against burn-in on a panel that spends all day showing the
   same four digits. */
static void oledSleep(bool on) {
  if (!oledAddr) return;
  if (on == oledOn) return;
  oledOn = on;
  oledCmd(on ? 0xAF : 0xAE);
  if (on) oledFlush();
}
static void oledSetFlip(bool f) { oledFlip = f; oledFlush(); }
static void oledSetContrast(uint8_t c) { oledCmd(0x81); oledCmd(c); }

#endif // WLG_DISPLAY_H
