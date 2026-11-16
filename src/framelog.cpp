#include "framelog.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "config.h"
#include "log.h"

namespace re6cc::framelog {
namespace {

long long g_freq = 0;

// The main thread's (the frames, and the Presents made on it): touched by that thread only.
DWORD g_main = 0;
bool g_in_game = false;          // inside the game's frame function
long long g_present_in_game = 0; // Present's time (with the mod's part) inside it, this frame
long long g_prev_start = 0;      // the last frame's start
struct Frame {
  bool valid = false;
  long long tick = 0, game = 0, after = 0;
};
Frame g_cur;                     // the frame since g_prev_start, closed by the next frame's start
long long g_present_acc = 0;     // the device's Present since g_prev_start
long long g_mod_present_acc = 0; // the mod's part of those Presents
struct Second {
  int frames = 0, over33 = 0, over100 = 0, hitches = 0;
  long long span = 0, span_max = 0, game = 0, game_max = 0, present = 0, present_max = 0, mod = 0, mod_max = 0;
};
Second g_sec;
long long g_line_start = 0;

// From any thread.
volatile LONG g_presents = 0;    // every Present, whichever thread
volatile LONG g_scenes = 0;
volatile LONG g_scene_gap_max = 0;  // microseconds
DWORD g_draw_thread = 0;         // the thread of the game's first EndScene: its draw thread
long long g_last_scene = 0;      // the draw thread's own
volatile LONG g_made[kKinds] = {};
volatile LONG g_made_us[kKinds] = {};

constexpr long long kSecond = 1000000;
constexpr int kHitchesPerSecond = 10;

void raise(volatile LONG* at, LONG v) {
  LONG cur = *at;
  while (v > cur) {
    const LONG was = InterlockedCompareExchange(at, v, cur);
    if (was == cur) break;
    cur = was;
  }
}

LONG clamp32(long long v) { return v < 0 ? 0 : v > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<LONG>(v); }

double ms(long long us) { return static_cast<double>(us) / 1000.0; }

// The screen, for the lines: a copy - the tick's text is its own stack buffer, gone once tick() returns (a long
// frame's line, written later from begin(), printed that dead buffer's bytes until 2026-11-13).
char g_where[64] = "";

}  // namespace

bool on() { return config::get().frame_log; }

long long now_us() {
  if (!g_freq) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart > 0 ? f.QuadPart : 1;
  }
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  // Split so the multiplication cannot overflow however long the machine has been up.
  return (c.QuadPart / g_freq) * kSecond + (c.QuadPart % g_freq) * kSecond / g_freq;
}

void game_begin() {
  if (!g_main) g_main = GetCurrentThreadId();
  g_in_game = true;
}

void game_end() { g_in_game = false; }

void begin(long long start) {
  if (!g_main) g_main = GetCurrentThreadId();
  // The frame before this one ends where this one starts: everything between the two starts is its own.
  if (g_prev_start && g_cur.valid) {
    const long long span = start - g_prev_start;
    const long long present = g_present_acc;
    const long long mod = g_cur.tick + g_cur.after + g_mod_present_acc;
    Second& s = g_sec;
    ++s.frames;
    s.span += span;
    if (span > s.span_max) s.span_max = span;
    if (span > 33333) ++s.over33;
    if (span > 100000) ++s.over100;
    s.game += g_cur.game;
    if (g_cur.game > s.game_max) s.game_max = g_cur.game;
    s.present += present;
    if (present > s.present_max) s.present_max = present;
    s.mod += mod;
    if (mod > s.mod_max) s.mod_max = mod;
    if (span > 100000 && s.hitches < kHitchesPerSecond) {
      ++s.hitches;
      const long long rest = span - g_cur.game - present - mod;
      logf("frame: a %.1f ms frame (%s) - the game's frame %.1f ms, Present %.1f, the mod %.3f, the rest %.1f",
           ms(span), g_where, ms(g_cur.game), ms(present), ms(mod), ms(rest < 0 ? 0 : rest));
    }
  }
  g_prev_start = start;
  g_present_acc = 0;
  g_mod_present_acc = 0;
  g_present_in_game = 0;
  g_cur = Frame{};
}

void end(long long tick_us, long long game_us, long long after_us) {
  g_cur.valid = true;
  g_cur.tick = tick_us;
  g_cur.after = after_us;
  g_cur.game = game_us - g_present_in_game;
  if (g_cur.game < 0) g_cur.game = 0;
}

void present(long long mod_us, long long present_us) {
  InterlockedIncrement(&g_presents);
  if (!g_main || GetCurrentThreadId() != g_main) return;
  g_present_acc += present_us;
  g_mod_present_acc += mod_us;
  if (g_in_game) g_present_in_game += mod_us + present_us;
}

void end_scene(long long at) {
  // The game's draw thread only: the panel's own scene (drawn in Present, on the main thread) is not the game's.
  const DWORD me = GetCurrentThreadId();
  if (!g_draw_thread) g_draw_thread = me;
  if (me != g_draw_thread) return;
  InterlockedIncrement(&g_scenes);
  if (g_last_scene) raise(&g_scene_gap_max, clamp32(at - g_last_scene));
  g_last_scene = at;
}

void made(Kind kind, long long us) {
  if (kind < 0 || kind >= kKinds) return;
  InterlockedIncrement(&g_made[kind]);
  InterlockedExchangeAdd(&g_made_us[kind], clamp32(us));
}

void tick(long long now, const char* where) {
  std::snprintf(g_where, sizeof(g_where), "%s", where ? where : "");
  if (!g_line_start) {
    g_line_start = now;
    return;
  }
  const long long took = now - g_line_start;
  if (took < kSecond) return;
  g_line_start = now;
  const Second s = g_sec;
  g_sec = Second{};
  const LONG presents = InterlockedExchange(&g_presents, 0);
  const LONG scenes = InterlockedExchange(&g_scenes, 0);
  const LONG gap = InterlockedExchange(&g_scene_gap_max, 0);
  LONG made[kKinds], made_us[kKinds];
  for (int k = 0; k < kKinds; ++k) {
    made[k] = InterlockedExchange(&g_made[k], 0);
    made_us[k] = InterlockedExchange(&g_made_us[k], 0);
  }
  const int n = s.frames ? s.frames : 1;
  char what[256] = "";
  static const char* const kNames[kKinds] = {"textures", "buffers", "shaders", "declarations"};
  for (int k = 0; k < kKinds; ++k) {
    if (!made[k]) continue;
    const size_t at = std::strlen(what);
    std::snprintf(what + at, sizeof(what) - at, "%s%ld %s (%.1f ms)", at ? ", " : " | made ", static_cast<long>(made[k]),
                  kNames[k], ms(made_us[k]));
  }
  logf("frame: %s | %d frames in %.2f s, %d presents - average %.1f ms, the longest %.1f (%d over 33 ms, %d over "
       "100) | the game's frame %.1f, the longest %.1f | Present %.1f, the longest %.1f | the draw thread %ld scenes, "
       "the longest gap %.1f | the mod %.3f, the longest %.3f%s",
       g_where, s.frames, static_cast<double>(took) / kSecond, static_cast<int>(presents), ms(s.span / n),
       ms(s.span_max), s.over33, s.over100, ms(s.game / n), ms(s.game_max), ms(s.present / n), ms(s.present_max),
       static_cast<long>(scenes), ms(gap), ms(s.mod / n), ms(s.mod_max), what);
}

}  // namespace re6cc::framelog
