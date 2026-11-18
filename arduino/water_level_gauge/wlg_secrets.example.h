/* ============================================================================
 *  wlg_secrets.example.h — CREDENTIALS TEMPLATE
 *
 *  Copy this file to   wlg_secrets.h   (same folder as the .ino) and fill it in:
 *
 *      cd arduino/water_level_gauge
 *      cp wlg_secrets.example.h wlg_secrets.h
 *
 *  water_level_gauge.ino pulls it in with #if __has_include("wlg_secrets.h"), so
 *  the moment the file exists your credentials are used; when it does not exist
 *  the build is LAN-only (same gauge, no cloud). Nothing else to remember.
 *
 *  wlg_secrets.h is in .gitignore on purpose. The Blynk auth token is a
 *  password: whoever holds it can read AND write this device, and a token that
 *  reaches a public repo's history cannot be unpublished — rotate it in Blynk
 *  Console (Device -> Info -> revoke) if it ever leaks.
 *
 *  WiFi credentials are NOT here: they are entered over serial (`w` / `p`) and
 *  live in NVS on the board, never in the repo.
 * ==========================================================================*/
#ifndef WLG_SECRETS_H
#define WLG_SECRETS_H

/* Blynk Console -> your device -> Info -> Firmware Parameters for Arduino IDE */
#define BLYNK_TEMPLATE_ID   "TMPLAB12CD34"      // your template ID
#define BLYNK_TEMPLATE_NAME "Water Tank Gauge"  // must match the template name exactly
#define BLYNK_AUTH_TOKEN    "01234567890123456789012345678901"  // 32 characters

/* Self-hosted Blynk Server? Set the host here; "" means blynk.cloud. */
#define BLYNK_SERVER        ""
#define BLYNK_SERVER_PORT   80

/* The four event codes below must match Console -> Template -> Events, or the
   pushes never arrive. They are set in the sketch (BLYNK_EV_LOW / _HIGH /
   _FAULT / _RECOVER) and are NOT secrets, so they stay out of this file. */

#endif  // WLG_SECRETS_H
