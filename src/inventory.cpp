#include "inventory.h"

#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "image.h"
#include "items.h"
#include "log.h"
#include "mem.h"
#include "mt.h"

namespace re6cc::inventory {
namespace {

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
View g_view;

enum class Op { kCount, kThrow, kAdd, kPreload };
struct Change {
  Op op;
  int chara, slot, id, mode, count;
};
std::vector<Change> g_changes;

struct Lock {
  Lock() { EnterCriticalSection(&g_lock); }
  ~Lock() { LeaveCriticalSection(&g_lock); }
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;
};

// --- the inventory's own functions -----------------------------------------------------------
// set-slot (thiscall on the player: slot or -1 for one it picks, id, count (-1: a weapon's
// own, loaded), mode, unit (0: one made for an item that has one); the slot back, or -1),
// the top-up of the stacks it has of an item (id, count; what did not fit back), the
// refresh of the summary slots (the grenades' and the spray's totals), cItem's clear (its
// unit let go, its fields zeroed).
using SetSlotFn = int(__thiscall*)(void*, int, int, int, int, void*);
using TopUpFn = int(__thiscall*)(void*, int, int);
using RefreshFn = void(__thiscall*)(void*);
using ClearFn = void(__thiscall*)(void*);
SetSlotFn g_set_slot = nullptr;
TopUpFn g_top_up = nullptr;
RefreshFn g_refresh = nullptr;
ClearFn g_clear = nullptr;
int g_first_summary = -1, g_first_case = -1;  // slot numbers: the summary slots, then the case's

// --- the game's loader -----------------------------------------------------------------------
// set-slot makes a category-1 item (a weapon, a grenade, the spray) a unit of its own:
// the class keyed (7 << 16 | id) << 8 (kSigItemMake) - uWp1010 for the 909, uWp1100 for
// the crossbow, uWp1173 for the spray - whose model, its motions (the players' shared
// skeleton's, pl0000, for that weapon: any character can hold any weapon), sounds and
// effects all come from that class's archive, nativePC\arc\DX9\uWp1010.arc. The game
// loads an archive only for what a stage needs: what the characters carry as it loads
// (the characters' list, kSigCharsLoad), what lies in it, its enemies... A unit made
// without its archive looks for loose files that are not there, and the game quits
// ("Failed open file"). So an item goes in only once its archive is loaded - and one that
// is not, the game's own loader (sLoadMan) is asked for first, as a stage's load asks for
// what the characters carry (the user's word, 2026-11-05: "You should fix the game to be
// able to handle these adds instead"): its request (kSigLoadRequest, which asks for what
// the class needs with it too: a launcher's rounds, the crossbow's arrows, a grenade's
// thrown form) into the characters' list, then its commit, which sets the list's job.
// That list is let go only at the next stage's load, which asks again for what the
// characters carry then - an added grenade among it, as a pickup is. The loader's frame
// does not run while the game is paused (sBioMain's paused frame leaves it out): at the
// pause the mod runs it, after the game's frame, until what it asked for is in.
struct Loader {
  uint32_t current = 0;                       // sLoadMan: which list the loader adds to (a dword)
  uint32_t count = 0, items = 0, stride = 0;  // list k's count and items: these + k * stride
  int lists = 0;
  uint32_t path = 0, chars = 0;      // an entry's path (an MtString*; its characters this far into it)
  uint32_t archive = 0, state = 0;   // an entry's archive (an rArchive) and its state
  uint32_t flags = 0, failed = 0;    // the archive's flags: bit 0 loaded, `failed` failed
  int chars_list = -1;               // the characters' list: the one the mod asks into
  uint32_t kind = 0;                 // the kind of an item's class key
  std::string dir;                   // the archives' folder, "arc\DX9\"
} g_ld;
// An entry's state (the commit, the job): 1 or 4 asked for, 2 loading, 3 loaded; 0 once
// its list is chosen again, until it is asked for again - a commit then lets it go.
constexpr uint32_t kAgain = 0, kLoadedState = 3;
using RequestFn = void(__thiscall*)(void* lm, int kind, int number, int variant, int flag);
using CommitFn = int(__thiscall*)(void* lm);
using FrameFn = void(__thiscall*)(void* lm);
RequestFn g_request = nullptr;
CommitFn g_commit = nullptr;
FrameFn g_frame = nullptr;
const mt::Class* g_cls_loadman = nullptr;
const mt::Class* g_cls_item = nullptr;

// Main thread: the archives loaded now (their classes' names, sorted), as last read.
std::vector<std::string> g_here;
int g_here_age = -1;  // ticks since g_here was read (-1: not at this pause)
// Ids the loader was asked for at this pause, not all in yet: the loader's frame runs
// after the game's while any is (at most kLoadTicks frames from the last ask).
std::vector<int> g_asked;
int g_drive = 0;
// Adds waiting for the loader, made once their archives are in.
struct Waiting {
  int chara = 0, id = 0, count = 0, age = 0;
  bool asked = false;
};
std::vector<Waiting> g_waiting;
// Ids preloaded as they were picked (this game): past kPreloadMax that no one holds,
// a pick loads nothing (its Add does).
std::vector<int> g_preloaded;
// The weapons, grenades and spray some character in play holds in any form (main thread,
// the last tick's): with the loader not understood, the only ones known to be loaded.
std::vector<int> g_carried;
char g_note[160] = "";
int g_note_chara = -1;

uintptr_t item_at(uintptr_t chara, int i) {
  const game::Layout& l = game::layout();
  return chara + l.inventory + static_cast<uint32_t>(i) * l.item_size;
}

int nitems() {
  const game::Layout& l = game::layout();
  return l.nitems < kMaxItems ? l.nitems : kMaxItems;
}

bool summary_slot(int slot) { return g_first_summary >= 0 && slot >= g_first_summary && slot < g_first_case; }
bool case_slot(int slot) { return g_first_case >= 0 && slot >= g_first_case; }

void note_id(uint16_t* ids, int* n, int cap, int id) {
  for (int i = 0; i < *n; ++i)
    if (ids[i] == id) return;
  if (*n < cap) ids[(*n)++] = static_cast<uint16_t>(id);
}

// The note under a character's Add: what its last Add waits for, or why it was not made.
void note(int chara, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(g_note, sizeof(g_note), fmt, ap);
  va_end(ap);
  g_note_chara = chara;
}

const char* name_of(int id) {
  const items::Info* i = items::find(id);
  return i ? i->name : "?";
}

// The loader's own checks of its lists' fields, read out of its code: the request, the
// commit and its job, the frame, the add of an entry and its job's poll, the choice of a
// list; the characters' load (the list it chooses, the kind it asks their items by) and
// set-slot's make (the kind it makes an item by). Each where the others say.
bool discover_loader() {
  const uintptr_t request = game::sig_one("kSigLoadRequest"), commit = game::sig_one("kSigLoadCommit"),
                  job = game::sig_one("kSigLoadJob"), frame = game::sig_one("kSigLoadFrame"),
                  add = game::sig_one("kSigLoadAdd"), poll = game::sig_one("kSigLoadPoll"),
                  choose = game::sig_one("kSigLoadChoose"), chars = game::sig_one("kSigCharsLoad"),
                  chars_items = game::sig_one("kSigCharsItems"), make = game::sig_one("kSigItemMake"),
                  set = game::sig_one("kSigItemSetSlot");
  g_cls_loadman = mt::find("sLoadMan");
  g_cls_item = mt::find("uItem");
  if (!request || !commit || !job || !frame || !add || !poll || !choose || !chars || !chars_items || !make || !set ||
      !g_cls_loadman || !g_cls_loadman->vtable || !g_cls_item)
    return false;
  Loader ld;
  ld.current = mem::read<uint32_t>(commit + 7);
  ld.count = mem::read<uint32_t>(commit + 29);
  ld.items = mem::read<uint32_t>(commit + 50);
  ld.stride = 5 * 4;  // `lea eax, [eax + eax * 4]` then `[edi + eax * 4]`: list k's MtArray k * 20 bytes on
  ld.lists = mem::read<uint8_t>(add + 39);
  ld.state = mem::read<uint8_t>(commit + 59);
  ld.archive = mem::read<uint8_t>(commit + 81);
  ld.path = mem::read<uint8_t>(add + 71);
  ld.chars = mem::read<uint8_t>(add + 81);
  ld.flags = mem::read<uint8_t>(poll + 9);
  const uint32_t shift = mem::read<uint8_t>(poll + 12);
  ld.failed = shift < 32 ? 1u << shift : 0;
  ld.chars_list = mem::read<int8_t>(chars + 14);
  ld.kind = mem::read<uint8_t>(chars_items + 36);
  const uintptr_t prefix = mem::read<uintptr_t>(add + 93);
  const char* dir = image::rdata().contains(prefix) ? reinterpret_cast<const char*>(prefix) : nullptr;
  const size_t dlen = dir ? strnlen(dir, 32) : 0;
  // The fields as each function has them; the lists one after another (an MtArray's count
  // +4, its items +0x10), up to the word that says which one is added to; the loader's
  // frame sLoadMan's own (vtable slot 6) and its jobs the commit's; the characters' load
  // choosing its list with the choice and asking with the request; set-slot making an
  // item by the kind the characters' load asks an item for by.
  const uint32_t first = ld.count - 4;
  if (mem::read<uint32_t>(job + 6) != ld.current || mem::read<uint32_t>(choose + 6) != ld.current ||
      mem::read<uint32_t>(choose + 34) != ld.current || mem::read<uint32_t>(choose + 18) != ld.count ||
      mem::read<uint32_t>(choose + 44) != ld.items || mem::read<uint8_t>(choose + 53) != ld.state ||
      ld.items != ld.count + 0xC || ld.lists < 1 || ld.lists > 32 ||
      ld.current != first + static_cast<uint32_t>(ld.lists) * ld.stride || ld.current + 4 > g_cls_loadman->size ||
      mem::read<uint32_t>(frame + 50) != mem::read<uint32_t>(job + 16) ||
      mem::read<uint8_t>(frame + 63) != mem::read<uint8_t>(job + 12) ||
      mem::read<uint8_t>(frame + 56) != mem::read<uint8_t>(job + 28) ||
      mem::read<uintptr_t>(g_cls_loadman->vtable + 6 * 4) != frame || job < commit || job > commit + 0x120 ||
      mem::read<uint8_t>(poll + 2) != ld.archive || mem::read<uint8_t>(poll + 24) != ld.archive ||
      mem::read<uint8_t>(poll + 31) != ld.path || mem::read<uint8_t>(poll + 46) != ld.chars ||
      mem::read<uint8_t>(poll + 86) != ld.flags || mem::read<uint8_t>(poll + 92) != ld.state ||
      mem::read<uint8_t>(add + 78) != ld.path || !ld.failed || ld.failed == 1 ||
      ld.path == ld.archive || ld.path == ld.state || ld.archive == ld.state ||
      std::max({ld.path, ld.archive, ld.state}) + 4 > mem::read<uint8_t>(add + 44) ||
      game::call_target(chars + 15) != choose || game::call_target(chars + 51) != request ||
      game::call_target(chars_items + 37) != request || mem::read<uint32_t>(chars + 9) != mem::read<uint32_t>(chars + 39) ||
      mem::read<uint32_t>(chars_items + 26) != mem::read<uint32_t>(chars + 9) || chars_items < chars ||
      chars_items > chars + 0x400 || ld.chars_list < 0 || ld.chars_list >= ld.lists ||
      mem::read<uint8_t>(make + 29) != ld.kind || make < set || make > set + 0x100 || !dir || dlen < 2 || dlen >= 32)
    return false;
  ld.dir.assign(dir, dlen);
  g_ld = ld;
  g_request = reinterpret_cast<RequestFn>(request);
  g_commit = reinterpret_cast<CommitFn>(commit);
  g_frame = reinterpret_cast<FrameFn>(frame);
  return true;
}

uintptr_t loadman() {
  const uintptr_t lm = g_frame ? game::system("sLoadMan") : 0;
  return lm && mt::is_a(lm, g_cls_loadman) ? lm : 0;
}

// The class set-slot makes an item's unit of (one of uItem's), or nullptr: a category-1
// item's only (the others get no unit).
const mt::Class* item_class(int id) {
  if ((id >> 8) != 1 || !g_cls_item || !g_ld.kind) return nullptr;
  const mt::Class* c = mt::by_id(((g_ld.kind << 16) | (static_cast<uint32_t>(id) & 0xFFFF)) << 8);
  return c && mt::derives(c->dti, g_cls_item->dti) ? c : nullptr;
}

// Each entry of the loader's lists - only list `only`'s when it is >= 0 - with its state.
template <typename Each>
void entries(uintptr_t lm, int only, Each&& each) {
  const uint32_t need = std::max({g_ld.path, g_ld.archive, g_ld.state}) + 4;
  for (int k = 0; k < g_ld.lists; ++k) {
    if (only >= 0 && k != only) continue;
    const uint32_t at = static_cast<uint32_t>(k) * g_ld.stride;
    const int n = mem::read<int32_t>(lm + g_ld.count + at);
    const uintptr_t items = mem::read<uintptr_t>(lm + g_ld.items + at);
    if (n <= 0 || n > 0x4000 || !mem::readable(items, 4u * static_cast<uint32_t>(n))) continue;
    for (int i = 0; i < n; ++i) {
      const uintptr_t e = mem::read<uintptr_t>(items + 4u * static_cast<uint32_t>(i));
      if (e && mem::readable(e, need)) each(e, mem::read<uint32_t>(e + g_ld.state));
    }
  }
}

// A string's length (below `max`) in readable memory, or -1 - read a page at a time, so
// a string near the end of a heap block is not taken for unreadable.
int cstr_len(uintptr_t p, size_t max) {
  size_t n = 0;
  while (n < max) {
    const uintptr_t at = p + n;
    const size_t chunk = std::min<size_t>(0x1000 - (at & 0xFFF), max - n);
    if (!mem::readable(at, chunk)) return -1;
    const size_t k = strnlen(reinterpret_cast<const char*>(at), chunk);
    if (k < chunk) return static_cast<int>(n + k);
    n += chunk;
  }
  return -1;
}

// The class an entry's archive is for - its path is "<folder><class>" - or nullptr.
const char* entry_class(uintptr_t e) {
  const uintptr_t s = mem::read<uintptr_t>(e + g_ld.path);
  const int dir = static_cast<int>(g_ld.dir.size());
  const int len = s ? cstr_len(s + g_ld.chars, 128) : -1;
  const char* p = reinterpret_cast<const char*>(s + g_ld.chars);
  return len > dir && std::strncmp(p, g_ld.dir.c_str(), static_cast<size_t>(dir)) == 0 ? p + dir : nullptr;
}

bool entry_loaded(uintptr_t e) {
  const uintptr_t arc = mem::read<uintptr_t>(e + g_ld.archive);
  if (!arc || !mem::readable(arc + g_ld.flags, 4)) return false;
  const uint32_t f = mem::read<uint32_t>(arc + g_ld.flags);
  return (f & 1) && !(f & g_ld.failed);
}

// The archives loaded now, by class name.
void read_here() {
  g_here.clear();
  g_here_age = 0;
  const uintptr_t lm = loadman();
  if (!lm) return;
  entries(lm, -1, [&](uintptr_t e, uint32_t) {
    const char* name = entry_class(e);
    if (name && entry_loaded(e)) g_here.emplace_back(name);
  });
  std::sort(g_here.begin(), g_here.end());
}

bool here_name(const char* name) { return std::binary_search(g_here.begin(), g_here.end(), std::string(name)); }

// What the item needs is loaded (as last read): an item with no unit needs nothing. With
// the loader not understood, what someone in play holds in any form (the stage's load
// asked for it).
bool here(int id) {
  if ((id >> 8) != 1) return true;
  if (!g_frame) return std::find(g_carried.begin(), g_carried.end(), id) != g_carried.end();
  const mt::Class* c = item_class(id);
  return c && here_name(c->name);
}

// Some list's entry still waits for the loader (asked for, or loading) - one of the
// mod's, or a companion of one asked for with it.
bool loading_any(uintptr_t lm) {
  bool any = false;
  entries(lm, -1, [&](uintptr_t, uint32_t s) { any = any || (s != kAgain && s != kLoadedState); });
  return any;
}

// Ready to go in: loaded, and nothing asked for with it still loading.
bool ready(int id) {
  if (!here(id)) return false;
  const uintptr_t lm = loadman();
  return (id >> 8) != 1 || !lm || !loading_any(lm);
}

// The loader asked for these items (each one's class, and what the class needs with it)
// into the characters' list - whichever list the game chose last is put back after - and
// one commit; never while a list is being chosen (an entry in state 0: a commit would let
// it go). True when each is loaded or now asked for (a list holds its archive).
bool load(const std::vector<int>& ids) {
  std::vector<int> ask;
  for (const int id : ids)
    if (!here(id) && std::find(ask.begin(), ask.end(), id) == ask.end()) ask.push_back(id);
  if (ask.empty()) return true;
  const uintptr_t lm = loadman();
  if (!g_request || !g_commit || !lm) return false;
  for (const int id : ask)
    if (!item_class(id)) return false;
  auto* cur = reinterpret_cast<uint32_t*>(lm + g_ld.current);  // a field of the game's heap object
  bool settled = *cur < static_cast<uint32_t>(g_ld.lists);
  if (settled) entries(lm, -1, [&](uintptr_t, uint32_t s) { settled = settled && s != kAgain; });
  if (!settled) {
    logf("inventory: the loader is choosing a list (list %u) - asked again next frame", *cur);
    return false;
  }
  std::vector<std::string> before;
  entries(lm, g_ld.chars_list, [&](uintptr_t e, uint32_t) {
    if (const char* n = entry_class(e)) before.emplace_back(n);
  });
  std::sort(before.begin(), before.end());
  const uint32_t was = *cur;
  *cur = static_cast<uint32_t>(g_ld.chars_list);
  for (const int id : ask) g_request(reinterpret_cast<void*>(lm), static_cast<int>(g_ld.kind), id, 0, 0);
  const int waiting = g_commit(reinterpret_cast<void*>(lm));
  *cur = was;
  std::string added, names;
  entries(lm, g_ld.chars_list, [&](uintptr_t e, uint32_t) {
    const char* n = entry_class(e);
    if (n && !std::binary_search(before.begin(), before.end(), std::string(n))) {
      added += added.empty() ? "" : " ";
      added += n;
    }
  });
  // Each asked for: a list holds its archive now (the request makes nothing of a class it
  // does not load).
  bool all = true;
  for (const int id : ask) {
    const mt::Class* c = item_class(id);
    bool held = false;
    entries(lm, -1, [&](uintptr_t e, uint32_t) {
      const char* n = entry_class(e);
      held = held || (n && !std::strcmp(n, c->name));
    });
    all = all && held;
    names += names.empty() ? "" : ", ";
    names += name_of(id);
    names += " (";
    names += c->name;
    names += held ? ")" : ": not taken)";
    if (held && std::find(g_asked.begin(), g_asked.end(), id) == g_asked.end()) g_asked.push_back(id);
  }
  g_drive = 0;
  logf("inventory: the game's loader asked for %s into the characters' list (%d; the list chosen last, %u, put back): "
       "%d archive(s) to load, new: %s",
       names.c_str(), g_ld.chars_list, was, waiting, added.empty() ? "none" : added.c_str());
  return all;
}

// The character's items in use, by their slot numbers (the summary slots left out), and
// what it holds in any form.
void read_case(const game::Character& c, Case* out, View* v) {
  const game::Layout& l = game::layout();
  out->index = c.index;
  out->name = c.name;
  out->control = c.control;
  out->nitems = out->nheld = 0;
  const int hand = *reinterpret_cast<const int32_t*>(c.obj + l.in_hand);
  for (int i = 0; i < nitems(); ++i) {
    const uintptr_t it = item_at(c.obj, i);
    const int id = *reinterpret_cast<const uint16_t*>(it + l.it_id);
    const int slot = *reinterpret_cast<const uint8_t*>(it + l.it_slot);
    const bool used = *reinterpret_cast<const uint8_t*>(it + l.it_used) != 0;
    if (id && (id >> 8) == 1) note_id(v->carried, &v->ncarried, static_cast<int>(sizeof(v->carried) / 2), id);
    if (!used) continue;
    note_id(out->held, &out->nheld, kMaxItems, id);
    if (summary_slot(slot)) continue;
    Item& item = out->items[out->nitems++];
    item = Item{};
    item.slot = slot;
    item.id = id;
    item.mode = *reinterpret_cast<const uint8_t*>(it + l.it_mode);
    for (int m = 0; m < kModes; ++m) {
      item.num[m] = *reinterpret_cast<const int16_t*>(it + l.it_num + 2 * m);
      item.most[m] = *reinterpret_cast<const int16_t*>(it + l.it_max + 2 * m);
    }
    item.in_hand = item.slot == hand;
    item.in_case = case_slot(slot);
  }
  // By slot number (the game's order: the weapons' slots first, then the case's).
  for (int i = 1; i < out->nitems; ++i)
    for (int j = i; j > 0 && out->items[j - 1].slot > out->items[j].slot; --j) std::swap(out->items[j - 1], out->items[j]);
}

const game::Character* chara_of(const game::Run& run, int index) {
  for (int i = 0; i < run.nchars; ++i)
    if (run.chars[i].index == index) return &run.chars[i];
  return nullptr;
}

// The cItem in `slot` (in use) if it is still `id`, else 0.
uintptr_t find_item(const game::Character& c, int slot, int id) {
  const game::Layout& l = game::layout();
  for (int i = 0; i < nitems(); ++i) {
    const uintptr_t it = item_at(c.obj, i);
    if (!*reinterpret_cast<const uint8_t*>(it + l.it_used) || *reinterpret_cast<const uint8_t*>(it + l.it_slot) != slot)
      continue;
    const int now = *reinterpret_cast<const uint16_t*>(it + l.it_id);
    if (now == id) return it;
    logf("inventory: %s's slot %d holds 0x%X now, not 0x%X - left alone", c.name, slot, now, id);
    return 0;
  }
  return 0;
}

// A count, made: the item still in its slot with the id it was typed for. A case
// item's stacks are what the summary slots total: worked out again after.
void set_count_now(const game::Character& c, const Change& ch) {
  const game::Layout& l = game::layout();
  const uintptr_t it = find_item(c, ch.slot, ch.id);
  if (!it) return;
  const int most = *reinterpret_cast<const int16_t*>(it + l.it_max + 2 * ch.mode);
  const int count = ch.count < 0 ? 0 : ch.count > most ? most : ch.count;
  auto* num = reinterpret_cast<int16_t*>(it + l.it_num + 2 * ch.mode);
  logf("inventory: %s's slot %d (0x%X) count %d -> %d", c.name, ch.slot, ch.id, *num, count);
  *num = static_cast<int16_t>(count);
  if (case_slot(ch.slot) && g_refresh) g_refresh(reinterpret_cast<void*>(c.obj));
}

// A case item thrown away as the game's own discard does it: cleared, the summary worked
// out again. Never the item in hand.
void throw_now(const game::Character& c, const Change& ch) {
  const game::Layout& l = game::layout();
  if (!g_clear || !g_refresh || !case_slot(ch.slot)) return;
  if (*reinterpret_cast<const int32_t*>(c.obj + l.in_hand) == ch.slot) return;
  const uintptr_t it = find_item(c, ch.slot, ch.id);
  if (!it) return;
  logf("inventory: %s's slot %d (0x%X, %d) thrown away", c.name, ch.slot, ch.id,
       *reinterpret_cast<const int16_t*>(it + l.it_num));
  g_clear(reinterpret_cast<void*>(it));
  g_refresh(reinterpret_cast<void*>(c.obj));
}

// A weapon the character holds in use now loads `ammo` (its live items, not a view read
// earlier in the tick).
bool takes_now(const game::Character& c, int ammo) {
  const game::Layout& l = game::layout();
  for (int i = 0; i < nitems(); ++i) {
    const uintptr_t it = item_at(c.obj, i);
    if (*reinterpret_cast<const uint8_t*>(it + l.it_used) &&
        items::loads(*reinterpret_cast<const uint16_t*>(it + l.it_id), ammo))
      return true;
  }
  return false;
}

// An item added as a pickup goes in: onto the stacks there are first (the top-up), the
// rest into free slots a slot's most at a time; the summary worked out again. Only what
// Add offers (no weapon; ammo only of a kind a weapon the character holds loads), and only
// what is ready (its archive loaded, nothing asked for with it still loading).
void add_now(const game::Character& c, int id, int count) {
  const items::Info* info = items::find(id);
  if (!info || !info->offer || info->kind == items::kWeapon || !g_set_slot || !g_top_up || !g_refresh) return;
  void* player = reinterpret_cast<void*>(c.obj);
  if (info->kind == items::kAmmo && !takes_now(c, id)) {
    note(c.index, "Nothing %s holds takes %s.", c.name, info->name);
    logf("inventory: %s not added to %s: no weapon %s holds loads it", info->name, c.name, c.name);
    return;
  }
  if (!ready(id)) {
    logf("inventory: 0x%X (%s) not added to %s: its archive is not loaded", id, info->name, c.name);
    return;
  }
  int rest = count < 1 ? 1 : count;
  if (info->most > 1) rest = g_top_up(player, id, rest);
  while (rest > 0) {
    const int n = rest < info->most ? rest : info->most;
    if (n < 1 || g_set_slot(player, -1, id, n, 0, nullptr) < 0) break;
    rest -= n;
  }
  g_refresh(player);
  if (rest > 0) note(c.index, "Only %d %s fit in %s's inventory.", count - rest, info->name, c.name);
  logf("inventory: %d %s added to %s%s", count - rest, info->name, c.name, rest > 0 ? " (the rest: no room)" : "");
}

// An Add asked for: made now when its archive is loaded, else the loader asked for it and
// the Add waits (in front of the next ones) until it is in.
void add_or_wait(const game::Character& c, const Change& ch, const View& v) {
  const items::Info* info = items::find(ch.id);
  if (!info || !info->offer || info->kind == items::kWeapon) {
    logf("inventory: 0x%X not added to %s: Add does not offer it", ch.id, c.name);
    return;
  }
  if (ready(ch.id)) {
    g_note[0] = '\0';
    add_now(c, ch.id, ch.count);
    return;
  }
  if (!v.loader || !item_class(ch.id)) {
    note(c.index, "The %s cannot be added here: the game has not loaded it (see the log).", info->name);
    logf("inventory: 0x%X (%s) not added to %s: not loaded here, and the game's loader not understood", ch.id,
         info->name, c.name);
    return;
  }
  Waiting w{ch.chara, ch.id, ch.count, 0, false};
  w.asked = here(ch.id) || load({ch.id});
  g_waiting.push_back(w);
  note(c.index, "Loading the %s...", info->name);
}

// The picked items no one holds that were preloaded (and are loaded still).
int preloaded_unheld(const View& v) {
  int n = 0;
  for (const int id : g_preloaded) {
    bool carried = false;
    for (int i = 0; i < v.ncarried; ++i) carried = carried || v.carried[i] == id;
    n += !carried && here(id);
  }
  return n;
}

}  // namespace

Avail avail(const View& v, int id) {
  for (int k = 0; k < items::kCount; ++k)
    if (items::kItems[k].id == id) return static_cast<Avail>(v.avail[k]);
  return (id >> 8) == 1 ? kNever : kHere;
}

void init() {
  InitializeCriticalSection(&g_lock);
  g_lock_ready = true;
}

void install() {
  const game::Layout& l = game::layout();
  if (!l.it_used) return;
  const uintptr_t set = game::sig_one("kSigItemSetSlot"), top = game::sig_one("kSigItemTopUp"),
                  refresh = game::sig_one("kSigItemRefresh"), clear = game::sig_one("kSigItemClear");
  const int summary = refresh ? mem::read<uint8_t>(refresh + 24) : -1, first_case = top ? mem::read<uint8_t>(top + 27) : -1;
  // Each works on the inventory the mod reads (cItem's unit inside an item), the summary
  // slots before the case's, inside the inventory.
  if (!set || !top || !refresh || !clear || mem::read<uint32_t>(set + 34) != l.inventory ||
      mem::read<uint32_t>(top + 33) != l.inventory || mem::read<uint8_t>(clear + 6) + 4u > l.item_size || summary < 1 ||
      first_case <= summary || first_case >= l.nitems) {
    logf("inventory: the inventory's own functions were not found (or disagree) - no Add or Throw away");
    return;
  }
  g_set_slot = reinterpret_cast<SetSlotFn>(set);
  g_top_up = reinterpret_cast<TopUpFn>(top);
  g_refresh = reinterpret_cast<RefreshFn>(refresh);
  g_clear = reinterpret_cast<ClearFn>(clear);
  g_first_summary = summary;
  g_first_case = first_case;
  logf("inventory: set-slot exe+0x%X, top-up exe+0x%X, refresh exe+0x%X, clear exe+0x%X; the summary slots %d-%d, the "
       "case from %d",
       static_cast<unsigned>(set - image::base()), static_cast<unsigned>(top - image::base()),
       static_cast<unsigned>(refresh - image::base()), static_cast<unsigned>(clear - image::base()), summary,
       first_case - 1, first_case);
  if (!discover_loader()) {
    g_request = nullptr;
    g_commit = nullptr;
    g_frame = nullptr;
    logf("inventory: the game's loader was not understood - a grenade or the spray goes in only when someone in the "
         "stage holds one");
    return;
  }
  int classes = 0, offered = 0;
  for (const items::Info& i : items::kItems)
    if (i.offer && (i.id >> 8) == 1) {
      ++offered;
      classes += item_class(i.id) != nullptr;
    }
  const mt::Class* c909 = item_class(0x101);
  logf("inventory: the game's loader: %d lists (counts sLoadMan +0x%X, items +0x%X, 0x%X apart; the one added to "
       "+0x%X), an entry's path +0x%X (its characters +0x%X on), archive +0x%X (its flags +0x%X: bit 0 loaded, 0x%X "
       "failed), state +0x%X; archives \"%s<class>\". The mod asks into the characters' list (%d) by kind %u - "
       "set-slot's (the 909: %s; %d of the %d grenades and spray Add offers have a class) - and runs the loader's "
       "frame (exe+0x%X) at the pause",
       g_ld.lists, g_ld.count, g_ld.items, g_ld.stride, g_ld.current, g_ld.path, g_ld.chars, g_ld.archive, g_ld.flags,
       g_ld.failed, g_ld.state, g_ld.dir.c_str(), g_ld.chars_list, g_ld.kind, c909 ? c909->name : "none", classes,
       offered, static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_frame) - image::base()));
}

View view() {
  if (!g_lock_ready) return View{};
  Lock lock;
  return g_view;
}

void set_count(int chara, int slot, int id, int mode, int count) {
  if (!g_lock_ready) return;
  Lock lock;
  g_changes.push_back(Change{Op::kCount, chara, slot, id, mode, count});
}

void throw_away(int chara, int slot, int id) {
  if (!g_lock_ready) return;
  Lock lock;
  g_changes.push_back(Change{Op::kThrow, chara, slot, id, 0, 0});
}

void add(int chara, int id, int count) {
  if (!g_lock_ready) return;
  Lock lock;
  g_changes.push_back(Change{Op::kAdd, chara, -1, id, 0, count});
}

void preload(int id) {
  if (!g_lock_ready) return;
  Lock lock;
  g_changes.push_back(Change{Op::kPreload, -1, -1, id, 0, 0});
}

void tick(const game::Run& run) {
  if (!g_lock_ready) return;
  const game::Layout& l = game::layout();
  const bool known = l.it_used && l.in_hand;
  std::vector<Change> changes;
  {
    Lock lock;
    changes.swap(g_changes);
  }
  View v;
  v.known = known;
  v.paused = run.screen == game::Screen::kPause;
  v.online = run.online;
  for (int i = 0; i < run.nchars; ++i) v.online = v.online || run.chars[i].control == game::Control::kNetwork;
  v.can_add = g_set_slot && v.paused && !v.online;
  v.loader = g_frame != nullptr;
  if (!run.in_game) g_preloaded.clear();
  // The characters' items: read only while the panel can show them (the changes are made at the pause, when it is up).
  if (known && run.panel)
    for (int i = 0; i < run.nchars && v.ncases < game::kMaxChars; ++i) read_case(run.chars[i], &v.cases[v.ncases++], &v);
  if (run.panel) g_carried.assign(v.carried, v.carried + v.ncarried);
  // What the stage has loaded: read as the pause opens, now and then while it is up, and
  // every frame while something asked for is still loading or a change is made.
  if (!v.paused) {
    g_here_age = -1;
    g_asked.clear();
    for (const Waiting& w : g_waiting)
      note(w.chara, "The %s was not added: the pause closed while it was loading.", name_of(w.id));
    g_waiting.clear();
  } else if (g_here_age < 0 || ++g_here_age >= 30 || !changes.empty() || !g_asked.empty() || !g_waiting.empty()) {
    read_here();
  }
  if (v.paused && !g_asked.empty()) {
    g_asked.erase(std::remove_if(g_asked.begin(), g_asked.end(), [](int id) { return ready(id); }), g_asked.end());
    if (g_asked.empty()) g_drive = 0;
  }
  // Changes are made at the pause only (the world stands still); one asked for at
  // another moment belongs to a showing that has gone.
  bool changed = false;
  if (known && v.paused && (!changes.empty() || !g_waiting.empty())) {
    // The Adds waiting for the loader first: made once in, dropped after kLoadTicks.
    std::vector<Waiting> waiting;
    waiting.swap(g_waiting);
    for (Waiting& w : waiting) {
      const game::Character* c = chara_of(run, w.chara);
      if (!c || c->control == game::Control::kNetwork || !v.can_add) continue;
      if (ready(w.id)) {
        logf("inventory: the %s loaded after %d frame(s)", name_of(w.id), w.age);
        g_note[0] = '\0';
        add_now(*c, w.id, w.count);
        changed = true;
      } else if (++w.age > kLoadTicks) {
        note(w.chara, "The %s was not added: the game did not load it (see the log).", name_of(w.id));
        logf("inventory: the %s not loaded after %d frames - not added", name_of(w.id), w.age);
      } else {
        if (!w.asked) w.asked = load({w.id});
        g_waiting.push_back(w);
      }
    }
    for (const Change& ch : changes) {
      if (ch.op == Op::kPreload) {
        // An item picked in an Add list: loaded while it waits to be added - not past
        // kPreloadMax picked items no one holds, and nothing Add does not offer.
        const items::Info* info = items::find(ch.id);
        if (!v.can_add || !info || !info->offer || here(ch.id) || !item_class(ch.id) ||
            std::find(g_preloaded.begin(), g_preloaded.end(), ch.id) != g_preloaded.end())
          continue;
        if (preloaded_unheld(v) >= kPreloadMax) {
          logf("inventory: %d picked items no one holds are loaded - the %s loads when it is added", kPreloadMax,
               name_of(ch.id));
          continue;
        }
        g_preloaded.push_back(ch.id);
        load({ch.id});
        continue;
      }
      const game::Character* c = chara_of(run, ch.chara);
      if (!c || c->control == game::Control::kNetwork) continue;
      if (ch.op == Op::kCount && ch.mode >= 0 && ch.mode < kModes) set_count_now(*c, ch);
      else if (ch.op == Op::kThrow && v.can_add) throw_now(*c, ch);
      else if (ch.op == Op::kAdd && v.can_add) add_or_wait(*c, ch, v);
      changed = true;
    }
  }
  if (changed) {
    // What the panel shows: as the changes left it.
    View after;
    after.known = v.known;
    after.paused = v.paused;
    after.online = v.online;
    after.can_add = v.can_add;
    after.loader = v.loader;
    for (int i = 0; i < run.nchars && after.ncases < game::kMaxChars; ++i)
      read_case(run.chars[i], &after.cases[after.ncases++], &after);
    v = after;
  }
  if (v.paused)
    for (int k = 0; k < items::kCount; ++k) {
      const int id = items::kItems[k].id;
      v.avail[k] = here(id) ? kHere : v.loader && item_class(id) ? kLoadable : kNever;
    }
  v.loading = !g_waiting.empty();
  std::snprintf(v.note, sizeof(v.note), "%s", g_note);
  v.note_chara = g_note_chara;
  Lock lock;
  g_view = v;
}

void after_frame(void* main) {
  if (!g_frame || (g_asked.empty() && g_waiting.empty())) return;
  const game::Layout& l = game::layout();
  if (!main || !l.paused || !*reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(main) + l.paused)) return;
  const uintptr_t lm = loadman();
  if (!lm || ++g_drive > kLoadTicks) return;
  g_frame(reinterpret_cast<void*>(lm));
}

}  // namespace re6cc::inventory
