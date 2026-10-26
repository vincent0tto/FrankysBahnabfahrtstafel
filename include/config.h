#pragma once
// ---------------------------------------------------------------------------
// Frankys Bahnabfahrtstafel – configuration
// ---------------------------------------------------------------------------

// ---- Stop -----------------------------------------------------------------
// Find IDs with:
//   https://www.vrn.de/mngvrn/XML_STOPFINDER_REQUEST?outputFormat=rapidJSON&type_sf=any&name_sf=<Name>
#define STOP_ID           "de:08226:3479"          // Schriesheim, Bahnhof
#define STOP_DISPLAY_NAME "Schriesheim Bahnhof"   // header text (UTF-8)

// Optional filters (comma separated, empty = show everything).
// Line = transportation.disassembledName, e.g. "5,628".
// Platform = location.properties.platform, e.g. "TrRiS,TrRiN".
#define FILTER_LINES      ""
#define FILTER_PLATFORMS  ""

// Destination shortening: "from|to" pairs separated by ';'. Earlier pairs win
// and replaced text isn't touched again, so "X|X" keeps X unabbreviated.
#define DEST_ABBREVIATIONS "Hauptbahnhof|Hbf;Alter OEG-Bahnhof|Alter OEG-Bahnhof;Bahnhof|Bf;Friedhof|Friedh."

// Some rnv trips name the town of this stop as their destination although
// they go on (line 5 Käfertal -> Weinheim shows "Schriesheim"). Those get the
// direction of their platform instead: "platform|destination" pairs.
#define PLATFORM_DIRECTIONS "TrRiN|Weinheim;TrRiS|Heidelberg"

// Every PLATFORM_EVERY_S seconds the time column shows the platform for
// PLATFORM_SHOW_S seconds, with PLATFORM_HEADER in place of the clock.
// 0 = never. The VRN only sends short codes ("A", "B", "TrRiN"), rename them
// with "platform|label" pairs; codes without a pair are shown as they are.
#define PLATFORM_EVERY_S   40
#define PLATFORM_SHOW_S    10
#define PLATFORM_HEADER    "Gleis"
#define PLATFORM_LABELS    "TrRiS|1;End|2;TrRiN|3"

// ---- API ------------------------------------------------------------------
// VRN EFA Open Service (Mentz RapidJSON). Two hosts, the second is used as
// fallback when the first one fails. Data: Verkehrsverbund Rhein-Neckar (VRN),
// Datenlizenz Deutschland – Namensnennung.
#define API_URL_PRIMARY   "https://www.vrn.de/mngvrn/XML_DM_REQUEST"
#define API_URL_FALLBACK  "https://trias.vrn.de/vrn/XML_DM_REQUEST"
#define API_LIMIT         20        // events requested per call

#define POLL_INTERVAL_S        30   // normal refresh
#define POLL_RETRY_S           30   // wait after a failed fetch (both hosts), no backoff
#define HTTP_TIMEOUT_MS     10000

// ---- Robustness -----------------------------------------------------------
#define STALE_WARN_S          (5 * 60)    // show "!" in header
#define STALE_DROP_S         (60 * 60)    // show "keine Abfahrtsinformationen"
#define WIFI_RECONNECT_S          30      // kick the WiFi stack after this
#define WIFI_REBOOT_S        (15 * 60)    // reboot if offline this long
#define WDT_TIMEOUT_S             90   // > 3x HTTP_TIMEOUT_MS
#define MIN_FREE_HEAP_BYTES    20000      // reboot below this (internal RAM)
#define DAILY_REBOOT_HOUR          4      // -1 = disabled; also checks for a firmware update
#define LOG_BUFFER_BYTES   (32 * 1024)    // newest serial log for the settings page (PSRAM)

// ---- Firmware update from GitHub ------------------------------------------
// After each start the board asks GitHub for the newest release (built by
// .github/workflows/firmware.yml on every push to main) and installs it if
// it is newer than the running firmware. Repo and releases are public, the
// firmware there contains no passwords (src/core/credentials.cpp).
#define AUTO_UPDATE   1   // 0 = off
#define GITHUB_REPO   "vincent0tto/FrankysBahnabfahrtstafel"
#define UPDATE_ASSET  "firmware.bin"

// ---- Time -----------------------------------------------------------------
#define TZ_INFO     "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER1 "ptbtime1.ptb.de"
#define NTP_SERVER2 "pool.ntp.org"

// ---- Display --------------------------------------------------------------
#define PANEL_RES_X   64
#define PANEL_RES_Y   64
#define PANEL_CHAIN    3            // side by side -> 192 x 64

// HUB75 timing / driver. Try these (one at a time) if single pixels flicker,
// show wrong colours or the image is shifted:
//  - PANEL_CLKPHASE: clock edge; true = library default. A garbage pixel at
//    the very top-left or a 1px shift usually means this is wrong -> toggle.
//  - PANEL_LAT_BLANKING: 1..4, higher removes ghosting at row switches.
//  - PANEL_I2S_SPEED: ignored on the ESP32-S3 because S3_LCD_DIV_NUM in
//    platformio.ini sets the clock (pixels that flicker in a wrong colour and
//    move with the text -> raise it).
//  - PANEL_DRIVE_CAP: lower it if wrong-colour pixels flicker next to
//    bright (white) content; raise it if colours smear or break up.
//  - PANEL_DRIVER: SHIFTREG for plain panels; FM6126A / FM6124 / ICN2038S /
//    MBI5124 / DP3246 if that chip name is printed on the panel back.
#define PANEL_CLKPHASE      true
#define PANEL_LAT_BLANKING  3
#define PANEL_I2S_SPEED     HUB75_I2S_CFG::HZ_8M
#define PANEL_DRIVER        HUB75_I2S_CFG::SHIFTREG
// GPIO drive strength of the six colour pins (R1..B2), 0..3 (~5/10/20/40 mA).
// The library sets 3; the hard edges of all six switching together (white
// text) coupled over the jumper wires into red/blue pixels. Softer colour
// edges fix that without slowing the clock (a slower clock broke WiFi).
// Address, LAT, OE and CLK keep 3: softer there gave ghost rows.
#define PANEL_DRIVE_CAP     1
// The library trades colour depth for refresh to reach this. It calculates
// with 8 MHz although S3_LCD_DIV_NUM=24 really clocks 6.7 MHz, so the real
// rate is ~17 % lower than requested.
#define PANEL_MIN_REFRESH_HZ 120

// rnv DFI amber
#define COLOR_R 255
#define COLOR_G 120
#define COLOR_B   0

// Brightness, dimming and off time below are only the defaults:
// they can be changed on the web page (http://abfahrtstafel.local), which
// stores them on the board.
// Brightness in percent (1..100) of BRIGHTNESS_MAX, which caps the panel at
// 70 % of its hardware brightness (0..255): 100 % = 178, 85 % = 151.
#define BRIGHTNESS_MAX    178
#define BRIGHTNESS_DAY     85       // %
#define BRIGHTNESS_NIGHT   20       // %
#define NIGHT_START_HOUR   23       // -1 = no night dimming
#define NIGHT_END_HOUR      5

// Panel completely dark and no API calls between these hours. -1 = never.
#define OFF_START_HOUR      2
#define OFF_END_HOUR        6
#define WAKE_FETCH_LEAD_S 120   // fetch this long before the panel comes on

#define TICKER_SPEED_PX_S  30       // scrolling text speed

#define EVENT_TEST_S       30       // "Auf der Tafel testen" on the event page shows a scene this long

// When a departure leaves, 1 in N times its tram/bus drives out of the row
// (src/display/depart_anim.cpp). Only if no other shown row leaves in the same minute.
// 0 = never, 1 = always. Defaults, changeable on the web page (Tafel).
#define DEPART_ANIM_CHANCE 10
#define DEPART_ANIM_CHANCE_BUS 3   // same for buses (line 628 etc.)

// Promo texts shown now and then instead of the 4th row are set up on the
// web page; there are none built in. Icons: src/display/icons.cpp.
#define PROMO_INTERVAL_S   75   // average pause between showings (+-50 %), 0 = off
#define PROMO_SHOW_S         25   // how long one text stays
#define PROMO_MIN_MINUTES   7   // only if the 4th row's departure is this far away
// VRN disruption texts take turns with the promos in the 4th row
#define DISRUPTION_MIN_MINUTES  5   // only if the 4th row's departure is this far away
#define DISRUPTION_PASSES       2   // scroll through all messages this often (default; web page)

// ---- St.-Pauli-Ticker (switched on on the web page) ------------------------
// Matches from ESPN's site API (free, no key, not documented officially; the
// certificate chain ends in ISRG Root X1 like the VRN's). The team's id is in
// the URL of its ESPN page, espn.com/soccer/club/_/id/270/st-pauli.
#define MATCH_API_URL     "https://site.api.espn.com/apis/site/v2/sports/soccer"
#define MATCH_TEAM_ID     "270"             // FC St. Pauli
#define MATCH_LEAGUES     "ger.1,ger.2,ger.dfb_pokal"  // ESPN league slugs; not the friendlies ("club.friendly")
#define MATCH_POLL_IDLE_S  3600   // the team's results and fixtures
#define MATCH_POLL_LIVE_S    15   // the score while it runs (ESPN has no limit, ~25 kB a time)
#define MATCH_RETRY_S       300   // the list after a failed fetch
#define MATCH_LIVE_LEAD_S   300   // start asking this long before kick-off
#define MATCH_LIVE_MAX_S  (3 * 3600)  // live at most this long after kick-off (cup: extra time, penalties)
#define MATCH_FINAL_H         3   // the final score is a promo this long (counted from kick-off + 2 h)
#define MATCH_GOAL_EVERY_S   30   // while live the last goal scrolls through the row this often
#define MATCH_CLOCK_EVERY_S  10   // while live the minute replaces the score this often ...
#define MATCH_CLOCK_SHOW_S    3   // ... for this long

// HUB75 pin map (ESP32-S3-DevKitC-1 N8R8; avoids PSRAM 35-37, straps 0/3/45/46,
// USB 19/20, UART 43/44)
#define PIN_R1   4
#define PIN_G1   5
#define PIN_B1   6
#define PIN_R2   7
#define PIN_G2  15
#define PIN_B2  16
#define PIN_A   17
#define PIN_B   18
#define PIN_C    8
#define PIN_D    9
#define PIN_E   10
#define PIN_LAT 11
#define PIN_OE  12
#define PIN_CLK 13

#define HOSTNAME "abfahrtstafel"
