#include "dispatch.h"

#include <cstdio>
#include <cstring>

#include "cheats.h"
#include "config.h"
#include "crash.h"
#include "framelog.h"
#include "image.h"
#include "input.h"
#include "inventory.h"
#include "log.h"
#include "mem.h"
#include "memlog.h"
#include "mt.h"
#include "overlay.h"
#include "probe.h"
#include "records.h"

namespace re6cc::dispatch {
namespace {

// The process's primary thread: DllMain(PROCESS_ATTACH) runs on it (d3d9.dll
// is one of the exe's own imports), and so does the game's frame loop.
DWORD g_main_thread = 0;
constexpr int kFrameSlot = 6;  // sBioMain's frame function (sMain::vtable[6])
using FrameFn = void(__thiscall*)(void*);
FrameFn g_orig_frame = nullptr;
uintptr_t g_frame_entry = 0;  // the vtable entry re-pointed at hk_frame
Snapshot g_snap;
CRITICAL_SECTION g_snap_lock;
bool g_lock_ready = false;
volatile LONG g_first_tick = 0;
volatile LONG g_foreign_logged = 0;
HANDLE g_first_event = nullptr;
DWORD g_last_filter = 0;
DWORD g_last_rates = 0;
bool g_in_game_prev = false;
game::Screen g_screen_prev = game::Screen::kNone;
uint32_t g_watch_prev[config::kMaxWatch] = {};
bool g_watch_seen[config::kMaxWatch] = {};

// The Watch diagnostic: each value (an offset from BH6.exe's base) logged when it changes.
void watch() {
  const config::Settings& c = config::get();
  for (int i = 0; i < c.nwatch; ++i) {
    const uintptr_t at = image::base() + c.watch[i].rva;
    uint32_t v = 0;
    if (!mem::readable(at, c.watch[i].size)) continue;
    std::memcpy(&v, reinterpret_cast<const void*>(at), c.watch[i].size);
    if (g_watch_seen[i] && v == g_watch_prev[i]) continue;
    logf("watch: exe+0x%X = 0x%X (%u)", c.watch[i].rva, v, v);
    g_watch_prev[i] = v;
    g_watch_seen[i] = true;
  }
}

void tick(void* main) {
  const DWORD now = GetTickCount();
  if (!g_first_tick) {
    InterlockedExchange(&g_first_tick, 1);
    if (g_first_event) SetEvent(g_first_event);
    logf("main-thread tick running on thread %lu (sBioMain %p's frame)", GetCurrentThreadId(), main);
  }
  if (now - g_last_filter > 1000) {
    g_last_filter = now;
    reassert_crash_filter();
  }
  if (now - g_last_rates >= 1000) {
    g_last_rates = now;
    overlay::dx9::log_rates();
  }

  if (config::get().nwatch) watch();

  Snapshot s;
  s.main_thread = g_main_thread;
  s.ticks = g_snap.ticks + 1;
  s.ready = game::ready();
  if (s.ready && !config::get().disable_game) {
    game::tick(main, &s.run);
    // The panel (at the pause, by the toggle key over play, everywhere with AlwaysShow) or a records window up:
    // the numbers they show are read this tick.
    s.run.panel = s.run.screen == game::Screen::kPause || config::get().always_show || overlay::visible() ||
                  overlay::requested();
    if (s.run.in_game != g_in_game_prev) {
      logf("game %s", s.run.in_game ? "started" : "ended");
      memlog::poke(s.run.in_game);  // a diagnostic (MemoryLog): a memory line at each load
      g_in_game_prev = s.run.in_game;
    }
    if (s.run.screen != g_screen_prev) {
      logf("screen: %s", game::screen_name(s.run.screen));
      g_screen_prev = s.run.screen;
    }
    probe::tick(s.run);  // a diagnostic (Trace = 1)
    // The cheats run every tick, paused or not: the holds are put back every frame.
    cheats::tick(s.run);
    // The characters' inventories: the view, and a count changed while paused.
    inventory::tick(s.run);
    // The records windows' screens (the menus): what they show, and the changes asked for there.
    records::tick(s.run);
    s.records = records::screen();
  }
  s.show_panel = s.run.screen == game::Screen::kPause;
  // The game's mouse is let go while the panel is up (its own thread does it) - at the pause, or opened over the
  // game with the toggle key; beside a records screen only while the pointer is over the window (the screen
  // behind stays the game's).
  const bool beside = s.records != records::Screen::kNone && !s.show_panel && !config::get().always_show;
  input::tick((s.show_panel || config::get().always_show || overlay::visible()) &&
              (!beside || overlay::capturing_mouse()));
  if (g_lock_ready) {
    EnterCriticalSection(&g_snap_lock);
    g_snap = s;
    LeaveCriticalSection(&g_snap_lock);
  }
  if (framelog::on()) {  // a diagnostic (FrameLog): a line a second on where the frames' time goes
    char where[48];
    std::snprintf(where, sizeof(where), "%s%s", g_in_game_prev ? "in a game, " : "", game::screen_name(g_screen_prev));
    framelog::tick(framelog::now_us(), where);
  }
}

void __thiscall hk_frame(void* main) {
  const bool mine = GetCurrentThreadId() == g_main_thread;
  // A diagnostic (FrameLog): the frame's time, the mod's and the game's parts of it.
  const bool timed = mine && framelog::on();
  const long long t0 = timed ? framelog::now_us() : 0;
  if (timed) framelog::begin(t0);
  if (mine) {
    tick(main);
  } else if (!InterlockedExchange(&g_foreign_logged, 1)) {
    logf("dispatch: sBioMain's frame called from thread %lu, not the main thread %lu - no tick", GetCurrentThreadId(),
         g_main_thread);
  }
  const long long t1 = timed ? framelog::now_us() : 0;
  if (timed) framelog::game_begin();
  g_orig_frame(main);
  if (timed) framelog::game_end();
  const long long t2 = timed ? framelog::now_us() : 0;
  // The game's loader, which the paused frame leaves out, run while the game stays paused
  // and an Add's archive is still loading.
  if (mine && game::ready() && !config::get().disable_game) inventory::after_frame(main);
  if (timed) framelog::end(t1 - t0, t2 - t1, framelog::now_us() - t2);
}

}  // namespace

void init() {
  g_main_thread = GetCurrentThreadId();
  g_first_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  InitializeCriticalSection(&g_snap_lock);
  g_lock_ready = true;
}

bool install() {
  const mt::Class* c = mt::find("sBioMain");
  if (!c || !c->vtable) {
    logf("ERROR: dispatch: sBioMain's vtable not found - no tick");
    return false;
  }
  const uintptr_t entry = c->vtable + kFrameSlot * sizeof(void*);
  void* was = mem::read<void*>(entry);
  // The frame function is the game's own code, not another tool's hook.
  if (!image::text().contains(reinterpret_cast<uintptr_t>(was))) {
    logf("ERROR: dispatch: sBioMain's frame slot holds %p, outside the game's code - no tick", was);
    return false;
  }
  g_orig_frame = reinterpret_cast<FrameFn>(was);
  if (!mem::write<void*>(entry, reinterpret_cast<void*>(&hk_frame))) {
    logf("ERROR: dispatch: sBioMain's vtable could not be written - no tick");
    g_orig_frame = nullptr;
    return false;
  }
  g_frame_entry = entry;
  logf("dispatch: sBioMain's frame (vtable %08X slot %d, exe+0x%X) hooked - the game's frame ticks the mod",
       static_cast<unsigned>(c->vtable), kFrameSlot, static_cast<unsigned>(reinterpret_cast<uintptr_t>(was) - image::base()));
  return true;
}

void uninstall() {
  if (g_frame_entry && g_orig_frame && mem::read<uintptr_t>(g_frame_entry) == reinterpret_cast<uintptr_t>(&hk_frame))
    mem::write<uintptr_t>(g_frame_entry, reinterpret_cast<uintptr_t>(g_orig_frame));
}

Snapshot snapshot() {
  if (!g_lock_ready) return Snapshot{};
  EnterCriticalSection(&g_snap_lock);
  const Snapshot s = g_snap;
  LeaveCriticalSection(&g_snap_lock);
  return s;
}

bool on_main_thread() { return GetCurrentThreadId() == g_main_thread; }
HANDLE first_tick_event() { return g_first_event; }

}  // namespace re6cc::dispatch
