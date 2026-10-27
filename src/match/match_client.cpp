#include "match/match_client.h"

#include <Arduino.h>
#include <stdio.h>

#include "config.h"
#include "core/https_json.h"
#include "core/log.h"
#include "core/psram_alloc.h"
#include "core/settings_store.h"

struct Ticker {
  const char* teamId;
  const char* name;   // in the log
  bool against;       // the goals it concedes count, not its own
  Match current;      // written by the network task only, under `mutex`
  volatile uint32_t version;
  volatile uint32_t goalSeq;
  uint32_t testSeq;   // goalSeq of the last "Tor testen": shown without a match
  bool wasActive;
  uint32_t nextListMs;
};
static Ticker tickers[N_TEAMS] = {{MATCH_TEAM_ID, "St. Pauli", false}, {HSV_TEAM_ID, "HSV", true}};
static SemaphoreHandle_t mutex;

static uint32_t nextLiveMs = 0;
static uint8_t liveTurn = 0;  // the team whose scoreboard is asked for next

void matchBegin() { mutex = xSemaphoreCreateMutex(); }

const Match& matchNet(MatchTeam team) { return tickers[team].current; }

static void logMatch(const Ticker& t, const char* what, const Match& m) {
  char teams[56], score[8], when[16];
  matchTeamsText(m, false, teams, sizeof(teams));
  matchScoreText(m, score, sizeof(score));
  struct tm lt;
  localtime_r(&m.kickoff, &lt);
  strftime(when, sizeof(when), "%d.%m. %H:%M", &lt);
  Log.printf("[match] %s %s: %s %s (%s)%s%s\n", t.name, what, teams, score, when, m.lastGoal[0] ? ", " : "",
             m.lastGoal);
}

// Makes `m` the team's current match; a goal since the last look (the HSV:
// one conceded) counts for the animation. Logs another match, and of the
// same one a new score.
static void publish(Ticker& t, const Match& m) {
  int goals = newGoals(t.current, m, t.against);
  bool same = m.id == t.current.id;
  bool changed = !same || m.goalsHome != t.current.goalsHome || m.goalsAway != t.current.goalsAway ||
                 m.finished != t.current.finished;
  xSemaphoreTake(mutex, portMAX_DELAY);
  t.current = m;
  t.version++;
  if (goals && (t.against || settingsNet().goalAnim)) t.goalSeq++;
  xSemaphoreGive(mutex);
  if (!changed) return;
  if (goals) logMatch(t, t.against ? "GEGENTOR" : "TOR", m);
  else if (same) logMatch(t, m.finished ? "final" : "score", m);
  else if (m.id) logMatch(t, matchPhase(m, time(nullptr)) == MatchPhase::Next ? "next" : "now", m);
  else Log.printf("[match] %s: no match coming\n", t.name);
}

// One team schedule (`fixtures`: the ones to come, else the results) into `m`.
static bool fetchSchedule(const Ticker& t, bool fixtures, Match& m) {
  char url[128];
  snprintf(url, sizeof(url), "%s/all/teams/%s/schedule%s", MATCH_API_URL, t.teamId, fixtures ? "?fixture=true" : "");
  JsonDocument filter;
  buildMatchFilter(filter, false);
  JsonDocument doc(&spiRam);
  if (httpsGetJson(url, filter, doc, "match") != FetchResult::Ok) return false;
  if (!matchesFromJson(doc.as<JsonVariantConst>(), t.teamId, MATCH_LEAGUES, time(nullptr), m)) {
    Log.printf("[match] %s schedule: no events\n", t.name);
    return false;
  }
  return true;
}

// The results and the fixtures. A running or just finished match stays even
// if neither lists it (yet).
static bool fetchList(const Ticker& t, Match& m) {
  MatchPhase p = matchPhase(t.current, time(nullptr));
  m = p == MatchPhase::Live || p == MatchPhase::Final ? t.current : Match();
  return fetchSchedule(t, false, m) && fetchSchedule(t, true, m);
}

// The league's scoreboard of `cur`'s kick-off day (ESPN's days are US
// Eastern, the same date for German kick-offs in UTC).
static bool fetchScoreboard(const Match& cur, JsonDocument& doc) {
  char url[128], day[12];
  struct tm t;
  gmtime_r(&cur.kickoff, &t);
  strftime(day, sizeof(day), "%Y%m%d", &t);
  snprintf(url, sizeof(url), "%s/%s/scoreboard?dates=%s", MATCH_API_URL, cur.league, day);
  JsonDocument filter;
  buildMatchFilter(filter, true);
  return httpsGetJson(url, filter, doc, "match") == FetchResult::Ok;
}

// One scoreboard, for the next team in turn whose match runs: with both
// running the two share the polls. The other team's match comes along if
// it's on the same scoreboard (the derby too).
static void pollLive(Match& m) {
  time_t now = time(nullptr);
  int first = -1;
  for (int k = 0; k < N_TEAMS && first < 0; k++) {
    int i = (liveTurn + k) % N_TEAMS;
    if (tickers[i].wasActive && matchPollLive(tickers[i].current, now)) first = i;
  }
  if (first < 0) return;
  liveTurn = (first + 1) % N_TEAMS;
  const Match asked = tickers[first].current;
  JsonDocument doc(&spiRam);
  if (!fetchScoreboard(asked, doc)) return;
  for (int i = 0; i < N_TEAMS; i++) {
    Ticker& t = tickers[i];
    if (!t.wasActive || !matchPollLive(t.current, now)) continue;
    if (i != first && !sameScoreboard(t.current, asked)) continue;
    if (matchFromScoreboard(doc.as<JsonVariantConst>(), t.current.id, t.teamId, m)) publish(t, m);
    else Log.printf("[match] %s: %lu not on the scoreboard\n", t.name, (unsigned long)t.current.id);
  }
}

void matchMaintain(const bool active[N_TEAMS]) {
  for (int i = 0; i < N_TEAMS; i++) {
    Ticker& t = tickers[i];
    if (!active[i]) {
      if (t.wasActive) {  // the board shows no stale score later
        xSemaphoreTake(mutex, portMAX_DELAY);
        t.current = Match();
        t.version++;
        xSemaphoreGive(mutex);
      }
      t.wasActive = false;
      continue;
    }
    if (!t.wasActive) t.nextListMs = millis();  // switched on: look at once
    t.wasActive = true;
  }

  // one fetch per call, each may take a few seconds
  static Match m;
  for (Ticker& t : tickers) {
    if (!t.wasActive || (int32_t)(millis() - t.nextListMs) < 0) continue;
    bool ok = fetchList(t, m);
    t.nextListMs = millis() + (ok ? MATCH_POLL_IDLE_S : MATCH_RETRY_S) * 1000UL;
    if (!ok) return;
    publish(t, m);
    nextLiveMs = millis() + MATCH_POLL_LIVE_S * 1000UL;
    return;
  }
  if ((int32_t)(millis() - nextLiveMs) >= 0) {
    nextLiveMs = millis() + MATCH_POLL_LIVE_S * 1000UL;
    pollLive(m);
  }
}

void goalTest(MatchTeam team) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  tickers[team].testSeq = ++tickers[team].goalSeq;
  xSemaphoreGive(mutex);
  Log.printf("[match] %s goal animation test\n", tickers[team].name);
}

bool matchFetch(Match& out, uint32_t& version) {
  Ticker& t = tickers[TEAM_PAULI];
  if (version == t.version || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = t.current;
  version = t.version;
  xSemaphoreGive(mutex);
  return true;
}

bool goalFetch(MatchTeam team, uint32_t& version, Match& m) {
  Ticker& t = tickers[team];
  if (version == t.goalSeq || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  version = t.goalSeq;
  m = t.goalSeq == t.testSeq ? Match() : t.current;
  xSemaphoreGive(mutex);
  return true;
}
