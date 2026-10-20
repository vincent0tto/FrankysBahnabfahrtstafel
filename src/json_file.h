#pragma once
// JSON files on the flash file system (LittleFS on the "spiffs" partition,
// which firmware updates don't touch), for the settings and the event.
#include <ArduinoJson.h>
#include <stddef.h>

// Mounts the file system, formatting it on the very first start. Safe to
// call more than once. false if it isn't available.
bool jsonFsBegin();

// Reads `path` into `doc` (PSRAM). `n`: file size, 0 if there is none (then
// it returns EmptyInput).
DeserializationError jsonFileRead(const char* path, JsonDocument& doc, size_t& n);

// Writes `doc` next to `path` and renames it over it, so a power cut leaves
// either the old or the new file. `n`: bytes written.
bool jsonFileWrite(const char* path, const JsonDocument& doc, size_t& n);
