#include "game.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "image.h"
#include "log.h"
#include "mem.h"
#include "mt.h"
#include "sigs.h"
#include "units.h"

namespace re6cc::game {
namespace {

bool g_ready = false;
char g_status[160] = "waiting for the game's renderer to start";
Layout g_lay;

const mt::Class* g_cls_system = nullptr;  // cSystem: every s* singleton derives from it
const mt::Class* g_cls_main = nullptr;    // sBioMain
const mt::Class* g_cls_player = nullptr;  // uPlayer: Leon, Helena, Chris, ... and the modes' others
const mt::Class* g_cls_plplayer = nullptr;  // uPlPlayer: the campaigns' and the Mercenaries' characters
const mt::Class* g_cls_enemy = nullptr;   // uEnemy
const mt::Class* g_cls_chara = nullptr;   // uCharacter
const mt::Class* g_cls_splayer = nullptr; // sPlayer
const mt::Class* g_cls_info = nullptr;    // sInfoMan
const mt::Class* g_cls_gameman = nullptr; // sGameMan
const mt::Class* g_cls_gameinfo = nullptr;  // cGameInfo: a profile's chapter and its numbers
const mt::Class* g_cls_stage = nullptr;   // aStageBase: a stage (its scenario timers)
uintptr_t g_splayer = 0, g_sinfo = 0, g_sgameman = 0;

// The systems the main system holds, by class name (from the first tick).
std::map<std::string, uintptr_t> g_systems;
bool g_systems_seen = false;

// --- signatures ---------------------------------------------------------------------------
struct Sig {
  const char* name;
  const char* pattern;
  int matches;
};
#define RE6CC_SIG_ENTRY(n, p, m) {#n, p, m},
constexpr Sig kSigs[] = {RE6CC_SIGS(RE6CC_SIG_ENTRY)};
#undef RE6CC_SIG_ENTRY

const Sig* sig(const char* name) {
  for (const Sig& s : kSigs)
    if (!std::strcmp(s.name, name)) return &s;
  return nullptr;
}

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }
uintptr_t vslot(const mt::Class* c, int slot) { return c && c->vtable ? mem::read<uintptr_t>(c->vtable + 4 * slot) : 0; }

// What failed, for the panel and the log.
bool fail(const char* what) {
  std::snprintf(g_status, sizeof(g_status), "%s was not found in the game (see the log)", what);
  logf("ERROR: game: %s not found or not as known - the mod stays off", what);
  return false;
}

// Every pointer in the main system's instance to an object of a cSystem class:
// MT Framework's singletons, each named by its class record.
void survey_systems(uintptr_t main) {
  g_systems_seen = true;
  const uint32_t size = g_cls_main ? g_cls_main->size : 0;
  if (!size || !mem::readable(main, size)) {
    logf("game: the main system %08X is not readable over its %u bytes - no systems", static_cast<unsigned>(main), size);
    return;
  }
  const mem::Range img = image::whole();
  int found = 0;
  for (uint32_t off = 0; off + 4 <= size; off += 4) {
    const auto v = mem::read<uintptr_t>(main + off);
    if (!v || (v & 3) || img.contains(v) || v == main) continue;
    uintptr_t vt = 0;
    if (!mem::read_safe(v, &vt) || !image::rdata().contains(vt)) continue;
    const uintptr_t dti = mt::dti_of_object(v);
    if (!dti || !g_cls_system || !mt::derives(dti, g_cls_system->dti)) continue;
    const char* name = mt::class_name_of(v);
    if (!name || g_systems.count(name)) continue;
    g_systems[name] = v;
    ++found;
    if (config::get().trace) logf("game: system %-24s %08X (sBioMain+0x%X)", name, static_cast<unsigned>(v), off);
  }
  std::string names;
  for (const auto& kv : g_systems) {
    if (!names.empty()) names += ", ";
    names += kv.first;
  }
  logf("game: the main system %08X holds %d systems: %s", static_cast<unsigned>(main), found, names.c_str());
  g_splayer = system("sPlayer");
  g_sinfo = system("sInfoMan");
  g_sgameman = system("sGameMan");
  if (g_splayer && !mt::is_a(g_splayer, g_cls_splayer)) g_splayer = 0;
  if (g_sinfo && !mt::is_a(g_sinfo, g_cls_info)) g_sinfo = 0;
  if (g_sgameman && (!g_cls_gameman || !mt::is_a(g_sgameman, g_cls_gameman))) g_sgameman = 0;
  if (!g_sgameman) logf("game: sGameMan missing - the game's mode is not known");
  if (!g_splayer || !g_sinfo)
    logf("ERROR: game: sPlayer %s, sInfoMan %s", g_splayer ? "found" : "missing", g_sinfo ? "found" : "missing");
}

// --- discovery of the layout ------------------------------------------------------------
// The pause: sBioMain's frame (vtable slot 6) compares what was asked for (a byte
// the Start button sets, and the pause screen's own Start clears) with whether the
// game is paused (another byte), and switches: paused, its frame runs only what a
// pause keeps running (the screens, the sounds' fade) and the units stand still.
bool discover_pause() {
  const uintptr_t at = sig_one("kSigMainPause");
  const uintptr_t frame = vslot(g_cls_main, 6);
  if (!at || !image::text().contains(frame) || at < frame || at > frame + 0x100) return fail("the game's pause");
  g_lay.paused = u32at(at + 10);
  const uint32_t asked = u32at(at + 2);
  if (u32at(at + 31) != g_lay.paused || g_lay.paused + 1 > g_cls_main->size || asked + 1 > g_cls_main->size ||
      asked == g_lay.paused)
    return fail("the game's pause (its fields disagree)");
  logf("game: the game is paused while sBioMain +0x%X is set (asked for at +0x%X; its frame exe+0x%X)", g_lay.paused,
       asked, static_cast<unsigned>(frame - image::base()));
  return true;
}

bool discover_characters() {
  // Health: uCharacter's subtract-health, its vtable slot 44 (every character's;
  // the players' and the enemies' damage both come to it).
  const uintptr_t sub = sig_one("kSigSubLife");
  if (!sub || sub != vslot(g_cls_chara, kSubLifeSlot)) return fail("the characters' health");
  g_lay.hp = u32at(sub + 8);
  g_lay.hp_max = u32at(sub + 26);
  if (u32at(sub + 19) != g_lay.hp || u32at(sub + 33) != g_lay.hp || u32at(sub + 42) != g_lay.hp ||
      g_lay.hp_max != g_lay.hp + 2 || g_lay.hp_max + 2 > g_cls_chara->size)
    return fail("the characters' health (its fields disagree)");
  // sPlayer's characters and who drives each: its hand-over of one.
  const uintptr_t ctl = sig_one("kSigPlayerControl");
  if (!ctl) return fail("sPlayer's characters");
  g_lay.players = u8at(ctl + 19);
  g_lay.nplayers = static_cast<int>(u8at(ctl + 12));
  g_lay.control = u32at(ctl + 38);
  if (u32at(ctl + 56) != g_lay.control || g_lay.nplayers < 1 || g_lay.nplayers > kMaxChars ||
      g_lay.players + 4 * g_lay.nplayers > g_cls_splayer->size || g_lay.control + 4 > g_cls_player->size)
    return fail("sPlayer's characters (they do not fit)");
  // A player's take-damage (uPlayer's slot 121) lets off a character this machine
  // does not drive by the same word.
  const uintptr_t apply = sig_one("kSigPlayerApply");
  if (!apply || apply != vslot(g_cls_player, kPlayerApplySlot) || u32at(apply + 107) != g_lay.control)
    return fail("the players' damage");
  // A player's hit (uPlPlayer's slot 117): the hit's damage and who dealt it.
  const uintptr_t hit = sig_one("kSigPlayerHit");
  const uintptr_t hit_fn = vslot(g_cls_plplayer, kPlayerHitSlot);
  if (!hit || !image::text().contains(hit_fn) || hit < hit_fn || hit > hit_fn + 0x200)
    return fail("the players' hits");
  g_lay.hit_damage = u8at(hit + 48);
  g_lay.hit_attacker = u8at(hit + 15);
  // An enemy's take-damage (uEnemy's slot 128) hands the amount to subtract-health
  // through the vtable.
  const uintptr_t eapply = sig_one("kSigEnemyApply");
  if (!eapply || eapply != vslot(g_cls_enemy, kEnemyApplySlot) || u32at(eapply + 70) != 4u * kSubLifeSlot)
    return fail("the enemies' damage");
  logf("game: health +0x%X (most +0x%X), who drives a player +0x%X, sPlayer's %d characters +0x%X; a hit's damage +0x%X, "
       "its dealer +0x%X",
       g_lay.hp, g_lay.hp_max, g_lay.control, g_lay.nplayers, g_lay.players, g_lay.hit_damage, g_lay.hit_attacker);
  return true;
}

// The game's own infinite ammo: a bit of a word of the player's, under which a shot
// uses no rounds and a reload fills the gun from nothing. Not a failure when it is not
// as known: Infinite ammo is then not offered.
void discover_ammo() {
  const uintptr_t use = sig_one("kSigAmmoUse");
  const uintptr_t reload = sig_one("kSigAmmoReload");
  if (!use || !reload) {
    logf("game: the game's own infinite ammo was not found - no Infinite ammo");
    return;
  }
  const uint32_t inv = u32at(use + 15), size = u8at(use + 32), n = u8at(use + 35);
  const uint32_t flags = u32at(use + 56), bit = u8at(use + 62);
  if (u32at(reload + 2) != flags || u8at(reload + 8) != bit || bit > 31 || flags + 4 > g_cls_plplayer->size ||
      !size || !n || inv + size * n > g_cls_plplayer->size) {
    logf("game: the game's own infinite ammo is not as known (the shot's and the reload's disagree) - no Infinite ammo");
    return;
  }
  g_lay.inventory = inv;
  g_lay.item_size = size;
  g_lay.nitems = static_cast<int>(n);
  g_lay.ammo_flags = flags;
  g_lay.ammo_bit = bit;
  logf("game: the game's own infinite ammo: bit %u of a player's +0x%X (the inventory +0x%X: %u items of %u bytes)", bit,
       flags, inv, n, size);
}

// An item's fields (cItem): in use and its slot number from the shot's own search of
// the inventory, its id, mode and counts from cItem's set-count, its mosts from the
// get-count/get-most pair (the one whose words are not the counts). Not a failure when
// they are not as known: no inventories in the panel then.
void discover_item_fields() {
  const uintptr_t use = sig_one("kSigAmmoUse"), set = sig_one("kSigItemSetNum");
  const std::vector<uintptr_t> get = sig_all("kSigItemGet");
  if (!use || !set || get.size() != 2 || !g_lay.item_size) {
    logf("game: an item's fields were not found - no inventories");
    return;
  }
  const uint32_t used = u8at(use + 46), slot = u8at(use + 24), id = u8at(set + 22), mode = u8at(set + 11),
                 num = u8at(set + 59);
  const uint32_t a = u8at(get[0] + 54), b = u8at(get[1] + 54);
  const uint32_t most = a == num ? b : b == num ? a : 0;
  // Both getters and the setter agree on the mode and the id; three counts, then three mosts.
  if (!most || most != num + 6 || u8at(get[0] + 11) != mode || u8at(get[1] + 11) != mode || u8at(get[0] + 22) != id ||
      u8at(get[1] + 22) != id || most + 6 > g_lay.item_size || used >= id || slot >= id) {
    logf("game: an item's fields disagree (in use +0x%X, slot +0x%X, id +0x%X, mode +0x%X, counts +0x%X, mosts +0x%X) - "
         "no inventories",
         used, slot, id, mode, num, most);
    return;
  }
  // The slot in hand: the reload's own search of the inventory, agreeing on it.
  const uintptr_t hand = sig_one("kSigInHand");
  if (!hand || u32at(hand + 17) != g_lay.inventory || u8at(hand + 26) != slot || u8at(hand + 34) != g_lay.item_size ||
      static_cast<int>(u8at(hand + 37)) != g_lay.nitems || u32at(hand + 50) != g_lay.control ||
      u32at(hand + 6) + 4 > g_cls_plplayer->size) {
    logf("game: the slot in hand was not found (or the reload's search disagrees) - no inventories");
    return;
  }
  g_lay.in_hand = u32at(hand + 6);
  g_lay.it_used = used;
  g_lay.it_slot = slot;
  g_lay.it_id = id;
  g_lay.it_mode = mode;
  g_lay.it_num = num;
  g_lay.it_max = most;
  logf("game: an item: in use +0x%X, slot +0x%X, id +0x%X, mode +0x%X, counts +0x%X, mosts +0x%X (3 of each, a word); "
       "the slot in hand uPlayer +0x%X",
       used, slot, id, mode, num, most, g_lay.in_hand);
}

// The profile's skill points: sInfoMan's set-skill-points, which the skill screen's
// purchases call. Not a failure when it is not as known: no skill points then.
void discover_skill_points() {
  const uintptr_t buy = sig_one("kSigSkillBuy"), level = sig_one("kSigSkillLevel");
  const uintptr_t set = buy ? call_target(buy + 59) : 0;
  if (!set || !level || call_target(level + 40) != set || !mem::matches(set, mem::parse_pattern(RE6CC_SKILL_POINTS_SET))) {
    logf("game: the skill points were not found - no skill points");
    return;
  }
  const mt::Class* sys = mt::find("cSystemInfo");
  const uint32_t index = u8at(set + 30), size = u32at(set + 37), points = u32at(set + 44);
  // Two profiles (sInfoMan's mSystem[0] and [1]): the second's points must lie inside sInfoMan too.
  if (!sys || size != sys->size || index + 4 > g_cls_info->size || points + size + 4 > g_cls_info->size) {
    logf("game: the skill points are not as known (a profile is not cSystemInfo's size) - no skill points");
    return;
  }
  g_lay.profile = index;
  g_lay.profile_size = size;
  g_lay.skill_points = points;
  logf("game: the skill points sInfoMan +0x%X + the profile (+0x%X) * 0x%X (set-skill-points exe+0x%X)", points, index,
       size, static_cast<unsigned>(set - image::base()));
}

// sGameMan's mode, whether a session is online, and the modes' match: read where the
// players' take-damage tests the mode, and where the match clock runs down (its start
// tells whether this machine runs the match), the match's frame calls it, and a time
// bonus adds to it. Not a failure when not as known: the mode is then not known (the
// panel shows the Results in any game, and the Mercenaries' clock is not frozen).
void discover_modes() {
  const uintptr_t apply = sig_one("kSigPlayerApply"), head = sig_one("kSigMercClockHead"),
                  clock = sig_one("kSigMercClock"), frame = sig_one("kSigMercFrame"), bonus = sig_one("kSigMercBonus");
  if (!apply || !head || !clock || !frame || !bonus || !g_cls_gameman) {
    logf("game: sGameMan's mode and match were not found - the mode is not known");
    return;
  }
  const uint32_t mode = u32at(apply + 10), left = u32at(clock + 10), hud = u32at(clock + 98), end = u32at(clock + 53),
                 online = u32at(clock + 63), host = u32at(head + 5), me = u32at(head + 11), step = u32at(frame + 6);
  const uintptr_t most_at = u32at(bonus + 4);
  float most = 0;
  const bool most_ok = image::whole().contains(most_at) && mem::read_safe(most_at, &most) && most > 60.0f && most < 360000.0f;
  bool ok = most_ok && u32at(clock + 2) == mode && u32at(clock + 80) == left && u32at(bonus + 19) == left &&
            u32at(head + 23) == end && call_target(frame + 87) == head && clock > head && clock < head + 0x100 &&
            host != me;
  for (const uint32_t f : {mode, left, hud, end, online, host, me, step}) ok = ok && f + 4 <= g_cls_gameman->size;
  if (!ok) {
    logf("game: sGameMan's mode and match are not as known (their reads disagree) - the mode is not known");
    return;
  }
  g_lay.mode = mode;
  g_lay.online = online;
  g_lay.match_clock = left;
  g_lay.match_hud = hud;
  g_lay.match_step = step;
  g_lay.match_end = end;
  g_lay.match_host = host;
  g_lay.match_me = me;
  g_lay.match_most = most;
  logf("game: sGameMan: the mode +0x%X, online +0x%X; the match clock +0x%X (the HUD's +0x%X, at most %.2f s), its step "
       "+0x%X and end +0x%X, the host +0x%X and this machine +0x%X",
       mode, online, left, hud, static_cast<double>(most), step, end, host, me);
}

// The chapter's numbers (the results'): a counter's add gives the blocks (one a character)
// and where character 0's counters are; the clock's add (called from sInfoMan's frame,
// which gives the profiles' cGameInfo in sInfoMan) where its clock is; the restore from the
// copy a save takes, where that copy is; the results' grading, which counters it reads; the
// Game Over's count of a death, that its counter is the grading's deaths and its add the
// same. Not a failure when not as known: no Results section then.
void discover_results() {
  const uintptr_t add = sig_one("kSigChapterCounterAdd"), loop = sig_one("kSigChapterClockLoop"),
                  restore = sig_one("kSigChapterRestore"), grade = sig_one("kSigChapterGrade"),
                  death = sig_one("kSigDeathCounted");
  const uintptr_t clock_add = loop ? call_target(loop + 29) : 0;
  const uintptr_t locked = death ? call_target(death + 29) : 0;
  if (!add || !loop || !restore || !grade || !death || !clock_add || !locked || !g_cls_gameinfo || !g_lay.profile_size ||
      !mem::matches(clock_add, mem::parse_pattern(RE6CC_CLOCK_ADD))) {
    logf("game: the chapter's numbers were not found - no Results");
    return;
  }
  const int nblocks = static_cast<int>(u8at(add + 6)), ncounters = static_cast<int>(u8at(add + 15));
  const uint32_t words = u8at(add + 20), counters = u32at(add + 26), most = u32at(add + 38);
  const uint32_t block = u32at(clock_add + 18), clock = u32at(clock_add + 27);
  const uint32_t profile = u8at(loop + 2), info_size = u32at(loop + 11), info = u32at(loop + 25);
  const uint32_t copy_size = u32at(restore + 7), copy = u8at(restore + 13), live = u32at(restore + 17);
  const int shots = static_cast<int>(u8at(grade + 1)), hits = static_cast<int>(u8at(grade + 61)),
            kills = static_cast<int>(u8at(grade + 117)), deaths = static_cast<int>(u8at(grade + 141));
  float clock_most = 0;
  const uintptr_t clock_most_at = u32at(clock_add + 41);
  const bool clock_most_ok = image::whole().contains(clock_most_at) && mem::read_safe(clock_most_at, &clock_most) &&
                             clock_most > 60.0f && clock_most < 10000000.0f;
  const bool distinct = shots != hits && shots != kills && shots != deaths && hits != kills && hits != deaths &&
                        kills != deaths && shots < ncounters && hits < ncounters && kills < ncounters &&
                        deaths < ncounters;
  // Two profiles' cGameInfo at least lie in sInfoMan, after its profiles' cSystemInfo.
  const bool ok = clock_most_ok && distinct && u32at(add + 45) == most && most > 0 && most <= 0xFFFF && nblocks > 0 &&
                  nblocks <= kMaxChars && static_cast<int>(u8at(loop + 37)) == nblocks && block == 2 * words &&
                  u32at(clock_add + 60) == block && u32at(clock_add + 69) == clock && counters == clock + 4 &&
                  counters + 2 * static_cast<uint32_t>(ncounters) <= clock + block && copy + copy_size == live &&
                  clock >= live && clock + nblocks * block <= live + copy_size && info_size == g_cls_gameinfo->size &&
                  profile == g_lay.profile && info >= g_lay.skill_points && info + 2 * info_size <= g_cls_info->size &&
                  static_cast<int>(u8at(death + 27)) == deaths && call_target(locked + 59) == add &&
                  u8at(locked + 43) == profile && u32at(locked + 46) == info_size && u32at(locked + 55) == info;
  if (!ok) {
    logf("game: the chapter's numbers are not as known (their reads disagree) - no Results");
    return;
  }
  g_lay.game_info = info;
  g_lay.game_info_size = info_size;
  g_lay.saved_back = live - copy;
  g_lay.clock = clock;
  g_lay.counters = counters;
  g_lay.block = block;
  g_lay.nblocks = nblocks;
  g_lay.ncounters = ncounters;
  g_lay.counter_most = static_cast<int>(most);
  g_lay.clock_most = clock_most;
  g_lay.c_shots = shots;
  g_lay.c_hits = hits;
  g_lay.c_kills = kills;
  g_lay.c_deaths = deaths;
  logf("game: the chapter's numbers: sInfoMan +0x%X + the profile * 0x%X (cGameInfo); a character's block of 0x%X from "
       "+0x%X: the clock (at most %.0f s), %d counters (at most %u) from +0x%X - shots %d, hits %d, kills %d, deaths %d; "
       "the copy a save takes 0x%X before",
       info, info_size, block, clock, static_cast<double>(clock_most), ncounters, most, counters, shots, hits, kills,
       deaths, g_lay.saved_back);
}

// A stage's scenario timers: their read (the stage's get, which the countdowns on
// screen call), the stage's frame running them (each running one counted down or up),
// and the two displays' calls of the get. Not a failure when not as known: no countdown
// of a stage's then (the Mercenaries' clock is apart).
void discover_stage_timers() {
  const uintptr_t get = sig_one("kSigStageGet"), loop = sig_one("kSigStageTimers"), cd = sig_one("kSigCountDown"),
                  small = sig_one("kSigSmallTimerRead");
  const uintptr_t frame = vslot(g_cls_stage, 7);
  if (!get || !loop || !cd || !small || !g_cls_stage || !frame) {
    logf("game: a stage's timers were not found - no stage countdowns");
    return;
  }
  const uint32_t timers = u32at(get + 12), n = u8at(get + 6), running = u32at(loop + 29), down = u32at(loop + 42);
  if (u32at(loop + 20) != timers + 4 || down != running + 4 || timers != down + 4 || n < 1 || n > 32 ||
      timers + 4 * n > g_cls_stage->size || call_target(cd + 126) != get || call_target(small + 39) != get ||
      loop < frame || loop > frame + 0x400) {
    logf("game: a stage's timers are not as known (their reads disagree) - no stage countdowns");
    return;
  }
  g_lay.timers = timers;
  g_lay.timers_running = running;
  g_lay.timers_down = down;
  g_lay.ntimers = static_cast<int>(n);
  logf("game: a stage's %u scenario timers +0x%X (running +0x%X, counting down +0x%X); the countdowns' reads exe+0x%X, "
       "exe+0x%X",
       n, timers, running, down, static_cast<unsigned>(cd + 126 - image::base()),
       static_cast<unsigned>(small + 39 - image::base()));
}

// An FSM's own timers (cFSMOrder's: every scenario script and character script has 32, as a stage has): the loop
// that runs them each frame, and the CountDown order's read of one in place (its timer kind 0 - stage 851's
// countdowns, 2026-11-13). Not a failure when not as known: no countdown of an FSM's own then.
void discover_own_timers() {
  const uintptr_t run = sig_one("kSigOwnTimers"), read = sig_one("kSigCountDownOwn"), cd = sig_one("kSigCountDown");
  const mt::Class* fsm = mt::find("cFSMOrder");
  if (!run || !read || !cd || !fsm) {
    logf("game: an FSM's own timers were not found - no countdowns of an FSM's own");
    return;
  }
  const uint32_t timers = u32at(run + 45), n = u32at(run + 59), running = u32at(run + 66), down = u32at(run + 78);
  if (u32at(read + 9) != timers || read != cd + 137 || down != running + 4 || n != 32 || timers + 4 * n > fsm->size ||
      down + 4 > fsm->size) {
    logf("game: an FSM's own timers are not as known (their reads disagree) - no countdowns of an FSM's own");
    return;
  }
  g_lay.own_timers = timers;
  g_lay.own_running = running;
  g_lay.own_down = down;
  g_lay.nown = static_cast<int>(n);
  logf("game: an FSM's %u own timers +0x%X (running +0x%X, counting down +0x%X; cFSMOrder, %u bytes); the CountDown's "
       "read of one exe+0x%X",
       n, timers, running, down, fsm->size, static_cast<unsigned>(read - image::base()));
}

// "uPl00LeonUsa" -> "Leon": the class right under uPlPlayer names the character
// (the costumes derive from it).
const char* name_of_class(const mt::Class* c) {
  static const struct {
    const char* cls;
    const char* name;
  } kNames[] = {{"uPl00Leon", "Leon"},       {"uPl01Helena", "Helena"}, {"uPl02Chris", "Chris"},
                {"uPl03Piers", "Piers"},     {"uPl04Jake", "Jake"},     {"uPl05Sherry", "Sherry"},
                {"uPl06Ada", "Ada"},         {"uPl07Helper", "Agent"},  {"uPl30Ellis", "Ellis"},
                {"uPl40Coach", "Coach"},     {"uPl50Nick", "Nick"},     {"uPl60Rochelle", "Rochelle"},
                {"uPlEnemy", "a creature"}};
  for (int depth = 0; c && depth < 8; ++depth) {
    for (const auto& n : kNames)
      if (!std::strcmp(c->name, n.cls)) return n.name;
    c = mt::by_dti(c->parent);
  }
  return "?";
}

}  // namespace

std::vector<uintptr_t> sig_all(const char* name) {
  const Sig* s = sig(name);
  if (!s) return {};
  std::vector<uintptr_t> hits = mem::find_all(image::text(), s->pattern, 64);
  if (static_cast<int>(hits.size()) != s->matches)
    logf("game: %s: %d match(es), expected %d", name, static_cast<int>(hits.size()), s->matches);
  return hits;
}

uintptr_t sig_one(const char* name) {
  const std::vector<uintptr_t> h = sig_all(name);
  return h.size() == 1 ? h[0] : 0;
}

uintptr_t call_target(uintptr_t at) {
  return mem::read<uint8_t>(at) == 0xE8 ? at + 5 + mem::read<int32_t>(at + 1) : 0;
}

bool discover() {
  if (!mt::init()) return fail("MT Framework's class records");
  g_cls_system = mt::find("cSystem");
  g_cls_main = mt::find("sBioMain");
  g_cls_player = mt::find("uPlayer");
  g_cls_plplayer = mt::find("uPlPlayer");
  g_cls_enemy = mt::find("uEnemy");
  g_cls_chara = mt::find("uCharacter");
  g_cls_splayer = mt::find("sPlayer");
  g_cls_info = mt::find("sInfoMan");
  g_cls_gameman = mt::find("sGameMan");
  g_cls_gameinfo = mt::find("cGameInfo");
  g_cls_stage = mt::find("aStageBase");
  if (!g_cls_system || !g_cls_main || !g_cls_main->vtable || !g_cls_player || !g_cls_player->vtable || !g_cls_plplayer ||
      !g_cls_plplayer->vtable || !g_cls_enemy || !g_cls_enemy->vtable || !g_cls_chara || !g_cls_chara->vtable ||
      !g_cls_splayer || !g_cls_info)
    return fail("the game's main classes");
  logf("game: sBioMain's record %08X, vtable %08X, %u bytes", static_cast<unsigned>(g_cls_main->dti),
       static_cast<unsigned>(g_cls_main->vtable), g_cls_main->size);
  if (!units::discover()) return fail("the game's units");
  if (!discover_pause() || !discover_characters()) return false;
  discover_ammo();
  discover_item_fields();
  discover_skill_points();
  discover_modes();
  discover_results();
  discover_stage_timers();
  discover_own_timers();
  g_ready = true;
  std::snprintf(g_status, sizeof(g_status), "ready");
  return true;
}

bool ready() { return g_ready; }
const char* status_text() { return g_status; }
const Layout& layout() { return g_lay; }

void tick(void* main, Run* out) {
  if (!g_systems_seen) survey_systems(reinterpret_cast<uintptr_t>(main));
  *out = Run{};
  out->main = reinterpret_cast<uintptr_t>(main);
  out->paused = mem::read<uint8_t>(out->main + g_lay.paused) != 0;
  out->nchars = characters(out->chars);
  out->in_game = out->nchars > 0;
  if (g_sgameman && g_lay.mode) {
    out->mode_known = true;
    out->mode = mem::read<uint32_t>(g_sgameman + g_lay.mode);
    out->online = mem::read<uint8_t>(g_sgameman + g_lay.online) != 0;
  }
  out->screen = !out->in_game ? Screen::kNone : out->paused ? Screen::kPause : Screen::kPlay;
}

const char* screen_name(Screen s) {
  switch (s) {
    case Screen::kNone: return "none";
    case Screen::kPlay: return "play";
    case Screen::kPause: return "pause";
  }
  return "?";
}

uintptr_t system(const char* name) {
  const auto it = g_systems.find(name);
  if (it != g_systems.end()) return it->second;
  // The game makes some systems of its own subclass of MT Framework's (sBioUnit for sUnit, sBioArea for sArea):
  // asked for by the base's name, the one held whose class derives from it (remembered under that name too).
  const mt::Class* base = g_systems_seen ? mt::find(name) : nullptr;
  if (!base) return 0;
  for (const auto& kv : g_systems)
    if (mt::is_a(kv.second, base)) {
      logf("game: %s asked for: the main system's %s (%08X), of a class derived from it", name, kv.first.c_str(),
           static_cast<unsigned>(kv.second));
      g_systems[name] = kv.second;
      return kv.second;
    }
  return 0;
}

int characters(Character out[kMaxChars]) {
  if (!g_ready || !g_splayer) return 0;
  int n = 0;
  for (int i = 0; i < g_lay.nplayers && n < kMaxChars; ++i) {
    const uintptr_t c = mem::read<uintptr_t>(g_splayer + g_lay.players + 4 * i);
    if (!c) continue;
    // A character the game is making or running (state 1 or 2), as sPlayer's own check takes it.
    const uint32_t state = mem::read<uint32_t>(c + units::layout().unit_flags) & 7;
    if (state != 1 && state != 2) continue;
    if (!mt::is_a(c, g_cls_player)) continue;
    Character& ch = out[n++];
    ch.obj = c;
    ch.index = i;
    ch.control = static_cast<Control>(mem::read<uint32_t>(c + g_lay.control));
    ch.name = character_name(c);
    ch.hp = mem::read<int16_t>(c + g_lay.hp);
    ch.hp_max = mem::read<int16_t>(c + g_lay.hp_max);
  }
  return n;
}

const char* character_name(uintptr_t obj) { return name_of_class(units::class_of(obj)); }

void set_health(uintptr_t obj, int hp) {
  if (!obj) return;
  // A field of the game's heap object: written directly (mem::write is for code and
  // read-only pages).
  const int most = *reinterpret_cast<const int16_t*>(obj + g_lay.hp_max);
  *reinterpret_cast<int16_t*>(obj + g_lay.hp) = static_cast<int16_t>(hp < 0 ? 0 : hp > most ? most : hp);
}

uintptr_t skill_points_at() {
  if (!g_ready || !g_sinfo || !g_lay.skill_points) return 0;
  const uint32_t i = mem::read<uint32_t>(g_sinfo + g_lay.profile);
  return i < 2 ? g_sinfo + g_lay.skill_points + i * g_lay.profile_size : 0;
}

uintptr_t game_info_at() {
  if (!g_ready || !g_sinfo || !g_lay.game_info) return 0;
  const uint32_t i = mem::read<uint32_t>(g_sinfo + g_lay.profile);
  // The profiles' cGameInfo: as many as lie in sInfoMan.
  if (g_lay.game_info + (i + 1) * g_lay.game_info_size > g_cls_info->size) return 0;
  return g_sinfo + g_lay.game_info + i * g_lay.game_info_size;
}

uintptr_t character_at(int index) {
  if (!g_ready || !g_splayer || index < 0 || index >= g_lay.nplayers) return 0;
  return mem::read<uintptr_t>(g_splayer + g_lay.players + 4 * index);
}

bool input_blocked(int index, uint32_t blocked_field) {
  if (!g_splayer || !blocked_field || index < 0 || index >= 8) return false;
  const uint8_t bit = static_cast<uint8_t>(1u << index);
  return (mem::read<uint8_t>(g_splayer + blocked_field) & bit) || (mem::read<uint8_t>(g_splayer + blocked_field + 1) & bit);
}

uintptr_t sgameman() { return g_sgameman; }

}  // namespace re6cc::game
