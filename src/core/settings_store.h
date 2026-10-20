#pragma once
// Owner of the settings at run time: loaded from the flash file system
// (src/core/json_file.h) at start, changed by the web page in the network task,
// copied by the render loop.
#include <stdint.h>

#include "core/settings.h"

// Mounts the file system and loads the stored settings (defaults if none or
// broken; those of older firmware are moved over from NVS). Call once in
// setup(), first of the stores.
void settingsBegin();

// The current settings. Network task only (it is the only writer).
const Settings& settingsNet();

// Network task: makes `s` current (the render loop picks it up) and stores
// it. false if storing failed; `s` is used anyway until the next restart.
bool settingsSave(const Settings& s);

// Counts up with every settingsSave() since the start. Network task only.
uint32_t settingsVersion();

// A Settings in PSRAM for parsing into before settingsSave(). Network task
// only, one at a time.
Settings& settingsScratch();

// Render loop: copies the settings into `out` if they changed since
// `version`, which is updated. true if copied.
bool settingsFetch(Settings& out, uint32_t& version);
