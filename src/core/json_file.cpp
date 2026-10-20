#include "core/json_file.h"

#include <Arduino.h>
#include <LittleFS.h>

static bool mounted = false;

bool jsonFsBegin() {
  if (!mounted) mounted = LittleFS.begin(true);  // the partition is labelled "spiffs"
  return mounted;
}

DeserializationError jsonFileRead(const char* path, JsonDocument& doc, size_t& n) {
  n = 0;
  File f = mounted ? LittleFS.open(path, "r") : File();
  if (!f) return DeserializationError::EmptyInput;
  n = f.size();
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  return err;
}

bool jsonFileWrite(const char* path, const JsonDocument& doc, size_t& n) {
  n = 0;
  if (!mounted) return false;
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  n = serializeJson(doc, f);
  bool ok = n == measureJson(doc);
  f.close();
  return ok && LittleFS.rename(tmp, path);
}
