#include "merc.h"

#include <windows.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

#include "cheats.h"
#include "image.h"
#include "log.h"
#include "mem.h"
#include "mt.h"
#include "sigs.h"
#include "units.h"
#include "vthook.h"

namespace re6cc::merc {
namespace {

// --- the match (sGameMan's; block 0's fields) ----------------------------------------------
struct MatchLayout {
  uint32_t running = 0;      // the combo running (a byte)
  uint32_t combo = 0;        // its count
  uint32_t best = 0;         // the best combo
  uint32_t combo_clock = 0;  // the combo's window (a float, seconds)
  uint32_t ct_running = 0;   // Combo Time running (a byte)
  uint32_t ct_clock = 0;     // ... its clock (a float)
  uint32_t ct_kills = 0;     // ... the kills in it
  uint32_t earned = 0;       // what the running combo has earned
  uint32_t score = 0;
  uint32_t kills = 0;        // the stage's kills
  uint32_t mode_merc = 0, mode_no_mercy = 0;  // the modes' values
  int cap = 0, cap_no_mercy = 0;              // the most a score holds
  int target = 0, target_no_mercy = 0;        // the kills that clear a stage
  float ct_most = 0;                          // how long Combo Time runs
  uint32_t stride = 0;                        // a player's block (block 0 the first player's)
  uint32_t size = 0;                          // sGameMan's size
};
MatchLayout g_m;
bool g_match_ok = false;
mem::Patch g_combo_clock;  // Endless combo: the combo's window never runs down
mem::Patch g_ct_clock;     // Combo Time always on: its clock never runs down

// Endless combo's earnings: a kill's award (the game adds it to what the running combo has earned, put on the score
// only at the combo's end - which never comes) comes to hk_combo_award (the award's one call, re-pointed while the
// switch is on), which lets the game add it and puts what the combo has earned on the score at once, the cap kept.
using AwardFn = void(__thiscall*)(void*, uint32_t, int);
AwardFn g_award = nullptr;
mem::Patch g_award_call;
volatile LONG g_banking = 0;  // the tick's: Endless combo on, a match running here offline (0 no, 1 the match, 2 No Mercy)
void __thiscall hk_combo_award(void* gm, uint32_t player, int amount) {
  g_award(gm, player, amount);
  const LONG banking = g_banking;
  if (!banking || player >= 8) return;
  const uintptr_t block = reinterpret_cast<uintptr_t>(gm) + player * g_m.stride;
  if (player * g_m.stride + g_m.score + 4 > g_m.size) return;
  int32_t& earned = *reinterpret_cast<int32_t*>(block + g_m.earned);
  int32_t& score = *reinterpret_cast<int32_t*>(block + g_m.score);
  if (earned <= 0) return;
  const int cap = banking == 2 ? g_m.cap_no_mercy : g_m.cap;
  const long long sum = static_cast<long long>(score) + earned;
  score = static_cast<int32_t>(sum > cap ? cap : sum);
  earned = 0;
}

// --- the switches' match: what the tick saw last ------------------------------------------
uint32_t g_patched = 0;  // the patches as the last change left them
bool g_timed_was = false;
int g_last_step = -1;
uint32_t g_last_mode = 0;
Status g_status;
CRITICAL_SECTION g_lock;
bool g_lock_ready = false;

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }

template <typename T>
T& field(uintptr_t obj, uint32_t off) {
  return *reinterpret_cast<T*>(obj + off);
}

bool int_in_image(uintptr_t at, int lo, int hi, int* out) {
  int v = 0;
  if (!image::whole().contains(at) || !mem::read_safe(at, &v) || v < lo || v > hi) return false;
  *out = v;
  return true;
}
bool float_in_image(uintptr_t at, float lo, float hi, float* out) {
  float v = 0;
  if (!image::whole().contains(at) || !mem::read_safe(at, &v) || !(v >= lo && v <= hi)) return false;
  *out = v;
  return true;
}

// The first character this machine plays with health: who the kills are dealt by.
const game::Character* dealer_of(const game::Run& run) {
  for (int i = 0; i < run.nchars; ++i)
    if (run.chars[i].control == game::Control::kLocal && run.chars[i].hp > 0) return &run.chars[i];
  return nullptr;
}

// --- Kill enemies as they appear: a hit written into each enemy's own hit record ----------
// An enemy takes its hits through its hit record (a DamageInfo in the enemy): its brain (a unit of its own, on a line
// sUnit runs before the enemies') reads it each frame while it is marked valid and hands it to the enemy's damage
// handler - the death, its one kill report for the player who dealt it (the score, the combo, the stage's kill, the
// time a hand-to-hand kill puts on the clock), as for any hit. The network writes a hit there the same way for a
// partner's shot. The tick writes one - all the enemy's health, dealt by the first character this machine plays, the
// attack its own hand-to-hand - into each enemy in play whose record is free, a few a frame, before the game's frame
// (its brain reads it in that frame); one still in the record at the next tick is cleared.
//
// Every enemy in play is tried until it is dead (2026-11-14: before, the tick tried again only an enemy a first hit
// had been written into - the ones beyond the first four as the switch went on, and any not ready to be hit at its
// start, never were). An enemy whose hit went unread (its brain did not read it: asleep, busy, or it dropped it) is
// tried again soon; one that took it and lives on (a shrug), a moment later. A hit of the mod's is lethal whatever
// the enemy's own damage code makes of it: its take-damage (cheats.cpp hooks slot 128 of every enemy) asks lethal(),
// which takes all the health - a class works a hit out by where it landed (the mod's lands on part 0: the network
// writes the part, the mod none), and some shrink it (Gnezdo 1%, the zombies' and J'avo's limbs, the Whopper) or make
// it nothing (an armoured part). After a shrug the hit deals kBigHit; after kForceAfter, the take-damage takes all the
// health even from nothing; and it lands on another of the enemy's parts each time (the part index cycles: an enemy
// whose own code keeps its life - Rasklapanje's subtract-health never goes below 1 - loses a limb a hit). The enemies
// in play are known by their own coming and going - each enemy class's start (as sUnit puts it into play) and
// destructor hooked - not by walking every unit each frame.
struct KillLayout {
  uint32_t record = 0, valid = 0;           // uEnemy: its hit record, and its valid byte
  uint32_t damage = 0, attrs = 0, nattrs = 0;  // the record: the damage, the attribute words (and how many)
  uint32_t pos = 0, dir = 0;                // ... where it hit, and its direction (four floats each)
  uint32_t word60 = 0, word64 = 0;          // ... the collision it hit, and the part of it (the damage code's region)
  uint32_t attack = 0, dealer = 0;          // ... the attack's id, who dealt it
  uint32_t byte30 = 0, byte31 = 0;          // ... two bytes (the first a random one)
  uint32_t state = 0;                       // uEnemy: its state (a byte; 2 dying)
  uint32_t died = 0, reported = 0;          // ... its death handled, its kill reported (bytes)
  uint32_t flags = 0;                       // ... the word whose bit 31 keeps a kill from being reported
  uint32_t ended = 0;                       // ... the byte its erase sets last: its brain then ends it (0: not known)
  uint32_t coord_pos = 0;                   // uCoord: a unit's position (mPos)
  uintptr_t damage_vt = 0;                  // DamageInfo's vtable: the record's first word
};
KillLayout g_k;
bool g_kills_ok = false;
bool g_lethal_ok = false;  // the enemies' take-damage is hooked (cheats.cpp): lethal() sees the mod's hits taken
const mt::Class* g_cls_enemy = nullptr;

// The enemies in play: put in by their start, taken out by their destructor (both on the main thread, from sUnit's
// line frame; the tick and the take-damage read them there too - the lock is for any other caller of a destructor).
std::vector<vthook::Hooked> g_starts, g_dtors;
CRITICAL_SECTION g_play_lock;
using StartFn = void(__thiscall*)(void*);
using DtorFn = void*(__thiscall*)(void*, unsigned);

constexpr int kPerTick = 8;          // hits written a tick, at most
constexpr DWORD kSoonMs = 100;       // an enemy whose hit went unread (or was dropped): tried again after this ...
constexpr int kSoonMost = 50;        // ... this many times (5 s)
constexpr DWORD kRetryMs = 400;      // one that took a hit and lives on: tried again after this ...
constexpr int kShrugMost = 20;       // ... this many times (8 s)
constexpr DWORD kSlowMs = 2000;      // ... and from then on, either, only this often
constexpr int kBigAfter = 1;         // a hit after this many shrugs deals kBigHit ...
constexpr int kForceAfter = 3;       // ... after this many its take-damage takes all its health even from nothing ...
constexpr int kSpreadAfter = 3;      // ... and lands on another of its parts each time:
constexpr uint32_t kSpreadParts = 32;  // the part indices tried (below 70: the J'avo's attached mutations start there)
constexpr int32_t kBigHit = 0x7FFF;  // the most health a character holds (the take-damage takes at most this)

// Who the kills are dealt by (the tick's: the first character this machine plays, its class's id the attack - its
// own hand-to-hand), while Kill enemies as they appear is on in a match running here; none otherwise.
struct Dealer {
  uintptr_t obj = 0;
  const char* name = "";
  uint32_t attack = 0;
};
Dealer g_dealer;

// An enemy in play, and what the mod has done to it.
struct Foe {
  uintptr_t obj = 0;
  const mt::Class* cls = nullptr;
  DWORD next = 0;         // not tried again before this (GetTickCount), once `wait` is set
  bool wait = false;
  bool pending = false;   // a hit of the mod's is in its record (written at the last tick)
  bool applied = false;   // ... and its take-damage ran on it (lethal())
  int32_t damage = 0;     // what that hit holds: its damage, dealer, attack and random byte
  uintptr_t dealer = 0;
  uint32_t attack = 0;
  uint8_t mark = 0;
  int writes = 0;         // hits written into it
  int unread = 0;         // hits it did not read (cleared at the next tick) or dropped before its damage handler
  int shrugs = 0;         // hits its take-damage took, it alive after
  int took = 0;           // what its own damage code worked the last one out as (before lethal())
  bool dead = false;      // seen dead or dying (counted)
  bool ended = false;     // its body ended as its kill counted (its end byte set while the switch acts)
  bool noted = false;     // its trouble logged
  // A part of another enemy (a J'avo's mutation): the enemy whose take-damage took the hit written into it - its own
  // brain reads its parts' records. Not hit again while that enemy is about (the mod hits it), again once it has gone
  // (a part may live on alone: the head that runs off).
  uintptr_t owner = 0;
};
std::vector<Foe> g_foes;
size_t g_turn = 0;   // where the tick's writes start (each enemy its turn when more are due than a tick writes)
int g_npending = 0;  // the foes with a hit of the mod's in their record

// This match, by class: for the log at its end.
struct Tally {
  const mt::Class* cls = nullptr;
  int seen = 0, killed = 0, died = 0, gone = 0, writes = 0, unread = 0, shrugs = 0, ended = 0;
  bool noted_shrug = false, noted_kept = false, noted_unread = false;
};
std::vector<Tally> g_tally;
int g_killed = 0, g_hits = 0, g_writing = 0, g_logged = 0;  // this match: killed by the mod, hits written; this tick
int g_ended = 0;  // this match: bodies ended as their kills counted
uintptr_t g_brain_end = 0;  // the enemies' brains' end step (uEmIntel's): it ends an enemy whose end byte is set

Foe* foe_of(uintptr_t e) {
  for (Foe& f : g_foes)
    if (f.obj == e) return &f;
  return nullptr;
}

Tally& tally_of(const mt::Class* c) {
  for (Tally& t : g_tally)
    if (t.cls == c) return t;
  g_tally.push_back(Tally{c});
  return g_tally.back();
}

const char* name_of(const mt::Class* c) { return c ? c->name : "an enemy"; }

// Its record holds the hit the mod wrote (its fields; marked valid too unless `any`).
bool holds(const Foe& f, bool any) {
  const uintptr_t rec = f.obj + g_k.record;
  return (any || field<uint8_t>(f.obj, g_k.valid)) && field<int32_t>(rec, g_k.damage) == f.damage &&
         field<uintptr_t>(rec, g_k.dealer) == f.dealer && field<uint32_t>(rec, g_k.attack) == f.attack &&
         field<uint8_t>(rec, g_k.byte30) == f.mark;
}

// Dead or dying, by the game's own marks: no health, its state dying, its death handled or its kill reported.
bool dead(uintptr_t u) {
  return field<int16_t>(u, game::layout().hp) <= 0 || field<uint8_t>(u, g_k.state) == 2 ||
         field<uint8_t>(u, g_k.died) || field<uint8_t>(u, g_k.reported);
}

// It can take a hit now: running, alive, a kill the game reports (bit 31: one it never would - a chrysalid hatched,
// one the stage replaces), its record free.
bool can_hit(uintptr_t u) {
  return (field<uint32_t>(u, units::layout().unit_flags) & 7) == 2 && !dead(u) && !(field<uint32_t>(u, g_k.flags) >> 31) &&
         !field<uint8_t>(u, g_k.valid) && field<uintptr_t>(u + g_k.record, 0) == g_k.damage_vt;
}

void wait(Foe& f, DWORD now, DWORD ms) {
  f.next = now + ms;
  f.wait = true;
}
bool due(const Foe& f, DWORD now) { return !f.wait || static_cast<LONG>(now - f.next) >= 0; }

// A hit written into an enemy - all its health (kBigHit once it has lived through one), from the dealer's place
// towards it, on part 0 of it (another part each time once it has lived through a few) - marked valid last.
void write_hit(Foe& f, const Dealer& by) {
  const uintptr_t u = f.obj, rec = u + g_k.record;
  const int hp = field<int16_t>(u, game::layout().hp);
  const auto* ep = reinterpret_cast<const float*>(u + g_k.coord_pos);
  const auto* pp = reinterpret_cast<const float*>(by.obj + g_k.coord_pos);
  float d[3] = {ep[0] - pp[0], ep[1] - pp[1], ep[2] - pp[2]};
  const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (!(len > 0.001f) || !std::isfinite(len)) {
    d[0] = d[1] = 0;
    d[2] = 1;
  } else {
    for (float& c : d) c /= len;
  }
  const int32_t damage = f.shrugs >= kBigAfter ? kBigHit : hp;
  const uint32_t part =
      f.shrugs >= kSpreadAfter ? static_cast<uint32_t>(f.shrugs - kSpreadAfter) % kSpreadParts : 0;
  const auto mark = static_cast<uint8_t>(std::rand() & 0xFF);
  field<int32_t>(rec, g_k.damage) = damage;
  for (uint32_t k = 0; k < g_k.nattrs; ++k) field<uint32_t>(rec, g_k.attrs + 4 * k) = 0;
  for (int k = 0; k < 3; ++k) {
    field<float>(rec, g_k.pos + 4 * k) = ep[k];
    field<float>(rec, g_k.dir + 4 * k) = d[k];
  }
  field<float>(rec, g_k.pos + 12) = 0;
  field<float>(rec, g_k.dir + 12) = 0;
  field<uint32_t>(rec, g_k.word60) = 0;
  field<uint32_t>(rec, g_k.word64) = part;
  field<uint32_t>(rec, g_k.attack) = by.attack;
  field<uintptr_t>(rec, g_k.dealer) = by.obj;
  field<uint8_t>(rec, g_k.byte30) = mark;
  field<uint8_t>(rec, g_k.byte31) = 0;
  field<uint8_t>(u, g_k.valid) = 1;  // last
  f.pending = true;
  f.applied = false;
  f.damage = damage;
  f.dealer = by.obj;
  f.attack = by.attack;
  f.mark = mark;
  ++f.writes;
  ++g_npending;
  ++g_hits;
  ++tally_of(f.cls).writes;
  if (g_logged < 8) {
    ++g_logged;
    logf("merc: a hit of %d written into %s %08X (try %d, part %u), dealt by %s (attack 0x%08X)", damage,
         name_of(f.cls), static_cast<unsigned>(u), f.writes, part, by.name, by.attack);
  }
}

// An enemy's trouble, in the log once for its class this match.
void note(Foe& f, bool* noted, const char* what) {
  if (*noted || f.noted) return;
  *noted = f.noted = true;
  const game::Layout& lay = game::layout();
  const uintptr_t u = f.obj;
  logf("merc: %s %08X %s - health %d of %d, state %u, its flags 0x%08X; %d hits written (%d taken and lived on, %d "
       "not taken); its own damage code worked the last one taken out as %d",
       name_of(f.cls), static_cast<unsigned>(u), what, field<int16_t>(u, lay.hp), field<int16_t>(u, lay.hp_max),
       field<uint8_t>(u, g_k.state), field<uint32_t>(u, g_k.flags), f.writes, f.shrugs, f.unread, f.took);
}

// The tick's part (under the lock): what became of the last tick's hits - the game's frame has run since - and each
// enemy's death; then the hits due, while `acts` and the game is not paused (nothing reads them at the pause).
void kill_tick(bool acts, bool paused) {
  const DWORD now = GetTickCount();
  EnterCriticalSection(&g_play_lock);
  for (Foe& f : g_foes) {
    const uintptr_t u = f.obj;
    Tally& t = tally_of(f.cls);
    if (f.pending) {
      const bool still = holds(f, false);
      if (still) field<uint8_t>(u, g_k.valid) = 0;  // the mod's, still there: cleared (taken or not)
      if (f.applied && f.owner) {
        wait(f, now, kRetryMs);  // a part's hit, taken by the enemy it is part of
      } else if (f.applied) {
        if (!dead(u)) {  // taken, and it lives on
          ++f.shrugs;
          ++t.shrugs;
          wait(f, now, f.shrugs > kShrugMost ? kSlowMs : kRetryMs);
          if (f.shrugs == kBigAfter)
            note(f, &t.noted_shrug, "took a hit of the mod's and lives on - hit harder from now on");
          else if (f.shrugs == kForceAfter + 3) {
            // Even its take-damage taking all its health leaves it alive: the game keeps its life (One hit kills'
            // rules say why), or its hits never reach its health.
            const char* why = cheats::keeps_life(reinterpret_cast<const void*>(u));
            char what[200];
            std::snprintf(what, sizeof(what), "lives on through the mod's hits (%s) - hit on another part of it each "
                          "time", why ? why : "its take-damage taking all its health");
            note(f, &t.noted_kept, what);
          }
        }
      } else if (!paused || !still) {  // not read by its brain this frame, or dropped before its damage handler
        ++f.unread;
        ++t.unread;
        wait(f, now, f.unread > kSoonMost ? kSlowMs : kSoonMs);
        if (f.unread == kSoonMost)
          note(f, &t.noted_unread, "has taken none of the mod's last hits (its brain does not read them: asleep, out "
                                   "of the fight, or busy)");
      }
      f.pending = false;
      --g_npending;
    }
    // Its death, once a life: the mod's when its take-damage took the mod's hit.
    if (!f.dead && dead(u)) {
      f.dead = true;
      if (f.applied) {
        ++t.killed;
        ++g_killed;
      } else {
        ++t.died;
      }
    } else if (f.dead && !dead(u)) {
      f.dead = false;  // a life again (its start run again, its health given back): tried from scratch
      f.wait = false;
      f.shrugs = f.unread = 0;
      f.noted = false;
      f.ended = false;
    }
    // A body, while the switch acts (not at the pause): once its kill has counted (reported - the score, the combo,
    // the stage's kill are done at the death), ended as its erase ends it - its end byte set, its brain ends it in
    // the game's next frame: no death motion, no dissolve, its place in the stage's count free at once. Not a part of
    // another enemy, nor a body whose kill the game does not report (a chrysalid's J'avo: its own way on).
    if (acts && !paused && g_k.ended && f.dead && !f.ended && !f.owner && field<uint8_t>(u, g_k.reported)) {
      f.ended = true;
      if (!field<uint8_t>(u, g_k.ended)) {
        field<uint8_t>(u, g_k.ended) = 1;
        ++t.ended;
        ++g_ended;
        if (g_ended == 1)
          logf("merc: %s %08X dead (its kill counted): its body ended at once - its brain ends it as its erase would",
               name_of(f.cls), static_cast<unsigned>(u));
      }
    }
    f.applied = false;
  }
  int written = 0;
  if (acts && !paused && g_dealer.obj && !g_foes.empty()) {
    const size_t n = g_foes.size();
    size_t last = g_turn;
    for (size_t k = 0; k < n && written < kPerTick; ++k) {
      const size_t i = (g_turn + k) % n;
      Foe& f = g_foes[i];
      if (f.pending || f.dead || !due(f, now) || !can_hit(f.obj)) continue;
      if (f.owner && foe_of(f.owner)) continue;  // a part: its enemy is hit
      write_hit(f, g_dealer);
      ++written;
      last = i;
    }
    g_turn = (last + 1) % n;
  }
  g_writing = written;
  LeaveCriticalSection(&g_play_lock);
}

// The match's tallies started again: the enemies in play now counted as seen.
void restart_tally() {
  EnterCriticalSection(&g_play_lock);
  g_tally.clear();
  for (const Foe& f : g_foes) ++tally_of(f.cls).seen;
  g_killed = g_hits = g_logged = g_ended = 0;
  LeaveCriticalSection(&g_play_lock);
}

// The match's end: what the mod did to each kind of enemy, in the log.
void log_tally() {
  EnterCriticalSection(&g_play_lock);
  if (g_hits) {
    int alive = 0;
    for (const Foe& f : g_foes) alive += !f.dead && !f.owner;
    logf("merc: the match: %d enemies killed by the mod's hits (%d hits written), %d left in play; %d bodies ended as "
         "their kills counted",
         g_killed, g_hits, alive, g_ended);
    for (const Tally& t : g_tally) {
      int here = 0;
      for (const Foe& f : g_foes) here += f.cls == t.cls && !f.dead && !f.owner;
      if (!t.writes && !here) continue;
      logf("merc:   %-14s %3d in play: %3d killed by the mod, %3d died otherwise, %3d gone alive, %3d still in play - "
           "%4d hits written, %3d taken and lived on, %4d not taken; %3d bodies ended",
           name_of(t.cls), t.seen, t.killed, t.died, t.gone, here, t.writes, t.shrugs, t.unread, t.ended);
    }
  }
  LeaveCriticalSection(&g_play_lock);
}

// An enemy started (it comes into play): known from now on, tried at the next tick (before the game's next frame:
// its brain reads the hit in that frame).
void __thiscall hk_enemy_start(void* self) {
  const uintptr_t start = vthook::original(g_starts, self, 0);
  if (!start) return;
  reinterpret_cast<StartFn>(start)(self);
  const auto e = reinterpret_cast<uintptr_t>(self);
  EnterCriticalSection(&g_play_lock);
  if (Foe* f = foe_of(e)) {
    if (f->pending) {
      if (holds(*f, false)) field<uint8_t>(e, g_k.valid) = 0;
      --g_npending;
    }
    *f = Foe{e, f->cls};  // started again: from scratch
  } else {
    Foe nf;
    nf.obj = e;
    nf.cls = units::class_of(e);
    g_foes.push_back(nf);
    ++tally_of(nf.cls).seen;
  }
  LeaveCriticalSection(&g_play_lock);
}

// An enemy destroyed (it has gone): forgotten - before its memory goes.
void* __thiscall hk_enemy_dtor(void* self, unsigned flags) {
  const auto e = reinterpret_cast<uintptr_t>(self);
  EnterCriticalSection(&g_play_lock);
  for (size_t i = 0; i < g_foes.size(); ++i) {
    Foe& f = g_foes[i];
    if (f.obj != e) continue;
    if (!f.dead) {
      Tally& t = tally_of(f.cls);
      if (dead(e) && f.applied) {
        ++t.killed;
        ++g_killed;
      } else if (dead(e)) {
        ++t.died;
      } else {
        ++t.gone;
      }
    }
    if (f.pending) --g_npending;
    g_foes.erase(g_foes.begin() + static_cast<long>(i));
    break;
  }
  for (Foe& f : g_foes)
    if (f.owner == e) f.owner = 0;  // its parts on their own now
  LeaveCriticalSection(&g_play_lock);
  const uintptr_t dtor = vthook::original(g_dtors, self, 0);
  return dtor ? reinterpret_cast<DtorFn>(dtor)(self, flags) : self;
}

// --- The flow: how a Mercenaries stage sends its enemies (2026-11-14) ----------------------
// A stage's EmSet script sends its enemies through its EnemyAdd orders: while sEnemy holds fewer than mMinEmNum
// set-placed enemies (3 to 13 on the Mercenaries' maps), one more from the order's stock every mWaitTimer (0 to 2 s).
// sEnemy counts every set-placed model it holds, alive or dead: a body keeps its place until its erase has run - so with
// every enemy killed as it came, the places filled with bodies and the stage waited for them (docs/modes.md, "How a
// stage sends its enemies"). While Kill enemies as they appear acts, the bodies of the kinds that fill the Mercenaries'
// maps dissolve at once: in each family's erase action (the class by name, the function from its vtable) each load of
// one of its waits (`movss xmm0, [constant]`, the constant the one known) is re-pointed at a short one of the mod's - the
// whole death plays, its waits short, and the stage's own limits stand. The other kinds (one to three a map) erase in
// their own time. Three stages (s1200, s1201, s1204) also send by where the player is: em_add_check's AreaHit orders put
// the player's area in RSC[0] and the spawner sends from that area's groups only (s1200's far end of the platform has
// none; its ticket gates' run out at the 40 kills that move the stage on) - "unless I move around". While it acts,
// AreaHit's area test (its one call, re-pointed) finds the player in every area: em_add_check takes the first, whose
// groups carry each of the three stages to its next phase. And a stage moves on to its next sender by kills (an
// EnemyCheck counting the dead among its listed entries), whatever the sender before it has left unsent: in normal play
// the sender runs some enemies ahead of the kills, but with every enemy killed as it comes it runs just behind them, so a
// stage can move on with entries left unsent in groups the next sender does not send - stranded until the last sender,
// which the stage reaches only once nearly all the listed entries are dead: s1206 stopped at 89 kills (its fifth sender
// moves on at 78 dead with group 8's last entries unsent; its sixth, groups 6 and 9, needs 90 of 95 dead). While it
// acts, each sender's stock (its builder's one call, re-pointed) also takes the groups of every sender before it this
// match (the param's mStockGroup widened while the game's builder runs, then put back): nothing is left behind, and no
// group a sender has not sent from comes early (the bosses', the secret one's). With all that, a sender's own wait
// between its enemies was the pace (2026-11-15: "we could spawn more enemies in faster"; No Mercy's Steel Beast sends
// one every 2 s - 300 kills took 8.7 minutes): while it acts, the test of a sender's timer goes straight to its send
// (the jbe made a jmp), so it sends whenever fewer than its mMinEmNum are in play - the stage's own limit, as its
// wait-0 senders do (s1208's, every No Mercy stage's first). And then a body's time in the count was the pace (the
// user: "decrease the death animation ... instead of just kicking the can with a higher cap"): every erase ends by
// setting one byte of the enemy's, and the enemies' brains' end step (uEmIntel's, which every family's brain runs
// last) ends an enemy with it set - so while it acts the tick sets it on each body whose kill has counted (kill_tick):
// gone in the game's next frame, no death animation, no dissolve. All put back as it stops acting.
const float kFastSeconds = 0.25f;  // an erase's wait in seconds, at most
const float kFastFrames = 8.0f;    // ... in frames (sBioMain's +0x70 a frame: Napad's waits, a zombie's lying)
struct EraseWait {
  const char* cls;   // the erase action's class
  int slot;          // its function (its vtable slot: 6 the start, 7 a frame)
  float was;         // the game's wait there (its constant's value)
  int loads;         // how many loads of it the function makes
  const float* fast; // the mod's
};
const EraseWait kEraseWaits[] = {
    {"cEm1000ActDeadErase", 6, 7.0f, 1, &kFastSeconds},     // J'avo: its dissolve (then 0.5 s)
    {"cEm5000ActDeadErase", 7, 70.0f, 1, &kFastFrames},     // zombies: lying ...
    {"cEm5000ActDeadErase", 7, 8.0f, 1, &kFastSeconds},     // ... then the dissolve
    {"cEm5700ActDeadErase", 6, 2.0f, 1, &kFastSeconds},     // dogs: lying ...
    {"cEm5700ActDeadErase", 7, 5.0f, 1, &kFastSeconds},     // ... then the dissolve
    {"cEm5700ActDeadEraseIce", 6, 7.0f, 1, &kFastSeconds},  // dogs frozen
    {"cEm3200ActErase", 6, 420.0f, 2, &kFastFrames},        // Napad (in frames) ...
    {"cEm3200ActErase", 6, 300.0f, 1, &kFastFrames},        // ... after an explosive's death
    {"cEm5600ActDeadErase", 6, 2.0f, 1, &kFastSeconds},     // the Whopper: lying ...
    {"cEm5600ActDeadErase", 7, 3.0f, 1, &kFastSeconds},     // ...
    {"cEm5600ActDeadErase", 7, 7.0f, 1, &kFastSeconds},     // ... then the dissolve
};
std::vector<mem::Patch> g_erase_loads;  // the loads found, re-pointed while the switch acts
using StockFn = void(__thiscall*)(void*, void*, void*, void*);
StockFn g_stock_fn = nullptr;   // EnemyAdd's stock builder
mem::Patch g_stock_call;        // EnemyAdd's call of it
uint32_t g_stock_type = 0, g_stock_groups = 0;  // the param: its mStockType (2: by groups) and mStockGroup (a bit a group)
mem::Patch g_wait_jump;  // EnemyAdd's wait test: its jbe to the send made a jmp while the switch acts
uint32_t g_wait_timer = 0, g_wait_param = 0, g_wait_min = 0;  // the work's timer; the param's mWaitTimer, mMinEmNum
// This match: the groups each script's senders have stocked from. A script is the `this` its orders run with (its
// cFSMOrder: FreeS32, FreeFlag, mCntState are its own), so a boss's own script (Em_BossPop_*) keeps its groups to
// itself and the stage's spawner widens with its own senders' groups only.
struct ScriptGroups {
  const void* script;
  uint32_t groups;
};
constexpr int kScripts = 32;
ScriptGroups g_sent[kScripts] = {};
int g_sent_count = 0;
int g_stock_logged = 0;

uint32_t* sent_of(const void* script) {
  for (int i = 0; i < g_sent_count; ++i)
    if (g_sent[i].script == script) return &g_sent[i].groups;
  if (!script || g_sent_count >= kScripts) return nullptr;
  g_sent[g_sent_count] = {script, 0};
  return &g_sent[g_sent_count++].groups;
}
using InsideFn = bool(__fastcall*)(void*);
InsideFn g_inside_fn = nullptr;  // an area task's player test
mem::Patch g_inside_call;        // AreaHit's call of it
volatile LONG g_flowing = 0;     // the tick's: Kill enemies as they appear acting (the area test the mod's)
bool g_flow_was = false;

// AreaHit asks whether a player is inside an area task: every one while the flow acts.
bool __fastcall hk_area_inside(void* task) {
  if (g_flowing) return true;
  return g_inside_fn(task);
}

// A sender's stock built (its first frame): with the groups of every sender before it in its script this match, while
// the flow acts.
void __thiscall hk_build_stock(void* self, void* stock, void* list, void* param) {
  const auto p = reinterpret_cast<uintptr_t>(param);
  uint32_t* sent = g_flowing && p && field<uint32_t>(p, g_stock_type) == 2 ? sent_of(self) : nullptr;
  if (!sent) {
    g_stock_fn(self, stock, list, param);
    return;
  }
  uint32_t& groups = field<uint32_t>(p, g_stock_groups);
  const uint32_t own = groups, extra = *sent & ~own;
  *sent |= own;
  groups = own | extra;
  g_stock_fn(self, stock, list, param);
  groups = own;
  if (extra && g_stock_logged < 6) {
    ++g_stock_logged;
    logf("merc: the stage's next sender (groups 0x%X) also stocks the groups of the ones before it in its script "
         "(0x%X): nothing they left unsent is stranded",
         own, extra);
  }
}

// A call of `fn` at `at` (E8 rel32) prepared to call `hook` instead.
bool prepare_call(mem::Patch& p, uintptr_t at, const void* hook, const char* label) {
  char expect[24];
  std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", u8at(at + 1), u8at(at + 2), u8at(at + 3), u8at(at + 4));
  std::vector<uint8_t> call(5);
  call[0] = 0xE8;
  const auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook) - (at + 5));
  std::memcpy(call.data() + 1, &rel, 4);
  return p.prepare(at, expect, call, label);
}

// The loads of the float `was` (`movss xmm0, [constant]`, F3 0F 10 05 + its address, the constant in the image's
// read-only data) in the function at `fn` - up to its end (the int3 padding after it).
std::vector<uintptr_t> wait_loads(uintptr_t fn, float was) {
  std::vector<uintptr_t> out;
  if (!fn || !image::text().contains(fn) || !mem::readable(fn, 0x800)) return out;
  for (uintptr_t p = fn; p < fn + 0x800 - 8; ++p) {
    if (u8at(p) == 0xCC && u8at(p + 1) == 0xCC) break;
    if (u8at(p) != 0xF3 || u8at(p + 1) != 0x0F || u8at(p + 2) != 0x10 || u8at(p + 3) != 0x05) continue;
    const uintptr_t c = u32at(p + 4);
    float v = 0;
    if (image::rdata().contains(c) && mem::read_safe(c, &v) && v == was) out.push_back(p);
  }
  return out;
}

// The flow's patches and call, each read out of the game's own code and checked against the others.
void discover_flow() {
  // The erases' waits: each family's own, in its erase action's code.
  int actions = 0;
  const char* last = "";
  const mt::Class* action = mt::find("cEmAction");
  for (const EraseWait& w : kEraseWaits) {
    const mt::Class* c = mt::find(w.cls);
    const uintptr_t fn = c && c->vtable && action && mt::derives(c->dti, action->dti)
                             ? mem::read<uintptr_t>(c->vtable + 4 * w.slot)
                             : 0;
    const std::vector<uintptr_t> at = wait_loads(fn, w.was);
    if (static_cast<int>(at.size()) != w.loads) {
      logf("merc: %s's wait of %.0f (slot %d) not as known (%d loads) - that wait stays the game's", w.cls,
           static_cast<double>(w.was), w.slot, static_cast<int>(at.size()));
      continue;
    }
    for (const uintptr_t p : at) {
      char expect[40];
      std::snprintf(expect, sizeof(expect), "F3 0F 10 05 %02X %02X %02X %02X", u8at(p + 4), u8at(p + 5), u8at(p + 6),
                    u8at(p + 7));
      std::vector<uint8_t> load = {0xF3, 0x0F, 0x10, 0x05, 0, 0, 0, 0};
      const auto to = reinterpret_cast<uintptr_t>(w.fast);
      std::memcpy(load.data() + 4, &to, 4);
      mem::Patch patch;
      if (patch.prepare(p, expect, load, w.cls)) g_erase_loads.push_back(patch);
    }
    if (std::strcmp(last, w.cls) != 0) ++actions;
    last = w.cls;
  }
  logf("merc: %d erase actions' waits found (%d loads: the J'avo's, the zombies', the dogs', Napad's, the Whopper's) - "
       "while Kill enemies as they appear acts those bodies dissolve at once (%.2f s, %.0f frames a wait), so a body "
       "soon leaves the place it holds in the stage's count",
       actions, static_cast<int>(g_erase_loads.size()), static_cast<double>(kFastSeconds),
       static_cast<double>(kFastFrames));
  // EnemyAdd's stock builder: its param's type and groups as the builder reads them, inside its param class.
  const uintptr_t stock = game::sig_one("kSigEnemyAddStock");
  const uintptr_t build = stock ? game::call_target(stock + 11) : 0;
  const mt::Class* add = mt::find("cFSMOrder::FuncParamAddEnemy");
  if (build && image::text().contains(build) && mem::matches(build, mem::parse_pattern(RE6CC_STOCK_BUILD)) && add &&
      u8at(build + 40) == 2 && u8at(build + 32) + 4 <= add->size && u8at(build + 60) + 4 <= add->size &&
      u8at(stock + 18) + 4 <= add->size) {
    g_stock_type = u8at(build + 32);
    g_stock_groups = u8at(build + 60);
    if (prepare_call(g_stock_call, stock + 11, reinterpret_cast<void*>(&hk_build_stock), "the stage's sender's stock"))
      g_stock_fn = reinterpret_cast<StockFn>(build);
  }
  if (g_stock_fn)
    logf("merc: a stage's sender stocks its groups (exe+0x%X -> exe+0x%X; its param's type +0x%X, groups +0x%X) - while "
         "Kill enemies as they appear acts, each takes the groups of every sender before it in its script too",
         static_cast<unsigned>(stock + 11 - image::base()), static_cast<unsigned>(build - image::base()), g_stock_type,
         g_stock_groups);
  else
    logf("merc: a stage's sender's stock is not as known - what a sender leaves unsent waits for the stage's last");
  // EnemyAdd's wait: the test of its timer, in the function of the stock's call (after it, before the function's end).
  // The timer tested is the one the frame's time comes off and the one the send step reloads from the param (`fld
  // [mWaitRandom]; fadd st, st; fmulp; fadd [mWaitTimer]; fstp [the timer]`: the wait, its random and mMinEmNum one
  // after another, as the class registers them), the jbe's target the send step (its loop over mAddEmNum, the field
  // after mMinEmNum), the constant 0.
  const uintptr_t wait = game::sig_one("kSigEnemyAddWait");
  bool one_fn = wait && stock && wait > stock && wait - stock < 0x400;
  for (uintptr_t p = stock; one_fn && p < wait; ++p) one_fn = !(u8at(p) == 0xCC && u8at(p + 1) == 0xCC);
  if (one_fn && add) {
    const uint32_t min = u8at(wait + 2), timer = u8at(wait + 13);
    const auto rel = static_cast<uint8_t>(u8at(wait + 22));
    const uintptr_t zero = u32at(wait + 17), send = wait + 23 + static_cast<int8_t>(rel);
    float z = 1;
    const mem::Pattern reload_at = mem::parse_pattern("D9 43 ?? DC C0 DE C9 D8 43 ?? D9 5D ??");
    uintptr_t reload = 0;
    for (uintptr_t p = send; !reload && p < send + 0x80; ++p)
      if (mem::matches(p, reload_at) && u8at(p + 12) == timer) reload = p;
    const uint32_t w = reload ? u8at(reload + 9) : 0;
    char expect[8];
    std::snprintf(expect, sizeof(expect), "76 %02X", rel);
    if (u8at(wait + 34) == timer && u8at(wait + 37) == timer && image::rdata().contains(zero) &&
        mem::read_safe(zero, &z) && z == 0.0f && mem::matches(send, mem::parse_pattern("33 F6 39 73 ??")) &&
        u8at(send + 4) == min + 4 && reload && u8at(reload + 2) == w + 4 && min == w + 8 && min + 8 <= add->size &&
        g_wait_jump.prepare(wait + 21, expect, {0xEB, rel}, "the stage's sender's wait")) {
      g_wait_timer = timer;
      g_wait_param = w;
      g_wait_min = min;
    }
  }
  if (g_wait_jump.prepared())
    logf("merc: a stage's sender's wait (its test exe+0x%X; the work's timer +0x%X, from its param's mWaitTimer "
         "+0x%X) - while Kill enemies as they appear acts, a sender sends whenever fewer than its mMinEmNum (+0x%X) "
         "are in play",
         static_cast<unsigned>(g_wait_jump.at - image::base()), g_wait_timer, g_wait_param, g_wait_min);
  else
    logf("merc: a stage's sender's wait is not as known - each sender keeps its wait between enemies");
  // A body's end: the enemies' brains' end step - uEmIntel's own, in its vtable (each family's brain ends its own with
  // it: the J'avo's, the zombies' and the animals' call it last) - ends the enemy whose end byte is set, the last
  // thing every erase does: the brain's mRno and the enemy's made 3 (the same field, as each is a unit).
  const uintptr_t brain_end = game::sig_one("kSigBrainEnd");
  const mt::Class* intel = mt::find("uEmIntel");
  bool in_vtable = false;
  for (int k = 5; brain_end && intel && intel->vtable && mem::readable(intel->vtable, 4 * 64) && k < 64 && !in_vtable;
       ++k)
    in_vtable = mem::read<uintptr_t>(intel->vtable + 4 * k) == brain_end;
  const uint32_t end_byte = brain_end ? u32at(brain_end + 13) : 0;
  if (in_vtable && u8at(brain_end + 22) == u8at(brain_end + 29) && u8at(brain_end + 10) + 4 <= intel->size &&
      end_byte > g_k.reported && end_byte + 1 <= g_cls_enemy->size) {
    g_k.ended = end_byte;
    g_brain_end = brain_end;
    logf("merc: a body's end: the enemies' brains end one whose byte +0x%X is set (uEmIntel's exe+0x%X, the last "
         "step of every erase) - while Kill enemies as they appear acts, a body is ended so as its kill counts: no "
         "death motion, no dissolve",
         g_k.ended, static_cast<unsigned>(brain_end - image::base()));
  } else {
    logf("merc: the enemies' brains' end step is not as known - a body plays its death (its erase's waits short)");
  }
  // AreaHit's area test: an area task's player test.
  const uintptr_t area = game::sig_one("kSigAreaHitInside");
  const uintptr_t inside = area ? game::call_target(area + 17) : 0;
  if (inside && image::text().contains(inside) && mem::matches(inside, mem::parse_pattern(RE6CC_AREA_INSIDE)) &&
      prepare_call(g_inside_call, area + 17, reinterpret_cast<void*>(&hk_area_inside), "AreaHit's area test"))
    g_inside_fn = reinterpret_cast<InsideFn>(inside);
  if (g_inside_fn)
    logf("merc: AreaHit's area test (exe+0x%X -> exe+0x%X) - while Kill enemies as they appear acts, the player is in "
         "every area: a stage that sends by area sends from its first",
         static_cast<unsigned>(area + 17 - image::base()), static_cast<unsigned>(inside - image::base()));
  else
    logf("merc: AreaHit's area test is not as known - a stage that sends by area sends from the player's");
}

// The stall: the stage's kills standing for kStallMs while the switch acts - the enemies in play logged, once a stall.
constexpr DWORD kStallMs = 20000;
int g_stall_kills = -1;
DWORD g_stall_since = 0;
bool g_stall_logged = false;
void stall_check(int kills, float clock) {
  const DWORD now = GetTickCount();
  if (kills != g_stall_kills || !g_stall_since) {
    g_stall_kills = kills;
    g_stall_since = now;
    g_stall_logged = false;
    return;
  }
  if (g_stall_logged || now - g_stall_since < kStallMs) return;
  g_stall_logged = true;
  EnterCriticalSection(&g_play_lock);
  int alive = 0, bodies = 0, parts = 0;
  for (const Foe& f : g_foes) {
    if (f.owner) ++parts;
    else if (f.dead) ++bodies;
    else ++alive;
  }
  char scripts[96] = "none";
  for (int i = 0, at = 0; i < g_sent_count && at < static_cast<int>(sizeof(scripts)) - 12; ++i)
    at += std::snprintf(scripts + at, sizeof(scripts) - at, "%s0x%X", i ? ", " : "", g_sent[i].groups);
  logf("merc: the stage has sent no enemy for %u s (kills %d, the clock %.0f s; its scripts' senders' groups so far: "
       "%s): %d enemies in play, %d bodies, %d parts of others",
       static_cast<unsigned>(kStallMs / 1000), kills, static_cast<double>(clock), scripts, alive, bodies, parts);
  const game::Layout& lay = game::layout();
  int shown = 0;
  for (const Foe& f : g_foes) {
    if (f.owner || f.dead || shown >= 12) continue;
    ++shown;
    const uintptr_t u = f.obj;
    logf("merc:   %s %08X: health %d of %d, state %u, unit %u, flags 0x%08X, record %s; %d hits written, %d taken and "
         "lived on, %d not taken",
         name_of(f.cls), static_cast<unsigned>(u), field<int16_t>(u, lay.hp), field<int16_t>(u, lay.hp_max),
         field<uint8_t>(u, g_k.state), field<uint32_t>(u, units::layout().unit_flags) & 7, field<uint32_t>(u, g_k.flags),
         field<uint8_t>(u, g_k.valid) ? "busy" : "free", f.writes, f.shrugs, f.unread);
  }
  LeaveCriticalSection(&g_play_lock);
}

void follow(mem::Patch& p, bool on) {
  if (!p.prepared()) return;
  if (on && !p.applied && !p.apply()) logf("merc: the %s patch could not be put in", p.label);
  if (!on && p.applied && !p.revert()) logf("merc: the %s patch was not ours to take out any more", p.label);
}

}  // namespace

bool discover_match() {
  if (!g_lock_ready) {
    InitializeCriticalSection(&g_lock);
    InitializeCriticalSection(&g_play_lock);
    g_lock_ready = true;
  }
  const game::Layout& lay = game::layout();
  const mt::Class* gm = mt::find("sGameMan");
  const uintptr_t combo = game::sig_one("kSigMercCombo"), end = game::sig_one("kSigMercComboEnd"),
                  ct = game::sig_one("kSigMercComboTime"), pickup = game::sig_one("kSigMercComboTimePickup"),
                  target = game::sig_one("kSigMercTarget");
  if (!gm || !lay.mode || !combo || !end || !ct || !pickup || !target) {
    logf("merc: the match was not found - no Mercenaries section");
    return false;
  }
  MatchLayout m;
  m.running = u32at(combo + 21);
  m.combo_clock = u32at(combo + 43);
  m.combo = u32at(end + 32);
  m.best = u32at(end + 39);
  m.score = u32at(end + 56);
  m.earned = u32at(end + 62);
  m.ct_running = u32at(ct + 84);
  m.ct_clock = u32at(ct + 95);
  m.ct_kills = u32at(ct + 156);
  m.kills = u32at(target + 19);
  m.mode_merc = u8at(ct + 8);
  m.mode_no_mercy = u8at(ct + 13);
  m.cap_no_mercy = static_cast<int>(u32at(end + 87));
  m.cap = static_cast<int>(u32at(end + 102));
  const uint32_t stride = u32at(combo + 11);
  bool ok = u32at(combo + 34) == lay.mode && u32at(end + 10) == lay.mode && u32at(end + 76) == lay.mode &&
            u32at(ct + 2) == lay.mode && u32at(pickup + 5) == lay.mode && u32at(target + 2) == lay.mode &&
            u8at(target + 13) == m.mode_merc && u8at(target + 40) == m.mode_no_mercy && u8at(end + 80) == m.mode_no_mercy &&
            m.mode_merc != m.mode_no_mercy && u32at(end + 25) == stride && u32at(ct + 76) == stride &&
            u32at(end + 127) == m.running && u32at(end + 70) == m.combo && u32at(end + 50) == m.best &&
            u32at(end + 115) == m.score && u32at(end + 121) == m.earned && u32at(ct + 149) == m.ct_running &&
            u32at(target + 45) == m.kills && u32at(pickup + 19) == lay.match_step && u32at(target + 34) == lay.match_end &&
            u32at(target + 59) == lay.match_end && static_cast<int>(u32at(end + 94)) == m.cap_no_mercy &&
            static_cast<int>(u32at(end + 109)) == m.cap && m.cap > 0 && m.cap_no_mercy > m.cap &&
            int_in_image(u32at(target + 25), 1, 1000, &m.target) &&
            int_in_image(u32at(target + 51), 1, 1000, &m.target_no_mercy) &&
            float_in_image(u32at(pickup + 88), 1.0f, 600.0f, &m.ct_most);
  // Block 0's fields lie within one block, inside sGameMan.
  for (const uint32_t f : {m.running, m.combo, m.best, m.combo_clock, m.ct_running, m.ct_clock, m.ct_kills, m.earned,
                           m.score})
    ok = ok && f >= m.running && f < m.running + stride && f + 4 <= gm->size;
  ok = ok && m.kills + 4 <= gm->size && stride > 0;
  if (!ok) {
    logf("merc: the match's fields are not as known (their reads disagree) - no Mercenaries section");
    return false;
  }
  m.stride = stride;
  m.size = gm->size;
  g_m = m;
  g_match_ok = true;
  // The kill award's call: after the combo marked running (its byte), to the function that adds to what it earned,
  // at most the cap.
  const uintptr_t award = game::sig_one("kSigComboAwardCall"), award_end = game::sig_one("kSigComboAwardEnd");
  const uintptr_t award_fn = award ? game::call_target(award + 15) : 0;
  if (award && award_end && award_fn && u32at(award + 10) == m.running && u32at(award_end + 6) == m.earned &&
      u32at(award_end + 25) == m.earned && static_cast<int>(u32at(award_end + 12)) == m.cap &&
      static_cast<int>(u32at(award_end + 19)) == m.cap && award_end > award_fn && award_end < award_fn + 0x200) {
    char expect[24];
    std::snprintf(expect, sizeof(expect), "E8 %02X %02X %02X %02X", u8at(award + 16), u8at(award + 17),
                  u8at(award + 18), u8at(award + 19));
    std::vector<uint8_t> call(5);
    call[0] = 0xE8;
    const auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&hk_combo_award) - (award + 15 + 5));
    std::memcpy(call.data() + 1, &rel, 4);
    if (g_award_call.prepare(award + 15, expect, call, "kill award")) g_award = reinterpret_cast<AwardFn>(award_fn);
  }
  if (!g_award) logf("merc: the kill award is not as known - Endless combo's earnings wait for the combo's end");
  if (!g_combo_clock.prepare(combo + 47, "F3 0F 5C 44 24 14", mem::nops(6), "combo's clock"))
    logf("merc: the combo's clock is not as known - no Endless combo");
  if (!g_ct_clock.prepare(ct + 99, "F3 0F 5C 44 24 0C", mem::nops(6), "Combo Time's clock"))
    logf("merc: Combo Time's clock is not as known - no Combo Time always on");
  logf("merc: the match (modes 0x%X, 0x%X): the score +0x%X (at most %d / %d), earned +0x%X, the combo +0x%X (running "
       "+0x%X, its window +0x%X, the best +0x%X), Combo Time +0x%X (%.0f s, its clock +0x%X, kills +0x%X), the stage's "
       "kills +0x%X (%d / %d clear it)",
       m.mode_merc, m.mode_no_mercy, m.score, m.cap, m.cap_no_mercy, m.earned, m.combo, m.running, m.combo_clock, m.best,
       m.ct_running, static_cast<double>(m.ct_most), m.ct_clock, m.ct_kills, m.kills, m.target, m.target_no_mercy);
  return true;
}

bool discover_kills(bool take_damage_hooked) {
  const game::Layout& lay = game::layout();
  g_cls_enemy = mt::find("uEnemy");
  const mt::Class* dmg = mt::find("DamageInfo");
  const mt::Class* coord = mt::find("uCoord");
  const mt::Class* player = mt::find("uPlayer");
  const uintptr_t hc = game::sig_one("kSigHitCheck"), rb = game::sig_one("kSigHitRebuild"),
                  rp = game::sig_one("kSigHitRebuildPos"), kr = game::sig_one("kSigKillReport"),
                  ko = game::sig_one("kSigKillOnce");
  const uintptr_t props = coord && coord->vtable ? mem::read<uintptr_t>(coord->vtable + 4 * 3) : 0;
  if (!g_cls_enemy || !dmg || !dmg->vtable || !coord || !player || !hc || !rb || !rp || !kr || !ko || !props ||
      !lay.hp || !lay.hit_damage || !image::text().contains(props) ||
      !mem::matches(props, mem::parse_pattern(RE6CC_COORD_PROPS))) {
    logf("merc: an enemy's hit record was not found - no Kill enemies as they appear");
    return false;
  }
  KillLayout k;
  k.valid = u32at(hc + 49);
  k.record = u32at(hc + 57);
  k.damage = u8at(rb + 25);
  k.attrs = u8at(rb + 66);
  k.nattrs = u8at(rp + 10) / 32;
  k.pos = u8at(rp + 28);
  k.dir = u8at(rp + 62);
  k.word60 = u8at(rp + 94);
  k.word64 = u8at(rp + 104);
  k.attack = u8at(rp + 113);
  k.dealer = u8at(rp + 122);
  k.byte30 = u8at(rp + 131);
  k.byte31 = u8at(rp + 140);
  k.state = u8at(kr + 2);
  k.died = u32at(kr + 22);
  k.flags = u32at(ko + 30);
  k.reported = u32at(ko + 47);
  k.coord_pos = u8at(props + 27);
  k.damage_vt = dmg->vtable;
  const uintptr_t report = game::call_target(kr + 93);
  const char* pos_name = reinterpret_cast<const char*>(static_cast<uintptr_t>(u32at(props + 34)));
  // The record's fields as the network writes them, the valid byte as the brain reads it,
  // the damage and the dealer as the players' hits have them; the kill report the one the
  // damage handler calls; the position mPos by its own name.
  const bool ok = k.valid == k.record + u8at(rb + 14) && k.damage == lay.hit_damage && k.dealer == lay.hit_attacker &&
                  u8at(rp + 39) == k.pos + 4 && u8at(rp + 53) == k.pos + 8 && u8at(rp + 50) == k.pos + 12 &&
                  u8at(rp + 71) == k.dir + 4 && u8at(rp + 85) == k.dir + 8 && u8at(rp + 82) == k.dir + 12 &&
                  k.nattrs >= 1 && k.nattrs <= 4 && k.record + dmg->size <= g_cls_enemy->size &&
                  u32at(kr + 9) == lay.hp && u32at(kr + 81) == k.died && report && ko >= report && ko < report + 0x40 &&
                  k.died + 1 <= g_cls_enemy->size && k.reported + 1 <= g_cls_enemy->size &&
                  k.flags + 4 <= g_cls_enemy->size && image::whole().contains(reinterpret_cast<uintptr_t>(pos_name)) &&
                  mem::readable(reinterpret_cast<uintptr_t>(pos_name), 5) && !std::strcmp(pos_name, "mPos") &&
                  k.coord_pos + 12 <= coord->size;
  if (!ok) {
    logf("merc: an enemy's hit record is not as known (its reads disagree) - no Kill enemies as they appear");
    return false;
  }
  // The enemies' coming and going: every enemy class's start and destructor (whatever each has there), in the slots
  // sUnit's line frame calls.
  const units::Layout& ul = units::layout();
  const int starts = ul.start_slot >= 0 ? vthook::hook(g_cls_enemy, ul.start_slot, 0, false,
                                                       reinterpret_cast<void*>(&hk_enemy_start), &g_starts)
                                        : 0;
  const int dtors = ul.destroy_slot >= 0 ? vthook::hook(g_cls_enemy, ul.destroy_slot, 0, false,
                                                        reinterpret_cast<void*>(&hk_enemy_dtor), &g_dtors)
                                         : 0;
  if (!starts || starts != dtors) {
    vthook::unhook(g_starts);
    vthook::unhook(g_dtors);
    logf("merc: the enemies' start and destruction could not be hooked (%d, %d) - no Kill enemies as they appear",
         starts, dtors);
    return false;
  }
  logf("merc: the enemies' start (slot %d) and destructor (slot %d) hooked in %d vtables - each enemy known as it "
       "comes and goes",
       ul.start_slot, ul.destroy_slot, starts);
  g_k = k;
  g_kills_ok = true;
  g_lethal_ok = take_damage_hooked;
  logf("merc: an enemy's hit record +0x%X (valid +0x%X; damage +0x%X, dealer +0x%X, attack +0x%X, where +0x%X, "
       "towards +0x%X, its part +0x%X); a unit's position +0x%X",
       k.record, k.valid, k.damage, k.dealer, k.attack, k.pos, k.dir, k.word64, k.coord_pos);
  discover_flow();
  if (g_lethal_ok)
    logf("merc: Kill enemies as they appear: every enemy in play tried until it is dead (%d a frame at most); a hit of "
         "the mod's takes all the health in the enemies' take-damage (slot %d, hooked) - harder (%d) once one lives "
         "through one, all its health even from nothing after %d, another part of it each time after %d",
         kPerTick, game::kEnemyApplySlot, kBigHit, kForceAfter, kSpreadAfter);
  else
    logf("merc: Kill enemies as they appear: the enemies' take-damage is not hooked - a hit of the mod's takes what "
         "each enemy's own damage code makes of all its health (one whose code shrinks it lives on)");
  return true;
}

bool tick(const game::Run& run, const Wanted& want) {
  if (!g_lock_ready) return false;
  const game::Layout& lay = game::layout();
  const uintptr_t gm = run.in_game && run.mode_known ? game::sgameman() : 0;
  Status s;
  const uint32_t mode = gm ? run.mode : 0;
  const bool merc_mode = g_match_ok && gm && (mode == g_m.mode_merc || mode == g_m.mode_no_mercy);
  const int step = merc_mode ? field<int32_t>(gm, lay.match_step) : -1;
  s.match = merc_mode && step >= 1 && step <= 3;
  bool acts = false;
  if (s.match) {
    s.no_mercy = mode == g_m.mode_no_mercy;
    s.offline = !run.online && field<int32_t>(gm, lay.match_host) == field<int32_t>(gm, lay.match_me);
    s.step = step;
    acts = s.offline && step == 1 && field<int32_t>(gm, lay.match_end) == 0 && field<float>(gm, lay.match_clock) > 0;
  }
  // The patches and the kill award's call, put in or taken out as what they follow changes: Endless combo's window
  // held and its earnings banked as earned, Combo Time's clock held - while the switch is on in a match played here.
  const bool endless = acts && want.endless_combo, timed = acts && want.combo_time;
  const uint32_t mask = (endless ? 1u : 0u) | (timed ? 2u : 0u);
  if (mask != g_patched) {
    follow(g_combo_clock, endless);
    follow(g_award_call, endless && g_award);
    follow(g_ct_clock, timed);
    g_patched = mask;
  }
  InterlockedExchange(&g_banking, endless && g_award_call.applied ? (s.no_mercy ? 2 : 1) : 0);
  // Combo Time always on: started as its hourglass starts it (its clock, its kills, then running) as the switch goes
  // on in a match, or a match begins with it on - its clock held from then on (only the match's end stops it).
  if (timed && !g_timed_was && g_ct_clock.applied && !field<uint8_t>(gm, g_m.ct_running)) {
    field<float>(gm, g_m.ct_clock) = g_m.ct_most;
    field<int32_t>(gm, g_m.ct_kills) = 0;
    field<uint8_t>(gm, g_m.ct_running) = 1;
    logf("merc: Combo Time started (%.0f s, held)", static_cast<double>(g_m.ct_most));
  }
  g_timed_was = timed;
  // Kill enemies as they appear: the dealer, and every enemy in play hit until it is dead (while the switch is on and
  // the match plays here; the last tick's hits seen to whatever the switch).
  const bool killing = g_kills_ok && want.kill && acts;
  const game::Character* by = killing ? dealer_of(run) : nullptr;
  const mt::Class* by_cls = by ? units::class_of(by->obj) : nullptr;
  g_dealer = by ? Dealer{by->obj, by->name, by_cls ? by_cls->id : 0} : Dealer{};
  if (g_kills_ok && (killing || g_npending)) kill_tick(killing, run.paused);
  else g_writing = 0;
  // The flow: the bodies' erases short and the area test the mod's while it acts (not online: acts is offline only).
  if (killing != g_flow_was) {
    InterlockedExchange(&g_flowing, killing ? 1 : 0);
    for (mem::Patch& p : g_erase_loads) follow(p, killing);
    follow(g_stock_call, killing && g_stock_fn);
    follow(g_wait_jump, killing);
    follow(g_inside_call, killing && g_inside_fn);
    g_flow_was = killing;
  }
  // One match: a match ending - or one beginning with the switch already on (from the
  // last one, or the ini) - switches Kill enemies as they appear off. What the mod did to
  // each kind of enemy goes into the log as a match ends.
  const bool playing = step == 1, was_playing = g_last_step == 1 && mode == g_last_mode;
  const bool off = want.kill && g_kills_ok && playing != was_playing;
  if (g_kills_ok && was_playing && !playing) log_tally();
  if (g_kills_ok && playing && !was_playing) {
    restart_tally();
    g_sent_count = 0;
    g_stock_logged = 0;
  }
  // A stage that has sent nothing for a while (its kills standing) with the switch on: what is in play, once.
  if (killing && s.match) stall_check(field<int32_t>(gm, g_m.kills), field<float>(gm, lay.match_clock));
  else g_stall_since = 0;
  g_last_step = step;
  g_last_mode = mode;
  // The numbers, as the switches left them (read only while the panel can show them).
  if (s.match && run.panel) {
    s.time_left = field<float>(gm, lay.match_clock);
    s.score = field<int32_t>(gm, g_m.score);
    s.waiting = field<int32_t>(gm, g_m.earned);
    s.combo = field<int32_t>(gm, g_m.combo);
    s.best_combo = field<int32_t>(gm, g_m.best);
    if (s.combo > s.best_combo) s.best_combo = s.combo;
    s.combo_left = field<uint8_t>(gm, g_m.running) ? field<float>(gm, g_m.combo_clock) : 0;
    s.kills = field<int32_t>(gm, g_m.kills);
    s.target = s.no_mercy ? g_m.target_no_mercy : g_m.target;
    s.combo_time = field<uint8_t>(gm, g_m.ct_running) != 0;
    s.combo_time_left = s.combo_time ? field<float>(gm, g_m.ct_clock) : 0;
    s.combo_time_kills = field<int32_t>(gm, g_m.ct_kills);
    if (killing) {
      EnterCriticalSection(&g_play_lock);
      for (const Foe& f : g_foes) s.killing += !f.dead && !f.owner;
      LeaveCriticalSection(&g_play_lock);
    }
  }
  s.killed = g_killed;
  EnterCriticalSection(&g_lock);
  g_status = s;
  LeaveCriticalSection(&g_lock);
  return off;
}

Status status() {
  if (!g_lock_ready) return Status{};
  EnterCriticalSection(&g_lock);
  const Status s = g_status;
  LeaveCriticalSection(&g_lock);
  return s;
}

bool match_found() { return g_match_ok; }
bool kills_found() { return g_kills_ok; }

int lethal(void* self, void* hit, int amount) {
  // Every hit on every enemy comes here (a campaign's too): nothing to look up while no hit of the mod's is waiting.
  if (!g_kills_ok || !g_lock_ready || !g_npending || !self || !hit) return amount;
  const auto e = reinterpret_cast<uintptr_t>(self);
  const uintptr_t owner = reinterpret_cast<uintptr_t>(hit) - g_k.record;  // the enemy whose record the hit is
  EnterCriticalSection(&g_play_lock);
  Foe* f = foe_of(owner);
  if (f && f->pending && holds(*f, true)) {
    f->applied = true;
    f->took = amount;
    // A part's record handed to the enemy it is part of (a J'avo's mutation: its brain takes its parts' hits).
    if (owner != e) {
      f->owner = e;
      if (Foe* o = foe_of(e)) o->applied = true;
    }
    const int hp = field<int16_t>(e, game::layout().hp);
    if (hp > 0 && amount < hp && (amount > 0 || f->shrugs >= kForceAfter)) amount = hp;
  }
  LeaveCriticalSection(&g_play_lock);
  return amount;
}

void remove_patches() {
  g_combo_clock.revert();
  g_ct_clock.revert();
  g_award_call.revert();
  for (mem::Patch& p : g_erase_loads) p.revert();
  g_stock_call.revert();
  g_wait_jump.revert();
  g_inside_call.revert();
  InterlockedExchange(&g_flowing, 0);
  InterlockedExchange(&g_banking, 0);
  g_dealer = Dealer{};
  vthook::unhook(g_starts);
  vthook::unhook(g_dtors);
  g_kills_ok = false;
}

bool endless_found() { return g_match_ok && g_combo_clock.prepared() && g_award != nullptr; }
bool combo_time_found() { return g_match_ok && g_ct_clock.prepared(); }

}  // namespace re6cc::merc
