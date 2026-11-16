#pragma once

// Hooks in the game's vtables (its MT Framework classes, known by their records): a slot of every vtable of the
// classes deriving from a base re-pointed at a function of the mod's, what each vtable held there kept for the hook
// to call on to. A table is written only where it is a vtable as far as the slot (every entry before it a function
// of the game's, or a hook of the mod's in place of one): data the class index took for a vtable is never touched
// (zlib's inflate table holds a getDTI's address - hooking it broke every start, 2026-11-06).
#include <cstdint>
#include <vector>

#include "game.h"
#include "image.h"
#include "mem.h"
#include "mt.h"

namespace re6cc::vthook {

struct Hooked {
  uintptr_t vt = 0;     // the vtable
  uintptr_t entry = 0;  // its slot, re-pointed at the hook
  uintptr_t orig = 0;   // what the slot held
  uintptr_t hook = 0;
};

// What `self`'s own vtable held in the hooked slot (the fallback when its vtable is not among them).
inline uintptr_t original(const std::vector<Hooked>& v, const void* self, uintptr_t fallback) {
  const uintptr_t vt = *static_cast<const uintptr_t*>(self);
  for (const Hooked& h : v)
    if (h.vt == vt) return h.orig;
  return fallback;
}

// The mod's functions put into vtables (by any of its modules): one of them in a slot is a hook in place of a
// function of the game's.
inline std::vector<uintptr_t>& hooks() {
  static std::vector<uintptr_t> v;
  return v;
}
inline void note(const void* fn) {
  const auto f = reinterpret_cast<uintptr_t>(fn);
  for (const uintptr_t h : hooks())
    if (h == f) return;
  hooks().push_back(f);
}
inline bool ours(uintptr_t v) {
  for (const uintptr_t h : hooks())
    if (h == v) return true;
  return false;
}

// Whether the function at `fn` is `generic`, or calls it within its first 0x40 bytes (a class's own that calls the
// game's first).
inline bool is_or_calls(uintptr_t fn, uintptr_t generic) {
  if (fn == generic) return true;
  if (!image::text().contains(fn) || !mem::readable(fn, 0x40)) return false;
  for (uintptr_t a = fn; a < fn + 0x40; ++a)
    if (game::call_target(a) == generic) return true;
  return false;
}

// Whether the table at `vt` is a vtable reaching `slot`: every entry up to it a function (the game's, or a hook of the
// mod's in place of one) - and its first five whatever the slot (MtObject's own, getDTI the fifth), so a hook of a
// low slot (a destructor's, 0) asks as much of a table as the class index does: zlib's table, which holds uEm6527's
// getDTI, has a first entry (0x690800) inside the code, its second not.
inline bool reaches(uintptr_t vt, int slot) {
  for (int i = 0; i <= (slot > 4 ? slot : 4); ++i) {
    uintptr_t v = 0;
    if (!mem::read_safe(vt + 4 * i, &v) || !(image::text().contains(v) || ours(v))) return false;
  }
  return true;
}

// Each vtable of a class deriving from `base` (or of `base` alone, with `only`) whose `slot` holds the game's
// `generic` (or, with `overrides`, a class's own that calls it first; with no `generic`, whatever function of the
// game's each class has there) gets `hook` there; what it held is kept for the hook.
inline int hook(const mt::Class* base, int slot, uintptr_t generic, bool overrides, void* fn, std::vector<Hooked>* out,
                bool only = false) {
  if (!base || slot < 0) return 0;
  note(fn);
  int count = 0;
  const mt::Class* all = mt::classes(&count);
  int n = 0;
  for (int i = 0; i < count; ++i) {
    const mt::Class& c = all[i];
    if (only ? c.dti != base->dti : !mt::derives(c.dti, base->dti)) continue;
    for (uintptr_t vt : mt::vtables_of(&c)) {
      if (!reaches(vt, slot)) continue;
      const uintptr_t entry = vt + 4 * slot;
      const uintptr_t was = mem::read<uintptr_t>(entry);
      if (!generic ? !image::text().contains(was) : overrides ? !is_or_calls(was, generic) : was != generic) continue;
      bool seen = false;
      for (const Hooked& h : *out) seen = seen || h.entry == entry;
      if (seen || !mem::write<uintptr_t>(entry, reinterpret_cast<uintptr_t>(fn))) continue;
      out->push_back(Hooked{vt, entry, was, reinterpret_cast<uintptr_t>(fn)});
      ++n;
    }
  }
  return n;
}

// Each hooked slot given back what it held - while it still holds the hook (one another tool put in since is its).
inline void unhook(std::vector<Hooked>& v) {
  for (const Hooked& h : v)
    if (h.entry && mem::readable(h.entry, 4) && mem::read<uintptr_t>(h.entry) == h.hook)
      mem::write<uintptr_t>(h.entry, h.orig);
  v.clear();
}

// The slot of `vt` that holds `fn` (from slot 0, at most `most`), or -1.
inline int slot_of(uintptr_t vt, uintptr_t fn, int most) {
  for (int i = 0; i <= most && vt; ++i) {
    uintptr_t v = 0;
    if (!mem::read_safe(vt + 4 * i, &v) || !(image::text().contains(v) || ours(v))) return -1;
    if (v == fn) return i;
  }
  return -1;
}

}  // namespace re6cc::vthook
