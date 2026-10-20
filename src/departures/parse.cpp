#include "departures/parse.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static void copyStr(char* dst, size_t n, const char* src) {
  if (n == 0) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = '\0';
}

// Howard Hinnant's days_from_civil (no timegm() on newlib/ESP32).
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097 + doe - 719468;
}

bool parseIsoUtc(const char* s, time_t& out) {
  if (!s) return false;
  int Y, M, D, h, m, sec = 0, consumed = 0;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &Y, &M, &D, &h, &m, &sec, &consumed) < 6 || consumed == 0)
    return false;
  if (M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || m > 59 || sec > 60) return false;
  long offset = 0;
  const char* tz = s + consumed;
  if (*tz == '.') {  // fractional seconds
    while (*++tz >= '0' && *tz <= '9') {}
  }
  if (*tz == '+' || *tz == '-') {
    int oh = 0, om = 0;
    if (sscanf(tz + 1, "%2d:%2d", &oh, &om) < 1) return false;
    offset = (oh * 3600L + om * 60L) * (*tz == '-' ? -1 : 1);
  }
  out = (time_t)(daysFromCivil(Y, M, D) * 86400L + h * 3600L + m * 60L + sec - offset);
  return true;
}

// Characters the u8g2 "_tf" fonts can draw: ASCII + Latin-1. Typographic
// characters get an ASCII replacement, everything else becomes '?'.
static uint32_t displayableCodepoint(uint32_t cp) {
  if (cp < 0x20) return ' ';  // newline, tab, ...
  if (cp == 0xA0) return ' '; // nbsp
  if (cp < 0x7F || (cp >= 0xA1 && cp <= 0xFF)) return cp;
  switch (cp) {
    case 0x2013: case 0x2014: return '-';
    case 0x2018: case 0x2019: case 0x201A: return '\'';
    case 0x201C: case 0x201D: case 0x201E: return '"';
    case 0x2026: return '.';
    default: return '?';
  }
}

size_t sanitizeUtf8(const char* in, char* out, size_t outSize) {
  if (outSize == 0) return 0;
  size_t o = 0;
  const unsigned char* p = (const unsigned char*)(in ? in : "");
  while (*p) {
    uint32_t cp;
    int extra;
    if (*p < 0x80) { cp = *p; extra = 0; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
    else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
    else { cp = '?'; extra = -1; }  // stray continuation byte
    p++;
    for (int i = 0; i < extra; i++) {
      if ((*p & 0xC0) != 0x80) { cp = '?'; break; }
      cp = (cp << 6) | (*p++ & 0x3F);
    }
    cp = displayableCodepoint(cp);
    size_t need = cp < 0x80 ? 1 : 2;
    if (o + need + 1 > outSize) break;  // never split a character
    if (need == 1) {
      out[o++] = (char)cp;
    } else {
      out[o++] = (char)(0xC0 | (cp >> 6));
      out[o++] = (char)(0x80 | (cp & 0x3F));
    }
  }
  out[o] = '\0';
  return o;
}

static void replaceAll(std::string& s, const std::string& from, const std::string& to) {
  if (from.empty()) return;
  for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size()))
    s.replace(pos, from.size(), to);
}

void shortenDestination(const char* in, const char* abbreviations, char* out, size_t outSize) {
  std::string s(in ? in : "");
  replaceAll(s, ", ", " ");
  // "from|to;from|to"
  std::vector<std::pair<std::string, std::string>> rules;
  const char* a = abbreviations ? abbreviations : "";
  while (*a) {
    const char* end = strchr(a, ';');
    std::string pair = end ? std::string(a, end - a) : std::string(a);
    size_t bar = pair.find('|');
    if (bar > 0 && bar != std::string::npos) rules.emplace_back(pair.substr(0, bar), pair.substr(bar + 1));
    if (!end) break;
    a = end + 1;
  }
  // One pass from left to right: at each position the first matching rule
  // wins and its output is not looked at again ("X|X" protects X).
  std::string r;
  for (size_t i = 0; i < s.size();) {
    bool hit = false;
    for (const auto& rule : rules)
      if (s.compare(i, rule.first.size(), rule.first) == 0) {
        r += rule.second;
        i += rule.first.size();
        hit = true;
        break;
      }
    if (!hit) r += s[i++];
  }
  copyStr(out, outSize, r.c_str());
}

// Whole clock minutes like the official apps: 19:00 at 18:52:59 -> 8.
// (Epoch seconds and UTC offsets are whole minutes, so this is timezone-safe.)
long minutesUntil(time_t when, time_t now) {
  return (long)(when / 60) - (long)(now / 60);
}

bool hasDeparted(const Departure& d, time_t now) { return minutesUntil(d.when, now) < 0; }

void formatCountdown(const Departure& d, time_t now, char* out, size_t outSize) {
  if (d.cancelled) { copyStr(out, outSize, "fällt aus"); return; }
  long mins = minutesUntil(d.when, now);
  if (mins <= 0) { copyStr(out, outSize, "sofort"); return; }
  if (mins < 60) { snprintf(out, outSize, "%ld min", mins); return; }
  struct tm lt;
  localtime_r(&d.when, &lt);
  snprintf(out, outSize, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

bool csvContains(const char* csv, const char* item) {
  if (!csv || !item) return false;
  size_t len = strlen(item);
  const char* p = csv;
  while (*p) {
    while (*p == ' ') p++;
    const char* end = strchr(p, ',');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    while (n > 0 && p[n - 1] == ' ') n--;
    if (n == len && strncmp(p, item, len) == 0) return true;
    if (!end) break;
    p = end + 1;
  }
  return false;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

void buildFilter(JsonDocument& filter) {
  filter.clear();
  JsonObject ev = filter["stopEvents"][0].to<JsonObject>();
  ev["realtimeStatus"] = true;
  ev["isCancelled"] = true;
  ev["departureTimePlanned"] = true;
  ev["departureTimeEstimated"] = true;
  ev["location"]["properties"]["platform"] = true;
  ev["location"]["parent"]["parent"]["name"] = true;  // town of the stop
  JsonObject tr = ev["transportation"].to<JsonObject>();
  tr["disassembledName"] = true;
  tr["number"] = true;
  tr["product"]["class"] = true;
  tr["destination"]["name"] = true;
  JsonObject info = ev["infos"][0].to<JsonObject>();
  info["id"] = true;
  info["infoLinks"][0]["urlText"] = true;
}

// Finds `key` in "key|value;key|value".
static bool lookupPair(const char* pairs, const char* key, std::string& value) {
  for (const char* a = pairs ? pairs : ""; *a;) {
    const char* end = strchr(a, ';');
    std::string pair = end ? std::string(a, end - a) : std::string(a);
    size_t bar = pair.find('|');
    if (bar != std::string::npos && pair.compare(0, bar, key) == 0) {
      value = pair.substr(bar + 1);
      return true;
    }
    if (!end) break;
    a = end + 1;
  }
  return false;
}

static void appendTicker(Board& out, const char* utf8) {
  char conv[TICKER_LEN];
  sanitizeUtf8(utf8, conv, sizeof(conv));
  if (!conv[0]) return;
  size_t len = strlen(out.ticker);
  const char* sep = len ? " +++ " : "";
  if (len + strlen(sep) + strlen(conv) + 1 > sizeof(out.ticker)) return;
  strcat(out.ticker, sep);
  strcat(out.ticker, conv);
}

bool parseDepartures(JsonVariantConst root, time_t now, const ParseOptions& opt, Board& out) {
  JsonArrayConst events = root["stopEvents"];
  if (events.isNull()) return false;

  static const int kMaxCandidates = 40;
  static Departure cand[kMaxCandidates];  // static: keep it off the task stack
  bool stopCancelled[kMaxCandidates], extraTrip[kMaxCandidates];
  int nc = 0;
  out.ticker[0] = '\0';
  const char* seenInfo[16];
  int nSeen = 0;

  for (JsonObjectConst ev : events) {
    JsonObjectConst tr = ev["transportation"];
    const char* line = tr["disassembledName"] | (const char*)nullptr;
    if (!line) {
      line = tr["number"] | "";
      if (strncmp(line, "RNV ", 4) == 0) line += 4;
    }
    const char* platform = ev["location"]["properties"]["platform"] | "";

    if (opt.filterLines && *opt.filterLines && !csvContains(opt.filterLines, line)) continue;
    if (opt.filterPlatforms && *opt.filterPlatforms && !csvContains(opt.filterPlatforms, platform)) continue;

    time_t planned, est;
    if (!parseIsoUtc(ev["departureTimePlanned"] | (const char*)nullptr, planned)) continue;
    bool realtime = parseIsoUtc(ev["departureTimeEstimated"] | (const char*)nullptr, est);
    time_t when = realtime ? est : planned;

    // disruption texts (deduplicated by id)
    for (JsonObjectConst info : ev["infos"].as<JsonArrayConst>()) {
      const char* id = info["id"] | "";
      bool seen = false;
      for (int i = 0; i < nSeen; i++) seen |= strcmp(seenInfo[i], id) == 0;
      if (seen || nSeen >= 16) continue;
      seenInfo[nSeen++] = id;
      for (JsonObjectConst link : info["infoLinks"].as<JsonArrayConst>())
        appendTicker(out, link["urlText"] | "");
    }

    if (minutesUntil(when, now) < 0) continue;  // departure minute is over
    if (nc >= kMaxCandidates) continue;

    Departure& d = cand[nc++];
    memset(&d, 0, sizeof(d));
    sanitizeUtf8(line, d.line, sizeof(d.line));
    copyStr(d.platform, sizeof(d.platform), platform);
    std::string label;
    sanitizeUtf8(lookupPair(opt.platformLabels, platform, label) ? label.c_str() : platform, d.platformLabel,
                 sizeof(d.platformLabel));
    const char* dest = tr["destination"]["name"] | "";
    const char* town = ev["location"]["parent"]["parent"]["name"] | "";
    std::string byPlatform;
    if (*town && strcmp(dest, town) == 0 && lookupPair(opt.platformDirections, platform, byPlatform))
      dest = byPlatform.c_str();
    char shortDest[96];
    shortenDestination(dest, opt.abbreviations, shortDest, sizeof(shortDest));
    sanitizeUtf8(shortDest, d.dest, sizeof(d.dest));
    d.when = when;
    d.planned = planned;
    d.realtime = realtime;
    d.cls = tr["product"]["class"] | 0;
    // Only a cancellation of the whole trip counts. "isCancelled" alone also
    // fires for a stop-level DEPARTURE_CANCELLED, which the rnv dispatch sets
    // on re-planned trips (status EXTRA_STOPS) that the official apps still
    // show as running.
    d.cancelled = false;
    stopCancelled[nc - 1] = ev["isCancelled"] | false;
    extraTrip[nc - 1] = false;
    for (JsonVariantConst rs : ev["realtimeStatus"].as<JsonArrayConst>()) {
      const char* s = rs | "";
      if (strstr(s, "CANCEL")) d.cancelled = true;
      if (strcmp(s, "EXTRA_TRIP") == 0) extraTrip[nc - 1] = true;
    }
  }

  // The rnv dispatch sometimes replaces a trip by an EXTRA_TRIP (same line,
  // platform and planned time, other destination) and only marks the original
  // "isCancelled". Both would show up, so drop the original.
  bool replaced[kMaxCandidates] = {};
  for (int i = 0; i < nc; i++)
    for (int j = 0; j < nc && stopCancelled[i] && !cand[i].cancelled; j++)
      if (j != i && extraTrip[j] && cand[j].planned == cand[i].planned &&
          strcmp(cand[j].line, cand[i].line) == 0 && strcmp(cand[j].platform, cand[i].platform) == 0)
        replaced[i] = true;
  // The VRN timetable sometimes holds one departure twice (other trip code,
  // same line, platform, planned time and destination). Keep one, preferably
  // the one with real-time data.
  for (int i = 0; i < nc; i++)
    for (int j = 0; j < i && !replaced[i]; j++)
      if (!replaced[j] && cand[j].planned == cand[i].planned && strcmp(cand[j].line, cand[i].line) == 0 &&
          strcmp(cand[j].platform, cand[i].platform) == 0 && strcmp(cand[j].dest, cand[i].dest) == 0) {
        if (cand[i].realtime && !cand[j].realtime) replaced[j] = true;
        else replaced[i] = true;
      }
  int kept = 0;
  for (int i = 0; i < nc; i++)
    if (!replaced[i]) cand[kept++] = cand[i];
  nc = kept;

  std::stable_sort(cand, cand + nc, [](const Departure& a, const Departure& b) {
    return a.when != b.when ? a.when < b.when : a.planned < b.planned;
  });
  out.n = std::min(nc, MAX_DEPARTURES);
  for (int i = 0; i < out.n; i++) out.deps[i] = cand[i];
  return true;
}
