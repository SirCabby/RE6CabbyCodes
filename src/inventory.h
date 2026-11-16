#pragma once

#include "game.h"
#include "items.h"

// Each character's inventory: the items (cItem) of every character in sPlayer's array,
// as the tick reads them (main thread), and what the panel asks for - a count changed, an
// item thrown away or added - made at the pause. A change names the item it was made for
// (its slot and id): an item moved, used up or replaced since is left alone.
//
// The inventory's slots (by their numbers, mSlotNo): the weapons' (0-6), the tablet
// case's (7), the summary slots the grenades and the spray are chosen from (8-12: each
// the total of its stacks, which the game works out), and the case (13-23: ammo, herbs,
// the grenades' and the spray's stacks). Throw away and Add go through the inventory's
// own functions - a case item cleared and the summary worked out again (the game's own
// discard); an item set into a free slot as a pickup goes in (onto the stacks there are
// first) - offline only (nothing of it reaches an online partner's game).
//
// Add puts in ammo, grenades, the spray and herbs - the ammo only of a kind one of the
// character's own weapons loads (the game's own weapon-to-ammo function, transcribed in
// items.h) - and never a weapon: the Records WEAPONS window puts a character's weapons in
// its saved inventory (the user, 2026-11-14). A grenade or the spray (category 1) becomes a
// unit of its own as it goes in, made from its class's archive. One whose archive the stage
// has not loaded is loaded first, by the game's own loader, at the pause (see
// inventory.cpp, "The game's loader").
namespace re6cc::inventory {

constexpr int kMaxItems = 24;  // the most a character holds (the game's own count is checked against it)
constexpr int kModes = 3;      // the counts an item keeps (one a mode: the grenade launcher's rounds, ...)

struct Item {
  int slot = -1;  // its slot number
  int id = 0;
  int mode = 0;   // the mode in use
  int num[kModes] = {}, most[kModes] = {};
  bool in_hand = false;
  bool in_case = false;  // a case item (a stack, a herb): it can be thrown away
};

struct Case {
  int index = -1;  // in sPlayer's array
  const char* name = "";
  game::Control control = game::Control::kLocal;
  int nitems = 0;
  Item items[kMaxItems];  // by slot number; the summary slots left out (their stacks are listed)
  int nheld = 0;
  uint16_t held[kMaxItems];  // the ids it holds in use (its weapons among them: the ammo Add offers it)
};

// Whether an item can go into an inventory now: what it needs is loaded (kHere - always
// so for ammo and herbs, which get no unit), the game's own loader loads it first
// (kLoadable), or neither (kNever: the loader was not understood, or the game has no
// class for it).
enum Avail : uint8_t { kNever, kLoadable, kHere };

struct View {
  bool known = false;   // an item's fields were found in the game
  bool paused = false;  // changes can be made now
  bool can_add = false; // Add and Throw away: the inventory's own functions found, offline, at the pause
  bool online = false;  // ... not in an online session
  bool loader = false;  // the game's loader was understood: what is not loaded is loaded first
  bool loading = false; // an Add waits for the loader
  int ncases = 0;
  Case cases[game::kMaxChars];
  // The weapons, grenades and spray some character in play holds in any form (in use,
  // to be picked up, a summary).
  int ncarried = 0;
  uint16_t carried[3 * kMaxItems];
  uint8_t avail[items::kCount] = {};  // at the pause: each item of items.h (Avail)
  char note[160] = "";  // what the last Add is waiting for, or why it was not made ...
  int note_chara = -1;  // ... and whose (sPlayer's index)
};
View view();  // any thread
Avail avail(const View& v, int id);
// One of the weapons the character holds in use loads `ammo` (in one of its modes): Add
// offers it that ammo.
inline bool takes(const Case& c, int ammo) {
  for (int i = 0; i < c.nheld; ++i)
    if (items::loads(c.held[i], ammo)) return true;
  return false;
}

// Any thread: the count of `mode` of the item in `slot` of the character at `chara`
// (sPlayer's index), if it is still `id` there, set to `count` (0 .. its most) at the
// next tick while the game is paused. A character played over the network is not
// changed.
void set_count(int chara, int slot, int id, int mode, int count);
// ... the case item in `slot` thrown away (if it is still `id`, and not in hand).
void throw_away(int chara, int slot, int id);
// ... `count` of item `id` added - one Add offers (never a weapon; ammo only of a kind a
// weapon the character holds then loads). One whose archive is not loaded waits for the
// loader (at most kLoadTicks frames, and only while the pause lasts).
void add(int chara, int id, int count);
// ... the item picked in an Add list: its archive asked for now, so that its Add acts at
// once (not past kPreloadMax picked items no one holds at a time: those load on Add).
void preload(int id);
constexpr int kLoadTicks = 20 * 60;  // how long an Add waits for the loader (about 20 seconds)
constexpr int kPreloadMax = 4;

void init();                      // from DllMain: the lock
void install();                   // from boot::started(), after discovery: the inventory's own functions
void tick(const game::Run& run);  // main thread
// Main thread, after the game's frame (`main` its sBioMain): while the game stays paused
// and an archive the mod asked for is not in yet, the loader's own frame - which the
// game's paused frame leaves out.
void after_frame(void* main);

}  // namespace re6cc::inventory
