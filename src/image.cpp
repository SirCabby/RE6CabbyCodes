#include "image.h"

#include <vector>

#include "log.h"

namespace re6cc::image {
namespace {

HMODULE g_exe = nullptr;
uintptr_t g_base = 0;
mem::Range g_text, g_rdata, g_data, g_whole, g_iat;
const char* g_problem = "not looked at yet";

}  // namespace

void init() {
  if (g_exe) return;
  g_exe = GetModuleHandleA(nullptr);
  g_base = reinterpret_cast<uintptr_t>(g_exe);
  IMAGE_NT_HEADERS* nt = mem::nt_headers(g_exe);
  if (!nt) {
    g_problem = "the program's headers are not a PE's";
    return;
  }
  // The Steam build's four sections, by name: .text, .rdata, .data, .rsrc.
  g_text = mem::section(g_exe, ".text");
  g_rdata = mem::section(g_exe, ".rdata");
  g_data = mem::section(g_exe, ".data");
  const mem::Range rsrc = mem::section(g_exe, ".rsrc");
  if (g_text.empty() || g_rdata.empty() || g_data.empty() || rsrc.empty()) {
    g_problem = "the program's sections are not the Steam build's (.text, .rdata, .data, .rsrc)";
    return;
  }
  g_whole = mem::Range{g_base, rsrc.end};
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT];
  if (dir.VirtualAddress && dir.Size) g_iat = mem::Range{g_base + dir.VirtualAddress, g_base + dir.VirtualAddress + dir.Size};
  g_problem = nullptr;
  logf("image: BH6.exe at %p (built %08lX); code %08X+%X, rdata %08X+%X, data %08X+%X, imports %08X+%X",
       static_cast<void*>(g_exe), static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
       static_cast<unsigned>(g_text.begin), static_cast<unsigned>(g_text.size()), static_cast<unsigned>(g_rdata.begin),
       static_cast<unsigned>(g_rdata.size()), static_cast<unsigned>(g_data.begin), static_cast<unsigned>(g_data.size()),
       static_cast<unsigned>(g_iat.begin), static_cast<unsigned>(g_iat.size()));
}

uintptr_t base() { return g_base; }
HMODULE module() { return g_exe; }
mem::Range text() { return g_text; }
mem::Range rdata() { return g_rdata; }
mem::Range data() { return g_data; }
mem::Range whole() { return g_whole; }
const char* layout_problem() { return g_problem; }
mem::Range iat() { return g_iat; }

uintptr_t slot_of(void* fn) {
  if (g_iat.empty() || !fn) return 0;
  const std::vector<uintptr_t> hits = mem::find_dwords(g_iat, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fn)), 2);
  return hits.empty() ? 0 : hits[0];
}

void* hook_import(const char* what, void* fn, void* hook, uintptr_t* slot_out) {
  const uintptr_t slot = slot_of(fn);
  if (slot_out) *slot_out = slot;
  if (!slot) {
    if (!mem::find_dwords(g_iat, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook)), 1).empty())
      logf("imports: the game's %s slot already holds our hook", what);
    else
      logf("imports: %s is not in the game's import table as %p - not hooked", what, fn);
    return nullptr;
  }
  if (!mem::write<uintptr_t>(slot, reinterpret_cast<uintptr_t>(hook))) {
    logf("imports: the game's %s slot (exe+0x%X) could not be written", what, static_cast<unsigned>(slot - g_base));
    return nullptr;
  }
  logf("imports: the game's %s slot (exe+0x%X) now points at our hook", what, static_cast<unsigned>(slot - g_base));
  return fn;
}

}  // namespace re6cc::image
