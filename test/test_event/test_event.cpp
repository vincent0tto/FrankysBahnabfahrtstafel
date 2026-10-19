// Host tests: pio test -e native
#include <string.h>
#include <unity.h>

#include "config.h"
#include "event.h"

static Event e, t;  // big, keep them off the stack

void setUp() {}
void tearDown() {}

void test_defaults() {
  eventDefaults(e);
  TEST_ASSERT_FALSE(e.on);
  TEST_ASSERT_EQUAL(BRIGHTNESS_DAY, e.brightness);
  TEST_ASSERT_EQUAL(1, e.nScenes);
  TEST_ASSERT_TRUE(e.scenes[0].nLines > 0);
}

void test_rgb565() {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(0xFFFFFF));
  TEST_ASSERT_EQUAL_HEX16(0x0000, rgb565(0x000000));
  TEST_ASSERT_EQUAL_HEX16(0xF800, rgb565(0xFF0000));
  TEST_ASSERT_EQUAL_HEX16(0x07E0, rgb565(0x00FF00));
  TEST_ASSERT_EQUAL_HEX16(0x001F, rgb565(0x0000FF));
}

void test_colors() {
  TEST_ASSERT_EQUAL_HEX32(0xFF0000, hueColor(0));
  TEST_ASSERT_EQUAL_HEX32(0x00FF00, hueColor(120));
  TEST_ASSERT_EQUAL_HEX32(0x0000FF, hueColor(240));
  TEST_ASSERT_EQUAL_HEX32(0xFF0000, hueColor(360));
  TEST_ASSERT_EQUAL_HEX32(0xFF00FF, hueColor(-60));
  TEST_ASSERT_EQUAL_HEX32(0xFF7800, dimColor(0xFF7800, 256));
  TEST_ASSERT_EQUAL_HEX32(0x7F3C00, dimColor(0xFF7800, 128));
  TEST_ASSERT_EQUAL_HEX32(0, dimColor(0xFFFFFF, 0));
}

void test_scene_at() {
  e = Event();
  uint32_t ms;
  TEST_ASSERT_EQUAL(-1, eventSceneAt(e, 0, ms));
  e.nScenes = 3;
  e.scenes[0].durS = 5;
  e.scenes[1].durS = 10;
  e.scenes[2].durS = 2;
  TEST_ASSERT_EQUAL(0, eventSceneAt(e, 0, ms));
  TEST_ASSERT_EQUAL(0, ms);
  TEST_ASSERT_EQUAL(0, eventSceneAt(e, 4999, ms));
  TEST_ASSERT_EQUAL(1, eventSceneAt(e, 5000, ms));
  TEST_ASSERT_EQUAL(0, ms);
  TEST_ASSERT_EQUAL(2, eventSceneAt(e, 16500, ms));
  TEST_ASSERT_EQUAL(1500, ms);
  // loops
  TEST_ASSERT_EQUAL(0, eventSceneAt(e, 17000 + 1234, ms));
  TEST_ASSERT_EQUAL(1234, ms);
}

void test_json_roundtrip() {
  eventDefaults(e);
  e.on = true;
  e.brightness = 42;
  e.nScenes = 2;
  Scene& sc = e.scenes[1];
  sc = Scene();
  strcpy(sc.name, "Happy Hour");
  sc.durS = 12;
  sc.bg = 0x1a0b2e;
  sc.trans = Transition::Wipe;
  sc.img = 3;
  sc.nLines = 2;
  strcpy(sc.lines[0].text, "Cocktails für alle");
  sc.lines[0].color = 0xff3bd0;
  sc.lines[0].size = TextSize::Large;
  sc.lines[0].v = VPos::Top;
  sc.lines[0].h = HAlign::Left;
  sc.lines[0].anim = Anim::Wave;
  sc.lines[0].speed = 9;
  strcpy(sc.lines[1].text, "ab 18 Uhr");
  sc.lines[1].size = TextSize::Small;
  sc.lines[1].v = VPos::Bottom;
  sc.lines[1].h = HAlign::Right;
  sc.lines[1].rainbow = true;
  sc.lines[1].anim = Anim::Type;

  JsonDocument doc;
  eventToJson(e, doc.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("#1a0b2e", doc["scenes"][1]["bg"]);
  TEST_ASSERT_EQUAL_STRING("large", doc["scenes"][1]["lines"][0]["size"]);
  TEST_ASSERT_EQUAL_STRING("wipe", doc["scenes"][1]["trans"]);
  TEST_ASSERT_EQUAL_STRING("wave", doc["scenes"][1]["lines"][0]["anim"]);
  TEST_ASSERT_EQUAL_STRING("rainbow", doc["scenes"][1]["lines"][1]["color"]);
  TEST_ASSERT_TRUE(eventFromJson(doc.as<JsonVariantConst>(), t));

  TEST_ASSERT_TRUE(t.on);
  TEST_ASSERT_EQUAL(42, t.brightness);
  TEST_ASSERT_EQUAL(2, t.nScenes);
  TEST_ASSERT_EQUAL_STRING(e.scenes[0].name, t.scenes[0].name);
  const Scene& u = t.scenes[1];
  TEST_ASSERT_EQUAL_STRING("Happy Hour", u.name);
  TEST_ASSERT_EQUAL(12, u.durS);
  TEST_ASSERT_EQUAL_HEX32(0x1a0b2e, u.bg);
  TEST_ASSERT_TRUE(u.trans == Transition::Wipe);
  TEST_ASSERT_EQUAL(3, u.img);
  TEST_ASSERT_EQUAL(-1, t.scenes[0].img);
  TEST_ASSERT_FALSE(doc["scenes"][0]["img"].is<int>());  // none: left out
  TEST_ASSERT_TRUE(t.scenes[0].trans == Transition::Cut);
  TEST_ASSERT_EQUAL(2, u.nLines);
  TEST_ASSERT_EQUAL_STRING("Cocktails für alle", u.lines[0].text);
  TEST_ASSERT_EQUAL_HEX32(0xff3bd0, u.lines[0].color);
  TEST_ASSERT_TRUE(u.lines[0].size == TextSize::Large);
  TEST_ASSERT_TRUE(u.lines[0].v == VPos::Top);
  TEST_ASSERT_TRUE(u.lines[0].h == HAlign::Left);
  TEST_ASSERT_TRUE(u.lines[0].anim == Anim::Wave);
  TEST_ASSERT_EQUAL(9, u.lines[0].speed);
  TEST_ASSERT_FALSE(u.lines[0].rainbow);
  TEST_ASSERT_TRUE(u.lines[1].size == TextSize::Small);
  TEST_ASSERT_TRUE(u.lines[1].v == VPos::Bottom);
  TEST_ASSERT_TRUE(u.lines[1].h == HAlign::Right);
  TEST_ASSERT_TRUE(u.lines[1].rainbow);
  TEST_ASSERT_TRUE(u.lines[1].anim == Anim::Type);
  TEST_ASSERT_EQUAL(EVENT_SPEED_DEFAULT, u.lines[1].speed);
}

void test_json_clamps_and_rejects() {
  TEST_ASSERT_FALSE(eventFromJson(JsonVariantConst(), t));

  JsonDocument doc;
  deserializeJson(doc, R"({"brightnessPct":500,"scenes":[
    {"name":"A very long scene name that is cut","durS":1,"bg":"red","trans":"fade","img":16,
     "lines":[{"text":"x","color":"#12345","size":"giant","v":"up","h":7,"anim":"spin","speed":0},
              {"speed":99},{},{},{"text":"fifth"}]},
    "not a scene",{"durS":9999}]})");
  TEST_ASSERT_TRUE(eventFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(100, t.brightness);
  TEST_ASSERT_EQUAL(2, t.nScenes);
  const Scene& a = t.scenes[0];
  TEST_ASSERT_EQUAL(sizeof(a.name) - 1, strlen(a.name));
  TEST_ASSERT_EQUAL(2, a.durS);
  TEST_ASSERT_EQUAL_HEX32(0, a.bg);  // malformed: black
  TEST_ASSERT_EQUAL(MAX_EVENT_LINES, a.nLines);
  TEST_ASSERT_EQUAL_HEX32(EventLine().color, a.lines[0].color);
  TEST_ASSERT_TRUE(a.lines[0].size == TextSize::Normal);
  TEST_ASSERT_TRUE(a.lines[0].v == VPos::Middle);
  TEST_ASSERT_TRUE(a.lines[0].h == HAlign::Center);
  TEST_ASSERT_TRUE(a.lines[0].anim == Anim::None);
  TEST_ASSERT_FALSE(a.lines[0].rainbow);
  TEST_ASSERT_EQUAL(1, a.lines[0].speed);
  TEST_ASSERT_EQUAL(10, a.lines[1].speed);
  TEST_ASSERT_TRUE(a.trans == Transition::Cut);
  TEST_ASSERT_EQUAL(-1, a.img);
  TEST_ASSERT_EQUAL(600, t.scenes[1].durS);
  TEST_ASSERT_EQUAL(0, t.scenes[1].nLines);

  // no "scenes": the example scene stays; an empty list is kept empty
  deserializeJson(doc, R"({"on":true})");
  TEST_ASSERT_TRUE(eventFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_TRUE(t.on);
  TEST_ASSERT_EQUAL(1, t.nScenes);
  deserializeJson(doc, R"({"scenes":[]})");
  TEST_ASSERT_TRUE(eventFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(0, t.nScenes);
}

void test_text_sanitized() {
  JsonDocument doc;
  deserializeJson(doc, R"({"lines":[{"text":"Party – heute 🎉"}]})");
  Scene sc;
  TEST_ASSERT_TRUE(sceneFromJson(doc.as<JsonVariantConst>(), sc));
  TEST_ASSERT_EQUAL_STRING("Party - heute ?", sc.lines[0].text);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_rgb565);
  RUN_TEST(test_colors);
  RUN_TEST(test_scene_at);
  RUN_TEST(test_json_roundtrip);
  RUN_TEST(test_json_clamps_and_rejects);
  RUN_TEST(test_text_sanitized);
  return UNITY_END();
}
