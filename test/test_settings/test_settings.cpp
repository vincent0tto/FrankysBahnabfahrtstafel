// Host tests: pio test -e native
#include <string.h>
#include <unity.h>

#include "config.h"
#include "settings.h"

static Settings s, t;  // big, keep them off the stack

void setUp() {}
void tearDown() {}

static TimeWindow win(int from, int to) {
  TimeWindow w;
  w.from = from;
  w.to = to;
  return w;
}

void test_in_window() {
  TEST_ASSERT_TRUE(inWindow(win(120, 360), 120));
  TEST_ASSERT_TRUE(inWindow(win(120, 360), 359));
  TEST_ASSERT_FALSE(inWindow(win(120, 360), 360));
  TEST_ASSERT_FALSE(inWindow(win(120, 360), 60));
  // over midnight
  TEST_ASSERT_TRUE(inWindow(win(23 * 60, 5 * 60), 23 * 60 + 30));
  TEST_ASSERT_TRUE(inWindow(win(23 * 60, 5 * 60), 0));
  TEST_ASSERT_FALSE(inWindow(win(23 * 60, 5 * 60), 5 * 60));
  TEST_ASSERT_FALSE(inWindow(win(23 * 60, 5 * 60), 12 * 60));
  // until midnight, empty
  TEST_ASSERT_TRUE(inWindow(win(12 * 60, 1440), 1439));
  TEST_ASSERT_FALSE(inWindow(win(600, 600), 600));
}

void test_defaults() {
  settingsDefaults(s);
  TEST_ASSERT_EQUAL(BRIGHTNESS_DAY, s.brightness);
  TEST_ASSERT_TRUE(s.offEnabled);
  TEST_ASSERT_TRUE(isOffTime(s, 3 * 60));
  TEST_ASSERT_FALSE(isOffTime(s, 6 * 60));
  TEST_ASSERT_EQUAL(BRIGHTNESS_NIGHT, brightnessAt(s, 23 * 60));
  TEST_ASSERT_EQUAL(BRIGHTNESS_DAY, brightnessAt(s, 12 * 60));
  static const char* const presets[] = PROMO_TEXTS;
  TEST_ASSERT_EQUAL(sizeof(presets) / sizeof(presets[0]), s.nPromos);
  for (int i = 0; i < s.nPromos; i++) TEST_ASSERT_EQUAL_STRING(presets[i], s.promos[i].preset);
}

void test_promo_from_config() {
  Promo p;
  promoFromConfig("[ig]@frankys", p);
  TEST_ASSERT_EQUAL_STRING("[ig]", p.icon);
  TEST_ASSERT_EQUAL_STRING("@frankys", p.text);
  TEST_ASSERT_EQUAL_STRING("", p.right);
  TEST_ASSERT_FALSE(p.hasWin);

  promoFromConfig("@12-18[gc]|Grilled Cheese|5 min", p);
  TEST_ASSERT_EQUAL_STRING("[gc]", p.icon);
  TEST_ASSERT_EQUAL_STRING("Grilled Cheese", p.text);
  TEST_ASSERT_EQUAL_STRING("5 min", p.right);
  TEST_ASSERT_TRUE(p.hasWin);
  TEST_ASSERT_EQUAL(12 * 60, p.win.from);
  TEST_ASSERT_EQUAL(18 * 60, p.win.to);

  promoFromConfig("@16Kaffee", p);
  TEST_ASSERT_EQUAL(1440, p.win.to);
  TEST_ASSERT_EQUAL_STRING("Kaffee", p.text);
}

void test_json_roundtrip() {
  settingsDefaults(s);
  s.offEnabled = false;
  s.off = win(60, 300);
  s.brightness = 99;
  s.dim[1].win = win(18 * 60, 19 * 60);
  s.dim[1].value = 50;
  s.nDim = 2;
  s.promoShowS = 12;
  s.promos[0].on = false;
  Promo own;
  strcpy(own.icon, "[c2]");
  strcpy(own.text, "Waffeln");
  strcpy(own.right, "gleich");
  own.hasWin = true;
  own.win = win(14 * 60, 17 * 60 + 30);
  s.promos[s.nPromos++] = own;
  s.icons[2].used = true;
  strcpy(s.icons[2].name, "Herz");
  s.icons[2].rows[0] = 0x0F0;

  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), false);
  TEST_ASSERT_FALSE(doc["promos"][0]["text"].is<const char*>());  // stored without preset details
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));

  TEST_ASSERT_FALSE(t.offEnabled);
  TEST_ASSERT_EQUAL(60, t.off.from);
  TEST_ASSERT_EQUAL(300, t.off.to);
  TEST_ASSERT_EQUAL(99, t.brightness);
  TEST_ASSERT_EQUAL(2, t.nDim);
  TEST_ASSERT_EQUAL(50, t.dim[1].value);
  TEST_ASSERT_EQUAL(12, t.promoShowS);
  TEST_ASSERT_EQUAL(s.nPromos, t.nPromos);
  TEST_ASSERT_FALSE(t.promos[0].on);
  TEST_ASSERT_TRUE(t.promos[1].on);
  const Promo& o = t.promos[t.nPromos - 1];
  TEST_ASSERT_NULL(o.preset);
  TEST_ASSERT_EQUAL_STRING("[c2]", o.icon);
  TEST_ASSERT_EQUAL_STRING("Waffeln", o.text);
  TEST_ASSERT_EQUAL_STRING("gleich", o.right);
  TEST_ASSERT_TRUE(o.hasWin);
  TEST_ASSERT_EQUAL(17 * 60 + 30, o.win.to);
  TEST_ASSERT_TRUE(t.icons[2].used);
  TEST_ASSERT_FALSE(t.icons[0].used);
  TEST_ASSERT_EQUAL_STRING("Herz", t.icons[2].name);
  TEST_ASSERT_EQUAL_HEX16(0x0F0, t.icons[2].rows[0]);
}

void test_json_clamps_and_rejects() {
  JsonDocument doc;
  deserializeJson(doc,
                  "{\"brightness\":999,\"promoShowS\":0,\"off\":{\"on\":true,\"from\":\"25:00\",\"to\":\"x\"},"
                  "\"dim\":[{\"from\":\"1:00\",\"to\":\"2:00\",\"value\":0},{\"from\":\"bad\"}],"
                  "\"promos\":[{\"preset\":\"gibt es nicht\",\"on\":false},{\"text\":\"\"},"
                  "{\"icon\":\"<script>\",\"text\":\"Ein sehr sehr sehr sehr sehr sehr sehr sehr sehr sehr langer Text\"}],"
                  "\"icons\":[{\"id\":9,\"rows\":[1]},{\"id\":1,\"rows\":[65535]}]}");
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  settingsDefaults(s);
  TEST_ASSERT_EQUAL(255, t.brightness);
  TEST_ASSERT_EQUAL(3, t.promoShowS);
  TEST_ASSERT_EQUAL(s.off.from, t.off.from);  // malformed times keep the default
  TEST_ASSERT_EQUAL(1, t.nDim);
  TEST_ASSERT_EQUAL(1, t.dim[0].value);
  TEST_ASSERT_EQUAL(s.nPromos + 1, t.nPromos);  // unknown preset and empty one dropped
  const Promo& p = t.promos[t.nPromos - 1];
  TEST_ASSERT_EQUAL_STRING("", p.icon);
  TEST_ASSERT_EQUAL(sizeof(p.text) - 1, strlen(p.text));
  TEST_ASSERT_TRUE(t.icons[1].used);
  TEST_ASSERT_EQUAL_HEX16(0xFFF, t.icons[1].rows[0]);

  TEST_ASSERT_FALSE(settingsFromJson(JsonVariantConst(), t));
}

void test_custom_icon_tag() {
  TEST_ASSERT_EQUAL(0, customIconIndex("[c0]"));
  TEST_ASSERT_EQUAL(7, customIconIndex("[c7]"));
  TEST_ASSERT_EQUAL(-1, customIconIndex("[c8]"));
  TEST_ASSERT_EQUAL(-1, customIconIndex("[gc]"));
  TEST_ASSERT_EQUAL(-1, customIconIndex("[c1]x"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_in_window);
  RUN_TEST(test_defaults);
  RUN_TEST(test_promo_from_config);
  RUN_TEST(test_json_roundtrip);
  RUN_TEST(test_json_clamps_and_rejects);
  RUN_TEST(test_custom_icon_tag);
  return UNITY_END();
}
