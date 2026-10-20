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
  TEST_ASSERT_TRUE(s.disruptOn);
  TEST_ASSERT_EQUAL(DISRUPTION_PASSES, s.disruptPasses);
  TEST_ASSERT_EQUAL(DEPART_ANIM_CHANCE, s.animChance);
  TEST_ASSERT_EQUAL(DEPART_ANIM_CHANCE_BUS, s.animChanceBus);
  TEST_ASSERT_FALSE(s.promoFullRow);
  TEST_ASSERT_EQUAL(BRIGHTNESS_MAX, brightnessToPanel(100));
  TEST_ASSERT_EQUAL(151, brightnessToPanel(85));
  TEST_ASSERT_EQUAL(BRIGHTNESS_MAX, brightnessToPanel(200));
  TEST_ASSERT_EQUAL(0, brightnessToPanel(0));
  TEST_ASSERT_EQUAL(0, s.nPromos);  // none built in
}

void test_promo_from_text() {
  Promo p;
  promoFromText("[ig]@frankys", p);
  TEST_ASSERT_EQUAL_STRING("[ig]", p.icon);
  TEST_ASSERT_EQUAL_STRING("@frankys", p.text);
  TEST_ASSERT_EQUAL_STRING("", p.right);
  TEST_ASSERT_FALSE(p.hasWin);

  promoFromText("@12-18[gc]|Grilled Cheese|5 min", p);
  TEST_ASSERT_EQUAL_STRING("[gc]", p.icon);
  TEST_ASSERT_EQUAL_STRING("Grilled Cheese", p.text);
  TEST_ASSERT_EQUAL_STRING("5 min", p.right);
  TEST_ASSERT_TRUE(p.hasWin);
  TEST_ASSERT_EQUAL(12 * 60, p.win.from);
  TEST_ASSERT_EQUAL(18 * 60, p.win.to);

  promoFromText("@16Kaffee", p);
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
  s.promoFullRow = true;
  s.disruptOn = false;
  s.disruptPasses = 4;
  s.animChance = 0;
  s.animChanceBus = 1;
  promoFromText("[ig]@frankys", s.promos[s.nPromos]);
  s.promos[s.nPromos++].on = false;
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
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));

  TEST_ASSERT_FALSE(t.offEnabled);
  TEST_ASSERT_EQUAL(60, t.off.from);
  TEST_ASSERT_EQUAL(300, t.off.to);
  TEST_ASSERT_EQUAL(99, t.brightness);
  TEST_ASSERT_EQUAL(2, t.nDim);
  TEST_ASSERT_EQUAL(50, t.dim[1].value);
  TEST_ASSERT_EQUAL(12, t.promoShowS);
  TEST_ASSERT_TRUE(t.promoFullRow);
  TEST_ASSERT_FALSE(t.disruptOn);
  TEST_ASSERT_EQUAL(4, t.disruptPasses);
  TEST_ASSERT_EQUAL(0, t.animChance);
  TEST_ASSERT_EQUAL(1, t.animChanceBus);
  TEST_ASSERT_EQUAL(s.nPromos, t.nPromos);
  TEST_ASSERT_FALSE(t.promos[0].on);
  TEST_ASSERT_EQUAL_STRING("@frankys", t.promos[0].text);
  TEST_ASSERT_TRUE(t.promos[1].on);
  const Promo& o = t.promos[t.nPromos - 1];
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

void test_dim_skip() {
  settingsDefaults(s);
  s.dim[0].win = win(22 * 60, 6 * 60);
  s.dim[0].value = 20;
  s.dim[1].win = win(0, 12 * 60);
  s.dim[1].value = 50;
  s.nDim = 2;
  int32_t day = 20740;  // 2026-10-14
  TEST_ASSERT_EQUAL(day - 1, windowStartDay(s.dim[0].win, 3 * 60, day));
  TEST_ASSERT_EQUAL(day, windowStartDay(s.dim[0].win, 23 * 60, day));

  s.dim[0].skipDay = day;  // tonight's 22:00-06:00 skipped
  TEST_ASSERT_EQUAL(20, brightnessAt(s, 3 * 60, day));        // last night's still applies
  TEST_ASSERT_EQUAL(s.brightness, brightnessAt(s, 23 * 60, day));
  TEST_ASSERT_EQUAL(50, brightnessAt(s, 3 * 60, day + 1));    // the next matching one
  TEST_ASSERT_EQUAL(20, brightnessAt(s, 23 * 60, day + 1));   // only once
  TEST_ASSERT_EQUAL(20, brightnessAt(s, 23 * 60, 0));         // day unknown: no skips

  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), false);
  TEST_ASSERT_EQUAL_STRING("2026-10-14", doc["dim"][0]["skip"]);
  TEST_ASSERT_FALSE(doc["dim"][1]["skip"].is<const char*>());
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(day, t.dim[0].skipDay);
  TEST_ASSERT_EQUAL(0, t.dim[1].skipDay);

  deserializeJson(doc, "{\"dim\":[{\"from\":\"1:00\",\"to\":\"2:00\",\"skip\":\"2026-13-01\"}]}");
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(0, t.dim[0].skipDay);
}

void test_json_clamps_and_rejects() {
  JsonDocument doc;
  deserializeJson(doc,
                  "{\"brightnessPct\":999,\"promoShowS\":0,\"disruptPasses\":99,\"animChance\":500,\"off\":{\"on\":true,\"from\":\"25:00\",\"to\":\"x\"},"
                  "\"dim\":[{\"from\":\"1:00\",\"to\":\"2:00\",\"pct\":0},{\"from\":\"bad\"}],"
                  "\"promos\":[{\"preset\":\"gibt es nicht\",\"on\":false},{\"text\":\"\"},"
                  "{\"icon\":\"<script>\",\"text\":\"Ein sehr sehr sehr sehr sehr sehr sehr sehr sehr sehr langer Text\"}],"
                  "\"icons\":[{\"id\":9,\"rows\":[1]},{\"id\":1,\"rows\":[65535]}]}");
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  settingsDefaults(s);
  TEST_ASSERT_EQUAL(100, t.brightness);
  TEST_ASSERT_EQUAL(3, t.promoShowS);
  TEST_ASSERT_EQUAL(10, t.disruptPasses);
  TEST_ASSERT_EQUAL(100, t.animChance);
  TEST_ASSERT_TRUE(t.disruptOn);        // missing: default
  TEST_ASSERT_FALSE(t.promoFullRow);
  TEST_ASSERT_EQUAL(s.off.from, t.off.from);  // malformed times keep the default
  TEST_ASSERT_EQUAL(1, t.nDim);
  TEST_ASSERT_EQUAL(1, t.dim[0].value);
  TEST_ASSERT_EQUAL(2, t.nPromos);  // empty one dropped
  TEST_ASSERT_FALSE(t.promos[0].on);
  TEST_ASSERT_EQUAL_STRING("gibt es nicht", t.promos[0].text);
  const Promo& p = t.promos[t.nPromos - 1];
  TEST_ASSERT_EQUAL_STRING("", p.icon);
  TEST_ASSERT_EQUAL(sizeof(p.text) - 1, strlen(p.text));
  TEST_ASSERT_TRUE(t.icons[1].used);
  TEST_ASSERT_EQUAL_HEX16(0xFFF, t.icons[1].rows[0]);

  TEST_ASSERT_FALSE(settingsFromJson(JsonVariantConst(), t));
}

// built-in promos stored by older firmware become ordinary ones
void test_old_presets() {
  JsonDocument doc;
  deserializeJson(doc,
                  "{\"promos\":[{\"on\":false,\"preset\":\"@12[gc]|Grilled Cheese|5 min\"},"
                  "{\"on\":true,\"preset\":\"[fw]|Flat White|sofort\",\"icon\":\"[fw]\",\"text\":\"Cappuccino\",\"right\":\"sofort\"}]}");
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(2, t.nPromos);
  TEST_ASSERT_FALSE(t.promos[0].on);
  TEST_ASSERT_EQUAL_STRING("[gc]", t.promos[0].icon);
  TEST_ASSERT_EQUAL_STRING("Grilled Cheese", t.promos[0].text);
  TEST_ASSERT_EQUAL_STRING("5 min", t.promos[0].right);
  TEST_ASSERT_EQUAL(12 * 60, t.promos[0].win.from);
  TEST_ASSERT_EQUAL_STRING("Cappuccino", t.promos[1].text);

  // stored again without "preset", and gone once deleted
  JsonDocument out;
  settingsToJson(t, out.to<JsonObject>(), false);
  TEST_ASSERT_FALSE(out["promos"][0]["preset"].is<const char*>());
  TEST_ASSERT_EQUAL_STRING("Grilled Cheese", out["promos"][0]["text"]);
  out["promos"].as<JsonArray>().remove(0);
  TEST_ASSERT_TRUE(settingsFromJson(out.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(1, t.nPromos);
  TEST_ASSERT_EQUAL_STRING("Cappuccino", t.promos[0].text);
}

void test_promo_days() {
  const int32_t FRI = 20742;  // 2026-10-16
  TEST_ASSERT_EQUAL(3, weekday(0));  // 1970-01-01, a Thursday
  TEST_ASSERT_EQUAL(4, weekday(FRI));
  TEST_ASSERT_EQUAL(5, weekday(FRI + 1));
  TEST_ASSERT_EQUAL(0, weekday(FRI + 3));

  Promo p;
  TEST_ASSERT_TRUE(promoOpen(p, -1, 0));  // every day, no window: also without time
  p.days = 1 << 4 | 1 << 5;               // Fri, Sat
  TEST_ASSERT_FALSE(promoOpen(p, -1, 0));
  TEST_ASSERT_TRUE(promoOpen(p, 600, FRI));
  TEST_ASSERT_TRUE(promoOpen(p, 600, FRI + 1));
  TEST_ASSERT_FALSE(promoOpen(p, 600, FRI + 2));
  TEST_ASSERT_FALSE(promoOpen(p, 600, FRI - 1));

  // Fri and Sat 22:00-02:00: Sun 01:00 still belongs to Saturday's
  p.hasWin = true;
  p.win = win(22 * 60, 2 * 60);
  TEST_ASSERT_TRUE(promoOpen(p, 23 * 60, FRI));
  TEST_ASSERT_FALSE(promoOpen(p, 21 * 60, FRI));
  TEST_ASSERT_TRUE(promoOpen(p, 60, FRI + 2));
  TEST_ASSERT_FALSE(promoOpen(p, 60, FRI));  // Thursday's
  TEST_ASSERT_FALSE(promoOpen(p, 23 * 60, FRI + 2));
  p.on = false;
  TEST_ASSERT_FALSE(promoOpen(p, 23 * 60, FRI));

  // JSON: stored only if not every day, nothing at all counts as every day
  settingsDefaults(s);
  promoFromText("Kaffee", s.promos[s.nPromos++]);
  promoFromText("Kuchen", s.promos[s.nPromos++]);
  s.promos[0].days = 0x1F;  // Mon-Fri
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), false);
  TEST_ASSERT_EQUAL(0x1F, doc["promos"][0]["days"]);
  TEST_ASSERT_FALSE(doc["promos"][1]["days"].is<int>());
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(0x1F, t.promos[0].days);
  TEST_ASSERT_EQUAL(ALL_DAYS, t.promos[1].days);
  doc["promos"][0]["days"] = 0;
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(ALL_DAYS, t.promos[0].days);
  doc["promos"][0]["days"] = 0x1FF;
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(ALL_DAYS, t.promos[0].days);
}

// build 10 stored 0..255 under other keys: back to the defaults
void test_old_brightness_dropped() {
  JsonDocument doc;
  deserializeJson(doc, "{\"brightness\":200,\"dim\":[{\"from\":\"23:00\",\"to\":\"05:00\",\"value\":20}]}");
  TEST_ASSERT_TRUE(settingsFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(BRIGHTNESS_DAY, t.brightness);
  TEST_ASSERT_EQUAL(BRIGHTNESS_DAY, t.dim[0].value);
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
  RUN_TEST(test_promo_from_text);
  RUN_TEST(test_json_roundtrip);
  RUN_TEST(test_dim_skip);
  RUN_TEST(test_json_clamps_and_rejects);
  RUN_TEST(test_old_presets);
  RUN_TEST(test_promo_days);
  RUN_TEST(test_old_brightness_dropped);
  RUN_TEST(test_custom_icon_tag);
  return UNITY_END();
}
