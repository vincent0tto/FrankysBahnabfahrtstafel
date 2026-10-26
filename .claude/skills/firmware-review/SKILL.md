---
name: firmware-review
description: Review changes to the Bahnabfahrtstafel firmware (ESP32-S3, HUB75 panels, web page) against this project's own pitfalls — task/core ownership, RAM vs PSRAM, watchdog, JSON filters and nesting limits, stored settings compatibility, the public OTA release, and the web page. Use when asked to review a diff, commit, branch or PR in this repo, or before pushing to main (every push to main ships to the board).
argument-hint: "[commit | range | branch | PR# — default: uncommitted changes, else HEAD]"
---

# Firmware review for Frankys Bahnabfahrtstafel

Every push to `main` that touches code is built by `.github/workflows/firmware.yml`,
published as a **public** release and installed by the board on its next start
(`src/core/updater.cpp`). There is no staging: a bad commit lands on the wall in the café.
Review with that in mind.

## 1. Pick the diff

- Argument given: a commit (`git show <sha>`), a range (`git diff a..b`), a branch
  (`git diff main...<branch>`) or a PR number (`gh pr diff <n>`).
- No argument: `git diff HEAD` if there are uncommitted changes, otherwise `git show HEAD`.

Read the changed files in full around each hunk, not only the hunk — most bugs here are about
*which task* runs the code and *where* memory comes from, which the hunk alone doesn't show.

## 2. Build and test

Run what CI runs and report the result (pass/fail with the relevant output):

```sh
pio test -e native
pio run -e esp32s3
```

If render code changed (`src/display`, `src/event/*render*`, `src/race`, `src/karaoke`,
`src/match/goal_render*`), offer to run `tools/preview/run.sh` and look at the PNGs.
If a new `.cpp` is host-testable logic, check it was added to `build_src_filter` in
`[env:native]` (`platformio.ini`) and, if rendered, to `tools/preview/run.sh`.

## 3. Project checklist

Go through every item that the diff touches. Only report real problems, each with
`file:line`, what goes wrong, and when.

**Tasks and cores** (see the header of `src/main.cpp`)
- Core 0 `netTask`: WiFi, HTTPS, web server, stores' writers. Core 1 `loop()`: rendering only.
- Functions documented as "Network task only" / "Render loop" must not be called from the
  other side. Rendering must never block on the network, on LittleFS or on a mutex wait
  without timeout (`loop()` uses `pdMS_TO_TICKS(10)`, not `portMAX_DELAY`).
- Shared state goes through the store pattern: data under a mutex + a `volatile uint32_t`
  version the reader compares (`*Fetch(out, version)`). A new shared field that skips this,
  or a reader that copies without the mutex, is a race.

**Watchdog and timing**
- Long work in `netTask` (fetches, file writes, updates) needs `esp_task_wdt_reset()` around it;
  `WDT_TIMEOUT_S` panics and reboots.
- `millis()` comparisons must be wrap-safe: `(int32_t)(millis() - t) >= 0` or
  `millis() - start >= dur`, never `millis() > t`.
- No `delay()` in the render path; frame pacing is `xTaskDelayUntil` at 33 ms.

**Memory**
- Internal RAM is scarce (WiFi/TLS + HUB75 DMA); below `MIN_FREE_HEAP_BYTES` the board reboots.
  Big buffers and `JsonDocument`s belong in PSRAM (`SpiRamAllocator` in `src/core/psram_alloc.h`,
  `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`). Flag large stack arrays (net task stack is 16 KB),
  large new statics in internal RAM, and `String` concatenation in loops.
- Fixed-size buffers: check every `strncpy`/`snprintf`/array index for bounds and termination,
  especially text coming from the web page or an API.

**JSON and APIs** (`src/core/https_json.*`, `vrn_client`, `match_client`, `parse.cpp`, `match.cpp`)
- Fetched JSON is parsed through a filter; a field read in code but missing from the filter is
  silently empty. Check filter and reader match.
- `DeserializationOption::NestingLimit` must cover the real response depth (ESPN needs 16 —
  see commit c7e7a0b). New/changed endpoints: compare against a fixture in `test/fixtures/`.
- Missing/null fields and API errors must degrade gracefully (keep last good data), never crash.
- Parser/formatter changes need a test in the matching `test/test_*/` with a fixture.

**Stored data and upgrades** (`settings*.cpp`, `event_store`, `image_store`, `json_file`)
- Boards in the field have files written by older firmware. Renamed/removed keys, changed
  meanings or ranges need a migration in `settingsFromJson` (or the store's equivalent);
  new keys need a default that matches old behaviour.
- Values from JSON must be clamped; texts cut to their buffer.
- Writes go through `jsonFileWrite` (write + rename) so a power cut can't corrupt the file.

**Security and release**
- Releases are public and built without `include/secrets.h`: nothing secret may be compiled
  in or logged. Passwords live in flash (`src/core/credentials.cpp`); `Log` output is visible
  on the web page (`/api/log`).
- New HTTPS hosts: the chain must end in a root in `include/certs.h`, otherwise the fetch fails
  on the board while working in a browser.
- Anything that could brick the board or loop restarts (updater, `ESP.restart()` paths,
  `displayBegin` failure, partition/flash settings in `platformio.ini`) gets extra scrutiny.

**Web page** (`src/web/index.html`, `src/web/web.cpp`)
- Each new `/api/...` route in `web.cpp` matches what `index.html` calls (method, path, fields).
- POST handlers validate input before passing it to a store.
- The page is gzipped into flash; keep it free of external CDN dependencies (the café WiFi may
  have no internet) unless the feature is online-only anyway.

**Conventions**
- Code comments in English, user-facing texts (web page, panel, README) and commit messages in
  German, matching the existing style. Comments explain *why*, at the density of the
  surrounding code.
- User-visible features are described in `README.md`; flag a new feature or setting that isn't.

## 4. Report

Start with the build/test result. Then list findings ranked most severe first:

- **Blocker** — would crash, reboot-loop, brick, leak a secret, or corrupt stored data on the board
- **Bug** — wrong behaviour in some situation (state when it happens)
- **Nit** — style, naming, missing test or README line

Keep each finding to a few lines with `file:line`. If nothing survives, say so plainly.
Don't edit files unless the user asks; then fix the findings and re-run step 2.
