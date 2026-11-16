#include "units.h"

#include "game.h"

#include <unordered_map>

#include "image.h"
#include "log.h"
#include "mem.h"

namespace re6cc::units {
namespace {

Layout g_layout;
std::unordered_map<uintptr_t, const mt::Class*> g_by_vtable;

// The first match of `pattern` in the `span` bytes from `fn`, or 0.
uintptr_t find_in(uintptr_t fn, size_t span, const char* pattern) {
  if (!image::text().contains(fn)) return 0;
  return mem::find_pattern(mem::Range{fn, fn + span}, pattern);
}

}  // namespace

bool discover() {
  const mt::Class* c = mt::find("sUnit");
  if (!c || !c->vtable) {
    logf("ERROR: units: sUnit's vtable not found");
    return false;
  }
  // Slot 10 walks one line: the line's address is sUnit + index * 24; its flags
  // and first unit, then each unit's state flags and next link.
  //   mov eax,[sMain]; cmp [eax+68],0; push ebp; mov ebp,ecx; jne; cmp byte [eax+6C],0; jne;
  //   mov ecx,[esp+8]; lea eax,[ecx+ecx*2]; mov edx,[ebp+eax*8+3C]; lea eax,[ebp+eax*8]; shr edx,1;
  //   test dl,1; jne; push esi; mov esi,[eax+40]; test esi,esi; je; push ebx; mov bl,[eax+3C];
  //   mov eax,[ebp]; mov edx,[eax+3C]; push edi; push ecx; mov ecx,ebp; and bl,1; call edx;
  //   mov eax,[esi+10]; mov edi,[esi+18]; prefetcht0 [edi]; mov ecx,eax; and cl,7
  const uintptr_t walk = find_in(mem::read<uintptr_t>(c->vtable + 10 * 4), 0x60,
                                 "A1 ?? ?? ?? ?? 83 78 ?? 00 55 8B E9 75 ?? 80 78 ?? 00 0F 85 ?? ?? ?? ?? 8B 4C 24 08 "
                                 "8D 04 49 8B 54 C5 ?? 8D 44 C5 00 D1 EA F6 C2 01 0F 85 ?? ?? ?? ?? 56 8B 70 ?? 85 F6 "
                                 "0F 84 ?? ?? ?? ?? 53 8A 58 ?? 8B 45 00 8B 50 ?? 57 51 8B CD 80 E3 01 FF D2 8B 46 ?? "
                                 "8B 7E ?? 0F 18 0F 8B C8 80 E1 07");
  // Slot 6 runs every line in use, in the order its table gives: how many there
  // are is a field of sUnit's (read through the game's own pointer to it).
  //   mov ecx,[sUnit]; push edi; xor edi,edi; cmp [ecx+count],edi; jbe; mov edx,[esi];
  //   mov eax,[esi+edi*4+order]; mov edx,[edx+24]; push eax; mov ecx,esi; call edx
  const uintptr_t run = find_in(mem::read<uintptr_t>(c->vtable + 6 * 4), 0x80,
                                "8B 0D ?? ?? ?? ?? 57 33 FF 39 B9 ?? ?? ?? ?? 76 ?? 8B 16 8B 84 BE ?? ?? ?? ?? 8B 52 24 50 "
                                "8B CE FF D2");
  if (!walk || !run) {
    logf("ERROR: units: sUnit's line walk (slot 10: %s) or its frame (slot 6: %s) is not the code known - no units",
         walk ? "found" : "missing", run ? "found" : "missing");
    return false;
  }
  Layout l;
  l.stride = 24;  // lea eax,[ecx+ecx*2], then [ebp+eax*8+...]: both in the pattern
  l.line_flags = mem::read<uint8_t>(walk + 34);
  l.line_head = mem::read<uint8_t>(walk + 53);
  l.unit_flags = mem::read<uint8_t>(walk + 83);
  l.unit_next = mem::read<uint8_t>(walk + 86);
  l.lines_at = mem::read<uint32_t>(run + 11);
  const uint32_t order = mem::read<uint32_t>(run + 22);
  // The lines lie inside sUnit before its count; the order table (a dword a line)
  // says how many it can hold.
  l.most = l.lines_at > l.line_head ? (l.lines_at - l.line_head) / l.stride : 0;
  if (l.most > 128) l.most = 128;
  if (mem::read<uint8_t>(walk + 65) != l.line_flags || !l.most || l.lines_at + 4 > c->size || order + 4 > c->size ||
      l.unit_flags >= 0x40 || l.unit_next >= 0x40) {
    logf("ERROR: units: sUnit's lines (flags +0x%X, first +0x%X, count +0x%X) do not fit its %u bytes - no units",
         l.line_flags, l.line_head, l.lines_at, c->size);
    return false;
  }
  // The line frame (sUnit's slot 9): a starting unit made running and started through its vtable's slot (a byte
  // offset), a going one taken off its line and destroyed through its first slot - each writing the state the walk
  // reads, the two in one function.
  const uintptr_t start = game::sig_one("kSigUnitStart"), destroy = game::sig_one("kSigUnitDestroy");
  const uint32_t start_off = start ? mem::read<uint8_t>(start + 28) : 1;
  if (start && destroy && start_off % 4 == 0 && mem::read<uint8_t>(start + 25) == l.unit_flags &&
      mem::read<uint8_t>(destroy + 13) == l.unit_flags && destroy > start && destroy < start + 0x200) {
    l.start_slot = static_cast<int>(start_off / 4);
    l.destroy_slot = 0;
  } else {
    logf("units: a unit's start and destruction (sUnit's line frame) not as known - no unit's coming and going seen");
  }
  g_layout = l;
  logf("units: lines of %u bytes (flags +0x%X, first unit +0x%X; how many in use sUnit +0x%X, room for %u); a unit's "
       "state +0x%X, next +0x%X; started through slot %d, destroyed through slot %d",
       l.stride, l.line_flags, l.line_head, l.lines_at, l.most, l.unit_flags, l.unit_next, l.start_slot,
       l.destroy_slot);
  return true;
}

const Layout& layout() { return g_layout; }

const mt::Class* class_of(uintptr_t unit) {
  const uintptr_t vt = *reinterpret_cast<const uintptr_t*>(unit);
  const auto it = g_by_vtable.find(vt);
  if (it != g_by_vtable.end()) return it->second;
  const mt::Class* c = mt::by_dti(mt::dti_of_object(unit));
  g_by_vtable[vt] = c;
  return c;
}

}  // namespace re6cc::units
