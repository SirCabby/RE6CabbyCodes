#pragma once

#include <cstddef>

struct IDirect3DDevice9;

// A diagnostic (MemoryLog = <seconds> in the ini; 0, the default, is off): where the game's
// memory goes. BH6.exe is a 32-bit program: it has 4 GB of address space at most, and under
// Proton's usual mode (the old WoW64: a 32-bit Linux process) the Linux side lives in the
// same 4 GB - Wine's own libraries, the 32-bit Vulkan driver with its shader compiler, and
// the GPU memory Direct3D (DXVK) maps in. Each line, from the mod's own thread:
// - the mappings as Linux has them (/proc/self/maps, through Wine's unix path): what is in
//   use by kind, what Wine holds in reserve for Windows' allocations (mapped, no access),
//   and what is not mapped at all - the Linux side's room (a driver's allocation, a GPU
//   mapping) - with its largest gap;
// - the Windows view (VirtualQuery): committed by type, reserved, free and the largest free
//   block - the room the game's and Direct3D's own allocations are made from;
// - the texture memory Direct3D still offers (the device's GetAvailableTextureMem);
// - the mod's own heap (its operator new/delete and ImGui's allocator count into it).
// A line every <seconds>, and at once when a game starts or ends (a load). The whole mapping
// list goes to RE6CabbyCodes.maps.txt (the run before kept as .prev.txt) at the first game's
// start, each time the room falls below 512, 256, 128 or 64 MB, and at a crash.
namespace re6cc::memlog {

void start();                                // DllMain, after the ini: the thread, when it is asked for
void poke(bool game_started);                // the tick: a game started or ended - a line now
void on_present(IDirect3DDevice9* device);   // the present hook: the texture memory, now and then
void on_crash();                             // the crash logger: a last line and the mappings

// ImGui's allocator (ImGui::SetAllocatorFunctions), counted as the mod's heap.
void* heap_alloc(size_t size, void* user);
void heap_free(void* p, void* user);

}  // namespace re6cc::memlog
