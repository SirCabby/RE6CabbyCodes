#include "cheats.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#include "config.h"
#include "image.h"
#include "log.h"
#include "mem.h"
#include "merc.h"
#include "mt.h"
#include "units.h"
#include "vthook.h"

namespace re6cc::cheats {
namespace {

volatile LONG g_on[sw::kCount] = {};
volatile LONG g_dirty = 0;  // a switch changed: the tick writes the ini
bool g_ok[sw::kCount] = {};
const char* g_why[sw::kCount] = {};

const mt::Class* g_cls_player = nullptr;  // uPlayer: a hit dealt by one of these is a player's

// --- the events the switches act on ---------------------------------------------------------------
// The tick is the mod's eye on the game: each frame it reads a few words (the pause, sPlayer's characters) and hands
// on what changed - a switch turned on or off, a character come into play (a new one, or the same one driven by
// another: this machine, the AI, the network). The cheats act on those, and in the game's own calls (the hooks and
// re-pointed calls); none of them writes into the game every frame.
uint32_t g_was_on = 0;  // the switches on (and found) as the last tick saw them, a bit each
struct Seen {
  uintptr_t obj = 0;
  uint32_t control = 0;
};
Seen g_seen[game::kMaxChars];
int g_nseen = 0;

// --- the damage hooks ---------------------------------------------------------------------
// Each hook goes into every vtable of a class deriving from uPlayer (or uEnemy)
// whose slot holds the game's function for it, and calls on to what the object's
// own vtable held there (the classes that override the slot keep their own).
using vthook::Hooked;
using vthook::original;
std::vector<Hooked> g_sub, g_hit, g_apply, g_eapply, g_ework, g_add, g_reset, g_airadd, g_spend;
uintptr_t g_sub_fn = 0, g_hit_fn = 0, g_apply_fn = 0, g_eapply_fn = 0, g_ework_fn = 0;  // the game's own (each slot's usual)
uintptr_t g_add_fn = 0, g_reset_fn = 0, g_airadd_fn = 0;  // uPlayer's health add, reset, combat gauge add
int g_add_slot = -1, g_reset_slot = -1, g_airadd_slot = -1;  // their slots (where uPlayer's vtable holds them)
uintptr_t g_spend_fn = 0;  // uPlPlayer's spend of combat gauge blocks ...
int g_spend_slot = -1;     // ... and its slot (uPlPlayer's vtable's)

// A player who is to take no damage: driven here with God mode on, or by the AI
// with Partner takes no damage on. Another player's character over the network is
// left to its own game.
bool spared(const void* self) {
  const auto c = *reinterpret_cast<const uint32_t*>(reinterpret_cast<uintptr_t>(self) + game::layout().control);
  return (c == static_cast<uint32_t>(game::Control::kLocal) && g_on[sw::kGodMode]) ||
         (c == static_cast<uint32_t>(game::Control::kAi) && g_on[sw::kPartnerGod]);
}

using SubFn = void(__thiscall*)(void*, int);
using AddFn = void(__thiscall*)(void*, int);
using HitFn = void(__thiscall*)(void*, void*);
using ApplyFn = char(__thiscall*)(void*, int, int, int, int, int, int);
using EnemyApplyFn = void(__thiscall*)(void*, void*, int);

// Health taken off a player (subtract-health, slot 44: what every hit's and every
// scripted blow's damage comes to, through the take-damage below or straight): a
// spared one keeps it.
void __thiscall hk_player_sub(void* self, int amount) {
  if (static_cast<int16_t>(amount) > 0 && spared(self)) return;
  reinterpret_cast<SubFn>(original(g_sub, self, g_sub_fn))(self, amount);
}

// Health added to a player (uPlayer's add: a heal, getting up from dying - any change the game makes as an add, a
// word): a spared one loses none, and any it is given fills it up.
void __thiscall hk_player_add(void* self, int amount) {
  const auto orig = reinterpret_cast<AddFn>(original(g_add, self, g_add_fn));
  const int16_t a = static_cast<int16_t>(amount);
  if (a == 0 || !spared(self)) {
    orig(self, amount);
    return;
  }
  if (a < 0) return;
  const auto p = reinterpret_cast<uintptr_t>(self);
  const game::Layout& lay = game::layout();
  const int most = mem::read<int16_t>(p + lay.hp_max), hp = mem::read<int16_t>(p + lay.hp);
  orig(self, most > hp ? most - hp : amount);
}

// A player's take-damage (slot 121): what the hits and the enemies' grabs and blows
// call to take health - and, for a blow that downs a player at once, to put them
// into the dying state straight away. A spared player is let off as the game lets
// off a partner the AI drives in a campaign: nothing taken, not down.
char __thiscall hk_player_apply(void* self, int a, int b, int c, int d, int e, int f) {
  if (spared(self)) return 0;
  return reinterpret_cast<ApplyFn>(original(g_apply, self, g_apply_fn))(self, a, b, c, d, e, f);
}

// A hit on a player (slot 117) works out the reaction from what the hit would leave
// - a hit that would down them makes them fall. On a spared player it is played as a
// hit that does no damage: its damage is 0 while the game's own handling runs (the
// flinch plays), and put back after, as the hit may be another's too.
void __thiscall hk_player_hit(void* self, void* hit) {
  const auto orig = reinterpret_cast<HitFn>(original(g_hit, self, g_hit_fn));
  if (!hit || !spared(self)) {
    orig(self, hit);
    return;
  }
  auto* damage = reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(hit) + game::layout().hit_damage);
  const int32_t was = *damage;
  *damage = 0;
  orig(self, hit);
  *damage = was;
}

// --- God mode on a ride: the vehicle a spared character rides takes no damage ------------------------
// The rides (uSmRide's: Ada's helicopter and Chris's jet - uSmPlHarrier -, the players' bike, jeep and snowmobile, the
// enemies' bikes and jeeps) are set models, not characters: a hit on one reaches its own subtract-health (vtable slot
// 44, uSmRide's - the players' bike's and jeep's own call it after their rules for a computer rider), which scales the
// damage by its first rider's level when that is a player and takes it off the ride's health; at none it is destroyed
// (Ada's helicopter crashes, and Ada with it). A ride has two riders, in the order they got on. One a spared character
// rides (yours with God mode, the computer's with Partner takes no damage) takes nothing; the enemies' rides, and a
// ride with no spared rider, take their damage as the game has it. As either switch goes on, a ride in play with a
// spared rider has its health filled up once (above 0 only: one the game has destroyed is left to it).
struct RideLayout {
  uint32_t rider = 0;  // uSmRide: its first rider (a unit), the second right after it
};
RideLayout g_ride;
std::vector<Hooked> g_ride_sub;
uintptr_t g_ride_sub_fn = 0;  // uSmRide's subtract-health
const mt::Class* g_cls_ride = nullptr;
bool g_rides_ok = false;

bool ride_spared(uintptr_t ride) {
  for (int i = 0; i < 2; ++i) {
    uintptr_t r = 0;
    if (mem::read_safe(ride + g_ride.rider + 4 * i, &r) && r && mt::is_a(r, g_cls_player) &&
        spared(reinterpret_cast<const void*>(r)))
      return true;
  }
  return false;
}

// Health taken off a ride (slot 44, each class's own): a ride with a spared rider keeps it.
void __thiscall hk_ride_sub(void* self, int amount) {
  if (static_cast<int16_t>(amount) > 0 && ride_spared(reinterpret_cast<uintptr_t>(self))) return;
  reinterpret_cast<SubFn>(original(g_ride_sub, self, g_ride_sub_fn))(self, amount);
}

// uSmRide's subtract-health (its own signature), cross-checked: in uSmRide's own vtable's slot 44, reading its first
// rider where the ride-on fills the first seat (the second right after it), and taking from the characters' health
// words; hooked in every ride class's slot 44, whatever each has there (the bike's and the jeep's own call it).
bool install_rides(const game::Layout& lay) {
  const uintptr_t sub = game::sig_one("kSigRideSubLife"), on = game::sig_one("kSigRideOn");
  const mt::Class* ride = mt::find("uSmRide");
  if (!sub || !on || !ride || !ride->vtable || !g_cls_player || !lay.hp) return false;
  const uint32_t rider = mem::read<uint32_t>(sub + 13);
  if (mem::read<uintptr_t>(ride->vtable + 4 * game::kSubLifeSlot) != sub || mem::read<uint32_t>(sub + 72) != rider ||
      mem::read<uint32_t>(sub + 234) != lay.hp || mem::read<uint32_t>(sub + 245) != lay.hp ||
      mem::read<uint32_t>(sub + 252) != lay.hp_max || mem::read<uint32_t>(sub + 259) != lay.hp ||
      mem::read<uint32_t>(sub + 268) != lay.hp || (mem::read<uint32_t>(sub + 277) & 0xFFFFFFu) != 0x0004C2u ||
      mem::read<uint32_t>(on + 27) != rider || mem::read<uint32_t>(on + 36) != rider ||
      mem::read<uint32_t>(on + 44) != rider + 4 || mem::read<uint32_t>(on + 57) != rider + 4 || rider + 8 > ride->size)
    return false;
  const int n = vthook::hook(ride, game::kSubLifeSlot, 0, false, reinterpret_cast<void*>(&hk_ride_sub), &g_ride_sub);
  if (n <= 0) return false;
  g_ride.rider = rider;
  g_ride_sub_fn = sub;
  g_cls_ride = ride;
  logf("cheats: God mode: the rides' subtract-health (slot %d; uSmRide's exe+0x%X, its riders +0x%X and +0x%X) hooked "
       "in %d vtables - a ride a spared character rides takes nothing",
       game::kSubLifeSlot, static_cast<unsigned>(sub - image::base()), rider, rider + 4, n);
  return true;
}

// As God mode (or the partner's) goes on: each ride in play with a spared rider and some health left filled up.
void repair_rides() {
  const uintptr_t sunit = game::system("sUnit");
  if (!g_rides_ok || !sunit) return;
  const game::Layout& lay = game::layout();
  units::each(sunit, [&](uintptr_t u, const mt::Class* c) {
    if (!c || !mt::derives(c->dti, g_cls_ride->dti) || !ride_spared(u)) return true;
    const int16_t hp = mem::read<int16_t>(u + lay.hp), most = mem::read<int16_t>(u + lay.hp_max);
    if (hp >= 1 && hp < most) {
      logf("cheats: God mode: %s %08X ridden by a spared character - its health %d filled up to %d", c->name,
           static_cast<unsigned>(u), hp, most);
      *reinterpret_cast<int16_t*>(u + lay.hp) = most;
    }
    return true;
  });
}

// Whether a hit was dealt by a player's character (the shooter of a shot, the
// thrower of a grenade, the one a blow is from) - yours or your partner's.
bool dealt_by_player(const void* hit) {
  uintptr_t by = 0;
  if (!mem::read_safe(reinterpret_cast<uintptr_t>(hit) + game::layout().hit_attacker, &by) || !by) return false;
  return mt::is_a(by, g_cls_player);
}

// --- One hit kills on an enemy that keeps its life -----------------------------------------
// One hit kills takes all of an enemy's health. But some enemies keep their life through a hit,
// by the game's own rules: their fight goes on through what a hit's damage is worked into
// besides their health - their parts (Deborah's cores, Rasklapanje's limbs, the parts a boss's
// take-damage works a hit into through slot 129), their stagger and their own counters (Brzak's
// tongue, Ustanak's knockdown and guard, Haos' damage count). A player's damaging hit on one is
// worked out as kBigHit, the most health a character can have, so each of those breaks or passes
// at one hit through the game's own code - with its own limits kept (a part it holds at 1 stays
// at 1). The enemies that keep their life (each found from the game's own code):
//   - the game keeps a point: mIsNoDead, or the take-damage's other keep-alive flag;
//   - its own code keeps it: Deborah's damage hands on no health under her flag (the mine cart),
//     Rasklapanje's subtract-health never lets health below 1;
//   - its end is the stage's script, not damage - One hit kills takes it to its last point, never
//     0 (0 would mark it dead, a state the game never gives it): Brzak (its damage hands on 1
//     whatever the hit), an enemy whose brain has no death reaction (Ustanak), Haos on the pipes
//     (stage 903: the stage moves on through its hand's reactions, and its death would end them),
//     the attack chopper the stage shoots down (its damage keeps a point until the stage's scene).
// Enemies that can die are left exactly as before: the health ends them - Haos in the chase among
// them (stage 972, mode 1: the stage waits for it gone or dead; its death reaction is a death).
constexpr int32_t kBigHit = 0x7FFF;
// Haos' mode (its placement's mSetType) whose end is the stage's script: 2, on the pipes (stage 903).
constexpr uint32_t kHaosModePipes = 2;
struct Keep {
  uint32_t keep = 0, no_dead = 0;  // uEnemy: the two flags its take-damage keeps a point for (bytes)
  uint32_t flags = 0;              // uCharacter: the flags word ...
  uint32_t no_life_bit = 0;        // ... and the bit under which Deborah's damage hands on no health
  uintptr_t no_life_fn = 0;        // her damage worked out (uEm4500's slot 126)
  uintptr_t hit_count_fn = 0;      // Brzak's (uEm4200's slot 126): 1 handed on whatever the hit
  uintptr_t floor_fn = 0;          // Rasklapanje's subtract-health (slot 44): never below 1
  uint32_t intel = 0;              // uEnemy: its brain (mpIntel) ...
  uint32_t death_reaction = 0;     // ... whose death reaction is at this vtable offset ...
  uintptr_t no_reaction = 0;       // ... or uEmIntel's own: none
  uintptr_t haos_fn = 0;           // Haos' damage worked out (uEm8000's slot 126) ...
  uint32_t haos_mode = 0;          // ... and its mode
  uintptr_t hold_fn = 0;           // the choppers' damage worked out (uEm9900's slot 126): a point kept ...
  uint32_t hold_stage_at = 0, hold_scene_at = 0;  // ... while cGameInfo's stage (at) ...
  uint32_t hold_stage = 0, hold_scene = 0;        // ... is this one and its scene (at) below this
  uint32_t parts = 0, broken = 0;  // uEnemy: the parts' (health, most) pairs and their broken mask (the log)
};
Keep g_keep;
bool g_big_ok = false;

uintptr_t slot_of(const void* self, int slot) {
  return mem::read<uintptr_t>(*static_cast<const uintptr_t*>(self) + 4 * slot);
}

// Whether an enemy's brain has no death reaction (its damage alone never ends it: the stage's
// script does).
bool no_death_reaction(uintptr_t e) {
  if (!g_keep.no_reaction) return false;
  uintptr_t brain = 0, vt = 0, fn = 0;
  if (!mem::read_safe(e + g_keep.intel, &brain) || !brain || !mem::read_safe(brain, &vt) || !vt ||
      !mem::read_safe(vt + g_keep.death_reaction, &fn))
    return false;
  return fn == g_keep.no_reaction;
}

// Why an enemy keeps its life through a hit (null when the hit can end it); *last: its end is the
// stage's script - One hit kills must stop at its last point itself.
const char* kept_alive(const void* self, bool* last = nullptr) {
  const auto e = reinterpret_cast<uintptr_t>(self);
  if (last) *last = false;
  if (mem::read<uint8_t>(e + g_keep.no_dead)) return "mIsNoDead: it may not die";
  if (mem::read<uint8_t>(e + g_keep.keep)) return "its keep-alive flag: it may not die yet";
  const uintptr_t work = original(g_ework, self, 0);
  if (g_keep.no_life_fn && work == g_keep.no_life_fn && (mem::read<uint32_t>(e + g_keep.flags) & g_keep.no_life_bit))
    return "its own damage takes no health now";
  if (g_keep.floor_fn && slot_of(self, game::kSubLifeSlot) == g_keep.floor_fn)
    return "its own subtract-health never goes below 1";
  const char* why = nullptr;
  if (g_keep.hit_count_fn && work == g_keep.hit_count_fn) {
    why = "its own damage takes 1 a hit, whatever the hit: the stage's script ends it";
  } else if (no_death_reaction(e)) {
    why = "its brain has no death reaction: the stage's script ends it";
  } else if (g_keep.haos_fn && work == g_keep.haos_fn) {
    if (mem::read<uint32_t>(e + g_keep.haos_mode) == kHaosModePipes)
      why = "Haos on the pipes: the stage moves on through its hand, not its death";
  } else if (g_keep.hold_fn && work == g_keep.hold_fn) {
    const uintptr_t gi = game::game_info_at();
    if (gi && mem::read<uint32_t>(gi + g_keep.hold_stage_at) == g_keep.hold_stage &&
        mem::read<uint32_t>(gi + g_keep.hold_scene_at) < g_keep.hold_scene)
      why = "its own damage keeps a point until the stage's scene: the stage's script ends it";
  }
  if (why && last) *last = true;
  return why;
}

// Why a hit is to be worked out as a big one (One hit kills on, a player's hit with damage, on an
// enemy that keeps its life through it), or null.
const char* big_hit(const void* self, const void* hit) {
  if (!g_big_ok || !g_on[sw::kOneHitKills] || !hit) return nullptr;
  const int32_t damage = *reinterpret_cast<const int32_t*>(reinterpret_cast<uintptr_t>(hit) + game::layout().hit_damage);
  if (damage <= 0 || damage >= kBigHit || !dealt_by_player(hit)) return nullptr;
  return kept_alive(self);
}

uint32_t broken_parts(const void* self) {
  return g_keep.broken ? mem::read<uint32_t>(reinterpret_cast<uintptr_t>(self) + g_keep.broken) : 0;
}

// The log: the first big hit on an enemy (and why), and each part one breaks.
uintptr_t g_noted[8] = {};
unsigned g_noted_next = 0;
void note_big(const void* self, const char* why, uint32_t broken_before) {
  const auto e = reinterpret_cast<uintptr_t>(self);
  const char* name = mt::class_name_of(e);
  bool seen = false;
  for (const uintptr_t n : g_noted) seen = seen || n == e;
  if (!seen) {
    g_noted[g_noted_next++ % 8] = e;
    logf("cheats: One hit kills: %s %08X keeps its life (%s) - a player's hit on it is worked out as %d: its parts, "
         "its stagger",
         name ? name : "an enemy", static_cast<unsigned>(e), why, kBigHit);
  }
  const uint32_t now = broken_parts(self);
  if (now & ~broken_before)
    logf("cheats: One hit kills: %s %08X - part(s) 0x%X broken at one hit", name ? name : "an enemy",
         static_cast<unsigned>(e), now & ~broken_before);
}

// The hit's damage as a big one while `fn` runs (the hit is the game's: put back after).
template <typename Fn>
void with_big_hit(const void* self, void* hit, const char* why, Fn fn) {
  auto* damage = reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(hit) + game::layout().hit_damage);
  const int32_t was = *damage;
  const uint32_t broken = broken_parts(self);
  *damage = kBigHit;
  fn();
  *damage = was;
  note_big(self, why, broken);
}

using EnemyWorkFn = int(__thiscall*)(void*, void*);

// An enemy's damage worked out (slot 126; the hit is the damage handler's own copy, its damage
// already scaled to the mode): the health it is to take - with the hit worked into its parts and
// its stagger on the way (Deborah's cores, every enemy's stagger). A player's damaging hit on an
// enemy that keeps its life is worked out as a big one.
int __thiscall hk_enemy_work(void* self, void* hit) {
  const auto orig = reinterpret_cast<EnemyWorkFn>(original(g_ework, self, g_ework_fn));
  const char* why = big_hit(self, hit);
  if (!why) return orig(self, hit);
  int health = 0;
  with_big_hit(self, hit, why, [&] { health = orig(self, hit); });
  return health;
}

// An enemy's take-damage (slot 128): the hit's damage, taken off its health. With
// One hit kills a hit of a player's that does damage takes all the health there is,
// and the game goes on as with any killing hit: the enemy's own death, or - for an
// enemy the game keeps alive for now (a boss between its phases, one that may not die
// yet) - the last point it keeps. An enemy whose end is the stage's script (Brzak, Ustanak,
// Haos on the pipes, the shot-down chopper) is taken to its last point, never 0 - the
// game never marks it dead. The take-damage first works the hit into the parts a class keeps
// there (slot 129: a J'avo's, Ogroman's, Haos' ...): for an enemy that keeps its life, from a
// big hit.
void __thiscall hk_enemy_apply(void* self, void* hit, int amount) {
  const auto orig = reinterpret_cast<EnemyApplyFn>(original(g_eapply, self, g_eapply_fn));
  amount = merc::lethal(self, hit, amount);  // Kill enemies as they appear: a hit of its own takes all the health
  if (g_on[sw::kOneHitKills] && hit && amount > 0 && dealt_by_player(hit)) {
    const int hp = *reinterpret_cast<const int16_t*>(reinterpret_cast<uintptr_t>(self) + game::layout().hp);
    bool last = false;
    if (g_big_ok) kept_alive(self, &last);
    if (last) amount = hp > 1 ? hp - 1 : 0;
    else if (hp > amount) amount = hp;
  }
  const char* why = big_hit(self, hit);
  if (!why) {
    orig(self, hit, amount);
    return;
  }
  with_big_hit(self, hit, why, [&] { orig(self, hit, amount); });
}

// Whether `fn` is the game's `generic` or a function of a class's own that calls it
// first thing (an override that adds to it): a call to it in its first bytes.
// The vtable hooks (vthook.h): each class deriving from `base` whose `slot` holds `generic` (or one calling it first,
// with `overrides`; with no `generic` whatever each class has there) gets `hook` there.
int hook_vtables(const mt::Class* base, int slot, uintptr_t generic, bool overrides, void* hook,
                 std::vector<Hooked>* out) {
  return vthook::hook(base, slot, generic, overrides, hook, out);
}
using vthook::unhook;

// A function of uPlayer's (found by its signature) hooked in every player vtable that holds it - its slot where
// uPlayer's own vtable has it.
int hook_player_fn(uintptr_t fn, void* hook, std::vector<Hooked>* out, int* slot, uintptr_t* orig) {
  if (!fn || !g_cls_player || !g_cls_player->vtable) return 0;
  const int at = vthook::slot_of(g_cls_player->vtable, fn, 300);
  if (at < 0) return 0;
  *slot = at;
  *orig = fn;
  return hook_vtables(g_cls_player, at, fn, false, hook, out);
}

// One hit kills on an enemy that keeps its life: the flags the take-damage keeps a point for
// (read where it reads them), the damage worked out (slot 126) hooked in every enemy vtable -
// whatever each class has there - and Deborah's: her damage worked out is the slot-126
// function that holds the end handing on no health under her flag. Without it all, One hit
// kills stays as it was (the health only).
void install_big(const mt::Class* enemy) {
  const game::Layout& lay = game::layout();
  const uintptr_t keep = game::sig_one("kSigEnemyKeepAlive"), work = game::sig_one("kSigEnemyWork"),
                  part = game::sig_one("kSigEnemyPart"), no_life = game::sig_one("kSigEnemyNoLife");
  const uintptr_t work_slot = enemy->vtable ? mem::read<uintptr_t>(enemy->vtable + 4 * game::kEnemyWorkSlot) : 0;
  // The keep-alive test lies in the take-damage itself, after asking slot 58; the two flags are bytes side by side.
  if (!keep || keep < g_eapply_fn || keep > g_eapply_fn + 0x200 ||
      mem::read<uint32_t>(keep + 4) != 4u * game::kEnemyMayDieSlot) {
    logf("cheats: One hit kills: the take-damage's keep-alive test not found - the health only");
    return;
  }
  g_keep.keep = mem::read<uint32_t>(keep + 18);
  g_keep.no_dead = mem::read<uint32_t>(keep + 27);
  if (g_keep.no_dead + 1 != g_keep.keep || g_keep.keep >= enemy->size || g_keep.no_dead < lay.hp) {
    logf("cheats: One hit kills: the keep-alive flags +0x%X/+0x%X do not fit uEnemy - the health only", g_keep.no_dead,
         g_keep.keep);
    return;
  }
  // The damage worked out: uEnemy's own reads the hit's damage where a hit keeps it.
  if (!work || work != work_slot || mem::read<uint8_t>(work + 17) != lay.hit_damage) {
    logf("cheats: One hit kills: the enemies' damage worked out (slot %d) not found - the health only",
         game::kEnemyWorkSlot);
    return;
  }
  g_ework_fn = work;
  // The parts (for the log): 32, their (health, most) pairs, the broken mask right after them.
  if (part && mem::read<uint8_t>(part + 11) == 31) {
    const uint32_t parts = mem::read<uint32_t>(part + 17), broken = mem::read<uint32_t>(part + 28);
    if (broken == parts + 32 * 8 && broken + 4 <= enemy->size) {
      g_keep.parts = parts;
      g_keep.broken = broken;
    }
  }
  const int n = hook_vtables(enemy, game::kEnemyWorkSlot, 0, false, reinterpret_cast<void*>(&hk_enemy_work), &g_ework);
  // The hooked slot-126 function that holds the code at `at` (the nearest one starting below it).
  auto work_holding = [](uintptr_t at, uintptr_t within) -> uintptr_t {
    uintptr_t fn = 0;
    for (const Hooked& h : g_ework)
      if (at && h.orig <= at && at - h.orig < within && h.orig > fn) fn = h.orig;
    return fn;
  };
  // Deborah's: the hooked slot-126 function nearest below the end, setting bl to 1 at its start.
  if (no_life) {
    const uintptr_t fn = work_holding(no_life, 0x400);
    bool bl1 = false;
    for (uintptr_t a = fn; fn && a < fn + 0x30 && !bl1; ++a)
      bl1 = mem::read<uint8_t>(a) == 0xBB && mem::read<uint32_t>(a + 1) == 1;
    const uint32_t flags = mem::read<uint32_t>(no_life + 2);
    if (fn && bl1 && flags + 4 <= enemy->size) {
      g_keep.no_life_fn = fn;
      g_keep.flags = flags;
      g_keep.no_life_bit = 1u << 1;
    }
  }
  // Brzak's: a hooked slot-126 function that runs uEnemy's own on the hit and hands on 1.
  const uintptr_t hit_count = game::sig_one("kSigEnemyHitCount");
  if (hit_count && game::call_target(hit_count + 5) == work && mem::read<uint32_t>(hit_count + 11) == 1)
    for (const Hooked& h : g_ework)
      if (h.orig == hit_count) g_keep.hit_count_fn = hit_count;
  // Rasklapanje's: a subtract-health that runs the characters' own, then puts health back to 1.
  const uintptr_t floor = game::sig_one("kSigEnemyFloor");
  if (floor && g_sub_fn && game::call_target(floor + 8) == g_sub_fn && mem::read<uint32_t>(floor + 16) == lay.hp &&
      mem::read<uint32_t>(floor + 40) == lay.hp && mem::read<uint32_t>(floor + 26) == lay.hp_max)
    g_keep.floor_fn = floor;
  // The brain's death reaction: the pending hits' choice (after the keep-alive flag, slot 58 and the health),
  // and uEmIntel's own (`or eax, -1; ret`: none).
  const uintptr_t intel = game::sig_one("kSigEnemyIntel"), reaction = game::sig_one("kSigEnemyReaction");
  const mt::Class* brain = mt::find("uEmIntel");
  if (intel && reaction && brain && brain->vtable && mem::read<uint32_t>(intel + 11) == mem::read<uint32_t>(intel + 29) &&
      mem::read<uint32_t>(intel + 11) + 4 <= enemy->size && mem::read<uint32_t>(reaction + 15) == g_keep.keep &&
      mem::read<uint32_t>(reaction + 26) == 4u * game::kEnemyMayDieSlot && mem::read<uint32_t>(reaction + 47) == lay.hp) {
    const uint32_t death = mem::read<uint32_t>(reaction + 62);
    const uintptr_t none = death % 4 == 0 && death < 0x400 ? mem::read<uintptr_t>(brain->vtable + death) : 0;
    if (none && image::text().contains(none) && mem::read<uint32_t>(none) == 0xC3FFC883u) {  // or eax,-1; ret
      g_keep.intel = mem::read<uint32_t>(intel + 11);
      g_keep.death_reaction = death;
      g_keep.no_reaction = none;
    }
  }
  // Haos': the hooked slot-126 function holding its mode switch (9 bytes in); the mode a field of its
  // own class (the one whose vtable holds the function).
  const uintptr_t haos = game::sig_one("kSigEnemyHaosMode");
  if (const uintptr_t fn = work_holding(haos, 0x10)) {
    uint32_t size = 0;
    for (const Hooked& h : g_ework)
      if (h.orig == fn) {
        const uintptr_t vt = h.vt;  // a stand-in object: its first dword the vtable
        if (const mt::Class* c = mt::by_dti(mt::dti_of_object(reinterpret_cast<uintptr_t>(&vt)))) size = c->size;
        break;
      }
    if (mem::read<uint32_t>(haos + 18) >= enemy->size / 2 && mem::read<uint32_t>(haos + 18) + 4 <= size) {
      g_keep.haos_fn = fn;
      g_keep.haos_mode = mem::read<uint32_t>(haos + 18);
    }
  }
  // The choppers': the hooked slot-126 function holding the hold, its stage and scene in the profile's cGameInfo.
  const uintptr_t hold = game::sig_one("kSigEnemyStageHold");
  if (const uintptr_t fn = work_holding(hold, 0x400); fn && lay.game_info && mem::read<uint8_t>(hold + 8) == lay.profile &&
                                                      mem::read<uint32_t>(hold + 11) == lay.game_info_size &&
                                                      mem::read<uint32_t>(hold + 44) == lay.hp) {
    const uint32_t stage_at = mem::read<uint32_t>(hold + 18), scene_at = mem::read<uint32_t>(hold + 31);
    if (stage_at >= lay.game_info && stage_at + 4 <= lay.game_info + lay.game_info_size && scene_at >= lay.game_info &&
        scene_at + 4 <= lay.game_info + lay.game_info_size) {
      g_keep.hold_fn = fn;
      g_keep.hold_stage_at = stage_at - lay.game_info;
      g_keep.hold_scene_at = scene_at - lay.game_info;
      g_keep.hold_stage = mem::read<uint32_t>(hold + 22);
      g_keep.hold_scene = mem::read<uint32_t>(hold + 35);
    }
  }
  g_big_ok = n > 0;
  logf("cheats: One hit kills: the enemies' damage worked out (slot %d) hooked in %d vtables; an enemy keeping its "
       "life (+0x%X, mIsNoDead +0x%X) works a player's hit out as %d - its parts (+0x%X, broken +0x%X) and stagger",
       game::kEnemyWorkSlot, n, g_keep.keep, g_keep.no_dead, kBigHit, g_keep.parts, g_keep.broken);
  // The classes whose own damage code keeps their life (named from the vtable that holds it).
  auto name_of = [](uintptr_t fn) -> const char* {
    for (const Hooked& h : g_ework)
      if (h.orig == fn) {
        const uintptr_t vt = h.vt;  // a stand-in object: its first dword the vtable
        const char* name = mt::class_name_of(reinterpret_cast<uintptr_t>(&vt));
        return name ? name : "an enemy";
      }
    return "an enemy";
  };
  if (g_keep.no_life_fn)
    logf("cheats: One hit kills: %s's damage worked out (exe+0x%X) hands on no health while bit 1 of its +0x%X is "
         "set - it keeps its life then too",
         name_of(g_keep.no_life_fn), static_cast<unsigned>(g_keep.no_life_fn - image::base()), g_keep.flags);
  else
    logf("cheats: One hit kills: Deborah's damage worked out not found (her mine cart's health)");
  if (g_keep.hit_count_fn)
    logf("cheats: One hit kills: %s's damage worked out (exe+0x%X) hands on 1 whatever the hit - its life counts hits "
         "(the game's script ends it): taken to its last point, a player's hit worked out big",
         name_of(g_keep.hit_count_fn), static_cast<unsigned>(g_keep.hit_count_fn - image::base()));
  else
    logf("cheats: One hit kills: Brzak's damage worked out not found (its life counted in hits)");
  logf("cheats: One hit kills: a subtract-health that never goes below 1 %s (exe+0x%X); a brain's death reaction its "
       "vtable +0x%X, none at exe+0x%X (an enemy's brain +0x%X)",
       g_keep.floor_fn ? "found" : "NOT found", static_cast<unsigned>(g_keep.floor_fn ? g_keep.floor_fn - image::base() : 0),
       g_keep.death_reaction, static_cast<unsigned>(g_keep.no_reaction ? g_keep.no_reaction - image::base() : 0),
       g_keep.intel);
  if (g_keep.haos_fn)
    logf("cheats: One hit kills: %s's damage worked out (exe+0x%X): in mode %u (+0x%X; on the pipes) the stage moves "
         "on through its hand - taken to its last point; in its other modes its death and mIsNoDead are the game's",
         name_of(g_keep.haos_fn), static_cast<unsigned>(g_keep.haos_fn - image::base()), kHaosModePipes,
         g_keep.haos_mode);
  else
    logf("cheats: One hit kills: Haos' damage worked out not found (its modes)");
  if (g_keep.hold_fn)
    logf("cheats: One hit kills: %s's damage worked out (exe+0x%X) keeps a point in stage %u until scene %u (cGameInfo "
         "+0x%X, +0x%X) - taken to its last point then",
         name_of(g_keep.hold_fn), static_cast<unsigned>(g_keep.hold_fn - image::base()), g_keep.hold_stage,
         g_keep.hold_scene, g_keep.hold_stage_at, g_keep.hold_scene_at);
  else
    logf("cheats: One hit kills: the choppers' stage hold not found");
}

// --- byte patches ---------------------------------------------------------------------------
mem::Patch g_buy, g_level;  // the skill screen's purchases: the price not taken off the points

// --- Infinite ammo: the game's own bit, set on the characters this machine drives ------------
// Set as the switch goes on and as a character of this machine's comes into play, cleared (only where the mod set it)
// as the switch goes off or the character goes to another; the game clears the whole flags word only as it sets a
// character up and in its reset (a Continue, a stage's start: hk_player_reset puts the bit back after it). Nothing
// else of the game's clears it.
struct Armed {
  uintptr_t obj = 0;  // a character whose bit the mod set (it was clear)
};
std::vector<Armed> g_armed;  // under g_armed_lock (the reset's hook may come from another thread)
CRITICAL_SECTION g_armed_lock;
bool g_ammo_ok = false;

uint32_t& ammo_word(uintptr_t obj) { return *reinterpret_cast<uint32_t*>(obj + game::layout().ammo_flags); }
uint32_t ammo_bit() { return 1u << game::layout().ammo_bit; }

bool armed_by_us(uintptr_t obj) {
  for (const Armed& a : g_armed)
    if (a.obj == obj) return true;
  return false;
}

// The bit set on one character (where it was clear: the mod's then), or cleared from it (where the mod set it).
void arm(uintptr_t obj, bool on) {
  EnterCriticalSection(&g_armed_lock);
  uint32_t& w = ammo_word(obj);
  if (on && !(w & ammo_bit())) {
    w |= ammo_bit();
    if (!armed_by_us(obj)) g_armed.push_back(Armed{obj});
  } else if (!on) {
    for (size_t i = 0; i < g_armed.size(); ++i)
      if (g_armed[i].obj == obj) {
        w &= ~ammo_bit();
        g_armed.erase(g_armed.begin() + static_cast<long>(i));
        break;
      }
  }
  LeaveCriticalSection(&g_armed_lock);
}

// The events: the switch turned (every character), a character come into play or gone to another (that one). The
// characters the mod armed that are no longer in play are forgotten.
void ammo_events(const game::Run& run, bool on, bool switched, const bool* fresh) {
  if (!g_ammo_ok) return;
  for (int i = 0; i < run.nchars; ++i) {
    const game::Character& c = run.chars[i];
    if (switched || fresh[i]) arm(c.obj, on && c.control == game::Control::kLocal);
  }
  EnterCriticalSection(&g_armed_lock);
  for (size_t i = 0; i < g_armed.size();) {
    bool here = false;
    for (int k = 0; k < run.nchars; ++k) here = here || run.chars[k].obj == g_armed[i].obj;
    if (here) ++i;
    else g_armed.erase(g_armed.begin() + static_cast<long>(i));
  }
  LeaveCriticalSection(&g_armed_lock);
}

// A player's reset (uPlayer's: a Continue, a stage's start) sets its flags words to 0: the bit put back where the
// mod had set it.
using ResetFn = void(__thiscall*)(void*);
void __thiscall hk_player_reset(void* self) {
  reinterpret_cast<ResetFn>(original(g_reset, self, g_reset_fn))(self);
  const auto p = reinterpret_cast<uintptr_t>(self);
  if (!g_ammo_ok) return;
  EnterCriticalSection(&g_armed_lock);
  const bool ours = armed_by_us(p);
  LeaveCriticalSection(&g_armed_lock);
  if (ours && g_on[sw::kInfiniteAmmo]) ammo_word(p) |= ammo_bit();
}

// The game's bit spares every item its use-an-item function would take - a shot's
// rounds and a thrown grenade, but the health tablets too, wherever a tablet is
// taken (a heal of yourself, of your partner, the ones set aside a hit takes). Those
// calls (each found as a call of the use function right after the tablets' slot is
// looked up) are re-pointed at hk_use_tablets while Infinite ammo is on: the bit is
// let go for the tablets' use, so they are used up as without the cheat.
using UseFn = void(__thiscall*)(void*, int, int);
UseFn g_use = nullptr;      // the game's use-an-item (kSigAmmoUse)
std::vector<mem::Patch> g_tablet_calls;

void __thiscall hk_use_tablets(void* player, int slot, int count) {
  auto* w = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(player) + game::layout().ammo_flags);
  const uint32_t bit = 1u << game::layout().ammo_bit;
  const bool lift = g_on[sw::kInfiniteAmmo] && (*w & bit);
  if (lift) *w &= ~bit;
  g_use(player, slot, count);
  if (lift) *w |= bit;
}

// The 5-byte call at `site` re-pointed at `fn`, as a patch (its old bytes kept).
std::vector<uint8_t> call_to(uintptr_t site, const void* fn) {
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(fn) - (site + 5));
  std::vector<uint8_t> b = {0xE8, 0, 0, 0, 0};
  std::memcpy(b.data() + 1, &rel, 4);
  return b;
}

// Every call of the use function whose 0x80 bytes before look up the tablets' slot
// (`push <the tablets' id>`, then a call of the find-slot function).
void find_tablet_calls(uintptr_t use, uint32_t tablet, uintptr_t find_slot) {
  const mem::Range text = image::text();
  uint8_t push[5] = {0x68};
  std::memcpy(push + 1, &tablet, 4);
  for (uintptr_t a = text.begin; a + 5 <= text.end; ++a) {
    if (mem::read<uint8_t>(a) != 0xE8 || game::call_target(a) != use) continue;
    bool tablets = false;
    for (uintptr_t b = a > text.begin + 0x80 ? a - 0x80 : text.begin; b < a && !tablets; ++b) {
      if (std::memcmp(reinterpret_cast<const void*>(b), push, 5)) continue;
      for (uintptr_t c = b + 5; c < b + 24 && c < a; ++c)
        if (game::call_target(c) == find_slot) tablets = true;
    }
    if (!tablets) continue;
    mem::Patch p;
    char expect[24];
    std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", mem::read<uint8_t>(a + 1), mem::read<uint8_t>(a + 2),
                  mem::read<uint8_t>(a + 3), mem::read<uint8_t>(a + 4));
    if (p.prepare(a, expect, call_to(a, reinterpret_cast<const void*>(&hk_use_tablets)), "tablets' use"))
      g_tablet_calls.push_back(p);
  }
}

// --- Infinite ammo and the helicopter's missiles --------------------------------------------------
// The helicopter Ada flies (sm5060_Ada_Heli: uSm5060 < uSmPlHarrier, the players' aircraft - stages 701, 703, 706,
// 751, 771, 773) carries missiles of its own, which no item of the inventory is: the game's infinite-ammo bit never
// reaches them. Its count, mMissileNum (a copy a seat), is what its setup gives it (a global of the game's: 6), and
// nothing refills it. The missile action's start (its vtable slot 6) launches one while the seat's copy is above 0 -
// taking one off both copies and setting the seat's wait (mMissileWait: a second) - else plays the launch dry. While
// Infinite ammo is on both takes are NOPs: none is used up, and the wait between launches is the game's own. As the
// switch goes on, a helicopter in play that has used some gets its load back (the tick: one walk of sUnit's units).
// Chris's jet (uSm5020, of the same aircraft class) is given no missiles by its setup (no constructor of its writes
// the count): the switch gives it none either.
struct Missiles {
  uint32_t count = 0;              // the aircraft: mMissileNum, two dwords (a seat each)
  uintptr_t load = 0;              // the global the helicopter's setup gives each copy
  const mt::Class* cls = nullptr;  // the class whose setup that is (the helicopter's)
};
Missiles g_heli;
mem::Patch g_missile[2];  // the launch's two takes (the seat's copy, the other)
bool g_missiles_ok = false;

// The launch, cross-checked: the missile action's own start, testing and taking from the same count (its two copies
// side by side, the waits after them, inside the aircraft's class); the setup giving both copies the same global and
// zeroing both waits, in the constructor of a class of aircraft (the nearest store of a vtable before it).
bool install_missiles() {
  const uintptr_t launch = game::sig_one("kSigMissileLaunch"), setup = game::sig_one("kSigMissileLoad");
  const mt::Class* act = mt::find("cSmPlHarrierAct_Missile");
  const mt::Class* plane = mt::find("uSmPlHarrier");
  if (!launch || !setup || !act || !act->vtable || !plane) return false;
  const uint32_t count = mem::read<uint32_t>(launch + 29);
  if (mem::read<uintptr_t>(act->vtable + 4 * 6) != launch || mem::read<uint32_t>(launch + 55) != count ||
      mem::read<uint32_t>(launch + 64) != count + 4 || mem::read<uint32_t>(launch + 84) != count + 8 ||
      count + 16 > plane->size)
    return false;
  const uintptr_t load = mem::read<uint32_t>(setup + 2);
  if (load != mem::read<uint32_t>(setup + 21) || !image::data().contains(load) ||
      mem::read<uint32_t>(setup + 8) != count || mem::read<uint32_t>(setup + 29) != count + 4 ||
      mem::read<uint32_t>(setup + 16) != count + 8 || mem::read<uint32_t>(setup + 37) != count + 12)
    return false;
  const mt::Class* heli = nullptr;
  for (uintptr_t a = setup; a > setup - 0x800 && !heli; --a) {
    if (mem::read<uint16_t>(a) != 0x06C7) continue;  // mov dword [esi], imm32
    uintptr_t vt = mem::read<uintptr_t>(a + 2);      // a stand-in object: its first dword the vtable
    const mt::Class* c =
        image::rdata().contains(vt) ? mt::by_dti(mt::dti_of_object(reinterpret_cast<uintptr_t>(&vt))) : nullptr;
    if (c && c->vtable == vt && mt::derives(c->dti, plane->dti)) heli = c;
  }
  if (!heli || heli->size < count + 16) return false;
  if (!g_missile[0].prepare(launch + 53, "01 91 ?? ?? ?? ??", mem::nops(6), "helicopter's missiles") ||
      !g_missile[1].prepare(launch + 62, "01 91 ?? ?? ?? ??", mem::nops(6), "helicopter's missiles' copy")) {
    g_missile[0] = g_missile[1] = mem::Patch{};
    return false;
  }
  g_heli = Missiles{count, load, heli};
  logf("cheats: Infinite ammo: %s's missiles (+0x%X, a copy a seat; its setup gives %d) - the missile action's two "
       "takes (exe+0x%X, exe+0x%X) NOPs while on",
       heli->name, count, mem::read<int32_t>(load), static_cast<unsigned>(launch + 53 - image::base()),
       static_cast<unsigned>(launch + 62 - image::base()));
  return true;
}

// As Infinite ammo goes on: each helicopter in play that has used missiles gets its load back (both copies).
void refill_missiles() {
  const uintptr_t sunit = game::system("sUnit");
  const int32_t load = mem::read<int32_t>(g_heli.load);
  if (!g_missiles_ok || !sunit || load <= 0) return;
  units::each(sunit, [&](uintptr_t u, const mt::Class* c) {
    if (!c || !mt::derives(c->dti, g_heli.cls->dti)) return true;
    auto* n = reinterpret_cast<int32_t*>(u + g_heli.count);
    if (n[0] < load || n[1] < load) {
      logf("cheats: Infinite ammo: %s %08X - its missiles (%d, %d) back to %d", c->name, static_cast<unsigned>(u),
           n[0], n[1], load);
      n[0] = n[1] = load;
    }
    return true;
  });
}

uint32_t g_patched = 0;  // the patches as the last change left them (a bit each: the tick's list)

void follow(mem::Patch& p, bool on) {
  if (!p.prepared()) return;
  if (on && !p.applied && !p.apply()) logf("cheats: the %s patch could not be put in", p.label);
  if (!on && p.applied && !p.revert()) logf("cheats: the %s patch was not ours to take out any more", p.label);
}

void unavailable(sw::Kind k, const char* why) {
  g_ok[k] = false;
  g_why[k] = why;
}

// --- One hit kills on the room bombs: a player's hit blows one up at once ---------------------
// The floor robots that roll at you and blow up (uSmRoomBomb - uSm1310 - in Leon's chapter 4, s0512)
// are set models, not enemies: the enemies' damage hooks never see them. A bomb keeps its own points
// (5, in the characters' health word) and its two hit checks - while it rolls (its Move action's) and
// in the second it beeps before it goes off by you (its Warning action's) - take 1 a hit, whatever the
// hit's damage: it goes off at the hit after its last point (6 hits). Only a hit the game's own instant
// test passes (the hit's attributes) empties its points at once. Both checks make that test only for a
// hit the game counts as the players': one that did damage, dealt by the players' side (a player's
// character, the players' weapons and items, the guns and vehicles they man) and not by a character
// played over the network. With One hit kills on, those two calls of the test are re-pointed at
// hk_bomb_instant, which says yes to each such hit - the game's own code then empties the bomb's points
// and blows it up as for an instant hit. An enemy's hit, or the stage's, never reaches the test.
struct Bombs {
  uint32_t dealer = 0;   // a set model's hit record: who dealt the hit (a unit; the hit's own dealer, copied)
  uintptr_t test = 0;    // the game's instant test (thiscall (model) -> al)
};
Bombs g_bomb;
bool g_bombs_ok = false;
mem::Patch g_bomb_calls[2];  // the two checks' calls of the test
uintptr_t g_bomb_noted[8] = {};
unsigned g_bomb_noted_next = 0;
int g_bomb_logged = 0;

using BombTestFn = char(__thiscall*)(void*);

// thiscall (model) -> al, as the game's test: the model in ecx, nothing on the stack. Reached only for a hit the game
// counts as the players' (above).
char __fastcall hk_bomb_instant(void* model) {
  if (!g_on[sw::kOneHitKills]) return reinterpret_cast<BombTestFn>(g_bomb.test)(model);
  const auto m = reinterpret_cast<uintptr_t>(model);
  bool seen = false;
  for (const uintptr_t n : g_bomb_noted) seen = seen || n == m;
  if (!seen && g_bomb_logged < 32) {
    ++g_bomb_logged;
    g_bomb_noted[g_bomb_noted_next++ % 8] = m;
    uintptr_t by = 0;
    mem::read_safe(m + g_bomb.dealer, &by);
    const char* name = mt::class_name_of(m);
    const char* who = by ? mt::class_name_of(by) : nullptr;
    logf("cheats: One hit kills: a room bomb (%s %08X) hit by %s - blown up at one hit", name ? name : "a set model",
         static_cast<unsigned>(m), who ? who : "the players' side");
  }
  return 1;
}

// Whether the function at `fn` calls `target` within its first `within` bytes.
bool calls_within(uintptr_t fn, uintptr_t target, size_t within) {
  if (!image::text().contains(fn) || !mem::readable(fn, within + 5)) return false;
  for (uintptr_t a = fn; a < fn + within; ++a)
    if (mem::read<uint8_t>(a) == 0xE8 && game::call_target(a) == target) return true;
  return false;
}

// The room bombs' two hit checks (each its own signature), cross-checked: the same instant test, the same emptying
// of the points (which writes the characters' health word), the same side test, hit record and network field (who
// drives a player); each the frame of its own action class's (Move's calls the rolling one, Warning's is the other).
bool install_bombs(const game::Layout& lay) {
  const uintptr_t roll = game::sig_one("kSigBombHit"), warn = game::sig_one("kSigBombWarnHit");
  const mt::Class* bomb = mt::find("uSmRoomBomb");
  const mt::Class* moving = mt::find("cSmRoomBombAct_Move");
  const mt::Class* warning = mt::find("cSmRoomBombAct_Warning");
  if (!roll || !warn || !bomb || !moving || !moving->vtable || !warning || !warning->vtable || !g_cls_player) return false;
  const uintptr_t test = game::call_target(roll + 85), zero = game::call_target(roll + 96);
  const uint32_t damage = mem::read<uint32_t>(roll + 22), dealer = mem::read<uint32_t>(roll + 36);
  if (!test || test != game::call_target(warn + 73) || !zero || zero != game::call_target(warn + 84) ||
      !game::call_target(roll + 57) || game::call_target(roll + 57) != game::call_target(warn + 50) ||
      damage != mem::read<uint32_t>(warn + 20) || dealer != mem::read<uint32_t>(warn + 29) ||
      mem::read<uint32_t>(roll + 71) != lay.control || mem::read<uint32_t>(warn + 64) != lay.control ||
      damage + 4 > bomb->size || dealer + 4 > bomb->size || !image::text().contains(test))
    return false;
  // The emptying: `xor eax, eax; mov [ecx+<health>], ax`.
  if (!mem::readable(zero, 9) || mem::read<uint32_t>(zero) != 0x8966C033u || mem::read<uint8_t>(zero + 4) != 0x81 ||
      mem::read<uint32_t>(zero + 5) != lay.hp)
    return false;
  bool in_move = false, in_warning = false;
  for (int i = 0; i < 12; ++i) {
    in_move = in_move || calls_within(mem::read<uintptr_t>(moving->vtable + 4 * i), roll, 0x100);
    in_warning = in_warning || mem::read<uintptr_t>(warning->vtable + 4 * i) == warn;
  }
  if (!in_move || !in_warning) return false;
  const uintptr_t sites[2] = {roll + 85, warn + 73};
  for (int i = 0; i < 2; ++i) {
    char expect[24];
    std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", mem::read<uint8_t>(sites[i] + 1),
                  mem::read<uint8_t>(sites[i] + 2), mem::read<uint8_t>(sites[i] + 3), mem::read<uint8_t>(sites[i] + 4));
    if (!g_bomb_calls[i].prepare(sites[i], expect, call_to(sites[i], reinterpret_cast<const void*>(&hk_bomb_instant)),
                                 i ? "room bombs' warning hit" : "room bombs' hit")) {
      g_bomb_calls[0] = g_bomb_calls[1] = mem::Patch{};
      return false;
    }
  }
  g_bomb.dealer = dealer;
  g_bomb.test = test;
  logf("cheats: One hit kills: the room bombs (%s, set models) - their hit checks exe+0x%X and exe+0x%X call the game's "
       "instant test (exe+0x%X; a hit's dealer +0x%X): the players' hits are such hits while One hit kills is on",
       bomb->name, static_cast<unsigned>(roll - image::base()), static_cast<unsigned>(warn - image::base()),
       static_cast<unsigned>(test - image::base()), dealer);
  return true;
}

// --- Steady aim: the sway's own "no player" way out, the kick's "no shot" one ------------------
// The aim's sway (two angles a weapon's aim is turned by, from two slow random swings of
// the player's) has a way out for a call with no player that gives 0 and 0: every call
// takes it while the switch is on (its `jne` past it NOPed) - the over-the-shoulder aim,
// the gun's line and the scope alike. A shot's kick (added to the aim's climb, which
// lifts the reticle and, past its most, the camera) is skipped as for a frame with no
// shot (its `je` made a `jmp`); what has climbed comes back down as ever.
mem::Patch g_sway, g_kick;

// --- Steady aim through a scope: the view held while the aim is ------------------------------------
// Through a scope (the sniper rifles') the game's view is the gun itself: its scope camera (a mode of
// the player's camera, put in as the scope comes up) sets its eye at the gun model's scope joint and
// looks down that joint's axis, and the gun's line - where a shot goes, and the laser - runs down the
// same axis. The gun is in the hands of the character's aim pose, an animation: it breathes, kicks at
// a shot and works the bolt, and the scope moves with it (the sway above is only the game's own swing
// on top). Through a scope the aim is the pose's two numbers, which only the pad and the mouse move
// (the player's copies of its brain's): the pitch (-1 up .. 1 down) and the waist's twist (degrees).
// While the switch is on, the scope camera's call is re-pointed at hk_scope_view, which lets the game
// work its view out, then holds it in the body's frame - the eye and the direction - while neither
// number moves, and while one does follows the gun's own motion on that axis (the twist the view's
// yaw, the pitch its pitch): the aim answers the pad and the mouse as the game's does, and the pose's
// breathing and kick never reach the view. As the scope comes up the view is the game's until the gun
// has settled (the pose raising it). The gun's line's call (hk_gun_line) then points the line the game
// worked out along the held view, so a shot goes where the reticle is.
struct ScopeLayout {
  uint32_t cam_player = 0;          // the scope camera: its player
  uint32_t cam_eye = 0, cam_at = 0;  // ... its view: the eye and the point it looks at (vectors)
  uint32_t pitch = 0, twist = 0;    // uPlayer: the aim's pitch (-1 up .. 1 down), the waist's twist (degrees)
  uint32_t world = 0;               // uModel: the world matrix (4x4 floats: the three axes, then the position)
  uint32_t from = 0, to = 0;        // uPlayer: the gun's line, from and to (vectors): a shot's
};
ScopeLayout g_sl;
using ScopeViewFn = uint32_t(__fastcall*)(void*);
using GunLineFn = void(__fastcall*)(void*);
ScopeViewFn g_scope_view = nullptr;  // the game's scope camera (fastcall (camera) -> al: the view worked out)
GunLineFn g_gun_line = nullptr;      // the game's gun's line (fastcall (player))
mem::Patch g_scope_call, g_line_call;
bool g_scope_ok = false;
uint32_t g_frame = 0;  // the tick's count of the frames not paused

// How the gun settles as the scope comes up: still (in the body's frame) below this speed for this long,
// or held at the latest this long after it came up.
constexpr float kSettleSpeed = 0.1f;  // radians a second (about 6 degrees)
constexpr double kSettleTime = 0.2, kSettleMost = 1.5;
// A number of the aim's moved: by more than this. The held view's pitch at the most (up or down, radians).
constexpr float kPitchMoved = 1e-5f, kTwistMoved = 1e-3f, kPitchMost = 1.53f;

// The held view, in the body's frame (one scope at a time: the scope camera is this machine's).
struct Held {
  uintptr_t player = 0;
  uint32_t frame = 0;                 // the tick's count when last seen
  bool holding = false;               // the gun settled: the view held (else the game's own)
  double up = 0, still = 0, at = 0;   // seconds: the scope came up, the gun still since, the last view
  float pitch = 0, twist = 0;         // the aim's numbers at the last view
  float gun[3] = {}, gun_eye[3] = {};  // the game's view at the last view: its direction and eye
  float dir[3] = {}, eye[3] = {};      // the held view: its direction and eye
};
Held g_held;
int g_scope_logged = 0;

// Seconds since the first call (small numbers: precise whatever the FPU's precision is set to).
double seconds_now() {
  static LARGE_INTEGER freq = {}, first = {};
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
    first = t;
  }
  return freq.QuadPart ? static_cast<double>(t.QuadPart - first.QuadPart) / static_cast<double>(freq.QuadPart) : 0.0;
}
double (*g_now)() = &seconds_now;  // the clock the settling is timed by (the tests' own)

// The body's frame, from its world matrix: the forward axis (the third row), up made square to it, the side
// from the two; the position (the fourth row).
struct Basis {
  float x[3], y[3], z[3], o[3];
};
bool finite3(const float* v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }
float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
bool unit3(float* v) {
  const float n = std::sqrt(dot3(v, v));
  if (!(n > 1e-6f) || !std::isfinite(n)) return false;
  for (int i = 0; i < 3; ++i) v[i] /= n;
  return true;
}
bool body_basis(uintptr_t model, Basis* b) {
  const float* m = reinterpret_cast<const float*>(model + g_sl.world);
  for (int i = 0; i < 3; ++i) {
    b->z[i] = m[8 + i];
    b->y[i] = m[4 + i];
    b->o[i] = m[12 + i];
  }
  if (!finite3(b->o) || !unit3(b->z)) return false;
  const float k = dot3(b->y, b->z);
  for (int i = 0; i < 3; ++i) b->y[i] -= k * b->z[i];
  if (!unit3(b->y)) return false;
  b->x[0] = b->y[1] * b->z[2] - b->y[2] * b->z[1];
  b->x[1] = b->y[2] * b->z[0] - b->y[0] * b->z[2];
  b->x[2] = b->y[0] * b->z[1] - b->y[1] * b->z[0];
  return true;
}
// A direction in the body's frame as its yaw and pitch (up the pitch's positive), and back; a direction from the
// world into the body's frame and back.
void angles(const float* l, float* yaw, float* pitch) {
  *yaw = std::atan2(l[0], l[2]);
  *pitch = std::atan2(l[1], std::sqrt(l[0] * l[0] + l[2] * l[2]));
}
void from_angles(float yaw, float pitch, float* l) {
  const float cp = std::cos(pitch);
  l[0] = std::sin(yaw) * cp;
  l[1] = std::sin(pitch);
  l[2] = std::cos(yaw) * cp;
}
void to_body(const Basis& b, const float* d, float* l) {
  l[0] = dot3(d, b.x);
  l[1] = dot3(d, b.y);
  l[2] = dot3(d, b.z);
}
void to_world(const Basis& b, const float* l, float* d) {
  for (int i = 0; i < 3; ++i) d[i] = l[0] * b.x[i] + l[1] * b.y[i] + l[2] * b.z[i];
}
constexpr float kPi = 3.14159265f;
float wrap_pi(float a) {
  if (!std::isfinite(a)) return 0;
  a = std::fmod(a + kPi, 2 * kPi);
  return (a < 0 ? a + 2 * kPi : a) - kPi;
}
// Turn v the way the unit a turns into the unit b (the shortest way: about their cross).
void turn_like(float* v, const float* a, const float* b) {
  float k[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  const float sn = std::sqrt(dot3(k, k)), cs = dot3(a, b);
  if (!(sn > 1e-7f) || cs < 0) return;  // no turn (half a turn between two frames: none)
  for (int i = 0; i < 3; ++i) k[i] /= sn;
  const float kv[3] = {k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0]};
  const float kd = dot3(k, v);
  for (int i = 0; i < 3; ++i) v[i] = v[i] * cs + kv[i] * sn + k[i] * kd * (1 - cs);
}

// The held view's direction (world) for a player whose scope view is held now (the last frame's or this one's),
// in its body's frame as it stands: false when there is none.
bool held_direction(uintptr_t player, float* d) {
  const Held& h = g_held;
  if (!h.holding || h.player != player || g_frame - h.frame > 1) return false;
  Basis b;
  if (!body_basis(player, &b)) return false;
  to_world(b, h.dir, d);
  return finite3(d);
}

// After the game's scope camera has worked its view out (cam: its eye and the point it looks at).
void hold_view(uintptr_t cam) {
  const uintptr_t player = mem::read<uintptr_t>(cam + g_sl.cam_player);
  if (!player ||
      mem::read<uint32_t>(player + game::layout().control) != static_cast<uint32_t>(game::Control::kLocal))
    return;
  Basis b;
  if (!body_basis(player, &b)) return;
  float* eye = reinterpret_cast<float*>(cam + g_sl.cam_eye);
  float* at = reinterpret_cast<float*>(cam + g_sl.cam_at);
  float d[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
  const float len = std::sqrt(dot3(d, d));
  const float pitch = mem::read<float>(player + g_sl.pitch), twist = mem::read<float>(player + g_sl.twist);
  if (!finite3(eye) || !unit3(d) || !std::isfinite(pitch) || !std::isfinite(twist)) return;
  float g[3], ge[3];  // the game's view in the body's frame: its direction and eye
  to_body(b, d, g);
  const float rel[3] = {eye[0] - b.o[0], eye[1] - b.o[1], eye[2] - b.o[2]};
  to_body(b, rel, ge);

  Held& h = g_held;
  const double now = g_now();
  if (h.player != player || g_frame - h.frame > 2) {  // the scope came up
    h = Held{};
    h.player = player;
    h.up = h.still = now;
  } else if (!h.holding) {
    // Settling: the gun's turn in the body's frame since the last view.
    const float k[3] = {h.gun[1] * g[2] - h.gun[2] * g[1], h.gun[2] * g[0] - h.gun[0] * g[2],
                        h.gun[0] * g[1] - h.gun[1] * g[0]};
    const float moved = std::atan2(std::sqrt(dot3(k, k)), dot3(h.gun, g));
    const double dt = now - h.at;
    if (moved > kSettleSpeed * static_cast<float>(dt > 0 ? dt : 0)) h.still = now;
    if (now - h.still >= kSettleTime || now - h.up >= kSettleMost) {
      h.holding = true;
      for (int i = 0; i < 3; ++i) {
        h.dir[i] = g[i];
        h.eye[i] = ge[i];
      }
      if (g_scope_logged < 8) {
        ++g_scope_logged;
        logf("cheats: Steady aim: a scope's view held %.2f s after it came up (the gun %s)", now - h.up,
             now - h.still >= kSettleTime ? "settled" : "not still yet: held at the latest");
      }
    }
  } else {
    // Holding: what of the gun's turn the aim made (its twist the yaw, its pitch the pitch: the body upright, the
    // world's up its own), the whole turn while either moves (the body lying: no up to part them by); else none.
    const bool yawed = std::fabs(twist - h.twist) > kTwistMoved, pitched = std::fabs(pitch - h.pitch) > kPitchMoved;
    if (yawed || pitched) {
      if (b.y[1] > 0.9f) {
        float sy, sp, y0, p0, y1, p1;
        angles(h.dir, &sy, &sp);
        angles(h.gun, &y0, &p0);
        angles(g, &y1, &p1);
        if (yawed) sy = wrap_pi(sy + wrap_pi(y1 - y0));
        if (pitched) sp = std::fmax(-kPitchMost, std::fmin(kPitchMost, sp + (p1 - p0)));
        from_angles(sy, sp, h.dir);
      } else {
        turn_like(h.dir, h.gun, g);
        unit3(h.dir);
      }
      for (int i = 0; i < 3; ++i) h.eye[i] += ge[i] - h.gun_eye[i];
    }
  }
  h.frame = g_frame;
  h.at = now;
  h.pitch = pitch;
  h.twist = twist;
  for (int i = 0; i < 3; ++i) {
    h.gun[i] = g[i];
    h.gun_eye[i] = ge[i];
  }
  if (!h.holding) return;

  float view[3], e[3];
  to_world(b, h.dir, view);
  to_world(b, h.eye, e);
  for (int i = 0; i < 3; ++i) e[i] += b.o[i];
  if (!finite3(view) || !finite3(e) || !unit3(view)) return;
  for (int i = 0; i < 3; ++i) {
    eye[i] = e[i];
    at[i] = e[i] + view[i] * len;
  }
}

// fastcall (camera) -> al, as the game's scope camera: true when it worked a view out.
uint32_t __fastcall hk_scope_view(void* cam) {
  const uint32_t worked = g_scope_view(cam);
  if ((worked & 0xFF) && g_on[sw::kSteadyAim]) hold_view(reinterpret_cast<uintptr_t>(cam));
  return worked;
}

// fastcall (player), as the game's gun's line: the line it worked out pointed along the held scope view.
void __fastcall hk_gun_line(void* self) {
  g_gun_line(self);
  if (!g_on[sw::kSteadyAim]) return;
  const auto player = reinterpret_cast<uintptr_t>(self);
  float s[3];
  if (!held_direction(player, s)) return;
  const float* from = reinterpret_cast<const float*>(player + g_sl.from);
  float* to = reinterpret_cast<float*>(player + g_sl.to);
  const float d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const float len = std::sqrt(dot3(d, d));
  if (!finite3(from) || !(len > 1e-3f) || !std::isfinite(len)) return;
  for (int i = 0; i < 3; ++i) to[i] = from[i] + s[i] * len;
}

// The scope camera (the one of the camera modes' callbacks whose view is the scope camera, which calls the aim
// sway) and the gun's line (the laser's call; it calls the sway as a scope's view is up), with what the mod reads
// of them: the camera's player and view as the callback copies them out, the zoom the camera reads where the aim's
// numbers end; the line's ends as a shot reads them; the player's world matrix where uModel's joint matrix (the
// players' own slot 20) has it.
bool install_scope(const game::Layout& lay, uintptr_t sway) {
  const std::vector<uintptr_t> views = game::sig_all("kSigScopeView");
  const uintptr_t camera = game::sig_one("kSigScopeCamera"), call = game::sig_one("kSigGunLineCall"),
                  end = game::sig_one("kSigGunLineEnd"), shot = game::sig_one("kSigShotLine"),
                  aim = game::sig_one("kSigAimState"), joint = game::sig_one("kSigJointMatrix");
  if (!sway || !camera || !call || !end || !shot || !aim || !joint || !g_cls_player || !g_cls_player->vtable ||
      !lay.control)
    return false;
  uintptr_t view = 0;
  for (const uintptr_t v : views)
    if (game::call_target(v + 15) == camera) view = view ? ~uintptr_t{0} : v;
  if (!view || view == ~uintptr_t{0} || !calls_within(camera, sway, 0x300)) return false;
  const uintptr_t line = game::call_target(call + 100);
  if (!line || !calls_within(line, sway, 0x300) || end <= line || end > line + 0x1000) return false;
  ScopeLayout s;
  s.cam_player = mem::read<uint32_t>(camera + 55);
  s.cam_eye = mem::read<uint8_t>(view + 39);
  s.cam_at = mem::read<uint8_t>(view + 62);
  s.pitch = mem::read<uint32_t>(aim + 54);
  s.twist = mem::read<uint32_t>(aim + 106);
  s.world = mem::read<uint32_t>(joint + 10);
  s.from = mem::read<uint32_t>(end + 4);
  s.to = mem::read<uint32_t>(end + 66);
  const uint32_t zoom = mem::read<uint32_t>(aim + 122);
  // The callback's eye and point as three floats each; the camera's player read twice; the zoom the camera reads
  // the aim's last number (the pitch's copy, the twist and the zoom after the pitch); the line's ends as the
  // line writes them (each three floats) and a shot reads them; the players' slot 20 uModel's joint matrix.
  if (mem::read<uint8_t>(view + 45) != s.cam_eye + 4 || mem::read<uint8_t>(view + 51) != s.cam_eye + 8 ||
      mem::read<uint8_t>(view + 68) != s.cam_at + 4 || mem::read<uint8_t>(view + 74) != s.cam_at + 8 ||
      mem::read<uint32_t>(camera + 113) != s.cam_player || mem::read<uint32_t>(camera + 159) != zoom ||
      mem::read<uint32_t>(aim + 80) != s.pitch + 4 || s.twist != s.pitch + 8 || zoom != s.pitch + 12 ||
      mem::read<uint32_t>(end + 18) != s.from + 4 || mem::read<uint32_t>(end + 26) != s.from + 8 ||
      mem::read<uint32_t>(end + 42) != s.to + 4 || mem::read<uint32_t>(end + 58) != s.to + 8 ||
      mem::read<uint32_t>(end + 116) != s.to || mem::read<uint32_t>(shot + 40) != s.from ||
      mem::read<uint32_t>(shot + 48) != s.from + 4 || mem::read<uint32_t>(shot + 56) != s.from + 8 ||
      mem::read<uint32_t>(shot + 64) != s.to || mem::read<uint32_t>(shot + 72) != s.to + 8 ||
      mem::read<uintptr_t>(g_cls_player->vtable + 4 * 20) != joint)
    return false;
  // Each in the player (and the matrix in a model) as its class's size has room for.
  const uint32_t most = g_cls_player->size;
  if (!s.cam_player || s.cam_eye < 0x10 || s.cam_at < 0x10 || s.pitch + 12 > most || s.from + 12 > most ||
      s.to + 12 > most || s.world + 64 > most || lay.control + 4 > most)
    return false;
  char expect[2][24];
  const uintptr_t sites[2] = {view + 15, call + 100};
  for (int i = 0; i < 2; ++i)
    std::snprintf(expect[i], sizeof(expect[i]), "E8 %02X %02X %02X %02X", mem::read<uint8_t>(sites[i] + 1),
                  mem::read<uint8_t>(sites[i] + 2), mem::read<uint8_t>(sites[i] + 3), mem::read<uint8_t>(sites[i] + 4));
  if (!g_scope_call.prepare(sites[0], expect[0], call_to(sites[0], reinterpret_cast<const void*>(&hk_scope_view)),
                            "scope's view") ||
      !g_line_call.prepare(sites[1], expect[1], call_to(sites[1], reinterpret_cast<const void*>(&hk_gun_line)),
                           "gun's line")) {
    g_scope_call = g_line_call = mem::Patch{};
    return false;
  }
  g_sl = s;
  g_scope_view = reinterpret_cast<ScopeViewFn>(camera);
  g_gun_line = reinterpret_cast<GunLineFn>(line);
  logf("cheats: Steady aim: the scope's view (exe+0x%X: its camera exe+0x%X, the camera's player +0x%X, eye +0x%X, "
       "looking at +0x%X) and the gun's line (exe+0x%X: from +0x%X to +0x%X) re-pointed while on; the aim's pitch "
       "+0x%X and twist +0x%X, a model's world matrix +0x%X",
       static_cast<unsigned>(sites[0] - image::base()), static_cast<unsigned>(camera - image::base()), s.cam_player,
       s.cam_eye, s.cam_at, static_cast<unsigned>(sites[1] - image::base()), s.from, s.to, s.pitch, s.twist, s.world);
  return true;
}

// --- Infinite air: the air of a character under water held at its most --------------------------
// Under water a character's air is its combat gauge (a float, and its most): a dive's actions set
// the gauge's refill rate to sPlayer's dive rate (-8 by the game's own parameters) for a character
// this machine's player drives - the AI's partner gets 0, so it never runs short - and the gauge on
// screen (uIdOxygen) shows it while the character is under water (a bit of a flags word). In the
// stages with a dive (Leon's water, Ada's submarine) the player's frame runs the drowning: under
// water with the gauge at 0, sPlayer's set-health puts the character at 0 and its drowning plays
// (God mode cannot stop it: no hit, no damage). The gauge's add (hk_gauge_add, below: the frame's
// refill goes through it) turns a drain under water into a fill to its most for each character this
// machine drives: the air never runs down, and the drowning never comes. On land the gauge is
// Infinite stamina's.
struct AirLayout {
  uint32_t air = 0, most = 0;      // uPlayer: the air (the combat gauge) and its most (floats)
  uint32_t flags = 0, bit = 0;     // uPlayer: under water - this bit of this word
};
AirLayout g_air;

bool install_air(const game::Layout& lay) {
  const uintptr_t drown = game::sig_one("kSigDrown"), gauge = game::sig_one("kSigAirGauge");
  const uintptr_t test = drown ? game::call_target(drown + 23) : 0;
  const mt::Class* player = mt::find("uPlayer");
  // The test both call: `mov eax, [ecx + flags]; shr eax, bit; and al, 1; ret`.
  if (!drown || !gauge || !test || game::call_target(gauge + 12) != test || !player || !lay.hp ||
      !mem::readable(test, 12) || mem::read<uint16_t>(test) != 0x818B || mem::read<uint16_t>(test + 6) != 0xE8C1 ||
      mem::read<uint8_t>(test + 9) != 0x24 || mem::read<uint8_t>(test + 10) != 0x01 || mem::read<uint8_t>(test + 11) != 0xC3)
    return false;
  AirLayout a;
  a.flags = mem::read<uint32_t>(test + 2);
  a.bit = mem::read<uint8_t>(test + 8);
  a.air = mem::read<uint32_t>(drown + 38);
  a.most = mem::read<uint32_t>(gauge + 96);
  // The gauge on screen shows the air the drowning tests, over the float after it; with the characters' health.
  if (mem::read<uint32_t>(gauge + 88) != a.air || a.most != a.air + 4 || mem::read<uint32_t>(gauge + 34) != lay.hp ||
      a.bit > 31 || a.air <= lay.hp || a.most + 4 > player->size || a.flags + 4 > player->size)
    return false;
  g_air = a;
  logf("cheats: Infinite air: a character's air (the combat gauge) +0x%X, its most +0x%X; under water: bit %u of "
       "+0x%X; the drowning exe+0x%X (action 0x%X)",
       a.air, a.most, a.bit, a.flags, static_cast<unsigned>(drown - image::base()), mem::read<uint32_t>(drown + 87));
  return true;
}

// The combat gauge's add (uPlayer's slot 122: the frame's refill, at the rate each action sets - a dive's the drain;
// nothing while the pause after a hit lasts), for a character this machine drives: under water, with Infinite air, a
// drain is turned into a fill to its most - the air never runs down, so the game's drowning never comes; on land,
// with Infinite stamina, any add fills it to its most - a gauge the game emptied is full again at the next refill.
// The computer's and the network's characters: the game's own.
using GaugeAddFn = void(__thiscall*)(void*, float);
// The characters' dives (for the log): under water at their last add, or not - the add runs on land too.
CRITICAL_SECTION g_dive_lock;
struct Dive {
  uintptr_t obj = 0;
  bool under = false;
};
Dive g_dives[game::kMaxChars];
int g_dive_logged = 0;
// Whether `obj` has just gone under water (it was not at its last add), noted.
bool dive_begins(uintptr_t obj, bool under) {
  EnterCriticalSection(&g_dive_lock);
  Dive* d = nullptr;
  for (Dive& e : g_dives) d = !d && e.obj == obj ? &e : d;
  for (Dive& e : g_dives) d = !d && !e.obj ? &e : d;
  if (!d) d = &g_dives[0];
  const bool begins = d->obj == obj ? under && !d->under : under;
  d->obj = obj;
  d->under = under;
  LeaveCriticalSection(&g_dive_lock);
  return begins;
}
void __thiscall hk_gauge_add(void* self, float amount) {
  const auto p = reinterpret_cast<uintptr_t>(self);
  if (mem::read<uint32_t>(p + game::layout().control) == static_cast<uint32_t>(game::Control::kLocal)) {
    const bool under = (mem::read<uint32_t>(p + g_air.flags) & (1u << g_air.bit)) != 0;
    const float most = mem::read<float>(p + g_air.most), air = mem::read<float>(p + g_air.air);
    if (g_on[sw::kInfiniteAir] && g_ok[sw::kInfiniteAir]) {
      if (dive_begins(p, under) && g_dive_logged < 32) {
        ++g_dive_logged;
        logf("cheats: Infinite air: %s is under water - the air (%.0f of %.0f) held at its most",
             game::character_name(p), static_cast<double>(air), static_cast<double>(most));
      }
      if (under && amount < 0 && most > 0) amount = most - air;  // the drain made a fill to its most
    }
    // On land: whatever the add, the gauge filled to its most (a drain too; at its most, nothing).
    if (!under && g_on[sw::kInfiniteStamina] && g_ok[sw::kInfiniteStamina] && most > 0) amount = most - air;
  }
  reinterpret_cast<GaugeAddFn>(original(g_airadd, self, g_airadd_fn))(self, amount);
}

// --- Infinite stamina: the combat gauge held at its most on land ---------------------------------
// On land the combat gauge (the air under water's float: AirLayout) is what a move with a cost pays - melee, a quick
// shot: the blocks its record in sPlayer's action table gives. It holds 300 below its blocks and 100 a block above
// them (its most 800, 1100 or 1300: 5, 8 or 10 blocks, by the Combat Gauge Boost skills). The game's play of an
// action refuses a move while the blocks held (slot 124) are fewer than its cost, else pays it through uPlPlayer's
// spend (slot 123; thiscall (blocks), ret 4): 100 a block off, and the gauge emptied when 300 or less is left - none
// of it under the Training skill, the game's own way past. The player's frame refills the gauge through the add
// (slot 122). With the switch on, the spend takes nothing from a character this machine drives (as under Training),
// and on land each add fills the gauge to its most (hk_gauge_add, above): a gauge the game itself empties - going
// down, a hit in a versus mode - is full again at the frame's next refill once the character is up (two adds from
// 300 or less: the game's own add stops at 400 as a refill crosses 300). Left to the game: a gauge it keeps at 0
// itself (a scene's script turning it off, Piers with his mutated arm: the refill empties it, no add), the
// computer's and the network's characters, and Agent Hunt's creatures (uPlEnemy: a gauge, an add and spends of
// their own).
using SpendFn = void(__thiscall*)(void*, int);
uint32_t g_training = 0;  // the skill under which the game's spend takes nothing (for the log)
// The characters whose spend the mod has not taken (for the log: the first of each, 32 lines at most).
CRITICAL_SECTION g_spend_lock;
uintptr_t g_spent[8] = {};
unsigned g_spent_next = 0;
int g_spend_logged = 0;

void note_spend(uintptr_t p, int blocks) {
  EnterCriticalSection(&g_spend_lock);
  bool seen = false;
  for (const uintptr_t n : g_spent) seen = seen || n == p;
  const bool log = !seen && g_spend_logged < 32;
  if (log) {
    ++g_spend_logged;
    g_spent[g_spent_next++ % 8] = p;
  }
  LeaveCriticalSection(&g_spend_lock);
  if (log)
    logf("cheats: Infinite stamina: %s's combat gauge (%.0f of %.0f) - a spend of %d block(s) not taken",
         game::character_name(p), static_cast<double>(mem::read<float>(p + g_air.air)),
         static_cast<double>(mem::read<float>(p + g_air.most)), blocks);
}

// thiscall (blocks), as the game's spend: for a character this machine drives, nothing taken (the game's own way
// under its Training skill: a plain return).
void __thiscall hk_gauge_spend(void* self, int blocks) {
  const auto p = reinterpret_cast<uintptr_t>(self);
  if (blocks != 0 && g_on[sw::kInfiniteStamina] && g_ok[sw::kInfiniteStamina] &&
      mem::read<uint32_t>(p + game::layout().control) == static_cast<uint32_t>(game::Control::kLocal)) {
    note_spend(p, blocks);
    return;
  }
  reinterpret_cast<SpendFn>(original(g_spend, self, g_spend_fn))(self, blocks);
}

// The spend (its own signature), cross-checked against the add: uPlPlayer's own in the slot after the add's, asking
// the slot after its own (the blocks held) first, and taking from the gauge the add fills (the air the drowning
// tests), kept to the same most. Hooked in every player vtable holding it.
bool install_stamina(const mt::Class* plplayer) {
  const uintptr_t spend = game::sig_one("kSigGaugeSpend");
  if (!spend || !g_cls_player || !plplayer || !plplayer->vtable || g_airadd_slot < 0) return false;
  const int slot = vthook::slot_of(plplayer->vtable, spend, 300);
  if (slot < 0 || slot != g_airadd_slot + 1 || mem::read<uint32_t>(spend + 50) != 4u * static_cast<uint32_t>(slot + 1) ||
      mem::read<uint32_t>(spend + 95) != g_air.air || mem::read<uint32_t>(spend + 111) != g_air.air ||
      mem::read<uint32_t>(spend + 123) != g_air.air || mem::read<uint32_t>(spend + 131) != g_air.most)
    return false;
  const int n = hook_vtables(g_cls_player, slot, spend, false, reinterpret_cast<void*>(&hk_gauge_spend), &g_spend);
  if (n <= 0) return false;
  g_spend_fn = spend;
  g_spend_slot = slot;
  g_training = mem::read<uint8_t>(spend + 31);
  logf("cheats: Infinite stamina: the combat gauge's spend (slot %d, exe+0x%X; the game's own takes nothing under "
       "skill 0x%X) hooked in %d vtables - nothing spent; on land its add (slot %d) fills it",
       slot, static_cast<unsigned>(spend - image::base()), g_training, n, g_airadd_slot);
  return true;
}

// --- Automatic QTEs ------------------------------------------------------------------------------
// Every button prompt - a door's, an item's, a grab's, a crank's - is a request of
// sActionButton's, which each player's manager asks about every frame through one check
// (thiscall (pad, player) on the manager's unit holding the request): "pressed?". That one
// call is re-pointed at hk_qte_check, which asks the game's check first and, when it says
// no, answers yes for a QTE of a character this machine drives - not drawn. A press in
// time with a good window waits for its window and is marked good (as a press in it is).
// On top, the scenes that go automatic for the game's own option (mExOptAutoActBtn: the
// Amateur difficulty's path) read it as on while the switch is (their tests patched).
struct QteLayout {
  uint32_t request = 0;    // ButtonUnit: its request (cActionButton)
  uint32_t drawn = 0;      // ButtonUnit: drawn this frame (a byte)
  uint32_t kind = 0;       // ButtonUnit: the kind (a dword)
  uint32_t progress = 0;   // ButtonUnit: a press in time's progress, in % (a float)
  uint32_t window = 0;     // ButtonUnit: the start of its good window, in % (a float)
  uint32_t answered = 0;   // cActionButton: answered this frame (a byte)
  uint32_t failed = 0;     // cActionButton: the players who failed it (a bit each)
  uint32_t flags = 0;      // cActionButton: its flags ...
  uint32_t dodge_bit = 0;  // ... the bit of a dodge made as a plain press
  uint32_t good = 0;       // cActionButton: pressed in its good window (a byte)
  uint32_t blocked = 0;    // sPlayer: input held back, a bit for each player (two bytes)
  float full = 0;          // 100 (%)
};
QteLayout g_qte;
using QteCheckFn = bool(__thiscall*)(void*, void*, int);
QteCheckFn g_qte_check = nullptr;
mem::Patch g_qte_call;                  // the managers' call of the check
std::vector<mem::Patch> g_auto_scenes;  // the option's tests in the scenes
int g_qte_logged = 0;

constexpr uint32_t kKindPress = 0x01, kKindTimed = 0x0C;

// A QTE: two buttons at once, a mash, a counter's double press, the stick waggled or
// turned, a hold for a time, a press in time, the timing rings (kinds 4 to 0x13 and their
// failing forms 0x17 to 0x19), and a dodge made as a plain press (kind 1 with its bit).
// A plain press, a hold and a let-go (1 to 3) are a context action's - a door, an item,
// cover, a melee move, helping the partner -, left to the player; kind 0x14 is the AI's.
bool is_qte(uint32_t kind, uint32_t flags) {
  return (kind >= 0x04 && kind <= 0x13) || (kind >= 0x17 && kind <= 0x19) ||
         (kind == kKindPress && (flags & g_qte.dodge_bit));
}

bool __thiscall hk_qte_check(void* self, void* pad, int player) {
  const auto unit = reinterpret_cast<uintptr_t>(self);
  const float before = mem::read<float>(unit + g_qte.progress);
  const bool pressed = g_qte_check(self, pad, player);
  if (pressed || !g_on[sw::kAutoQte] || player < 0 || player >= game::kMaxChars) return pressed;
  const uintptr_t req = mem::read<uintptr_t>(unit + g_qte.request);
  if (!req || mem::read<uint8_t>(req + g_qte.answered) || (mem::read<uint32_t>(req + g_qte.failed) & (1u << player)))
    return false;
  const uintptr_t chara = game::character_at(player);
  if (!chara || mem::read<uint32_t>(chara + game::layout().control) != static_cast<uint32_t>(game::Control::kLocal) ||
      game::input_blocked(player, g_qte.blocked))
    return false;
  const uint32_t kind = mem::read<uint32_t>(unit + g_qte.kind);
  if (!is_qte(kind, mem::read<uint32_t>(req + g_qte.flags))) return false;
  *reinterpret_cast<uint8_t*>(unit + g_qte.drawn) = 0;
  if (kind == kKindTimed) {
    // Its good window from `window` % of its time: waited for - unless the next frame
    // would run it out.
    const float p = mem::read<float>(unit + g_qte.progress), w = mem::read<float>(unit + g_qte.window);
    if (w < g_qte.full && p < w && p + (p - before) < g_qte.full) return false;
    *reinterpret_cast<uint8_t*>(req + g_qte.good) = 1;
  }
  if (g_qte_logged < 24) {
    ++g_qte_logged;
    logf("cheats: a QTE (kind 0x%X) answered for character %d", kind, player);
  }
  return true;
}

// Each test of the option in the scenes: `cmp [option], 0` read as never 0 (`cmp ..,-1`),
// or `cmp [option], reg` (the register 0 there) as `cmp ..,esp` (never 0 nor 1); each
// followed by a je/jne.
void prepare_auto_scenes(const game::Layout& lay) {
  const std::vector<uintptr_t> imm = game::sig_all("kSigAutoActBtn"), reg = game::sig_all("kSigAutoActBtnReg");
  if (imm.empty()) return;
  const uint32_t global = mem::read<uint32_t>(imm[0] + 1), option = mem::read<uint32_t>(imm[0] + 17);
  const auto same = [&](uintptr_t m) {
    return mem::read<uint32_t>(m + 1) == global && mem::read<uint32_t>(m + 17) == option &&
           mem::read<uint8_t>(m + 7) == lay.profile && mem::read<uint32_t>(m + 10) == lay.profile_size;
  };
  // The option lies in the profile's cSystemInfo, with its skill points.
  const uint32_t gap = option > lay.skill_points ? option - lay.skill_points : lay.skill_points - option;
  if (!lay.profile_size || gap >= lay.profile_size) return;
  std::vector<mem::Patch> out;
  for (const uintptr_t m : imm) {
    mem::Patch p;
    if (!same(m) || !p.prepare(m + 21, "00", {0xFF}, "an automatic scene's test")) return;
    out.push_back(p);
  }
  for (const uintptr_t m : reg) {
    const uint8_t modrm = mem::read<uint8_t>(m + 15);
    mem::Patch p;
    if (!same(m) || (modrm != 0xAC && modrm != 0x9C) ||
        !p.prepare(m + 15, modrm == 0xAC ? "AC" : "9C", {0xA4}, "an automatic scene's test"))
      return;
    out.push_back(p);
  }
  g_auto_scenes = out;
}

bool install_qte(const game::Layout& lay) {
  const uintptr_t call = game::sig_one("kSigQteCall"), check = game::sig_one("kSigQteCheck"),
                  unit_set = game::sig_one("kSigQteUnitSet"), timed = game::sig_one("kSigQteTimed"),
                  kinds = game::sig_one("kSigQteKinds"), blocked = game::sig_one("kSigInputBlocked");
  if (!call || !check || !unit_set || !timed || !kinds || !blocked || game::call_target(call + 20) != check ||
      game::call_target(timed + 63) != blocked)
    return false;
  QteLayout q;
  q.request = mem::read<uint8_t>(check + 5);
  q.drawn = mem::read<uint8_t>(check + 8);
  q.answered = mem::read<uint8_t>(check + 27);
  q.kind = mem::read<uint8_t>(unit_set + 42);
  q.flags = mem::read<uint8_t>(unit_set + 101);
  q.dodge_bit = mem::read<uint8_t>(unit_set + 102);
  q.progress = mem::read<uint8_t>(timed + 37);
  q.failed = mem::read<uint8_t>(timed + 104);
  q.window = mem::read<uint8_t>(timed + 116);
  q.good = mem::read<uint8_t>(timed + 209);
  q.blocked = mem::read<uint32_t>(blocked + 38);
  const uintptr_t full_at = mem::read<uint32_t>(timed + 4);
  // The unit's request, kind and drawn byte as every reader has them (drawn the byte after
  // in-use, which the managers test).
  if (mem::read<uint8_t>(call + 31) != q.request || mem::read<uint8_t>(unit_set + 13) != q.request ||
      mem::read<uint8_t>(timed + 13) != q.request || mem::read<uint8_t>(kinds + 2) != q.kind ||
      mem::read<uint8_t>(call + 6) + 1u != q.drawn || mem::read<uint8_t>(kinds + 5) != 0x19 ||
      !image::whole().contains(full_at) || !mem::read_safe(full_at, &q.full) || q.full != 100.0f || !q.dodge_bit ||
      !q.blocked || !lay.players)
    return false;
  char expect[24];
  std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", mem::read<uint8_t>(call + 21),
                mem::read<uint8_t>(call + 22), mem::read<uint8_t>(call + 23), mem::read<uint8_t>(call + 24));
  if (!g_qte_call.prepare(call + 20, expect, call_to(call + 20, reinterpret_cast<const void*>(&hk_qte_check)),
                          "QTE check"))
    return false;
  g_qte = q;
  g_qte_check = reinterpret_cast<QteCheckFn>(check);
  prepare_auto_scenes(lay);
  logf("cheats: Automatic QTEs: the managers' check (exe+0x%X) re-pointed while on; %d automatic scenes' tests%s",
       static_cast<unsigned>(call + 20 - image::base()), static_cast<int>(g_auto_scenes.size()),
       g_auto_scenes.empty() ? " (not as known: left alone)" : "");
  return true;
}

// --- the chapter's numbers: Freeze play time, Deaths don't count, Misses don't count ----------
// The clock: its add (sInfoMan's frame adds the frame's time to every character's) returns
// at once while Freeze play time is on - nothing else runs it on. A death: the Game Over's
// count pushes 0 for 1 while Deaths don't count is on. A miss: a shot is counted (sInfoMan's
// counter add, its one call for the shots) only as it hits - for a character this machine
// plays, the shots' call comes to hk_count_shot, which counts nothing, and the two calls for
// the hits (a shot's, once a shot, and a blow's) to hk_count_hit, which counts the hit and then
// holds the shots at the hits; as the switch goes on the shots are put at the hits once.
mem::Patch g_clock_add, g_death;
using CountFn = void(__thiscall*)(void*, int, int, int);
CountFn g_count = nullptr;     // sInfoMan's counter add (thiscall (character, counter, amount))
mem::Patch g_count_calls[3];   // the shots' call, then the hits' two
bool g_count_ok = false;

uintptr_t block_of(uintptr_t info, int chara) { return info + static_cast<uint32_t>(chara) * game::layout().block; }
uint16_t* counter(uintptr_t info, int chara, int k) {
  return reinterpret_cast<uint16_t*>(block_of(info, chara) + game::layout().counters + 2 * static_cast<uint32_t>(k));
}
float* chapter_clock(uintptr_t info, int chara) {
  return reinterpret_cast<float*>(block_of(info, chara) + game::layout().clock);
}

// A character of sPlayer's this machine drives (a counter's character number).
bool driven_here(int chara) {
  const uintptr_t c = chara >= 0 && chara < game::kMaxChars ? game::character_at(chara) : 0;
  return c && mem::read<uint32_t>(c + game::layout().control) == static_cast<uint32_t>(game::Control::kLocal);
}
// The shots of a character held at its hits (in the profile in use's chapter numbers).
void shots_at_hits(int chara) {
  const game::Layout& lay = game::layout();
  const uintptr_t info = game::game_info_at();
  if (!info || chara < 0 || chara >= lay.nblocks || lay.c_shots < 0 || lay.c_hits < 0) return;
  uint16_t* shots = counter(info, chara, lay.c_shots);
  const uint16_t hits = *counter(info, chara, lay.c_hits);
  if (*shots != hits) *shots = hits;
}
void __thiscall hk_count_shot(void* info, int chara, int which, int amount) {
  if (g_on[sw::kNoMissCount] && driven_here(chara)) return;  // a shot counts as it hits
  g_count(info, chara, which, amount);
}
void __thiscall hk_count_hit(void* info, int chara, int which, int amount) {
  g_count(info, chara, which, amount);
  if (g_on[sw::kNoMissCount] && driven_here(chara)) shots_at_hits(chara);
}

// The three calls - each of sInfoMan's counter add (the one of its two of that shape whose own call is the chapter's
// counter add), the shots' with the shots' number, the hits' with the hits'; the hits' after the same shooter's
// record as the shots', its once-a-shot flag one byte.
bool install_counts(const game::Layout& lay) {
  const uintptr_t shot = game::sig_one("kSigShotCount"), hit = game::sig_one("kSigHitCount"),
                  blow = game::sig_one("kSigHitCountBlow"), chapter_add = game::sig_one("kSigChapterCounterAdd");
  const std::vector<uintptr_t> adds = game::sig_all("kSigInfoCount");
  if (!shot || !hit || !blow || !chapter_add || lay.c_shots < 0 || lay.c_hits < 0) return false;
  const uintptr_t add = game::call_target(shot + 30);
  bool known = false;
  for (const uintptr_t a : adds) known = known || a == add;
  if (!add || !known || game::call_target(hit + 35) != add || game::call_target(blow + 15) != add ||
      game::call_target(add + 59) != chapter_add || mem::read<uint8_t>(add + 43) != lay.profile ||
      mem::read<uint32_t>(add + 46) != lay.game_info_size || mem::read<uint32_t>(add + 55) != lay.game_info ||
      mem::read<uint8_t>(shot + 26) != static_cast<uint32_t>(lay.c_shots) ||
      mem::read<uint8_t>(hit + 31) != static_cast<uint32_t>(lay.c_hits) ||
      mem::read<uint8_t>(blow + 13) != static_cast<uint32_t>(lay.c_hits) ||
      game::call_target(shot) != game::call_target(hit) || mem::read<uint32_t>(hit + 21) != mem::read<uint32_t>(hit + 42))
    return false;
  const uintptr_t sites[3] = {shot + 30, hit + 35, blow + 15};
  const void* hooks[3] = {reinterpret_cast<const void*>(&hk_count_shot), reinterpret_cast<const void*>(&hk_count_hit),
                          reinterpret_cast<const void*>(&hk_count_hit)};
  for (int i = 0; i < 3; ++i) {
    char expect[24];
    std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", mem::read<uint8_t>(sites[i] + 1),
                  mem::read<uint8_t>(sites[i] + 2), mem::read<uint8_t>(sites[i] + 3), mem::read<uint8_t>(sites[i] + 4));
    if (!g_count_calls[i].prepare(sites[i], expect, call_to(sites[i], hooks[i]), i ? "hits' count" : "shots' count")) {
      for (mem::Patch& p : g_count_calls) p = mem::Patch{};
      return false;
    }
  }
  g_count = reinterpret_cast<CountFn>(add);
  logf("cheats: Misses don't count: the shots' count (exe+0x%X) and the hits' two (exe+0x%X, exe+0x%X) of sInfoMan's "
       "counter add (exe+0x%X) re-pointed while on",
       static_cast<unsigned>(sites[0] - image::base()), static_cast<unsigned>(sites[1] - image::base()),
       static_cast<unsigned>(sites[2] - image::base()), static_cast<unsigned>(add - image::base()));
  return true;
}

bool install_results(const game::Layout& lay) {
  if (!lay.clock || lay.c_deaths < 0) return false;
  const uintptr_t loop = game::sig_one("kSigChapterClockLoop"), death = game::sig_one("kSigDeathCounted");
  const uintptr_t add = loop ? game::call_target(loop + 29) : 0;
  return add && death && g_clock_add.prepare(add, "8B 44 24", {0xC2, 0x08, 0x00}, "chapter clock") &&
         g_death.prepare(death + 24, "6A 01", {0x6A, 0x00}, "death count");
}

// --- Freeze countdown timer, and the countdown's editor ---------------------------------------
// The modes' match clock (sGameMan's, the Mercenaries' and No Mercy's): its run-down's
// `subss` NOPed while the match runs on this machine, offline - the time bonuses add to it
// elsewhere, and still do. A campaign's countdown: a scenario timer, which the CountDown
// order shows each frame it is up (the SmallTimer order a gauge over the players): one of
// the stage's 32 (read through the stage's get) or of the FSM's own 32 (cFSMOrder's, read
// in place: stage 851's countdowns, which the mod did not see until 2026-11-13). The
// displays' reads come to the mod - the get's two calls re-pointed, the in-place read
// rewritten into a call - which notes the timer and, while the switch is on, holds it where
// it was before it reads on. The CountDown's timer is held whichever way it runs (stage
// 1003's counts up to its end); a gauge's only counting down (every gauge in the game counts
// an action's progress up, its end the action done: never held).
mem::Patch g_match_clock;
using StageGetFn = float(__thiscall*)(void*, uint32_t);
StageGetFn g_stage_get = nullptr;
std::vector<mem::Patch> g_stage_reads;  // always in: they only note what is read
mem::Patch g_own_read;                  // always in, as those
const mt::Class* g_cls_fsm = nullptr;   // cFSMOrder: whose own timers the CountDown reads in place
enum class Timers : uint8_t { kStage, kOwn };
struct Shown {
  uintptr_t obj = 0;  // the stage, or the FSM
  Timers kind = Timers::kStage;
  uint32_t n = 0;
  uint32_t frame = 0;
  bool seen = false;
};
Shown g_shown;             // main thread
uint32_t g_play_frames = 0;  // the frames the game ran unpaused (the tick's count)
struct Hold {
  uintptr_t obj = 0;
  Timers kind = Timers::kStage;
  uint32_t n = 0;
  float v = 0;
  bool down = true;
  uint32_t frame = 0;  // its display's last read
  bool on = false;
};
Hold g_hold;
int g_reads_logged = 0;
bool g_online = false;  // a character online (the tick's): no hold - each machine runs its own

// A kind of timers' words: running bits, counting-down bits, the floats, how many.
struct TimerWords {
  uint32_t running, down, timers;
  int n;
};
TimerWords words_of(Timers kind) {
  const game::Layout& l = game::layout();
  return kind == Timers::kOwn ? TimerWords{l.own_running, l.own_down, l.own_timers, l.nown}
                              : TimerWords{l.timers_running, l.timers_down, l.timers, l.ntimers};
}

// The timer a display reads held where it was while the switch is on: running and counting down (or, for the
// CountDown, up). Time the game gives back since - a raise of one counting down, a drop of one counting up - is
// taken up as the new hold. A hold lapses when its display went unread for kHoldLapse frames the game ran (hidden,
// then shown again on a timer started afresh; a display read every frame, a few frames' grace either way), and only
// its own display lets it go (a gauge read beside it does not).
constexpr uint32_t kHoldLapse = 30;
void hold_timer(uintptr_t obj, Timers kind, uint32_t n, bool either_way) {
  const TimerWords w = words_of(kind);
  const bool mine = g_hold.on && g_hold.obj == obj && g_hold.kind == kind && g_hold.n == n;
  const uint32_t bit = n < 32 ? 1u << n : 0;
  const bool running = n < static_cast<uint32_t>(w.n) && (mem::read<uint32_t>(obj + w.running) & bit);
  const bool down = (mem::read<uint32_t>(obj + w.down) & bit) != 0;
  if (!running || (!down && !either_way)) {
    if (mine) g_hold.on = false;
    return;
  }
  auto* t = reinterpret_cast<float*>(obj + w.timers + 4 * n);
  const bool given = down ? *t > g_hold.v : *t < g_hold.v;
  if (!mine || g_hold.down != down || g_hold.frame + kHoldLapse < g_play_frames || given) {
    g_hold = Hold{obj, kind, n, *t, down, g_play_frames, true};
  } else {
    *t = g_hold.v;
    g_hold.frame = g_play_frames;
  }
}

// A display's read of a timer: noted (the panel's countdown, the log), held while the switch is on.
void read_timer(uintptr_t obj, Timers kind, uint32_t n, bool either_way) {
  if (g_reads_logged < 8 && (!g_shown.seen || g_shown.obj != obj || g_shown.kind != kind || g_shown.n != n)) {
    ++g_reads_logged;
    if (kind == Timers::kOwn) {
      const char* name = mt::class_name_of(obj);
      logf("cheats: a countdown on screen shows its FSM's own timer %u (%s %08X)", n, name ? name : "?",
           static_cast<unsigned>(obj));
    } else {
      logf("cheats: a countdown on screen shows the stage's timer %u (stage %08X)", n, static_cast<unsigned>(obj));
    }
  }
  g_shown = Shown{obj, kind, n, g_play_frames, true};
  if (g_on[sw::kFreezeCountdown] && g_ok[sw::kFreezeCountdown] && !g_online) hold_timer(obj, kind, n, either_way);
  else g_hold.on = false;
}

// The CountDown's read of a stage timer (kSigCountDown +126) and the SmallTimer's (kSigSmallTimerRead +39).
float __thiscall hk_countdown_stage(void* stage, uint32_t n) {
  read_timer(reinterpret_cast<uintptr_t>(stage), Timers::kStage, n, true);
  return g_stage_get(stage, n);
}
float __thiscall hk_gauge_stage(void* stage, uint32_t n) {
  read_timer(reinterpret_cast<uintptr_t>(stage), Timers::kStage, n, false);
  return g_stage_get(stage, n);
}

// The CountDown's read of its FSM's own timer: the game reads it in place, unchecked (n is the order's byte), and so
// does this.
float __thiscall hk_countdown_own(void* fsm, uint32_t n) {
  const auto at = reinterpret_cast<uintptr_t>(fsm);
  read_timer(at, Timers::kOwn, n, true);
  return mem::read<float>(at + game::layout().own_timers + 4 * n);
}

// That read's 19 bytes (movzx eax,[esi+7]; movss xmm0,[edx+eax*4+timers]; movss [esp+14h],xmm0) rewritten as the
// stage timer's read beside it goes: the number pushed, the FSM (edx) as `this`, a call of the mod's read (thiscall,
// ret 4, its float on the x87 stack), stored where the game keeps it. After it the order reloads all it uses.
std::vector<uint8_t> own_read_bytes(uintptr_t site, const void* fn) {
  std::vector<uint8_t> b = {0x0F, 0xB6, 0x46, 0x07,     // movzx eax, byte [esi+7] (the game's own)
                            0x50,                       // push eax
                            0x8B, 0xCA,                 // mov ecx, edx
                            0xE8, 0, 0, 0, 0,           // call fn
                            0xD9, 0x5C, 0x24, 0x14,     // fstp dword [esp+14h]
                            0x90, 0x90, 0x90};
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(fn) - (site + 12));
  std::memcpy(b.data() + 8, &rel, 4);
  return b;
}

bool install_countdown(const game::Layout& lay) {
  bool any = false;
  const uintptr_t clock = game::sig_one("kSigMercClock");
  if (lay.match_clock && clock && g_match_clock.prepare(clock + 17, "F3 0F 5C C1", mem::nops(4), "match clock")) any = true;
  const uintptr_t get = game::sig_one("kSigStageGet");
  if (lay.timers && get) {
    std::vector<mem::Patch> reads;
    const struct {
      uintptr_t site;
      const void* hook;
    } calls[] = {{game::sig_one("kSigCountDown") + 126, reinterpret_cast<const void*>(&hk_countdown_stage)},
                 {game::sig_one("kSigSmallTimerRead") + 39, reinterpret_cast<const void*>(&hk_gauge_stage)}};
    for (const auto& c : calls) {
      if (game::call_target(c.site) != get) continue;
      char expect[24];
      std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", mem::read<uint8_t>(c.site + 1),
                    mem::read<uint8_t>(c.site + 2), mem::read<uint8_t>(c.site + 3), mem::read<uint8_t>(c.site + 4));
      mem::Patch p;
      if (p.prepare(c.site, expect, call_to(c.site, c.hook), "countdown's read")) reads.push_back(p);
    }
    if (reads.size() == 2) {
      g_stage_get = reinterpret_cast<StageGetFn>(get);
      g_stage_reads = reads;
      for (mem::Patch& p : g_stage_reads)
        if (!p.apply()) logf("cheats: a countdown's read could not be re-pointed");
      any = true;
    }
  }
  const uintptr_t own = game::sig_one("kSigCountDownOwn");
  g_cls_fsm = mt::find("cFSMOrder");
  if (lay.own_timers && own && g_cls_fsm) {
    const uint32_t t = lay.own_timers;
    char expect[64];
    std::snprintf(expect, sizeof(expect), "0F B6 46 07 F3 0F 10 84 82 %02X %02X %02X %02X F3 0F 11 44 24 14", t & 0xFF,
                  (t >> 8) & 0xFF, (t >> 16) & 0xFF, t >> 24);
    if (g_own_read.prepare(own, expect, own_read_bytes(own, reinterpret_cast<const void*>(&hk_countdown_own)),
                           "countdown's own read") &&
        g_own_read.apply())
      any = true;
    else
      logf("cheats: the countdown's read of an FSM's own timer could not be re-pointed");
  }
  return any;
}

// The countdown on screen this frame: the match clock (offline, on the machine running
// the match, while it plays) or the timer a display read (in the last frame the game
// ran), running and counting down - an FSM's still the FSM it was.
struct Countdown {
  bool match = false;
  float* t = nullptr;  // the timer's
};
Countdown countdown_now(const game::Run& run) {
  const game::Layout& l = game::layout();
  Countdown c;
  if (run.online) return c;
  const uintptr_t gm = game::sgameman();
  if (gm && l.match_clock && run.mode_known && run.mode != game::kModeCampaign) {
    c.match = mem::read<int32_t>(gm + l.match_host) == mem::read<int32_t>(gm + l.match_me) &&
              mem::read<int32_t>(gm + l.match_step) == 1 && mem::read<int32_t>(gm + l.match_end) == 0;
    return c;
  }
  const TimerWords w = words_of(g_shown.kind);
  if (!g_shown.seen || g_shown.frame + 1 < g_play_frames || g_shown.n >= static_cast<uint32_t>(w.n)) return c;
  if (g_shown.kind == Timers::kOwn && !mt::is_a(g_shown.obj, g_cls_fsm)) return c;
  const uint32_t bit = 1u << g_shown.n;
  if ((mem::read<uint32_t>(g_shown.obj + w.running) & bit) && (mem::read<uint32_t>(g_shown.obj + w.down) & bit))
    c.t = reinterpret_cast<float*>(g_shown.obj + w.timers + 4 * g_shown.n);
  return c;
}

// The edits waiting for the pause (a counter's by character).
CRITICAL_SECTION g_edit_lock;
bool g_lock_ready = false;
struct Pending {
  bool set = false;
  double value = 0;
};
constexpr int kEdits = static_cast<int>(Edit::kCount);
Pending g_pending[kEdits][game::kMaxChars];
Status g_status;

}  // namespace

void init() {
  InitializeCriticalSection(&g_edit_lock);
  InitializeCriticalSection(&g_armed_lock);
  InitializeCriticalSection(&g_dive_lock);
  InitializeCriticalSection(&g_spend_lock);
  g_lock_ready = true;
  for (int k = 0; k < sw::kCount; ++k) {
    g_on[k] = config::get().on[k] ? 1 : 0;
    g_ok[k] = false;
    g_why[k] = "the game is not ready yet";
  }
}

bool install() {
  const game::Layout& lay = game::layout();
  g_cls_player = mt::find("uPlayer");
  const mt::Class* plplayer = mt::find("uPlPlayer");
  const mt::Class* enemy = mt::find("uEnemy");
  g_sub_fn = game::sig_one("kSigSubLife");
  g_apply_fn = game::sig_one("kSigPlayerApply");
  g_eapply_fn = game::sig_one("kSigEnemyApply");
  g_hit_fn = plplayer && plplayer->vtable ? mem::read<uintptr_t>(plplayer->vtable + 4 * game::kPlayerHitSlot) : 0;

  // God mode and the partner's: the players' subtract-health, hit and take-damage.
  if (g_cls_player && g_sub_fn && g_apply_fn && g_hit_fn) {
    const int sub = hook_vtables(g_cls_player, game::kSubLifeSlot, g_sub_fn, false, reinterpret_cast<void*>(&hk_player_sub), &g_sub);
    const int hit = hook_vtables(g_cls_player, game::kPlayerHitSlot, g_hit_fn, false, reinterpret_cast<void*>(&hk_player_hit), &g_hit);
    const int apply = hook_vtables(g_cls_player, game::kPlayerApplySlot, g_apply_fn, false,
                                   reinterpret_cast<void*>(&hk_player_apply), &g_apply);
    // The health add (a heal, getting up): its health and its most as subtract-health has them.
    const uintptr_t add = game::sig_one("kSigPlayerHealthAdd");
    const int adds = add && mem::read<uint32_t>(add + 11) == lay.hp && mem::read<uint32_t>(add + 22) == lay.hp &&
                             mem::read<uint32_t>(add + 29) == lay.hp_max
                         ? hook_player_fn(add, reinterpret_cast<void*>(&hk_player_add), &g_add, &g_add_slot, &g_add_fn)
                         : 0;
    logf("cheats: the players' damage hooked in %d vtables (subtract-health), %d (hit), %d (take-damage), %d (the "
         "health add, slot %d)",
         sub, hit, apply, adds, g_add_slot);
    if (sub && apply) {
      g_ok[sw::kGodMode] = g_ok[sw::kPartnerGod] = true;
      g_rides_ok = install_rides(lay);
      if (!g_rides_ok) logf("cheats: God mode: the rides' subtract-health not found - a ride takes its damage");
    } else {
      unavailable(sw::kGodMode, "The players' damage could not be hooked (see the log).");
      unavailable(sw::kPartnerGod, "The players' damage could not be hooked (see the log).");
    }
  } else {
    unavailable(sw::kGodMode, "The players' damage was not found in the game (see the log).");
    unavailable(sw::kPartnerGod, "The players' damage was not found in the game (see the log).");
  }

  // One hit kills: the enemies' take-damage (and the overrides that call it first).
  if (enemy && g_eapply_fn && g_cls_player) {
    const int n = hook_vtables(enemy, game::kEnemyApplySlot, g_eapply_fn, true, reinterpret_cast<void*>(&hk_enemy_apply),
                               &g_eapply);
    logf("cheats: the enemies' take-damage hooked in %d vtables", n);
    if (n) g_ok[sw::kOneHitKills] = true;
    else unavailable(sw::kOneHitKills, "The enemies' damage could not be hooked (see the log).");
    if (n) install_big(enemy);
    g_bombs_ok = n && install_bombs(lay);
    if (n && !g_bombs_ok) logf("cheats: One hit kills: the room bombs' hit checks not found - they take their usual hits");
  } else {
    unavailable(sw::kOneHitKills, "The enemies' damage was not found in the game (see the log).");
  }

  // Infinite ammo: the game's own bit - with the tablets' uses kept out of it.
  const uintptr_t use = game::sig_one("kSigAmmoUse"), tablet = game::sig_one("kSigTabletUse");
  if (lay.ammo_flags && use && tablet && game::call_target(tablet + 21) == use) {
    g_use = reinterpret_cast<UseFn>(use);
    find_tablet_calls(use, mem::read<uint32_t>(tablet + 1), game::call_target(tablet + 7));
  }
  g_ammo_ok = lay.ammo_flags != 0 && g_use && !g_tablet_calls.empty();
  // The reset (a Continue, a stage's start): the flags' reset it calls writes the word with the bit whole
  // (`mov [esi+word], ebx`); the slot in hand as the inventory has it.
  const uintptr_t reset = game::sig_one("kSigPlayerReset");
  const uintptr_t flags_reset = reset ? game::call_target(reset + 45) : 0;
  bool writes_word = false;
  if (flags_reset && image::text().contains(flags_reset) && mem::readable(flags_reset, 0x300))
    for (uintptr_t a = flags_reset; a < flags_reset + 0x300 && !writes_word; ++a)
      writes_word = mem::read<uint16_t>(a) == 0x9E89 && mem::read<uint32_t>(a + 2) == lay.ammo_flags;
  const int resets = g_ammo_ok && writes_word && mem::read<uint32_t>(reset + 69) == lay.in_hand
                         ? hook_player_fn(reset, reinterpret_cast<void*>(&hk_player_reset), &g_reset, &g_reset_slot,
                                          &g_reset_fn)
                         : 0;
  g_ammo_ok = g_ammo_ok && resets > 0;
  if (g_ammo_ok) {
    g_ok[sw::kInfiniteAmmo] = true;
    logf("cheats: Infinite ammo: the game's own bit, the health tablets (item 0x%X) used up as ever at %d call(s); "
         "the bit put back after the players' reset (slot %d, %d vtables)",
         mem::read<uint32_t>(tablet + 1), static_cast<int>(g_tablet_calls.size()), g_reset_slot, resets);
    g_missiles_ok = install_missiles();
    if (!g_missiles_ok) logf("cheats: Infinite ammo: the helicopter's missiles not found - used up as the game has it");
  } else {
    unavailable(sw::kInfiniteAmmo, "The game's own infinite ammo was not found (see the log).");
  }

  // Infinite skill points: the skill screen's two subtractions of a price.
  const uintptr_t buy = game::sig_one("kSigSkillBuy"), level = game::sig_one("kSigSkillLevel");
  const bool buy_ok = buy && g_buy.prepare(buy + 21, "29 8E ?? ?? ?? ??", mem::nops(6), "skill purchase");
  const bool level_ok = level && g_level.prepare(level + 9, "29 86 ?? ?? ?? ??", mem::nops(6), "skill level purchase");
  // Both subtract from the same field of the screen's (its copy of the points).
  if (buy_ok && level_ok && mem::read<uint32_t>(buy + 23) == mem::read<uint32_t>(level + 11) && lay.skill_points) {
    g_ok[sw::kInfiniteSkillPoints] = true;
    logf("cheats: the skill screen's purchases found (exe+0x%X, exe+0x%X)",
         static_cast<unsigned>(buy + 21 - image::base()), static_cast<unsigned>(level + 9 - image::base()));
  } else {
    g_buy = mem::Patch{};
    g_level = mem::Patch{};
    unavailable(sw::kInfiniteSkillPoints, "The skill screen's purchases were not found (see the log).");
  }

  // Steady aim: the sway's way out (its `jne` lands just past it) and the kick's (the
  // `je` past the kick lands on the frame's own path for no shot; the shot's flags are the
  // word with the infinite-ammo bit).
  const uintptr_t sway = game::sig_one("kSigAimSway"), kick = game::sig_one("kSigAimKick");
  const bool sway_ok = sway && mem::read<uint8_t>(sway + 18) == 46 - 19 &&
                       g_sway.prepare(sway + 17, "75 ??", {0x90, 0x90}, "aim sway");
  const bool kick_ok = kick && lay.ammo_flags && mem::read<uint32_t>(kick + 2) == lay.ammo_flags &&
                       g_kick.prepare(kick + 32, "0F 84", {0x90, 0xE9}, "aim kick");
  if (sway_ok && kick_ok) {
    g_ok[sw::kSteadyAim] = true;
    logf("cheats: Steady aim: the sway's way out exe+0x%X, the kick's exe+0x%X",
         static_cast<unsigned>(sway + 17 - image::base()), static_cast<unsigned>(kick + 32 - image::base()));
    g_scope_ok = install_scope(lay, sway);
    if (!g_scope_ok)
      logf("cheats: Steady aim: the scope's view and the gun's line not as known - a scope's view is left to the game");
  } else {
    g_sway = mem::Patch{};
    g_kick = mem::Patch{};
    unavailable(sw::kSteadyAim, "The aim's sway and kick were not found in the game (see the log).");
  }

  // Infinite air: the combat gauge's add, its gauge the one the drowning tests.
  const uintptr_t air_add = game::sig_one("kSigAirAdd");
  const int air_adds = install_air(lay) && air_add && mem::read<uint32_t>(air_add + 27) == g_air.air &&
                               mem::read<uint32_t>(air_add + 41) == g_air.air
                           ? hook_player_fn(air_add, reinterpret_cast<void*>(&hk_gauge_add), &g_airadd,
                                            &g_airadd_slot, &g_airadd_fn)
                           : 0;
  if (air_adds > 0) {
    g_ok[sw::kInfiniteAir] = true;
    logf("cheats: Infinite air: the combat gauge's add (slot %d) hooked in %d vtables - a drain under water made a fill",
         g_airadd_slot, air_adds);
  } else {
    unavailable(sw::kInfiniteAir, "The air under water was not found in the game (see the log).");
  }
  // Infinite stamina: the combat gauge's spend (the slot after the add), and the add above for the land.
  if (air_adds > 0 && install_stamina(plplayer)) {
    g_ok[sw::kInfiniteStamina] = true;
  } else {
    logf("cheats: Infinite stamina: the combat gauge's spend not found, or not as its add has the gauge");
    unavailable(sw::kInfiniteStamina, "The combat gauge was not found in the game (see the log).");
  }

  if (install_qte(lay)) g_ok[sw::kAutoQte] = true;
  else unavailable(sw::kAutoQte, "The game's button prompts were not found (see the log).");

  if (install_results(lay)) {
    g_ok[sw::kFreezePlaytime] = g_ok[sw::kNoDeathCount] = true;
    g_count_ok = install_counts(lay);
    if (g_count_ok) g_ok[sw::kNoMissCount] = true;
    else unavailable(sw::kNoMissCount, "The shots' and hits' counts were not found in the game (see the log).");
    logf("cheats: the chapter's clock's add exe+0x%X, the Game Over's death count exe+0x%X",
         static_cast<unsigned>(g_clock_add.at - image::base()), static_cast<unsigned>(g_death.at - image::base()));
  } else {
    g_clock_add = mem::Patch{};
    g_death = mem::Patch{};
    for (const sw::Kind k : {sw::kFreezePlaytime, sw::kNoDeathCount, sw::kNoMissCount})
      unavailable(k, "The chapter's numbers were not found in the game (see the log).");
  }

  // The Mercenaries: the match's fields (and the combo's and Combo Time's clocks), the
  // enemies' hit record.
  const bool match = merc::discover_match();
  if (match && merc::endless_found()) g_ok[sw::kEndlessCombo] = true;
  else unavailable(sw::kEndlessCombo, "The Mercenaries' combo was not found in the game (see the log).");
  if (match && merc::combo_time_found()) g_ok[sw::kComboTime] = true;
  else unavailable(sw::kComboTime, "The Mercenaries' Combo Time was not found in the game (see the log).");
  if (match && merc::discover_kills(!g_eapply.empty())) g_ok[sw::kAutoKill] = true;
  else unavailable(sw::kAutoKill, "The enemies' hit record was not found in the game (see the log).");

  if (install_countdown(lay)) {
    g_ok[sw::kFreezeCountdown] = true;
    logf("cheats: Freeze countdown timer: the match clock %s, a stage's countdowns %s, an FSM's own %s",
         g_match_clock.prepared() ? "found" : "NOT found", g_stage_get ? "found" : "NOT found",
         g_own_read.applied ? "found" : "NOT found");
  } else {
    unavailable(sw::kFreezeCountdown, "The countdowns were not found in the game (see the log).");
  }
  return true;
}

bool enabled(sw::Kind k) { return g_on[k] != 0; }

void set(sw::Kind k, bool on) {
  InterlockedExchange(&g_on[k], on ? 1 : 0);
  InterlockedExchange(&g_dirty, 1);
}

bool available(sw::Kind k) { return g_ok[k]; }
const char* unavailable_reason(sw::Kind k) { return g_why[k] ? g_why[k] : ""; }

Status status() {
  if (!g_lock_ready) return Status{};
  EnterCriticalSection(&g_edit_lock);
  const Status s = g_status;
  LeaveCriticalSection(&g_edit_lock);
  return s;
}

void edit(Edit what, int chara, double value) {
  if (!g_lock_ready || chara < 0 || chara >= game::kMaxChars) return;
  EnterCriticalSection(&g_edit_lock);
  g_pending[static_cast<int>(what)][chara] = Pending{true, value};
  LeaveCriticalSection(&g_edit_lock);
}

void tick(const game::Run& run) {
  if (!run.paused) ++g_frame;  // the scope's view: frames the world moves in (the pause holds it as it was)
  if (InterlockedExchange(&g_dirty, 0)) {
    bool on[sw::kCount];
    for (int k = 0; k < sw::kCount; ++k) on[k] = g_on[k] != 0;
    config::save_switches(on);
  }
  const game::Layout& lay = game::layout();
  const bool paused = run.screen == game::Screen::kPause;
  if (run.in_game && !paused) ++g_play_frames;
  bool online = run.online;
  for (int i = 0; i < run.nchars; ++i) online = online || run.chars[i].control == game::Control::kNetwork;
  g_online = online;

  // The events: the switches turned since the last tick, and the characters come into play.
  uint32_t now_on = 0;
  for (int k = 0; k < sw::kCount; ++k)
    if (g_on[k] && g_ok[k]) now_on |= 1u << k;
  const uint32_t turned = now_on ^ g_was_on;
  g_was_on = now_on;
  const auto on = [&](sw::Kind k) { return ((now_on >> k) & 1u) != 0; };
  const auto went = [&](sw::Kind k) { return ((turned >> k) & 1u) != 0; };
  bool fresh[game::kMaxChars] = {};
  bool any_fresh = false;
  for (int i = 0; i < run.nchars; ++i) {
    const game::Character& c = run.chars[i];
    bool known = false;
    for (int k = 0; k < g_nseen; ++k)
      known = known || (g_seen[k].obj == c.obj && g_seen[k].control == static_cast<uint32_t>(c.control));
    fresh[i] = !known;
    any_fresh = any_fresh || fresh[i];
  }
  g_nseen = run.nchars;
  for (int i = 0; i < run.nchars; ++i) g_seen[i] = Seen{run.chars[i].obj, static_cast<uint32_t>(run.chars[i].control)};

  // God mode and the partner's: a spared character's health filled up as the switch goes on or as it comes into play
  // (above the dying point only: one the game itself has put down is left to it); the hooks keep it so.
  for (int i = 0; i < run.nchars; ++i) {
    const game::Character& c = run.chars[i];
    const sw::Kind k = c.control == game::Control::kLocal ? sw::kGodMode
                       : c.control == game::Control::kAi  ? sw::kPartnerGod
                                                          : sw::kCount;
    if (k == sw::kCount || !on(k) || !(fresh[i] || went(k))) continue;
    if (c.hp >= 2 && c.hp < c.hp_max) game::set_health(c.obj, c.hp_max);
  }
  // ... and a ride a spared character rides, as either switch goes on.
  if (run.in_game && ((went(sw::kGodMode) && on(sw::kGodMode)) || (went(sw::kPartnerGod) && on(sw::kPartnerGod))))
    repair_rides();
  // Infinite ammo: the game's bit, as the switch turns and as a character comes into play; as it goes on, the missiles
  // a helicopter in play has used given back.
  if (went(sw::kInfiniteAmmo) || any_fresh) ammo_events(run, on(sw::kInfiniteAmmo), went(sw::kInfiniteAmmo), fresh);
  if (went(sw::kInfiniteAmmo) && on(sw::kInfiniteAmmo) && run.in_game) refill_missiles();
  // Misses don't count: the shots put at the hits as the switch goes on, or a character of this machine's comes in.
  if (on(sw::kNoMissCount) && run.in_game)
    for (int i = 0; i < run.nchars; ++i)
      if (run.chars[i].control == game::Control::kLocal && (went(sw::kNoMissCount) || fresh[i]))
        shots_at_hits(run.chars[i].index);

  // The patches and re-pointed calls, put in or taken out as what they follow changes: the switches, a character
  // online (the scenes' automatic tests), a match running here (its clock).
  const Countdown cd = run.in_game ? countdown_now(run) : Countdown{};
  uint32_t want = 0;
  const bool wants[] = {on(sw::kInfiniteSkillPoints),
                        on(sw::kInfiniteAmmo),
                        on(sw::kOneHitKills) && g_bombs_ok,
                        on(sw::kSteadyAim),
                        on(sw::kSteadyAim) && g_scope_ok,
                        on(sw::kAutoQte),
                        on(sw::kAutoQte) && !online,
                        on(sw::kFreezePlaytime),
                        on(sw::kNoDeathCount),
                        on(sw::kFreezeCountdown) && cd.match,
                        on(sw::kNoMissCount) && g_count_ok};
  for (size_t i = 0; i < std::size(wants); ++i)
    if (wants[i]) want |= 1u << i;
  if (want != g_patched) {
    const auto b = [&](int i) { return ((want >> i) & 1u) != 0; };
    follow(g_buy, b(0));
    follow(g_level, b(0));
    for (mem::Patch& p : g_tablet_calls) follow(p, b(1));
    for (mem::Patch& p : g_missile) follow(p, b(1));
    for (mem::Patch& p : g_bomb_calls) follow(p, b(2));
    follow(g_sway, b(3));
    follow(g_kick, b(3));
    follow(g_scope_call, b(4));
    follow(g_line_call, b(4));
    follow(g_qte_call, b(5));
    for (mem::Patch& p : g_auto_scenes) follow(p, b(6));
    follow(g_clock_add, b(7));
    follow(g_death, b(8));
    follow(g_match_clock, b(9));
    for (mem::Patch& p : g_count_calls) follow(p, b(10));
    g_patched = want;
  }
  if (went(sw::kFreezeCountdown) && !on(sw::kFreezeCountdown)) g_hold.on = false;

  // The Mercenaries' switches (one match for Kill enemies as they appear).
  merc::Wanted mwant;
  mwant.endless_combo = on(sw::kEndlessCombo);
  mwant.combo_time = on(sw::kComboTime);
  mwant.kill = on(sw::kAutoKill);
  if (merc::tick(run, mwant)) {
    logf("cheats: Kill enemies as they appear switched off (it lasts one match)");
    set(sw::kAutoKill, false);
  }

  const uintptr_t info = run.in_game ? game::game_info_at() : 0;
  // The edits, while the game is paused; and what the panel shows.
  const uintptr_t points = game::skill_points_at();
  const uintptr_t gm = game::sgameman();
  EnterCriticalSection(&g_edit_lock);
  if (paused) {
    Pending& sp = g_pending[static_cast<int>(Edit::kSkillPoints)][0];
    if (sp.set && points) {
      double v = sp.value;
      if (v < 0) v = 0;
      if (v > kMostSkillPoints) v = kMostSkillPoints;
      const int32_t was = *reinterpret_cast<const int32_t*>(points);
      *reinterpret_cast<int32_t*>(points) = static_cast<int32_t>(v);
      logf("cheats: skill points %d -> %d", was, static_cast<int>(v));
    }
    // A counter: the live one and the copy a save takes (so a quit to the title and
    // Continue keep it), for a character this machine plays.
    const int index_of[4] = {lay.c_deaths, lay.c_kills, lay.c_shots, lay.c_hits};
    const Edit edits[4] = {Edit::kDeaths, Edit::kKills, Edit::kShots, Edit::kHits};
    const char* const names[4] = {"deaths", "kills", "shots", "hits"};
    for (int e = 0; e < 4 && info; ++e)
      for (int ch = 0; ch < game::kMaxChars && ch < lay.nblocks; ++ch) {
        const Pending& p = g_pending[static_cast<int>(edits[e])][ch];
        if (!p.set || index_of[e] < 0) continue;
        bool local = false;
        for (int i = 0; i < run.nchars; ++i)
          local = local || (run.chars[i].index == ch && run.chars[i].control == game::Control::kLocal);
        if (!local) continue;
        const int v = p.value < 0 ? 0 : p.value > lay.counter_most ? lay.counter_most : static_cast<int>(p.value);
        logf("cheats: character %d's %s %d -> %d", ch, names[e], *counter(info, ch, index_of[e]), v);
        *counter(info, ch, index_of[e]) = static_cast<uint16_t>(v);
        *counter(info - lay.saved_back, ch, index_of[e]) = static_cast<uint16_t>(v);
      }
    // The clock: every character's (the game adds to each alike), live and the save's copy.
    const Pending& tp = g_pending[static_cast<int>(Edit::kTime)][0];
    if (tp.set && info) {
      const float v = tp.value < 0 ? 0.0f : tp.value > lay.clock_most ? lay.clock_most : static_cast<float>(tp.value);
      logf("cheats: the chapter's clock %.2f -> %.2f s", static_cast<double>(*chapter_clock(info, 0)),
           static_cast<double>(v));
      for (int ch = 0; ch < lay.nblocks; ++ch) {
        *chapter_clock(info, ch) = v;
        *chapter_clock(info - lay.saved_back, ch) = v;
      }
    }
    // The countdown on screen: the match clock (and the HUD's copy) or the stage's timer;
    // never below a second (the game's own end comes at 0).
    const Pending& cp = g_pending[static_cast<int>(Edit::kCountdown)][0];
    if (cp.set && (cd.match || cd.t) && lay.match_most > 0) {
      const float v = cp.value < 1 ? 1.0f : cp.value > lay.match_most ? lay.match_most : static_cast<float>(cp.value);
      if (cd.match && gm) {
        logf("cheats: the match clock %.2f -> %.2f s", static_cast<double>(mem::read<float>(gm + lay.match_clock)),
             static_cast<double>(v));
        *reinterpret_cast<float*>(gm + lay.match_clock) = v;
        *reinterpret_cast<float*>(gm + lay.match_hud) = v;
      } else if (cd.t) {
        logf("cheats: the countdown on screen (%s timer %u) %.2f -> %.2f s",
             g_shown.kind == Timers::kOwn ? "its FSM's own" : "the stage's", g_shown.n, static_cast<double>(*cd.t),
             static_cast<double>(v));
        *cd.t = v;
        if (g_hold.on && g_hold.obj == g_shown.obj && g_hold.kind == g_shown.kind && g_hold.n == g_shown.n)
          g_hold.v = v;
      }
    }
  }
  for (auto& what : g_pending)
    for (Pending& p : what) p.set = false;  // an edit belongs to the pause it was made in
  if (!run.panel) {  // what the panel shows: read only while it can show
    LeaveCriticalSection(&g_edit_lock);
    return;
  }
  g_status = Status{};
  g_status.points = points != 0;
  g_status.skill_points = points ? *reinterpret_cast<const int32_t*>(points) : 0;
  if (info) {
    g_status.results = true;
    g_status.counter_most = lay.counter_most;
    g_status.clock_most = lay.clock_most;
    int first = -1;
    for (int i = 0; i < run.nchars; ++i) {
      const game::Character& c = run.chars[i];
      if (c.control != game::Control::kLocal || c.index >= lay.nblocks || g_status.ncounters >= 2) continue;
      if (first < 0) first = c.index;
      Counters& n = g_status.chars[g_status.ncounters++];
      n.index = c.index;
      n.name = c.name;
      n.shots = *counter(info, c.index, lay.c_shots);
      n.hits = *counter(info, c.index, lay.c_hits);
      n.kills = *counter(info, c.index, lay.c_kills);
      n.deaths = *counter(info, c.index, lay.c_deaths);
    }
    g_status.clock = *chapter_clock(info, first < 0 ? 0 : first);
  }
  if ((cd.match && gm) || cd.t) {
    g_status.countdown = true;
    g_status.match_clock = cd.match;
    g_status.countdown_left = cd.match ? mem::read<float>(gm + lay.match_clock) : *cd.t;
    g_status.countdown_most = lay.match_most;
  }
  LeaveCriticalSection(&g_edit_lock);
}

const char* keeps_life(const void* enemy) { return enemy && g_big_ok ? kept_alive(enemy) : nullptr; }

void remove_hooks() {
  unhook(g_sub);
  unhook(g_hit);
  unhook(g_apply);
  unhook(g_eapply);
  unhook(g_ework);
  unhook(g_add);
  unhook(g_reset);
  unhook(g_airadd);
  unhook(g_spend);
  unhook(g_ride_sub);
  g_rides_ok = false;
  for (mem::Patch& p : g_count_calls) p.revert();
  g_count_ok = false;
  g_big_ok = false;
  for (mem::Patch& p : g_bomb_calls) p.revert();
  g_bombs_ok = false;
  g_buy.revert();
  g_level.revert();
  for (mem::Patch& p : g_tablet_calls) p.revert();
  for (mem::Patch& p : g_missile) p.revert();
  g_missiles_ok = false;
  g_sway.revert();
  g_kick.revert();
  g_scope_call.revert();
  g_line_call.revert();
  g_scope_ok = false;
  g_qte_call.revert();
  for (mem::Patch& p : g_auto_scenes) p.revert();
  g_clock_add.revert();
  g_death.revert();
  g_match_clock.revert();
  for (mem::Patch& p : g_stage_reads) p.revert();
  g_own_read.revert();
  merc::remove_patches();
}

}  // namespace re6cc::cheats
