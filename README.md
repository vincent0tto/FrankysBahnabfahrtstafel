# Franky's Bahnabfahrtstafel

A small departure board for **[Franky's](https://frankys-schriesheim.de)**, the café right
next to the station in Schriesheim. It shows when the next trams and buses leave
**Schriesheim Bahnhof**, so you can finish your coffee and still catch your train.

- 🌐 Website: <https://frankys-schriesheim.de>
- 📸 Instagram: [@frankys_by_coffeetoego](https://www.instagram.com/frankys_by_coffeetoego/)

## What it does

The board looks like the amber LED displays at the rnv stops and shows the next departures
with live times, just like at the platform:

```
Schriesheim Bahnhof                     10:42
──────────────────────────────────────────────
 5  Heidelberg                         sofort
 5  Weinheim                            3 min
628 Wilhelmsfeld Autohalle              8 min
 5  Weinheim Alter OEG-Bahnhof          12 min
```

- **Live times:** delays are already included, cancelled trips show `fällt aus`, and
  disruption notices scroll along the bottom line.
- **A little extra from the café:** now and then a line shows a message from Franky's,
  for example the Instagram handle or what's on offer, with a small pixel icon.
- **Tiny trams:** every so often a little tram or bus drives out of its row when it departs.
- **Night rest:** at night the board switches itself off and dims in the late evening.
- **Easy to change:** messages, icons, brightness and times can be changed from any phone in
  the café's WiFi, no cable or computer needed.
- **Looks after itself:** it reconnects after WiFi problems and installs updates on its own.

The departure data comes from the Verkehrsverbund Rhein‑Neckar (VRN).

---

# Technical details

Everything below is for whoever builds, repairs or develops the board.

## Hardware

An ESP32-S3-DEV-KIT-N8R8 drives three Waveshare 22100 panels (64×64, P3, HUB75E, 1/32 scan)
mounted side by side, which gives 192×64 px.

Long destinations scroll (pause, slide to the end, pause, jump back).
Fonts and layout are set at the top of `src/render.cpp`. To preview a layout without
hardware, run `tools/preview/run.sh`; it writes `tools/preview/preview.png` and the
departure animation frames to `tools/preview/preview_anim.png`.

## Data source

The board reads the **VRN EFA Open Service** (Mentz RapidJSON). It needs no API key or login;
this was checked on 2026‑10‑08 and real-time data is included.

```
https://www.vrn.de/mngvrn/XML_DM_REQUEST?outputFormat=rapidJSON&type_dm=any&mode=direct&useRealtime=1&name_dm=de:08226:3479&limit=20
```

`https://trias.vrn.de/vrn/XML_DM_REQUEST` is the automatic fallback host. The rnv's own
GraphQL Data Hub would need an API key, which you request by e‑mail at opendata@rnv-online.de.
This project doesn't need it.

License: *Datenlizenz Deutschland – Namensnennung*, source **Verkehrsverbund Rhein‑Neckar (VRN)**.
VRN's terms ask for a free registration at <https://opendata.vrn.de>. The endpoint works
without one, but registering is the polite thing to do.

### Another stop?

1. Look up the stop ID:
   `https://www.vrn.de/mngvrn/XML_STOPFINDER_REQUEST?outputFormat=rapidJSON&type_sf=any&name_sf=Heidelberg%20Bismarckplatz`
   and take `locations[0].id`, e.g. `de:08221:1146`.
2. Set `STOP_ID` and `STOP_DISPLAY_NAME` in `include/config.h`.
3. Optional: use `FILTER_LINES` / `FILTER_PLATFORMS` to show only certain lines or platforms.
   The platform codes for Schriesheim are `TrRiS`, `TrRiN`, `A`, `B`, …; the serial log shows them.

## Wiring

| HUB75 (panel 1 **IN**) | ESP32-S3 GPIO |
|---|---|
| R1 / G1 / B1 | 4 / 5 / 6 |
| R2 / G2 / B2 | 7 / 15 / 16 |
| A / B / C / D / **E** | 17 / 18 / 8 / 9 / 10 |
| LAT / OE / CLK | 11 / 12 / 13 |
| GND (all GND pins) | GND |

- Run the ribbon cables from panel 1 **OUT** to panel 2 **IN**, then panel 2 OUT to panel 3 IN.
  Seen from the front, panel 1 is the **left** one. If the picture comes out in the wrong order,
  swap the panels physically.
- **Power:** use a 5 V supply of at least 10 A (e.g. MeanWell LRS‑75‑5) connected to each panel's
  VH4 power connector. Connect the supply's GND to the ESP32's GND. Don't power the panels
  through the ESP32. The ESP32 itself can run from USB or from the 5 V pin of the same supply.
- The pin map avoids PSRAM (GPIO 35–37), the strapping pins, USB (19/20) and UART (43/44).
- 3.3 V logic usually works with these panels. If you see flickering or wrong pixels, put a
  74HCT245 between the ESP32 and panel 1, or use shorter cables.

## Build and flash

```bash
brew install platformio              # once
cp include/secrets.h.example include/secrets.h   # then fill in WLAN SSID/password, OTA password
pio run -e esp32s3 -t upload -t monitor
```

Use the **UART** USB-C port (CH343) for flashing and for the serial log.

The firmware itself holds no passwords. On its first start the board copies the values from
`secrets.h` into its flash (NVS, `src/credentials.cpp`) and uses them from there, also after
updates from GitHub. It only takes them again when `secrets.h` changes. Later the WiFi can be
changed on the settings page; if the board can't connect to a new WiFi, it falls back to the
previous one. If the old WiFi is gone too, flash once over USB with an updated `secrets.h`.

Later updates can go over WiFi:

```bash
export ABFAHRT_OTA_PASSWORD=<OTA_PASSWORD from secrets.h>
pio run -e esp32s3-ota -t upload
```

### Automatic updates from GitHub

Every push to `main` makes GitHub Actions (`.github/workflows/firmware.yml`) run the tests,
build the firmware and publish it as release `build-<N>`, where N is the number of commits.
After every start, including the daily reboot at 04:00 during the off time, the board asks
GitHub for the newest release. If it is newer than the running firmware, the board installs it
and restarts. A firmware built locally from the same commit counts as up to date.

The repo and its releases are public. The CI builds without `secrets.h`, so the released
firmware contains no passwords, and the board needs no token to download it.

The serial log shows `[update] build 12 is up to date` or `[update] build-13 > 12, installing`.
While an update installs, the panel shows "Software-Update ...". `AUTO_UPDATE 0` in
`config.h` turns updates off. The bootloader has no automatic rollback: a firmware that
crashes before the update check has to be flashed over USB. That's why the CI only publishes
builds that pass the tests.

### Settings page

Open `http://abfahrtstafel.local` (or the IP from the serial log) from any device in the same
WiFi. There is no login. On the page you can set:

- **Ruhezeit** (default 02:00–06:00): the panel is completely dark and the board makes no API
  calls. It fetches again 2 minutes before the panel comes back on (`WAKE_FETCH_LEAD_S`).
- **Helligkeit** in percent: a default (85 %; the **Standard** button resets the slider to
  it) plus up to 4 time windows with their own brightness. 100 % is capped at 70 % of the
  panel's hardware brightness (`BRIGHTNESS_MAX`).
- **Promo-Texte**: show time, pause, and which texts appear. The built-in texts
  (`PROMO_TEXTS`) can be switched on and off and edited; ↺ restores the firmware's version.
  You can add up to 20 texts of your own, each
  with an icon, an optional right column and an optional time window.
- **Icons**: the built-in icons (Instagram, Grilled Cheese, Flat White, Kuchen, Bier) plus up
  to 8 icons of your own, drawn in a 12×9 pixel editor.
- **WLAN**: name and password of the WiFi, used after a restart (see above).

The board stores the settings in flash (NVS), a partition that firmware updates (from GitHub,
OTA or a normal USB upload) don't touch, so they survive restarts and updates. Only erasing the
whole flash (`pio run -t erase`) resets them. The values in `config.h` are only the defaults:
a built-in text that wasn't edited follows new firmware; an edited one keeps your version,
even if the firmware drops or changes that text.

### Tests (parser, run on the Mac)

```bash
pio test -e native
```

These tests need a host compiler. If you haven't accepted the Xcode license yet, run
`sudo xcodebuild -license` once first.

## What the display shows (details)

| Display | Meaning |
|---|---|
| `sofort` | departs in under 1 minute |
| `7 min` | real-time forecast, or the timetable time if there's no forecast |
| `13:40` | 60 minutes or more away |
| `fällt aus` | trip is cancelled |
| bottom line, scrolling | VRN disruption messages for the lines shown |
| `!` next to the clock | no WiFi, or the data is more than 5 minutes old |
| `Zurzeit keine Abfahrtsinformationen` | no departures, or the data is more than 60 minutes old |
| Instagram logo + `@frankys_by_coffeetoego` in the 4th row | promo texts: now and then, only while that departure is ≥ `PROMO_MIN_MINUTES` away (texts, icons and timing on the settings page) |
| tram/bus driving out of a row | 1 in 5 tram departures and every bus departure, only if no other one leaves in the same minute: the two-car tram (or the bus) pulls away and drives out (`DEPART_ANIM_CHANCE`, `DEPART_ANIM_CHANCE_BUS`) |

## 24/7 operation

- **WiFi lost:** automatic reconnect; the WiFi stack is restarted after 30 s and the board
  reboots after 15 min. Meanwhile it keeps counting down from the last data it received.
- **API errors:** it switches to the fallback host and backs off 30 s → 60 → 120 → 300 s.
  The last good data stays on the display.
- **Watchdog** (90 s) on both tasks, a reboot if internal RAM runs low, and an optional daily
  reboot at 04:00 (`DAILY_REBOOT_HOUR`), which also checks for a firmware update.
- **Off time** from 02:00 to 06:00 and **dimming** from 23:00 to 05:00. Both are defaults that
  you can change on the settings page.
- **TLS:** the board checks the server certificate against ISRG Root X1/X2 (`include/certs.h`,
  valid until 2035/2040). If VRN ever switches to a different certificate authority, add its
  root certificate to that file.

## Code structure

| File | Purpose |
|---|---|
| `include/config.h` | stop, filters, pins, brightness, timings |
| `src/parse.cpp` | JSON → departures, umlauts (CP437), countdown text (tested on the host) |
| `src/vrn_client.cpp` | HTTPS request, streaming JSON parse with filter, PSRAM |
| `src/net.cpp` | WiFi watchdog, NTP, OTA |
| `src/settings.cpp` | run-time settings: defaults, time windows, JSON (tested on the host) |
| `src/settings_store.cpp` | settings in flash (NVS), handover to the render loop |
| `src/web.cpp`, `src/web_page.h` | settings page and its JSON API |
| `src/updater.cpp` | firmware update from the GitHub releases |
| `src/icons.cpp` | built-in promo icons |
| `src/render.cpp` | rnv layout, ticker, status screens |
| `src/depart_anim.cpp` | departure animation (sprites + timeline) |
| `src/display.cpp` | panel setup, dimming |
| `src/main.cpp` | network task (core 0) and render loop (core 1, ~30 fps) |
| `.github/workflows/firmware.yml` | CI: tests, build, release |
