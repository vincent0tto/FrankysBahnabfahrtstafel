#pragma once
// WiFi and OTA passwords, stored in flash (NVS) and not in the firmware, so
// the firmware in the public GitHub releases holds no secrets. They get there
// from include/secrets.h on a USB/OTA build (see credentialsBegin) or from
// the web page (WLAN). Network task only.

// Loads the stored credentials. If the build has a secrets.h whose values
// differ from the ones it stored last time, those become current (once, so a
// later WLAN change on the web page is kept by builds with the same file).
void credentialsBegin();

// WiFi to try: the current one, or with `previous` the one before the last
// change. Empty ssid = none.
const char* credentialsSsid(bool previous);
const char* credentialsPass(bool previous);

// The previous WiFi worked: it becomes current again, the current one previous.
void credentialsSwap();

// New WiFi from the web page; the current one becomes the previous one.
// false if storing failed.
bool credentialsSetWifi(const char* ssid, const char* pass);

// Password for ArduinoOTA, empty = none stored (OTA stays off).
const char* credentialsOta();
