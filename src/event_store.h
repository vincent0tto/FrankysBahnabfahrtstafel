#pragma once
// Owner of the event (src/event.h) at run time, like settings_store.h for the
// settings: loaded from the flash file system (LittleFS on the "spiffs"
// partition, which firmware updates don't touch) at start, changed by the
// web page in the network task, copied by the render loop.
#include <stdint.h>

#include "event.h"

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
