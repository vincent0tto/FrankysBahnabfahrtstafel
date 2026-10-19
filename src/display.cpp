#include "display.h"

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <esp_random.h>
#include <stdlib.h>

#include "config.h"
#include "event_render.h"
#include "log.h"

static MatrixPanel_I2S_DMA* matrix = nullptr;
static U8G2_FOR_ADAFRUIT_GFX u8;
static uint16_t amber;
static uint8_t currentBrightness = 0;

bool displayBegin() {
  HUB75_I2S_CFG::i2s_pins pins = {PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PIN_A,
                                  PIN_B,  PIN_C,  PIN_D,  PIN_E,  PIN_LAT, PIN_OE, PIN_CLK};
  HUB75_I2S_CFG cfg(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
  cfg.double_buff = true;
  cfg.clkphase = PANEL_CLKPHASE;
  cfg.latch_blanking = PANEL_LAT_BLANKING;
  cfg.i2sspeed = PANEL_I2S_SPEED;
  cfg.driver = PANEL_DRIVER;
  cfg.min_refresh_rate = PANEL_MIN_REFRESH_HZ;
  matrix = new MatrixPanel_I2S_DMA(cfg);
  if (!matrix->begin()) {
    Log.println("[display] begin() failed (not enough DMA memory?)");
    return false;
  }
  amber = matrix->color565(COLOR_R, COLOR_G, COLOR_B);
  u8.begin(*matrix);
  srand(esp_random());  // dice for the departure animation
  currentBrightness = brightnessToPanel(BRIGHTNESS_DAY);  // the settings take over with the first frame
  matrix->setBrightness8(currentBrightness);
  return true;
}

static bool asleep = false;

// Brightness from the settings' dim windows; the default one while the time
// isn't known. The event has one of its own.
static void updateBrightness(const Settings& s, const Event& ev, const DisplayStatus& st, time_t now) {
  uint8_t want = brightnessToPanel(ev.on          ? ev.brightness
                                   : st.timeValid ? brightnessAt(s, minuteOfDay(now), localDay(now))
                                                  : s.brightness);
  if (want != currentBrightness) {
    currentBrightness = want;
    matrix->setBrightness8(want);
  }
}

bool displayRender(const Board& b, const Settings& s, const Event& ev, const DisplayStatus& st, time_t now,
                   uint32_t ms, uint32_t eventMs) {
  if (!matrix) return false;
  // Off time: blank both buffers once and switch the LEDs off (also while
  // the nightly update installs). Never while the time is unknown, nor
  // while the event is on.
  bool off = !ev.on && st.timeValid && isOffTime(s, minuteOfDay(now));
  if (off) {
    if (!asleep) {
      Log.println("[display] off time, panel dark");
      for (int i = 0; i < 2; i++) {
        matrix->clearScreen();
        matrix->flipDMABuffer();
      }
      matrix->setBrightness8(0);
      currentBrightness = 0;
      asleep = true;
    }
    return true;
  }
  if (asleep) {
    Log.println("[display] off time over");
    asleep = false;
  }
  updateBrightness(s, ev, st, now);
  matrix->clearScreen();
  if (ev.on) renderEvent(*matrix, u8, ev, eventMs);
  else renderFrame(*matrix, u8, amber, b, s, st, now, ms);
  matrix->flipDMABuffer();
  return false;
}
