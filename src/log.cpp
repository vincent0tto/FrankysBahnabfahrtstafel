#include "log.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <time.h>

#include "config.h"
#include "log_ring.h"
#include "net.h"

LogOut Log;

static LogRing ring;
static SemaphoreHandle_t lock = nullptr;  // both cores log
static bool lineStart = true;
static uint32_t bootId = 0;

void logBegin() {
  bootId = esp_random();
  char* buf = (char*)heap_caps_malloc(LOG_BUFFER_BYTES, MALLOC_CAP_SPIRAM);
  if (!buf) return;  // serial only
  ring = LogRing(buf, LOG_BUFFER_BYTES);
  lock = xSemaphoreCreateMutex();
}

uint32_t logBootId() { return bootId; }

size_t LogOut::write(const uint8_t* buf, size_t n) {
  Serial.write(buf, n);
  if (!lock || !n) return n;

  // local time once SNTP has it, before that seconds since the start
  char stamp[16];
  time_t now = time(nullptr);
  if (timeValid()) {
    struct tm lt;
    localtime_r(&now, &lt);
    strftime(stamp, sizeof stamp, "%H:%M:%S ", &lt);
  } else {
    snprintf(stamp, sizeof stamp, "+%lus ", (unsigned long)(millis() / 1000));
  }

  xSemaphoreTake(lock, portMAX_DELAY);
  const char* s = (const char*)buf;
  const char* end = s + n;
  while (s < end) {
    if (*s == '\r') {
      s++;
      continue;
    }
    if (lineStart) ring.append(stamp, strlen(stamp));
    const char* nl = (const char*)memchr(s, '\n', end - s);
    const char* stop = nl ? nl + 1 : end;
    const char* cr = (const char*)memchr(s, '\r', stop - s);
    if (cr) stop = cr;
    ring.append(s, stop - s);
    lineStart = stop[-1] == '\n';
    s = stop;
  }
  xSemaphoreGive(lock);
  return n;
}

size_t logRead(uint32_t since, char* out, size_t outCap, uint32_t& start) {
  start = 0;
  if (!lock) return 0;
  xSemaphoreTake(lock, portMAX_DELAY);
  size_t n = ring.read(since, out, outCap, start);
  xSemaphoreGive(lock);
  return n;
}
