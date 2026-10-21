#pragma once
// Owner of the event (src/event/event.h) and the karaoke list at run time, like settings_store.h for the
// settings: loaded from the flash file system (LittleFS on the "spiffs"
// partition, which firmware updates don't touch) at start, changed by the
// web page in the network task, copied by the render loop.
#include <stdint.h>

#include "event/event.h"
#include "karaoke/karaoke.h"
#include "race/race.h"

// Mounts the file system (formats it on the very first start) and loads the
// stored event (defaults if none or broken). Call once in setup().
void eventBegin();

// The current event. Network task only (it is the only writer).
const Event& eventNet();

// Network task: makes `e` current (the render loop picks it up) and stores
// it. false if storing failed; `e` is used anyway until the next restart.
bool eventSave(const Event& e);

// Counts up with every eventSave() since the start. Network task only.
uint32_t eventVersion();

// An Event in PSRAM for parsing into before eventSave(). Network task only,
// one at a time.
Event& eventScratch();

// Render loop: copies the event into `out` if it changed since `version`,
// which is updated. true if copied.
bool eventFetch(Event& out, uint32_t& version);

// Network task: shows `sc` on the board for EVENT_TEST_S seconds (whether the
// event is on or not), without storing it. For "Auf der Tafel testen".
void eventTest(const Scene& sc);

// Network task: ends such a test at once.
void eventTestStop();

// Render loop: if a test started or stopped since `version` (updated),
// `active` tells which and `out` gets the scene to test. true if changed.
bool eventTestFetch(Scene& out, bool& active, uint32_t& version);

// The karaoke list (src/karaoke/karaoke.h), the same way: loaded in eventBegin(),
// stored as its own file, with its own version.
const Karaoke& karaokeNet();
bool karaokeSave(const Karaoke& k);
uint32_t karaokeVersion();
Karaoke& karaokeScratch();
bool karaokeFetch(Karaoke& out, uint32_t& version);

// Network task: starts the tram race (src/race/race.h) worked out from `seed`,
// over everything else, or a new one if one is running; ends by itself. With
// `fiesta` the next seed from `seed` on whose race has the dancing Mexican.
// Returns the race, valid until the next call.
const Race& raceStart(uint32_t seed, bool fiesta = false);

// Network task: ends the race at once.
void raceStop();

// Network task: whether a race is on the board.
bool raceRunning();

// Render loop: like eventTestFetch() for the race: `seed` of the one to
// start if `active`.
bool raceFetch(uint32_t& seed, bool& active, uint32_t& version);
