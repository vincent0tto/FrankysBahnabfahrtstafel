#include "image_store.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "event_render.h"
#include "json_file.h"
#include "log.h"

// File: this header, then the frames.
struct FileHeader {
  char magic[4];  // "EVI1"
  uint8_t frames;
  uint8_t reserved;
  uint16_t delayMs;
  char name[IMAGE_NAME_LEN];
};
static const char MAGIC[4] = {'E', 'V', 'I', '1'};
static const size_t FREE_MARGIN = 16 * 1024;  // never fill the file system up completely

struct Image {
  ImageInfo info;
  uint16_t* px = nullptr;  // PSRAM, frames * IMAGE_W * IMAGE_H
};
static Image images[MAX_IMAGES];  // px swapped by the network task under `mutex`
static SemaphoreHandle_t mutex;
static bool mounted = false;

static void path(char* out, size_t n, int id) { snprintf(out, n, "/img%d.bin", id); }
static bool validId(int id) { return id >= 0 && id < MAX_IMAGES; }

static void load(int id) {
  char p[16];
  path(p, sizeof(p), id);
  if (!LittleFS.exists(p)) return;
  File f = LittleFS.open(p, "r");
  FileHeader h;
  bool ok = f && f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && memcmp(h.magic, MAGIC, 4) == 0 &&
            h.frames >= 1 && h.frames <= MAX_IMAGE_FRAMES && h.delayMs > 0;
  size_t bytes = ok ? (size_t)h.frames * IMAGE_FRAME_BYTES : 0;
  uint16_t* px = ok ? (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM) : nullptr;
  ok = px && f.read((uint8_t*)px, bytes) == bytes;
  f.close();
  if (!ok) {
    heap_caps_free(px);
    Log.printf("[image] %s broken, skipped\n", p);
    return;
  }
  Image& im = images[id];
  im.px = px;
  im.info.used = true;
  memcpy(im.info.name, h.name, IMAGE_NAME_LEN);
  im.info.name[IMAGE_NAME_LEN - 1] = 0;
  im.info.frames = h.frames;
  im.info.delayMs = h.delayMs;
}

void imageBegin() {
  mutex = xSemaphoreCreateMutex();
  mounted = jsonFsBegin();  // mounted already by settingsBegin()
  int n = 0;
  for (int id = 0; mounted && id < MAX_IMAGES; id++) {
    load(id);
    n += images[id].info.used;
  }
  Log.printf("[image] %d loaded\n", n);
  setImageDrawer(imageDraw);
}

ImageInfo imageInfo(int id) { return validId(id) ? images[id].info : ImageInfo(); }

const uint16_t* imagePixels(int id) { return validId(id) ? images[id].px : nullptr; }

size_t imageFreeBytes() {
  if (!mounted) return 0;
  size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  return used < total ? total - used : 0;
}

size_t imageTotalBytes() { return mounted ? LittleFS.totalBytes() : 0; }

// Makes `px` (or none) the pixels of image `id`, frees the old ones.
static void swapIn(int id, uint16_t* px, const ImageInfo& info) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  uint16_t* old = images[id].px;
  images[id].px = px;
  images[id].info = info;
  xSemaphoreGive(mutex);
  heap_caps_free(old);  // the render loop draws under `mutex`, so it is done with it
}

bool imageStore(int id, const char* name, uint8_t frames, uint16_t delayMs, uint16_t* px, const char*& err) {
  err = nullptr;
  size_t bytes = (size_t)frames * IMAGE_FRAME_BYTES;
  char p[16], tmp[16];
  path(p, sizeof(p), id);
  snprintf(tmp, sizeof(tmp), "/img%d.tmp", id);
  if (!mounted) err = "Dateisystem nicht verfügbar";
  else if (!validId(id) || frames < 1 || frames > MAX_IMAGE_FRAMES) err = "ungültiges Bild";
  if (err) {
    heap_caps_free(px);
    return false;
  }
  // Written next to the old one and renamed over it; if there is only room
  // without the old one, that goes first.
  size_t need = sizeof(FileHeader) + bytes + FREE_MARGIN;
  size_t old = 0;
  if (LittleFS.exists(p)) {
    File f = LittleFS.open(p, "r");
    old = f.size();
    f.close();
  }
  if (imageFreeBytes() < need) {
    if (imageFreeBytes() + old < need) {
      heap_caps_free(px);
      err = "nicht genug Platz, bitte erst ein anderes Bild löschen";
      return false;
    }
    LittleFS.remove(p);
  }
  FileHeader h = {};
  memcpy(h.magic, MAGIC, 4);
  h.frames = frames;
  h.delayMs = delayMs;
  snprintf(h.name, sizeof(h.name), "%s", name);
  File f = LittleFS.open(tmp, "w");
  bool ok = f && f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h) && f.write((const uint8_t*)px, bytes) == bytes;
  if (f) f.close();
  ok = ok && LittleFS.rename(tmp, p);
  if (!ok) {
    LittleFS.remove(tmp);
    heap_caps_free(px);
    err = "Speichern im Flash fehlgeschlagen";
    Log.printf("[image] saving %d FAILED\n", id);
    return false;
  }
  ImageInfo info;
  info.used = true;
  memcpy(info.name, h.name, IMAGE_NAME_LEN);
  info.frames = frames;
  info.delayMs = delayMs;
  swapIn(id, px, info);
  Log.printf("[image] %d \"%s\" saved, %u frames, %u KB free\n", id, info.name, frames,
             (unsigned)(imageFreeBytes() / 1024));
  return true;
}

bool imageDelete(int id) {
  if (!validId(id)) return false;
  char p[16];
  path(p, sizeof(p), id);
  if (mounted) LittleFS.remove(p);
  swapIn(id, nullptr, ImageInfo());
  Log.printf("[image] %d deleted\n", id);
  return true;
}

void imageDraw(Adafruit_GFX& g, int id, uint32_t ms) {
  if (!validId(id)) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const Image& im = images[id];
  if (im.px) drawPixels(g, im.px + (size_t)(ms / im.info.delayMs % im.info.frames) * IMAGE_W * IMAGE_H);
  xSemaphoreGive(mutex);
}
