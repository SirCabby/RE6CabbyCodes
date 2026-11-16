#include "records.h"

#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#include "image.h"
#include "log.h"
#include "mem.h"
#include "mt.h"
#include "records_text.h"
#include "sigs.h"
#include "units.h"
#include "vthook.h"

// What the game keeps, as its own code reads it (docs/records.md has the detail):
//
// - sInfoMan holds two profiles (cSystemInfo, the one in use by a dword of sInfoMan's); a profile's working copy
//   (its first part) is what everything reads and changes, and the game's save writes it after a commit to the
//   copy behind it (the second part) - a game's start takes the working copy back from that copy, so a change not
//   yet saved is lost then.
// - The chapter records: a 12-byte record for each character (7: the Agent's are Ada's), difficulty (5) and
//   chapter (5) - cleared (bit 0; bits 1 and 2 two conditions of the clear), the best rank (0 S .. 5 E), the best
//   time (a float; the game's "none" is the chapter clock's most).
// - The medals: a byte each (at most 99), in nine categories (seven lists of their own, two sharing one get).
// - The dog tag templates and the emblems: a byte each, 0 locked, 1 new, 3 seen.
// - The titles: a family for each counter (1-301): its level held, a byte (bit 7: NEW); the counter a dword
//   (some families a float). The title table: 12 bytes a title {message, counter, level, stars, float, -, goal}.
//   The Dog Tag screen shows what earns a title only once its counter has started ("? ? ?" before), and its page
//   buttons skip a page where no title's counter has: an award brings the counter up to the title's goal, as
//   earning it leaves it.
// - The Mercenaries' unlocks: a dword of stage bits (a mode's ten each, from its first; the first three of each
//   always open) and 64 bits of characters (a bit a costume, the character select's table's place; a few always
//   there, No Mercy's own four never the profile's) - the getters the stage select (each frame) and the character
//   select (as it is made) read. A costume comes with a Collection file (the files' bytes, as a clear's).
namespace re6cc::records {
namespace {

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
View g_view;
struct Lock {
  Lock() { EnterCriticalSection(&g_lock); }
  ~Lock() { LeaveCriticalSection(&g_lock); }
};

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }
uintptr_t vslot(const mt::Class* c, int slot) { return c && c->vtable ? mem::read<uintptr_t>(c->vtable + 4 * slot) : 0; }

// A call's target read out of its rel32 (`at` the rel32 itself).
uintptr_t rel(uintptr_t at) { return at + 4 + mem::read<int32_t>(at); }

bool g_known = false;

// --- the layout, read out of the game's code -----------------------------------------------------------------
struct Medals {
  uint32_t counts = 0;    // the counts (cSystemInfo's)
  uint32_t n = 0;         // its medals
  uintptr_t unused = 0;   // the game's table: 2 for a number that is no medal
  int base = 0;           // its medal 0's name (a message number)
};

struct Lay {
  // sInfoMan: the profile in use (a dword), a profile's size, the first's place; the lock the game takes over
  // the profiles (a CRITICAL_SECTION) when either "in use" byte is set.
  uint32_t profile = 0, profile_size = 0, profile_base = 0;
  uint32_t lock_cs = 0, lock_use = 0;
  uintptr_t lock_flag = 0;
  // The chapter records (cSystemInfo's): the first, a record's size (12), the rank's and the time's places in it.
  uint32_t clear = 0;
  int chars = 0, difficulties = 0, chapters = 0;
  // The rewards of a clear: the dog tag templates and the Collection's "new" files (bytes); the unlock flags, the
  // Mercenaries' stages' and characters' bits (dwords, cSystemInfo's).
  uint32_t plates = 0, nplates = 0, files = 0, nfiles = 0;
  uintptr_t file_set = 0;  // cSystemInfo's file set (thiscall (file): 0 -> 1), the one every reward of a file calls
  uint32_t unlock = 0, unlock_any = 0, unlock_three = 0;
  uint32_t stages = 0, stage_chris = 0, stage_jake = 0, characters = 0, char_ada = 0;
  int files_chris[2] = {}, files_jake[2] = {};
  int stories = 0, diff_achievement = 0, diff_achievements = 0;
  // The emblems, the title states and counters (cSystemInfo's).
  uint32_t emblems = 0, nemblems = 0;
  uint32_t states = 0, nstates = 0, counters = 0, ncounters = 0;
  int top_level = 0, top_achievement = 0, titles_counted = 0, titles_counter = 0, titular = 0, titular_achievement = 0;
  // The title table (in the game's data) and the counters not in use.
  uintptr_t table = 0, table_count = 0, unused_counters = 0;
  Medals medal[kMedalCats];
  // The Records screen: its page and PROFILES' view (dwords), the pages, the view of the chapter grid; the medal
  // category and the categories shown (a mask); its own copy of the grid (a byte a record: the rank, or
  // "not cleared"), and of the percent.
  const mt::Class* record = nullptr;
  uint32_t page = 0, view = 0, medal_cat = 0, medal_mask = 0, grid = 0, percent = 0;
  int profiles_page = -1, medals_page = -1, grid_view = -1;
  uint32_t not_cleared = 0, rank_most = 0;
  // The screens' step (a byte, uIdCBase's) and their closing one; the Dog Tag screen's list steps, its rows (a
  // row's state its first dword) for the emblems and the titles.
  const mt::Class* dogtag = nullptr;
  uint32_t step = 0, closing = 0, emblem_step = 0, title_step = 0;
  uint32_t emblem_rows = 0, emblem_count = 0, title_rows = 0, title_count = 0, row_size = 0;
  // The title list's pages: a title row's progress (its counter, what the detail and the pages go by), the rows a
  // page; the last page and the pages its buttons go to (a bit a page), the Dog Tag screen's.
  uint32_t title_progress = 0, page_rows = 0, title_last_page = 0, title_pages = 0;
  // The save: sInfoMan's wait (+ its afters), sStorage's busy word; the profile's cGameInfo.
  uint32_t info_wait = 0, storage_busy = 0, gi_base = 0, gi_size = 0;
  uintptr_t storage_global = 0;
};
Lay L;
bool g_chapters = false, g_medals = false, g_emblems = false, g_titles = false;

// The skills, as the skill screen and the game's own "can be bought" test read them.
constexpr int kMaxGroup = 8;  // the most kinds one count adds up (the game's: 6)
struct Group {
  uint16_t kind = 0;  // the kind asked for
  int n = 0;
  uint16_t of[kMaxGroup] = {};  // the kinds it counts (itself among them)
};
struct SkillLay {
  // The skill table (the game's static data): the records, how many (a global), a record's size and its fields.
  uintptr_t table = 0, count = 0;
  uint32_t rec_size = 0, r_group = 0, r_level = 0, r_top = 0, r_mask = 0, r_id = 0, r_price = 0, r_need = 0,
           r_param = 0, r_goal = 0;
  // The profile's skill states (cSystemInfo's bytes), how many, "bought".
  uint32_t states = 0, nstates = 0, bought = 0;
  // A record's requirement as the game's test reads it: the skill before it bought, the kills of an enemy kind,
  // the kills with a kind of weapon, the campaigns complete (and how many); 0 and the rest: none.
  int need_previous = 0, need_kills = 0, need_weapons = 0, need_campaigns = 0, campaigns_goal = 0;
  // The skill screen: its class, its step and the frame's closing case; its kind of list (a record is listed when
  // its mask has that bit), its user (the profile its list is filled from), its rows (16 bytes; at most, and how
  // many now) and a row's fields (its skill the row's first byte).
  const mt::Class* screen = nullptr;
  uint32_t step = 0, kind = 0, user = 0, rows = 0, nrows = 0, rows_most = 0;
  uint32_t row_state = 0, row_need = 0, row_can = 0, row_price = 0;
  uint32_t kinds_with_needs = 0;  // the kinds of list with a skill behind a requirement (a bit each)
  // The kills: the kill table (8 bytes a record: its entry, two keys, the kind - a word), how many (a global), the
  // keys the skills' count asks for; the counts (cSystemInfo's, 8 bytes apart) and how many; the kinds counted
  // with others.
  uintptr_t kill_table = 0, kill_count = 0;
  uint32_t kill_rec = 0, k_index = 0, k_key1 = 0, k_key2 = 0, k_kind = 0, kill_key1 = 0, kill_key2 = 0;
  uint32_t kills = 0, nkills = 0;
  Group kill_group[2];
  // The weapon kills: the weapon table (its entry, the weapon - a word), how many; the kills (cSystemInfo's), how
  // many entries, an entry's size; the weapons counted together (the game's switch over the kinds).
  uintptr_t weapon_table = 0, weapon_count = 0;
  uint32_t weapon_rec = 0, w_weapon = 0, weapon_kills = 0, nweapon_kills = 0, weapon_stride = 0;
  int nweapon_groups = 0;
  Group weapon_group[32];
};
SkillLay K;
bool g_skills = false;
constexpr uint8_t kSkillSeen = 2;  // the state the skill screen's start leaves a new skill in (unlocked, seen)

// The game's calls (the user's choice, 2026-11-06): its achievement, and its profile save's steps.
using AchieveFn = void(__thiscall*)(void* sinfo, uint32_t achievement);
using BoolFn = uint8_t(__thiscall*)(void* self);
using VoidFn = void(__thiscall*)(void* self);
using After1Fn = void(__thiscall*)(void* sinfo, uint32_t a, uint32_t b);
using After2Fn = void(__thiscall*)(void* sinfo, uint32_t a);
AchieveFn g_achieve = nullptr;
BoolFn g_dirty = nullptr, g_busy = nullptr;
VoidFn g_commit = nullptr, g_copy_info = nullptr, g_request = nullptr;
After1Fn g_after1 = nullptr;
After2Fn g_after2 = nullptr;

constexpr uint8_t kStepClosing = 0x5A;  // checked in both screens' frames (discover_screens)

// --- requests --------------------------------------------------------------------------------------------------
struct Request {
  enum Kind : uint8_t { kChapter, kMedal, kEmblem, kTitle, kSkill, kWeaponRow, kCarry, kEnemy, kMerc } kind = kChapter;
  ChapterSet chapter;
  MedalSet medal;
  uint8_t emblem = 0, enemy = 0;
  TitleSet title;
  SkillSet skill;
  WeaponSet weapon;
  CarrySet carry;
  MercSet merc;
};
std::vector<Request> g_requests;
char g_note[200] = "";

void note(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(g_note, sizeof(g_note), fmt, ap);
  va_end(ap);
}

// --- discovery -------------------------------------------------------------------------------------------------
bool pattern_at(uintptr_t at, const char* p) { return at && mem::matches(at, mem::parse_pattern(p)); }

// The profiles and the chapter records: the Records screen's constructor filling its grid, the getters it calls.
bool discover_chapters() {
  const uintptr_t fill = game::sig_one("kSigRecordGridFill");
  if (!fill) return false;
  const uintptr_t get = rel(fill + 55), rank = rel(fill + 89);
  if (!pattern_at(get, RE6CC_CLEAR_GET) || !pattern_at(rank, RE6CC_CLEAR_RANK)) {
    logf("records: the chapter records' gets are not as known");
    return false;
  }
  const mt::Class* sys = mt::find("cSystemInfo");
  const mt::Class* info = mt::find("sInfoMan");
  L.profile = u8at(fill + 31);
  L.profile_size = u32at(fill + 38);
  L.profile_base = u8at(fill + 47);
  L.grid = u32at(fill + 2);
  L.not_cleared = u32at(fill + 50);
  L.rank_most = u32at(fill + 101);
  L.chapters = static_cast<int>(u8at(fill + 115));
  L.difficulties = static_cast<int>(u8at(fill + 126));
  L.chars = static_cast<int>(u8at(fill + 144));
  L.percent = u32at(fill + 164);
  L.clear = u32at(get + 49) * 12;
  const uint32_t rank_at = u32at(rank + 54);
  // The getters' own limits agree with the grid's: 8 characters asked for, the eighth (the Agent) read as the
  // seventh (Ada); the rank the record's second word; a record 12 bytes.
  const bool ok = sys && info && L.profile_size == sys->size && L.profile_base + 2 * L.profile_size <= info->size &&
                  L.profile + 4 <= info->size && L.chars == kChars && L.difficulties == kDifficulties &&
                  L.chapters == kChapters && u8at(get + 7) == 8 && u8at(get + 16) == kDifficulties &&
                  u8at(get + 25) == kChapters && u8at(get + 30) == 7 && u32at(get + 34) == 6 &&
                  u8at(rank + 7) == 8 && u32at(rank + 34) == 6 && rank_at == L.clear + 4 && u8at(fill + 97) == L.not_cleared &&
                  u8at(fill + 122) == static_cast<uint32_t>(kChapters) && L.rank_most == kRanks - 1 &&
                  L.not_cleared == kRanks && L.clear + 12u * kChars * kDifficulties * kChapters <= L.profile_size;
  if (!ok) {
    logf("records: the chapter records are not as known (profile %u bytes at +0x%X, records +0x%X, %d x %d x %d)",
         L.profile_size, L.profile_base, L.clear, L.chars, L.difficulties, L.chapters);
    return false;
  }
  logf("records: sInfoMan's profiles +0x%X (in use +0x%X), 0x%X bytes; the chapter records +0x%X (12 bytes: cleared, "
       "rank, time; %d characters x %d difficulties x %d chapters); the Records grid +0x%X, its percent +0x%X",
       L.profile_base, L.profile, L.profile_size, L.clear, L.chars, L.difficulties, L.chapters, L.grid, L.percent);
  return true;
}

// The Records screen: its pages, PROFILES' views (the one that draws the grid), the medal category.
bool discover_record_screen() {
  L.record = mt::find("uIdRecord00");
  const uintptr_t pages = game::sig_one("kSigRecordPages"), input = game::sig_one("kSigRecordInput");
  const uintptr_t frame = vslot(L.record, 8);
  if (!L.record || !pages || !input || pages < frame || pages > frame + 0x300) return false;
  L.page = u8at(pages + 23);
  const uintptr_t table = u32at(pages + 32);
  // The jump table's cases, in their order: page 0's case at +36, page 3's at +63.
  const uintptr_t profiles = rel(pages + 39), medals = rel(pages + 66);
  if (u8at(pages + 26) != 4 || u32at(table) != pages + 36 || u32at(table + 12) != pages + 63 ||
      !pattern_at(profiles, RE6CC_RECORD_PROFILES) || u8at(input + 15) != L.page)
    return false;
  L.profiles_page = 0;
  L.view = u32at(profiles + 10);
  const uintptr_t views = u32at(profiles + 22);
  // The view whose draw reads the grid: its first cell's number and its read agree with the constructor's grid.
  for (int v = 0; v <= static_cast<int>(u8at(profiles + 16)); ++v) {
    const uintptr_t at = profiles + 26 + 8 * v;
    if (u32at(views + 4 * v) != at) return false;
    const uintptr_t draw = rel(at + 4);
    const mem::Range body{draw, draw + 0x200};
    const uintptr_t first = mem::find_pattern(body, RE6CC_RECORD_GRID_FIRST);
    const uintptr_t read = first ? mem::find_pattern(body, RE6CC_RECORD_GRID_READ, first) : 0;
    if (first && read && u8at(first + 4) + u32at(read + 4) == L.grid) L.grid_view = v;
  }
  // The medal page: page 3's draw reads the category the input steps through.
  const uintptr_t medal = game::sig_one("kSigMedalPage");
  L.medal_cat = u32at(input + 60);
  L.medal_mask = u32at(input + 76);
  if (L.grid_view < 0 || !medal || medal < medals || medal > medals + 0x200 || u32at(medal + 2) != L.medal_cat ||
      u32at(input + 84) != L.medal_cat || static_cast<int>(u8at(input + 16)) != 3)
    return false;
  L.medals_page = 3;
  const uint32_t most = L.record->size;
  return L.view + 4 <= most && L.medal_cat + 4 <= most && L.medal_mask + 4 <= most && L.grid + 175 <= most &&
         L.percent + 4 <= most;
}

// The medal categories: the medal page's own get (a switch over the profile's gets) and its names.
bool discover_medals() {
  const uintptr_t page = game::sig_one("kSigMedalPage"), name = game::sig_one("kSigMedalName");
  const uintptr_t sw = page ? rel(page + 101) : 0, names = name ? rel(name + 15) : 0;
  if (!pattern_at(sw, RE6CC_MEDAL_SWITCH) || !pattern_at(names, RE6CC_MEDAL_NAMES) ||
      u8at(sw + 8) != kMedalCats - 1 || u8at(names + 7) != kMedalCats - 2)
    return false;
  const uintptr_t cases = u32at(sw + 18), ncases = u32at(names + 13);
  for (int c = 0; c < kMedalCats; ++c) {
    Medals& m = L.medal[c];
    const uintptr_t at = u32at(cases + 4 * c);
    uintptr_t get = 0;
    int list = -1;
    if (pattern_at(at, RE6CC_MEDAL_CASE)) {
      get = rel(at + 19);
    } else if (pattern_at(at, RE6CC_MEDAL_CASE2)) {
      get = rel(at + 22);
      list = static_cast<int>(u8at(at + 17));
    } else {
      return false;
    }
    if (list < 0 && pattern_at(get, RE6CC_MEDAL_GET32)) {
      m.n = u32at(get + 5);
      m.unused = u32at(get + 18);
      m.counts = u32at(get + 29);
    } else if (list < 0 && pattern_at(get, RE6CC_MEDAL_GET8)) {
      m.n = u8at(get + 6);
      m.unused = u32at(get + 16);
      m.counts = u32at(get + 27);
    } else if (list >= 0 && pattern_at(get, RE6CC_MEDAL_GET2) && list < static_cast<int>(u8at(get + 6)) &&
               u8at(get + 52) == u8at(get + 15)) {
      m.n = u8at(get + 15);
      m.unused = u32at(get + (list ? 38 : 24));
      m.counts = u32at(get + 59) + list * u8at(get + 52);
    } else {
      return false;
    }
    // Its names: the name function's case (from category 1: `mov eax, base`), the default (category 0) where its
    // `ja` goes.
    const uintptr_t nc = c ? u32at(ncases + 4 * (c - 1)) : names + 10 + static_cast<int8_t>(u8at(names + 9));
    if (u8at(nc) != 0xB8) return false;
    m.base = static_cast<int>(u32at(nc + 1));
    if (!m.n || m.n > static_cast<uint32_t>(kMaxMedals) || !image::whole().contains(m.unused) ||
        m.counts + m.n > L.profile_size || m.base < records_text::kMedalFirst ||
        m.base + static_cast<int>(m.n) > records_text::kMedalFirst + records_text::kMedalMsgs)
      return false;
  }
  for (int c = 0; c < kMedalCats; ++c)
    logf("records: medals %d (%s): %u at cSystemInfo +0x%X, names from message %d", c, records_text::kMedalCategory[c],
         L.medal[c].n, L.medal[c].counts, L.medal[c].base);
  return true;
}

// The game's achievement (sInfoMan's): skipped by the game under sBioMain's no-achievement bits; sets the emblem
// and calls Steam. Its lock is the one the mod takes over the profile.
bool discover_achievement() {
  const uintptr_t a = game::sig_one("kSigAchievement");
  const uintptr_t titles_end = game::sig_one("kSigDogTagTitlesEnd"), award = game::sig_one("kSigClearAward");
  if (!a || u8at(a + 67) != L.profile || u32at(a + 70) != L.profile_size || u8at(a + 77) != L.profile_base ||
      (titles_end && rel(titles_end + 36) != a) || (award && rel(award + 28) != a))
    return false;
  const uintptr_t emblem_get = rel(a + 80);
  if (!pattern_at(emblem_get, RE6CC_BYTE_GET)) return false;
  L.lock_use = u8at(a + 32);
  L.lock_flag = u32at(a + 38);
  L.lock_cs = u8at(a + 47);
  L.emblems = u32at(emblem_get + 18);
  L.nemblems = u8at(emblem_get + 6);
  if (L.nemblems != static_cast<uint32_t>(kEmblems) || u8at(a + 62) != L.nemblems ||
      L.emblems + L.nemblems > L.profile_size || !image::whole().contains(L.lock_flag))
    return false;
  g_achieve = reinterpret_cast<AchieveFn>(a);
  logf("records: the game's achievement exe+0x%X (sInfoMan's lock +0x%X when +0x%X or exe+0x%X is set); the emblems "
       "cSystemInfo +0x%X (%u)",
       static_cast<unsigned>(a - image::base()), L.lock_cs, L.lock_use, static_cast<unsigned>(L.lock_flag - image::base()),
       L.emblems, L.nemblems);
  return true;
}

// The profile's save, as the menus make it as they close.
bool discover_save() {
  const uintptr_t s = game::sig_one("kSigProfileSave");
  if (!s || u8at(s + 117) != L.profile || u32at(s + 120) != L.profile_size || u8at(s + 127) != L.profile_base ||
      u8at(s + 157) != L.profile || u32at(s + 22) != u32at(s + 111))
    return false;
  const mt::Class* info = mt::find("cGameInfo");
  L.info_wait = u32at(s + 28);
  L.storage_busy = u32at(s + 61);
  L.storage_global = u32at(s + 55);
  L.gi_size = u32at(s + 160);
  L.gi_base = u32at(s + 167);
  const mt::Class* sinfo = mt::find("sInfoMan");
  if (!info || L.gi_size != info->size || !sinfo || L.gi_base + 2 * L.gi_size > sinfo->size ||
      L.info_wait + 4 > sinfo->size || u32at(s + 178) != L.storage_global || u32at(s + 194) != L.storage_global ||
      !image::data().contains(L.storage_global))
    return false;
  g_after2 = reinterpret_cast<After2Fn>(rel(s + 40));
  g_after1 = reinterpret_cast<After1Fn>(rel(s + 83));
  if (rel(s + 96) != reinterpret_cast<uintptr_t>(g_after2)) return false;
  g_dirty = reinterpret_cast<BoolFn>(rel(s + 129));
  g_commit = reinterpret_cast<VoidFn>(rel(s + 151));
  g_copy_info = reinterpret_cast<VoidFn>(rel(s + 172));
  g_busy = reinterpret_cast<BoolFn>(rel(s + 183));
  g_request = reinterpret_cast<VoidFn>(rel(s + 199));
  logf("records: the profile's save: changed? exe+0x%X, commit exe+0x%X, cGameInfo (sInfoMan +0x%X, 0x%X) copy exe+0x%X, "
       "sStorage (exe+0x%X) busy? exe+0x%X / ask exe+0x%X / +0x%X, then sInfoMan's afters exe+0x%X, exe+0x%X (+0x%X)",
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_dirty) - image::base()),
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_commit) - image::base()), L.gi_base, L.gi_size,
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_copy_info) - image::base()),
       static_cast<unsigned>(L.storage_global - image::base()),
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_busy) - image::base()),
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_request) - image::base()), L.storage_busy,
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_after1) - image::base()),
       static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_after2) - image::base()), L.info_wait);
  return true;
}

// What a clear gives (the game's chapter end): templates, files, the unlocks, the achievements.
bool discover_rewards() {
  const uintptr_t aw = game::sig_one("kSigClearAward"), camp = game::sig_one("kSigClearCampaigns"),
                  chris = game::sig_one("kSigClearChris"), jake = game::sig_one("kSigClearJake"),
                  three = game::sig_one("kSigClearThree"), ada = game::sig_one("kSigClearAda"),
                  diff = game::sig_one("kSigClearDifficulty");
  if (!aw || !camp || !chris || !jake || !three || !ada || !diff || !g_achieve) return false;
  const uintptr_t plate_get = rel(aw + 52), files_set = rel(aw + 113);
  // sInfoMan's file set calls cSystemInfo's (its own lock taken first): `lea ecx, ...; call` 46 bytes in.
  const uintptr_t files_inner = u8at(files_set + 46) == 0xE8 ? rel(files_set + 47) : 0;
  if (!pattern_at(plate_get, RE6CC_BYTE_GET) || !pattern_at(files_inner, RE6CC_FILE_SET) || rel(aw + 134) != files_set ||
      rel(aw + 147) != files_set || rel(diff + 23) != reinterpret_cast<uintptr_t>(g_achieve))
    return false;
  L.stories = static_cast<int>(u8at(aw + 10));
  L.plates = u32at(plate_get + 18);
  L.nplates = u8at(plate_get + 6);
  L.files = u32at(files_inner + 13);
  L.nfiles = u8at(files_inner + 6);
  L.file_set = files_inner;
  L.files_jake[0] = static_cast<int>(u8at(aw + 111));
  L.files_jake[1] = static_cast<int>(u8at(aw + 118));
  L.files_chris[0] = static_cast<int>(u8at(aw + 132));
  L.files_chris[1] = static_cast<int>(u8at(aw + 139));
  // The flags are counted from sInfoMan's start (the profile's place added): made cSystemInfo's.
  const uint32_t base = L.profile_base;
  L.unlock = u32at(camp + 59) - base;
  L.unlock_any = u8at(camp + 63);
  L.stages = u32at(chris + 21) - base;
  L.stage_chris = 1u << 1;  // `shr edx, 1; test dl, 1`
  L.stage_jake = 1u << u8at(jake + 27);
  L.unlock_three = u8at(three + 25);
  L.characters = u32at(ada + 21) - base;
  L.char_ada = u32at(ada + 26);
  L.diff_achievement = static_cast<int>(u8at(diff + 14));
  L.diff_achievements = static_cast<int>(u8at(diff + 36));
  const bool ok = L.stories == 4 && u8at(camp + 30) == 3 && u8at(chris + 2) == 2 && u8at(jake + 2) == 4 &&
                  u8at(three + 4) == 7 && u8at(three + 6) == 7 && u8at(ada + 2) == 8 && u32at(jake + 21) - base == L.stages &&
                  u32at(jake + 74) - base == L.stages && u8at(jake + 78) == L.stage_jake &&
                  u32at(three + 21) - base == L.unlock && u32at(chris + 21) - base == L.stages &&
                  L.plates + L.nplates <= L.profile_size && L.files + L.nfiles <= L.profile_size &&
                  L.unlock + 4 <= L.profile_size && L.stages + 4 <= L.profile_size && L.characters + 4 <= L.profile_size &&
                  L.nplates > static_cast<uint32_t>(L.stories * kChapters) && L.diff_achievements == 4 &&
                  L.diff_achievement + L.diff_achievements <= kEmblems;
  for (int f : {L.files_jake[0], L.files_jake[1], L.files_chris[0], L.files_chris[1]})
    if (f < 0 || f >= static_cast<int>(L.nfiles)) return false;
  if (!ok) return false;
  logf("records: a clear's rewards: the achievement and template story*5+chapter+1 (templates +0x%X, %u), files %d %d "
       "(Chris ch.1) %d %d (Jake ch.3) of +0x%X; a story complete: +0x%X |= 0x%X, Chris' +0x%X |= 0x%X, Jake's |= 0x%X, "
       "the three |= 0x%X, Ada's +0x%X |= 0x%X; a whole game on a difficulty or harder: achievement %d + it (of %d)",
       L.plates, L.nplates, L.files_chris[0], L.files_chris[1], L.files_jake[0], L.files_jake[1], L.files, L.unlock,
       L.unlock_any, L.stages, L.stage_chris, L.stage_jake, L.unlock_three, L.characters, L.char_ada,
       L.diff_achievement, L.diff_achievements);
  return true;
}

// The Dog Tag screen: its lists' steps and rows; the titles (states, counters, the title table).
bool discover_dogtag() {
  L.dogtag = mt::find("uIdCustomize10");
  const uintptr_t lists = game::sig_one("kSigDogTagLists"), titles = game::sig_one("kSigDogTagTitles"),
                  titles_end = game::sig_one("kSigDogTagTitlesEnd"), emblems = game::sig_one("kSigDogTagEmblems");
  const uintptr_t frame = vslot(L.dogtag, 8), init = vslot(L.dogtag, 5);
  if (!L.dogtag || !lists || !titles || !titles_end || !emblems || lists < frame || lists > frame + 0x400 ||
      titles < init || titles > init + 0x400 || emblems < init || emblems > init + 0x600)
    return false;
  L.step = u8at(lists + 18);
  L.emblem_step = u8at(lists + 30);
  L.title_step = u8at(lists + 24);
  L.emblem_rows = u32at(emblems + 2);
  L.emblem_count = u32at(emblems + 12);
  L.title_rows = u32at(titles + 2);
  L.title_count = u32at(titles + 12);
  L.row_size = u8at(titles_end + 3);
  L.unused_counters = u32at(titles + 58);
  const uintptr_t state_get = rel(titles + 120), find = rel(titles + 152), def = rel(titles + 164),
                  counter_get = rel(titles + 200), counterf_get = rel(titles + 207), emblem_get = rel(emblems + 50);
  if (u8at(lists + 23) != L.step || u8at(lists + 29) != L.step || u32at(titles + 67) != L.unused_counters ||
      u32at(titles_end + 10) != L.title_count || L.row_size != 0x14 || !pattern_at(state_get, RE6CC_BYTE_GET32) ||
      !pattern_at(find, RE6CC_TITLE_FIND) || !pattern_at(def, RE6CC_TITLE_DEF) ||
      !pattern_at(counter_get, RE6CC_COUNTER_GET) || !pattern_at(counterf_get, RE6CC_COUNTER_GETF) ||
      !pattern_at(emblem_get, RE6CC_BYTE_GET) || u32at(emblem_get + 18) != L.emblems ||
      u8at(titles + 187) != 6 || u8at(titles + 221) != 8 || u8at(titles + 228) != 5)
    return false;
  L.states = u32at(state_get + 20);
  L.nstates = u32at(state_get + 5);
  L.counters = u32at(counter_get + 20);
  L.ncounters = u32at(counter_get + 6);
  L.table = u32at(def + 20);
  L.table_count = u32at(def + 6);
  L.titular = static_cast<int>(u8at(titles_end + 24));
  L.titular_achievement = static_cast<int>(u8at(titles_end + 34));
  // The title list's pages, right after the rows are filled: which pages its buttons go to, by the rows' progress.
  const uintptr_t pages = game::sig_one("kSigDogTagTitlePages");
  if (pages != titles_end + 40 || u32at(pages + 8) != L.title_count || u32at(pages + 58) != u32at(pages + 156) ||
      u32at(pages + 67) != u32at(pages + 142) || u32at(pages + 109) != L.unused_counters ||
      u8at(pages + 125) != L.row_size || u8at(pages + 163) != u8at(pages + 128) ||
      u32at(pages + 166) != u8at(pages + 128) * L.row_size || u32at(pages + 83) < L.title_rows ||
      u32at(pages + 83) - L.title_rows + 4 > L.row_size)
    return false;
  L.title_progress = u32at(pages + 83) - L.title_rows;
  L.page_rows = u8at(pages + 128);
  L.title_last_page = u32at(pages + 58);
  L.title_pages = u32at(pages + 67);
  // The game's title check: the top level's achievement, the titles counted into a counter.
  const uintptr_t check = game::sig_one("kSigTitleCheck"), count = game::sig_one("kSigTitleCount");
  if (!check || !count || rel(count + 2) != state_get) return false;
  L.top_level = static_cast<int>(u8at(check + 14));
  L.top_achievement = static_cast<int>(u8at(check + 83));
  L.titles_counted = static_cast<int>(u32at(count + 14));
  L.titles_counter = static_cast<int>(u8at(count + 56));
  const uint32_t most = L.dogtag->size;
  const bool ok = u32at(counterf_get + 20) == L.counters && u32at(counterf_get + 6) == L.ncounters &&
                  L.nstates == L.ncounters && L.ncounters <= static_cast<uint32_t>(kMaxTitleRows) &&
                  u32at(find + 2) == L.table_count && u32at(find + 24) == L.table + 4 &&
                  L.states + L.nstates <= L.profile_size && L.counters + 4 * L.ncounters <= L.profile_size &&
                  image::whole().contains(L.table) && image::whole().contains(L.table_count) &&
                  image::whole().contains(L.unused_counters) && L.top_level == kMaxLevels &&
                  u8at(check + 96) == static_cast<uint32_t>(L.top_achievement) &&
                  u8at(check + 111) == static_cast<uint32_t>(L.top_achievement) &&
                  L.top_achievement < kEmblems && L.titular_achievement < kEmblems &&
                  L.titles_counted <= static_cast<int>(L.nstates) && L.titles_counter >= 1 &&
                  L.titles_counter <= static_cast<int>(L.ncounters) && L.emblem_rows + L.row_size * kEmblems <= most &&
                  L.title_rows + L.row_size * L.ncounters <= most && L.emblem_count + 4 <= most &&
                  L.title_count + 4 <= most && L.step + 1 <= most && L.page_rows == 15 &&
                  L.title_last_page + 4 <= most && L.title_pages + 4 <= most &&
                  (L.ncounters + L.page_rows - 1) / L.page_rows <= 32;
  if (!ok) return false;
  logf("records: the Dog Tag screen's step +0x%X (the emblem list %u, the title list %u), its rows (0x%X bytes) of "
       "emblems +0x%X (how many +0x%X) and titles +0x%X (+0x%X; a row's progress +0x%X, %u rows a page, the pages "
       "shown +0x%X up to +0x%X); the title states +0x%X and counters +0x%X (%u), the title table exe+0x%X (count "
       "exe+0x%X); a level-%d title gives achievement %d, %d titles achievement %d, the titles held go to counter %d",
       L.step, L.emblem_step, L.title_step, L.row_size, L.emblem_rows, L.emblem_count, L.title_rows, L.title_count,
       L.title_progress, L.page_rows, L.title_pages, L.title_last_page, L.states, L.counters, L.ncounters,
       static_cast<unsigned>(L.table - image::base()), static_cast<unsigned>(L.table_count - image::base()),
       L.top_level, L.top_achievement, L.titular, L.titular_achievement, L.titles_counter);
  return true;
}

// A screen's frame closes at the screens' closing step (`mov byte [esi+step], 0x5A`).
bool frame_closes(const mt::Class* c) {
  const uint8_t pat[4] = {0xC6, 0x46, static_cast<uint8_t>(L.step), kStepClosing};
  const uintptr_t frame = vslot(c, 8);
  for (uintptr_t a = frame; a && a < frame + 0x600; ++a)
    if (mem::readable(a, 4) && !std::memcmp(reinterpret_cast<const void*>(a), pat, 4)) return true;
  return false;
}

// Both screens' frames close at the same step.
bool closing_step_known() { return frame_closes(L.record) && frame_closes(L.dogtag); }

// The kinds a case of the weapon kills' switch adds up: `push kind; call one` and then, each kind more, `push kind;
// mov ecx, esi; mov edi, eax` (the first) or `add edi, eax`; `call one` - to `add eax, edi; pop edi; pop esi; ret 4`.
bool weapon_case(uintptr_t at, uintptr_t one, Group* g) {
  static const uint8_t kEnd[] = {0x03, 0xC7, 0x5F, 0x5E, 0xC2, 0x04, 0x00};
  g->n = 0;
  if (u8at(at) != 0x68 || u8at(at + 5) != 0xE8 || rel(at + 6) != one) return false;
  g->of[g->n++] = static_cast<uint16_t>(u32at(at + 1));
  at += 10;
  while (std::memcmp(reinterpret_cast<const void*>(at), kEnd, sizeof(kEnd)) != 0) {
    const uint8_t keep = g->n == 1 ? 0x8B : 0x03;  // mov edi, eax / add edi, eax
    if (g->n >= kMaxGroup || u8at(at) != 0x68 || u8at(at + 5) != 0x8B || u8at(at + 6) != 0xCE || u8at(at + 7) != keep ||
        u8at(at + 8) != 0xF8 || u8at(at + 9) != 0xE8 || rel(at + 10) != one)
      return false;
    g->of[g->n++] = static_cast<uint16_t>(u32at(at + 1));
    at += 14;
  }
  return g->n > 1;
}

// The skills: the skill screen's list (filled from the skill table and the profile), the game's own test of what
// its rows say can be bought, and the counts the test reads - every place out of the game's code.
bool discover_skills() {
  K.screen = mt::find("uIdSkillSet10");
  const uintptr_t fill = game::sig_one("kSigSkillListFill"), user = game::sig_one("kSigSkillScreenUser");
  const uintptr_t save = game::sig_one("kSigProfileSave"), campaigns = game::sig_one("kSigClearCampaigns");
  if (!K.screen || !fill || !user || !save || !campaigns) return false;
  // The skill table: its count and its records (a record past the count reads the first).
  const uintptr_t count_fn = rel(fill + 0x71), record_fn = rel(fill + 0x84);
  if (!pattern_at(count_fn, RE6CC_SKILL_COUNT) || !pattern_at(record_fn, RE6CC_SKILL_RECORD) ||
      u32at(count_fn + 1) != u32at(record_fn + 6))
    return false;
  K.count = u32at(count_fn + 1);
  K.table = u32at(record_fn + 20);
  // A record's fields, as the fill reads them: from its skill's place (the fill walks the records by it).
  const uint32_t id = u8at(fill + 0xA2);
  auto place = [&](uintptr_t at) { return static_cast<uint32_t>(static_cast<int>(id) + static_cast<int8_t>(u8at(at))); };
  K.r_id = id;
  K.r_mask = place(fill + 0xB2);
  K.r_group = place(fill + 0xC3);
  K.r_need = place(fill + 0x116);
  K.r_level = place(fill + 0x13F);
  K.r_price = place(fill + 0x145);
  K.r_top = place(fill + 0x15A);
  K.rec_size = u8at(fill + 0x18F);
  // The screen's own fields: its kind of list, its rows (`add edx, edx` then `[edi + edx*8 + rows]`: 16 bytes a
  // row, its skill the row's first byte), how many, a row's state, requirement, "can be bought" and price.
  K.kind = u32at(fill + 0xA5);
  K.nrows = u32at(fill + 0x62);
  K.rows_most = u8at(fill + 0xDE);
  K.rows = u32at(fill + 0x10B);
  K.row_state = u8at(fill + 0x168);
  K.row_need = u8at(fill + 0x16E);
  K.row_can = u8at(fill + 0x17D);
  K.row_price = u8at(fill + 0x183);
  K.bought = u8at(fill + 0x11C);
  const uintptr_t get = rel(fill + 0x110);
  const bool fill_ok =
      place(fill + 0xCB) == K.r_group && place(fill + 0xF4) == id && place(fill + 0x14E) == id &&
      place(fill + 0x153) == K.r_level && place(fill + 0x16B) == K.r_need && place(fill + 0x180) == K.r_price &&
      u32at(fill + 0xD4) == K.nrows && u32at(fill + 0xDA) == K.nrows && u32at(fill + 0xFD) == K.nrows &&
      u8at(fill + 0x148) == K.row_price && u8at(fill + 0x13A) == K.bought && u8at(fill + 0x117) == 1 &&
      u32at(fill + 0xEA) == u32at(fill + 0x120) && u32at(fill + 0xEA) == u32at(fill + 0x172) &&
      u8at(fill + 0xF0) == L.profile && u8at(fill + 0x126) == L.profile && u32at(fill + 0xF7) == L.profile_size &&
      u32at(fill + 0x129) == L.profile_size && u8at(fill + 0x106) == L.profile_base &&
      u8at(fill + 0x131) == L.profile_base && pattern_at(get, RE6CC_BYTE_GET) && rel(fill + 0x134) == get &&
      K.rec_size == 20 && std::max({K.r_group, K.r_level, K.r_top, K.r_mask, K.r_id, K.r_need}) < K.rec_size &&
      K.r_price + 4 <= K.rec_size;
  if (!fill_ok) {
    logf("records: the skill screen's list is not as known");
    return false;
  }
  K.nstates = u8at(get + 6);
  K.states = u32at(get + 18);
  // The game's "can be bought" test (what a row says): any state but 0, or the record's requirement met.
  const uintptr_t avail = rel(fill + 0x177);
  if (!pattern_at(avail, RE6CC_SKILL_AVAILABLE)) return false;
  const uintptr_t cases = u32at(avail + 0x48);
  K.r_param = u8at(avail + 0x76);
  K.r_goal = u8at(avail + 0x82);
  // The cases in their order (the requirement less one: `dec eax`): the skill before it bought, the kills of an
  // enemy kind, the kills with a kind of weapon, the campaigns complete.
  const bool avail_ok =
      rel(avail + 0x10) == record_fn && rel(avail + 0x29) == get && rel(avail + 0x60) == get &&
      u8at(avail + 0x16) == L.profile && u32at(avail + 0x19) == L.profile_size && u8at(avail + 0x26) == L.profile_base &&
      u8at(avail + 0x4E) == L.profile && u32at(avail + 0x55) == L.profile_size && u8at(avail + 0x5D) == L.profile_base &&
      u8at(avail + 0x22) == K.r_id && u8at(avail + 0x52) == K.r_id && u8at(avail + 0x3B) == K.r_need &&
      u8at(avail + 0x66) == K.bought && u8at(avail + 0x42) == 3 && u32at(cases) == avail + 0x4C &&
      u32at(cases + 4) == avail + 0x73 && u32at(cases + 8) == avail + 0x87 && u32at(cases + 12) == avail + 0x9B &&
      u8at(avail + 0x8A) == K.r_param && u8at(avail + 0x96) == K.r_goal && K.r_param + 2 <= K.rec_size &&
      K.r_goal + 2 <= K.rec_size;
  if (!avail_ok) {
    logf("records: the game's test of a skill on sale is not as known");
    return false;
  }
  K.need_previous = 1;
  K.need_kills = 2;
  K.need_weapons = 3;
  K.need_campaigns = 4;
  // The campaigns: each story's test (to its last chapter) - the one a chapter's clear asks (records' own
  // story_complete) - and how many must be complete.
  const uintptr_t story = rel(avail + 0xA6), story_one = rel(campaigns + 15);
  K.campaigns_goal = static_cast<int>(u8at(avail + 0xB7));
  if (u8at(story_one) != 0x8B || u8at(story_one + 4) != 0x6A || u8at(story_one + 5) != u8at(avail + 0xA1) ||
      u8at(story_one + 7) != 0xE8 || rel(story_one + 8) != story ||
      u8at(avail + 0xA1) != static_cast<uint32_t>(kChapters - 1) ||
      static_cast<int>(u8at(avail + 0xB2)) != L.stories || K.campaigns_goal < 1 || K.campaigns_goal > L.stories)
    return false;
  // The kills of an enemy kind: the kill table's find and the counts' get, the two kinds counted with others.
  const uintptr_t kills = rel(avail + 0x7B);
  if (!pattern_at(kills, RE6CC_SKILL_KILLS)) return false;
  const uintptr_t find = rel(kills + 0x20), kget = rel(kills + 0x3C);
  for (uint32_t at : {0x51u, 0x7Eu, 0xABu, 0xD8u, 0x105u, 0x13Eu, 0x16Bu})
    if (rel(kills + at) != find) return false;
  for (uint32_t at : {0x68u, 0x95u, 0xC2u, 0xEFu, 0x11Cu, 0x155u, 0x182u})
    if (rel(kills + at) != kget) return false;
  if (!pattern_at(find, RE6CC_KILL_FIND) || !pattern_at(kget, RE6CC_KILL_GET) ||
      u32at(kills + 0x12E) != u32at(kills + 0x123) || u32at(kills + 0x135) != u32at(kills + 0x09) ||
      u32at(kills + 0x48) != u32at(kills + 0x14))
    return false;
  K.kill_key1 = u8at(kills + 0x1E);  // `push 3; push 0`: the find's first argument is the last pushed
  K.kill_key2 = u8at(kills + 0x1C);
  K.kill_group[0] = Group{};
  K.kill_group[0].kind = static_cast<uint16_t>(u32at(kills + 0x09));
  for (uint32_t at : {0x135u, 0x160u}) K.kill_group[0].of[K.kill_group[0].n++] = static_cast<uint16_t>(u32at(kills + at));
  K.kill_group[1] = Group{};
  K.kill_group[1].kind = static_cast<uint16_t>(u32at(kills + 0x14));
  for (uint32_t at : {0x48u, 0x73u, 0x9Eu, 0xCBu, 0xF8u, 0x123u})
    K.kill_group[1].of[K.kill_group[1].n++] = static_cast<uint16_t>(u32at(kills + at));
  // The find: the first record's key2 is where the walk starts; the entry, key1 and the kind read about it.
  const int at_entry = static_cast<int8_t>(u8at(find + 0x4D));
  K.kill_count = u32at(find + 2);
  K.kill_table = u32at(find + 0x1D) + at_entry;
  K.k_index = 0;
  K.k_key2 = static_cast<uint32_t>(-at_entry);
  K.k_key1 = static_cast<uint32_t>(-at_entry + static_cast<int8_t>(u8at(find + 0x24)));
  K.k_kind = static_cast<uint32_t>(-at_entry + static_cast<int8_t>(u8at(find + 0x33)));
  K.kill_rec = u8at(find + 0x3B);
  K.nkills = u8at(kget + 6);
  K.kills = u32at(kget + 14);
  // The weapon kills: the switch over the kinds, its cases (each adds up its weapons through the one-weapon count)
  // and its default (the weapon asked, alone, inline).
  const uintptr_t wk = rel(avail + 0x8F);
  if (!pattern_at(wk, RE6CC_SKILL_WEAPON_KILLS)) return false;
  const uint32_t first = static_cast<uint32_t>(-static_cast<int32_t>(u32at(wk + 8))), last = u8at(wk + 0x10);
  const uintptr_t dflt = rel(wk + 0x13), btab = u32at(wk + 0x1A), jtab = u32at(wk + 0x21);
  if (!pattern_at(dflt, RE6CC_WEAPON_DEFAULT) || !image::whole().contains(btab) || !image::whole().contains(jtab))
    return false;
  uintptr_t one = 0;
  K.nweapon_groups = 0;
  for (uint32_t w = first; w <= first + last; ++w) {
    const uintptr_t c = u32at(jtab + 4 * u8at(btab + (w - first)));
    if (c == dflt) continue;
    if (!one && u8at(c + 5) == 0xE8) one = rel(c + 6);
    if (K.nweapon_groups >= static_cast<int>(sizeof(K.weapon_group) / sizeof(K.weapon_group[0]))) return false;
    Group& g = K.weapon_group[K.nweapon_groups];
    if (!pattern_at(one, RE6CC_WEAPON_FIND) || !weapon_case(c, one, &g)) return false;
    g.kind = static_cast<uint16_t>(w);
    ++K.nweapon_groups;
  }
  const uintptr_t wget = rel(dflt + 0x42);
  if (!one || rel(one + 0x48) != wget || !pattern_at(wget, RE6CC_WEAPON_GET) || u32at(one + 4) != u32at(dflt + 2) ||
      u32at(one + 0x14) != u32at(dflt + 0xE) || u8at(one + 0x1B) != u8at(dflt + 0x15) ||
      u8at(one + 0x23) != u8at(dflt + 0x1D))
    return false;
  K.weapon_count = u32at(one + 4);
  K.weapon_table = u32at(one + 0x14);
  K.w_weapon = u8at(one + 0x1B);
  K.weapon_rec = u8at(one + 0x23);
  K.nweapon_kills = u8at(wget + 6);
  K.weapon_stride = u8at(wget + 13);
  K.weapon_kills = u32at(wget + 17);
  // The screen: its user (the profile its list is filled from: the push makes it the one in use), its start
  // filling the list (slot 5 calls the fill), its frame's step and the closing step's case (the menus' save).
  K.user = u32at(user + 10);
  const uintptr_t push = rel(user + 23);
  if (!pattern_at(push, RE6CC_PROFILE_PUSH) || u8at(push + 0x46) != L.profile || u8at(push + 0x50) != L.profile ||
      u32at(user + 44) != K.rows)
    return false;
  const uintptr_t start = vslot(K.screen, 5), frame = vslot(K.screen, 8);
  bool fills = false;
  for (uintptr_t a = start; a && a < start + 0x600 && !fills; ++a)
    fills = mem::readable(a, 5) && u8at(a) == 0xE8 && rel(a + 1) == fill;
  const uintptr_t steps = frame ? mem::find_pattern(mem::Range{frame, frame + 0x100}, RE6CC_SCREEN_STEPS) : 0;
  if (!fills || !steps) return false;
  K.step = u8at(steps + 2);
  const uint32_t last_step = u8at(steps + 10);
  const uintptr_t closing =
      kStepClosing <= last_step ? u32at(u32at(steps + 27) + 4 * u8at(u32at(steps + 20) + kStepClosing)) : 0;
  if (!closing || u8at(closing) != 0x8B || u8at(closing + 1) != 0xCE || u8at(closing + 2) != 0xE8 ||
      rel(closing + 3) != save || (L.step && K.step != L.step))
    return false;
  // The kinds of list with a skill sold only after a requirement (a requirement of 2 or more: none for 0, the skill
  // before it for 1), from the game's own table.
  const uint32_t n = mem::read<uint32_t>(K.count);
  K.kinds_with_needs = 0;
  int needs = 0;
  for (uint32_t i = 0; i < n && i < 1024; ++i) {
    const uintptr_t r = K.table + K.rec_size * i;
    const int need = static_cast<int8_t>(mem::read<uint8_t>(r + K.r_need));
    if (need == K.need_kills || need == K.need_weapons || need == K.need_campaigns) {
      K.kinds_with_needs |= mem::read<uint8_t>(r + K.r_mask);
      ++needs;
    }
  }
  const uint32_t most = K.screen->size;
  const bool ok = n > 0 && n <= 1024 && image::whole().contains(K.table) && image::whole().contains(K.count) &&
                  image::whole().contains(K.kill_table) && image::whole().contains(K.kill_count) &&
                  image::whole().contains(K.weapon_table) && image::whole().contains(K.weapon_count) &&
                  K.nstates > 0 && K.states + K.nstates <= L.profile_size && K.kill_rec == 8 &&
                  std::max({K.k_key1, K.k_key2, K.k_index}) < K.kill_rec && K.k_kind + 2 <= K.kill_rec &&
                  K.kills + 8 * K.nkills <= L.profile_size && K.w_weapon + 2 <= K.weapon_rec &&
                  K.weapon_kills + K.weapon_stride * K.nweapon_kills <= L.profile_size && K.weapon_stride >= 4 &&
                  K.kind + 4 <= most && K.user + 4 <= most && K.nrows + 4 <= most && K.step + 1 <= most &&
                  K.rows + 16 * K.rows_most <= most && std::max({K.row_state, K.row_need, K.row_can}) < 16 &&
                  K.row_price + 4 <= 16 && needs > 0 && needs <= kMaxSkills;
  if (!ok) {
    logf("records: the skills are not as known (%u records, %d behind a requirement)", n, needs);
    return false;
  }
  logf("records: the skill table exe+0x%X (%u records of %u bytes: skill +%u, mask +%u, requirement +%u, its kind +%u "
       "and goal +%u, price +%u); the skill states cSystemInfo +0x%X (%u; %u bought); %d behind a requirement "
       "(kinds of list 0x%X)",
       static_cast<unsigned>(K.table - image::base()), n, K.rec_size, K.r_id, K.r_mask, K.r_need, K.r_param, K.r_goal,
       K.r_price, K.states, K.nstates, K.bought, needs, K.kinds_with_needs);
  logf("records: the skill screen (uIdSkillSet10): its step +0x%X, kind of list +0x%X, user +0x%X, rows +0x%X (16 "
       "bytes, at most %u, how many +0x%X; state +%u, requirement +%u, can be bought +%u, price +%u); it closes through "
       "the menus' save",
       K.step, K.kind, K.user, K.rows, K.rows_most, K.nrows, K.row_state, K.row_need, K.row_can, K.row_price);
  logf("records: a skill's requirements as the game tests them (exe+0x%X): the kills of an enemy kind (the kill table "
       "exe+0x%X, keys %u/%u, counts cSystemInfo +0x%X of %u; 0x%X counts %d kinds, 0x%X %d), the kills with a kind of "
       "weapon (the weapon table exe+0x%X, kills +0x%X every 0x%X of %u; %d kinds count several), %d campaigns "
       "complete",
       static_cast<unsigned>(avail - image::base()), static_cast<unsigned>(K.kill_table - image::base()), K.kill_key1,
       K.kill_key2, K.kills, K.nkills, K.kill_group[0].kind, K.kill_group[0].n, K.kill_group[1].kind,
       K.kill_group[1].n, static_cast<unsigned>(K.weapon_table - image::base()), K.weapon_kills, K.weapon_stride,
       K.nweapon_kills, K.nweapon_groups, K.campaigns_goal);
  return true;
}

// --- the weapons -------------------------------------------------------------------------------------------------
// The Records screen's WEAPONS page as its draw reads it, and the characters' saved inventories as the New Game's
// reset of them writes them.
struct WeaponLay {
  // The page: its number (the frame's jump table's case), the weapon table (the game's data: its records - how many
  // a global - and a record's size, row (a signed byte), "always named" byte, item word and mode byte); the counts
  // (cSystemInfo's: Times Used and Enemies Defeated, a record of `stride` bytes for each of `nstats` rows); a row's
  // name (mes_file name_base + the row); the rows (the largest row + 1, as the screen counts them).
  int page = -1, name_base = 0, rows = 0;
  uintptr_t table = 0, count = 0;
  uint32_t rec = 0, r_entry = 0, r_always = 0, r_id = 0, r_mode = 0;
  uint32_t uses = 0, kills = 0, stride = 0, nstats = 0;
  uint32_t id_mask = 0;  // an inventory word's item (the rest its mode)
  // The saved inventories (a profile's cGameInfo's live data): the records, a record's size, how many; the slot in
  // hand (a byte), the slots (an item word, its count a word at +slot_count) and how many, the weapon slots (the
  // first ones); "kept" (not had) and "had" (a full magazine) as counts.
  uint32_t carried = 0, carried_size = 0, ncarried = 0, hand = 0, slots = 0, nslots = 0, slot_size = 0,
           slot_count = 0, weapon_slots = 0;
  int kept = 0, had = 0;
  // Each character's weapon table (the game's data, sGameMan's gets): the tables and their counts (a dword each, of
  // `ntables` characters), an entry's size, its stage (a word: below 0, from the start) and its item.
  uintptr_t tables = 0, counts = 0;
  uint32_t ntables = 0, t_size = 0, t_stage = 0, t_id = 0;
};
WeaponLay W;
bool g_weapons = false, g_carried = false;

// The page: its draw (the frame's page 1), the row's reads (the table's record, its "always named" byte, Times
// Used), the cursor's "Enemies Defeated", the name's base; the row a weapon's use counts on (the item's word).
bool discover_weapons() {
  const uintptr_t pages = game::sig_one("kSigRecordPages"), rows = game::sig_one("kSigRecordWeaponRows"),
                  kills = game::sig_one("kSigRecordWeaponKills"), entry = game::sig_one("kSigWeaponEntry"),
                  items = game::sig_one("kSigCharsItems");
  if (!pages || !rows || !kills || !entry || !items || !L.record) return false;
  // Page 1's case of the frame's jump table: `mov ecx, esi; call draw`, 9 bytes after page 0's.
  const uintptr_t jtab = u32at(pages + 32);
  if (u32at(jtab + 4) != pages + 45 || u8at(pages + 45) != 0x8B || u8at(pages + 46) != 0xCE || u8at(pages + 47) != 0xE8)
    return false;
  const uintptr_t draw = rel(pages + 48);
  const mem::Range body{draw, draw + 0x800};
  const uintptr_t name = mem::find_pattern(mem::Range{draw, draw + 0x400}, RE6CC_WEAPON_NAME);
  if (!body.contains(rows) || !body.contains(kills) || !name || kills < rows) {
    logf("records: the WEAPONS page's reads are not in its draw (exe+0x%X)", static_cast<unsigned>(draw - image::base()));
    return false;
  }
  const uintptr_t find = rel(rows + 18), uses = rel(rows + 59), kget = rel(kills + 39);
  if (!pattern_at(find, RE6CC_WEAPON_ROW) || !pattern_at(uses, RE6CC_WEAPON_GET) || !pattern_at(kget, RE6CC_WEAPON_GET))
    return false;
  W.page = 1;
  W.count = u32at(find + 2);
  W.table = u32at(find + 19);
  W.rec = u8at(find + 33);
  W.r_entry = 0;  // `movsx edi, byte [eax]`: a record's first byte
  W.r_always = u8at(rows + 32);
  W.nstats = u8at(uses + 6);
  W.stride = u8at(uses + 13);
  W.uses = u32at(uses + 17);
  W.kills = u32at(kget + 17);
  W.name_base = static_cast<int>(u32at(name + 8));
  // The row a use counts on: the table's mode bytes (`mov eax, table + mode`), the item the word before (-2), the
  // row the byte at -10 from the mode.
  W.r_mode = u32at(entry + 24) - W.table;
  W.r_id = W.r_mode + static_cast<int8_t>(u8at(entry + 35));
  const int entry_at = static_cast<int>(W.r_mode) + static_cast<int8_t>(u8at(entry + 73));
  W.id_mask = u32at(items + 4);
  const uint32_t n = mem::read<uint32_t>(W.count);
  bool ok = u32at(rows + 12) == u32at(rows + 35) && u32at(kills + 14) == u32at(rows + 12) &&
            u8at(rows + 41) == L.profile && u32at(rows + 49) == L.profile_size && u8at(rows + 56) == L.profile_base &&
            u8at(kills + 20) == L.profile && u32at(kills + 29) == L.profile_size && u8at(kills + 36) == L.profile_base &&
            u8at(kget + 6) == W.nstats && u8at(kget + 13) == W.stride && W.uses != W.kills &&
            u32at(entry + 4) == W.count && u8at(entry + 56) == W.rec && entry_at == static_cast<int>(W.r_entry) &&
            W.r_mode < W.rec && W.r_id + 2 <= W.rec && W.r_always < W.rec && W.r_mode != W.r_always &&
            W.r_id != W.r_always && W.r_id < W.r_mode && W.stride >= 8 && W.nstats > 0 &&
            std::max(W.uses, W.kills) + 4 + W.stride * (W.nstats - 1) <= L.profile_size &&
            (W.uses > W.kills ? W.uses - W.kills : W.kills - W.uses) < W.stride &&
            W.name_base == records_text::kWeaponFirst && W.id_mask == 0xFFF &&
            image::whole().contains(W.table) && image::data().contains(W.count) && n > 0 && n <= 256;
  // The skills window reads the same table for the weapon kills: where both are understood, they agree.
  if (ok && g_skills)
    ok = K.weapon_table == W.table && K.weapon_count == W.count && K.weapon_rec == W.rec && K.w_weapon == W.r_id &&
         K.weapon_kills == W.kills && K.weapon_stride == W.stride && K.nweapon_kills == W.nstats;
  // The rows as the screen counts them: the largest row + 1, each one's counts in the profile's.
  W.rows = 0;
  for (uint32_t i = 0; ok && i < n; ++i) {
    const int e = static_cast<int8_t>(mem::read<uint8_t>(W.table + W.rec * i + W.r_entry));
    if (e >= W.rows) W.rows = e + 1;
  }
  ok = ok && W.rows > 0 && W.rows <= static_cast<int>(W.nstats) && W.rows <= kMaxWeaponRows &&
       W.rows <= records_text::kWeaponMsgs;
  if (!ok) {
    logf("records: the WEAPONS page is not as known (table exe+0x%X, %u records; counts +0x%X/+0x%X every 0x%X)",
         static_cast<unsigned>(W.table - image::base()), n, W.uses, W.kills, W.stride);
    return false;
  }
  logf("records: the WEAPONS page (the Records screen's page %d, exe+0x%X): %d rows from the weapon table exe+0x%X "
       "(%u records of %u bytes: row +%u, always named +%u, item +%u, mode +%u); Times Used cSystemInfo +0x%X, Enemies "
       "Defeated +0x%X (every 0x%X, %u); names from message %d",
       W.page, static_cast<unsigned>(draw - image::base()), W.rows, static_cast<unsigned>(W.table - image::base()), n,
       W.rec, W.r_entry, W.r_always, W.r_id, W.r_mode, W.uses, W.kills, W.stride, W.nstats, W.name_base);
  return true;
}

// The saved inventories: the New Game's reset of a campaign's two (the carried record, the default's slots copied
// into it, the weapon table and its counts), and its weapons due by the stage (kept made had).
bool discover_carried() {
  const uintptr_t reset = game::sig_one("kSigCarriedReset"), due = game::sig_one("kSigCarriedDue");
  if (!reset || !due || due < reset || due > reset + 0x180 || !L.gi_size || !g_copy_info) return false;
  const uintptr_t get = rel(reset + 18), defs = rel(reset + 69), tabs = rel(reset + 103), counts = rel(reset + 156);
  if (!pattern_at(get, RE6CC_CARRIED_GET) || !pattern_at(defs, RE6CC_DATA_GET) || !pattern_at(tabs, RE6CC_DATA_GET) ||
      !pattern_at(counts, RE6CC_DATA_GET) || rel(due + 24) != rel(reset + 134))
    return false;
  W.ncarried = u8at(get + 6);
  W.carried_size = u8at(get + 13);
  W.carried = u32at(get + 17);
  W.hand = u8at(reset + 77);
  W.slots = u8at(reset + 80);
  W.nslots = u32at(reset + 85);  // dwords: a slot is one
  W.slot_size = u8at(due + 65);
  W.slot_count = u8at(due + 58);
  W.weapon_slots = u8at(due + 68);
  W.kept = static_cast<int8_t>(u8at(due + 59));
  W.had = -1;  // `or edx, -1`
  W.tables = u32at(tabs + 14);
  W.counts = u32at(counts + 14);
  W.ntables = u8at(tabs + 6);
  W.t_size = u8at(due + 82);
  W.t_stage = 0;  // `movzx eax, word [esi]`
  W.t_id = u8at(due + 43);
  const mt::Class* info = mt::find("cGameInfo");
  const bool ok =
      u8at(reset + 2) == L.profile && u8at(reset + 117) == L.profile && u32at(reset + 5) == L.gi_size &&
      u32at(reset + 120) == L.gi_size && u32at(reset + 12) == L.gi_base && info && info->size == L.gi_size &&
      u8at(due + 79) == W.slot_count && u8at(tabs + 6) == u8at(counts + 6) && u8at(defs + 6) >= W.ncarried &&
      W.ntables == W.ncarried && W.ncarried == static_cast<uint32_t>(kCarriers) && W.slot_size == 4 &&
      W.nslots * W.slot_size + W.slots <= W.carried_size && W.hand < W.slots && W.slot_count + 2 <= W.slot_size &&
      W.weapon_slots > 0 && W.weapon_slots < W.nslots && W.kept == -2 &&
      W.carried + W.carried_size * W.ncarried <= L.gi_size && W.t_id + 2 <= W.t_size && W.t_stage + 2 <= W.t_size &&
      image::whole().contains(W.tables) && image::whole().contains(W.counts) && g_weapons;
  if (!ok) {
    logf("records: the saved inventories are not as known (cGameInfo +0x%X, %u of 0x%X bytes)", W.carried, W.ncarried,
         W.carried_size);
    return false;
  }
  logf("records: the saved inventories: cGameInfo +0x%X (%u characters, 0x%X bytes: in hand +%u, %u slots from +0x%X, "
       "the first %u weapons; kept %d, had %d); the weapon tables exe+0x%X, their counts exe+0x%X (%u bytes an entry: "
       "stage +%u, item +%u)",
       W.carried, W.ncarried, W.carried_size, W.hand, W.nslots, W.slots, W.weapon_slots, W.kept, W.had,
       static_cast<unsigned>(W.tables - image::base()), static_cast<unsigned>(W.counts - image::base()), W.t_size,
       W.t_stage, W.t_id);
  return true;
}

// --- the enemies -------------------------------------------------------------------------------------------------
// The Records screen's ENEMIES page as its draw reads it.
struct EnemyLay {
  // The page: its number (the frame's jump table's case), the enemy table (the game's data: its records - how many a
  // global - and a record's size, row (a signed byte), "always named" byte and "no Times Defeated" byte); the counts
  // (cSystemInfo's: Enemies Defeated and Times Defeated, a record of `stride` bytes for each of `nstats` rows); a
  // row's name (mes_file name_base + the row); the rows (the largest row + 1, as the screen counts them).
  int page = -1, name_base = 0, rows = 0;
  uintptr_t table = 0, count = 0;
  uint32_t rec = 0, r_entry = 0, r_always = 0, r_no_defeats = 0;
  uint32_t kills = 0, defeated = 0, stride = 0, nstats = 0;
};
EnemyLay E;
bool g_enemies = false;

// The page: its draw (the frame's page 2), the row's reads (the enemy table's record, its "always named" byte,
// Enemies Defeated, Times Defeated), the cursor's Times Defeated (drawn by the record's byte), the name's base.
bool discover_enemies() {
  const uintptr_t pages = game::sig_one("kSigRecordPages"), rows = game::sig_one("kSigRecordEnemyRows"),
                  defeats = game::sig_one("kSigRecordEnemyDefeats");
  if (!pages || !rows || !defeats || !L.record) return false;
  // Page 2's case of the frame's jump table: `mov ecx, esi; call draw`, 9 bytes after page 1's.
  const uintptr_t jtab = u32at(pages + 32);
  if (u32at(jtab + 8) != pages + 54 || u8at(pages + 54) != 0x8B || u8at(pages + 55) != 0xCE || u8at(pages + 56) != 0xE8)
    return false;
  const uintptr_t draw = rel(pages + 57);
  const mem::Range body{draw, draw + 0x800};
  const uintptr_t name = mem::find_pattern(mem::Range{draw, draw + 0x400}, RE6CC_WEAPON_NAME);
  if (!body.contains(rows) || !body.contains(defeats) || !name || defeats < rows) {
    logf("records: the ENEMIES page's reads are not in its draw (exe+0x%X)", static_cast<unsigned>(draw - image::base()));
    return false;
  }
  const uintptr_t find = rel(rows + 18), kget = rel(rows + 59), dget = rel(rows + 85);
  if (!pattern_at(find, RE6CC_WEAPON_ROW) || !pattern_at(kget, RE6CC_KILL_GET) || !pattern_at(dget, RE6CC_KILL_GET))
    return false;
  E.page = 2;
  E.count = u32at(find + 2);
  E.table = u32at(find + 19);
  E.rec = u8at(find + 33);
  E.r_entry = 0;  // `movsx edi, byte [eax]`: a record's first byte
  E.r_always = u8at(rows + 32);
  E.r_no_defeats = u8at(defeats + 2);
  E.nstats = u8at(kget + 6);
  E.kills = u32at(kget + 14);
  E.defeated = u32at(dget + 14);
  E.stride = 8;  // the gets' `mov eax, [ecx + eax*8 + counts]` (the pattern's SIB byte)
  E.name_base = static_cast<int>(u32at(name + 8));
  const uint32_t n = mem::read<uint32_t>(E.count);
  const uint32_t low = std::min(E.kills, E.defeated), high = std::max(E.kills, E.defeated);
  bool ok = u32at(rows + 12) == u32at(rows + 35) && u32at(rows + 12) == u32at(rows + 66) && u8at(rows + 41) == L.profile &&
            u32at(rows + 49) == L.profile_size && u8at(rows + 56) == L.profile_base && u8at(rows + 72) == L.profile &&
            u32at(rows + 75) == L.profile_size && u8at(rows + 82) == L.profile_base && u8at(dget + 6) == E.nstats &&
            low != high && high - low < E.stride && high + 4 + E.stride * (E.nstats - 1) <= L.profile_size &&
            E.nstats > 0 && E.rec >= 4 && E.r_always < E.rec && E.r_no_defeats < E.rec && E.r_always != E.r_entry &&
            E.r_no_defeats != E.r_entry && E.r_always != E.r_no_defeats && E.name_base == records_text::kEnemyFirst &&
            image::whole().contains(E.table) && image::data().contains(E.count) && n > 0 && n <= 256;
  // The skills window reads the same table and counts for the kills a skill asks: where both are understood, they
  // agree.
  if (ok && g_skills)
    ok = K.kill_table == E.table && K.kill_count == E.count && K.kill_rec == E.rec && K.k_index == E.r_entry &&
         K.kills == E.kills && K.nkills == E.nstats;
  // The rows as the screen counts them: the largest row + 1 (0x564780's walk), each one's counts in the profile's.
  E.rows = 0;
  for (uint32_t i = 0; ok && i < n; ++i) {
    const int e = static_cast<int8_t>(mem::read<uint8_t>(E.table + E.rec * i + E.r_entry));
    if (e >= E.rows) E.rows = e + 1;
  }
  ok = ok && E.rows > 0 && E.rows <= static_cast<int>(E.nstats) && E.rows <= kMaxEnemyRows &&
       E.rows <= records_text::kEnemyMsgs;
  if (!ok) {
    logf("records: the ENEMIES page is not as known (table exe+0x%X, %u records; counts +0x%X/+0x%X)",
         static_cast<unsigned>(E.table - image::base()), n, E.kills, E.defeated);
    return false;
  }
  logf("records: the ENEMIES page (the Records screen's page %d, exe+0x%X): %d rows from the enemy table exe+0x%X (%u "
       "records of %u bytes: row +%u, always named +%u, no Times Defeated +%u); Enemies Defeated cSystemInfo +0x%X, Times "
       "Defeated +0x%X (every %u, %u); names from message %d",
       E.page, static_cast<unsigned>(draw - image::base()), E.rows, static_cast<unsigned>(E.table - image::base()), n,
       E.rec, E.r_entry, E.r_always, E.r_no_defeats, E.kills, E.defeated, E.stride, E.nstats, E.name_base);
  return true;
}

// --- the Mercenaries' unlocks ----------------------------------------------------------------------------------------
// The stages a Mercenaries mode lets you pick and the characters (each costume) its character select offers, as the
// game's own getters work them out of the profile; the stage select and the character select as the game fills
// them.
struct MercLay {
  // The profile's bits (cSystemInfo's): the stages (a dword), the characters (64 bits).
  uint32_t stages = 0, chars = 0;
  // The stage getter: the bits always open, the profile's bits it passes on; each mode's first bit (the
  // Mercenaries' 0, No Mercy's the stage select's shift), the stage select's rows.
  uint32_t stage_always = 0, stage_copied = 0;
  int first[2] = {0, 0};
  int rows = 0;
  // The character getter: always available; No Mercy's extras (set in No Mercy while sGameMan's flag has its bit,
  // else cleared) - not the profile's.
  uint64_t char_always = 0, char_extras = 0, char_settable = 0;
  uint32_t mode_nomercy = 0, extras_flag = 0, extras_bit = 0;
  // The stage table (sGameMan's static data): a record (10 bytes, the stage's number a word at its start) a bit,
  // how many (a global).
  uintptr_t stage_table = 0, stage_count = 0;
  uint32_t stage_rec = 0;
  // The character tables, the Mercenaries' and No Mercy's (5 bytes a record, a record a bit): their select ids (the
  // record's +4; its costume the byte before), how many (globals).
  uintptr_t char_ids[2] = {}, char_count[2] = {};
  uint32_t char_rec = 0;
  // The stage select: its side panel's pages (each mode's: a count byte, then the ids; a page's size, how many) and
  // its counts, masks (a bit an id shown) and ids (a page's size in the screen).
  const mt::Class* stage_screen = nullptr;
  uintptr_t pages[2] = {};
  int npages[2] = {};
  uint32_t page_size = 0, p_counts = 0, p_masks = 0, p_ids = 0, p_ids_page = 0;
  // The character select: its copy of the characters available (64 bits, the constructor's); its rows (the
  // characters shown, a dword each), cursor and how many; each character's costume cursor, how many of its
  // costumes are offered and their list (a costume's place among its character's); the costumes' bits (a signed
  // byte each, `fam_stride` a character) and how many (a byte a character), the characters.
  const mt::Class* char_screen = nullptr;
  uint32_t cs_mask = 0, cs_rows = 0, cs_cursor = 0, cs_nrows = 0, cs_ccursor = 0, cs_counts = 0, cs_lists = 0,
           cs_list_size = 0;
  uintptr_t fam_bits = 0, fam_counts = 0;
  int families = 0, fam_most = 0, fam_stride = 0;
  // The Collection file each costume gives (by its bit; -1 none): Costume 1's at a match's end, Costumes 2 and 3's
  // with the online event that gave them.
  int8_t file[64];
};
MercLay M;
bool g_merc = false;

// The stage getter's chain after its head, walked to its `ret`: `mov edx, ecx` and `shr edx, n` (the last `shr ecx,
// n`), `test dl, 1` (`test cl, 1`), `je` over the bit's set - `mov eax, imm` (the first, with the bits always open
// in it), `or eax, imm` - which must be that bit alone; the bits always open in `mov eax, imm` and `or eax, imm`
// between the tests.
bool walk_stage_getter(uintptr_t at, uint32_t* always, uint32_t* copied) {
  uint32_t al = 0, cp = 0;
  int shift = -1, tested = -1;
  bool moved = false;
  for (int i = 0; i < 160; ++i) {
    const uint8_t b0 = static_cast<uint8_t>(u8at(at)), b1 = static_cast<uint8_t>(u8at(at + 1));
    if (b0 == 0xC3) {
      *always = al;
      *copied = cp;
      return moved && cp && !(al & cp);
    }
    if (b0 == 0x8B && b1 == 0xD1) {
      at += 2;
    } else if (b0 == 0xC1 && (b1 == 0xEA || b1 == 0xE9) && u8at(at + 2) < 32) {
      shift = static_cast<int>(u8at(at + 2));
      at += 3;
    } else if (b0 == 0xF6 && (b1 == 0xC2 || b1 == 0xC1) && u8at(at + 2) == 1 && shift >= 0) {
      tested = shift;
      at += 3;
    } else if (b0 == 0xB8 && !moved && tested < 0) {
      al = u32at(at + 1);
      moved = true;
      at += 5;
    } else if (b0 == 0x0D && tested < 0 && moved) {
      al |= u32at(at + 1);
      at += 5;
    } else if (b0 == 0x74 && tested >= 0 && moved) {
      const uintptr_t set = at + 2, to = set + u8at(at + 1);
      uint32_t adds = 0;
      if (u8at(set) == 0xB8 && to == set + 5 && !cp && (u32at(set + 1) & al) == al) adds = u32at(set + 1) & ~al;
      else if (u8at(set) == 0x0D && to == set + 5) adds = u32at(set + 1);
      else if (u8at(set) == 0x83 && u8at(set + 1) == 0xC8 && to == set + 3)
        adds = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(u8at(set + 2))));
      else return false;
      if (adds != (1u << tested) || (cp & adds)) return false;
      cp |= adds;
      tested = -1;
      at = to;
    } else {
      return false;
    }
  }
  return false;
}

// The stage select's start calls the panel's fill: `fill` within one of its calls' functions (their first 0x100).
bool start_fills(const mt::Class* c, uintptr_t fill) {
  const uintptr_t start = vslot(c, 5);
  for (uintptr_t a = start; a && a < start + 0x80; ++a) {
    if (u8at(a) == 0xC3) break;
    if (u8at(a) != 0xE8) continue;
    const uintptr_t t = rel(a + 1);
    if (fill >= t && fill < t + 0x100) return true;
  }
  return false;
}

// A costume's Costume 1 award (the match end's, one a character): its bit and its Collection file.
bool costume_award(uintptr_t t, int* bit, int* file, uintptr_t* set) {
  uint32_t mask = 0;
  if (pattern_at(t, RE6CC_COSTUME_AWARD8)) {
    mask = u8at(t + 27);
    *bit = static_cast<int>(u8at(t + 36));
    *set = rel(t + 38);
  } else if (pattern_at(t, RE6CC_COSTUME_AWARD32)) {
    mask = u32at(t + 26);
    *bit = static_cast<int>(u8at(t + 38));
    *set = rel(t + 40);
  } else {
    return false;
  }
  const uintptr_t f = mem::find_pattern({t, t + 0xA0}, RE6CC_COSTUME_FILE);
  if (!f || rel(f + 7) != L.file_set) return false;
  *file = static_cast<int>(u8at(f + 1));
  return u8at(t + 11) == L.profile && u32at(t + 14) == L.profile_size &&
         u32at(t + 21) == L.profile_base + M.chars && *bit < 32 && mask == (1u << *bit) &&
         *file < static_cast<int>(L.nfiles);
}

bool discover_merc() {
  std::memset(M.file, -1, sizeof(M.file));
  const uintptr_t sg = game::sig_one("kSigMercStageGetter"), cg = game::sig_one("kSigMercCharGetter"),
                  st = game::sig_one("kSigMercStageTable"), sf = game::sig_one("kSigMercStageFrame"),
                  sp = game::sig_one("kSigMercStagePanel"), spn = game::sig_one("kSigMercStagePanelNoMercy"),
                  cf = game::sig_one("kSigMercCharFill"), ev = game::sig_one("kSigMercCostumeEvents"),
                  aw = game::sig_one("kSigMercCostumeAwards");
  M.stage_screen = mt::find("uIdStartUp31");
  M.char_screen = mt::find("uIdStartUp32");
  const uint32_t mode = game::layout().mode;
  if (!sg || !cg || !st || !sf || !sp || !spn || !cf || !ev || !aw || !M.stage_screen || !M.char_screen || !mode ||
      !L.file_set || !frame_closes(M.stage_screen) || !frame_closes(M.char_screen))
    return false;
  const uint32_t base = L.profile_base;
  // The getters read the profile in use's bits - the ones a campaign's completion sets (discover_rewards).
  if (u8at(sg + 2) != L.profile || u32at(sg + 5) != L.profile_size || u32at(sg + 12) != base + L.stages ||
      u8at(cg + 5) != L.profile || u32at(cg + 8) != L.profile_size || u32at(cg + 16) != base + L.characters ||
      u32at(cg + 24) != base + L.characters + 4 || !walk_stage_getter(sg + 16, &M.stage_always, &M.stage_copied))
    return false;
  M.stages = L.stages;
  M.chars = L.characters;
  // The characters: always available (two or's of the low dword), No Mercy's extras by sGameMan's flag (set, else
  // cleared - the two branches' masks each other's complements), the mode the one the mod reads (game.cpp).
  M.char_always = static_cast<uint64_t>(u32at(cg + 32) | u32at(cg + 144));
  M.char_extras = u32at(cg + 170) |
                  static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(u8at(cg + 176)))))
                      << 32;
  const uint32_t keep_lo = u32at(cg + 185),
                 keep_hi = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(u8at(cg + 191))));
  M.mode_nomercy = u8at(cg + 154);
  M.extras_flag = u32at(cg + 161);
  M.extras_bit = u8at(cg + 165);
  if (u32at(cg + 150) != mode || keep_lo != ~static_cast<uint32_t>(M.char_extras) ||
      keep_hi != ~static_cast<uint32_t>(M.char_extras >> 32) || (M.char_always & M.char_extras) || !M.extras_bit ||
      M.mode_nomercy != game::kModeNoMercy)
    return false;
  // The stage select: its frame reads the stages open each frame (No Mercy's shifted to its first), into the rows.
  M.first[1] = static_cast<int>(u8at(sf + 36));
  M.rows = static_cast<int>(u8at(sf + 56));
  if (vslot(M.stage_screen, 8) != sf || u32at(sf + 23) != mode || u32at(sf + 9) != M.mode_nomercy ||
      rel(sf + 30) != sg || rel(sf + 40) != sg || M.rows != kMercStages || M.first[1] < M.rows ||
      M.first[1] + M.rows > 32)
    return false;
  for (int m = 0; m < 2; ++m)
    for (int r = 0; r < M.rows; ++r)
      if (!(((M.stage_always | M.stage_copied) >> (M.first[m] + r)) & 1)) return false;
  // The stage table: a record a stage bit (the Mercenaries' then No Mercy's) - `lea eax, [eax+eax*4]`, a word at
  // [eax*2]: 10 bytes.
  M.stage_count = u32at(st + 16);
  M.stage_table = u32at(st + 31);
  M.stage_rec = 10;
  if (!image::whole().contains(M.stage_count) || !image::whole().contains(M.stage_table) ||
      mem::read<uint32_t>(M.stage_count) < static_cast<uint32_t>(M.first[1] + M.rows))
    return false;
  // The stage select's side panel: the Mercenaries' fill and No Mercy's (its start's), over one set of the screen's
  // fields; the character tables they read.
  M.pages[0] = u32at(sp + 10) - 1;
  M.page_size = u8at(sp + 170);
  M.p_ids = u32at(sp + 16);
  M.p_counts = u32at(sp + 22);
  M.p_masks = M.p_counts + u8at(sp + 49);
  M.p_ids_page = u8at(sp + 167);
  M.char_count[0] = u32at(sp + 83);
  M.char_ids[0] = u32at(sp + 94);
  M.char_rec = u8at(sp + 130);
  M.pages[1] = u32at(spn + 20) - 1;
  M.char_count[1] = u32at(spn + 84);
  M.char_ids[1] = u32at(spn + 99);
  const uint32_t span0 = u32at(sp + 183) - (M.pages[0] + 1), span1 = u32at(spn + 205) - (M.pages[1] + 1);
  if (!M.page_size || span0 % M.page_size || span1 % M.page_size) return false;
  M.npages[0] = static_cast<int>(span0 / M.page_size);
  M.npages[1] = static_cast<int>(span1 / M.page_size);
  const uint32_t ssize = M.stage_screen->size;
  if (rel(sp + 1) != cg || rel(spn + 1) != cg || u32at(spn + 11) != M.p_ids || u32at(spn + 26) != M.p_counts ||
      u8at(spn + 52) != M.p_masks - M.p_counts || u8at(spn + 166) != M.p_masks - M.p_counts ||
      u8at(spn + 189) != M.p_ids_page || u8at(spn + 192) != M.page_size || u8at(spn + 144) != M.char_rec ||
      !start_fills(M.stage_screen, sp) || M.npages[0] < 1 || M.npages[0] > 8 || M.npages[1] < 1 || M.npages[1] > 8 ||
      M.p_masks + 4u * M.npages[1] > ssize || M.p_counts + 4u * M.npages[1] > ssize ||
      M.p_ids + M.p_ids_page * M.npages[1] > ssize || M.char_rec != 5)
    return false;
  for (int m = 0; m < 2; ++m) {
    const uint32_t n = image::whole().contains(M.char_count[m]) ? mem::read<uint32_t>(M.char_count[m]) : 0;
    if (!n || n > 64 || !image::whole().contains(M.char_ids[m] - 1)) return false;
  }
  // The character select: its start's fill of its lists, from its copy of the characters available.
  const uintptr_t cstart = vslot(M.char_screen, 5);
  M.cs_rows = u32at(cf + 22);
  M.cs_cursor = u32at(cf + 34);
  M.cs_nrows = u32at(cf + 40);
  M.cs_ccursor = u32at(cf + 53);
  M.cs_counts = u32at(cf + 68);
  M.cs_lists = u32at(cf + 86);
  M.fam_bits = u32at(cf + 129);
  M.fam_counts = u32at(cf + 170);
  M.cs_mask = u32at(cf + 214);
  M.fam_most = static_cast<int>(u8at(cf + 266));
  M.fam_stride = static_cast<int>(u8at(cf + 323));
  M.families = static_cast<int>(u8at(cf + 338));
  const uint32_t csize = M.char_screen->size;
  M.cs_list_size = M.families ? u32at(cf + 80) / static_cast<uint32_t>(M.families) : 0;
  if (cf < cstart || cf > cstart + 0x80 || M.families < 1 || M.families > kMercFamilies ||
      M.fam_most != kMercCostumes || M.fam_stride != kMercCostumes || u8at(cf + 19) != 4u * M.families ||
      u32at(cf + 80) != M.cs_list_size * M.families || M.cs_list_size != 4u * M.fam_most ||
      u32at(cf + 100) != static_cast<uint32_t>(-static_cast<int32_t>(M.cs_ccursor)) || u32at(cf + 220) != M.cs_mask + 4 ||
      u32at(cf + 237) != M.cs_lists || u32at(cf + 257) != M.cs_cursor || u32at(cf + 276) != M.fam_counts ||
      u8at(cf + 297) != M.cs_counts - M.cs_ccursor || u32at(cf + 353) != M.cs_nrows ||
      M.cs_rows + 4u * M.families > csize || M.cs_ccursor + 4u * M.families > csize ||
      M.cs_counts + 4u * M.families > csize || M.cs_lists + M.cs_list_size * M.families > csize ||
      M.cs_mask + 8 > csize || !image::whole().contains(M.fam_bits) || !image::whole().contains(M.fam_counts))
    return false;
  // What the profile can hold: every character's costumes but the ones always there and No Mercy's extras.
  M.char_settable = 0;
  for (int f = 0; f < M.families; ++f)
    for (int k = 0; k < static_cast<int>(mem::read<uint8_t>(M.fam_counts + f)) && k < M.fam_stride; ++k) {
      const int bit = static_cast<int8_t>(mem::read<uint8_t>(M.fam_bits + M.fam_stride * f + k));
      if (bit >= 0 && bit < 64) M.char_settable |= 1ull << bit;
    }
  M.char_settable &= ~(M.char_always | M.char_extras);
  if (!M.char_settable) return false;
  // The Collection files: Costume 1's (each match-end award's), Costumes 2 and 3's (the online event's table).
  uintptr_t set_all = 0;
  int awards = 0;
  for (int k = 0; k < 7; ++k) {
    int bit = 0, file = 0;
    uintptr_t set = 0;
    if (!costume_award(rel(aw + 32 + 10 * k), &bit, &file, &set) || (set_all && set != set_all)) return false;
    set_all = set;
    M.file[bit] = static_cast<int8_t>(file);
    ++awards;
  }
  const uintptr_t table = u32at(ev + 6), tail = mem::find_pattern({ev, ev + 0x200}, RE6CC_MERC_EVENTS_TAIL);
  if (!tail || rel(tail + 39) != L.file_set || u8at(tail + 82) != 8) return false;
  const uintptr_t end = u32at(tail + 95);
  if (end <= table || (end - table) % 8 || (end - table) / 8 > 64 || !image::whole().contains(table)) return false;
  int events = 0;
  for (uintptr_t e = table; e < end; e += 8, ++events) {
    const uint32_t bit = mem::read<uint32_t>(e), file = mem::read<uint32_t>(e + 4);
    if (bit >= 64 || file >= L.nfiles || !((M.char_settable >> bit) & 1)) return false;
    M.file[bit] = static_cast<int8_t>(file);
  }
  int nsettable = 0;
  for (int b = 0; b < 64; ++b) nsettable += (M.char_settable >> b) & 1;
  logf("records: the Mercenaries' unlocks: the stages cSystemInfo +0x%X (always open 0x%X, the profile's 0x%X; %d rows "
       "a mode from bit %d and %d), the stage table exe+0x%X; the characters +0x%X (64 bits: always 0x%llX, No Mercy's "
       "extras 0x%llX by sGameMan +0x%X bit %u; %d the profile's); the stage select's panel (counts +0x%X, masks +0x%X, "
       "ids +0x%X; %d and %d pages), the character select (its copy +0x%X; rows +0x%X, cursor +0x%X, how many +0x%X; "
       "costume cursors +0x%X, counts +0x%X, lists +0x%X; %d characters, the tables exe+0x%X and exe+0x%X); "
       "Collection files with %d Costume 1s and %d online events' costumes",
       M.stages, M.stage_always, M.stage_copied, M.rows, M.first[0], M.first[1],
       static_cast<unsigned>(M.stage_table - image::base()), M.chars,
       static_cast<unsigned long long>(M.char_always), static_cast<unsigned long long>(M.char_extras), M.extras_flag,
       M.extras_bit, nsettable, M.p_counts, M.p_masks, M.p_ids, M.npages[0], M.npages[1], M.cs_mask, M.cs_rows,
       M.cs_cursor, M.cs_nrows, M.cs_ccursor, M.cs_counts, M.cs_lists, M.families,
       static_cast<unsigned>(M.fam_bits - image::base()), static_cast<unsigned>(M.fam_counts - image::base()), awards,
       events);
  return true;
}

// --- the profile -------------------------------------------------------------------------------------------------
uintptr_t g_sinfo = 0;

uintptr_t profile() {
  if (!g_sinfo) return 0;
  const uint32_t i = mem::read<uint32_t>(g_sinfo + L.profile);
  return i < 2 ? g_sinfo + L.profile_base + i * L.profile_size : 0;
}

// The profile in use's cGameInfo: its live data is what Continue, New Game and Chapter Select start from (the menus'
// save copies it into the part the save file keeps).
uintptr_t game_info() {
  if (!g_sinfo || !L.gi_size) return 0;
  const uint32_t i = mem::read<uint32_t>(g_sinfo + L.profile);
  return i < 2 ? g_sinfo + L.gi_base + i * L.gi_size : 0;
}

// sInfoMan's lock, taken as the game takes it.
struct ProfileLock {
  CRITICAL_SECTION* cs = nullptr;
  ProfileLock() {
    if (!g_sinfo) return;
    if (mem::read<uint8_t>(g_sinfo + L.lock_use) || mem::read<uint8_t>(L.lock_flag)) {
      cs = reinterpret_cast<CRITICAL_SECTION*>(g_sinfo + L.lock_cs);
      EnterCriticalSection(cs);
    }
  }
  ~ProfileLock() {
    if (cs) LeaveCriticalSection(cs);
  }
  ProfileLock(const ProfileLock&) = delete;
  ProfileLock& operator=(const ProfileLock&) = delete;
};

template <typename T>
T& field(uintptr_t a) {
  return *reinterpret_cast<T*>(a);
}

uint32_t record_index(int c, int d, int ch) { return static_cast<uint32_t>((c * kDifficulties + d) * kChapters + ch); }
uintptr_t record_at(uintptr_t prof, int c, int d, int ch) { return prof + L.clear + 12 * record_index(c, d, ch); }
bool cleared(uintptr_t prof, int c, int d, int ch) { return field<uint32_t>(record_at(prof, c, d, ch)) != 0; }

// A story's characters (the Agent's records are Ada's: the story's second is the first's).
int story_char(int s, int k) { return s * 2 + k < kChars ? s * 2 + k : kChars - 1; }

// Every chapter of story `s` cleared on some difficulty by one of its characters.
bool story_complete(uintptr_t prof, int s) {
  for (int ch = 0; ch < kChapters; ++ch) {
    bool any = false;
    for (int d = 0; d < kDifficulties && !any; ++d)
      for (int k = 0; k < 2 && !any; ++k) any = cleared(prof, story_char(s, k), d, ch);
    if (!any) return false;
  }
  return true;
}

// Every chapter of every story cleared on difficulty `d` or a harder one.
bool whole_game(uintptr_t prof, int d) {
  for (int s = 0; s < L.stories; ++s)
    for (int ch = 0; ch < kChapters; ++ch) {
      bool any = false;
      for (int e = d; e < kDifficulties && !any; ++e)
        for (int k = 0; k < 2 && !any; ++k) any = cleared(prof, story_char(s, k), e, ch);
      if (!any) return false;
    }
  return true;
}

int percent_of(uintptr_t prof) {
  int n = 0;
  for (int c = 0; c < kChars; ++c)
    for (int d = 0; d < kDifficulties; ++d)
      for (int ch = 0; ch < kChapters; ++ch) n += cleared(prof, c, d, ch);
  return n * 100 / (kChars * kDifficulties * kChapters);
}

// The title table's records by counter and level (the table is the game's static data: indexed once).
uintptr_t g_defs[kMaxTitleRows + 1][kMaxLevels + 1] = {};

void index_titles() {
  std::memset(g_defs, 0, sizeof(g_defs));
  const uint32_t n = mem::read<uint32_t>(L.table_count);
  for (uint32_t i = 0; i < n && i < 4096; ++i) {
    const uintptr_t t = L.table + 12 * i;
    const int counter = mem::read<uint16_t>(t + 2), level = mem::read<uint8_t>(t + 4);
    if (counter >= 1 && counter <= kMaxTitleRows && level >= 1 && level <= kMaxLevels && !g_defs[counter][level])
      g_defs[counter][level] = t;
  }
}

// The title table's record of (counter, level), or 0.
uintptr_t title_def(int counter, int level) {
  return counter >= 1 && counter <= kMaxTitleRows && level >= 1 && level <= kMaxLevels ? g_defs[counter][level] : 0;
}

bool counter_used(int counter) {
  return counter >= 1 && counter <= static_cast<int>(L.ncounters) &&
         mem::read<uint8_t>(L.unused_counters + counter - 1) != 2;
}

bool counter_float(int counter) {
  const uintptr_t first = title_def(counter, 1);
  return first && mem::read<uint8_t>(first + 6) != 0;
}

// A family's counter as the Dog Tag screen's start reads it into a row (a float family's cut to a whole number).
uint32_t counter_value(uintptr_t prof, int counter) {
  const uintptr_t at = prof + L.counters + 4 * (counter - 1);
  if (counter_float(counter)) {
    const float f = field<float>(at);
    return f > 0 ? static_cast<uint32_t>(f) : 0;
  }
  return field<uint32_t>(at);
}

// A family's counter brought up to `goal` (never down), in the family's own kind (a float for two of them; the game's
// check rounds it, the screen cuts it - a whole number is the same to both). The titles held are the game's own
// count (the Title Collector's counter): the award's recount writes that one.
bool raise_counter(uintptr_t prof, int counter, uint32_t goal) {
  if (counter == L.titles_counter || counter_value(prof, counter) >= goal) return false;
  const uintptr_t at = prof + L.counters + 4 * (counter - 1);
  if (counter_float(counter)) field<float>(at) = static_cast<float>(goal);
  else field<uint32_t>(at) = goal;
  return true;
}

// Held, but its counter short of what the held level needs - so the game shows "? ? ?" for what earns it. Only an
// award from outside the game leaves a title so (the mod's before 2026-11-06); the titles held are the game's count.
bool counter_behind(uintptr_t prof, int counter) {
  const uint8_t held = field<uint8_t>(prof + L.states + counter - 1) & 0x7F;
  const uintptr_t t = held ? title_def(counter, held) : 0;
  return t && counter != L.titles_counter && counter_value(prof, counter) < mem::read<uint32_t>(t + 8);
}

// --- the skills ----------------------------------------------------------------------------------------------------
uintptr_t skill_record(uint32_t i) { return K.table + K.rec_size * i; }
uint32_t skill_records() {
  const uint32_t n = mem::read<uint32_t>(K.count);
  return n < 1024 ? n : 1024;
}
int record_need(uintptr_t r) { return static_cast<int8_t>(mem::read<uint8_t>(r + K.r_need)); }
bool behind_need(uintptr_t r) {
  const int need = record_need(r);
  return need == K.need_kills || need == K.need_weapons || need == K.need_campaigns;
}
// A record on a kind of list (its mask's bit).
bool listed_on(uintptr_t r, uint32_t kind) { return kind < 8 && ((mem::read<uint8_t>(r + K.r_mask) >> kind) & 1); }

uint8_t skill_state(uintptr_t prof, uint32_t id) { return id < K.nstates ? field<uint8_t>(prof + K.states + id) : 0; }

// The kills of one enemy entry, as the game's find and get read them: the entry of the first record with both keys
// and the kind (none: 0); an entry past the counts reads the first.
int kills_one(uintptr_t prof, uint32_t kind) {
  const uint32_t n = mem::read<uint32_t>(K.kill_count);
  for (uint32_t i = 0; i < n && i < 4096; ++i) {
    const uintptr_t r = K.kill_table + K.kill_rec * i;
    if (mem::read<uint8_t>(r + K.k_key1) != K.kill_key1 || mem::read<uint8_t>(r + K.k_key2) != K.kill_key2 ||
        mem::read<uint16_t>(r + K.k_kind) != kind)
      continue;
    const int entry = static_cast<int8_t>(mem::read<uint8_t>(r + K.k_index));
    if (entry < 0) return 0;
    return field<int32_t>(prof + K.kills + 8 * (static_cast<uint32_t>(entry) < K.nkills ? entry : 0));
  }
  return 0;
}

// The kills of an enemy kind (two kinds count others with them: J'avo Killer's and Zombie Hunter's).
int kills_of(uintptr_t prof, uint32_t kind) {
  for (const Group& g : K.kill_group)
    if (g.n && g.kind == kind) {
      int sum = 0;
      for (int i = 0; i < g.n; ++i) sum += kills_one(prof, g.of[i]);
      return sum;
    }
  return kills_one(prof, kind);
}

// The kills with one weapon: its record's entry (a signed byte; past the entries, the first).
int weapon_one(uintptr_t prof, uint32_t weapon) {
  const uint32_t n = mem::read<uint32_t>(K.weapon_count);
  for (uint32_t i = 0; i < n && i < 4096; ++i) {
    const uintptr_t r = K.weapon_table + K.weapon_rec * i;
    if (mem::read<uint16_t>(r + K.w_weapon) != weapon) continue;
    const uint32_t entry = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem::read<uint8_t>(r))));
    return field<int32_t>(prof + K.weapon_kills + K.weapon_stride * (entry < K.nweapon_kills ? entry : 0));
  }
  return 0;
}

// The kills with a kind of weapon (the game's switch adds up the handguns', the shotguns' ... ; any other alone).
int weapon_kills_of(uintptr_t prof, uint32_t weapon) {
  for (int k = 0; k < K.nweapon_groups; ++k) {
    const Group& g = K.weapon_group[k];
    if (g.kind != weapon) continue;
    int sum = 0;
    for (int i = 0; i < g.n; ++i) sum += weapon_one(prof, g.of[i]);
    return sum;
  }
  return weapon_one(prof, weapon);
}

int campaigns_complete(uintptr_t prof) {
  int n = 0;
  for (int s = 0; s < L.stories; ++s) n += story_complete(prof, s);
  return n;
}

// What a record's requirement counts, and its goal - as the game's test has them.
void need_count(uintptr_t prof, uintptr_t r, int* count, int* goal) {
  const int need = record_need(r);
  const uint32_t param = mem::read<uint16_t>(r + K.r_param);
  *goal = mem::read<uint16_t>(r + K.r_goal);
  if (need == K.need_kills) *count = kills_of(prof, param);
  else if (need == K.need_weapons) *count = weapon_kills_of(prof, param);
  else if (need == K.need_campaigns) *count = campaigns_complete(prof), *goal = K.campaigns_goal;
  else *count = 0, *goal = 0;
}

bool need_met(uintptr_t prof, uintptr_t r) {
  int count = 0, goal = 0;
  need_count(prof, r, &count, &goal);
  return count >= goal;
}

// The record behind a requirement for skill `id` on a kind of list, or 0.
uintptr_t needing(uint32_t id, uint32_t kind) {
  const uint32_t n = skill_records();
  for (uint32_t i = 0; i < n; ++i) {
    const uintptr_t r = skill_record(i);
    if (mem::read<uint8_t>(r + K.r_id) == id && listed_on(r, kind) && behind_need(r)) return r;
  }
  return 0;
}

// The profile the skill screen fills its list from (its user's), or 0.
uintptr_t skill_profile(uintptr_t screen) {
  if (!screen || !g_sinfo) return 0;
  const uint32_t u = mem::read<uint32_t>(screen + K.user);
  return u < 2 ? g_sinfo + L.profile_base + u * L.profile_size : 0;
}

// The skill screen's row of a skill (its first byte the skill the row shows - the level held, or the first), or 0.
uintptr_t skill_row(uintptr_t screen, uint32_t id) {
  const int32_t n = mem::read<int32_t>(screen + K.nrows);
  for (int32_t i = 0; i < n && i < static_cast<int32_t>(K.rows_most); ++i) {
    const uintptr_t row = screen + K.rows + 16 * i;
    if (mem::read<uint8_t>(row) == id) return row;
  }
  return 0;
}

// The weapon table (the game's static data) and the WEAPONS page's rows, read as the game reads them.
uint32_t weapon_records() {
  const uint32_t n = mem::read<uint32_t>(W.count);
  return n <= 256 ? n : 0;
}
uintptr_t weapon_record(uint32_t i) { return W.table + W.rec * i; }
int record_row(uintptr_t r) { return static_cast<int8_t>(mem::read<uint8_t>(r + W.r_entry)); }

// A page row's record, as the page's draw finds it: the first with that row, or 0.
uintptr_t row_record(int e) {
  const uint32_t n = weapon_records();
  for (uint32_t i = 0; i < n; ++i)
    if (record_row(weapon_record(i)) == e) return weapon_record(i);
  return 0;
}

// The row a use of an item in a mode counts on, as the game's own lookup finds it (the item, and the mode or a
// record of any mode), or -1.
int row_of(uint32_t item, int mode) {
  const uint32_t n = weapon_records();
  for (uint32_t i = 0; i < n; ++i) {
    const uintptr_t r = weapon_record(i);
    const int m = static_cast<int8_t>(mem::read<uint8_t>(r + W.r_mode));
    if (mem::read<uint16_t>(r + W.r_id) == item && (m < 0 || m == mode)) return record_row(r);
  }
  return -1;
}

uint32_t& row_uses(uintptr_t prof, int e) { return field<uint32_t>(prof + W.uses + W.stride * e); }
uint32_t row_kills(uintptr_t prof, int e) { return field<uint32_t>(prof + W.kills + W.stride * e); }
// More than the one use the mod gives, or a kill: the game's own count, never taken back.
bool row_played(uintptr_t prof, int e) { return row_uses(prof, e) > 1 || row_kills(prof, e) > 0; }

// A character's saved inventory (its carried record in the profile's cGameInfo) and its slots.
uintptr_t carried_at(uintptr_t gi, int c) { return gi + W.carried + W.carried_size * c; }
uintptr_t slot_at(uintptr_t rec, uint32_t s) { return rec + W.slots + W.slot_size * s; }
uint32_t slot_item(uintptr_t slot) { return field<uint16_t>(slot) & W.id_mask; }
int16_t& slot_count(uintptr_t slot) { return field<int16_t>(slot + W.slot_count); }

// The weapon slot holding an item in a saved inventory, or 0.
uintptr_t weapon_slot(uintptr_t rec, uint32_t item) {
  for (uint32_t s = 0; s < W.weapon_slots; ++s)
    if (slot_item(slot_at(rec, s)) == item) return slot_at(rec, s);
  return 0;
}

// A character's weapons found on the way, in its weapon table's order: those whose stage is one (not below 0,
// the ones it starts with).
template <typename F>
void each_carried(int c, F&& f) {
  if (c < 0 || c >= static_cast<int>(W.ntables)) return;
  const uintptr_t t = mem::read<uintptr_t>(W.tables + 4 * c);
  const uint32_t n = mem::read<uint32_t>(W.counts + 4 * c);
  if (!image::whole().contains(t) || n > 64) return;
  for (uint32_t i = 0; i < n; ++i) {
    const uintptr_t e = t + W.t_size * i;
    if (mem::read<int16_t>(e + W.t_stage) < 0) continue;
    f(static_cast<uint32_t>(mem::read<uint16_t>(e + W.t_id)));
  }
}

// The enemy table (the game's static data) and the ENEMIES page's rows, read as the game reads them: a row's record
// the first with that row (0x564860), its counts the profile's.
uintptr_t enemy_record(int e) {
  const uint32_t n = mem::read<uint32_t>(E.count);
  for (uint32_t i = 0; i < n && i < 256; ++i) {
    const uintptr_t r = E.table + E.rec * i;
    if (static_cast<int8_t>(mem::read<uint8_t>(r + E.r_entry)) == e) return r;
  }
  return 0;
}
int32_t& enemy_kills(uintptr_t prof, int e) { return field<int32_t>(prof + E.kills + E.stride * e); }
int32_t enemy_defeated(uintptr_t prof, int e) { return field<int32_t>(prof + E.defeated + E.stride * e); }

// The Mercenaries' unlocks, as the game's own getters work them out of the profile: the stages open (always open,
// else the profile's bit), the characters available (the profile's bits and the ones always there; No Mercy's
// extras while sGameMan's flag has its bit, else none).
uint32_t& merc_stages(uintptr_t prof) { return field<uint32_t>(prof + M.stages); }
uint64_t merc_chars(uintptr_t prof) {
  return field<uint32_t>(prof + M.chars) | static_cast<uint64_t>(field<uint32_t>(prof + M.chars + 4)) << 32;
}
void set_merc_chars(uintptr_t prof, uint64_t bits) {
  field<uint32_t>(prof + M.chars) = static_cast<uint32_t>(bits);
  field<uint32_t>(prof + M.chars + 4) = static_cast<uint32_t>(bits >> 32);
}
uint32_t stages_open(uintptr_t prof) { return M.stage_always | (merc_stages(prof) & M.stage_copied); }
bool extras_on(bool no_mercy) {
  const uintptr_t gm = no_mercy ? game::sgameman() : 0;
  return gm && (mem::read<uint8_t>(gm + M.extras_flag) & M.extras_bit);
}
uint64_t chars_available(uintptr_t prof, bool no_mercy) {
  const uint64_t a = merc_chars(prof) | M.char_always;
  return extras_on(no_mercy) ? a | M.char_extras : a & ~M.char_extras;
}
// A stage bit's stage (the stage table's record of that bit), or 0.
uint16_t stage_number(int bit) {
  return bit >= 0 && static_cast<uint32_t>(bit) < mem::read<uint32_t>(M.stage_count)
             ? mem::read<uint16_t>(M.stage_table + M.stage_rec * bit)
             : 0;
}
// A costume's own number (the character table's: 0 Default, 1-3 Costume n), by its bit.
uint8_t costume_number(int bit) {
  return bit >= 0 && static_cast<uint32_t>(bit) < mem::read<uint32_t>(M.char_count[1])
             ? mem::read<uint8_t>(M.char_ids[1] - 1 + M.char_rec * bit)
             : 0;
}
// A character of the character select: how many costumes it has, the bit of its costume `k` (below 0: none).
int family_costumes(int f) {
  const int n = mem::read<uint8_t>(M.fam_counts + f);
  return n < M.fam_stride ? n : M.fam_stride;
}
int family_bit(int f, int k) { return static_cast<int8_t>(mem::read<uint8_t>(M.fam_bits + M.fam_stride * f + k)); }
// A character whose costumes the profile holds (not one of No Mercy's own: those are its extras).
bool profile_family(int f) {
  bool any = false;
  for (int k = 0; k < family_costumes(f); ++k) {
    const int bit = family_bit(f, k);
    if (bit < 0) continue;
    if (bit >= 64 || ((M.char_extras >> bit) & 1)) return false;
    any = true;
  }
  return any;
}

// The stage select's side panel (each character's best score on the stage), as its start fills it: an entry of a
// page shown when one of the character table's records with its id is available - the Mercenaries' fill tests the
// low dword (a record's place below 32), No Mercy's all 64 bits.
void fill_stage_panel(uintptr_t screen, uint64_t avail, bool no_mercy) {
  const int m = no_mercy ? 1 : 0;
  const uint32_t n = std::min<uint32_t>(mem::read<uint32_t>(M.char_count[m]), 64);
  for (int p = 0; p < M.npages[m]; ++p) {
    const uintptr_t page = M.pages[m] + M.page_size * p;
    const uint32_t count = mem::read<uint8_t>(page);
    uint32_t mask = 0;
    for (uint32_t i = 0; i < count && i < 32 && 1 + i < M.page_size; ++i) {
      const uint8_t id = mem::read<uint8_t>(page + 1 + i);
      for (uint32_t r = 0; r < n; ++r) {
        if (mem::read<uint8_t>(M.char_ids[m] + M.char_rec * r) != id) continue;
        if (no_mercy ? ((avail >> r) & 1) : (r < 32 && ((static_cast<uint32_t>(avail) >> r) & 1))) {
          mask |= 1u << i;
          break;
        }
      }
    }
    field<uint32_t>(screen + M.p_masks + 4 * p) = mask;
  }
}

// The character select's lists, as its start fills them from its copy of the characters available (the copy the
// game's own, its extras as the game left them, the rest the profile's now): a character shown while one of its
// costumes is offered, each its costumes offered. The cursor stays on its row (held to the rows), each character's
// costume cursor on the costume it was on while that is still offered (else the first).
void fill_char_select(uintptr_t screen, uint64_t avail) {
  const uint64_t was = field<uint32_t>(screen + M.cs_mask) |
                       static_cast<uint64_t>(field<uint32_t>(screen + M.cs_mask + 4)) << 32;
  const uint64_t now = (was & M.char_extras) | (avail & ~M.char_extras);
  field<uint32_t>(screen + M.cs_mask) = static_cast<uint32_t>(now);
  field<uint32_t>(screen + M.cs_mask + 4) = static_cast<uint32_t>(now >> 32);
  int nrows = 0;
  for (int f = 0; f < M.families; ++f) {
    const uintptr_t list = screen + M.cs_lists + M.cs_list_size * f;
    int32_t& count = field<int32_t>(screen + M.cs_counts + 4 * f);
    int32_t& cursor = field<int32_t>(screen + M.cs_ccursor + 4 * f);
    int on = -1;  // the costume the cursor was on (its place among its character's)
    if (cursor >= 0 && cursor < count && cursor < M.fam_most) on = field<int32_t>(list + 4 * cursor);
    int n = 0, at = 0;
    for (int k = 0; k < family_costumes(f) && n < M.fam_most; ++k) {
      const int bit = family_bit(f, k);
      if (bit < 0 || bit >= 64 || !((now >> bit) & 1)) continue;
      if (k == on) at = n;
      field<int32_t>(list + 4 * n++) = k;
    }
    for (int j = n; j < M.fam_most; ++j) field<int32_t>(list + 4 * j) = 0;
    count = n;
    cursor = at;
    if (n) field<int32_t>(screen + M.cs_rows + 4 * nrows++) = f;
  }
  for (int j = nrows; j < M.families; ++j) field<int32_t>(screen + M.cs_rows + 4 * j) = 0;
  field<int32_t>(screen + M.cs_nrows) = nrows;
  int32_t& row = field<int32_t>(screen + M.cs_cursor);
  if (row >= nrows) row = nrows ? nrows - 1 : 0;
  if (row < 0) row = 0;
}

// --- the screens ---------------------------------------------------------------------------------------------------
struct Screens {
  uintptr_t record = 0, dogtag = 0, skill = 0;
  uintptr_t merc_stage = 0, merc_char = 0;  // the Mercenaries' stage select and character select
  uint32_t mode = 0;                        // sGameMan's mode (game::Run's)
};

// The screens' coming and going: the Records screen, the Dog Tag screen, the skill screen and the Mercenaries'
// stage and character selects are units of their own classes; each class's start (as sUnit puts the screen into
// play) and destructor are hooked, so the screen up is known as it comes and goes - no walk over the units. On the
// main thread (sUnit's line frame), as the tick reads it.
std::vector<vthook::Hooked> g_screen_starts, g_screen_dtors;
uintptr_t g_up_record = 0, g_up_dogtag = 0, g_up_skill = 0, g_up_merc_stage = 0, g_up_merc_char = 0;
using ScreenStartFn = void(__thiscall*)(void*);
using ScreenDtorFn = void*(__thiscall*)(void*, unsigned);

void __thiscall hk_screen_start(void* self) {
  const uintptr_t start = vthook::original(g_screen_starts, self, 0);
  if (start) reinterpret_cast<ScreenStartFn>(start)(self);
  const auto u = reinterpret_cast<uintptr_t>(self);
  const mt::Class* c = units::class_of(u);
  if (c && c == L.record) g_up_record = u;
  else if (c && c == L.dogtag) g_up_dogtag = u;
  else if (c && c == K.screen) g_up_skill = u;
  else if (c && c == M.stage_screen) g_up_merc_stage = u;
  else if (c && c == M.char_screen) g_up_merc_char = u;
}

void* __thiscall hk_screen_dtor(void* self, unsigned flags) {
  const auto u = reinterpret_cast<uintptr_t>(self);
  if (g_up_record == u) g_up_record = 0;
  if (g_up_dogtag == u) g_up_dogtag = 0;
  if (g_up_skill == u) g_up_skill = 0;
  if (g_up_merc_stage == u) g_up_merc_stage = 0;
  if (g_up_merc_char == u) g_up_merc_char = 0;
  const uintptr_t dtor = vthook::original(g_screen_dtors, self, 0);
  return dtor ? reinterpret_cast<ScreenDtorFn>(dtor)(self, flags) : self;
}

// The screens up: in the menus each one's, in a game only the skill screen's.
Screens find_screens(bool menus, uint32_t mode) {
  Screens s;
  s.mode = mode;
  if (menus) {
    s.record = g_up_record;
    s.dogtag = g_up_dogtag;
    if (g_merc) {
      s.merc_stage = g_up_merc_stage;
      s.merc_char = g_up_merc_char;
    }
  }
  if (g_skills) s.skill = g_up_skill;
  return s;
}

// Each screen class's start and destructor (in the slots sUnit's line frame calls), its own vtables only.
bool hook_screens() {
  const units::Layout& ul = units::layout();
  if (ul.start_slot < 0 || ul.destroy_slot < 0) return false;
  bool ok = true;
  int n = 0;
  for (const mt::Class* c : {L.record, L.dogtag, g_skills ? K.screen : nullptr, g_merc ? M.stage_screen : nullptr,
                             g_merc ? M.char_screen : nullptr}) {
    if (!c) continue;
    const int starts = vthook::hook(c, ul.start_slot, 0, false, reinterpret_cast<void*>(&hk_screen_start),
                                    &g_screen_starts, true);
    const int dtors = vthook::hook(c, ul.destroy_slot, 0, false, reinterpret_cast<void*>(&hk_screen_dtor),
                                   &g_screen_dtors, true);
    ok = ok && starts > 0 && starts == dtors;
    n += starts;
  }
  if (!ok) {
    vthook::unhook(g_screen_starts);
    vthook::unhook(g_screen_dtors);
  } else {
    logf("records: the screens' start (slot %d) and destructor (slot %d) hooked in %d vtables - each screen known as "
         "it comes and goes",
         ul.start_slot, ul.destroy_slot, n);
  }
  return ok;
}

// A screen is up between its opening (steps 0 and 1) and its closing.
bool screen_up(uintptr_t u) {
  const uint8_t step = mem::read<uint8_t>(u + L.step);
  return step >= 2 && step < kStepClosing;
}

// The skill screen is up (between its opening and its closing) with a list that has skills behind a requirement
// (the campaigns'; not the Mercenaries' or the other modes') and a user whose profile it is.
bool skills_up(uintptr_t u) {
  if (!g_skills || !u) return false;
  const uint8_t step = mem::read<uint8_t>(u + K.step);
  const uint32_t kind = mem::read<uint32_t>(u + K.kind);
  return step >= 2 && step < kStepClosing && kind < 8 && ((K.kinds_with_needs >> kind) & 1) && skill_profile(u);
}

// The menus' screens out of a game only; the skill screen anywhere (it comes up at a chapter's end too).
Screen which(const Screens& s, bool in_game) {
  if (skills_up(s.skill)) return Screen::kSkills;
  if (in_game) return Screen::kNone;
  if (s.record && screen_up(s.record)) {
    const uint32_t page = mem::read<uint32_t>(s.record + L.page);
    if (g_chapters && static_cast<int>(page) == L.profiles_page &&
        static_cast<int>(mem::read<uint32_t>(s.record + L.view)) == L.grid_view)
      return Screen::kChapters;
    if (g_medals && static_cast<int>(page) == L.medals_page) return Screen::kMedals;
    if (g_weapons && static_cast<int>(page) == W.page) return Screen::kWeapons;
    if (g_enemies && static_cast<int>(page) == E.page) return Screen::kEnemies;
  }
  if (s.dogtag && screen_up(s.dogtag)) {
    const uint8_t step = mem::read<uint8_t>(s.dogtag + L.step);
    if (g_emblems && (step == L.emblem_step || step == L.emblem_step + 1)) return Screen::kEmblems;
    if (g_titles && (step == L.title_step || step == L.title_step + 1)) return Screen::kTitles;
  }
  // The Mercenaries' stage select or character select, in the Mercenaries or No Mercy (the other modes use them too).
  if (g_merc && (s.mode == game::kModeMercenaries || s.mode == game::kModeNoMercy) &&
      ((s.merc_char && screen_up(s.merc_char)) || (s.merc_stage && screen_up(s.merc_stage))))
    return Screen::kMercenaries;
  return Screen::kNone;
}

// --- the changes ---------------------------------------------------------------------------------------------------
void achieve(int n) {
  if (!g_achieve || !g_sinfo || n < 0) return;
  g_achieve(reinterpret_cast<void*>(g_sinfo), static_cast<uint32_t>(n));
}

void set_byte_once(uintptr_t at) {
  if (!field<uint8_t>(at)) field<uint8_t>(at) = 1;
}

int g_changed = 0;  // changes made since the last save was asked for
// A saved inventory changed (the profile's cGameInfo, not its cSystemInfo): the menus' save runs for it though the
// game's own "changed?" (the cSystemInfo halves) says no.
bool g_info_changed = false;

int apply_chapters(uintptr_t prof, uintptr_t screen, const std::vector<ChapterSet>& sets) {
  bool story_was[4] = {}, game_was[kDifficulties] = {};
  for (int s = 0; s < L.stories && s < 4; ++s) story_was[s] = story_complete(prof, s);
  for (int d = 0; d < L.diff_achievements; ++d) game_was[d] = whole_game(prof, d);
  std::vector<int> chapter_achievements;
  int made = 0, taken = 0, timed = 0;
  const float none = game::layout().clock_most;
  {
    ProfileLock lock;
    for (const ChapterSet& c : sets) {
      if (c.chara >= kChars || c.difficulty >= kDifficulties || c.chapter >= kChapters) continue;
      const uintptr_t r = record_at(prof, c.chara, c.difficulty, c.chapter);
      uint32_t& flags = field<uint32_t>(r);
      uint32_t& rank = field<uint32_t>(r + 4);
      float& time = field<float>(r + 8);
      const uint8_t want = c.rank < kRanks ? c.rank : kRanks - 1;
      if (c.cleared) {
        if (!flags) {
          ++made;
          // A first clear's time: the game's own "none" (a real clear's time then replaces it).
          if (none > 0 && !(time > 0)) time = none;
          const int story = c.chara / 2;
          if (story < L.stories) {
            const int n = story * kChapters + c.chapter + 1;
            if (std::find(chapter_achievements.begin(), chapter_achievements.end(), n) == chapter_achievements.end())
              chapter_achievements.push_back(n);
            if (static_cast<uint32_t>(n) < L.nplates) set_byte_once(prof + L.plates + n);
            if (story == 1 && c.chapter == 0)
              for (int f : L.files_chris) set_byte_once(prof + L.files + f);
            if (story == 2 && c.chapter == 2)
              for (int f : L.files_jake) set_byte_once(prof + L.files + f);
          }
        }
        flags |= 1;
        rank = want;
        // Its best time typed (whole seconds, held at the game's most - what Chapter Select shows at most): a real
        // clear replaces it when faster (the game keeps the better, 0x563CA0).
        if (c.time >= 0 && none > 0) {
          time = c.time < none ? c.time : none;
          ++timed;
        }
      } else if (flags) {
        ++taken;
        flags = 0;
      }
      if (screen) field<uint8_t>(screen + L.grid + record_index(c.chara, c.difficulty, c.chapter)) =
          static_cast<uint8_t>(flags ? (rank < L.rank_most ? rank : L.rank_most) : L.not_cleared);
    }
    // What a story's completion unlocks, as the game's chapter end gives it.
    bool complete[4] = {};
    for (int s = 0; s < L.stories && s < 4; ++s) complete[s] = story_complete(prof, s);
    if (complete[0] || complete[1] || complete[2] || complete[3]) field<uint32_t>(prof + L.unlock) |= L.unlock_any;
    if (complete[1]) field<uint32_t>(prof + L.stages) |= L.stage_chris;
    if (complete[2]) field<uint32_t>(prof + L.stages) |= L.stage_jake;
    if (complete[0] && complete[1] && complete[2]) field<uint32_t>(prof + L.unlock) |= L.unlock_three;
    if (complete[3]) field<uint32_t>(prof + L.characters) |= L.char_ada;
    if (screen) field<uint32_t>(screen + L.percent) = static_cast<uint32_t>(percent_of(prof));
  }
  // The achievements, through the game's own call (outside the lock: the call takes it).
  for (int n : chapter_achievements) achieve(n);
  int whole = 0;
  for (int d = 0; d < L.diff_achievements; ++d)
    if (!game_was[d] && whole_game(prof, d)) {
      achieve(L.diff_achievement + d);
      ++whole;
    }
  int stories = 0;
  for (int s = 0; s < L.stories && s < 4; ++s) stories += !story_was[s] && story_complete(prof, s);
  logf("records: chapters: %d cleared, %d taken back, %d best time(s) set (%zu change(s)); %zu chapter achievement(s), "
       "%d whole-game; %d stor(ies) completed; Completed %d%%",
       made, taken, timed, sets.size(), chapter_achievements.size(), whole, stories, percent_of(prof));
  note("%d chapter record(s) set%s.", static_cast<int>(sets.size()), made ? ", with what each clear gives" : "");
  g_changed += static_cast<int>(sets.size());
  return static_cast<int>(sets.size());
}

int apply_medals(uintptr_t prof, const std::vector<MedalSet>& sets) {
  int n = 0;
  {
    ProfileLock lock;
    for (const MedalSet& m : sets) {
      if (m.cat >= kMedalCats) continue;
      const Medals& c = L.medal[m.cat];
      if (m.medal >= c.n || mem::read<uint8_t>(c.unused + m.medal) == 2) continue;
      field<uint8_t>(prof + c.counts + m.medal) = static_cast<uint8_t>(m.count < kMedalMost ? m.count : kMedalMost);
      ++n;
    }
  }
  logf("records: medals: %d count(s) set", n);
  note("%d medal count(s) set.", n);
  g_changed += n;
  return n;
}

int apply_emblems(uintptr_t prof, uintptr_t screen, const std::vector<uint8_t>& ids) {
  int n = 0, by_hand = 0;
  for (uint8_t e : ids) {
    if (e >= L.nemblems) continue;
    // The game's own achievement: the emblem and Steam's achievement. The game skips both under its
    // no-achievement state - then the emblem is set here.
    achieve(e);
    {
      ProfileLock lock;
      if (!field<uint8_t>(prof + L.emblems + e)) {
        field<uint8_t>(prof + L.emblems + e) = 1;
        ++by_hand;
      }
      if (screen) field<uint32_t>(screen + L.emblem_rows + L.row_size * e) = field<uint8_t>(prof + L.emblems + e);
    }
    ++n;
  }
  logf("records: emblems: %d awarded through the game's achievement (%d set by the mod: the game gave none)", n,
       by_hand);
  if (by_hand) note("%d emblem(s) awarded (the game is not giving achievements now: the emblems only).", n);
  else note("%d emblem(s) awarded, with their achievements.", n);
  g_changed += n;
  return n;
}

// The WEAPONS page: a row named by its first use (Times Used 0 -> 1), or that use taken back (1 -> 0) - never a row
// the game always names, nor one with more than that use or a kill.
int apply_weapon_rows(uintptr_t prof, const std::vector<WeaponSet>& sets) {
  int named = 0, back = 0;
  {
    ProfileLock lock;
    for (const WeaponSet& s : sets) {
      const int e = s.entry;
      const uintptr_t r = e < W.rows ? row_record(e) : 0;
      if (!r || mem::read<uint8_t>(r + W.r_always)) continue;
      uint32_t& uses = row_uses(prof, e);
      if (s.named && uses == 0) {
        uses = 1;
        ++named;
      } else if (!s.named && uses == 1 && !row_played(prof, e)) {
        uses = 0;
        ++back;
      }
    }
  }
  logf("records: the WEAPONS page: %d row(s) named (their first use), %d back to \"? ? ?\" (%zu asked)", named, back,
       sets.size());
  if (named && back) note("%d row(s) named, %d back to \"? ? ?\".", named, back);
  else if (back) note("%d row(s) back to \"? ? ?\".", back);
  else note("%d row(s) named on the WEAPONS page.", named);
  g_changed += named + back;
  return named + back;
}

// The saved inventories: a weapon of a character's table put in (kept -> had: a full magazine) or taken out (had
// -> kept, its loaded ammo with it). The slot in hand is not left on a weapon taken out: it moves to the first one
// still had.
int apply_carried(uintptr_t gi, const std::vector<CarrySet>& sets) {
  int given = 0, taken = 0;
  {
    ProfileLock lock;
    for (const CarrySet& s : sets) {
      if (s.chara >= kCarriers) continue;
      bool listed = false;
      each_carried(s.chara, [&](uint32_t item) { listed = listed || item == s.id; });
      const uintptr_t rec = carried_at(gi, s.chara);
      const uintptr_t slot = listed ? weapon_slot(rec, s.id) : 0;
      if (!slot) continue;
      int16_t& count = slot_count(slot);
      if (s.had && count == W.kept) {
        count = static_cast<int16_t>(W.had);
        ++given;
      } else if (!s.had && count != W.kept) {
        count = static_cast<int16_t>(W.kept);
        ++taken;
        const uint32_t i = static_cast<uint32_t>((slot - slot_at(rec, 0)) / W.slot_size);
        if (field<uint8_t>(rec + W.hand) == i)
          for (uint32_t k = 0; k < W.weapon_slots; ++k) {
            const uintptr_t o = slot_at(rec, k);
            if (slot_item(o) && slot_count(o) != W.kept) {
              field<uint8_t>(rec + W.hand) = static_cast<uint8_t>(k);
              break;
            }
          }
      }
    }
  }
  logf("records: the saved inventories: %d weapon(s) put in, %d taken out (%zu asked)", given, taken, sets.size());
  if (given && taken) note("%d weapon(s) given, %d taken away.", given, taken);
  else if (taken) note("%d weapon(s) taken away.", taken);
  else note("%d weapon(s) given.", given);
  if (given + taken) g_info_changed = true;
  g_changed += given + taken;
  return given + taken;
}

// The ENEMIES page: a row the page draws "? ? ?" (neither count above 0, not one it always names) named by its first
// kill - Enemies Defeated, below 1, made 1. A row named already is left as it is (its counts are play's); nothing is
// taken back.
int apply_enemies(uintptr_t prof, const std::vector<uint8_t>& entries) {
  int named = 0;
  {
    ProfileLock lock;
    for (uint8_t e : entries) {
      const uintptr_t r = e < E.rows ? enemy_record(e) : 0;
      if (!r || mem::read<uint8_t>(r + E.r_always)) continue;
      int32_t& kills = enemy_kills(prof, e);
      if (kills < 1 && enemy_defeated(prof, e) < 1) {
        kills = 1;
        ++named;
      }
    }
  }
  logf("records: the ENEMIES page: %d row(s) named (their first kill; %zu asked)", named, entries.size());
  if (named) note("%d enemy row(s) named on the ENEMIES page.", named);
  else note("Nothing to name: named already.");
  g_changed += named;
  return named;
}

// The Dog Tag screen's row of a title family, as its start fills it: the state, the counter (its progress: the
// detail shows what earns the title only when it is above 0), the level-1 title's stars flag and goal - then the
// next level's goal, or (none) the held one's and no stars.
void title_row(uintptr_t screen, uintptr_t prof, int counter) {
  if (!screen || !counter_used(counter)) return;
  const uintptr_t row = screen + L.title_rows + L.row_size * (counter - 1);
  const uint8_t state = field<uint8_t>(prof + L.states + counter - 1);
  field<uint32_t>(row) = state;
  const uintptr_t first = title_def(counter, 1);
  if (!first) return;
  field<uint32_t>(row + L.title_progress) = counter_value(prof, counter);
  field<uint32_t>(row + 8) = mem::read<uint8_t>(first + 5);
  field<uint32_t>(row + 0x10) = mem::read<uint32_t>(first + 8);
  uintptr_t t = title_def(counter, (state & 0x7F) + 1);
  if (!t) {
    field<uint32_t>(row + 8) = 0;
    t = title_def(counter, state & 0x7F);
  }
  if (t) field<uint32_t>(row + 0x10) = mem::read<uint32_t>(t + 8);
}

// The title list's pages its buttons go to, as the screen's start works them out: page 0, and each page with a
// title whose counter has started (its row's progress above 0).
void title_pages(uintptr_t screen) {
  if (!screen) return;
  const int count = field<int32_t>(screen + L.title_count);
  const int last = field<int32_t>(screen + L.title_last_page);
  const int per = static_cast<int>(L.page_rows);
  uint32_t pages = 1;
  for (int p = 0; p <= last && p < 32; ++p)
    for (int i = p * per; i < (p + 1) * per && i < count && i < static_cast<int>(L.ncounters); ++i)
      if (mem::read<uint8_t>(L.unused_counters + i) != 2 &&
          field<int32_t>(screen + L.title_rows + L.row_size * i + L.title_progress) > 0) {
        pages |= 1u << p;
        break;
      }
  field<uint32_t>(screen + L.title_pages) = pages;
}

int titles_held(uintptr_t prof) {
  int n = 0;
  for (int i = 0; i < L.titles_counted; ++i) n += field<uint8_t>(prof + L.states + i) != 0;
  return n;
}

int apply_titles(uintptr_t prof, uintptr_t screen, const std::vector<TitleSet>& sets) {
  int n = 0, top = 0, raised = 0;
  const int held_before = titles_held(prof);
  {
    ProfileLock lock;
    for (const TitleSet& t : sets) {
      const uintptr_t def =
          counter_used(t.counter) && t.level >= 1 && t.level <= kMaxLevels ? title_def(t.counter, t.level) : 0;
      if (!def) continue;
      // A level above the one held is awarded (a title is never taken back) ...
      uint8_t& state = field<uint8_t>(prof + L.states + t.counter - 1);
      if ((state & 0x7F) < t.level) {
        state = static_cast<uint8_t>(t.level | 0x80);  // NEW, as the game's own check marks one
        top += t.level >= L.top_level;
        ++n;
      }
      // ... with its counter brought up to the title's goal, as earning it leaves it (the game shows what earns a
      // title once its counter has started). A level held already: its counter only, when short.
      raised += raise_counter(prof, t.counter, mem::read<uint32_t>(def + 8));
      title_row(screen, prof, t.counter);
    }
    // The titles held, as the game's check counts them into their counter.
    field<uint32_t>(prof + L.counters + 4 * (L.titles_counter - 1)) = static_cast<uint32_t>(titles_held(prof));
    title_row(screen, prof, L.titles_counter);
    title_pages(screen);
  }
  if (top) achieve(L.top_achievement);
  const int held = titles_held(prof);
  if (held >= L.titular && held_before < L.titular) achieve(L.titular_achievement);
  logf("records: titles: %d awarded (%d at the top level), %d held; %d counter(s) brought up to their title's goal", n,
       top, held, raised);
  if (n) note("%d title(s) awarded.", n);
  else if (raised) note("%d title(s)' counts brought up to what they need.", raised);
  else note("Nothing awarded: held already.");
  g_changed += n + raised;
  return n + raised;
}

// Skills unlocked (the state a new one is left in once the screen has shown it: the screen sells it for skill points)
// or locked again (0: the screen shows "? ? ?" and what it asks) - never a bought one, never one whose requirement
// the game's own counts meet (the game sells it whatever its state, and gives it its state back at the next save).
// The screen's row for it is set as the game's fill sets it: its state, and whether it can be bought. Nothing else
// changes: the skill points, the sets, a skill's levels. The screen's own save as it closes keeps the change.
int apply_skills(uintptr_t prof, uintptr_t screen, const std::vector<SkillSet>& sets) {
  const uint32_t kind = mem::read<uint32_t>(screen + K.kind);
  int unlocked = 0, locked = 0, bought = 0, met = 0;
  {
    ProfileLock lock;
    for (const SkillSet& set : sets) {
      const uintptr_t r = needing(set.id, kind);
      if (!r) continue;
      uint8_t& state = field<uint8_t>(prof + K.states + set.id);
      if (state == K.bought) {
        ++bought;
        continue;
      }
      const bool by_game = need_met(prof, r);
      if (by_game) {
        ++met;  // the game sells it by its own count (and gives it its state at the menus' next save)
      } else if (set.unlocked && !state) {
        state = kSkillSeen;
        ++unlocked;
      } else if (!set.unlocked && state) {
        state = 0;
        ++locked;
      }
      if (const uintptr_t row = skill_row(screen, set.id); row && mem::read<uint8_t>(row + K.row_state) != K.bought) {
        field<uint8_t>(row + K.row_state) = state;
        field<uint8_t>(row + K.row_can) = state || by_game ? 1 : 0;
      }
    }
  }
  logf("records: skills: %d unlocked, %d locked again (%zu asked; %d bought and %d whose requirement is met kept)",
       unlocked, locked, sets.size(), bought, met);
  char kept[96] = "";
  if (bought || met)
    std::snprintf(kept, sizeof(kept), " %d kept: %s.", bought + met,
                  bought && met ? "bought, or their requirement met" : bought ? "bought" : "their requirement is met");
  if (unlocked && locked) note("%d skill(s) unlocked, %d locked again.%s", unlocked, locked, kept);
  else if (unlocked) note("%d skill(s) unlocked: the skill screen sells them for skill points.%s", unlocked, kept);
  else if (locked) note("%d skill(s) locked again.%s", locked, kept);
  else note("Nothing to change.%s", kept);
  if (unlocked || locked)
    std::snprintf(g_note + std::strlen(g_note), sizeof(g_note) - std::strlen(g_note),
                  " Kept when you leave the skill screen, as your purchases are.");
  return unlocked + locked;
}

// The Mercenaries' unlocks: a stage opened or closed again (one the game always opens: neither), a costume unlocked
// (with the Collection file the game gives with it) or locked again (one always there, or one of No Mercy's own:
// neither). The stage select reads the profile each frame; its side panel and the character select's lists are
// filled again as their start fills them. The menus' own save keeps the change (asked after it, as the records
// windows' are): a mode's start takes the profile back from its saved copy.
int apply_merc(uintptr_t prof, const Screens& s, const std::vector<MercSet>& sets) {
  const bool no_mercy = s.mode == game::kModeNoMercy;
  int opened = 0, closed = 0, unlocked = 0, locked = 0, files = 0, kept = 0;
  {
    ProfileLock lock;
    uint32_t& stages = merc_stages(prof);
    uint64_t chars = merc_chars(prof);
    for (const MercSet& m : sets) {
      if (m.stage) {
        if (m.bit >= 32 || !((M.stage_copied >> m.bit) & 1) || ((M.stage_always >> m.bit) & 1)) {
          ++kept;
          continue;
        }
        const uint32_t b = 1u << m.bit;
        if (m.open && !(stages & b)) {
          stages |= b;
          ++opened;
        } else if (!m.open && (stages & b)) {
          stages &= ~b;
          ++closed;
        }
        continue;
      }
      if (m.bit >= 64 || !((M.char_settable >> m.bit) & 1)) {
        ++kept;
        continue;
      }
      const uint64_t b = 1ull << m.bit;
      if (m.open) {
        if (!(chars & b)) {
          chars |= b;
          ++unlocked;
        }
        // Its Collection file, as the game's own unlock gives it (set to 1, "new", if not had).
        if (M.file[m.bit] >= 0) {
          uint8_t& f = field<uint8_t>(prof + L.files + static_cast<uint32_t>(M.file[m.bit]));
          if (!f) {
            f = 1;
            ++files;
          }
        }
      } else if (chars & b) {
        chars &= ~b;
        ++locked;
      }
    }
    set_merc_chars(prof, chars);
    const uint64_t avail = chars_available(prof, no_mercy);
    if (s.merc_stage) fill_stage_panel(s.merc_stage, avail, no_mercy);
    if (s.merc_char) fill_char_select(s.merc_char, avail);
  }
  logf("records: the Mercenaries' unlocks: %d stage(s) opened, %d closed again; %d costume(s) unlocked (%d Collection "
       "file(s) with them), %d locked again; %d not the profile's to change (%zu asked; %s)",
       opened, closed, unlocked, files, locked, kept, sets.size(), no_mercy ? "No Mercy" : "the Mercenaries");
  char what[160] = "";
  size_t o = 0;
  auto part = [&](int n, const char* text) {
    if (n && o < sizeof(what)) o += std::snprintf(what + o, sizeof(what) - o, "%s%d %s", o ? ", " : "", n, text);
  };
  part(opened, "stage(s) opened");
  part(closed, "stage(s) closed again");
  part(unlocked, "costume(s) unlocked");
  part(locked, "costume(s) locked again");
  if (o) note("%s.", what);
  else note("Nothing to change.");
  const int made = opened + closed + unlocked + locked + files;
  g_changed += made;
  return made;
}

// --- the save ------------------------------------------------------------------------------------------------------
// The steps the menus take as they close with the profile changed (the game's own), from the tick: the profile
// committed and the chapter's numbers copied, the save asked of sStorage unless it is busy; then, once it has
// taken it, sInfoMan's two afters, and its finish once it is done with them.
enum class Save : uint8_t { kIdle, kStart, kStorage, kInfo };
Save g_save = Save::kIdle;
int g_save_ticks = 0;
bool g_save_again = false;
char g_save_text[120] = "";
constexpr int kSaveTicks = 60 * 60;  // a save given up after about a minute

void save_text(const char* s) { std::snprintf(g_save_text, sizeof(g_save_text), "%s", s); }

void save_tick() {
  if (g_save == Save::kIdle) return;
  const uintptr_t prof = profile();
  const uintptr_t storage = L.storage_global ? mem::read<uintptr_t>(L.storage_global) : 0;
  if (!prof || !storage || !g_sinfo) {
    g_save = Save::kIdle;
    save_text("Not saved: the game's save was not found.");
    logf("records: the save was given up (sStorage %08X, profile %08X)", static_cast<unsigned>(storage),
         static_cast<unsigned>(prof));
    return;
  }
  if (++g_save_ticks > kSaveTicks) {
    g_save = Save::kIdle;
    save_text("Not saved: the game did not finish its save (see the log).");
    logf("records: the game's save did not finish in %d frames - given up", kSaveTicks);
    return;
  }
  switch (g_save) {
    case Save::kStart: {
      if (!g_dirty(reinterpret_cast<void*>(prof)) && !g_info_changed) {
        g_save = Save::kIdle;
        save_text("Saved (nothing new to save).");
        return;
      }
      g_commit(reinterpret_cast<void*>(g_sinfo));
      const uint32_t i = mem::read<uint32_t>(g_sinfo + L.profile);
      g_copy_info(reinterpret_cast<void*>(g_sinfo + L.gi_base + (i < 2 ? i : 0) * L.gi_size));
      g_info_changed = false;  // in the part the save keeps now (written now, or by the game's next save)
      if (g_busy(reinterpret_cast<void*>(storage))) {
        // As the menus do: no save now. The commit keeps the change in the profile's saved copy, which the
        // game's next save writes.
        g_save = Save::kIdle;
        save_text("The game cannot save now: kept for its next save.");
        logf("records: sStorage is busy - the profile committed, not written");
        return;
      }
      g_request(reinterpret_cast<void*>(storage));
      g_save = Save::kStorage;
      logf("records: the profile's save asked for (%d change(s))", g_changed);
      g_changed = 0;
      return;
    }
    case Save::kStorage:
      if (mem::read<uint32_t>(storage + L.storage_busy) == 1) return;
      g_after1(reinterpret_cast<void*>(g_sinfo), 0, 1);
      g_after2(reinterpret_cast<void*>(g_sinfo), 2);
      g_save = Save::kInfo;
      return;
    case Save::kInfo:
      if (mem::read<uint32_t>(g_sinfo + L.info_wait) != 0) return;
      g_after2(reinterpret_cast<void*>(g_sinfo), 0);
      logf("records: the profile saved (%d frames)", g_save_ticks);
      if (g_save_again) {
        g_save_again = false;
        g_save = Save::kStart;
        g_save_ticks = 0;
        return;
      }
      g_save = Save::kIdle;
      save_text("Saved.");
      return;
    case Save::kIdle: return;
  }
}

void ask_save() {
  if (!g_dirty || !g_request) {
    save_text("Not saved: the game's save was not found (the changes last until the game next saves).");
    return;
  }
  if (g_save == Save::kIdle || g_save == Save::kStart) {
    g_save = Save::kStart;
    g_save_ticks = 0;
  } else {
    g_save_again = true;  // after the one under way
  }
  save_text("Saving...");
}

// --- the view ------------------------------------------------------------------------------------------------------
void read_view(View* v, uintptr_t prof, const Screens& s) {
  switch (v->screen) {
    case Screen::kChapters:
      for (int c = 0; c < kChars; ++c)
        for (int d = 0; d < kDifficulties; ++d)
          for (int ch = 0; ch < kChapters; ++ch) {
            const uintptr_t r = record_at(prof, c, d, ch);
            Cell& cell = v->cell[c][d][ch];
            cell.cleared = field<uint32_t>(r) != 0;
            const uint32_t rank = field<uint32_t>(r + 4);
            cell.rank = static_cast<uint8_t>(rank < kRanks ? rank : kRanks - 1);
            cell.time = field<float>(r + 8);  // as it is: the window shows it as Chapter Select does
          }
      v->percent = percent_of(prof);
      v->time_most = game::layout().clock_most;
      break;
    case Screen::kMedals: {
      const uint32_t mask = mem::read<uint32_t>(s.record + L.medal_mask);
      v->medal_cat = static_cast<int>(mem::read<uint32_t>(s.record + L.medal_cat));
      for (int c = 0; c < kMedalCats; ++c) {
        MedalCat& m = v->medal[c];
        const Medals& lm = L.medal[c];
        m.shown = (mask >> c) & 1;
        m.base = lm.base;
        m.n = 0;
        for (uint32_t i = 0; i < lm.n && m.n < kMaxMedals; ++i) {
          if (mem::read<uint8_t>(lm.unused + i) == 2) continue;
          m.id[m.n] = static_cast<uint8_t>(i);
          m.count[m.n] = field<uint8_t>(prof + lm.counts + i);
          ++m.n;
        }
      }
      break;
    }
    case Screen::kEmblems:
      for (int e = 0; e < kEmblems; ++e) v->emblem[e] = field<uint8_t>(prof + L.emblems + e);
      break;
    case Screen::kTitles:
      v->ntitles = 0;
      for (int counter = 1; counter <= static_cast<int>(L.ncounters) && v->ntitles < kMaxTitleRows; ++counter) {
        if (!counter_used(counter)) continue;
        TitleRow& row = v->title[v->ntitles];
        row = TitleRow{};
        row.counter = static_cast<uint16_t>(counter);
        const uint8_t state = field<uint8_t>(prof + L.states + counter - 1);
        row.earned = state & 0x7F;
        row.fresh = (state & 0x80) != 0;
        row.progress = counter_value(prof, counter);
        row.behind = counter_behind(prof, counter);
        for (int lv = 1; lv <= kMaxLevels; ++lv) {
          const uintptr_t t = title_def(counter, lv);
          if (!t) continue;
          Level& l = row.level[row.nlevels++];
          l.level = static_cast<uint8_t>(lv);
          l.goal = mem::read<uint32_t>(t + 8);
          l.msg = mem::read<uint16_t>(t);
          if (lv == 1) row.stars = mem::read<uint8_t>(t + 5) != 0;
        }
        if (row.nlevels) ++v->ntitles;
      }
      break;
    case Screen::kSkills: {
      // The skills the screen's list sells only after a requirement, in its order (the table's).
      const uintptr_t sp = skill_profile(s.skill);
      const uint32_t kind = mem::read<uint32_t>(s.skill + K.kind);
      const uint32_t n = skill_records();
      v->nskills = 0;
      for (uint32_t i = 0; i < n && sp && v->nskills < kMaxSkills; ++i) {
        const uintptr_t r = skill_record(i);
        if (!listed_on(r, kind) || !behind_need(r)) continue;
        Skill& k = v->skill[v->nskills++];
        k = Skill{};
        k.id = mem::read<uint8_t>(r + K.r_id);
        k.state = skill_state(sp, k.id);
        const int need = record_need(r);
        k.need = need == K.need_kills ? Need::kKills : need == K.need_weapons ? Need::kWeaponKills : Need::kCampaigns;
        need_count(sp, r, &k.count, &k.goal);
        k.met = k.count >= k.goal;
        k.price = mem::read<uint32_t>(r + K.r_price);
      }
      break;
    }
    case Screen::kWeapons: {
      // The page's rows in its order: those it always names counted, the rest listed with their counts.
      v->nweapon_rows = v->nweapon_always = 0;
      for (int e = 0; e < W.rows && v->nweapon_rows < kMaxWeaponRows; ++e) {
        const uintptr_t r = row_record(e);
        if (!r) continue;
        if (mem::read<uint8_t>(r + W.r_always)) {
          ++v->nweapon_always;
          continue;
        }
        WeaponRow& w = v->weapon_row[v->nweapon_rows++];
        w.entry = static_cast<uint8_t>(e);
        w.uses = row_uses(prof, e);
        w.kills = row_kills(prof, e);
        w.played = row_played(prof, e);
      }
      // Each character's weapons found on the way, as its saved inventory holds them (the row each counts on: its
      // first mode's, as the game's lookup finds it).
      const uintptr_t gi = g_carried ? game_info() : 0;
      v->carried_known = gi != 0;
      for (int c = 0; c < kCarriers; ++c) {
        Carrier& k = v->carrier[c];
        k = Carrier{};
        if (!gi) continue;
        const uintptr_t rec = carried_at(gi, c);
        bool ready = true;
        each_carried(c, [&](uint32_t item) {
          if (k.n >= kMaxCarried) return;
          Carried& w = k.weapon[k.n++];
          w.id = static_cast<uint16_t>(item);
          w.entry = static_cast<int8_t>(row_of(item, 0));
          const uintptr_t s = weapon_slot(rec, item);
          if (!s) {
            ready = false;
            return;
          }
          w.count = slot_count(s);
          w.had = w.count != W.kept;
        });
        k.ready = ready && k.n > 0;
      }
      break;
    }
    case Screen::kEnemies:
      // The page's rows in its order: those it always names counted, the rest listed with their counts.
      v->nenemy_rows = v->nenemy_always = 0;
      for (int e = 0; e < E.rows && v->nenemy_rows < kMaxEnemyRows; ++e) {
        const uintptr_t r = enemy_record(e);
        if (!r) continue;
        if (mem::read<uint8_t>(r + E.r_always)) {
          ++v->nenemy_always;
          continue;
        }
        EnemyRow& w = v->enemy_row[v->nenemy_rows++];
        w.entry = static_cast<uint8_t>(e);
        w.kills = enemy_kills(prof, e);
        w.defeated = enemy_defeated(prof, e);
        w.shows_defeated = !mem::read<uint8_t>(r + E.r_no_defeats);
      }
      break;
    case Screen::kMercenaries: {
      // The mode's stages in the stage select's order (its rows: the mode's first bit on), and the characters whose
      // costumes the profile holds in the character select's order, with the costumes it lists for each.
      const bool no_mercy = s.mode == game::kModeNoMercy;
      v->merc_no_mercy = no_mercy;
      v->merc_char_screen = s.merc_char && screen_up(s.merc_char);
      const uint32_t open = stages_open(prof);
      v->merc_nstages = 0;
      for (int r = 0; r < M.rows && v->merc_nstages < kMercStages; ++r) {
        const int bit = M.first[no_mercy ? 1 : 0] + r;
        MercStage& st = v->merc_stage[v->merc_nstages++];
        st.bit = static_cast<uint8_t>(bit);
        st.number = stage_number(bit);
        st.open = (open >> bit) & 1;
        st.always = (M.stage_always >> bit) & 1;
      }
      const uint64_t avail = chars_available(prof, no_mercy);
      v->merc_nfamilies = 0;
      for (int f = 0; f < M.families && v->merc_nfamilies < kMercFamilies; ++f) {
        if (!profile_family(f)) continue;
        MercFamily& fam = v->merc_family[v->merc_nfamilies];
        fam = MercFamily{};
        fam.family = static_cast<uint8_t>(f);
        for (int k = 0; k < family_costumes(f) && fam.n < kMercCostumes; ++k) {
          const int bit = family_bit(f, k);
          if (bit < 0 || bit >= 64) continue;
          MercCostume& c = fam.costume[fam.n++];
          c.bit = static_cast<uint8_t>(bit);
          c.costume = costume_number(bit);
          c.open = (avail >> bit) & 1;
          c.always = (M.char_always >> bit) & 1;
        }
        if (fam.n) ++v->merc_nfamilies;
      }
      break;
    }
    case Screen::kNone: break;
  }
}

}  // namespace

void init() {
  InitializeCriticalSection(&g_lock);
  g_lock_ready = true;
}

void install() {
  if (!game::ready()) return;
  g_chapters = discover_chapters();
  const bool screen = g_chapters && discover_record_screen();
  g_medals = screen && discover_medals();
  const bool achievement = g_chapters && discover_achievement();
  const bool save = g_chapters && discover_save();
  const bool rewards = achievement && discover_rewards();
  const bool dogtag = achievement && discover_dogtag();
  if (dogtag) index_titles();
  const bool closing = (screen || dogtag) && closing_step_known();
  // The skills need the profile, its lock (the achievement's) and the stories (a clear's rewards).
  g_skills = rewards && discover_skills();
  // The weapons: the page needs the Records screen and the profile (and agrees with the skills' weapon table); the
  // saved inventories the page and the game's save (a change in them is kept by its copy of the cGameInfo).
  g_weapons = screen && achievement && discover_weapons();
  g_carried = g_weapons && save && discover_carried();
  // The enemies: the page needs the Records screen and the profile (and agrees with the skills' kill table).
  g_enemies = screen && achievement && discover_enemies();
  // The Mercenaries' unlocks: the profile, its lock (the achievement's), the bits and the files a clear's rewards
  // read, the profile's save (a mode's start takes the profile back from its saved copy).
  g_merc = rewards && save && discover_merc();
  g_chapters = g_chapters && screen && rewards && closing;
  g_medals = g_medals && closing;
  g_emblems = dogtag && closing;
  g_titles = dogtag && closing;
  g_weapons = g_weapons && closing;
  g_carried = g_carried && g_weapons;
  g_enemies = g_enemies && closing;
  if (!save) {
    g_dirty = nullptr;
    g_request = nullptr;
  }
  g_known = g_chapters || g_medals || g_emblems || g_titles || g_skills || g_weapons || g_enemies || g_merc;
  if (g_known && !hook_screens()) {
    logf("records: the screens' coming and going could not be hooked - no records windows");
    g_known = g_chapters = g_medals = g_emblems = g_titles = g_skills = g_weapons = g_carried = g_enemies = g_merc =
        false;
  }
  logf("records: %s - chapters %s, medals %s, emblems %s, titles %s, skills %s, weapons %s (saved inventories %s), "
       "enemies %s, the Mercenaries' unlocks %s; the game's achievement %s, its profile save %s",
       g_known ? "understood" : "NOT understood (no records windows)", g_chapters ? "yes" : "no", g_medals ? "yes" : "no",
       g_emblems ? "yes" : "no", g_titles ? "yes" : "no", g_skills ? "yes" : "no", g_weapons ? "yes" : "no",
       g_carried ? "yes" : "no", g_enemies ? "yes" : "no", g_merc ? "yes" : "no", g_achieve ? "found" : "not found",
       save ? "found" : "not found");
}

bool known() { return g_known; }

void remove_hooks() {
  vthook::unhook(g_screen_starts);
  vthook::unhook(g_screen_dtors);
  g_up_record = g_up_dogtag = g_up_skill = g_up_merc_stage = g_up_merc_char = 0;
}

void tick(const game::Run& run) {
  if (!g_lock_ready) return;
  std::vector<Request> requests;
  {
    Lock lock;
    requests.swap(g_requests);
  }
  View v;
  v.known = g_known;
  v.achievements = g_achieve != nullptr;
  v.can_save = g_request != nullptr;
  if (g_known && !g_sinfo) g_sinfo = game::system("sInfoMan");
  const uintptr_t prof = g_known ? profile() : 0;
  const bool menus = prof && !run.in_game;
  Screens s;
  if (g_sinfo && (menus || g_skills)) {
    s = find_screens(menus, run.mode_known ? run.mode : 0);
    v.screen = which(s, !menus);
    // What the screens show, logged as it changes (a window that does not come up says why here).
    static uint32_t last[8] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u};
    const uint32_t now[8] = {
        s.record ? mem::read<uint8_t>(s.record + L.step) : ~0u,
        s.record ? mem::read<uint32_t>(s.record + L.page) * 0x10000 + mem::read<uint32_t>(s.record + L.view) : ~0u,
        s.dogtag ? mem::read<uint8_t>(s.dogtag + L.step) : ~0u,
        s.record || s.dogtag || s.skill || s.merc_stage || s.merc_char ? 1u : 0u,
        s.skill ? mem::read<uint8_t>(s.skill + K.step) : ~0u,
        s.skill ? mem::read<uint32_t>(s.skill + K.kind) * 0x10000 + (mem::read<uint32_t>(s.skill + K.user) & 0xFFFF)
                : ~0u,
        s.merc_stage ? mem::read<uint8_t>(s.merc_stage + L.step) * 0x10000 + (s.mode & 0xFFFF) : ~0u,
        s.merc_char ? mem::read<uint8_t>(s.merc_char + L.step) * 0x10000 + (s.mode & 0xFFFF) : ~0u};
    if (std::memcmp(now, last, sizeof(now))) {
      if (s.merc_stage)
        logf("records: the stage select %08X: step %u, mode 0x%X", static_cast<unsigned>(s.merc_stage), now[6] >> 16,
             now[6] & 0xFFFF);
      if (s.merc_char)
        logf("records: the character select %08X: step %u, mode 0x%X", static_cast<unsigned>(s.merc_char),
             now[7] >> 16, now[7] & 0xFFFF);
      if (s.record)
        logf("records: the Records screen %08X: step %u, page %u, view %u", static_cast<unsigned>(s.record), now[0],
             now[1] >> 16, now[1] & 0xFFFF);
      if (s.dogtag)
        logf("records: the Dog Tag screen %08X: step %u", static_cast<unsigned>(s.dogtag), now[2]);
      if (s.skill)
        logf("records: the skill screen %08X: step %u, kind of list %u (%s), user %u%s", static_cast<unsigned>(s.skill),
             now[4], now[5] >> 16,
             now[5] >> 16 < 8 && ((K.kinds_with_needs >> (now[5] >> 16)) & 1) ? "skills behind a requirement"
                                                                              : "none behind a requirement",
             now[5] & 0xFFFF, run.in_game ? " (in a game)" : "");
      if (!s.record && !s.dogtag && !s.skill && !s.merc_stage && !s.merc_char && last[3] == 1u)
        logf("records: no screen up");
      std::memcpy(last, now, sizeof(now));
    }
  }
  if (v.screen == Screen::kSkills && !requests.empty()) {
    std::vector<SkillSet> skills;
    for (const Request& r : requests)
      if (r.kind == Request::kSkill) skills.push_back(r.skill);
    if (!skills.empty()) apply_skills(skill_profile(s.skill), s.skill, skills);
    if (skills.size() != requests.size())
      logf("records: %zu change(s) dropped - their screen is not up", requests.size() - skills.size());
  } else if (prof && v.screen != Screen::kNone && !requests.empty()) {
    std::vector<ChapterSet> chapters;
    std::vector<MedalSet> medals;
    std::vector<uint8_t> emblems;
    std::vector<TitleSet> titles;
    std::vector<WeaponSet> rows;
    std::vector<CarrySet> carried;
    std::vector<uint8_t> enemies;
    std::vector<MercSet> merc;
    for (const Request& r : requests) {
      if (r.kind == Request::kChapter && v.screen == Screen::kChapters) chapters.push_back(r.chapter);
      else if (r.kind == Request::kMedal && v.screen == Screen::kMedals) medals.push_back(r.medal);
      else if (r.kind == Request::kEmblem && v.screen == Screen::kEmblems) emblems.push_back(r.emblem);
      else if (r.kind == Request::kTitle && v.screen == Screen::kTitles) titles.push_back(r.title);
      else if (r.kind == Request::kWeaponRow && v.screen == Screen::kWeapons) rows.push_back(r.weapon);
      else if (r.kind == Request::kCarry && v.screen == Screen::kWeapons && g_carried) carried.push_back(r.carry);
      else if (r.kind == Request::kEnemy && v.screen == Screen::kEnemies) enemies.push_back(r.enemy);
      else if (r.kind == Request::kMerc && v.screen == Screen::kMercenaries) merc.push_back(r.merc);
    }
    int made = 0;
    if (!chapters.empty()) made += apply_chapters(prof, s.record, chapters);
    if (!medals.empty()) made += apply_medals(prof, medals);
    if (!emblems.empty()) made += apply_emblems(prof, s.dogtag, emblems);
    if (!titles.empty()) made += apply_titles(prof, s.dogtag, titles);
    if (!rows.empty()) made += apply_weapon_rows(prof, rows);
    const uintptr_t gi = carried.empty() ? 0 : game_info();
    if (gi) made += apply_carried(gi, carried);
    if (!enemies.empty()) made += apply_enemies(prof, enemies);
    if (!merc.empty()) made += apply_merc(prof, s, merc);
    const size_t used = chapters.size() + medals.size() + emblems.size() + titles.size() + rows.size() + carried.size() +
                        enemies.size() + merc.size();
    if (used != requests.size())
      logf("records: %zu change(s) dropped - their screen is not up", requests.size() - used);
    if (made) ask_save();
  } else if (!requests.empty()) {
    logf("records: %zu change(s) dropped - their screen is not up", requests.size());
  }
  save_tick();
  if (v.screen == Screen::kSkills || (prof && v.screen != Screen::kNone)) read_view(&v, prof, s);
  static Screen was = Screen::kNone;
  if (v.screen != was && g_save == Save::kIdle) {
    g_note[0] = '\0';
    g_save_text[0] = '\0';
  }
  if (v.screen != was) {
    static const char* const kNames[] = {"none",
                                         "the Records chapter grid",
                                         "the Records medals",
                                         "the Dog Tag emblem list",
                                         "the Dog Tag title list",
                                         "the skill screen",
                                         "the Records weapons",
                                         "the Records enemies",
                                         "the Mercenaries' stage or character select"};
    logf("records: screen: %s", kNames[static_cast<int>(v.screen)]);
    was = v.screen;
  }
  v.saving = g_save != Save::kIdle;
  std::snprintf(v.note, sizeof(v.note), "%s%s%s", g_note, g_note[0] && g_save_text[0] ? " " : "", g_save_text);
  Lock lock;
  g_view = v;
}

View view() {
  if (!g_lock_ready) return View{};
  Lock lock;
  return g_view;
}

Screen screen() {
  if (!g_lock_ready) return Screen::kNone;
  Lock lock;
  return g_view.screen;
}

void set_chapters(const ChapterSet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kChapter;
    r.chapter = sets[i];
    g_requests.push_back(r);
  }
}

void set_medals(const MedalSet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kMedal;
    r.medal = sets[i];
    g_requests.push_back(r);
  }
}

void award_emblems(const uint8_t* emblems, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kEmblem;
    r.emblem = emblems[i];
    g_requests.push_back(r);
  }
}

void award_titles(const TitleSet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kTitle;
    r.title = sets[i];
    g_requests.push_back(r);
  }
}

void set_skills(const SkillSet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kSkill;
    r.skill = sets[i];
    g_requests.push_back(r);
  }
}

void set_weapon_rows(const WeaponSet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kWeaponRow;
    r.weapon = sets[i];
    g_requests.push_back(r);
  }
}

void set_carried(const CarrySet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kCarry;
    r.carry = sets[i];
    g_requests.push_back(r);
  }
}

void reveal_enemies(const uint8_t* entries, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kEnemy;
    r.enemy = entries[i];
    g_requests.push_back(r);
  }
}

void set_merc(const MercSet* sets, int n) {
  if (!g_lock_ready) return;
  Lock lock;
  for (int i = 0; i < n; ++i) {
    Request r;
    r.kind = Request::kMerc;
    r.merc = sets[i];
    g_requests.push_back(r);
  }
}

const char* medal_name(const View& v, int cat, int medal) {
  if (cat < 0 || cat >= kMedalCats) return "";
  const int msg = v.medal[cat].base + medal - records_text::kMedalFirst;
  return msg >= 0 && msg < records_text::kMedalMsgs ? records_text::kMedal[msg] : "";
}

}  // namespace re6cc::records
