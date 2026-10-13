#pragma once
// Owner of the settings at run time: loaded from flash (NVS) at start,
// changed by the web page in the network task, copied by the render loop.
#include <stdint.h>

#include "settings.h"

// Loads the stored settings (defaults if none or broken). Call once in setup().
void settingsBegin();

// The current settings. Network task only (it is the only writer).
const Settings& settingsNet();

// Network task: makes `s` current (the render loop picks it up) and stores
// it. false if storing failed; `s` is used anyway until the next restart.
bool settingsSave(const Settings& s);

// A Settings in PSRAM for parsing into before settingsSave(). Network task
// only, one at a time.
Settings& settingsScratch();

// Render loop: copies the settings into `out` if they changed since
// `version`, which is updated. true if copied.
bool settingsFetch(Settings& out, uint32_t& version);
