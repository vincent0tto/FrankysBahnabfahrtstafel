#include "credentials.h"

#include <Arduino.h>
#include <Preferences.h>

#include "log.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Own namespace, apart from the settings that the web page reads back.
static const char* const NVS_NAMESPACE = "cred";

static String ssid, pass, prevSsid, prevPass, ota;

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD) && defined(OTA_PASSWORD)
#define HAVE_SECRETS 1
static uint32_t fnv1a(uint32_t h, const char* s) {
  do {
    h = (h ^ (uint8_t)*s) * 16777619u;
  } while (*s++);  // the terminating 0 is hashed too, as a separator
  return h;
}
#endif

static bool store() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString("ssid", ssid) == ssid.length() && prefs.putString("pass", pass) == pass.length() &&
            prefs.putString("prev_ssid", prevSsid) == prevSsid.length() &&
            prefs.putString("prev_pass", prevPass) == prevPass.length() && prefs.putString("ota", ota) == ota.length();
  prefs.end();
  return ok;
}

void credentialsBegin() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, true)) {  // fails if nothing was ever stored
    ssid = prefs.getString("ssid");
    pass = prefs.getString("pass");
    prevSsid = prefs.getString("prev_ssid");
    prevPass = prefs.getString("prev_pass");
    ota = prefs.getString("ota");
    prefs.end();
  }

#ifdef HAVE_SECRETS
  uint32_t seed = fnv1a(fnv1a(fnv1a(2166136261u, WIFI_SSID), WIFI_PASSWORD), OTA_PASSWORD);
  uint32_t stored = 0;
  if (prefs.begin(NVS_NAMESPACE, true)) {
    stored = prefs.getUInt("seed", 0);
    prefs.end();
  }
  if (WIFI_SSID[0] && seed != stored) {
    if (ssid != WIFI_SSID || pass != WIFI_PASSWORD) {
      prevSsid = ssid;
      prevPass = pass;
    }
    ssid = WIFI_SSID;
    pass = WIFI_PASSWORD;
    ota = OTA_PASSWORD;
    bool ok = store() && prefs.begin(NVS_NAMESPACE, false);
    if (ok) {
      ok = prefs.putUInt("seed", seed) == sizeof(seed);
      prefs.end();
    }
    Log.printf("[cred] taken from secrets.h%s\n", ok ? "" : ", storing FAILED");
  }
#endif

  Log.printf("[cred] wifi \"%s\", previous \"%s\"\n", ssid.c_str(), prevSsid.c_str());
}

const char* credentialsSsid(bool previous) { return (previous ? prevSsid : ssid).c_str(); }
const char* credentialsPass(bool previous) { return (previous ? prevPass : pass).c_str(); }
const char* credentialsOta() { return ota.c_str(); }

void credentialsSwap() {
  std::swap(ssid, prevSsid);
  std::swap(pass, prevPass);
  Log.printf("[cred] previous wifi \"%s\" works, made current%s\n", ssid.c_str(), store() ? "" : ", storing FAILED");
}

bool credentialsSetWifi(const char* newSsid, const char* newPass) {
  if (ssid != newSsid || pass != newPass) {
    prevSsid = ssid;
    prevPass = pass;
  }
  ssid = newSsid;
  pass = newPass;
  bool ok = store();
  Log.printf("[cred] wifi \"%s\" %s\n", ssid.c_str(), ok ? "saved" : "saving FAILED");
  return ok;
}
