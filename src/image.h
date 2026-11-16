#pragma once

#include <windows.h>

#include <cstdint>

#include "mem.h"

// BH6.exe as it is in memory. The Steam build is a plain PE - no packer, no DRM
// wrapper: its four sections are the code (.text), the read-only data (.rdata:
// the import table at its start, the vtables, the strings), the data (.data, most
// of it zero-filled at load: MT Framework's class records live there, built by the
// game's static initializers) and the resources (.rsrc). The image is not
// relocatable (based at 0x400000), but nothing here depends on where it lies.
namespace re6cc::image {

void init();                // the exe's sections (cheap; from DllMain)
uintptr_t base();
HMODULE module();
mem::Range text();          // the code
mem::Range rdata();         // read-only data: the import table, vtables, strings
mem::Range data();          // the data, its zero-filled part included (the class records live there)
mem::Range whole();         // headers to .rsrc
const char* layout_problem();  // null when the sections are as the Steam build has them

// The import address table (the PE's own directory for it).
mem::Range iat();

// The slot of the game's import table that holds `fn` (0 when none does), and the
// hook of one: the slot re-pointed at `hook`, its value before handed back.
uintptr_t slot_of(void* fn);
void* hook_import(const char* what, void* fn, void* hook, uintptr_t* slot_out = nullptr);

}  // namespace re6cc::image
