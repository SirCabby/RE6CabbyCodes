#include "boot.h"

#include <windows.h>

#include <cstdio>

#include "cheats.h"
#include "inventory.h"
#include "config.h"
#include "crash.h"
#include "dispatch.h"
#include "game.h"
#include "image.h"
#include "input.h"
#include "log.h"
#include "mem.h"
#include "overlay.h"
#include "records.h"

namespace re6cc::boot {
namespace {

volatile LONG g_done = 0;
char g_status[200] = "the game has not started its renderer yet";

// The import slots re-pointed at DllMain, with what they held, for the unload.
struct Hooked {
  uintptr_t slot = 0;
  uintptr_t was = 0, hook = 0;
};
Hooked g_hooked[4];
int g_nhooked = 0;

// The slot of `dll!name` in BH6.exe's import table (its names: the table keeps
// them beside the slots), and what it holds now (0: none).
uintptr_t named_slot(const char* dll, const char* name, void** holds) {
  const uintptr_t slot = mem::iat_slot(image::module(), dll, name);
  *holds = slot ? mem::read<void*>(slot) : nullptr;
  if (!*holds) {
    logf("imports: %s!%s not found in BH6.exe's import table - not hooked", dll, name);
    return 0;
  }
  // Bound already (the loader binds a program's imports before any DLL's DllMain
  // runs): a slot still holding its name's place would be bound over the hook.
  if (!mem::in_module(*holds)) {
    logf("imports: %s!%s is not bound yet (%p) - not hooked", dll, name, *holds);
    return 0;
  }
  return slot;
}

// That slot re-pointed at `hook`.
bool hook_slot(uintptr_t slot, const char* dll, const char* name, void* hook) {
  const uintptr_t was = mem::read<uintptr_t>(slot);
  if (!mem::write<uintptr_t>(slot, reinterpret_cast<uintptr_t>(hook))) {
    logf("imports: BH6.exe's %s!%s slot could not be written - not hooked", dll, name);
    return false;
  }
  if (g_nhooked < 4) g_hooked[g_nhooked++] = Hooked{slot, was, reinterpret_cast<uintptr_t>(hook)};
  logf("imports: BH6.exe's %s!%s (exe+0x%X) now comes to the mod", dll, name, static_cast<unsigned>(slot - image::base()));
  return true;
}

}  // namespace

void imports() {
  image::init();
  if (const char* problem = image::layout_problem()) {
    std::snprintf(g_status, sizeof(g_status), "%s - not the Steam version the mod was made for", problem);
    logf("ERROR: boot: %s (the import hooks still go in: they are the game's imports, not its code)", g_status);
  }
  // The game's C runtime sets its exception filter as it starts: chained under
  // ours. Its own end (ExitProcess) is logged, with the code it came through.
  void* holds = nullptr;
  if (const uintptr_t slot = named_slot("kernel32.dll", "SetUnhandledExceptionFilter", &holds))
    hook_slot(slot, "kernel32.dll", "SetUnhandledExceptionFilter", crash_filter_hook());
  if (const uintptr_t slot = named_slot("kernel32.dll", "ExitProcess", &holds))
    if (void* hook = crash_exit_hook("ExitProcess", holds)) hook_slot(slot, "kernel32.dll", "ExitProcess", hook);
  if (!config::get().disable_input) input::install();
  // The renderer: the game's Direct3DCreate9 comes to the mod (the boot runs at
  // its first call), then goes on to whichever d3d9.dll the game was bound to.
  overlay::dx9::install_import(image::module());
}

void remove_imports() {
  for (int i = 0; i < g_nhooked; ++i) {
    const Hooked& h = g_hooked[i];
    if (mem::read<uintptr_t>(h.slot) == h.hook) mem::write<uintptr_t>(h.slot, h.was);
  }
  g_nhooked = 0;
}

void started() {
  if (image::layout_problem()) {
    InterlockedExchange(&g_done, 1);
    return;
  }
  logf("boot: the game's renderer starts (thread %lu) - its class records are built", GetCurrentThreadId());
  if (config::get().disable_game) {
    logf("boot: game hooks disabled by config");
  } else if (!game::discover()) {
    std::snprintf(g_status, sizeof(g_status), "%s", game::status_text());
  }
  if (game::ready()) {
    cheats::install();
    inventory::install();
    records::install();
  }
  if (!config::get().disable_tick && game::ready()) dispatch::install();
  if (game::ready()) std::snprintf(g_status, sizeof(g_status), "ready");
  InterlockedExchange(&g_done, 1);
}

bool done() { return g_done != 0; }
const char* status() { return g_status; }

}  // namespace re6cc::boot
