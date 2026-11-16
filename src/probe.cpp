#include "probe.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "log.h"
#include "mem.h"
#include "mt.h"
#include "units.h"

namespace re6cc::probe {
namespace {

constexpr DWORD kRareMs = 2000;    // a word that changed this recently is noise, not logged
constexpr int kMostPerTick = 40;   // log lines a tick at most ...
constexpr int kMostPerSecond = 300;  // ... and a second

struct Watched {
  std::string name;
  uintptr_t obj = 0;
  uint32_t from = 0, size = 0;  // the window of the object watched (bytes)
  std::vector<uint32_t> words;
  std::vector<DWORD> changed;  // each word: when it last changed
};
std::vector<Watched> g_watched;
DWORD g_second = 0;
int g_this_second = 0, g_dropped = 0;
bool g_said = false;

Watched* find(const std::string& name) {
  for (Watched& w : g_watched)
    if (w.name == name) return &w;
  return nullptr;
}

// The object (or a window of it) watched under `name`; a new object starts afresh.
void watch(const std::string& name, uintptr_t obj, uint32_t from, uint32_t size) {
  if (!obj || !size) return;
  Watched* w = find(name);
  if (!w) {
    g_watched.push_back(Watched{});
    w = &g_watched.back();
    w->name = name;
  }
  if (w->obj == obj && w->from == from && w->size == size) return;
  if (!mem::readable(obj + from, size)) return;
  w->obj = obj;
  w->from = from;
  w->size = size & ~3u;
  w->words.assign(w->size / 4, 0);
  // Counted as changed now: a word that keeps changing (a timer) is never logged.
  w->changed.assign(w->size / 4, GetTickCount());
  std::memcpy(w->words.data(), reinterpret_cast<const void*>(obj + from), w->size);
  logf("probe: watching %s %08X (+0x%X, 0x%X bytes)", name.c_str(), static_cast<unsigned>(obj), from, w->size);
}

void compare(Watched& w, DWORD now, int* lines) {
  if (!w.obj || !mem::readable(w.obj + w.from, w.size)) return;
  const auto* live = reinterpret_cast<const uint32_t*>(w.obj + w.from);
  for (size_t i = 0; i < w.words.size(); ++i) {
    const uint32_t v = live[i];
    if (v == w.words[i]) continue;
    const bool rare = now - w.changed[i] > kRareMs;
    if (rare) {
      if (*lines < kMostPerTick && g_this_second < kMostPerSecond) {
        logf("probe: %s+0x%X %08X -> %08X", w.name.c_str(), static_cast<unsigned>(w.from + 4 * i), w.words[i], v);
        ++*lines;
        ++g_this_second;
      } else {
        ++g_dropped;
      }
    }
    w.words[i] = v;
    w.changed[i] = now;
  }
}

uint32_t size_of(uintptr_t obj) {
  const mt::Class* c = units::class_of(obj);
  return c ? c->size : 0;
}

}  // namespace

void tick(const game::Run& run) {
  if (!config::get().trace || !run.main) return;
  if (!g_said) {
    g_said = true;
    logf("probe: on (Trace = 1) - the game's rarely changing words are logged as they change");
  }
  const DWORD now = GetTickCount();
  if (now - g_second >= 1000) {
    if (g_dropped) logf("probe: %d more changes not logged (too many at once)", g_dropped);
    g_dropped = 0;
    g_this_second = 0;
    g_second = now;
  }
  const mt::Class* main_cls = mt::find("sBioMain");
  watch("sBioMain", run.main, 0, main_cls ? main_cls->size : 0);
  for (const char* name : {"sGameMan", "sId", "sQlocInput", "sPlayer", "sCutscene", "sBioScene", "sBioArea"}) {
    const uintptr_t s = game::system(name);
    watch(name, s, 0, s ? size_of(s) : 0);
  }
  // sBioArea's areas (aGame, the stage ...): every pointer in it to an object of a
  // class deriving from cArea.
  if (const uintptr_t area = game::system("sBioArea")) {
    const mt::Class* c_area = mt::find("cArea");
    const uint32_t size = size_of(area);
    int n = 0;
    for (uint32_t off = 0; c_area && off + 4 <= size && n < 8; off += 4) {
      const uintptr_t p = mem::read<uintptr_t>(area + off);
      if (!p || (p & 3) || !mem::readable(p, 8) || !mt::is_a(p, c_area)) continue;
      const char* cls = mt::class_name_of(p);
      char name[64];
      std::snprintf(name, sizeof(name), "sBioArea+0x%X:%s", off, cls ? cls : "?");
      watch(name, p, 0, size_of(p));
      ++n;
    }
  }
  // The screens sId holds (the in-game menu, the HUD's parts): every pointer in it to
  // an object of a uIdBase class.
  if (const uintptr_t id = game::system("sId")) {
    const mt::Class* c_ui = mt::find("uIdBase");
    const uint32_t size = size_of(id);
    int n = 0;
    for (uint32_t off = 0; c_ui && off + 4 <= size && n < 48; off += 4) {
      const uintptr_t p = mem::read<uintptr_t>(id + off);
      if (!p || (p & 3) || !mem::readable(p, 8) || !mt::is_a(p, c_ui)) continue;
      const char* cls = mt::class_name_of(p);
      char name[64];
      std::snprintf(name, sizeof(name), "sId+0x%X:%s", off, cls ? cls : "?");
      watch(name, p, 0, size_of(p));
      ++n;
    }
  }
  // sBioUnit: its lines' switches, and its last fields (its pause among them).
  if (const uintptr_t su = game::system("sBioUnit")) {
    const uint32_t size = size_of(su);
    watch("sBioUnit.lines", su, 0, 0x640);
    if (size > 0x400) watch("sBioUnit.tail", su, size - 0x400, 0x400);
  }
  int lines = 0;
  for (Watched& w : g_watched) compare(w, now, &lines);
}

}  // namespace re6cc::probe
