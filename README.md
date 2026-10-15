# Franky's Bahnabfahrtstafel

Eine kleine Abfahrtstafel für **[Franky's](https://frankys-schriesheim.de)**, das Café direkt
am Bahnhof in Schriesheim. Sie zeigt, wann die nächsten Bahnen und Busse am
**Schriesheim Bahnhof** abfahren – so kann man in Ruhe seinen Kaffee austrinken und erwischt
trotzdem seine Bahn.

- 🌐 Website: <https://frankys-schriesheim.de>
- 📸 Instagram: [@frankys_by_coffeetoego](https://www.instagram.com/frankys_by_coffeetoego/)

## Was die Tafel kann

Die Tafel sieht aus wie die orangefarbenen LED-Anzeigen an den rnv-Haltestellen und zeigt die
nächsten Abfahrten mit Echtzeit, genau wie am Bahnsteig:

```
Schriesheim Bahnhof                     10:42
──────────────────────────────────────────────
 5  Heidelberg                         sofort
 5  Weinheim                            3 min
628 Wilhelmsfeld Autohalle              8 min
 5  Weinheim Alter OEG-Bahnhof          12 min
```

- **Echtzeit:** Verspätungen sind schon eingerechnet, ausfallende Fahrten zeigen `fällt aus`,
  und Störungsmeldungen laufen in der untersten Zeile durch.
- **Ein kleines Extra vom Café:** ab und zu zeigt eine Zeile eine Nachricht von Franky's,
  zum Beispiel den Instagram-Namen oder ein Angebot, mit einem kleinen Pixel-Symbol.
- **Kleine Bahnen:** hin und wieder fährt eine kleine Bahn oder ein Bus aus seiner Zeile,
  wenn er abfährt.
- **Nachtruhe:** nachts schaltet sich die Tafel ab, am späten Abend wird sie dunkler.
- **Leicht zu ändern:** Nachrichten, Symbole, Helligkeit und Zeiten lassen sich von jedem
  Handy, Tablet oder Laptop im Mitarbeiter-WLAN des Cafés ändern, ganz ohne Kabel.
- **Kümmert sich um sich selbst:** nach WLAN-Problemen verbindet sie sich neu, und Updates
  installiert sie von allein.

Die Abfahrtsdaten kommen vom Verkehrsverbund Rhein‑Neckar (VRN).

---

# Technische Details

Alles ab hier ist für alle, die die Tafel bauen, reparieren oder weiterentwickeln.

## Hardware

Ein ESP32-S3-DEV-KIT-N8R8 steuert drei Waveshare-22100-Panels (64×64, P3, HUB75E, 1/32 Scan),
die nebeneinander montiert sind. Das ergibt 192×64 Pixel.

Lange Ziele laufen durch (Pause, bis zum Ende schieben, Pause, zurückspringen).
Schriften und Layout stehen oben in `src/render.cpp`. Um ein Layout ohne Hardware anzusehen,
`tools/preview/run.sh` ausführen; es schreibt `tools/preview/preview.png` und die Bilder der
Abfahrtsanimation nach `tools/preview/preview_anim.png`.

## Datenquelle

Die Tafel liest den **VRN EFA Open Service** (Mentz RapidJSON). Er braucht weder API-Key noch
Login; das wurde am 08.10.2026 geprüft, und Echtzeitdaten sind enthalten.

```
https://www.vrn.de/mngvrn/XML_DM_REQUEST?outputFormat=rapidJSON&type_dm=any&mode=direct&useRealtime=1&name_dm=de:08226:3479&limit=20
```

`https://trias.vrn.de/vrn/XML_DM_REQUEST` ist der automatische Ausweich-Host. Der eigene
GraphQL-Data-Hub der rnv bräuchte einen API-Key, den man per E-Mail an opendata@rnv-online.de
beantragt. Dieses Projekt braucht ihn nicht.

Lizenz: *Datenlizenz Deutschland – Namensnennung*, Quelle **Verkehrsverbund Rhein‑Neckar (VRN)**.
Es gelten die [Nutzungsbedingungen](https://opendata.vrn.de/nutzungsbedingungen) des VRN.
Eine Registrierung ist nicht nötig. Wer möchte, kann sich unter
<https://opendata.vrn.de/user/register> kostenlos registrieren; einen API-Key gibt es dafür nicht.

### Eine andere Haltestelle?

1. Haltestellen-ID nachschlagen:
   `https://www.vrn.de/mngvrn/XML_STOPFINDER_REQUEST?outputFormat=rapidJSON&type_sf=any&name_sf=Heidelberg%20Bismarckplatz`
   und `locations[0].id` nehmen, z. B. `de:08221:1146`.
2. `STOP_ID` und `STOP_DISPLAY_NAME` in `include/config.h` setzen.
3. Optional: mit `FILTER_LINES` / `FILTER_PLATFORMS` nur bestimmte Linien oder Steige anzeigen.
   Die Steig-Codes für Schriesheim sind `TrRiS`, `TrRiN`, `A`, `B`, …; das serielle Log zeigt sie an.

## Verkabelung

| HUB75 (Panel 1 **IN**) | ESP32-S3 GPIO |
|---|---|
| R1 / G1 / B1 | 4 / 5 / 6 |
| R2 / G2 / B2 | 7 / 15 / 16 |
| A / B / C / D / **E** | 17 / 18 / 8 / 9 / 10 |
| LAT / OE / CLK | 11 / 12 / 13 |
| GND (alle GND-Pins) | GND |

- Flachbandkabel von Panel 1 **OUT** zu Panel 2 **IN**, dann von Panel 2 OUT zu Panel 3 IN.
  Von vorne gesehen ist Panel 1 das **linke**. Wenn das Bild in der falschen Reihenfolge
  erscheint, die Panels umstecken.
- **Strom:** ein 5-V-Netzteil mit mindestens 10 A (verbaut: MeanWell LRS‑100‑5, 18 A) an den
  VH4-Stromanschluss jedes Panels. GND des Netzteils mit GND des ESP32 verbinden. Die Panels
  nicht über den ESP32 versorgen. Der ESP32 selbst kann über USB oder den 5-V-Pin desselben
  Netzteils laufen.
- Die Pinbelegung meidet PSRAM (GPIO 35–37), die Strapping-Pins, USB (19/20) und UART (43/44).
- 3,3-V-Logik funktioniert mit diesen Panels meistens. Bei Flackern oder falschen Pixeln einen
  74HCT245 zwischen ESP32 und Panel 1 setzen oder kürzere Kabel nehmen.

## Bauen und flashen

```bash
brew install platformio              # einmalig
cp include/secrets.h.example include/secrets.h   # dann WLAN-SSID/-Passwort und OTA-Passwort eintragen
pio run -e esp32s3 -t upload -t monitor
```

Zum Flashen und für das serielle Log den **UART**-USB-C-Anschluss (CH343) nehmen.

Die Firmware selbst enthält keine Passwörter. Beim ersten Start kopiert die Tafel die Werte aus
`secrets.h` in ihren Flash (NVS, `src/credentials.cpp`) und nimmt sie ab dann von dort, auch
nach Updates von GitHub. Neu übernommen werden sie nur, wenn sich `secrets.h` ändert. Später
lässt sich das WLAN auf der Einstellungsseite ändern; kann sich die Tafel nicht mit einem neuen
WLAN verbinden, nimmt sie wieder das vorherige. Ist auch das alte WLAN weg, einmal über USB mit
einer aktualisierten `secrets.h` flashen.

Spätere Updates gehen auch über WLAN:

```bash
export ABFAHRT_OTA_PASSWORD=<OTA_PASSWORD aus secrets.h>
pio run -e esp32s3-ota -t upload
```

### Automatische Updates von GitHub

Bei jedem Push auf `main` lässt GitHub Actions (`.github/workflows/firmware.yml`) die Tests
laufen, baut die Firmware und veröffentlicht sie als Release `build-<N>`, wobei N die Nummer
des Actions-Laufs ist (wie im Actions-Tab, z. B. „firmware #16"). Nach jedem Start, auch nach
dem täglichen Neustart um 04:00 während der Ruhezeit, fragt die Tafel bei GitHub nach dem
neuesten Release. Ist es neuer als die laufende Firmware, installiert sie es und startet neu.
Eine lokal gebaute Firmware bekommt die Nummer des neuesten `build-*`-Tags in ihrer
Git-Historie (`git pull` holt die Tags mit); aus demselben Commit gebaut gilt sie also als
aktuell.

Das Repo und seine Releases sind öffentlich. Die CI baut ohne `secrets.h`, daher enthält die
veröffentlichte Firmware keine Passwörter, und die Tafel braucht zum Herunterladen keinen Token.

Das serielle Log zeigt `[update] build 12 is up to date` oder `[update] build-13 > 12, installing`.
Während ein Update installiert wird, zeigt das Panel „Software-Update ...". `AUTO_UPDATE 0` in
`config.h` schaltet Updates ab. Der Bootloader hat kein automatisches Rollback: Eine Firmware,
die vor der Update-Prüfung abstürzt, muss über USB geflasht werden. Deshalb veröffentlicht die
CI nur Builds, die die Tests bestehen.

### Einstellungsseite

`http://abfahrtstafel.local` (oder die IP aus dem seriellen Log) von einem beliebigen Gerät im
selben WLAN öffnen. Es gibt kein Login. Auf der Seite lässt sich einstellen:

- **Ruhezeit** (Standard 02:00–06:00): Das Panel ist komplett dunkel, und die Tafel ruft keine
  Daten ab. 2 Minuten bevor das Panel wieder angeht, holt sie neue Daten (`WAKE_FETCH_LEAD_S`).
- **Helligkeit** in Prozent: ein Standardwert (85 %; der Knopf **Standard** setzt den Regler
  darauf zurück) und bis zu 4 Zeitfenster mit eigener Helligkeit. 100 % entsprechen 70 % der
  Hardware-Helligkeit des Panels (`BRIGHTNESS_MAX`).
- **Promo-Texte**: Anzeigedauer, Pause und welche Texte erscheinen. Die eingebauten Texte
  (`PROMO_TEXTS`) lassen sich an- und ausschalten und bearbeiten; ↺ stellt die Version der
  Firmware wieder her. Dazu kommen bis zu 20 eigene Texte, jeweils mit Symbol, optionaler
  rechter Spalte und optionalem Zeitfenster.
- **Icons**: die eingebauten Symbole (Instagram, Grilled Cheese, Flat White, Kuchen, Bier) und
  bis zu 8 eigene, gezeichnet in einem 12×9-Pixel-Editor.
- **WLAN**: Name und Passwort des WLANs, gilt nach einem Neustart (siehe oben).
- **Log**: die letzten Meldungen der Tafel (dieselben wie am USB-Anschluss, mit Uhrzeit), wird
  alle 10 Sekunden ergänzt, solange es angezeigt wird und der Tab im Vordergrund ist. Die Tafel hält etwa 32 KB davon im PSRAM
  (`LOG_BUFFER_BYTES`); nach einem Neustart beginnt es von vorn.

Die Tafel speichert die Einstellungen im Flash (NVS), in einer Partition, die Firmware-Updates
(von GitHub, per OTA oder ein normaler USB-Upload) nicht anrühren. So überstehen sie Neustarts
und Updates. Nur das Löschen des ganzen Flashs (`pio run -t erase`) setzt sie zurück. Die Werte
in `config.h` sind nur die Standardwerte: Ein eingebauter Text, der nicht bearbeitet wurde,
folgt neuer Firmware; ein bearbeiteter behält deine Version, auch wenn die Firmware den Text
entfernt oder ändert.

### Tests (Parser, laufen auf dem Mac)

```bash
pio test -e native
```

Die Tests brauchen einen Host-Compiler. Wer die Xcode-Lizenz noch nicht akzeptiert hat, führt
vorher einmal `sudo xcodebuild -license` aus.

## Was die Anzeige zeigt (im Detail)

| Anzeige | Bedeutung |
|---|---|
| `sofort` | fährt in unter 1 Minute ab |
| `7 min` | Echtzeit-Prognose, oder die Fahrplanzeit, wenn es keine Prognose gibt |
| `13:40` | 60 Minuten oder mehr entfernt |
| `fällt aus` | Fahrt fällt aus |
| unterste Zeile, durchlaufend | Störungsmeldungen des VRN für die angezeigten Linien |
| `!` neben der Uhr | kein WLAN, oder die Daten sind älter als 5 Minuten |
| `Zurzeit keine Abfahrtsinformationen` | keine Abfahrten, oder die Daten sind älter als 60 Minuten |
| Instagram-Logo + `@frankys_by_coffeetoego` in der 4. Zeile | Promo-Texte: ab und zu, nur solange diese Abfahrt ≥ `PROMO_MIN_MINUTES` entfernt ist (Texte, Symbole und Zeiten auf der Einstellungsseite) |
| Bahn/Bus fährt aus einer Zeile | bei 1 von 5 Bahn-Abfahrten und jeder Bus-Abfahrt, nur wenn in derselben Minute keine andere abfährt: die zweiteilige Bahn (oder der Bus) fährt an und aus dem Bild (`DEPART_ANIM_CHANCE`, `DEPART_ANIM_CHANCE_BUS`) |

## Dauerbetrieb rund um die Uhr

- **WLAN weg:** automatisches Neuverbinden; nach 30 s wird der WLAN-Stack neu gestartet, nach
  15 min startet die Tafel neu. Solange zählt sie mit den zuletzt empfangenen Daten weiter herunter.
- **API-Fehler:** Sie wechselt zum Ausweich-Host; klappt beides nicht, versucht sie es nach 30 s erneut.
  Die letzten guten Daten bleiben auf der Anzeige.
- **Watchdog** (90 s) auf beiden Tasks, Neustart bei knappem internem RAM und optional ein
  täglicher Neustart um 04:00 (`DAILY_REBOOT_HOUR`), bei dem sie auch nach einem
  Firmware-Update sucht.
- **Ruhezeit** von 02:00 bis 06:00 und **Dimmen** von 23:00 bis 05:00. Beides sind
  Standardwerte, die sich auf der Einstellungsseite ändern lassen.
- **TLS:** Die Tafel prüft das Serverzertifikat gegen ISRG Root X1/X2 (`include/certs.h`,
  gültig bis 2035/2040). Falls der VRN einmal zu einer anderen Zertifizierungsstelle wechselt,
  deren Root-Zertifikat in diese Datei eintragen.

## Aufbau des Codes

| Datei | Zweck |
|---|---|
| `include/config.h` | Haltestelle, Filter, Pins, Helligkeit, Zeiten |
| `src/parse.cpp` | JSON → Abfahrten, Umlaute (CP437), Countdown-Text (auf dem Host getestet) |
| `src/vrn_client.cpp` | HTTPS-Anfrage, Streaming-JSON-Parser mit Filter, PSRAM |
| `src/net.cpp` | WLAN-Watchdog, NTP, OTA |
| `src/settings.cpp` | Einstellungen zur Laufzeit: Standardwerte, Zeitfenster, JSON (auf dem Host getestet) |
| `src/settings_store.cpp` | Einstellungen im Flash (NVS), Übergabe an die Render-Schleife |
| `src/web.cpp`, `src/web_page.h` | Einstellungsseite und ihre JSON-API |
| `src/updater.cpp` | Firmware-Update aus den GitHub-Releases |
| `src/icons.cpp` | eingebaute Promo-Symbole |
| `src/render.cpp` | rnv-Layout, Laufschrift, Statusbildschirme |
| `src/depart_anim.cpp` | Abfahrtsanimation (Sprites + Zeitablauf) |
| `src/display.cpp` | Panel-Einrichtung, Dimmen |
| `src/main.cpp` | Netzwerk-Task (Core 0) und Render-Schleife (Core 1, ~30 fps) |
| `.github/workflows/firmware.yml` | CI: Tests, Build, Release |
