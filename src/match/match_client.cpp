#include "match/match_client.h"

#include <Arduino.h>
#include <stdio.h>

#include "config.h"
#include "core/https_json.h"
#include "core/log.h"
#include "core/psram_alloc.h"
#include "core/settings_store.h"

static Match current;  // written by the network task only, under `mutex`
static volatile uint32_t currentVersion = 0;
static volatile uint32_t goalSeq = 0;
static SemaphoreHandle_t mutex;

static bool wasActive = false;
static uint32_t nextListMs = 0, nextLiveMs = 0;

void matchBegin() { mutex = xSemaphoreCreateMutex(); }

const Match& matchNet() { return current; }

static void logMatch(const char* what, const Match& m) {
  char teams[56], score[8], when[16];
  matchTeamsText(m, false, teams, sizeof(teams));
  matchScoreText(m, score, sizeof(score));
  struct tm lt;
  localtime_r(&m.kickoff, &lt);
  strftime(when, sizeof(when), "%d.%m. %H:%M", &lt);
  Log.printf("[match] %s: %s %s (%s)%s%s\n", what, teams, score, when, m.lastGoal[0] ? ", " : "", m.lastGoal);
}

// Makes `m` current; a goal of ours since the last look counts for the
// animation. Logs another match, and of the same one a new score.
static void publish(const Match& m) {
  int goals = newGoals(current, m);
  bool same = m.id == current.id;
  bool changed = !same || m.goalsHome != current.goalsHome || m.goalsAway != current.goalsAway ||
                 m.finished != current.finished;
  xSemaphoreTake(mutex, portMAX_DELAY);
  current = m;
  currentVersion++;
  if (goals && settingsNet().goalAnim) goalSeq++;
  xSemaphoreGive(mutex);
  if (!changed) return;
  if (goals) logMatch("TOR", m);
  else if (same) logMatch(m.finished ? "final" : "score", m);
  else if (m.id) logMatch(matchPhase(m, time(nullptr)) == MatchPhase::Next ? "next" : "now", m);
  else Log.println("[match] no match coming");
}

// One team schedule (`fixtures`: the ones to come, else the results) into `m`.
static bool fetchSchedule(bool fixtures, Match& m) {
  char url[128];
  snprintf(url, sizeof(url), "%s/all/teams/%s/schedule%s", MATCH_API_URL, MATCH_TEAM_ID, fixtures ? "?fixture=true" : "");
  JsonDocument filter;
  buildMatchFilter(filter, false);
  JsonDocument doc(&spiRam);
  if (httpsGetJson(url, filter, doc, "match") != FetchResult::Ok) return false;
  if (!matchesFromJson(doc.as<JsonVariantConst>(), MATCH_TEAM_ID, MATCH_LEAGUES, time(nullptr), m)) {
    Log.println("[match] schedule: no events");
    return false;
  }
  return true;
}

// The results and the fixtures. A running or just finished match stays even
// if neither lists it (yet).
static bool fetchList(Match& m) {
  MatchPhase p = matchPhase(current, time(nullptr));
  m = p == MatchPhase::Live || p == MatchPhase::Final ? current : Match();
  return fetchSchedule(false, m) && fetchSchedule(true, m);
}

// The match from its league's scoreboard of the kick-off day (ESPN's days
// are US Eastern, the same date for German kick-offs in UTC).
static bool fetchOne(const Match& cur, Match& m) {
  char url[128], day[12];
  struct tm t;
  gmtime_r(&cur.kickoff, &t);
  strftime(day, sizeof(day), "%Y%m%d", &t);
  snprintf(url, sizeof(url), "%s/%s/scoreboard?dates=%s", MATCH_API_URL, cur.league, day);
  JsonDocument filter;
  buildMatchFilter(filter, true);
  JsonDocument doc(&spiRam);
  if (httpsGetJson(url, filter, doc, "match") != FetchResult::Ok) return false;
  if (matchFromScoreboard(doc.as<JsonVariantConst>(), cur.id, MATCH_TEAM_ID, m)) return true;
  Log.printf("[match] %lu not on the scoreboard\n", (unsigned long)cur.id);
  return false;
}

void matchMaintain(bool active) {
  if (!active) {
    if (wasActive) {  // the board shows no stale score later
      xSemaphoreTake(mutex, portMAX_DELAY);
      current = Match();
      currentVersion++;
      xSemaphoreGive(mutex);
    }
    wasActive = false;
    return;
  }
  if (!wasActive) nextListMs = millis();  // switched on: look at once
  wasActive = true;

  // one fetch per call, each may take a few seconds
  static Match m;
  if ((int32_t)(millis() - nextListMs) >= 0) {
    bool ok = fetchList(m);
    nextListMs = millis() + (ok ? MATCH_POLL_IDLE_S : MATCH_RETRY_S) * 1000UL;
    if (!ok) return;
    publish(m);
    nextLiveMs = millis() + MATCH_POLL_LIVE_S * 1000UL;
    return;
  }
  if (matchPollLive(current, time(nullptr)) && (int32_t)(millis() - nextLiveMs) >= 0) {
    nextLiveMs = millis() + MATCH_POLL_LIVE_S * 1000UL;
    if (fetchOne(current, m)) publish(m);
  }
}

void goalTest() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  goalSeq++;
  xSemaphoreGive(mutex);
  Log.println("[match] goal animation test");
}

bool matchFetch(Match& out, uint32_t& version) {
  if (version == currentVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = current;
  version = currentVersion;
  xSemaphoreGive(mutex);
  return true;
}

bool goalFetch(uint32_t& version) {
  if (version == goalSeq) return false;
  version = goalSeq;
  return true;
}
