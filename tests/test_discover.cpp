// Offline test of the mod's discovery and its damage, ammo and skill-point cheats over the game itself:
// BH6.exe laid out as in memory with its class records built as its static initializers build them
// (ghidra_proj/BH6.records.exe: tools/map_image.py, then tools/init_records.py) is copied to its own
// addresses inside this test's image, and the mod's real mt.cpp, units.cpp, game.cpp and cheats.cpp run over
// it - finding everything by the signatures of src/sigs.h and the class names, as the mod does in the game,
// and hooking the copy's own vtables. The game's own subtract-health then runs on made-up characters through
// the hooks.
//
// The test's image is based at 0x400000 with a section reserving the game's addresses (0x401000..0x18AB000) and
// its own code above them, so a linker script (the default's, with that section put first):
//
//   i686-w64-mingw32-ld --verbose | sed -n '/^=====/,/^=====/p' | sed '1d;$d' | sed 's|^  \.text  __image_base__
//       + ( __section_alignment__ < 0x1000 ? \. : __section_alignment__ ) :|  .exespace __image_base__ + 0x1000
//       (NOLOAD) : { *(.exespace) }\n  .text  __image_base__ + 0x1500000 :|' > tests/build/discover.ld
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static -Wl,--image-base=0x400000
//       -Wl,--disable-dynamicbase -Wl,-T,tests/build/discover.ld tests/test_discover.cpp -o tests/build/test_discover.exe
//   wine tests/build/test_discover.exe ghidra_proj/BH6.records.exe [-v]
//
// - every signature found exactly once (and every name its own);
// - the class records (mt::init over the rebuilt records) and the classes the mod asks for, with their vtables;
// - sUnit's lines read out of its own code; the pause, the characters' health, sPlayer's characters and who
//   drives each, a hit's damage and dealer, the game's own infinite ammo, the skill points - each operand as the
//   game's code has it;
// - the hooks: in every player class's vtable (subtract-health, hit, take-damage) and every enemy class's
//   take-damage (the overrides that call the game's first too), and the skill screen's two purchases prepared;
// - God mode: the game's own subtract-health still takes health from a character no switch spares, and none from
//   one that is (yours with God mode, the computer's with Partner takes no damage, never the network's); a hit on a
//   spared player reaches the game with no damage and is put back; a spared player's take-damage does nothing;
// - One hit kills: a player's hit takes all an enemy's health, an enemy's hit on another does not, and off nothing;
//   an enemy that keeps its life works a player's hit out as a big one - Deborah's own damage code (uEm4500's slot
//   126) run in the copy breaks each core at one hit (the core fight, the mine cart, the last fight), her own hold
//   kept; the game's own take-damage hands its parts (slot 129) the big hit; an enemy that can die as before;
//   Brzak's own damage code (its tongue's stagger at one hit; to its last point); Rasklapanje (its own floor),
//   Ustanak (a brain with no death reaction), Haos on the pipes (mode 2), the s0508 chopper before scene 150 - each
//   through the game's own take-damage: the big hit, its last point, never marked dead; Haos in the chase (mode 1)
//   dies at one hit, climbing (mode 0, mIsNoDead) keeps the game's point with the big hit;
// - Infinite ammo sets the game's bit on the characters this machine drives (no other), and clears only what it set;
//   the health tablets' three uses go through the mod while it is on, with the bit let go (they are used up);
// - Infinite skill points puts NOPs over both purchases' subtractions and takes them out again; the editor's value
//   reaches the profile's points at the pause only;
// - the tick reads the pause and the characters from made-up sBioMain and sPlayer instances;
// - Infinite air: the air (the combat gauge), its most and the under-water bit read out of the drowning and the
//   gauge on screen; the game's own drowning run on Leon in the copy - off, health 0 and the drowning played; on,
//   the air held at its most, a dive's drain through the game's own add and no drowning; on land, the computer's
//   partner and a character played over the network left to the game;
// - the modes, the chapter's numbers, the stage timers, the Mercenaries' match and an enemy's hit record read as
//   the game's code has them; Steady aim's two patches in and out;
// - the chapter's numbers: the game's own counter and clock adds land where the mod reads them; Freeze play time
//   (the game's clock add adds nothing), Deaths don't count, Misses don't count (shots held at hits, yours only);
//   the editors at the pause only, live and in the save's copy, a counter held at its most;
// - Automatic QTEs: the managers' check re-pointed and the scenes' 19 tests read as on (not with a character
//   online); the hook answers a QTE of yours (not drawn), not the computer's, not a context action, a dodge made
//   as a press yes, a press in time in its window (marked good), not with input held back or one failed;
// - the countdowns: the game's own match clock frozen, set at the pause, left alone online; Endless combo (the
//   earnings banked as earned), Combo Time always on, Kill enemies as they appear lasting one match; a stage's
//   countdown read through the mod, held, set at the pause, running down again when off; a gauge counting up left
//   to run, the CountDown's timer counting up held (time given back taken up); an FSM's own countdown (stage
//   851's): the game's own Timer order starting it, its own loop running it down, its own CountDown order showing
//   it through the rewritten read - held, set at the pause, a fresh start after a while not held to the old value,
//   running down again when off;
// - Add and Throw away through the game's own set-slot, top-up, refresh and clear: ammo onto its stacks then a
//   new one, herbs one a slot, a stack thrown away, a weapon nobody holds refused, nothing in play.
// - the records windows (records.cpp): the layouts read; over a made-up sInfoMan (at the game's global), sBioMain,
//   Steam's achievements and a storage, with the Records and Dog Tag screens as units on a made-up sUnit line: each
//   window's screen; chapters cleared and taken back as the game's own gets read them, a clear's rewards (template,
//   files, unlocks, the achievements through the game's own call), the game's own story/whole-game tests agreeing;
//   medal counts as the game's medal get reads them; emblems through the game's achievement (and the mod's under its
//   no-achievement state); titles raised (never lowered) with their counters brought up to the goal (never down; a
//   float family's as a float; the row's progress and the list's pages with them), a title held short flagged and
//   its counter brought up, "Rising Up" and "Titular Achievement"; the save through the game's own steps.
// - the weapons window (records.cpp): the WEAPONS page's and the saved inventories' layouts; the eight saved
//   inventories set up by the game's own New Game reset (over a made-up sGameMan); rows named by their first use or
//   taken back (never a row played), weapons put in and taken out of a character's saved inventory (the hand moved
//   off one taken out), as the game's own reads see them; the save run for a change to the cGameInfo alone.
// - the enemies window (records.cpp): the ENEMIES page's layout (agreeing with the skills' kill table); its rows and
//   counts as the game's own gets read them; "? ? ?" rows named by their first kill (a count below 1 made 1) as the
//   page's own test sees, rows it names already left as they are, a reveal asked with another page up dropped; the
//   save.
// - the Mercenaries window (records.cpp): the stages' and characters' bits as the game's own getters work them out;
//   the stage select and the character select through the mod's hooks, in the Mercenaries and No Mercy only; stages
//   opened and closed, costumes unlocked (with their Collection files) and locked, the ones always there refused; the
//   stage select's side panel and the character select's rows and lists as the game's own fill and start make them
//   (run in the copy), the cursor kept; the save at once.
#include "../src/mt.cpp"
#include "../src/units.cpp"
#include "../src/game.cpp"
#include "../src/cheats.cpp"
#include "../src/inventory.cpp"
#include "../src/merc.cpp"
#include "../src/records.cpp"

#include <algorithm>
#include <cstdarg>
#include <functional>
#include <malloc.h>
#include <memory>
#include <cstdio>
#include <string>

namespace re6cc {
bool g_verbose = false;
void logf(const char* fmt, ...) {
  if (!g_verbose) return;
  va_list ap;
  va_start(ap, fmt);
  std::printf("      log: ");
  std::vprintf(fmt, ap);
  std::printf("\n");
  va_end(ap);
}
namespace config {
Settings g_settings;
const Settings& get() { return g_settings; }
bool save_switches(const bool (&)[sw::kCount]) { return true; }
}  // namespace config
namespace image {
// BH6.exe's sections (Steam build 10112070), where this test copies them.
uintptr_t base() { return 0x400000; }
mem::Range text() { return {0x401000, 0x150102B}; }
mem::Range rdata() { return {0x1502000, 0x1737CAA}; }
mem::Range data() { return {0x1738000, 0x1878B00}; }
mem::Range whole() { return {0x400000, 0x18AB000}; }
}  // namespace image
}  // namespace re6cc

namespace {
using namespace re6cc;

// The game's addresses, reserved in this test's own image (not loaded from its file).
__attribute__((section(".exespace"), used)) char g_space[0x18AB000 - 0x401000];

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

bool load(const char* path) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  DWORD was = 0;
  const bool placed = reinterpret_cast<uintptr_t>(g_space) == 0x401000 &&
                      VirtualProtect(g_space, sizeof(g_space), PAGE_EXECUTE_READWRITE, &was);
  bool ok = placed && std::fseek(f, 0x1000, SEEK_SET) == 0 && std::fread(g_space, 1, sizeof(g_space), f) == sizeof(g_space);
  std::fclose(f);
  return ok;
}

// A made-up object of a class: a block whose first dword is one of the class's vtables (in the copy).
struct Obj {
  std::vector<uint8_t> mem;
  explicit Obj(size_t size, uintptr_t vt) : mem(size, 0) { std::memcpy(mem.data(), &vt, 4); }
  uintptr_t at() { return reinterpret_cast<uintptr_t>(mem.data()); }
  template <typename T>
  T& field(uint32_t off) {
    return *reinterpret_cast<T*>(mem.data() + off);
  }
};

uintptr_t vt_of(const char* cls) {
  const mt::Class* c = mt::find(cls);
  return c ? c->vtable : 0;
}

// What a stand-in for a game function saw.
int g_seen_damage = -1, g_seen_amount = -1, g_calls = 0;
void __thiscall stand_in_hit(void*, void* hit) {
  ++g_calls;
  g_seen_damage = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(hit) + game::layout().hit_damage);
}
char __thiscall stand_in_apply(void*, int, int, int, int, int, int) {
  ++g_calls;
  return 1;
}
void __thiscall stand_in_enemy(void*, void*, int amount) {
  ++g_calls;
  g_seen_amount = amount;
}
uint32_t g_seen_flags = 0;
// Steam's achievements as the game's unlock calls them (an ISteamUserStats' SetAchievement, thiscall (name)).
std::vector<std::string> g_steam_unlocked;
bool __thiscall steam_set(void*, const char* name) {
  g_steam_unlocked.push_back(name);
  return true;
}
// A player's slot 46 (thiscall (action, a, b, c, d), ret 0x14): the action it is asked to play.
int g_action = -1;
void __thiscall stand_in_action(void*, int action, int, int, int, int) { g_action = action; }
void __thiscall stand_in_use(void* player, int, int) {
  ++g_calls;
  g_seen_flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(player) + game::layout().ammo_flags);
}

void call_sub(Obj& o, int amount) {
  using Fn = void(__thiscall*)(void*, int);
  const uintptr_t vt = o.field<uintptr_t>(0);
  reinterpret_cast<Fn>(mem::read<uintptr_t>(vt + 4 * game::kSubLifeSlot))(o.mem.data(), amount);
}

// The scope camera's view and the gun's line as the game works them out, stood in for (the game's read the gun
// model's joints): down a made-up gun the test moves - its eye, and its direction as a yaw and a pitch (up the
// pitch's positive) in the world.
struct Gun {
  float eye[3] = {};
  float yaw = 0, pitch = 0;
};
Gun g_gun;
void gun_dir(float* d) {
  const float cp = std::cos(g_gun.pitch);
  d[0] = std::sin(g_gun.yaw) * cp;
  d[1] = std::sin(g_gun.pitch);
  d[2] = std::cos(g_gun.yaw) * cp;
}
uint32_t __fastcall stand_in_scope(void* cam) {
  const auto c = reinterpret_cast<uintptr_t>(cam);
  float d[3];
  gun_dir(d);
  float* eye = reinterpret_cast<float*>(c + cheats::g_sl.cam_eye);
  float* at = reinterpret_cast<float*>(c + cheats::g_sl.cam_at);
  for (int i = 0; i < 3; ++i) {
    eye[i] = g_gun.eye[i];
    at[i] = g_gun.eye[i] + d[i] * 300.0f;
  }
  return 0x7F01;  // al 1 (the rest of eax whatever the game's leaves)
}
void __fastcall stand_in_line(void* player) {
  const auto p = reinterpret_cast<uintptr_t>(player);
  float d[3];
  gun_dir(d);
  float* from = reinterpret_cast<float*>(p + cheats::g_sl.from);
  float* to = reinterpret_cast<float*>(p + cheats::g_sl.to);
  for (int i = 0; i < 3; ++i) {
    from[i] = g_gun.eye[i] + (i == 2 ? 0.25f : 0.0f);
    to[i] = from[i] + d[i] * 50.0f;
  }
}
double g_clock = 0;
double fake_now() { return g_clock; }
// A direction's yaw and pitch (up positive) in the world.
void angles_of(const float* d, float* yaw, float* pitch) {
  *yaw = std::atan2(d[0], d[2]);
  *pitch = std::atan2(d[1], std::sqrt(d[0] * d[0] + d[2] * d[2]));
}

// The game's kill award stood in (thiscall (player, amount) on sGameMan): it adds to what the player's combo earned.
void __thiscall stand_in_award(void* gm, uint32_t player, int amount) {
  *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(gm) + player * merc::g_m.stride + merc::g_m.earned) += amount;
}

// The players' health add stood in (thiscall (amount, a word); the game's tells its sound system, which the copy has
// none of): health added, kept to 0 .. its most.
void __thiscall stand_in_add(void* self, int amount) {
  const auto p = reinterpret_cast<uintptr_t>(self);
  auto& hp = *reinterpret_cast<int16_t*>(p + game::layout().hp);
  const int most = *reinterpret_cast<int16_t*>(p + game::layout().hp_max);
  int v = hp + static_cast<int16_t>(amount);
  hp = static_cast<int16_t>(v < 0 ? 0 : v > most ? most : v);
}
// A player's reset stood in (the game's reaches far into a running game): its flags words set to 0, as the game's.
void __thiscall stand_in_reset(void* self) {
  *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + game::layout().ammo_flags) = 0;
}
// A unit's start and destruction stood in (the game's own would run on a made-up unit): nothing done.
void __thiscall stand_in_start(void*) {}
void* __thiscall stand_in_dtor(void* self, unsigned) { return self; }
// A made-up unit started (or destroyed) through the mod's hook in its class's vtable, the game's own stood in for the
// call: the hook's own part runs. False when its vtable is not hooked.
bool through_hook(std::vector<vthook::Hooked>& hooks, void* unit, uintptr_t stand_in, const std::function<void()>& call) {
  const uintptr_t vt = *static_cast<uintptr_t*>(unit);
  for (vthook::Hooked& h : hooks)
    if (h.vt == vt) {
      const uintptr_t was = h.orig;
      h.orig = stand_in;
      call();
      h.orig = was;
      return true;
    }
  return false;
}

// The tick's run as dispatch makes it, with the panel able to show (the numbers it shows are read).
void game_tick(void* main, game::Run* r) {
  game::tick(main, r);
  r->panel = true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: test_discover.exe ghidra_proj/BH6.records.exe [-v]\n");
    return 2;
  }
  g_verbose = argc > 2 && !std::strcmp(argv[2], "-v");
  if (!load(argv[1])) {
    std::printf("FAIL  could not place %s at 0x401000\n", argv[1]);
    return 1;
  }
  std::printf("-- the signatures\n");
  {
    bool all = true, unique = true;
    for (const auto& s : game::kSigs) {
      const int n = static_cast<int>(mem::find_all(image::text(), s.pattern, 64).size());
      if (n != s.matches) {
        all = false;
        std::printf("      %s: %d match(es), want %d\n", s.name, n, s.matches);
      }
      int same = 0;
      for (const auto& t : game::kSigs) same += !std::strcmp(s.name, t.name);
      unique = unique && same == 1;
    }
    check(all, "every signature has the matches sigs.h says");
    check(unique, "every signature's name is its own");
  }

  std::printf("-- the class records\n");
  check(mt::init(), "mt::init finds MtObject's record and the root above it");
  check(mt::class_count() > 8000, "the records under the root (over 8000)");
  const mt::Class* main_c = mt::find("sBioMain");
  check(main_c && main_c->vtable && main_c->size == 0x104D8, "sBioMain: its vtable and size");
  check(vt_of("uPlayer") && vt_of("uPlPlayer") && vt_of("uEnemy") && vt_of("uCharacter") && vt_of("uPl00LeonUsa"),
        "uPlayer, uPlPlayer, uEnemy, uCharacter and a costume's vtables");
  check(mt::derives(mt::find("uPl00LeonUsa")->dti, mt::find("uPlayer")->dti), "a costume derives from uPlayer");

  std::printf("-- units and the game\n");
  check(units::discover(), "sUnit's lines read out of its code");
  const units::Layout& ul = units::layout();
  check(ul.stride == 24 && ul.line_flags == 0x3C && ul.line_head == 0x40 && ul.unit_flags == 0x10 &&
            ul.unit_next == 0x18 && ul.lines_at == 0x634,
        "lines of 24 bytes (flags +0x3C, first +0x40), a unit's state +0x10 and next +0x18, the count sUnit +0x634");
  check(game::discover(), "game::discover");
  const game::Layout& gl = game::layout();
  check(gl.paused == 0x1049C, "the pause: sBioMain +0x1049C");
  check(gl.hp == 0xF10 && gl.hp_max == 0xF12, "health +0xF10, its most +0xF12");
  check(gl.control == 0x4E90 && gl.players == 0x24 && gl.nplayers == 8, "who drives +0x4E90; sPlayer's 8 at +0x24");
  check(gl.hit_damage == 4 && gl.hit_attacker == 0x28, "a hit's damage +4, its dealer +0x28");
  check(gl.inventory == 0x46E0 && gl.item_size == 0x1C && gl.nitems == 24 && gl.ammo_flags == 0x4E74 &&
            gl.ammo_bit == 11,
        "the inventory +0x46E0 (24 of 0x1C); infinite ammo: bit 11 of +0x4E74");
  check(gl.profile == 0x20 && gl.profile_size == 0xFDD8 && gl.skill_points == 0x72C,
        "the skill points: sInfoMan +0x72C + the profile (+0x20) * 0xFDD8");
  check(gl.it_used == 4 && gl.it_slot == 5 && gl.it_id == 6 && gl.it_mode == 8 && gl.it_num == 0xA && gl.it_max == 0x10 &&
            gl.in_hand == 0x46D8,
        "an item: in use +4, slot +5, id +6, mode +8, counts +0xA, mosts +0x10; the slot in hand uPlayer +0x46D8");

  std::printf("-- the hooks\n");
  config::g_settings = config::Settings{};
  cheats::init();
  // What install writes into the game's data: a copy of .rdata and .data to compare with after.
  const mem::Range rd = image::rdata(), dt = image::data();
  const std::vector<uint8_t> rdata_was(reinterpret_cast<const uint8_t*>(rd.begin), reinterpret_cast<const uint8_t*>(rd.end));
  const std::vector<uint8_t> data_was(reinterpret_cast<const uint8_t*>(dt.begin), reinterpret_cast<const uint8_t*>(dt.end));
  check(cheats::install(), "cheats::install");
  // Every vtable hook the install puts in: the players' and the enemies' damage, the players' health add, reset and
  // combat gauge add, the enemies' start and destructor.
  const std::vector<const std::vector<vthook::Hooked>*> all_hooks = {
      &cheats::g_sub,   &cheats::g_hit,   &cheats::g_apply,  &cheats::g_eapply, &cheats::g_ework,
      &cheats::g_add,   &cheats::g_reset, &cheats::g_airadd, &cheats::g_spend,  &cheats::g_ride_sub,
      &merc::g_starts,  &merc::g_dtors};
  {
    std::vector<uintptr_t> hooked;
    for (const auto* list : all_hooks)
      for (const auto& h : *list) hooked.push_back(h.entry);
    std::sort(hooked.begin(), hooked.end());
    int stray = 0, slots = 0;
    auto compare = [&](const mem::Range& r, const std::vector<uint8_t>& was) {
      for (uintptr_t a = r.begin; a + 4 <= r.end; a += 4) {
        if (!std::memcmp(reinterpret_cast<const void*>(a), was.data() + (a - r.begin), 4)) continue;
        if (std::binary_search(hooked.begin(), hooked.end(), a)) {
          ++slots;
        } else if (++stray <= 8) {
          std::printf("      written outside a hooked vtable slot: %08X (was %08X)\n", static_cast<unsigned>(a),
                      *reinterpret_cast<const uint32_t*>(was.data() + (a - r.begin)));
        }
      }
    };
    compare(rd, rdata_was);
    compare(dt, data_was);
    char what[160];
    std::snprintf(what, sizeof(what),
                  "install writes nothing in the game's data but the vtable slots it hooks (%d slots, %d elsewhere)",
                  slots, stray);
    check(stray == 0 && slots == static_cast<int>(hooked.size()), what);
    // ... and each of those slots in a vtable that reaches it: every entry before it a function (the game's, or a hook).
    std::vector<uintptr_t> hooks;
    for (const auto* list : all_hooks)
      for (const auto& h : *list) hooks.push_back(h.hook);
    int short_tables = 0;
    for (const auto* list : all_hooks)
      for (const auto& h : *list) {
        bool all_code = true;
        for (uintptr_t e = h.vt; e < h.entry && all_code; e += 4) {
          const uintptr_t v = mem::read<uintptr_t>(e);
          all_code = image::text().contains(v) || std::find(hooks.begin(), hooks.end(), v) != hooks.end();
        }
        if (!all_code && ++short_tables <= 4)
          std::printf("      a slot written in a table that is no vtable that far: %08X (+%u)\n",
                      static_cast<unsigned>(h.vt), static_cast<unsigned>(h.entry - h.vt));
      }
    check(short_tables == 0, "every slot written lies in a vtable that reaches it (each entry before it a function)");
    // zlib's fixed Huffman table (inflate's lencode, 0x17014A0; its entry 279 is {0, 9, 209}) holds uEm6527's getDTI
    // address twice: the class index took it for two vtables, and the slot-126 hook wrote over entry 279 - every
    // archive inflated after came out broken and the game crashed loading (2026-11-06).
    check(mem::read<uint32_t>(0x17014A0) == 0x00000760 && mem::read<uint32_t>(0x17018FC) == 0x00D10900,
          "zlib's fixed Huffman table untouched (entry 279 still {0, 9, 209})");
    const std::vector<uintptr_t> v6527 = mt::vtables_of(mt::find("uEm6527"));
    check(!v6527.empty() && std::find(v6527.begin(), v6527.end(), 0x1701704u) == v6527.end() &&
              std::find(v6527.begin(), v6527.end(), 0x1701B04u) == v6527.end(),
          "uEm6527's vtables: its own, not the two places in zlib's table holding its getDTI's address");
    // A low slot's hook (a destructor's: slot 0) asks a table for its first five entries too, as the class index does:
    // zlib's table's first entry (0x690800) lies inside the code, its second (0x290800) not.
    check(image::text().contains(mem::read<uint32_t>(0x1701704)) && !vthook::reaches(0x1701704u, 0) &&
              !vthook::reaches(0x1701B04u, 0) && !v6527.empty() && vthook::reaches(v6527[0], 0),
          "a slot-0 hook refuses zlib's table (its first entry inside the code, not the four after it)");
    check(cheats::g_ework.size() == cheats::g_eapply.size(),
          "One hit kills' slot 126 in exactly the enemy vtables its take-damage (slot 128) went into");
  }
  for (int k = 0; k < sw::kCount; ++k) {
    char what[96];
    std::snprintf(what, sizeof(what), "switch %d available", k);
    check(cheats::available(static_cast<sw::Kind>(k)), what);
  }
  std::printf("      hooked: %d subtract-health, %d hit, %d take-damage (players); %d take-damage (enemies)\n",
              static_cast<int>(cheats::g_sub.size()), static_cast<int>(cheats::g_hit.size()),
              static_cast<int>(cheats::g_apply.size()), static_cast<int>(cheats::g_eapply.size()));
  std::printf("      hooked: %d health add, %d reset, %d gauge add, %d gauge spend (players); %d subtract-health "
              "(rides); %d start, %d destructor (enemies)\n",
              static_cast<int>(cheats::g_add.size()), static_cast<int>(cheats::g_reset.size()),
              static_cast<int>(cheats::g_airadd.size()), static_cast<int>(cheats::g_spend.size()),
              static_cast<int>(cheats::g_ride_sub.size()), static_cast<int>(merc::g_starts.size()),
              static_cast<int>(merc::g_dtors.size()));
  check(cheats::g_sub.size() >= 50 && cheats::g_apply.size() >= 50 && cheats::g_hit.size() >= 48,
        "every player class's vtable (some fifty)");
  check(cheats::g_eapply.size() >= 185, "every enemy class's vtable (some 190), the overrides that call it too");
  bool override_seen = false;
  for (const auto& h : cheats::g_eapply) override_seen = override_seen || h.orig != cheats::g_eapply_fn;
  check(override_seen, "an enemy class's own take-damage (one that calls the game's first) hooked as well");

  std::printf("-- God mode and the partner's\n");
  Obj leon(0x5300, vt_of("uPl00LeonUsa"));
  leon.field<int16_t>(gl.hp) = 1200;
  leon.field<int16_t>(gl.hp_max) = 1200;
  leon.field<uint32_t>(gl.control) = 0;
  call_sub(leon, 300);
  check(leon.field<int16_t>(gl.hp) == 900, "switches off: the game's own subtract-health takes 300 of 1200");
  cheats::set(sw::kGodMode, true);
  call_sub(leon, 300);
  check(leon.field<int16_t>(gl.hp) == 900, "God mode: nothing taken from you");
  call_sub(leon, -100);
  check(leon.field<int16_t>(gl.hp) == 1000, "God mode: health given back still goes on (a negative amount)");
  leon.field<uint32_t>(gl.control) = 1;
  call_sub(leon, 300);
  check(leon.field<int16_t>(gl.hp) == 700, "God mode alone: the computer's character still takes damage");
  cheats::set(sw::kPartnerGod, true);
  call_sub(leon, 300);
  check(leon.field<int16_t>(gl.hp) == 700, "Partner takes no damage: nothing taken from the computer's");
  leon.field<uint32_t>(gl.control) = 2;
  call_sub(leon, 300);
  check(leon.field<int16_t>(gl.hp) == 400, "both on: a character driven over the network still takes damage");
  leon.field<uint32_t>(gl.control) = 0;
  {
    // The health add (a heal, getting up from dying): uPlayer's own, through the mod's hook - the game's stood in.
    check(cheats::g_add_slot == 43 && cheats::g_add_fn == 0xA63AE0 && cheats::g_add.size() >= 50 &&
              mem::read<uintptr_t>(leon.field<uintptr_t>(0) + 4 * 43) ==
                  reinterpret_cast<uintptr_t>(&cheats::hk_player_add),
          "the health add (uPlayer's own, slot 43, 0xA63AE0) hooked in every player vtable");
    auto add_hp = [&](int amount) {
      through_hook(cheats::g_add, leon.mem.data(), reinterpret_cast<uintptr_t>(&stand_in_add),
                   [&] { cheats::hk_player_add(leon.mem.data(), amount); });
    };
    add_hp(-150);
    check(leon.field<int16_t>(gl.hp) == 400, "God mode: an add that takes health (-150) takes none");
    add_hp(150);
    check(leon.field<int16_t>(gl.hp) == 1200, "God mode: any add (a heal, getting up) fills you up (1200)");
    cheats::set(sw::kGodMode, false);
    add_hp(-150);
    check(leon.field<int16_t>(gl.hp) == 1050, "off: the game's own add (-150: 1050)");
    cheats::set(sw::kGodMode, true);
    leon.field<int16_t>(gl.hp) = 400;
  }

  // The hit and the take-damage: stand-ins for the game's (which reach far into the running game).
  const uintptr_t leon_vt = leon.field<uintptr_t>(0);
  cheats::g_hit.insert(cheats::g_hit.begin(), cheats::Hooked{leon_vt, 0, reinterpret_cast<uintptr_t>(&stand_in_hit), 0});
  cheats::g_apply.insert(cheats::g_apply.begin(),
                         cheats::Hooked{leon_vt, 0, reinterpret_cast<uintptr_t>(&stand_in_apply), 0});
  Obj hit(0x140, 0);
  hit.field<int32_t>(gl.hit_damage) = 450;
  using HitFn = void(__thiscall*)(void*, void*);
  using ApplyFn = char(__thiscall*)(void*, int, int, int, int, int, int);
  const auto hit_slot = reinterpret_cast<HitFn>(mem::read<uintptr_t>(leon_vt + 4 * game::kPlayerHitSlot));
  const auto apply_slot = reinterpret_cast<ApplyFn>(mem::read<uintptr_t>(leon_vt + 4 * game::kPlayerApplySlot));
  check(reinterpret_cast<uintptr_t>(hit_slot) == reinterpret_cast<uintptr_t>(&cheats::hk_player_hit) &&
            reinterpret_cast<uintptr_t>(apply_slot) == reinterpret_cast<uintptr_t>(&cheats::hk_player_apply),
        "a costume's vtable holds the mod's hit and take-damage");
  g_calls = 0;
  hit_slot(leon.mem.data(), hit.mem.data());
  check(g_calls == 1 && g_seen_damage == 0 && hit.field<int32_t>(gl.hit_damage) == 450,
        "God mode: the hit reaches the game with no damage, and its damage is put back after");
  g_calls = 0;
  const char applied = apply_slot(leon.mem.data(), 450, 8, 0, 0, 0, 0);
  check(g_calls == 0 && applied == 0, "God mode: your take-damage does nothing (not down)");
  cheats::set(sw::kGodMode, false);
  g_calls = 0;
  hit_slot(leon.mem.data(), hit.mem.data());
  check(g_calls == 1 && g_seen_damage == 450, "God mode off: the hit reaches the game with its damage");
  g_calls = 0;
  check(apply_slot(leon.mem.data(), 450, 0, 0, 0, 0, 0) == 1 && g_calls == 1, "God mode off: your take-damage is the game's");
  cheats::set(sw::kPartnerGod, false);

  std::printf("-- One hit kills\n");
  Obj zombie(0x4600, vt_of("uEm1000"));
  zombie.field<int16_t>(gl.hp) = 800;
  const uintptr_t zvt = zombie.field<uintptr_t>(0);
  using EnemyFn = void(__thiscall*)(void*, void*, int);
  const auto eslot = reinterpret_cast<EnemyFn>(mem::read<uintptr_t>(zvt + 4 * game::kEnemyApplySlot));
  check(reinterpret_cast<uintptr_t>(eslot) == reinterpret_cast<uintptr_t>(&cheats::hk_enemy_apply),
        "an enemy class's vtable holds the mod's take-damage");
  cheats::g_eapply.insert(cheats::g_eapply.begin(),
                          cheats::Hooked{zvt, 0, reinterpret_cast<uintptr_t>(&stand_in_enemy), 0});
  hit.field<uintptr_t>(gl.hit_attacker) = leon.at();
  eslot(zombie.mem.data(), hit.mem.data(), 100);
  check(g_seen_amount == 100, "off: a player's hit takes its own damage");
  cheats::set(sw::kOneHitKills, true);
  eslot(zombie.mem.data(), hit.mem.data(), 100);
  check(g_seen_amount == 800, "on: a player's hit with damage takes all 800");
  eslot(zombie.mem.data(), hit.mem.data(), 0);
  check(g_seen_amount == 0, "on: a hit with no damage stays one");
  Obj other(0x4600, vt_of("uEm1000"));
  hit.field<uintptr_t>(gl.hit_attacker) = other.at();
  eslot(zombie.mem.data(), hit.mem.data(), 100);
  check(g_seen_amount == 100, "on: an enemy's hit on another takes its own damage");
  hit.field<uintptr_t>(gl.hit_attacker) = 0;
  eslot(zombie.mem.data(), hit.mem.data(), 100);
  check(g_seen_amount == 100, "on: a hit nobody dealt takes its own damage");
  cheats::set(sw::kOneHitKills, false);

  std::printf("-- One hit kills on an enemy that keeps its life (Deborah's cores, a boss's parts)\n");
  {
    const cheats::Keep& k = cheats::g_keep;
    check(cheats::g_big_ok && k.keep == 0x3B33 && k.no_dead == 0x3B32 && k.parts == 0x3BE0 && k.broken == 0x3CE0,
          "the take-damage's keep-alive flags +0x3B33 and mIsNoDead +0x3B32; the parts +0x3BE0, broken +0x3CE0");
    check(k.no_life_fn == 0x761FC0 && k.flags == 0x2A08 && k.no_life_bit == 2,
          "Deborah's damage worked out (uEm4500's slot 126, 0x761FC0) hands on no health under bit 1 of +0x2A08");
    check(cheats::g_ework.size() >= 190 && cheats::g_ework_fn == 0x95D290,
          "the damage worked out (slot 126) hooked in every enemy vtable (some 190), each class's own");
    const uintptr_t dvt = vt_of("uEm4500");
    check(mem::read<uintptr_t>(dvt + 4 * game::kEnemyWorkSlot) == reinterpret_cast<uintptr_t>(&cheats::hk_enemy_work),
          "uEm4500's vtable holds the mod's damage worked out");

    // The game's globals Deborah's damage code and the take-damage read, made up in the copy: sGameMan (the mode
    // +0xAC: a campaign; online +0x878: no), sBioMain (+0x1045C), sPlayer (each player's damage dealt from +0x4140:
    // the `this` of the game's tally) and the system whose slot 25 answers whether an enemy may die (yes).
    struct Die {
      static bool __thiscall may(void*, void*) { return true; }
    };
    static void* die_vt[32] = {};
    die_vt[25] = reinterpret_cast<void*>(&Die::may);
    static void* die_obj = die_vt;
    std::vector<uint8_t> gameman(0x1000, 0), mainm(0x10500, 0), splay(0x4200, 0);
    const uint32_t kGameMan = 0x17CF454, kMainG = 0x186E1B4, kPlayerG = 0x17D0468, kDieG = 0x17CF40C;
    const uint32_t gm_was = mem::read<uint32_t>(kGameMan), main_was = mem::read<uint32_t>(kMainG),
                   pl_was = mem::read<uint32_t>(kPlayerG), die_was = mem::read<uint32_t>(kDieG);
    mem::write<uintptr_t>(kGameMan, reinterpret_cast<uintptr_t>(gameman.data()));
    mem::write<uintptr_t>(kMainG, reinterpret_cast<uintptr_t>(mainm.data()));
    mem::write<uintptr_t>(kPlayerG, reinterpret_cast<uintptr_t>(splay.data()));
    mem::write<uintptr_t>(kDieG, reinterpret_cast<uintptr_t>(&die_obj));
    // Where a hit landed on her (her slot 61, from her collision): a stand-in answering it.
    static int region = 0x14;
    struct Region {
      static int __thiscall of(void*, int) { return region; }
    };
    const uintptr_t slot61 = dvt + 4 * 61, slot61_was = mem::read<uintptr_t>(slot61);
    mem::write<uintptr_t>(slot61, reinterpret_cast<uintptr_t>(&Region::of));

    // Deborah, her parameter set as her file has it (the cores' DAMAGE_RATE_WEAK +0xB0 1.0, the last fight's +0xC8
    // 2.0; none of her own from a file, +0x3D80) and her three cores (parts 0-2).
    std::vector<uint8_t> param(0x140, 0);
    *reinterpret_cast<float*>(param.data() + 0xB0) = 1.0f;
    *reinterpret_cast<float*>(param.data() + 0xC8) = 2.0f;
    Obj deb(mt::find("uEm4500")->size, dvt);
    deb.field<uintptr_t>(0x3D24) = reinterpret_cast<uintptr_t>(param.data());
    auto cores = [&](int life) {
      for (int i = 0; i < 3; ++i) deb.field<int32_t>(k.parts + 8 * i) = deb.field<int32_t>(k.parts + 8 * i + 4) = life;
      deb.field<uint32_t>(k.broken) = 0;
      deb.field<uint32_t>(k.flags) = 0;
    };
    auto core = [&](int i) { return deb.field<int32_t>(k.parts + 8 * i); };
    Obj copy(0x140, 0);  // the damage handler's copy of a hit
    copy.field<int32_t>(gl.hit_damage) = 150;
    copy.field<uintptr_t>(gl.hit_attacker) = leon.at();
    leon.field<uint8_t>(0x4E88) = 0;  // the player's number (the tally's)
    using WorkFn = int(__thiscall*)(void*, void*);
    const auto work = reinterpret_cast<WorkFn>(mem::read<uintptr_t>(dvt + 4 * game::kEnemyWorkSlot));
    const uint32_t kThink = 0x3A88, kActStatus = 0x3A90;  // mThinkAttr (the fight), mActStatus

    // The fight with her cores (mThinkAttr bit 2; the stage sets her mIsNoDead), through the game's own code.
    deb.field<uint32_t>(kThink) = 0x9004;
    deb.field<uint8_t>(k.no_dead) = 1;
    cores(600);
    int h = work(deb.mem.data(), copy.mem.data());
    check(h == 150 && core(0) == 450 && !(deb.field<uint32_t>(k.broken) & 1),
          "off: a hit on her front core takes 150 of its 600 (and hands on 150 health)");
    cheats::set(sw::kOneHitKills, true);
    h = work(deb.mem.data(), copy.mem.data());
    check(core(0) == 0 && (deb.field<uint32_t>(k.broken) & 1) && (deb.field<uint32_t>(k.flags) & 8),
          "on: the front core broken at one hit, and her own code marks it (+0x2A08 bit 3)");
    check(h == cheats::kBigHit && copy.field<int32_t>(gl.hit_damage) == 150,
          "on: the health handed on is a big hit's (she keeps her point: mIsNoDead), the hit's damage put back");
    region = 0x15;
    work(deb.mem.data(), copy.mem.data());
    region = 0x16;
    work(deb.mem.data(), copy.mem.data());
    check(core(1) == 0 && core(2) == 0 && (deb.field<uint32_t>(k.broken) & 7) == 7 &&
              (deb.field<uint32_t>(k.flags) & 0x38) == 0x38,
          "on: the left and right cores the same (bits 4 and 5)");
    region = 0x14;
    cores(600);
    deb.field<uint32_t>(kActStatus) = 0x1000;
    work(deb.mem.data(), copy.mem.data());
    check(core(0) == 1 && !(deb.field<uint32_t>(k.broken) & 1),
          "on: while her own code holds a core (mActStatus bit 12) it is kept at 1, as the game keeps it");
    deb.field<uint32_t>(kActStatus) = 0;
    cores(600);
    deb.field<uint8_t>(k.no_dead) = 0;
    h = work(deb.mem.data(), copy.mem.data());
    check(h == 150 && core(0) == 450, "on: an enemy that can die - the hit as it is (its health ends it)");
    deb.field<uint8_t>(k.no_dead) = 1;
    cores(600);
    deb.field<uint8_t>(k.keep) = 1;
    deb.field<uint8_t>(k.no_dead) = 0;
    work(deb.mem.data(), copy.mem.data());
    check(core(0) == 0, "on: the take-damage's other keep-alive flag (+0x3B33) the same");
    deb.field<uint8_t>(k.keep) = 0;
    deb.field<uint8_t>(k.no_dead) = 1;

    // The mine cart: mThinkAttr bit 0, not mIsNoDead - her health held by her own code (bit 1 of +0x2A08).
    deb.field<uint8_t>(k.no_dead) = 0;
    deb.field<uint32_t>(kThink) = 0x8001;
    cores(250);
    deb.field<uint32_t>(k.flags) = 2;
    h = work(deb.mem.data(), copy.mem.data());
    check(h == 0 && core(0) == 0 && (deb.field<uint32_t>(k.broken) & 1),
          "the mine cart (her health held, +0x2A08 bit 1): a 250 core broken at one hit, no health handed on");
    cores(250);
    h = work(deb.mem.data(), copy.mem.data());
    check(h == 150 && core(0) == 100, "the mine cart, her health not held (and she may die): the hit as it is");
    // The last fight (mThinkAttr bit 1): a hit where her cores were (0x17) goes to the front core as it is.
    deb.field<uint32_t>(kThink) = 0x8002;
    deb.field<uint8_t>(k.no_dead) = 1;
    cores(600);
    region = 0x17;
    h = work(deb.mem.data(), copy.mem.data());
    check(core(0) == 0 && (deb.field<uint32_t>(k.broken) & 1) && h == 2 * cheats::kBigHit,
          "the last fight: the front core broken at one hit (its health handed on at its rate, 2.0)");
    region = 0x14;
    // Not a player's hit, or no damage: as it is.
    deb.field<uint32_t>(kThink) = 0x9004;
    cores(600);
    Obj em_other(0x4600, vt_of("uEm1000"));
    copy.field<uintptr_t>(gl.hit_attacker) = em_other.at();
    work(deb.mem.data(), copy.mem.data());
    check(core(0) == 450, "on: an enemy's hit on her - as it is");
    copy.field<uintptr_t>(gl.hit_attacker) = leon.at();
    cheats::set(sw::kOneHitKills, false);
    cores(600);
    work(deb.mem.data(), copy.mem.data());
    check(core(0) == 450, "off again: as it is");

    // The parts a boss's take-damage works a hit into first (slot 129: Ogroman's, Haos', a J'avo's ...): the
    // game's own take-damage (uEnemy's) on an uEm4800, a stand-in in its slot 129 seeing the hit.
    static int seen_parts = -1;
    struct Parts {
      static void __thiscall in(void*, void* hit) {
        seen_parts = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(hit) + 4);
      }
    };
    const mt::Class* ogc = mt::find("uEm4800");
    const uintptr_t ovt = ogc->vtable, slot129 = ovt + 4 * 129, slot129_was = mem::read<uintptr_t>(slot129);
    mem::write<uintptr_t>(slot129, reinterpret_cast<uintptr_t>(&Parts::in));
    Obj og(ogc->size, ovt);
    auto life = [&](int hp) { og.field<int16_t>(gl.hp) = og.field<int16_t>(gl.hp_max) = static_cast<int16_t>(hp); };
    const auto oslot = reinterpret_cast<EnemyFn>(mem::read<uintptr_t>(ovt + 4 * game::kEnemyApplySlot));
    check(reinterpret_cast<uintptr_t>(oslot) == reinterpret_cast<uintptr_t>(&cheats::hk_enemy_apply),
          "uEm4800's take-damage is the mod's (the game's own behind it)");
    Obj ohit(0x140, 0);
    ohit.field<int32_t>(gl.hit_damage) = 120;
    ohit.field<uintptr_t>(gl.hit_attacker) = leon.at();
    life(5000);
    og.field<uint8_t>(k.no_dead) = 1;
    oslot(og.mem.data(), ohit.mem.data(), 120);
    check(seen_parts == 120 && og.field<int16_t>(gl.hp) == 4880, "off: its parts see 120, its health takes 120");
    cheats::set(sw::kOneHitKills, true);
    life(5000);
    oslot(og.mem.data(), ohit.mem.data(), 120);
    check(seen_parts == cheats::kBigHit && og.field<int16_t>(gl.hp) == 1 && !og.field<uint8_t>(0xF24) &&
              ohit.field<int32_t>(gl.hit_damage) == 120,
          "on, mIsNoDead: its parts see a big hit, it keeps its last point (the game's own), the hit put back");
    og.field<uint8_t>(k.no_dead) = 0;
    life(5000);
    oslot(og.mem.data(), ohit.mem.data(), 120);
    check(seen_parts == 120 && og.field<int16_t>(gl.hp) == 0 && og.field<uint8_t>(0xF24) == 1,
          "on, one that can die: its parts see the hit as it is, the health ends it (marked dead)");
    cheats::set(sw::kOneHitKills, false);
    mem::write<uintptr_t>(slot129, slot129_was);

    // Brzak (uEm4200, the water's "Jaws"): its own damage worked out runs uEnemy's on the hit (its stagger: gauge 0,
    // 1000, at its tongue's rate - a made-up resource +0x3D80 holding a cEm4200Param with its file's rates,
    // RESIST_WT_BODY 0, RESIST_WT_TONGUE 1.0) and hands on 1 whatever the hit.
    check(k.hit_count_fn == 0x7285E0,
          "Brzak's damage worked out (uEm4200's slot 126, 0x7285E0) hands on 1 whatever the hit: its life counts hits");
    const mt::Class* jc = mt::find("uEm4200");
    const uintptr_t jvt = jc->vtable, jslot61 = jvt + 4 * 61, jslot61_was = mem::read<uintptr_t>(jslot61);
    mem::write<uintptr_t>(jslot61, reinterpret_cast<uintptr_t>(&Region::of));
    Obj jparam(mt::find("cEm4200Param")->size, vt_of("cEm4200Param"));
    jparam.field<float>(0x20C) = 0.0f;
    jparam.field<float>(0x210) = jparam.field<float>(0x214) = 1.0f;
    std::vector<uint8_t> jres(0x80, 0);
    *reinterpret_cast<uintptr_t*>(jres.data() + 0x74) = jparam.at();
    Obj jaws(jc->size, jvt);
    jaws.field<uintptr_t>(0x3D80) = reinterpret_cast<uintptr_t>(jres.data());
    jaws.field<uint32_t>(kThink) = 4;  // in the water
    auto stagger = [&] {
      jaws.field<uint8_t>(0x3B9C) = jaws.field<uint8_t>(0x3B9E) = 1;
      jaws.field<float>(0x3B84) = jaws.field<float>(0x3B8C) = 1000.0f;
    };
    const auto jwork = reinterpret_cast<WorkFn>(mem::read<uintptr_t>(jvt + 4 * game::kEnemyWorkSlot));
    check(reinterpret_cast<uintptr_t>(jwork) == reinterpret_cast<uintptr_t>(&cheats::hk_enemy_work),
          "uEm4200's vtable holds the mod's damage worked out");
    region = 10;  // its tongue
    stagger();
    h = jwork(jaws.mem.data(), copy.mem.data());
    check(h == 1 && jaws.field<float>(0x3B84) == 850.0f && jaws.field<uint8_t>(0x3B9E) == 1,
          "off: a hit on its tongue takes 150 of its stagger's 1000 (and 1 of its life)");
    cheats::set(sw::kOneHitKills, true);
    stagger();
    h = jwork(jaws.mem.data(), copy.mem.data());
    check(h == 1 && jaws.field<float>(0x3B84) <= 0.0f && jaws.field<uint8_t>(0x3B9E) == 0 &&
              copy.field<int32_t>(gl.hit_damage) == 150,
          "on: its stagger emptied at one tongue hit (its own reaction follows), 1 of its life, the hit put back");
    region = 0x14;  // its body
    stagger();
    jwork(jaws.mem.data(), copy.mem.data());
    check(jaws.field<float>(0x3B84) == 1000.0f && jaws.field<uint8_t>(0x3B9E) == 1,
          "on: a hit on its body does nothing to its stagger, as the game has it (its rate 0)");
    region = 0x14;
    // Its take-damage: to its last point, never 0 - the stage's script ends it.
    jaws.field<int16_t>(gl.hp) = jaws.field<int16_t>(gl.hp_max) = 32700;
    const auto jslot = reinterpret_cast<EnemyFn>(mem::read<uintptr_t>(jvt + 4 * game::kEnemyApplySlot));
    Obj jhit(0x140, 0);
    jhit.field<int32_t>(gl.hit_damage) = 150;
    jhit.field<uintptr_t>(gl.hit_attacker) = leon.at();
    jslot(jaws.mem.data(), jhit.mem.data(), 1);
    check(jaws.field<int16_t>(gl.hp) == 1 && !jaws.field<uint8_t>(0xF24) && jhit.field<int32_t>(gl.hit_damage) == 150,
          "on: its take-damage takes it to its last point - never 0, never marked dead (the script ends it)");
    jslot(jaws.mem.data(), jhit.mem.data(), 1);
    check(jaws.field<int16_t>(gl.hp) == 1 && !jaws.field<uint8_t>(0xF24), "on: and stays at its last point");
    cheats::set(sw::kOneHitKills, false);
    mem::write<uintptr_t>(jslot61, jslot61_was);

    // The other bosses whose own code keeps their life: each through the game's own take-damage (uEnemy's), a
    // stand-in in its slot 129 seeing the hit.
    check(k.floor_fn == 0x772270, "Rasklapanje's subtract-health (uEm4700Base's slot 44, 0x772270): never below 1");
    check(k.intel == 0x3A54 && k.death_reaction == 0xA0 && k.no_reaction == 0xA242B0,
          "an enemy's brain (mpIntel +0x3A54), its death reaction (vtable +0xA0), uEmIntel's none (0xA242B0)");
    check(k.haos_fn == 0x8DDE80 && k.haos_mode == 0x41C4, "Haos' damage worked out (uEm8000's slot 126, 0x8DDE80), its mode +0x41C4");
    check(k.hold_fn == 0x906790 && k.hold_stage == 0x1FC && k.hold_scene == 0x96 && k.hold_stage_at == 0x1B0C &&
              k.hold_scene_at == 0x1B14,
          "the choppers' damage worked out (uEm9900's slot 126, 0x906790): a point kept in stage 508 until scene 150");
    struct Boss {
      Obj obj;
      uintptr_t slot129 = 0, was129 = 0;
      EnemyFn take = nullptr;
      explicit Boss(const char* cls) : obj(mt::find(cls)->size, vt_of(cls)) {
        const uintptr_t vt = vt_of(cls);
        slot129 = vt + 4 * 129;
        was129 = mem::read<uintptr_t>(slot129);
        mem::write<uintptr_t>(slot129, reinterpret_cast<uintptr_t>(&Parts::in));
        take = reinterpret_cast<EnemyFn>(mem::read<uintptr_t>(vt + 4 * game::kEnemyApplySlot));
      }
      ~Boss() { mem::write<uintptr_t>(slot129, was129); }
      void life(int hp) {
        obj.field<int16_t>(0xF10) = obj.field<int16_t>(0xF12) = static_cast<int16_t>(hp);
        obj.field<uint8_t>(0xF24) = 0;
      }
      void hit(Obj& h, int amount) {
        seen_parts = -1;
        take(obj.mem.data(), h.mem.data(), amount);
      }
      int16_t hp() { return obj.field<int16_t>(0xF10); }
      bool dead() { return obj.field<uint8_t>(0xF24) != 0; }
    };
    cheats::set(sw::kOneHitKills, true);
    {
      Boss ras("uEm4700");
      check(reinterpret_cast<uintptr_t>(ras.take) == reinterpret_cast<uintptr_t>(&cheats::hk_enemy_apply),
            "uEm4700's take-damage is the mod's (the game's own behind it)");
      ras.life(3000);
      ras.hit(ohit, 120);
      check(seen_parts == cheats::kBigHit && ras.hp() == 1 && !ras.dead() && ohit.field<int32_t>(gl.hit_damage) == 120,
            "Rasklapanje: its parts (its limbs, its halves) see a big hit; its own floor keeps 1; the hit put back");
    }
    {
      Boss ust("uEm7000");
      Obj brain(0x500, vt_of("uEm7000Intel"));
      ust.obj.field<uintptr_t>(k.intel) = brain.at();
      ust.life(12000);
      ust.hit(ohit, 120);
      check(seen_parts == cheats::kBigHit && ust.hp() == 1 && !ust.dead(),
            "Ustanak (its brain has no death reaction): a big hit, taken to its last point, never marked dead");
      ust.hit(ohit, cheats::kBigHit);  // what its damage worked out hands on for a big hit
      check(ust.hp() == 1 && !ust.dead(), "Ustanak: a big amount handed on still stops at its last point");
      ust.obj.field<uintptr_t>(k.intel) = 0;
      ust.life(12000);
      ust.hit(ohit, 120);
      check(seen_parts == 120 && ust.hp() == 0 && ust.dead(), "an uEm7000 with no brain: as before, its health ends it");
    }
    {
      Boss haos("uEm8000");
      haos.obj.field<uint32_t>(k.haos_mode) = cheats::kHaosModePipes;
      haos.life(30000);
      haos.hit(ohit, 120);
      check(seen_parts == cheats::kBigHit && haos.hp() == 1 && !haos.dead(),
            "Haos on the pipes (mode 2, stage 903): a big hit, taken to its last point (the stage moves on through "
            "its hand)");
      haos.hit(ohit, 120);
      check(haos.hp() == 1 && !haos.dead(), "Haos on the pipes: and stays at its last point");
      // Stage 972's chase (mode 1): the stage waits for it gone or dead (EnemyCheck 0x2A), so it dies at one hit.
      haos.obj.field<uint32_t>(k.haos_mode) = 1;
      haos.life(30000);
      haos.hit(ohit, 120);
      check(seen_parts == 120 && haos.hp() == 0 && haos.dead(),
            "Haos in the chase (mode 1, stage 972): its health ends it at one hit (the stage waits for its death)");
      haos.obj.field<uint32_t>(k.haos_mode) = 0;  // the climb's (stage 972): mIsNoDead its placement's
      haos.obj.field<uint8_t>(k.no_dead) = 1;
      haos.life(30000);
      haos.hit(ohit, 120);
      check(seen_parts == cheats::kBigHit && haos.hp() == 1 && !haos.dead(),
            "Haos climbing (mode 0, mIsNoDead): the game's point kept, a big hit (its damage count passes at one hit)");
      haos.obj.field<uint8_t>(k.no_dead) = 0;
      haos.obj.field<uint32_t>(k.haos_mode) = 4;
      haos.life(1200);
      haos.hit(ohit, 120);
      check(seen_parts == 120 && haos.hp() == 0 && haos.dead(), "Haos in its last fight's mode 4: its health ends it");
    }
    {
      // The chopper: the profile's cGameInfo (a made-up sInfoMan) in stage 508, before and at scene 150.
      std::vector<uint8_t> sinfo(mt::find("sInfoMan")->size, 0);
      const uintptr_t sinfo_was = game::g_sinfo;
      game::g_sinfo = reinterpret_cast<uintptr_t>(sinfo.data());
      const uintptr_t gi = game::game_info_at();
      Boss heli("uEm9900");
      check(gi != 0, "the profile's cGameInfo found over a made-up sInfoMan");
      *reinterpret_cast<uint32_t*>(gi + k.hold_stage_at) = 0x1FC;
      *reinterpret_cast<uint32_t*>(gi + k.hold_scene_at) = 0x95;
      heli.life(1000);
      heli.hit(ohit, 5);
      check(heli.hp() == 1 && !heli.dead(), "the chopper in stage 508 before scene 150: kept to its last point");
      *reinterpret_cast<uint32_t*>(gi + k.hold_scene_at) = 0x96;
      heli.life(1000);
      heli.hit(ohit, 5);
      check(heli.hp() == 0 && heli.dead(), "the chopper from scene 150 on (and elsewhere): its health ends it");
      game::g_sinfo = sinfo_was;
    }
    {
      // An ordinary enemy with none of these: One hit kills as ever.
      Boss reg("uEm3000");
      reg.life(2000);
      reg.hit(ohit, 120);
      check(seen_parts == 120 && reg.hp() == 0 && reg.dead(), "an ordinary enemy (uEm3000): no big hit, its health ends it");
    }
    cheats::set(sw::kOneHitKills, false);

    mem::write<uintptr_t>(slot61, slot61_was);
    mem::write<uint32_t>(kGameMan, gm_was);
    mem::write<uint32_t>(kMainG, main_was);
    mem::write<uint32_t>(kPlayerG, pl_was);
    mem::write<uint32_t>(kDieG, die_was);
  }

  std::printf("-- Infinite ammo\n");
  game::Run run;
  run.panel = true;  // the panel able to show (as dispatch has it at the pause): its numbers read
  run.nchars = 2;
  run.screen = game::Screen::kPlay;
  Obj partner(0x5300, vt_of("uPl01HelenaUsa"));
  run.chars[0].obj = leon.at();
  run.chars[0].control = game::Control::kLocal;
  run.chars[1].obj = partner.at();
  run.chars[1].control = game::Control::kAi;
  const uint32_t bit = 1u << gl.ammo_bit;
  leon.field<uint32_t>(gl.ammo_flags) = 0x41;
  cheats::set(sw::kInfiniteAmmo, true);
  cheats::tick(run);
  check(leon.field<uint32_t>(gl.ammo_flags) == (0x41 | bit), "on: the bit set on yours, the word's others kept");
  check(partner.field<uint32_t>(gl.ammo_flags) == 0, "on: the computer's partner left alone (the game fills theirs)");
  {
    check(cheats::g_reset_slot == 109 && cheats::g_reset_fn == 0xA74B30 && cheats::g_reset.size() >= 50,
          "the players' reset (uPlayer's slot 109, 0xA74B30: a Continue, a stage's start) hooked in every player vtable");
    const bool reset = through_hook(cheats::g_reset, leon.mem.data(), reinterpret_cast<uintptr_t>(&stand_in_reset),
                                    [&] { cheats::hk_player_reset(leon.mem.data()); });
    check(reset && leon.field<uint32_t>(gl.ammo_flags) == bit,
          "on: the game's reset of you sets the flags to 0 - the bit put back after it");
    leon.field<uint32_t>(gl.ammo_flags) = 0x41 | bit;
    cheats::tick(run);
    check(leon.field<uint32_t>(gl.ammo_flags) == (0x41 | bit), "... and the tick writes nothing");
  }
  {
    // The tablets' uses: three calls of the use function, re-pointed at the mod while it is on.
    std::vector<uintptr_t> sites;
    for (const mem::Patch& p : cheats::g_tablet_calls) sites.push_back(p.at);
    std::sort(sites.begin(), sites.end());
    check(sites.size() == 3 && sites[0] == 0xACC532 && sites[1] == 0xDBBBD9 && sites[2] == 0xDC7073,
          "the health tablets' three uses found (a heal of yourself, of your partner, the ones a hit takes)");
    bool all_ours = true;
    for (const uintptr_t a : sites) all_ours = all_ours && game::call_target(a) == reinterpret_cast<uintptr_t>(&cheats::hk_use_tablets);
    check(all_ours, "on: each tablet use calls the mod's");
    const cheats::UseFn real = cheats::g_use;
    cheats::g_use = reinterpret_cast<cheats::UseFn>(&stand_in_use);
    g_calls = 0;
    cheats::hk_use_tablets(leon.mem.data(), 3, 1);
    check(g_calls == 1 && !(g_seen_flags & bit) && (leon.field<uint32_t>(gl.ammo_flags) & bit),
          "on: a tablet's use reaches the game with the bit let go (used up), the bit back after");
    cheats::g_use = real;
  }
  cheats::set(sw::kInfiniteAmmo, false);
  cheats::tick(run);
  check(leon.field<uint32_t>(gl.ammo_flags) == 0x41, "off: the bit the mod set cleared again");
  {
    bool all_game = true;
    for (const mem::Patch& p : cheats::g_tablet_calls) all_game = all_game && game::call_target(p.at) == game::sig_one("kSigAmmoUse");
    check(all_game, "off: each tablet use calls the game's again");
  }
  partner.field<uint32_t>(gl.ammo_flags) = bit;  // the game's own (a scene's turret)
  partner.field<uint32_t>(gl.control) = 0;
  run.chars[1].control = game::Control::kLocal;
  cheats::set(sw::kInfiniteAmmo, true);
  cheats::tick(run);
  cheats::set(sw::kInfiniteAmmo, false);
  cheats::tick(run);
  check(partner.field<uint32_t>(gl.ammo_flags) == bit, "off: a bit the game had set itself is kept");

  std::printf("-- Infinite ammo: the helicopter's missiles\n");
  {
    const cheats::Missiles& m = cheats::g_heli;
    const uintptr_t launch = game::sig_one("kSigMissileLaunch");
    const mt::Class* hc = mt::find("uSm5060");
    const mt::Class* jc = mt::find("uSm5020");
    const mt::Class* ac = mt::find("cSmPlHarrierAct_Missile");
    check(cheats::g_missiles_ok && launch == 0xB81440 && m.count == 0x41C0 && m.load == 0x1772008 &&
              mem::read<int32_t>(m.load) == 6 && m.cls == hc && cheats::g_missile[0].at == 0xB81475 &&
              cheats::g_missile[1].at == 0xB8147E,
          "Ada's helicopter (uSm5060): mMissileNum +0x41C0 (a copy a seat), 6 from its setup (0x1772008); the missile "
          "action's start (0xB81440) takes one at 0xB81475 and 0xB8147E");
    // The game's own launch (the action's start, through its vtable) on a made-up helicopter: the action's owner +8,
    // its seat +0x10; the seat's launch (1 a missile, 2 dry) where the launch writes it.
    Obj heli(hc->size, hc->vtable), jet(jc->size, jc->vtable), act(ac->size, ac->vtable);
    act.field<uintptr_t>(8) = heli.at();
    const uint32_t state = mem::read<uint32_t>(launch + 39);
    auto missiles = [&](Obj& o, int a, int b) {
      o.field<int32_t>(m.count) = a;
      o.field<int32_t>(m.count + 4) = b;
    };
    auto left = [&](Obj& o, int seat) { return o.field<int32_t>(m.count + 4 * seat); };
    auto fire = [&](int seat) {
      act.field<uint32_t>(0x10) = static_cast<uint32_t>(seat);
      heli.field<uint32_t>(state + 4 * seat) = 0;
      heli.field<float>(m.count + 8 + 4 * seat) = 0.0f;
      reinterpret_cast<void(__thiscall*)(void*)>(mem::read<uintptr_t>(ac->vtable + 4 * 6))(act.mem.data());
    };
    auto takes = [&](uint8_t first) {
      bool all = true;
      for (const mem::Patch& p : cheats::g_missile) all = all && mem::read<uint8_t>(p.at) == first;
      return all;
    };
    missiles(heli, 6, 6);
    fire(0);
    check(state == 0x4144 && left(heli, 0) == 5 && left(heli, 1) == 5 && heli.field<uint32_t>(state) == 1 &&
              heli.field<float>(m.count + 8) == 1.0f,
          "off: the game's own launch takes one off both copies (5, 5): a missile, the seat's wait a second");
    missiles(heli, 0, 0);
    fire(0);
    check(heli.field<uint32_t>(state) == 2 && left(heli, 0) == 0, "off: with none left the launch is dry");
    // In play on a line of a made-up sBioUnit: the helicopter (2 left) and Chris's jet (uSm5020: none from its setup).
    const mt::Class* uc = mt::find("sBioUnit");
    Obj sunit(uc->size, uc->vtable);
    sunit.field<uint32_t>(ul.lines_at) = 0x20;
    for (uint32_t i = 0; i < 0x20; ++i) sunit.field<uint32_t>(i * ul.stride + ul.line_flags) = 0;
    sunit.field<uintptr_t>(0x0A * ul.stride + ul.line_head) = heli.at();
    heli.field<uintptr_t>(ul.unit_next) = jet.at();
    heli.field<uint32_t>(ul.unit_flags) = jet.field<uint32_t>(ul.unit_flags) = 2;
    game::g_systems["sUnit"] = sunit.at();
    game::Run hr;  // in a game, no characters of sPlayer's
    hr.in_game = true;
    hr.screen = game::Screen::kPause;
    missiles(heli, 2, 2);
    missiles(jet, 0, 0);
    cheats::tick(hr);
    check(left(heli, 0) == 2 && takes(0x01), "off: the tick gives nothing back, the takes the game's");
    cheats::set(sw::kInfiniteAmmo, true);
    cheats::tick(hr);
    check(left(heli, 0) == 6 && left(heli, 1) == 6 && left(jet, 0) == 0 && left(jet, 1) == 0,
          "on: as the switch goes on the helicopter in play gets its load back (6, 6); Chris's jet none");
    check(takes(0x90), "on: both takes NOPs");
    fire(0);
    check(left(heli, 0) == 6 && left(heli, 1) == 6 && heli.field<uint32_t>(state) == 1 &&
              heli.field<float>(m.count + 8) == 1.0f,
          "on: the game's own launch fires a missile (its wait a second) and takes none (6, 6)");
    fire(1);
    check(left(heli, 0) == 6 && left(heli, 1) == 6 && heli.field<uint32_t>(state + 4) == 1,
          "on: the other seat's launch the same");
    missiles(heli, 3, 3);
    cheats::tick(hr);
    check(left(heli, 0) == 3, "on: given back only as the switch goes on, not by the tick after");
    cheats::set(sw::kInfiniteAmmo, false);
    cheats::tick(hr);
    check(takes(0x01), "off: the takes the game's again");
    fire(0);
    check(left(heli, 0) == 2 && left(heli, 1) == 2, "off: a launch takes one again (2, 2)");
    game::g_systems.erase("sUnit");
  }

  std::printf("-- God mode on a ride (the vehicle a spared character rides)\n");
  {
    const cheats::RideLayout& rl = cheats::g_ride;
    check(cheats::g_rides_ok && rl.rider == 0x30A4 && cheats::g_ride_sub_fn == 0xBB3DF0 &&
              cheats::g_ride_sub.size() >= 25 && cheats::g_cls_ride == mt::find("uSmRide"),
          "the rides' subtract-health (uSmRide's slot 44, 0xBB3DF0; its riders +0x30A4 and +0x30A8) hooked in every "
          "ride class's vtable, the players' bike's and jeep's own too");
    // The game's own subtract-health of each ride (through its vtable) over a made-up sPlayer at its global: each
    // player's level (+0x4120, 0: a player's ride takes half).
    std::vector<uint8_t> splay(0x4200, 0);
    const uint32_t kPlayerG = 0x17D0468, pl_was = mem::read<uint32_t>(kPlayerG);
    mem::write<uintptr_t>(kPlayerG, reinterpret_cast<uintptr_t>(splay.data()));
    auto make = [](const char* cls) { return Obj(mt::find(cls)->size, vt_of(cls)); };
    Obj heli = make("uSm5060"), bike = make("uSm5012"), ejeep = make("uSm5028v"), javo = make("uEm1000");
    auto ride = [&](Obj& o, uintptr_t first, uintptr_t second, int hp) {
      o.field<uintptr_t>(rl.rider) = first;
      o.field<uintptr_t>(rl.rider + 4) = second;
      o.field<int16_t>(gl.hp) = static_cast<int16_t>(hp);
      o.field<int16_t>(gl.hp_max) = 1500;
    };
    auto hit = [&](Obj& o, int amount) {
      using Fn = void(__thiscall*)(void*, int);
      reinterpret_cast<Fn>(mem::read<uintptr_t>(o.field<uintptr_t>(0) + 4 * game::kSubLifeSlot))(o.mem.data(), amount);
      return o.field<int16_t>(gl.hp);
    };
    const uint32_t leon_ctl = leon.field<uint32_t>(gl.control), partner_ctl = partner.field<uint32_t>(gl.control);
    leon.field<uint32_t>(gl.control) = 0;
    partner.field<uint32_t>(gl.control) = 1;
    ride(heli, leon.at(), 0, 1500);
    check(hit(heli, 300) == 1350, "off: a hit of 300 on the helicopter you fly takes 150 (the game halves it)");
    cheats::set(sw::kGodMode, true);
    check(hit(heli, 300) == 1350, "God mode: the helicopter you fly takes nothing");
    ride(heli, 0, leon.at(), 1350);
    check(hit(heli, 300) == 1350, "God mode: ... nor one you ride in its second seat");
    ride(heli, partner.at(), 0, 1350);
    check(hit(heli, 300) == 1200, "God mode alone: one the computer's partner rides takes its damage");
    cheats::set(sw::kPartnerGod, true);
    check(hit(heli, 300) == 1200, "Partner takes no damage: ... and with it, nothing");
    cheats::set(sw::kPartnerGod, false);
    leon.field<uint32_t>(gl.control) = 2;
    ride(heli, leon.at(), 0, 1500);
    check(hit(heli, 300) == 1350, "God mode: one a character played over the network rides takes its damage");
    leon.field<uint32_t>(gl.control) = 0;
    ride(bike, leon.at(), 0, 1500);
    check(hit(bike, 300) == 1500, "God mode: the bike you ride (its own subtract-health, then the rides') nothing");
    ride(ejeep, javo.at(), 0, 1500);
    check(hit(ejeep, 300) == 1200, "God mode: an enemy's jeep (a J'avo riding) takes its damage, all of it");
    check(hit(heli, -100) == 1400, "God mode: health given back still goes on (halved as the game has it: 1350 + 50)");
    cheats::set(sw::kGodMode, false);
    ride(bike, leon.at(), 0, 1500);
    check(hit(bike, 300) == 1350, "off: the bike you ride takes its damage again");
    // As the switch goes on: the rides in play (a made-up sBioUnit's line) with a spared rider filled up once.
    Obj wreck = make("uSm5060");
    ride(heli, leon.at(), 0, 900);
    ride(ejeep, javo.at(), 0, 500);
    ride(wreck, leon.at(), 0, 0);
    const mt::Class* uc = mt::find("sBioUnit");
    Obj sunit(uc->size, uc->vtable);
    sunit.field<uint32_t>(ul.lines_at) = 0x20;
    for (uint32_t i = 0; i < 0x20; ++i) sunit.field<uint32_t>(i * ul.stride + ul.line_flags) = 0;
    sunit.field<uintptr_t>(0x0A * ul.stride + ul.line_head) = heli.at();
    heli.field<uintptr_t>(ul.unit_next) = ejeep.at();
    ejeep.field<uintptr_t>(ul.unit_next) = wreck.at();
    heli.field<uint32_t>(ul.unit_flags) = ejeep.field<uint32_t>(ul.unit_flags) = wreck.field<uint32_t>(ul.unit_flags) = 2;
    game::g_systems["sUnit"] = sunit.at();
    game::Run hr;
    hr.in_game = true;
    hr.screen = game::Screen::kPause;
    cheats::tick(hr);
    check(heli.field<int16_t>(gl.hp) == 900, "off: the tick repairs nothing");
    cheats::set(sw::kGodMode, true);
    cheats::tick(hr);
    check(heli.field<int16_t>(gl.hp) == 1500 && ejeep.field<int16_t>(gl.hp) == 500 && wreck.field<int16_t>(gl.hp) == 0,
          "God mode going on: the helicopter you fly filled up (1500); an enemy's jeep, and a ride destroyed, left");
    heli.field<int16_t>(gl.hp) = 1000;
    cheats::tick(hr);
    check(heli.field<int16_t>(gl.hp) == 1000, "... once: the tick after holds nothing");
    cheats::set(sw::kGodMode, false);
    cheats::tick(hr);
    game::g_systems.erase("sUnit");
    mem::write<uint32_t>(kPlayerG, pl_was);
    leon.field<uint32_t>(gl.control) = leon_ctl;
    partner.field<uint32_t>(gl.control) = partner_ctl;
  }

  std::printf("-- Infinite skill points and the editor\n");
  const uintptr_t buy = game::sig_one("kSigSkillBuy") + 21, level = game::sig_one("kSigSkillLevel") + 9;
  const uint8_t buy_was = mem::read<uint8_t>(buy), level_was = mem::read<uint8_t>(level);
  cheats::set(sw::kInfiniteSkillPoints, true);
  cheats::tick(run);
  bool nops = true;
  for (int i = 0; i < 6; ++i) nops = nops && mem::read<uint8_t>(buy + i) == 0x90 && mem::read<uint8_t>(level + i) == 0x90;
  check(nops, "on: both purchases' subtractions are NOPs");
  cheats::set(sw::kInfiniteSkillPoints, false);
  cheats::tick(run);
  check(mem::read<uint8_t>(buy) == buy_was && buy_was == 0x29 && mem::read<uint8_t>(level) == level_was &&
            level_was == 0x29,
        "off: the game's subtractions back");

  // A made-up sInfoMan with profile 1 in use, and an sPlayer with the two characters.
  Obj info(mt::find("sInfoMan")->size, vt_of("sInfoMan"));
  info.field<uint32_t>(gl.profile) = 1;
  info.field<int32_t>(gl.skill_points + gl.profile_size) = 12345;
  game::g_sinfo = info.at();
  game::g_systems_seen = true;
  cheats::tick(run);
  check(cheats::status().points && cheats::status().skill_points == 12345, "the panel's skill points: profile 1's");
  cheats::edit(cheats::Edit::kSkillPoints, 0, 500000);
  cheats::tick(run);
  check(info.field<int32_t>(gl.skill_points + gl.profile_size) == 12345, "in play: the edit is not made");
  cheats::edit(cheats::Edit::kSkillPoints, 0, 500000);
  run.screen = game::Screen::kPause;
  cheats::tick(run);
  check(info.field<int32_t>(gl.skill_points + gl.profile_size) == 500000 && info.field<int32_t>(gl.skill_points) == 0,
        "at the pause: the profile in use gets the points typed (the other profile's left alone)");

  std::printf("-- the tick\n");
  Obj main_obj(main_c->size, main_c->vtable);
  Obj splayer(mt::find("sPlayer")->size, vt_of("sPlayer"));
  leon.field<uint32_t>(ul.unit_flags) = 2;
  partner.field<uint32_t>(ul.unit_flags) = 1;
  leon.field<int16_t>(gl.hp) = 1200;
  partner.field<int16_t>(gl.hp) = 600;
  partner.field<int16_t>(gl.hp_max) = 1200;
  partner.field<uint32_t>(gl.control) = 1;
  splayer.field<uintptr_t>(gl.players + 0) = leon.at();
  splayer.field<uintptr_t>(gl.players + 4) = partner.at();
  Obj gone(0x5300, vt_of("uPl02ChrisEurope"));
  gone.field<uint32_t>(ul.unit_flags) = 3;  // a unit on its way out
  splayer.field<uintptr_t>(gl.players + 8) = gone.at();
  game::g_splayer = splayer.at();
  game::Run r;
  game_tick(main_obj.mem.data(), &r);
  check(r.in_game && r.nchars == 2 && r.screen == game::Screen::kPlay, "in play: two characters (not the one going)");
  check(r.chars[0].control == game::Control::kLocal && !std::strcmp(r.chars[0].name, "Leon") && r.chars[0].hp == 1200 &&
            r.chars[1].control == game::Control::kAi && !std::strcmp(r.chars[1].name, "Helena") && r.chars[1].hp == 600,
        "Leon (you, 1200) and Helena (the computer, 600)");
  main_obj.field<uint8_t>(gl.paused) = 1;
  game_tick(main_obj.mem.data(), &r);
  check(r.paused && r.screen == game::Screen::kPause, "sBioMain's byte set: the pause");
  cheats::set(sw::kPartnerGod, true);
  cheats::tick(r);
  check(partner.field<int16_t>(gl.hp) == 1200, "Partner takes no damage: the computer's health held at its most");
  partner.field<int16_t>(gl.hp) = 1;
  game_tick(main_obj.mem.data(), &r);
  cheats::tick(r);
  check(partner.field<int16_t>(gl.hp) == 1, "... but one the game has put down (1 point) is left to it");
  partner.field<int16_t>(gl.hp) = 900;
  game_tick(main_obj.mem.data(), &r);
  cheats::tick(r);
  check(partner.field<int16_t>(gl.hp) == 900,
        "... and the tick holds nothing: health set by the game's own code (900) is left (the hooks keep damage off)");
  cheats::set(sw::kPartnerGod, false);
  cheats::tick(r);
  cheats::set(sw::kPartnerGod, true);
  cheats::tick(r);
  check(partner.field<int16_t>(gl.hp) == 1200, "... filled up again as the switch goes on");
  cheats::set(sw::kPartnerGod, false);

  std::printf("-- Infinite air\n");
  {
    const cheats::AirLayout& a = cheats::g_air;
    check(a.air == 0x3B00 && a.most == 0x3B04 && a.flags == 0x4E6C && a.bit == 30,
          "the air: the combat gauge +0x3B00 (its most +0x3B04); under water: bit 30 of +0x4E6C");
    const uintptr_t drown = game::sig_one("kSigDrown");
    const uintptr_t splayer_global = mem::read<uint32_t>(drown + 58);
    check(drown == 0x484750 && mem::read<uint32_t>(drown + 87) == 0x226 && splayer_global == 0x17D0468 &&
              game::sig_one("kSigAirGauge") == 0x9E0714,
          "the drowning (0x484750: action 0x226, sPlayer's global 0x17D0468), the gauge on screen (uIdOxygen 0x9E0714)");
    // The game's own drowning run on Leon in the copy - its under-water test, its slot 58 (this machine runs
    // the character), sPlayer's set-health - with its slot 46 (the action played) stood in for.
    std::vector<uintptr_t> vt(300);
    std::memcpy(vt.data(), reinterpret_cast<const void*>(leon.field<uintptr_t>(0)), vt.size() * 4);
    vt[46] = reinterpret_cast<uintptr_t>(&stand_in_action);
    auto drowning = [&](Obj& who) {
      const uintptr_t was = who.field<uintptr_t>(0);
      who.field<uintptr_t>(0) = reinterpret_cast<uintptr_t>(vt.data());
      *reinterpret_cast<uintptr_t*>(splayer_global) = splayer.at();
      g_action = -1;
      reinterpret_cast<void(__cdecl*)(void*)>(drown)(who.mem.data());
      *reinterpret_cast<uintptr_t*>(splayer_global) = 0;
      who.field<uintptr_t>(0) = was;
    };
    // The game's own add to the gauge (uPlayer's slot 122: what the gauge's frame adds the dive's rate through).
    auto add_air = [&](Obj& who, float amount) {
      const uintptr_t vtable = who.field<uintptr_t>(0);
      reinterpret_cast<void(__thiscall*)(void*, float)>(mem::read<uintptr_t>(vtable + 4 * 122))(who.mem.data(), amount);
    };
    const uint32_t under = 1u << a.bit;
    auto tick = [&] {
      game_tick(main_obj.mem.data(), &r);
      cheats::tick(r);
    };
    check(cheats::g_airadd.size() >= 50 && cheats::g_airadd_slot == 122 && cheats::g_airadd_fn == 0xA63B90,
          "the combat gauge's add (uPlayer's slot 122, 0xA63B90) hooked in every player vtable");
    leon.field<float>(a.most) = 500.0f;
    leon.field<float>(a.air) = 0.0f;
    leon.field<uint32_t>(a.flags) = 0x100 | under;
    partner.field<float>(a.most) = 500.0f;
    tick();
    drowning(leon);
    check(leon.field<int16_t>(gl.hp) == 0 && g_action == 0x226,
          "off: under water with no air, the game's own drowning - health 0, the drowning (0x226) played");
    leon.field<int16_t>(gl.hp) = 1200;
    leon.field<float>(a.air) = 120.0f;
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 112.0f, "off: the game's own add takes a dive's 8 off (112)");
    cheats::set(sw::kInfiniteAir, true);
    tick();
    check(leon.field<float>(a.air) == 112.0f && leon.field<uint32_t>(a.flags) == (0x100 | under),
          "on: nothing written by the tick (the air 112 until the game's own add)");
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 400.0f && leon.field<uint32_t>(a.flags) == (0x100 | under),
          "on: you under water - the game's own add of a dive's drain (-8) made a fill (from 112 the game's own add "
          "stops at 4 blocks, 400, as a refill crosses 3)");
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 500.0f, "... its next to its most (500)");
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 500.0f, "... and keeps it there (500)");
    drowning(leon);
    check(leon.field<int16_t>(gl.hp) == 1200 && g_action == -1, "on: the game's own drowning - nothing (air left)");
    leon.field<float>(a.air) = 0.0f;
    add_air(leon, -8.0f);
    drowning(leon);
    check(leon.field<float>(a.air) == 400.0f && leon.field<int16_t>(gl.hp) == 1200 && g_action == -1,
          "on: a gauge emptied is refilled at the game's next add (400), before its frame runs the drowning");
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 500.0f, "... and full at the one after");
    leon.field<uint32_t>(a.flags) = 0x100;
    leon.field<float>(a.air) = 250.0f;
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 242.0f, "on: on land your gauge (the combat gauge) is left to the game (242)");
    partner.field<uint32_t>(a.flags) = under;
    partner.field<float>(a.air) = 100.0f;
    add_air(partner, -8.0f);
    check(partner.field<float>(a.air) == 92.0f, "on: the computer's partner under water - left to the game (92)");
    leon.field<uint32_t>(a.flags) = under;
    leon.field<float>(a.air) = 100.0f;
    leon.field<uint32_t>(gl.control) = 2;
    add_air(leon, -8.0f);
    drowning(leon);
    check(leon.field<float>(a.air) == 92.0f && g_action == -1 && leon.field<int16_t>(gl.hp) == 1200,
          "on: a character played over the network - left to its own game (the drowning here passes it by too)");
    leon.field<uint32_t>(gl.control) = 0;
    cheats::set(sw::kInfiniteAir, false);
    tick();
    add_air(leon, -8.0f);
    check(leon.field<float>(a.air) == 84.0f, "off again: your air left to the game (84)");
    leon.field<uint32_t>(a.flags) = 0;
    partner.field<uint32_t>(a.flags) = 0;
    leon.field<float>(a.air) = partner.field<float>(a.air) = 0.0f;
    leon.field<float>(a.most) = partner.field<float>(a.most) = 0.0f;
  }

  std::printf("-- Infinite stamina\n");
  {
    const cheats::AirLayout& a = cheats::g_air;
    check(game::sig_one("kSigGaugeSpend") == 0xACE4C0 && cheats::g_spend_fn == 0xACE4C0 && cheats::g_spend_slot == 123 &&
              cheats::g_training == 0x54,
          "the combat gauge's spend: uPlPlayer's slot 123 (0xACE4C0), the one after its add; the game's own takes "
          "nothing under skill 0x54 (Training)");
    check(cheats::g_spend.size() >= 50 &&
              mem::read<uintptr_t>(leon.field<uintptr_t>(0) + 4 * 123) ==
                  reinterpret_cast<uintptr_t>(&cheats::hk_gauge_spend) &&
              mem::read<uintptr_t>(vt_of("uPlEnemy") + 4 * 123) == 0xAA5390,
          "the spend hooked in every player vtable holding it (Leon's); Agent Hunt's creatures' own (uPlEnemy) left");
    const uint32_t pause = mem::read<uint32_t>(game::sig_one("kSigAirAdd") + 10);
    check(pause == 0x3B08, "the add's pause (after a hit) +0x3B08");
    // The game's globals its skill check reads, made up in the copy: sInfoMan (the profile's cGameInfo: not No Hope,
    // which has no skills), sPlayer (the characters by their numbers) and sGameMan (each player's three skills, from
    // +0x728).
    std::vector<uint8_t> gman(0x1000, 0), sinfo(mt::find("sInfoMan")->size, 0);
    const uint32_t kGameMan = 0x17CF454, kInfoG = 0x17CF49C, kPlayerG = 0x17D0468;
    const uint32_t gm_was = mem::read<uint32_t>(kGameMan), info_was = mem::read<uint32_t>(kInfoG),
                   pl_was = mem::read<uint32_t>(kPlayerG);
    mem::write<uintptr_t>(kGameMan, reinterpret_cast<uintptr_t>(gman.data()));
    mem::write<uintptr_t>(kInfoG, reinterpret_cast<uintptr_t>(sinfo.data()));
    mem::write<uintptr_t>(kPlayerG, splayer.at());
    // The game's own spend (slot 123), add (122) and frame's refill (126: its rate, +0x3B10, times the frame's time
    // and its skills' factor, through the add), each through the character's own vtable.
    using SpendFn = void(__thiscall*)(void*, int);
    using FloatFn = void(__thiscall*)(void*, float);
    auto slot = [](Obj& who, int n) { return mem::read<uintptr_t>(who.field<uintptr_t>(0) + 4 * n); };
    auto spend = [&](Obj& who, int blocks) { reinterpret_cast<SpendFn>(slot(who, 123))(who.mem.data(), blocks); };
    auto add = [&](Obj& who, float amount) { reinterpret_cast<FloatFn>(slot(who, 122))(who.mem.data(), amount); };
    constexpr uint32_t kRate = 0x3B10;
    auto refill = [&](Obj& who, float rate, float dt) {
      who.field<float>(kRate) = rate;
      reinterpret_cast<FloatFn>(slot(who, 126))(who.mem.data(), dt);
    };
    auto gauge = [&](Obj& who) { return who.field<float>(a.air); };
    auto tick = [&] {
      game_tick(main_obj.mem.data(), &r);
      cheats::tick(r);
    };
    const int16_t leon_hp = leon.field<int16_t>(gl.hp), partner_hp = partner.field<int16_t>(gl.hp);
    const uint8_t leon_no = leon.field<uint8_t>(0x4E88), partner_no = partner.field<uint8_t>(0x4E88);
    for (Obj* o : {&leon, &partner}) {
      o->field<float>(a.most) = 1100.0f;  // 8 blocks over the 300 below them (Combat Gauge Boost Lv. 1)
      o->field<float>(a.air) = 1100.0f;
      o->field<uint32_t>(a.flags) = 0x100;  // on land
      o->field<int16_t>(gl.hp) = 1200;      // up: the frame's refill runs for a character standing
    }
    leon.field<uint8_t>(0x4E88) = 0;  // the players' numbers (sPlayer's array)
    partner.field<uint8_t>(0x4E88) = 1;
    tick();
    spend(leon, 1);
    check(gauge(leon) == 1000.0f, "off: the game's own spend takes a block (100) off your 1100");
    spend(leon, 7);
    check(gauge(leon) == 0.0f, "off: a spend that leaves 300 or less (below the first block) empties the gauge");
    leon.field<float>(a.air) = 1100.0f;
    refill(leon, -50.0f, 0.5f);
    check(gauge(leon) == 1075.0f, "off: the game's own frame's refill at a drain's rate (-50 a second, half a second)");
    *reinterpret_cast<uint32_t*>(gman.data() + 0x728) = cheats::g_training;  // Leon's first skill: Training
    leon.field<float>(a.air) = 1100.0f;
    spend(leon, 1);
    check(gauge(leon) == 1100.0f,
          "off: under the Training skill the game's own spend takes nothing (its own way past: the mod's)");
    *reinterpret_cast<uint32_t*>(gman.data() + 0x728) = 0;

    cheats::set(sw::kInfiniteStamina, true);
    leon.field<float>(a.air) = 1000.0f;
    tick();
    check(gauge(leon) == 1000.0f, "on: nothing written by the tick (1000 until the game's own add)");
    spend(leon, 1);
    spend(leon, 7);
    check(gauge(leon) == 1000.0f, "on: your spends take nothing (a block, 7 blocks: 1000 as it was)");
    refill(leon, 100.0f, 0.016f);
    check(gauge(leon) == 1100.0f, "on: the game's own frame's refill fills it to its most (1100) through the add");
    refill(leon, -100.0f, 0.5f);
    add(leon, -300.0f);
    check(gauge(leon) == 1100.0f, "on: on land a drain's rate, and the game's own add of a drain, keep it there");
    leon.field<float>(a.air) = 0.0f;  // the game's own emptying (going down)
    refill(leon, 100.0f, 0.016f);
    check(gauge(leon) == 400.0f,
          "on: a gauge the game emptied refilled at the frame's next refill - to 400 (the game's own add stops there "
          "as a refill crosses 300)");
    refill(leon, 0.0f, 0.016f);
    check(gauge(leon) == 1100.0f, "... its most at the next (an action's rate of 0 too)");
    leon.field<float>(a.air) = 1000.0f;
    leon.field<float>(pause) = 1.0f;
    refill(leon, 100.0f, 0.016f);
    spend(leon, 1);
    check(gauge(leon) == 1000.0f, "on: in the pause after a hit the game's own add adds nothing; the spend takes nothing");
    leon.field<float>(pause) = 0.0f;
    leon.field<int16_t>(gl.hp) = 1;  // down (dying)
    leon.field<float>(a.air) = 0.0f;
    refill(leon, 100.0f, 0.016f);
    check(gauge(leon) == 0.0f, "on: while you are down the game's frame has no refill: the gauge as the game left it");
    leon.field<int16_t>(gl.hp) = 1200;
    refill(leon, 100.0f, 0.016f);
    refill(leon, 100.0f, 0.016f);
    check(gauge(leon) == 1100.0f, "on: back on your feet, full at the frame's refills (400, then 1100)");
    // What the game keeps at 0 itself: a scene's script turning the gauge off (+0x4E80 bit 16), Piers' mutated arm.
    const uint32_t word = leon.field<uint32_t>(0x4E80);
    leon.field<uint32_t>(0x4E80) = word | 0x10000;
    refill(leon, 100.0f, 0.016f);
    check(gauge(leon) == 0.0f,
          "on: a scene that turns the gauge off (+0x4E80 bit 16): the game's own refill empties it - left to it");
    leon.field<uint32_t>(0x4E80) = word;
    {
      Obj piers(0x5300, vt_of("uPl03PiersCanon"));
      piers.field<float>(a.most) = piers.field<float>(a.air) = 1100.0f;
      piers.field<int16_t>(gl.hp) = 1200;
      refill(piers, 100.0f, 0.016f);
      check(gauge(piers) == 0.0f,
            "on: Piers with his mutated arm (uPl03PiersCanon): the game's own refill keeps his at 0 - left to it");
    }
    // Under water the gauge is the air: Infinite air's.
    leon.field<float>(a.air) = 600.0f;
    leon.field<uint32_t>(a.flags) = 0x100 | (1u << a.bit);
    add(leon, -8.0f);
    check(gauge(leon) == 592.0f, "on: under water (Infinite air off) the gauge is the air - a dive's drain the game's");
    cheats::set(sw::kInfiniteAir, true);
    add(leon, -8.0f);
    check(gauge(leon) == 1100.0f, "... and with Infinite air on too, Infinite air's fill");
    cheats::set(sw::kInfiniteAir, false);
    leon.field<uint32_t>(a.flags) = 0x100;
    // The computer's partner and a character played over the network: the game's own.
    spend(partner, 1);
    check(gauge(partner) == 1000.0f, "on: the computer's partner's spend the game's (1000)");
    refill(partner, -50.0f, 0.5f);
    check(gauge(partner) == 975.0f, "on: ... and its refill (975)");
    leon.field<uint32_t>(gl.control) = 2;
    leon.field<float>(a.air) = 1100.0f;
    spend(leon, 1);
    add(leon, 2.0f);
    check(gauge(leon) == 1002.0f, "on: a character played over the network: the game's own (1000, then 1002)");
    leon.field<uint32_t>(gl.control) = 0;
    cheats::set(sw::kInfiniteStamina, false);
    tick();
    leon.field<float>(a.air) = 1100.0f;
    spend(leon, 1);
    add(leon, 2.0f);
    check(gauge(leon) == 1002.0f, "off again: your spend takes its block and the add is the game's (1002)");

    mem::write<uint32_t>(kGameMan, gm_was);
    mem::write<uint32_t>(kInfoG, info_was);
    mem::write<uint32_t>(kPlayerG, pl_was);
    for (Obj* o : {&leon, &partner}) {
      o->field<float>(a.most) = o->field<float>(a.air) = o->field<float>(kRate) = 0.0f;
      o->field<uint32_t>(a.flags) = 0;
    }
    leon.field<int16_t>(gl.hp) = leon_hp;
    partner.field<int16_t>(gl.hp) = partner_hp;
    leon.field<uint8_t>(0x4E88) = leon_no;
    partner.field<uint8_t>(0x4E88) = partner_no;
  }

  std::printf("-- One hit kills on the room bombs (the floor robots of Leon's chapter 4)\n");
  {
    const uintptr_t roll = game::sig_one("kSigBombHit"), warn = game::sig_one("kSigBombWarnHit");
    check(roll == 0xBB94A0 && warn == 0xBB9F90 && cheats::g_bombs_ok,
          "the room bombs' two hit checks: while one rolls (0xBB94A0), in its warning (0xBB9F90)");
    check(cheats::g_bomb.dealer == 0x2B5C && cheats::g_bomb.test == 0xAD2F20 && cheats::g_bomb_calls[0].at == 0xBB94F5 &&
              cheats::g_bomb_calls[1].at == 0xBB9FD9,
          "a set model's hit record: its dealer +0x2B5C; the game's instant test 0xAD2F20, called at 0xBB94F5 and 0xBB9FD9");
    // A room bomb (uSm1310) as its setup leaves it - 5 points, its most 5 (0x50005 at the characters' health) - with
    // its slot 80 (post an event: thiscall (who, event, arg)) and slot 46 (play an action) stood in for, in a copy of
    // its vtable. Its slot 81 (0xBB99B0, the event's handling) is the game's.
    const mt::Class* bc = mt::find("uSm1310");
    std::vector<uintptr_t> bvt(120);
    std::memcpy(bvt.data(), reinterpret_cast<const void*>(bc->vtable), bvt.size() * 4);
    static int posted = -1, posted_arg = -1;
    struct Post {
      static void __thiscall event(void*, int, int what, int arg) {
        posted = what;
        posted_arg = arg;
      }
    };
    bvt[80] = reinterpret_cast<uintptr_t>(&Post::event);
    bvt[46] = reinterpret_cast<uintptr_t>(&stand_in_action);
    Obj bomb(bc->size, reinterpret_cast<uintptr_t>(bvt.data()));
    const uint32_t kFlags = 0x2A08, kDamage = 0x2B14;  // its flags (bit 2: a hit pending); the hit record's damage
    auto points = [&] { return bomb.field<int16_t>(gl.hp); };
    auto reset = [&](int16_t p) {
      bomb.field<int16_t>(gl.hp) = p;
      bomb.field<int16_t>(gl.hp_max) = 5;
      bomb.field<uint32_t>(kFlags) = 0;
      posted = posted_arg = g_action = -1;
    };
    // The hit record as the game fills it from a hit (0x5E64D0): the damage, the dealer.
    auto hit_by = [&](Obj* who, int damage) {
      bomb.field<int32_t>(kDamage) = damage;
      bomb.field<uintptr_t>(cheats::g_bomb.dealer) = who ? who->at() : 0;
    };
    // While it rolls: its Move action's frame runs the hit check, then 0xBB83D0 posts event 9 - with 1 (a point left:
    // its slot 81 takes one, action 9, the hit) or 0 (none left: action 10, it goes off).
    using CheckFn = char(__thiscall*)(void*);
    using EventFn = void(__thiscall*)(void*, int, int, int);
    const auto handle = reinterpret_cast<EventFn>(mem::read<uintptr_t>(bc->vtable + 4 * 81));
    auto rolling_hit = [&]() -> bool {
      posted = posted_arg = g_action = -1;
      if (!reinterpret_cast<CheckFn>(roll)(bomb.mem.data())) return false;
      reinterpret_cast<void(__thiscall*)(void*)>(0xBB83D0)(bomb.mem.data());
      if (posted == 9) handle(bomb.mem.data(), 0, posted, posted_arg);
      return true;
    };
    auto tick = [&] {
      game_tick(main_obj.mem.data(), &r);
      cheats::tick(r);
    };
    check(reinterpret_cast<uintptr_t>(handle) == 0xBB99B0, "uSm1310's slot 81 (the events it takes) is the game's 0xBB99B0");
    tick();
    check(game::call_target(0xBB94F5) == 0xAD2F20 && game::call_target(0xBB9FD9) == 0xAD2F20,
          "One hit kills off: both checks call the game's instant test");

    // Off: Leon's hits (150 damage each) - one point a hit, and the hit after the last sets it off.
    reset(5);
    hit_by(&leon, 150);
    bool first = rolling_hit();
    check(first && posted == 9 && posted_arg == 1 && points() == 4 && g_action == 9 && !(bomb.field<uint32_t>(kFlags) & 4),
          "off: Leon's hit - event 9 with a point left, 4 points, action 9 (the hit)");
    int hits = 1;
    while (g_action != 10 && hits < 10) {
      hit_by(&leon, 150);
      rolling_hit();
      ++hits;
    }
    check(hits == 6 && g_action == 10 && points() == 0,
          "off: the game's own code takes 6 hits (its 5 points, then one more) before it goes off (action 10)");

    // On: the two calls re-pointed at the mod; Leon's first hit sets it off.
    cheats::set(sw::kOneHitKills, true);
    tick();
    check(game::call_target(0xBB94F5) == reinterpret_cast<uintptr_t>(&cheats::hk_bomb_instant) &&
              game::call_target(0xBB9FD9) == reinterpret_cast<uintptr_t>(&cheats::hk_bomb_instant),
          "on: both checks call the mod's");
    reset(5);
    hit_by(&leon, 150);
    rolling_hit();
    check(posted == 9 && posted_arg == 0 && points() == 0 && g_action == 10,
          "on: Leon's first hit - its points emptied by the game's own code, event 9 with none left: it goes off (10)");
    reset(5);
    partner.field<uint32_t>(gl.control) = 1;
    hit_by(&partner, 40);
    rolling_hit();
    check(points() == 0 && g_action == 10, "on: the computer's partner's hit (Helena) - the same");
    reset(5);
    hit_by(&leon, 0);
    check(!rolling_hit() && points() == 5 && posted == -1, "on: a hit with no damage - nothing (as the game)");
    // Dealers made a character's size: the game reads who drives a dealer (+0x4E90) of any.
    Obj gun(0x5300, vt_of("uWp1010"));
    reset(5);
    hit_by(&gun, 150);
    rolling_hit();
    check(points() == 0 && g_action == 10, "on: a hit dealt by a players' weapon (uWp1010, the players' side to the game) - the same");
    Obj blast(0x5300, vt_of("uSm9045"));
    reset(5);
    hit_by(&blast, 150);
    check(!rolling_hit() && points() == 5, "on: a blast of the stage's (uSm9045) - the game's own test turns it away");
    Obj em(0x4600, vt_of("uEm1000"));
    reset(5);
    hit_by(&em, 150);
    check(!rolling_hit() && points() == 5, "on: an enemy's hit - the game's own test turns it away");
    leon.field<uint32_t>(gl.control) = 2;
    reset(5);
    hit_by(&leon, 150);
    check(!rolling_hit() && points() == 5, "on: a character played over the network - the game's own test turns it away");
    leon.field<uint32_t>(gl.control) = 0;

    // Its warning (the second it beeps before it goes off by you): the Warning action's frame, counting (its step 1),
    // over a made-up sBioMain (a frame's time +0x10234) - a hit takes a point (off) or empties them, and with none left
    // it goes off at once as one shot (event 10), not by you (event 6, a second on).
    const uint32_t kMainG = 0x186E1B4;
    const uint32_t main_was = mem::read<uint32_t>(kMainG);
    std::vector<uint8_t> mainm(0x10500, 0);
    *reinterpret_cast<float*>(mainm.data() + 0x10234) = 1.0f / 60;
    mem::write<uintptr_t>(kMainG, reinterpret_cast<uintptr_t>(mainm.data()));
    Obj act(mt::find("cSmRoomBombAct_Warning")->size, vt_of("cSmRoomBombAct_Warning"));
    auto warning = [&] {
      act.field<uint8_t>(0x14) = 1;
      act.field<uintptr_t>(0x18) = bomb.at();
      act.field<float>(0x1C) = 0.0f;
      posted = posted_arg = -1;
      reinterpret_cast<void(__thiscall*)(void*)>(warn)(act.mem.data());
    };
    check(mem::read<uintptr_t>(vt_of("cSmRoomBombAct_Warning") + 4 * 7) == warn, "its Warning action's frame (slot 7) is the check");
    reset(5);
    hit_by(&leon, 150);
    warning();
    check(points() == 0 && posted == 10, "on, in its warning: Leon's hit empties its points - it goes off at once (event 10)");
    reset(5);
    hit_by(nullptr, 0);
    warning();
    check(points() == 5 && posted == -1, "on, in its warning: no hit - nothing yet (its second still counting)");
    cheats::set(sw::kOneHitKills, false);
    tick();
    reset(5);
    hit_by(&leon, 150);
    warning();
    check(points() == 4 && posted == -1, "off, in its warning: Leon's hit takes one point, its second counts on");
    check(game::call_target(0xBB94F5) == 0xAD2F20 && game::call_target(0xBB9FD9) == 0xAD2F20,
          "off again: both checks call the game's instant test");
    mem::write<uintptr_t>(kMainG, main_was);
  }

  std::printf("-- the inventories\n");
  {
    inventory::init();
    // Leon: a gun in slot 1 (in hand), its arrows-like three counts in slot 4, tablets in slot 17.
    auto put = [&](Obj& who, int i, int slot, int id, int mode, std::initializer_list<int> num,
                   std::initializer_list<int> most) {
      const uintptr_t it = gl.inventory + i * gl.item_size;
      who.field<uint8_t>(it + gl.it_used) = 1;
      who.field<uint8_t>(it + gl.it_slot) = static_cast<uint8_t>(slot);
      who.field<uint16_t>(it + gl.it_id) = static_cast<uint16_t>(id);
      who.field<uint8_t>(it + gl.it_mode) = static_cast<uint8_t>(mode);
      int k = 0;
      for (int v : num) who.field<int16_t>(it + gl.it_num + 2 * k++) = static_cast<int16_t>(v);
      k = 0;
      for (int v : most) who.field<int16_t>(it + gl.it_max + 2 * k++) = static_cast<int16_t>(v);
    };
    put(leon, 3, 17, 0x11D, 0, {3}, {9});
    put(leon, 0, 1, 0x100, 0, {13}, {13});
    put(leon, 7, 4, 0x103, 1, {10, 5, 0}, {20, 10, 0});
    leon.field<int32_t>(gl.in_hand) = 1;
    partner.field<uint32_t>(gl.control) = 1;
    game::tick(main_obj.mem.data(), &r);  // dispatch's flag clear: the panel unable to show
    inventory::tick(r);
    check(inventory::view().ncases == 0, "the panel unable to show: no character's items read");
    game_tick(main_obj.mem.data(), &r);
    inventory::tick(r);
    const inventory::View v = inventory::view();
    check(v.known && v.paused && v.ncases == 2, "the view: both characters, at the pause");
    const inventory::Case& lc = v.cases[0];
    check(lc.nitems == 3 && lc.items[0].slot == 1 && lc.items[1].slot == 4 && lc.items[2].slot == 17,
          "Leon's three items by slot number (1, 4, 17), whatever their place in the inventory");
    check(lc.items[0].in_hand && !lc.items[1].in_hand && lc.items[0].num[0] == 13 && lc.items[1].mode == 1 &&
              lc.items[1].num[1] == 5 && lc.items[1].most[0] == 20 && lc.items[2].id == 0x11D,
          "each item's id, mode, counts and mosts; the one in hand marked");
    inventory::set_count(0, 17, 0x11D, 0, 7);
    inventory::set_count(0, 4, 0x103, 1, 99);
    inventory::set_count(0, 1, 0x105, 0, 1);  // typed for another item than the slot holds now
    inventory::tick(r);
    const uintptr_t tablets = gl.inventory + 3 * gl.item_size, arrows = gl.inventory + 7 * gl.item_size;
    check(leon.field<int16_t>(tablets + gl.it_num) == 7, "at the pause: the tablets' count set to 7");
    check(leon.field<int16_t>(arrows + gl.it_num + 2) == 10, "... a mode's count kept to its most (99 asked, 10)");
    check(leon.field<int16_t>(gl.inventory + gl.it_num) == 13, "... an item moved since the typing left alone");
    main_obj.field<uint8_t>(gl.paused) = 0;
    game_tick(main_obj.mem.data(), &r);
    inventory::set_count(0, 17, 0x11D, 0, 1);
    inventory::tick(r);
    check(leon.field<int16_t>(tablets + gl.it_num) == 7, "in play: no change (an edit belongs to the pause)");
    main_obj.field<uint8_t>(gl.paused) = 1;
    game_tick(main_obj.mem.data(), &r);
    leon.field<uint32_t>(gl.control) = 2;
    game_tick(main_obj.mem.data(), &r);
    inventory::set_count(0, 17, 0x11D, 0, 2);
    inventory::tick(r);
    check(leon.field<int16_t>(tablets + gl.it_num) == 7, "a character played over the network: left alone");
    leon.field<uint32_t>(gl.control) = 0;
  }

  std::printf("-- the modes, the results' numbers, the stage timers, the Mercenaries\n");
  check(gl.mode == 0xAC && gl.online == 0x878 && gl.match_clock == 0x486C && gl.match_hud == 0x3FCC &&
            gl.match_step == 0x3FC8 && gl.match_end == 0x4864 && gl.match_host == 0x87C && gl.match_me == 0x880 &&
            gl.match_most > 5999.0f && gl.match_most < 6000.0f,
        "sGameMan: the mode +0xAC, online +0x878; the match clock +0x486C (HUD +0x3FCC, at most 5999.99), step "
        "+0x3FC8, end +0x4864, host +0x87C, this machine +0x880");
  check(gl.game_info == 0x3F798 && gl.game_info_size == 0x3CE4 && gl.saved_back == 0x1AF0 && gl.clock == 0x1B3C &&
            gl.counters == 0x1B40 && gl.block == 0xB8 && gl.nblocks == 8 && gl.ncounters == 0x3F &&
            gl.counter_most == 50000 && gl.clock_most == 71999.0f,
        "the chapter's numbers: cGameInfo sInfoMan +0x3F798 (0x3CE4 each), a block of 0xB8 for 8 characters: the "
        "clock +0x1B3C (at most 71999 s), 63 counters +0x1B40 (at most 50000); the save's copy 0x1AF0 before");
  check(gl.c_shots == 0 && gl.c_hits == 1 && gl.c_kills == 2 && gl.c_deaths == 3,
        "the counters the results grade: shots 0, hits 1, kills 2, deaths 3");
  check(gl.timers == 0x664 && gl.timers_running == 0x65C && gl.timers_down == 0x660 && gl.ntimers == 32,
        "a stage's 32 timers +0x664 (running +0x65C, counting down +0x660)");
  check(merc::match_found() && merc::g_m.score == 0x4284 && merc::g_m.earned == 0x4280 && merc::g_m.combo == 0x4268 &&
            merc::g_m.running == 0x4264 && merc::g_m.combo_clock == 0x4270 && merc::g_m.best == 0x426C &&
            merc::g_m.ct_running == 0x4274 && merc::g_m.ct_clock == 0x4278 && merc::g_m.ct_kills == 0x427C &&
            merc::g_m.kills == 0xA34,
        "the match: the score +0x4284, earned +0x4280, the combo +0x4268 (running +0x4264, its window +0x4270, best "
        "+0x426C), Combo Time +0x4274 (clock +0x4278, kills +0x427C), the stage's kills +0xA34");
  check(merc::g_m.mode_merc == 0x1E && merc::g_m.mode_no_mercy == 0x50 && merc::g_m.cap == 9999999 &&
            merc::g_m.cap_no_mercy == 99999999 && merc::g_m.target == 150 && merc::g_m.target_no_mercy == 300 &&
            merc::g_m.ct_most == 30.0f,
        "the modes 0x1E and 0x50, the caps 9,999,999 / 99,999,999, the targets 150 / 300, Combo Time 30 s");
  check(merc::kills_found() && merc::g_k.record == 0x24A0 && merc::g_k.valid == 0x24BC && merc::g_k.damage == 4 &&
            merc::g_k.dealer == 0x28 && merc::g_k.attack == 0x2C && merc::g_k.pos == 0x40 && merc::g_k.dir == 0x50 &&
            merc::g_k.nattrs == 3 && merc::g_k.coord_pos == 0x50 && merc::g_k.died == 0x3B2F &&
            merc::g_k.reported == 0x2BA4,
        "an enemy's hit record +0x24A0 (valid +0x24BC; damage +4, dealer +0x28, attack +0x2C, where +0x40, towards "
        "+0x50); a unit's position (mPos) +0x50; died +0x3B2F, reported +0x2BA4");

  std::printf("-- Steady aim\n");
  {
    const uintptr_t sway = game::sig_one("kSigAimSway") + 17, kick = game::sig_one("kSigAimKick") + 32;
    cheats::set(sw::kSteadyAim, true);
    cheats::tick(run);
    check(mem::read<uint16_t>(sway) == 0x9090 && mem::read<uint8_t>(kick) == 0x90 && mem::read<uint8_t>(kick + 1) == 0xE9,
          "on: the sway takes its way out (its jne NOPed), the kick's je a jmp (the same place)");
    cheats::set(sw::kSteadyAim, false);
    cheats::tick(run);
    check(mem::read<uint8_t>(sway) == 0x75 && mem::read<uint8_t>(kick) == 0x0F && mem::read<uint8_t>(kick + 1) == 0x84,
          "off: the game's jne and je back");
  }

  std::printf("-- Steady aim through a scope (the view held while the aim is)\n");
  {
    const cheats::ScopeLayout& s = cheats::g_sl;
    check(cheats::g_scope_ok && s.cam_player == 0x80 && s.cam_eye == 0x50 && s.cam_at == 0x70 && s.pitch == 0x4ED0 &&
              s.twist == 0x4ED8 && s.world == 0xC0 && s.from == 0x4A30 && s.to == 0x4A40,
          "the scope camera's player +0x80, its eye +0x50 and the point it looks at +0x70; the aim's pitch +0x4ED0 and "
          "twist +0x4ED8; a model's world matrix +0xC0; the gun's line from +0x4A30 to +0x4A40");
    check(cheats::g_scope_call.at == 0x62E1DF && reinterpret_cast<uintptr_t>(cheats::g_scope_view) == 0x62C840 &&
              cheats::g_line_call.at == 0xA6CDE4 && reinterpret_cast<uintptr_t>(cheats::g_gun_line) == 0xA64240,
          "the scope's callback's call of the scope camera (0x62E1DF -> 0x62C840, the one of three callbacks whose "
          "camera calls the sway) and the laser's call of the gun's line (0xA6CDE4 -> 0xA64240)");
    cheats::set(sw::kSteadyAim, true);
    cheats::tick(run);
    check(game::call_target(0x62E1DF) == reinterpret_cast<uintptr_t>(&cheats::hk_scope_view) &&
              game::call_target(0xA6CDE4) == reinterpret_cast<uintptr_t>(&cheats::hk_gun_line),
          "on: both calls come to the mod");

    // The game's own scope callback (cdecl (camera, -, out): the view copied out) run on a made-up camera of
    // Leon's: its call of the scope camera comes to the mod, which calls on to the stand-in.
    const auto view_was = cheats::g_scope_view;
    const auto line_was = cheats::g_gun_line;
    cheats::g_scope_view = &stand_in_scope;
    cheats::g_gun_line = &stand_in_line;
    cheats::g_now = &fake_now;
    cheats::g_held = cheats::Held{};
    Obj cam(0x200, 0);
    cam.field<uintptr_t>(s.cam_player) = leon.at();
    std::vector<uint8_t> world_was(64);
    std::memcpy(world_was.data(), leon.mem.data() + s.world, 64);
    auto face = [&](float yaw) {  // Leon's world matrix: turned by yaw, standing at (10, 0, 20)
      const float m[16] = {std::cos(yaw), 0, -std::sin(yaw), 0, 0, 1, 0, 0,
                           std::sin(yaw), 0, std::cos(yaw),  0, 10, 0, 20, 1};
      std::memcpy(leon.mem.data() + s.world, m, 64);
    };
    face(0);
    leon.field<float>(s.pitch) = 0;
    leon.field<float>(s.twist) = 0;
    leon.field<uint32_t>(gl.control) = 0;
    using Callback = uint8_t(__cdecl*)(void*, void*, void*);
    const auto callback = reinterpret_cast<Callback>(cheats::g_scope_call.at - 15);
    uint8_t out[0x60] = {};
    float vy = 0, vp = 0, ve[3] = {};  // the view copied out: its yaw, pitch and eye
    auto frame = [&]() {
      g_clock += 1.0 / 60;
      ++cheats::g_frame;
      const uint8_t worked = callback(cam.mem.data(), nullptr, out);
      float e[3], a[3], d[3];
      std::memcpy(e, out + 0x10, 12);
      std::memcpy(a, out + 0x20, 12);
      for (int i = 0; i < 3; ++i) d[i] = a[i] - e[i];
      angles_of(d, &vy, &vp);
      std::memcpy(ve, e, 12);
      return worked;
    };
    constexpr float kDeg = 3.14159265f / 180, kTau = 2 * 3.14159265f;
    auto close = [](float a, float b, float tol) { return std::fabs(a - b) <= tol; };
    // The scope comes up: the pose raises the gun (30 degrees in 0.3 s).
    bool game_view = true;
    for (int i = 0; i <= 18; ++i) {
      g_gun.pitch = (-30.0f + 30.0f * i / 18) * kDeg;
      g_gun.yaw = 0;
      g_gun.eye[0] = 10;
      g_gun.eye[1] = 1.0f + 0.5f * i / 18;
      g_gun.eye[2] = 20.3f;
      game_view = game_view && frame() == 1 && close(vp, g_gun.pitch, 1e-4f) && close(ve[1], g_gun.eye[1], 1e-4f);
    }
    check(game_view && !cheats::g_held.holding,
          "the scope comes up: the view is the game's while the pose raises the gun (the callback's own copy, al 1)");
    // Breathing (the pitch 0.5 degrees, the yaw 0.3, the eye 3 mm) for two seconds.
    float held_y = 0, held_p = 0, held_e[3] = {}, most = 0;
    bool held = false, steady = true;
    int frames_to_hold = -1;
    for (int i = 0; i < 120; ++i) {
      const float t = i / 60.0f;
      g_gun.pitch = 0.5f * kDeg * std::sin(kTau * t / 4);
      g_gun.yaw = 0.3f * kDeg * std::sin(kTau * t / 3);
      g_gun.eye[1] = 1.5f + 0.003f * std::sin(kTau * t / 4);
      frame();
      if (cheats::g_held.holding && !held) {
        held = true;
        frames_to_hold = i;
        held_y = vy;
        held_p = vp;
        std::memcpy(held_e, ve, 12);
      } else if (held) {
        steady = steady && close(vy, held_y, 1e-4f) && close(vp, held_p, 1e-4f) && close(ve[1], held_e[1], 1e-4f);
        most = std::max(most, std::fabs(g_gun.pitch - vp));
      }
    }
    check(held && frames_to_hold >= 10 && frames_to_hold <= 14,
          "the gun settled (breathing, well below 6 degrees a second): the view held 0.2 s after the raise");
    check(steady && most > 0.2f * kDeg, "held: the gun breathes (half a degree, 3 mm), the view and its eye stand still");
    // A shot's kick: the gun up 4 degrees and 5 cm back for a tenth of a second.
    const float breath = g_gun.pitch;
    for (int i = 0; i < 6; ++i) {
      g_gun.pitch = breath + 4 * kDeg;
      g_gun.eye[2] = 20.25f;
      frame();
      steady = steady && close(vy, held_y, 1e-4f) && close(vp, held_p, 1e-4f) && close(ve[2], held_e[2], 1e-4f);
    }
    g_gun.pitch = breath;
    g_gun.eye[2] = 20.3f;
    frame();
    check(steady && close(vp, held_p, 1e-4f), "a shot's kick (4 degrees, the gun 5 cm back) never reaches the view");
    // Aiming down: the pitch 0.01 a frame for ten frames, the gun 0.9 degrees a frame with it.
    const float p0 = vp, y0 = vy;
    for (int i = 0; i < 10; ++i) {
      leon.field<float>(s.pitch) += 0.01f;
      g_gun.pitch -= 0.9f * kDeg;
      frame();
    }
    check(close(vp, p0 - 9 * kDeg, 1e-3f) && close(vy, y0, 1e-4f),
          "aiming down (the pitch by 0.1): the view follows the gun's own 9 degrees, its yaw held");
    // Turning: the waist's twist 1 degree a frame for ten frames, the gun with it.
    for (int i = 0; i < 10; ++i) {
      leon.field<float>(s.twist) += 1.0f;
      g_gun.yaw += 1.0f * kDeg;
      frame();
    }
    check(close(vy, y0 + 10 * kDeg, 1e-3f) && close(vp, p0 - 9 * kDeg, 1e-3f),
          "turning (the twist by 10 degrees): the view follows the gun's yaw, its pitch held");
    // The body turns 30 degrees (past the twist's most, or what it stands on): the gun with it, the aim as it was.
    const float y1 = vy, p1 = vp, c = std::cos(30 * kDeg), sn = std::sin(30 * kDeg);
    float e1[3];
    std::memcpy(e1, ve, 12);
    face(30 * kDeg);
    g_gun.yaw += 30 * kDeg;
    {
      const float rx = g_gun.eye[0] - 10, rz = g_gun.eye[2] - 20;
      g_gun.eye[0] = 10 + rx * c + rz * sn;
      g_gun.eye[2] = 20 - rx * sn + rz * c;
    }
    frame();
    {
      const float rx = e1[0] - 10, rz = e1[2] - 20;
      check(close(vy, y1 + 30 * kDeg, 1e-4f) && close(vp, p1, 1e-4f) && close(ve[0], 10 + rx * c + rz * sn, 1e-3f) &&
                close(ve[2], 20 - rx * sn + rz * c, 1e-3f) && close(ve[1], e1[1], 1e-4f),
            "the body turns 30 degrees (no aim moved): the held view turns with it, its eye about the body");
    }
    // The gun's line through the mod: the line the game works out down the gun (breathing off the held view)
    // pointed along the held view, from where the game has it.
    g_gun.pitch += 0.4f * kDeg;
    cheats::hk_gun_line(leon.mem.data());
    {
      const float* from = &leon.field<float>(s.from);
      const float* to = &leon.field<float>(s.to);
      float d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]}, ly, lp;
      const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      angles_of(d, &ly, &lp);
      check(close(len, 50.0f, 1e-3f) && close(ly, vy, 1e-4f) && close(lp, vp, 1e-4f) &&
                close(from[2], g_gun.eye[2] + 0.25f, 1e-4f),
            "the gun's line: from where the game has it, as long, along the held view - a shot goes where the reticle is");
    }
    // The scope down (no view for five frames), then up again: the game's view until the gun has settled anew.
    for (int i = 0; i < 5; ++i) {
      g_clock += 1.0 / 60;
      ++cheats::g_frame;
    }
    g_gun.pitch = -20 * kDeg;
    frame();
    check(!cheats::g_held.holding && close(vp, g_gun.pitch, 1e-4f),
          "the scope down and up again: the game's view (the gun raised anew)");
    {
      // A gun that never settles (turning 20 degrees a second, no aim moved): held at the latest 1.5 s after.
      int at = -1;
      for (int i = 0; i < 120 && at < 0; ++i) {
        g_gun.yaw += 20 * kDeg / 60;
        frame();
        if (cheats::g_held.holding) at = i;
      }
      check(at >= 87 && at <= 91, "a gun that never settles (20 degrees a second): held 1.5 s after the scope came up");
      cheats::g_held.frame = cheats::g_frame - 2;  // the view not seen since the frame before last
      cheats::hk_gun_line(leon.mem.data());
      float d[3];
      gun_dir(d);
      bool own = true;
      for (int i = 0; i < 3; ++i)
        own = own && close(leon.field<float>(s.to + 4 * i), leon.field<float>(s.from + 4 * i) + d[i] * 50.0f, 1e-3f);
      check(own, "the gun's line once the scope's view is gone (two frames): the game's own, down the gun");
    }
    // A character played over the network: its view is its own machine's - left to the game.
    cheats::g_held = cheats::Held{};
    leon.field<uint32_t>(gl.control) = 2;
    bool theirs = true;
    for (int i = 0; i < 40; ++i) {
      g_gun.pitch = 0.5f * kDeg * std::sin(i / 6.0f);
      frame();
      theirs = theirs && close(vp, g_gun.pitch, 1e-4f) && !cheats::g_held.holding;
    }
    check(theirs, "a character played over the network: the game's view, never held");
    leon.field<uint32_t>(gl.control) = 0;
    // Off: the game's own view at once (the call still the mod's until the tick), then the calls back.
    for (int i = 0; i < 40; ++i) frame();
    check(cheats::g_held.holding, "(held again, on)");
    {
      // Lying on the back (the body's up along the ground, its forward to the sky; the gun at the horizon - straight
      // down the body's own up): the aim's turn followed whole, the view held otherwise.
      const float lying[16] = {1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 10, 0.3f, 20, 1};
      std::memcpy(leon.mem.data() + s.world, lying, 64);
      cheats::g_held = cheats::Held{};
      g_gun.yaw = 0;
      g_gun.pitch = 2 * kDeg;
      for (int i = 0; i < 40; ++i) frame();
      const bool lying_held = cheats::g_held.holding;
      const float ly0 = vy, lp0 = vp;
      bool still = true;
      for (int i = 0; i < 30; ++i) {
        g_gun.pitch = 2 * kDeg + 0.5f * kDeg * std::sin(i / 5.0f);
        frame();
        still = still && close(vy, ly0, 1e-4f) && close(vp, lp0, 1e-4f);
      }
      g_gun.pitch = 2 * kDeg;
      frame();
      for (int i = 0; i < 10; ++i) {
        leon.field<float>(s.twist) += 1.0f;
        g_gun.yaw += 1.0f * kDeg;
        frame();
      }
      check(lying_held && still && close(vy, ly0 + 10 * kDeg, 1e-3f) && close(vp, lp0, 1e-3f),
            "lying on the back: held through the breathing, the aim's 10 degree turn followed whole");
      face(30 * kDeg);
    }
    cheats::set(sw::kSteadyAim, false);
    bool games = true;
    for (int i = 0; i < 10; ++i) {
      g_gun.pitch = 3 * kDeg * std::sin(i / 2.0f);
      frame();
      games = games && close(vp, g_gun.pitch, 1e-4f);
    }
    cheats::tick(run);
    check(games && game::call_target(0x62E1DF) == 0x62C840 && game::call_target(0xA6CDE4) == 0xA64240,
          "off: the game's view at once, and both calls the game's again");
    cheats::g_scope_view = view_was;
    cheats::g_gun_line = line_was;
    cheats::g_now = &cheats::seconds_now;
    cheats::g_held = cheats::Held{};
    std::memcpy(leon.mem.data() + s.world, world_was.data(), 64);
    leon.field<float>(s.pitch) = leon.field<float>(s.twist) = 0;
    std::memset(leon.mem.data() + s.from, 0, 12);
    std::memset(leon.mem.data() + s.to, 0, 12);
  }

  std::printf("-- the chapter's numbers\n");
  {
    // A made-up profile 1: its cGameInfo in the made-up sInfoMan.
    const uintptr_t gi = info.at() + gl.game_info + gl.game_info_size;
    run.screen = game::Screen::kPlay;
    run.in_game = true;
    run.nchars = 2;
    run.chars[0].index = 0;
    run.chars[0].control = game::Control::kLocal;
    run.chars[0].name = "Leon";
    run.chars[1].index = 1;
    run.chars[1].control = game::Control::kAi;
    // The game's own counter add and clock add, over the made-up cGameInfo.
    using AddFn = void(__thiscall*)(void*, int, int, int);
    using ClockFn = void(__thiscall*)(void*, int, float);
    const auto add = reinterpret_cast<AddFn>(game::sig_one("kSigChapterCounterAdd"));
    const auto clock = reinterpret_cast<ClockFn>(game::call_target(game::sig_one("kSigChapterClockLoop") + 29));
    add(reinterpret_cast<void*>(gi), 0, gl.c_shots, 10);
    add(reinterpret_cast<void*>(gi), 0, gl.c_hits, 4);
    add(reinterpret_cast<void*>(gi), 0, gl.c_deaths, 2);
    clock(reinterpret_cast<void*>(gi), 0, 61.5f);
    clock(reinterpret_cast<void*>(gi), 1, 61.5f);
    cheats::tick(run);
    cheats::Status st = cheats::status();
    check(st.results && st.ncounters == 1 && st.chars[0].index == 0 && st.chars[0].shots == 10 && st.chars[0].hits == 4 &&
              st.chars[0].deaths == 2 && st.clock == 61.5f,
          "the game's own adds land where the mod reads: Leon's 10 shots, 4 hits, 2 deaths, the clock 61.5 s");
    cheats::set(sw::kFreezePlaytime, true);
    cheats::tick(run);
    clock(reinterpret_cast<void*>(gi), 0, 10.0f);
    check(cheats::chapter_clock(gi, 0)[0] == 61.5f, "Freeze play time: the game's clock add adds nothing");
    cheats::set(sw::kFreezePlaytime, false);
    cheats::tick(run);
    clock(reinterpret_cast<void*>(gi), 0, 10.0f);
    check(cheats::chapter_clock(gi, 0)[0] == 71.5f, "off: it adds again");
    const uintptr_t death = game::sig_one("kSigDeathCounted") + 24;
    cheats::set(sw::kNoDeathCount, true);
    cheats::tick(run);
    check(mem::read<uint8_t>(death) == 0x6A && mem::read<uint8_t>(death + 1) == 0x00,
          "Deaths don't count: the Game Over's count pushes 0");
    cheats::set(sw::kNoDeathCount, false);
    cheats::tick(run);
    check(mem::read<uint8_t>(death + 1) == 0x01, "off: 1 again");
    const uintptr_t shot_site = game::sig_one("kSigShotCount") + 30, hit_site = game::sig_one("kSigHitCount") + 35,
                    blow_site = game::sig_one("kSigHitCountBlow") + 15;
    check(cheats::g_count_ok && reinterpret_cast<uintptr_t>(cheats::g_count) == 0x464A90 && shot_site == 0x569149 &&
              hit_site == 0x56FA39 && blow_site == 0x51AD56,
          "a shot counted (0x569149) and a hit (0x56FA39 a shot's, 0x51AD56 a blow's): sInfoMan's counter add 0x464A90");
    cheats::set(sw::kNoMissCount, true);
    cheats::tick(run);
    check(*cheats::counter(gi, 0, gl.c_shots) == 4, "Misses don't count: Leon's shots put at his hits as it goes on (4)");
    check(game::call_target(shot_site) == reinterpret_cast<uintptr_t>(&cheats::hk_count_shot) &&
              game::call_target(hit_site) == reinterpret_cast<uintptr_t>(&cheats::hk_count_hit) &&
              game::call_target(blow_site) == reinterpret_cast<uintptr_t>(&cheats::hk_count_hit),
          "on: the shots' call and the hits' two come to the mod");
    // The counts as the game's calls make them, through sInfoMan's own add (the made-up sInfoMan's profile 1).
    cheats::hk_count_shot(info.mem.data(), 0, gl.c_shots, 1);
    check(*cheats::counter(gi, 0, gl.c_shots) == 4, "... a shot of Leon's counts nothing until it hits");
    cheats::hk_count_hit(info.mem.data(), 0, gl.c_hits, 1);
    check(*cheats::counter(gi, 0, gl.c_hits) == 5 && *cheats::counter(gi, 0, gl.c_shots) == 5,
          "... a hit (the game's own add) raises the hits, then the shots with them (5)");
    *cheats::counter(gi, 1, gl.c_shots) = 9;
    cheats::hk_count_shot(info.mem.data(), 1, gl.c_shots, 1);
    check(*cheats::counter(gi, 1, gl.c_shots) == 10, "... the computer's partner's shot counted by the game (10)");
    cheats::tick(run);
    check(*cheats::counter(gi, 1, gl.c_shots) == 10 && *cheats::counter(gi, 0, gl.c_shots) == 5,
          "... and the tick writes nothing");
    cheats::set(sw::kNoMissCount, false);
    cheats::tick(run);
    cheats::hk_count_shot(info.mem.data(), 0, gl.c_shots, 1);
    check(*cheats::counter(gi, 0, gl.c_shots) == 6 && game::call_target(shot_site) == 0x464A90 &&
              game::call_target(hit_site) == 0x464A90 && game::call_target(blow_site) == 0x464A90,
          "off: a shot counts again (6), the three calls the game's");
    cheats::edit(cheats::Edit::kDeaths, 0, 0);
    cheats::edit(cheats::Edit::kTime, 0, 600);
    cheats::tick(run);
    check(*cheats::counter(gi, 0, gl.c_deaths) == 2 && cheats::chapter_clock(gi, 0)[0] == 71.5f,
          "in play: the editors make nothing");
    run.screen = game::Screen::kPause;
    cheats::edit(cheats::Edit::kDeaths, 0, 0);
    cheats::edit(cheats::Edit::kKills, 0, 99999);
    cheats::edit(cheats::Edit::kTime, 0, 600);
    cheats::edit(cheats::Edit::kShots, 1, 50);
    cheats::tick(run);
    check(*cheats::counter(gi, 0, gl.c_deaths) == 0 && *cheats::counter(gi - gl.saved_back, 0, gl.c_deaths) == 0,
          "at the pause: Leon's deaths 0, in the live numbers and the save's copy");
    check(*cheats::counter(gi, 0, gl.c_kills) == 50000, "... a counter typed past its most held at it (50000)");
    bool all = true;
    for (int c = 0; c < gl.nblocks; ++c)
      all = all && cheats::chapter_clock(gi, c)[0] == 600.0f && cheats::chapter_clock(gi - gl.saved_back, c)[0] == 600.0f;
    check(all, "... the clock 600 s for every character, live and the save's copy");
    check(*cheats::counter(gi, 1, gl.c_shots) == 10, "... the computer partner's counters left alone");
    run.screen = game::Screen::kPlay;
  }

  std::printf("-- Automatic QTEs\n");
  {
    const uintptr_t call = game::sig_one("kSigQteCall") + 20, check_fn = game::sig_one("kSigQteCheck");
    const std::vector<uintptr_t> scenes = game::sig_all("kSigAutoActBtn"), regs = game::sig_all("kSigAutoActBtnReg");
    check(cheats::g_auto_scenes.size() == 19, "the option's 19 tests in the scenes found (16 + 3)");
    cheats::set(sw::kAutoQte, true);
    cheats::tick(run);
    bool scenes_on = true;
    for (const uintptr_t m : scenes) scenes_on = scenes_on && mem::read<uint8_t>(m + 21) == 0xFF;
    for (const uintptr_t m : regs) scenes_on = scenes_on && mem::read<uint8_t>(m + 15) == 0xA4;
    check(game::call_target(call) == reinterpret_cast<uintptr_t>(&cheats::hk_qte_check) && scenes_on,
          "on: the managers' check comes to the mod; the scenes read the option as on");
    run.chars[1].control = game::Control::kNetwork;
    cheats::tick(run);
    bool scenes_off = true;
    for (const uintptr_t m : scenes) scenes_off = scenes_off && mem::read<uint8_t>(m + 21) == 0x00;
    check(game::call_target(call) == reinterpret_cast<uintptr_t>(&cheats::hk_qte_check) && scenes_off,
          "with a character online: the check still the mod's, the scenes' tests the game's");
    run.chars[1].control = game::Control::kAi;
    // The hook over a made-up unit and request, the game's check a stand-in that says "not pressed".
    static bool s_pressed = false;
    struct Stand {
      static bool __thiscall check(void*, void*, int) { return s_pressed; }
    };
    const cheats::QteCheckFn real = cheats::g_qte_check;
    cheats::g_qte_check = &Stand::check;
    const cheats::QteLayout& q = cheats::g_qte;
    Obj unit(0x84, 0), req(0x8C, 0);
    unit.field<uintptr_t>(q.request) = req.at();
    leon.field<uint32_t>(gl.control) = 0;
    partner.field<uint32_t>(gl.control) = 1;
    unit.field<uint32_t>(q.kind) = 0x08;  // the stick waggled (a grab)
    unit.field<uint8_t>(q.drawn) = 1;
    check(cheats::hk_qte_check(unit.mem.data(), nullptr, 0) && unit.field<uint8_t>(q.drawn) == 0,
          "a grab's stick waggle for you: answered, not drawn");
    check(!cheats::hk_qte_check(unit.mem.data(), nullptr, 1), "... for the computer's partner: left to the game");
    unit.field<uint32_t>(q.kind) = 0x01;  // a plain press: a door, an item
    check(!cheats::hk_qte_check(unit.mem.data(), nullptr, 0), "a plain press (a context action): left to you");
    req.field<uint32_t>(q.flags) = q.dodge_bit;
    check(cheats::hk_qte_check(unit.mem.data(), nullptr, 0), "... a dodge made as a plain press: answered");
    req.field<uint32_t>(q.flags) = 0;
    unit.field<uint32_t>(q.kind) = 0x0C;  // a press in time, its good window from 70%
    unit.field<float>(q.window) = 70.0f;
    unit.field<float>(q.progress) = 50.0f;
    req.field<uint8_t>(q.good) = 0;
    check(!cheats::hk_qte_check(unit.mem.data(), nullptr, 0), "a press in time before its window: waited for");
    unit.field<float>(q.progress) = 72.0f;
    check(cheats::hk_qte_check(unit.mem.data(), nullptr, 0) && req.field<uint8_t>(q.good) == 1,
          "... in its window: answered, and marked good");
    splayer.field<uint8_t>(q.blocked) = 1;  // player 0's input held back
    check(!cheats::hk_qte_check(unit.mem.data(), nullptr, 0), "input held back by the game: left alone");
    splayer.field<uint8_t>(q.blocked) = 0;
    req.field<uint32_t>(q.failed) = 1;
    check(!cheats::hk_qte_check(unit.mem.data(), nullptr, 0), "one already failed: left alone");
    req.field<uint32_t>(q.failed) = 0;
    s_pressed = true;
    unit.field<uint32_t>(q.kind) = 0x01;
    check(cheats::hk_qte_check(unit.mem.data(), nullptr, 0), "the game's own yes passed on");
    s_pressed = false;
    cheats::g_qte_check = real;
    cheats::set(sw::kAutoQte, false);
    cheats::tick(run);
    bool back = true;
    for (const uintptr_t m : scenes) back = back && mem::read<uint8_t>(m + 21) == 0x00;
    for (const uintptr_t m : regs) back = back && (mem::read<uint8_t>(m + 15) == 0xAC || mem::read<uint8_t>(m + 15) == 0x9C);
    check(game::call_target(call) == check_fn && back, "off: the game's check and tests back");
  }

  std::printf("-- the countdowns\n");
  {
    // The modes' match clock: a made-up sGameMan in a Mercenaries match, offline.
    Obj gm(mt::find("sGameMan")->size, vt_of("sGameMan"));
    gm.field<uint32_t>(gl.mode) = 0x1E;
    gm.field<int32_t>(gl.match_step) = 1;
    gm.field<float>(gl.match_clock) = 100.0f;
    game::g_sgameman = gm.at();
    run.mode_known = true;
    run.mode = 0x1E;
    run.screen = game::Screen::kPlay;
    using MatchClockFn = void(__thiscall*)(void*, float);
    const auto match_clock = reinterpret_cast<MatchClockFn>(game::sig_one("kSigMercClockHead"));
    cheats::set(sw::kFreezeCountdown, true);
    cheats::tick(run);
    match_clock(gm.mem.data(), 0.25f);
    check(gm.field<float>(gl.match_clock) == 100.0f && gm.field<float>(gl.match_hud) == 100.0f,
          "a match: the game's own clock run-down takes nothing while frozen");
    cheats::Status st = cheats::status();
    check(st.countdown && st.match_clock && st.countdown_left == 100.0f, "... the panel's countdown is the match clock");
    run.screen = game::Screen::kPause;
    cheats::edit(cheats::Edit::kCountdown, 0, 300);
    cheats::tick(run);
    check(gm.field<float>(gl.match_clock) == 300.0f && gm.field<float>(gl.match_hud) == 300.0f,
          "... at the pause: the time left set to 300 s (and the HUD's copy)");
    run.screen = game::Screen::kPlay;
    cheats::set(sw::kFreezeCountdown, false);
    cheats::tick(run);
    match_clock(gm.mem.data(), 0.25f);
    check(gm.field<float>(gl.match_clock) == 299.75f, "off: it runs down again");
    gm.field<uint8_t>(gl.online) = 1;
    run.online = true;
    cheats::set(sw::kFreezeCountdown, true);
    cheats::tick(run);
    match_clock(gm.mem.data(), 0.25f);
    check(gm.field<float>(gl.match_clock) == 299.5f, "online: left alone");
    run.online = false;
    gm.field<uint8_t>(gl.online) = 0;
    // The Mercenaries' switches over the same match (the patches' places found before they go in: a
    // signature does not match its own patched bytes).
    const uintptr_t combo_sub = game::sig_one("kSigMercCombo") + 47, ct_sub = game::sig_one("kSigMercComboTime") + 99;
    const uintptr_t award_site = game::sig_one("kSigComboAwardCall") + 15;
    check(award_site == 0x55C287 && reinterpret_cast<uintptr_t>(merc::g_award) == 0x553590,
          "the kill award (0x553590) and its one call (0x55C287)");
    cheats::set(sw::kEndlessCombo, true);
    cheats::set(sw::kComboTime, true);
    gm.field<int32_t>(merc::g_m.score) = 1000;
    gm.field<int32_t>(merc::g_m.earned) = 0;
    cheats::tick(run);
    check(mem::read<uint8_t>(combo_sub) == 0x90 &&
              game::call_target(award_site) == reinterpret_cast<uintptr_t>(&merc::hk_combo_award),
          "Endless combo: the combo's window held, the kill award's call the mod's");
    // A kill's award through the mod's call, the game's award stood in (it adds to what the combo has earned).
    const auto award_was = merc::g_award;
    merc::g_award = &stand_in_award;
    merc::hk_combo_award(gm.mem.data(), 0, 250);
    check(gm.field<int32_t>(merc::g_m.score) == 1250 && gm.field<int32_t>(merc::g_m.earned) == 0,
          "... a kill's award (250) put on the score as it is earned (1250)");
    gm.field<int32_t>(merc::g_m.score) = merc::g_m.cap - 100;
    merc::hk_combo_award(gm.mem.data(), 0, 250);
    check(gm.field<int32_t>(merc::g_m.score) == merc::g_m.cap, "... at most the game's cap (9,999,999)");
    gm.field<int32_t>(merc::g_m.score) = 1250;
    cheats::tick(run);
    check(gm.field<int32_t>(merc::g_m.score) == 1250, "... and the tick writes nothing");
    check(gm.field<uint8_t>(merc::g_m.ct_running) == 1 && gm.field<float>(merc::g_m.ct_clock) == 30.0f &&
              mem::read<uint8_t>(ct_sub) == 0x90,
          "Combo Time always on: started (30 s), its clock held");
    merc::Status ms = merc::status();
    check(ms.match && ms.offline && ms.score == 1250 && ms.target == 150 && ms.combo_time,
          "the panel's match: on, offline, the score, the target, Combo Time");
    cheats::set(sw::kEndlessCombo, false);
    cheats::set(sw::kComboTime, false);
    cheats::tick(run);
    gm.field<int32_t>(merc::g_m.earned) = 0;
    merc::hk_combo_award(gm.mem.data(), 0, 300);
    check(mem::read<uint8_t>(combo_sub) == 0xF3 && mem::read<uint8_t>(ct_sub) == 0xF3 &&
              game::call_target(award_site) == 0x553590 && gm.field<int32_t>(merc::g_m.earned) == 300 &&
              gm.field<int32_t>(merc::g_m.score) == 1250,
          "off: both clocks run down again, the award's call the game's (what a kill earns waits for the combo's end)");
    merc::g_award = award_was;
    // Kill enemies as they appear: one match long.
    cheats::set(sw::kAutoKill, true);
    cheats::tick(run);
    check(cheats::enabled(sw::kAutoKill), "Kill enemies as they appear: on through the match");
    {
      // The enemies known by their coming and going: each enemy class's start and destructor hooked.
      check(merc::g_starts.size() >= 185 && merc::g_starts.size() == merc::g_dtors.size(),
            "the enemies' start and destructor hooked in every enemy class's vtable (some 190)");
      check(merc::g_lethal_ok, "its hits lethal through the enemies' take-damage (hooked: it asks the mod)");
      run.chars[0].hp = 1200;
      cheats::tick(run);  // the dealer: Leon
      auto start = [&](Obj& e) {
        return through_hook(merc::g_starts, e.mem.data(), reinterpret_cast<uintptr_t>(&stand_in_start),
                            [&] { merc::hk_enemy_start(e.mem.data()); });
      };
      auto destroy = [&](Obj& e) {
        return through_hook(merc::g_dtors, e.mem.data(), reinterpret_cast<uintptr_t>(&stand_in_dtor),
                            [&] { merc::hk_enemy_dtor(e.mem.data(), 1); });
      };
      auto made_up = [&](Obj& e) {
        e.field<uint32_t>(ul.unit_flags) = 2;  // running
        e.field<int16_t>(gl.hp) = e.field<int16_t>(gl.hp_max) = 500;
        e.field<uintptr_t>(merc::g_k.record) = merc::g_k.damage_vt;
        e.field<float>(merc::g_k.coord_pos + 8) = 10.0f;
      };
      auto valid = [&](Obj& e) { return e.field<uint8_t>(merc::g_k.valid); };
      auto damage = [&](Obj& e) { return e.field<int32_t>(merc::g_k.record + merc::g_k.damage); };
      auto part = [&](Obj& e) { return e.field<uint32_t>(merc::g_k.record + merc::g_k.word64); };
      auto record = [&](Obj& e) { return reinterpret_cast<void*>(e.at() + merc::g_k.record); };
      auto foe = [&](Obj& e) { return merc::foe_of(e.at()); };
      auto waited = [&] {  // the waits passed (an enemy tried again after 100 ms, 400 ms, 2 s)
        for (merc::Foe& f : merc::g_foes) f.wait = false;
      };
      // What the game's take-damage does to the mod's hit, as its brain hands it over (the mod's hook asked first).
      auto taken = [&](Obj& e, int amount) { return merc::lethal(e.mem.data(), record(e), amount); };

      Obj em(mt::find("uEm1000")->size, vt_of("uEm1000"));
      made_up(em);
      const bool started = start(em);
      check(started && valid(em) == 0 && merc::g_foes.size() == 1 && foe(em),
            "an enemy coming into play (its start, through the mod's hook): known, its hit left to the next tick");
      cheats::tick(run);
      check(valid(em) == 1 && damage(em) == 500 && part(em) == 0 &&
                em.field<uintptr_t>(merc::g_k.record + merc::g_k.dealer) == leon.at(),
            "... the next tick, before the game's frame (its brain's turn): its hit written - all its 500, dealt by "
            "Leon, on part 0");
      cheats::tick(run);
      check(valid(em) == 0 && foe(em)->unread == 1, "... a hit not taken by the next tick: cleared");
      cheats::tick(run);
      check(valid(em) == 0, "... and not written again at once (an enemy whose hit went unread waits 100 ms)");
      Sleep(150);
      cheats::tick(run);
      check(valid(em) == 1 && foe(em)->writes == 2, "... then written again: an enemy is tried until it is dead");
      // Its take-damage: a hit of the mod's takes all its health, whatever its own damage code made of it.
      Obj game_hit(0x140, 0);
      game_hit.field<int32_t>(gl.hit_damage) = 30;
      game_hit.field<uintptr_t>(gl.hit_attacker) = leon.at();
      check(merc::lethal(em.mem.data(), game_hit.mem.data(), 30) == 30, "its take-damage: a hit of the game's as it is");
      check(taken(em, 0) == 0, "... the mod's worked out as nothing (an armoured part, a moment it cannot be hurt): "
                               "left so at first");
      check(taken(em, 30) == 500,
            "... the mod's worked out as 30 (its own code shrinking it: a limb, Gnezdo's 1%): all its 500");
      cheats::tick(run);
      check(valid(em) == 0 && foe(em)->shrugs == 1 && foe(em)->unread == 1 && !foe(em)->dead,
            "... taken and it lives on (the game kept its life): a shrug, the hit cleared");
      cheats::tick(run);
      check(valid(em) == 0, "... tried again only after 400 ms");
      waited();
      cheats::tick(run);
      check(valid(em) == 1 && damage(em) == cheats::kBigHit && part(em) == 0,
            "... then hit harder: 32767 (the most health a character holds)");
      for (int i = 0; i < 2; ++i) {  // two more taken, it alive after
        taken(em, 0);
        cheats::tick(run);
        waited();
        cheats::tick(run);
      }
      check(foe(em)->shrugs == 3 && valid(em) == 1 && taken(em, 0) == 500,
            "... after three: its take-damage takes all its health even from nothing");
      cheats::tick(run);
      waited();
      cheats::tick(run);
      check(foe(em)->shrugs == 4 && part(em) == 1, "... and the hit lands on another part of it each time (part 1)");
      // It dies of it: its take-damage took the mod's hit and its health is gone (the game's own does that; here by
      // hand, below through the game's own).
      taken(em, 500);
      em.field<int16_t>(gl.hp) = 0;
      const int killed_was = merc::g_killed;
      cheats::tick(run);
      check(foe(em)->dead && merc::g_killed == killed_was + 1 && valid(em) == 0,
            "dead: counted as the mod's kill");
      waited();
      cheats::tick(run);
      check(valid(em) == 0, "... and never hit again");
      check(destroy(em) && merc::g_foes.empty(), "... the enemy destroyed (through the mod's hook): forgotten");

      // Through the game's own take-damage (uEnemy's, behind the mod's hook in a zombie's vtable): the system whose
      // slot 25 answers whether an enemy may die made up at the game's global (yes).
      struct MayDie {
        static bool __thiscall yes(void*, void*) { return true; }
      };
      static void* may_vt[32] = {};
      may_vt[25] = reinterpret_cast<void*>(&MayDie::yes);
      static void* may_obj = may_vt;
      const uint32_t kMayG = 0x17CF40C, may_was = mem::read<uint32_t>(kMayG);
      mem::write<uintptr_t>(kMayG, reinterpret_cast<uintptr_t>(&may_obj));
      using EnemyFn = void(__thiscall*)(void*, void*, int);
      Obj zb(mt::find("uEm5200")->size, vt_of("uEm5200"));
      made_up(zb);
      const auto zslot = reinterpret_cast<EnemyFn>(mem::read<uintptr_t>(vt_of("uEm5200") + 4 * game::kEnemyApplySlot));
      check(reinterpret_cast<uintptr_t>(zslot) == reinterpret_cast<uintptr_t>(&cheats::hk_enemy_apply),
            "a zombie's take-damage (uEm5200) is the mod's, the game's own behind it");
      start(zb);
      cheats::tick(run);
      zslot(zb.mem.data(), record(zb), 30);
      check(zb.field<int16_t>(gl.hp) == 0 && zb.field<uint8_t>(0xF24) == 1,
            "the game's own take-damage given the mod's hit worked out as 30: health 500 to 0, marked dead");
      cheats::tick(run);
      check(foe(zb)->dead && merc::g_killed == killed_was + 2, "... the mod's kill");
      zb.field<int16_t>(gl.hp) = 500;
      zb.field<uint8_t>(0xF24) = 0;
      zslot(zb.mem.data(), game_hit.mem.data(), 30);
      check(zb.field<int16_t>(gl.hp) == 470, "... a hit of the game's takes its own 30");
      destroy(zb);
      mem::write<uint32_t>(kMayG, may_was);

      // Every enemy in play is tried, however many (before 2026-11-14 the first four as the switch went on, and only
      // they), and one not ready at first (its record holding a hit of the game's) once it is.
      cheats::set(sw::kAutoKill, false);
      cheats::tick(run);
      std::vector<std::unique_ptr<Obj>> crowd;
      for (int i = 0; i < 10; ++i) {
        crowd.push_back(std::make_unique<Obj>(mt::find("uEm1000")->size, vt_of("uEm1000")));
        made_up(*crowd.back());
        start(*crowd.back());
      }
      crowd[9]->field<uint8_t>(merc::g_k.valid) = 1;  // a hit of the game's waiting for its brain
      int written = 0;
      for (auto& e : crowd) written += valid(*e);
      check(written == 1 && merc::g_foes.size() == 10, "off: ten enemies coming into play - known, no hit");
      cheats::set(sw::kAutoKill, true);
      cheats::tick(run);
      written = 0;
      for (int i = 0; i < 9; ++i) written += valid(*crowd[i]) && damage(*crowd[i]) == 500;
      check(written == 8, "on again: the enemies in play hit, eight a tick");
      cheats::tick(run);
      written = 0;
      for (int i = 0; i < 9; ++i) written += foe(*crowd[i])->writes;
      check(written == 9 && valid(*crowd[8]) == 1, "... the ninth at the next tick (each enemy its turn)");
      crowd[9]->field<uint8_t>(merc::g_k.valid) = 0;  // the game's hit taken
      waited();
      cheats::tick(run);
      check(valid(*crowd[9]) == 1 && damage(*crowd[9]) == 500 && foe(*crowd[9])->writes == 1,
            "... the one whose record was busy hit once it is free");
      // At the pause nothing is written (nothing would read it), and a hit left over is cleared, not counted.
      waited();
      run.paused = true;
      cheats::tick(run);
      written = 0;
      for (auto& e : crowd) written += valid(*e);
      const int unread_was = foe(*crowd[9])->unread;
      check(written == 0, "at the pause: the last tick's hits cleared, none written");
      cheats::tick(run);
      check(foe(*crowd[9])->unread == unread_was, "... and none counted as unread");
      run.paused = false;
      cheats::tick(run);
      written = 0;
      for (auto& e : crowd) written += valid(*e);
      check(written == 8, "... in play again: hit again");
      merc::Status ks = merc::status();
      check(ks.killing == 10 && ks.killed == merc::g_killed, "the panel: ten in play, the kills counted");
      for (auto& e : crowd) destroy(*e);
      check(merc::g_foes.empty() && merc::g_npending == 0, "... and forgotten as they go");

      // A J'avo's mutation: its part's record taken by the J'avo's own take-damage (its brain reads its parts') - the
      // J'avo dies of it; the part is not hit again while the J'avo is about, again once it has gone.
      Obj javo(mt::find("uEm1000")->size, vt_of("uEm1000")), part_(mt::find("uEm2000")->size, vt_of("uEm2000"));
      made_up(javo);
      made_up(part_);
      start(javo);
      start(part_);
      cheats::tick(run);
      check(valid(javo) == 1 && valid(part_) == 1, "a J'avo and its mutation's part: both hit");
      const int killed_before = merc::g_killed;
      check(merc::lethal(javo.mem.data(), record(part_), 30) == 500, "... the part's hit taken by the J'avo: all its 500");
      javo.field<int16_t>(gl.hp) = 0;
      cheats::tick(run);
      check(foe(part_)->owner == javo.at() && foe(part_)->shrugs == 0 && foe(javo)->dead &&
                merc::g_killed == killed_before + 1,
            "... the J'avo dead: the mod's kill; the part known as the J'avo's (no shrug of its own)");
      waited();
      cheats::tick(run);
      check(valid(part_) == 0 && merc::status().killing == 0,
            "... the part not hit again while the J'avo is about, nor counted in play");
      destroy(javo);
      waited();
      cheats::tick(run);
      check(foe(part_)->owner == 0 && valid(part_) == 1, "... the J'avo gone: the part on its own, hit again");
      destroy(part_);

      // A body, while on: once its kill has counted (reported), ended at once - its end byte set (what its erase sets
      // last), and the game's own end step of the enemies' brains (uEmIntel's, run in the copy over a made-up brain:
      // in mode 3 its own work is none) ends it - both mRno 3. Not one whose kill is not reported, a live one, a part
      // of another; nothing at the pause; off, a body plays its own death.
      check(merc::g_k.ended == 0x3B30 && merc::g_brain_end == 0x942B10,
            "a body's end: the brains' end step (0x942B10, uEmIntel's) ends an enemy whose byte +0x3B30 is set");
      Obj body(mt::find("uEm1200")->size, vt_of("uEm1200")), unscored(mt::find("uEm1200")->size, vt_of("uEm1200")),
          living(mt::find("uEm1200")->size, vt_of("uEm1200"));
      for (Obj* e : {&body, &unscored, &living}) {
        made_up(*e);
        start(*e);
      }
      cheats::tick(run);  // each hit
      body.field<int16_t>(gl.hp) = 0;
      body.field<uint8_t>(merc::g_k.reported) = 1;  // dead, its kill counted
      unscored.field<int16_t>(gl.hp) = 0;           // dead, its kill not reported (a chrysalid's J'avo)
      auto ended = [&](Obj& e) { return e.field<uint8_t>(merc::g_k.ended); };
      run.paused = true;
      cheats::tick(run);
      check(ended(body) == 0 && foe(body)->dead, "at the pause: a body seen dead, left as it is");
      run.paused = false;
      const int ended_was = merc::g_ended;
      cheats::tick(run);
      check(ended(body) == 1 && foe(body)->ended && merc::g_ended == ended_was + 1,
            "on: a body whose kill has counted ended at once (its end byte set)");
      check(ended(unscored) == 0 && ended(living) == 0,
            "... not one whose kill is not reported, nor one alive");
      Obj brain(mt::find("uEm1000Intel")->size, vt_of("uEm1000Intel"));
      brain.field<uint32_t>(0x44) = 3;  // its mode: no work of its own here
      brain.field<uintptr_t>(0x40) = body.at();
      using BrainFn = void(__thiscall*)(void*);
      reinterpret_cast<BrainFn>(merc::g_brain_end)(brain.mem.data());
      check(brain.field<uint32_t>(0x38) == 3 && body.field<uint32_t>(0x38) == 3,
            "... its brain's own end step ends it: the brain's mRno and its own 3 (the unit ends)");
      brain.field<uintptr_t>(0x40) = living.at();
      reinterpret_cast<BrainFn>(merc::g_brain_end)(brain.mem.data());
      check(living.field<uint32_t>(0x38) == 0, "... a live one's brain: nothing ended");
      cheats::tick(run);
      check(ended(body) == 1 && merc::g_ended == ended_was + 1, "... and a body ended once");
      Obj piece(mt::find("uEm2000")->size, vt_of("uEm2000"));
      made_up(piece);
      start(piece);
      foe(piece)->owner = living.at();  // a part of a J'avo still about
      piece.field<int16_t>(gl.hp) = 0;
      piece.field<uint8_t>(merc::g_k.reported) = 1;
      cheats::tick(run);
      check(ended(piece) == 0, "... nor a part of another enemy (its J'avo's to end)");
      cheats::set(sw::kAutoKill, false);
      cheats::tick(run);
      Obj later(mt::find("uEm1200")->size, vt_of("uEm1200"));
      made_up(later);
      start(later);
      later.field<int16_t>(gl.hp) = 0;
      later.field<uint8_t>(merc::g_k.reported) = 1;
      cheats::tick(run);
      cheats::tick(run);
      check(ended(later) == 0, "off: a body plays its own death (its motion, its erase)");
      for (Obj* e : {&body, &unscored, &living, &piece, &later}) destroy(*e);
      cheats::set(sw::kAutoKill, true);
      cheats::tick(run);

      // The flow: a body holds its place in its stage's count until its erase has run; while Kill enemies as they
      // appear acts, the erases of the kinds that fill the maps load short waits (each family's own loads re-pointed),
      // and AreaHit's area test is the mod's.
      struct Load {
        uintptr_t at;
        float was;
        const float* fast;
      };
      const Load loads[] = {
          {0x6BE413, 7.0f, &merc::kFastSeconds},   // the J'avo's dissolve (cEm1000ActDeadErase's start)
          {0x7A4E9E, 70.0f, &merc::kFastFrames},   // a zombie lying (cEm5000ActDeadErase's frame) ...
          {0x7A4E83, 8.0f, &merc::kFastSeconds},   // ... then dissolving
          {0x80DCC8, 2.0f, &merc::kFastSeconds},   // a dog lying (cEm5700ActDeadErase's start) ...
          {0x8091E9, 5.0f, &merc::kFastSeconds},   // ... then dissolving (its frame)
          {0x80DDBA, 7.0f, &merc::kFastSeconds},   // a frozen dog (cEm5700ActDeadEraseIce's start)
          {0x70CA14, 420.0f, &merc::kFastFrames},  // Napad (cEm3200ActErase's start, in frames)
          {0x70CAE2, 420.0f, &merc::kFastFrames},
          {0x70CB48, 300.0f, &merc::kFastFrames},
          {0x7FFA18, 2.0f, &merc::kFastSeconds},   // the Whopper lying (cEm5600ActDeadErase's start) ...
          {0x7FCA32, 3.0f, &merc::kFastSeconds},   // ... (its frame)
          {0x7FCAA1, 7.0f, &merc::kFastSeconds},   // ... then dissolving
      };
      auto loads_from = [&](uintptr_t at) { return mem::read<uintptr_t>(at + 4); };
      bool found = merc::g_erase_loads.size() == std::size(loads);
      for (const Load& l : loads) {
        bool here = false;
        for (const mem::Patch& p : merc::g_erase_loads) here = here || p.at == l.at;
        found = found && here;
      }
      check(found, "the erases' 12 waits found where the build has them (the J'avo's, a zombie's, a dog's, Napad's, "
                   "the Whopper's)");
      bool fast = true;
      for (const Load& l : loads)
        fast = fast && loads_from(l.at) == reinterpret_cast<uintptr_t>(l.fast) && mem::read<uint8_t>(l.at) == 0xF3;
      check(fast && merc::kFastSeconds == 0.25f && merc::kFastFrames == 8.0f,
            "on, in a match played here: each load the mod's short wait (0.25 s, 8 frames)");
      const uintptr_t area_call = game::sig_one("kSigAreaHitInside") + 17;
      check(area_call == 0xD5A561 && reinterpret_cast<uintptr_t>(merc::g_inside_fn) == 0xE19030,
            "AreaHit's area test (0xD5A561 -> 0xE19030)");
      check(game::call_target(area_call) == reinterpret_cast<uintptr_t>(&merc::hk_area_inside) && merc::g_flowing,
            "... the mod's while on");
      Obj task(0x40, 0);
      check(merc::hk_area_inside(task.mem.data()), "AreaHit: the player in every area");
      cheats::set(sw::kAutoKill, false);
      cheats::tick(run);
      bool back = !merc::g_flowing;
      for (const Load& l : loads) {
        float v = 0;
        back = back && mem::read_safe(loads_from(l.at), &v) && v == l.was;
      }
      check(back && game::call_target(area_call) == 0xE19030,
            "off: each load the game's own wait again (7.0, 70, 8.0, 2.0, 5.0, 7.0, 420, 420, 300, 2.0, 3.0, 7.0), the "
            "area test the game's");
      check(!merc::hk_area_inside(task.mem.data()), "... and its own answer: nobody in a task with no shape");
      cheats::set(sw::kAutoKill, true);
      cheats::tick(run);
      // A stage's senders: each one's stock also takes the groups of every sender before it in its script this match
      // (what one left unsent is not stranded; a boss's own script keeps its groups) - the game's builder stood in,
      // seeing the param as the mod hands it on.
      const uintptr_t stock_call = game::sig_one("kSigEnemyAddStock") + 11;
      check(stock_call == 0xD593F5 && reinterpret_cast<uintptr_t>(merc::g_stock_fn) == 0xD46150 &&
                merc::g_stock_type == 0x1C && merc::g_stock_groups == 0x38,
            "a sender's stock builder (0xD593F5 -> 0xD46150): its param's type +0x1C, groups +0x38");
      check(game::call_target(stock_call) == reinterpret_cast<uintptr_t>(&merc::hk_build_stock),
            "... on: the builder's call the mod's");
      static uint32_t s_seen_groups = 0;
      struct Stand {
        static void __thiscall build(void*, void*, void*, void* param) {
          s_seen_groups = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param) + merc::g_stock_groups);
        }
      };
      const auto stock_was = merc::g_stock_fn;
      merc::g_stock_fn = &Stand::build;
      Obj spawner(0x10, 0), boss(0x10, 0);  // two scripts' orders' owners: the stage's spawner (EmSet), a boss's own
      merc::g_sent_count = 0;
      Obj pa(0x54, 0), pb(0x54, 0), pc(0x54, 0), pd(0x54, 0);
      pa.field<uint32_t>(0x1C) = pb.field<uint32_t>(0x1C) = pd.field<uint32_t>(0x1C) = 2;
      pa.field<uint32_t>(0x38) = 0x7;     // groups 0-2
      pb.field<uint32_t>(0x38) = 0x1E;    // groups 1-4 (not 0)
      pd.field<uint32_t>(0x38) = 0x8000;  // group 15: a boss's (Em_BossPop_1st)
      merc::hk_build_stock(spawner.mem.data(), nullptr, nullptr, pa.mem.data());
      check(s_seen_groups == 0x7 && merc::g_sent_count == 1 && merc::g_sent[0].groups == 0x7,
            "the first sender (groups 0-2): its own groups");
      merc::hk_build_stock(boss.mem.data(), nullptr, nullptr, pd.mem.data());
      check(s_seen_groups == 0x8000 && merc::g_sent_count == 2,
            "a boss's sender, in a script of its own (group 15): its own group only - not the spawner's");
      merc::hk_build_stock(spawner.mem.data(), nullptr, nullptr, pb.mem.data());
      check(s_seen_groups == 0x1F && pb.field<uint32_t>(0x38) == 0x1E,
            "the spawner's next (groups 1-4): stocked with group 0 too, which the first may have left unsent - not "
            "the boss's group; its own field put back");
      pc.field<uint32_t>(0x1C) = 0;  // a list of its own
      pc.field<uint32_t>(0x38) = 0x100;
      merc::hk_build_stock(spawner.mem.data(), nullptr, nullptr, pc.mem.data());
      check(s_seen_groups == 0x100, "a sender stocking a list of its own (type 0): as it is");
      cheats::set(sw::kAutoKill, false);
      cheats::tick(run);
      check(game::call_target(stock_call) == 0xD46150, "off: the builder's call the game's again");
      merc::hk_build_stock(spawner.mem.data(), nullptr, nullptr, pb.mem.data());
      check(s_seen_groups == 0x1E, "... and a sender's own groups only");
      merc::g_stock_fn = stock_was;
      cheats::set(sw::kAutoKill, true);
      cheats::tick(run);
      // A sender's wait: the game's own EnemyAdd frame (0xD59380) run in the copy over a made-up script (its orders'
      // works: one), the sender's work (stocked, sending; its timer) and param (a wait of 2.0 s, at most 5 in play; no
      // count kind, so none in play; nothing to send - sEnemy's is not made up - so the send step shows by the wait it
      // reloads). On, a sender whose wait still runs sends at once; at its minimum it does not; off, its wait runs.
      const uintptr_t wait_test = 0xD59611;  // (its signature, kSigEnemyAddWait +21, holds the jbe: not while on)
      check(merc::g_wait_jump.at == wait_test && merc::g_wait_timer == 0x38 &&
                merc::g_wait_param == 0x44 && merc::g_wait_min == 0x4C,
            "a sender's wait test (0xD59611): the work's timer +0x38, from the param's mWaitTimer +0x44; mMinEmNum "
            "+0x4C");
      check(mem::read<uint8_t>(wait_test) == 0xEB && mem::read<uint8_t>(wait_test + 1) == 0x14,
            "... on: its jbe to the send a jmp");
      struct Order {
        static float __thiscall frame_time(void*) { return 0.5f; }
      };
      static uintptr_t s_order_vt[16] = {};
      s_order_vt[8] = reinterpret_cast<uintptr_t>(&Order::frame_time);  // the order's slot +0x20: the frame's time
      Obj script(mt::find("cFSMOrder")->size, reinterpret_cast<uintptr_t>(s_order_vt));
      Obj wparam(mt::find("cFSMOrder::FuncParamAddEnemy")->size, 0), work(0x40, 0);
      uintptr_t works[1] = {work.at()};
      script.field<uint32_t>(0x9D4) = 1;
      script.field<uintptr_t>(0x9E0) = reinterpret_cast<uintptr_t>(works);
      work.field<uintptr_t>(4) = wparam.at();
      work.field<uint8_t>(0x3C) = 1;  // stocked: sending
      wparam.field<uint32_t>(0x14) = 0x3C;
      wparam.field<uint32_t>(0x18) = 2;   // mCheckType: none of the counts
      wparam.field<uint32_t>(0x3C) = 32;  // mRemainEmNum: no FreeS32
      wparam.field<float>(0x44) = 2.0f;   // mWaitTimer (mWaitRandom 0)
      wparam.field<uint32_t>(0x4C) = 5;   // mMinEmNum
      std::vector<uint8_t> host(0x1000, 0);  // sGameMan for the frame's test of the host: this machine
      const uint32_t kGameManG = 0x17CF454, gm_was = mem::read<uint32_t>(kGameManG);
      mem::write<uintptr_t>(kGameManG, reinterpret_cast<uintptr_t>(host.data()));
      using AddFrame = int(__thiscall*)(void*, void*, void*);
      const auto add_frame = reinterpret_cast<AddFrame>(0xD59380);
      work.field<float>(0x38) = 1.5f;
      add_frame(script.mem.data(), wparam.mem.data(), nullptr);
      check(work.field<float>(0x38) == 2.0f,
            "on: a sender 1.5 s from its next, fewer than its minimum in play, sends at once (its send step run: its "
            "wait of 2.0 s reloaded)");
      wparam.field<uint32_t>(0x4C) = 0;
      add_frame(script.mem.data(), wparam.mem.data(), nullptr);
      check(work.field<float>(0x38) == 2.0f, "... with as many in play as its minimum: no send, no wait run (its "
                                             "limit the stage's)");
      wparam.field<uint32_t>(0x4C) = 5;
      cheats::set(sw::kAutoKill, false);
      cheats::tick(run);
      check(mem::read<uint8_t>(wait_test) == 0x76, "off: the jbe the game's");
      work.field<float>(0x38) = 1.5f;
      add_frame(script.mem.data(), wparam.mem.data(), nullptr);
      check(work.field<float>(0x38) == 1.0f, "... its wait run by the frame's time (1.5 s, less 0.5), nothing sent");
      mem::write<uint32_t>(kGameManG, gm_was);
      cheats::set(sw::kAutoKill, true);
      cheats::tick(run);
    }
    gm.field<int32_t>(gl.match_step) = 2;  // time up
    cheats::tick(run);
    check(!cheats::enabled(sw::kAutoKill), "... switched off as the match ends");
    gm.field<int32_t>(gl.match_step) = 1;
    cheats::tick(run);
    // A stage's countdown: a made-up stage, its timer 3 running down; a display's read comes to the mod.
    run.mode = 0;
    gm.field<uint32_t>(gl.mode) = 0;
    Obj stage(mt::find("aStageBase")->size, vt_of("aStageBase"));
    stage.field<uint32_t>(gl.timers_running) = 1u << 3;
    stage.field<uint32_t>(gl.timers_down) = 1u << 3;
    stage.field<float>(gl.timers + 4 * 3) = 45.0f;
    const uintptr_t read_site = game::sig_one("kSigCountDown") + 126;
    check(game::call_target(read_site) == reinterpret_cast<uintptr_t>(&cheats::hk_countdown_stage),
          "the CountDown's read of a stage timer comes to the mod");
    using GetFn = float(__thiscall*)(void*, uint32_t);
    const float got = reinterpret_cast<GetFn>(game::call_target(read_site))(stage.mem.data(), 3);
    check(got == 45.0f, "... and reads on: the game's own read gives 45 s");
    cheats::set(sw::kFreezeCountdown, true);
    cheats::tick(run);
    stage.field<float>(gl.timers + 4 * 3) = 44.5f;  // the stage's frame ran it down
    reinterpret_cast<GetFn>(game::call_target(read_site))(stage.mem.data(), 3);
    cheats::tick(run);
    check(stage.field<float>(gl.timers + 4 * 3) == 45.0f, "frozen: the shown timer held at 45 s");
    st = cheats::status();
    check(st.countdown && !st.match_clock && st.countdown_left == 45.0f, "the panel's countdown is the stage's");
    // As the game goes: a frame (the stage's run-down, the display's read), then the tick before the next.
    const auto frame = [&](float t) {
      stage.field<float>(gl.timers + 4 * 3) = t;
      reinterpret_cast<GetFn>(game::call_target(read_site))(stage.mem.data(), 3);
    };
    frame(44.5f);
    run.screen = game::Screen::kPause;
    cheats::edit(cheats::Edit::kCountdown, 0, 120);
    cheats::tick(run);
    check(stage.field<float>(gl.timers + 4 * 3) == 120.0f, "at the pause: its time left set to 120 s");
    run.screen = game::Screen::kPlay;
    cheats::tick(run);
    frame(119.5f);
    cheats::tick(run);
    check(stage.field<float>(gl.timers + 4 * 3) == 120.0f, "... and held there");
    cheats::set(sw::kFreezeCountdown, false);
    cheats::tick(run);
    stage.field<float>(gl.timers + 4 * 3) = 119.5f;
    reinterpret_cast<GetFn>(game::call_target(read_site))(stage.mem.data(), 3);
    cheats::tick(run);
    check(stage.field<float>(gl.timers + 4 * 3) == 119.5f, "off: it runs down");

    // The SmallTimer's read of the same timer: a gauge, held only counting down - every gauge in the game counts an
    // action's progress up (stage 302's bombs being set, 60 s), left to run.
    const uintptr_t gauge_site = game::sig_one("kSigSmallTimerRead") + 39;
    check(game::call_target(gauge_site) == reinterpret_cast<uintptr_t>(&cheats::hk_gauge_stage),
          "the SmallTimer's read of a stage timer comes to the mod");
    cheats::set(sw::kFreezeCountdown, true);
    cheats::tick(run);
    stage.field<uint32_t>(gl.timers_down) = 0;  // counting up
    const auto gauge = [&](float t) {
      stage.field<float>(gl.timers + 4 * 3) = t;
      reinterpret_cast<GetFn>(game::call_target(gauge_site))(stage.mem.data(), 3);
      cheats::tick(run);
    };
    gauge(10.0f);
    gauge(10.25f);
    check(stage.field<float>(gl.timers + 4 * 3) == 10.25f, "on: a gauge counting up (a bomb being set) left to run");
    // The CountDown showing a timer counting up to its end (stage 1003's: 2301 to 2508, a Game Over there): held.
    const auto shown_up = [&](float t) {
      frame(t);
      cheats::tick(run);
    };
    shown_up(2301.0f);
    shown_up(2301.25f);
    check(stage.field<float>(gl.timers + 4 * 3) == 2301.0f, "the CountDown's timer counting up to its end: held (2301)");
    shown_up(2290.0f);  // the game puts it back: time given back
    shown_up(2290.25f);
    check(stage.field<float>(gl.timers + 4 * 3) == 2290.0f, "... time the game gives back taken up (2290)");
    st = cheats::status();
    check(!st.countdown, "... and no time-left editor for it (its value is no time left)");
    cheats::set(sw::kFreezeCountdown, false);
    cheats::tick(run);
    stage.field<uint32_t>(gl.timers_down) = 1u << 3;

    // An FSM's own countdown (stage 851's SN_140_countdown: 237 s, a Game Over at 0): a made-up cFSMOrder, its timer
    // 0 started by the game's own Timer order, run down by the game's own loop, shown by the game's own CountDown
    // order on a made-up stage's display - its in-place read rewritten into a call of the mod's.
    check(gl.own_timers == 0x930 && gl.own_running == 0xA4 && gl.own_down == 0xA8 && gl.nown == 32,
          "an FSM's 32 own timers +0x930 (running +0xA4, counting down +0xA8)");
    const uintptr_t cd = game::sig_one("kSigCountDown"), own_site = cheats::g_own_read.at;
    const uint8_t own_bytes[] = {0x0F, 0xB6, 0x46, 0x07, 0x50, 0x8B, 0xCA, 0xE8};
    check(own_site == cd + 137 && cheats::g_own_read.applied && !std::memcmp(reinterpret_cast<const void*>(own_site),
                                                                             own_bytes, sizeof(own_bytes)) &&
              game::call_target(own_site + 7) == reinterpret_cast<uintptr_t>(&cheats::hk_countdown_own) &&
              mem::read<uint32_t>(own_site + 12) == 0x14245CD9,
          "the CountDown's in-place read of its FSM's own timer (exe+0x93F929) made a call of the mod's");
    const uint32_t main_g = mem::read<uint32_t>(cd + 27), at_stage = mem::read<uint32_t>(cd + 33),
                   at_display = mem::read<uint32_t>(cd + 44);
    check(main_g == 0x186E1F8 && at_stage == 0x3830 && at_display == 0xB80,
          "the CountDown's display: the stage's ([0x186E1F8] +0x3830) +0xB80");
    Obj holder(at_stage + 4, 0), stage2(mt::find("aStageBase")->size, vt_of("aStageBase")),
        display(mt::find("uIdStageTimer")->size, vt_of("uIdStageTimer")),
        fsm(mt::find("cFSMOrder")->size, vt_of("cFSMOrder"));
    holder.field<uintptr_t>(at_stage) = stage2.at();
    stage2.field<uintptr_t>(at_display) = display.at();
    *reinterpret_cast<uintptr_t*>(main_g) = holder.at();
    using OrderFn = int(__thiscall*)(void*, void*, int);
    using OwnRunFn = void(__thiscall*)(void*, float);
    const uintptr_t timer_fn = 0xD507B0;  // the Timer order (its registration names it; no signature in the mod)
    const uint8_t timer_head[] = {0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0xF9};
    check(!std::memcmp(reinterpret_cast<const void*>(timer_fn), timer_head, sizeof(timer_head)),
          "the game's Timer order (0xD507B0)");
    const auto timer_order = reinterpret_cast<OrderFn>(timer_fn);
    const auto countdown_order = reinterpret_cast<OrderFn>(cd);
    const auto own_run = reinterpret_cast<OwnRunFn>(game::sig_one("kSigOwnTimers"));
    // The two orders' parameters as SN_140_countdown has them.
    alignas(4) uint8_t timer_prm[0x40] = {};
    timer_prm[4] = 2;  // run, counting down
    timer_prm[5] = 0;  // the FSM's own timer
    timer_prm[6] = 0;  // number 0
    timer_prm[7] = 1;  // its start set
    const float start = 237.0f, coef = 1.0f;
    std::memcpy(timer_prm + 0xC, &start, 4);
    alignas(4) uint8_t cd_prm[0x10] = {};  // shown (0), in seconds (0), the FSM's own (0) timer 0
    std::memcpy(cd_prm + 8, &coef, 4);
    timer_order(fsm.mem.data(), timer_prm, 0);
    check(fsm.field<float>(gl.own_timers) == 237.0f && (fsm.field<uint32_t>(gl.own_running) & 1) &&
              (fsm.field<uint32_t>(gl.own_down) & 1),
          "the game's own Timer order: the FSM's timer 0 at 237 s, running, counting down");
    // A frame as the game runs it: the FSM's timers run (a quarter of a second), its CountDown shows timer 0.
    const auto own_frame = [&]() {
      own_run(fsm.mem.data(), 0.25f);
      countdown_order(fsm.mem.data(), cd_prm, 0);
      cheats::tick(run);
    };
    own_frame();
    check(fsm.field<float>(gl.own_timers) == 236.75f && display.field<uint8_t>(0x160) == 1 &&
              display.field<int32_t>(0x220) == 0,
          "off: the game's own loop runs it down (236.75 s), its CountDown shows it (the display up, in seconds)");
    const int32_t running_shown = display.field<int32_t>(0x224);
    cheats::set(sw::kFreezeCountdown, true);
    cheats::tick(run);
    own_frame();  // held from here: 236.5
    own_frame();
    const int32_t held_shown = display.field<int32_t>(0x224);
    own_frame();
    check(fsm.field<float>(gl.own_timers) == 236.5f && display.field<int32_t>(0x224) == held_shown &&
              held_shown != running_shown,
          "frozen: the FSM's own timer held at 236.5 s through the game's own run-down, the display standing still");
    st = cheats::status();
    check(st.countdown && !st.match_clock && st.countdown_left == 236.5f, "the panel's countdown is the FSM's");
    run.screen = game::Screen::kPause;
    cheats::edit(cheats::Edit::kCountdown, 0, 120);
    cheats::tick(run);
    check(fsm.field<float>(gl.own_timers) == 120.0f, "at the pause: its time left set to 120 s");
    run.screen = game::Screen::kPlay;
    cheats::tick(run);
    own_frame();
    own_frame();
    check(fsm.field<float>(gl.own_timers) == 120.0f, "... and held there");
    for (uint32_t i = 0; i <= cheats::kHoldLapse; ++i) cheats::tick(run);  // hidden a while
    fsm.field<float>(gl.own_timers) = 40.0f;                                // started again, lower
    own_frame();
    own_frame();
    check(fsm.field<float>(gl.own_timers) == 39.75f,
          "shown again after a while on a fresh start: held where it starts again, not at the old hold");
    cheats::set(sw::kFreezeCountdown, false);
    cheats::tick(run);
    own_frame();
    check(fsm.field<float>(gl.own_timers) == 39.5f, "off: it runs down");
    *reinterpret_cast<uintptr_t*>(main_g) = 0;
    game::g_sgameman = 0;
    run.mode_known = false;
  }

  std::printf("-- Add and Throw away (the inventory's own functions)\n");
  {
    // Leon's inventory as the game lays it out: 24 items, item i in slot i, none in use.
    for (int i = 0; i < gl.nitems; ++i) {
      const uintptr_t it = gl.inventory + i * gl.item_size;
      std::memset(leon.mem.data() + it, 0, gl.item_size);
      leon.field<uint8_t>(it + gl.it_slot) = static_cast<uint8_t>(i);
    }
    leon.field<int32_t>(gl.in_hand) = 0;
    leon.field<uint32_t>(gl.control) = 0;
    // Leon holds the Wing Shooter (weapon slot 0): the ammo Add offers him is what it loads, 9mm.
    leon.field<uint8_t>(gl.inventory + gl.it_used) = 1;
    leon.field<uint16_t>(gl.inventory + gl.it_id) = 0x103;
    inventory::install();
    check(inventory::g_set_slot && inventory::g_first_summary == 8 && inventory::g_first_case == 13,
          "set-slot, top-up, refresh and clear found; the summary slots 8-12, the case from 13");
    main_obj.field<uint8_t>(gl.paused) = 1;
    game_tick(main_obj.mem.data(), &r);
    inventory::add(0, 0x201, 40);
    inventory::tick(r);
    auto total = [&](int id, int* stacks) {
      int n = 0;
      *stacks = 0;
      for (int i = 0; i < gl.nitems; ++i) {
        const uintptr_t it = gl.inventory + i * gl.item_size;
        if (leon.field<uint8_t>(it + gl.it_used) && leon.field<uint16_t>(it + gl.it_id) == id) {
          n += leon.field<int16_t>(it + gl.it_num);
          ++*stacks;
        }
      }
      return n;
    };
    int stacks = 0;
    check(total(0x201, &stacks) == 40 && stacks == 1, "Add 40 9mm Ammo: one stack of 40 (the game's own set-slot)");
    inventory::add(0, 0x201, 150);
    inventory::tick(r);
    check(total(0x201, &stacks) == 190 && stacks == 2, "Add 150 more: the stack topped up to 150, the rest a new one");
    inventory::View v = inventory::view();
    const inventory::Case& lc = v.cases[0];
    int slot = -1, n = 0;
    for (int i = 0; i < lc.nitems; ++i)
      if (lc.items[i].id == 0x201 && lc.items[i].in_case) {
        slot = lc.items[i].slot;
        n = lc.items[i].num[0];
        break;
      }
    check(slot >= 13 && n > 0, "the view: the stacks in the case");
    inventory::throw_away(0, slot, 0x201);
    inventory::tick(r);
    check(total(0x201, &stacks) == 190 - n && stacks == 1, "Throw away: that stack gone (the game's own clear)");
    inventory::add(0, 0x303, 2);
    inventory::tick(r);
    check(total(0x303, &stacks) == 2 && stacks == 2, "Add 2 green herbs: one a slot");
    inventory::add(0, 0x202, 10);  // 12-gauge shells: no weapon of Leon's loads them
    inventory::add(0, 0x104, 1);   // a weapon: Add offers none (the Records WEAPONS window gives them)
    inventory::tick(r);
    check(total(0x202, &stacks) == 0 && total(0x104, &stacks) == 0 && !inventory::view().loading &&
              std::strstr(inventory::view().note, "Nothing Leon holds takes 12-Gauge Shells"),
          "Add of 12-gauge shells (no shotgun held): none, and said so; of a weapon (the shotgun): nothing");
    inventory::add(0, 0x116, 2);  // remote bombs: no list of the loader holds them (no sLoadMan here yet)
    inventory::tick(r);
    check(total(0x116, &stacks) == 0 && inventory::view().loading,
          "a grenade whose archive is not loaded: not added at once - it waits for the loader");
    main_obj.field<uint8_t>(gl.paused) = 0;
    game_tick(main_obj.mem.data(), &r);
    inventory::add(0, 0x201, 10);
    inventory::tick(r);
    check(total(0x201, &stacks) == 190 - n, "in play: nothing added");
    check(!inventory::view().loading && inventory::view().note_chara == 0 && std::strstr(inventory::view().note, "pause closed"),
          "... and the remote bombs' Add dropped as the pause closed (its note says so)");
    main_obj.field<uint8_t>(gl.paused) = 1;
    game_tick(main_obj.mem.data(), &r);
  }

  std::printf("-- the game's loader: its own request, commit and frame over a made-up sLoadMan\n");
  {
    const inventory::Loader& ld = inventory::g_ld;
    check(inventory::g_request && inventory::g_commit && inventory::g_frame &&
              inventory::g_frame == reinterpret_cast<inventory::FrameFn>(mem::read<uintptr_t>(vt_of("sLoadMan") + 6 * 4)),
          "the loader's request, commit and frame found (the frame sLoadMan's vtable slot 6)");
    check(ld.lists == 9 && ld.count == 0x30400 && ld.items == 0x3040C && ld.stride == 0x14 && ld.current == 0x304B0,
          "9 lists (counts sLoadMan +0x30400, items +0x3040C, 0x14 apart), the one added to +0x304B0");
    check(ld.path == 4 && ld.chars == 8 && ld.archive == 8 && ld.state == 0xC && ld.flags == 0x50 && ld.failed == 0x10,
          "an entry: its path +4 (an MtString, characters +8 on), archive +8 (flags +0x50, failed 0x10), state +0xC");
    check(ld.chars_list == 3 && ld.kind == 7 && ld.dir == "arc\\DX9\\",
          "the characters' list 3, an item's class key of kind 7, archives \"arc\\DX9\\<class>\"");
    auto cls = [](int id) {
      const mt::Class* c = inventory::item_class(id);
      return std::string(c ? c->name : "");
    };
    check(cls(0x101) == "uWp1010" && cls(0x104) == "uWp1020" && cls(0x112) == "uWp1080" && cls(0x113) == "uWp1090" &&
              cls(0x117) == "uWp1100" && cls(0x11B) == "uWp1120" && cls(0x120) == "uWp1173" && cls(0x201).empty(),
          "an item's class: the 909 uWp1010, the shotgun uWp1020, the crossbow uWp1100, the spray uWp1173 (ammo none)");
    int offered = 0, with_class = 0, weapons = 0;
    for (const items::Info& i : items::kItems) {
      weapons += i.offer && i.kind == items::kWeapon;
      if (i.offer && (i.id >> 8) == 1) {
        ++offered;
        with_class += !cls(i.id).empty();
      }
    }
    check(offered == 5 && with_class == offered && weapons == 0,
          "Add offers no weapon; each grenade and the spray it offers has a class (5)");

    // The game's runtime its request needs, made up in the copy: MT Framework's allocators (the
    // table 0x1814338 and MtString's 0x179E830: slot 8 alloc, slot 9 free), the two imports it
    // calls (InterlockedIncrement/Decrement), its formatter (the CRT's _vsnprintf_s, 0x13F5DE1, a
    // jump to the test's), sInfoMan (0x17CF49C: read, unused for an item) and sResource
    // (0x186E6A0: its create, slot 12, hands out made-up archives).
    struct Fakes {
      static void* __thiscall alloc(void*, size_t size, size_t align) {
        return _aligned_malloc(size ? size : 1, align < 8 ? 8 : align);
      }
      static void __thiscall free_(void*, void* p) {
        if (p) _aligned_free(p);
      }
      static int __cdecl format(char* buf, size_t size, size_t, const char* fmt, va_list ap) {
        return std::vsnprintf(buf, size, fmt, ap);
      }
    };
    static void* alloc_vt[16] = {};
    alloc_vt[8] = reinterpret_cast<void*>(&Fakes::alloc);
    alloc_vt[9] = reinterpret_cast<void*>(&Fakes::free_);
    static void* alloc_obj = alloc_vt;
    struct Arc {
      std::string path;
      std::vector<uint8_t> mem = std::vector<uint8_t>(0x80, 0);
    };
    static std::vector<Arc*> made;
    struct Res {
      static void* __thiscall create(void*, void*, const char* path, int) {
        Arc* a = new Arc;
        a->path = path;
        made.push_back(a);
        return a->mem.data();
      }
    };
    static void* res_vt[16] = {};
    res_vt[12] = reinterpret_cast<void*>(&Res::create);
    static void* res_obj = res_vt;
    std::vector<uint8_t> info(0x50000, 0);
    const uint32_t kTable = 0x1814338, kStrAlloc = 0x179E830, kDec = 0x15021BC, kInc = 0x1502120, kFmt = 0x13F5DE1,
                   kInfo = 0x17CF49C, kRes = 0x186E6A0;
    uint32_t table_was[64];
    std::memcpy(table_was, reinterpret_cast<void*>(kTable), sizeof(table_was));
    const uint32_t str_was = mem::read<uint32_t>(kStrAlloc), dec_was = mem::read<uint32_t>(kDec),
                   inc_was = mem::read<uint32_t>(kInc), info_was = mem::read<uint32_t>(kInfo),
                   res_was = mem::read<uint32_t>(kRes);
    uint8_t fmt_was[5];
    std::memcpy(fmt_was, reinterpret_cast<void*>(kFmt), 5);
    for (int i = 0; i < 64; ++i) mem::write<uintptr_t>(kTable + 4 * i, reinterpret_cast<uintptr_t>(&alloc_obj));
    mem::write<uintptr_t>(kStrAlloc, reinterpret_cast<uintptr_t>(&alloc_obj));
    const HMODULE k32 = GetModuleHandleA("kernel32.dll");
    mem::write<uintptr_t>(kDec, reinterpret_cast<uintptr_t>(GetProcAddress(k32, "InterlockedDecrement")));
    mem::write<uintptr_t>(kInc, reinterpret_cast<uintptr_t>(GetProcAddress(k32, "InterlockedIncrement")));
    uint8_t jmp[5] = {0xE9};
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&Fakes::format) - (kFmt + 5));
    std::memcpy(jmp + 1, &rel, 4);
    mem::write_bytes(kFmt, jmp, 5);
    mem::write<uintptr_t>(kInfo, reinterpret_cast<uintptr_t>(info.data()));
    mem::write<uintptr_t>(kRes, reinterpret_cast<uintptr_t>(&res_obj));

    // The loader itself, as the game left it: the list chosen last the cutscenes' (7).
    const mt::Class* lmc = mt::find("sLoadMan");
    Obj lm(lmc->size, lmc->vtable);
    lm.field<uint32_t>(ld.current) = 7;
    game::g_systems["sLoadMan"] = lm.at();
    struct Seen {
      int calls = 0, slot = 0, id = 0, count = 0;
    };
    static Seen seen;
    struct SetSlot {
      static int __thiscall stand_in(void*, int slot, int id, int count, int, void*) {
        ++seen.calls;
        seen.slot = slot;
        seen.id = id;
        seen.count = count;
        return 2;
      }
    };
    const inventory::SetSlotFn real_set = inventory::g_set_slot;
    inventory::g_set_slot = &SetSlot::stand_in;  // a weapon's unit is more than the copy can make

    auto names_in = [&](int list) {
      std::vector<std::string> out;
      inventory::entries(lm.at(), list, [&](uintptr_t e, uint32_t) {
        if (const char* n = inventory::entry_class(e)) out.emplace_back(n);
      });
      std::sort(out.begin(), out.end());
      return out;
    };
    auto state_of = [&](const char* name) {
      int st = -1;
      inventory::entries(lm.at(), -1, [&](uintptr_t e, uint32_t s) {
        const char* n = inventory::entry_class(e);
        if (n && !std::strcmp(n, name)) st = static_cast<int>(s);
      });
      return st;
    };
    auto arc_of = [&](const char* name) -> Arc* {
      for (Arc* a : made)
        if (a->path == std::string("arc\\DX9\\") + name) return a;
      return nullptr;
    };
    auto frames = [&](int n) {
      for (int i = 0; i < n; ++i) inventory::after_frame(main_obj.mem.data());
    };
    const uint32_t job3 = 0x30040 + 3 * 0x54;

    inventory::tick(r);
    inventory::View v = inventory::view();
    check(v.loader && inventory::avail(v, 0x115) == inventory::kLoadable && inventory::avail(v, 0x201) == inventory::kHere,
          "at the pause: the flash grenade loadable, ammo there (no unit)");
    inventory::add(0, 0x115, 2);
    inventory::tick(r);
    v = inventory::view();
    check(names_in(3) == std::vector<std::string>{"uWp1094", "uWp1095"} && state_of("uWp1094") == 1 &&
              state_of("uWp1095") == 1,
          "Add of 2 flash grenades: the game's own request put its archive, and its thrown form's, in the characters' "
          "list (3), asked for");
    check(lm.field<uint32_t>(job3 + 0x48) == 1 && lm.field<uint32_t>(ld.current) == 7,
          "... the commit asked list 3's job to load them; the list chosen last (7) put back");
    check(seen.calls == 0 && v.loading && v.note_chara == 0 && std::strstr(v.note, "Loading the Flash Grenade"),
          "... nothing added yet: \"Loading the Flash Grenade...\" under Leon's Add");
    frames(8);
    Arc* fg = arc_of("uWp1094");
    check(fg && arc_of("uWp1095") && state_of("uWp1094") == 2 && lm.field<uint8_t>(0x20) == 4,
          "the mod runs the loader's frame at the pause: its job made the archives (through sResource), loading");
    inventory::tick(r);
    check(seen.calls == 0, "... still loading: still waiting");
    for (Arc* a : made) a->mem[0x50] = 1;  // loaded
    frames(1);
    check(state_of("uWp1094") == 3 && state_of("uWp1095") == 3 && lm.field<uint8_t>(0x20) == 0 &&
              lm.field<uint32_t>(job3 + 0x48) == 4,
          "the archives in: the entries loaded (3), the job done, the loader idle");
    inventory::tick(r);
    v = inventory::view();
    check(seen.calls == 1 && seen.slot == -1 && seen.id == 0x115 && seen.count == 2 && !v.loading && !v.note[0],
          "... and the Add made: set-slot(-1, the flash grenade, 2)");
    check(inventory::avail(v, 0x115) == inventory::kHere, "the flash grenade there now (its archive loaded)");
    lm.field<uint32_t>(0x30040 + 5 * 0x54 + 0x48) = 1;  // a job of the game's own, asked for
    frames(3);
    check(lm.field<uint8_t>(0x20) == 0, "nothing of the mod's loading: the loader's frame left to the game");
    lm.field<uint32_t>(0x30040 + 5 * 0x54 + 0x48) = 0;

    // A pick preloads: the incendiary grenade, with what the game's own request asks for with it.
    inventory::preload(0x114);
    inventory::tick(r);
    const std::vector<std::string> want{"uWp1092", "uWp1093", "uWp1094", "uWp1095"};
    check(names_in(3) == want, "a pick preloads: the incendiary grenade (uWp1092) and its thrown form (the game's own "
                               "request)");
    frames(8);
    for (Arc* a : made) a->mem[0x50] = 1;
    frames(1);
    inventory::tick(r);
    v = inventory::view();
    check(inventory::avail(v, 0x114) == inventory::kHere && state_of("uWp1092") == 3 && state_of("uWp1093") == 3,
          "... all of it loaded while the pause lasts");
    inventory::add(0, 0x114, 2);
    inventory::tick(r);
    check(seen.calls == 2 && seen.id == 0x114, "... so its Add acts at once");
    // A weapon is neither preloaded nor added: Add offers none (the Records WEAPONS window gives them).
    const size_t listed = names_in(3).size();
    inventory::preload(0x117);
    inventory::add(0, 0x117, 1);
    inventory::tick(r);
    check(names_in(3).size() == listed && seen.calls == 2 && !inventory::view().loading,
          "a weapon (the crossbow): neither preloaded nor added - nothing asked of the loader, no set-slot");
    // Two Adds of one grenade waiting together: both made once in.
    inventory::add(0, 0x116, 2);  // remote bombs
    inventory::add(0, 0x116, 3);
    inventory::tick(r);
    check(state_of("uWp1096") == 1 && inventory::view().loading && seen.calls == 2,
          "two Adds of remote bombs: their archive asked for once, both waiting");
    frames(8);
    for (Arc* a : made) a->mem[0x50] = 1;
    frames(1);
    inventory::tick(r);
    check(seen.calls == 4 && seen.id == 0x116 && seen.count == 3 && !inventory::view().loading,
          "... both made once it is in, in their order (2, then 3)");

    // A hand grenade: the game's request asks for its thrown form (variant 1) too.
    inventory::add(0, 0x113, 2);
    inventory::tick(r);
    check(state_of("uWp1090") == 1 && state_of("uWp1091") == 1 && seen.calls == 4,
          "a hand grenade: its archive and its thrown form's asked for, the Add waiting");
    // ... and the pause closes before they are in: the Add dropped, the frame left to the game.
    main_obj.field<uint8_t>(gl.paused) = 0;
    game_tick(main_obj.mem.data(), &r);
    inventory::tick(r);
    const uint8_t st = lm.field<uint8_t>(0x20);
    frames(4);
    v = inventory::view();
    check(lm.field<uint8_t>(0x20) == st && !v.loading && std::strstr(v.note, "pause closed") && seen.calls == 4,
          "the pause closed while loading: the Add dropped (noted), the loader's frame left to the game's own");
    main_obj.field<uint8_t>(gl.paused) = 1;
    game_tick(main_obj.mem.data(), &r);
    frames(8);
    for (Arc* a : made) a->mem[0x50] = 1;
    frames(1);

    // While the game chooses a list (an entry in state 0) nothing is asked for.
    uintptr_t first = 0;
    inventory::entries(lm.at(), 3, [&](uintptr_t e, uint32_t) {
      if (!first) first = e;
    });
    mem::write<uint32_t>(first + ld.state, 0);
    const size_t before = names_in(3).size();
    inventory::add(0, 0x120, 1);  // the spray
    inventory::tick(r);
    check(names_in(3).size() == before && inventory::view().loading,
          "while a list is being chosen (an entry 0): nothing asked for, the Add waits");
    mem::write<uint32_t>(first + ld.state, 3);
    inventory::tick(r);
    check(state_of("uWp1173") == 1, "... and asked for once the choice is over (the spray, uWp1173)");
    main_obj.field<uint8_t>(gl.paused) = 0;
    game_tick(main_obj.mem.data(), &r);
    inventory::tick(r);
    main_obj.field<uint8_t>(gl.paused) = 1;
    game_tick(main_obj.mem.data(), &r);

    // The loader not understood: only what someone holds is there; the rest is never added.
    const inventory::FrameFn frame_was = inventory::g_frame;
    inventory::g_frame = nullptr;
    const uintptr_t case13 = gl.inventory + 13 * gl.item_size;  // Leon's case slot 13: a hand grenade
    leon.field<uint8_t>(case13 + gl.it_used) = 1;
    leon.field<uint16_t>(case13 + gl.it_id) = 0x113;
    inventory::tick(r);
    v = inventory::view();
    check(inventory::avail(v, 0x115) == inventory::kNever && inventory::avail(v, 0x113) == inventory::kHere,
          "the loader not understood: a grenade no one holds never, the hand grenade Leon holds there");
    leon.field<uint8_t>(case13 + gl.it_used) = 0;
    leon.field<uint16_t>(case13 + gl.it_id) = 0;
    inventory::g_frame = frame_was;

    game::g_systems.erase("sLoadMan");
    inventory::g_set_slot = real_set;
    std::memcpy(reinterpret_cast<void*>(kTable), table_was, sizeof(table_was));
    mem::write<uint32_t>(kStrAlloc, str_was);
    mem::write<uint32_t>(kDec, dec_was);
    mem::write<uint32_t>(kInc, inc_was);
    mem::write_bytes(kFmt, fmt_was, 5);
    mem::write<uint32_t>(kInfo, info_was);
    mem::write<uint32_t>(kRes, res_was);
  }

  std::printf("-- the records windows (records.cpp)\n");
  {
    records::init();
    records::install();
    const records::Lay& rl = records::L;
    check(records::known() && records::g_chapters && records::g_medals && records::g_emblems && records::g_titles,
          "the records understood: chapters, medals, emblems, titles");
    check(rl.profile == 0x20 && rl.profile_size == 0xFDD8 && rl.profile_base == 0x38 && rl.clear == 0x1650 &&
              rl.grid == 0x244 && rl.percent == 0x2F4 && rl.not_cleared == 6 && rl.rank_most == 5,
          "the profiles sInfoMan +0x38 (in use +0x20, 0xFDD8 each); the chapter records +0x1650; the Records grid "
          "+0x244 (6: not cleared, ranks 0-5), its percent +0x2F4");
    check(rl.page == 0x48 && rl.profiles_page == 0 && rl.view == 0x320 && rl.grid_view == 1 && rl.medals_page == 3 &&
              rl.medal_cat == 0x340 && rl.medal_mask == 0x348 && rl.step == 0x38,
          "the Records screen: page +0x48 (PROFILES 0, MEDALS 3), PROFILES' view +0x320 (the grid 1), the medal "
          "category +0x340 and the categories shown +0x348; the screens' step +0x38");
    check(rl.medal[0].n == 200 && rl.medal[0].counts == 0x1FB0 && rl.medal[0].base == 3000 &&
              rl.medal[1].counts == 0x209C && rl.medal[1].base == 3200 && rl.medal[2].counts == 0x20E8 &&
              rl.medal[3].counts == 0x23B0 && rl.medal[2].base == 3300 && rl.medal[3].base == 3300 &&
              rl.medal[4].counts == 0x2284 && rl.medal[4].base == 3400 && rl.medal[5].counts == 0x22B6 &&
              rl.medal[5].base == 3450 && rl.medal[6].counts == 0x222C && rl.medal[6].base == 3350 &&
              rl.medal[7].counts == 0x2308 && rl.medal[8].counts == 0x2364 && rl.medal[8].base == 3550 &&
              rl.medal[5].unused == 0x1796E8C,
          "the medals: the campaigns' 200 at +0x1FB0 (names from 3000), the modes' 50 each (Survivors' team battle "
          "the shared get's second list, +0x22B6)");
    check(rl.emblems == 0xE1 && rl.nemblems == 50 && rl.lock_cs == 4 && rl.lock_use == 0x1C &&
              rl.lock_flag == 0x186E5D0 && reinterpret_cast<uintptr_t>(records::g_achieve) == 0x562DD0,
          "the game's achievement exe+0x162DD0; the emblems +0xE1 (50); sInfoMan's lock +4 (in use +0x1C or a global)");
    check(rl.plates == 0xC8 && rl.nplates == 25 && rl.files == 0x818 && rl.nfiles == 80 && rl.unlock == 0xB8 &&
              rl.unlock_any == 2 && rl.unlock_three == 4 && rl.stages == 0x970 && rl.stage_chris == 2 &&
              rl.stage_jake == 4 && rl.characters == 0x978 && rl.char_ada == 0x1000000 && rl.files_chris[0] == 0x33 &&
              rl.files_chris[1] == 0x34 && rl.files_jake[0] == 0x35 && rl.files_jake[1] == 0x36 &&
              rl.diff_achievement == 0x15 && rl.diff_achievements == 4 && rl.stories == 4,
          "a clear's rewards: templates +0xC8, files +0x818, the unlock flags +0xB8 (2 any story, 4 the three), stage "
          "bits +0x970, Ada's +0x978, files 0x33/34 and 0x35/36, achievements 0x15+");
    check(rl.emblem_step == 0x1E && rl.title_step == 0x28 && rl.emblem_rows == 0x400 && rl.emblem_count == 0x23C &&
              rl.title_rows == 0x7E8 && rl.title_count == 0x244 && rl.row_size == 0x14 && rl.states == 0x113 &&
              rl.nstates == 301 && rl.counters == 0x240 && rl.ncounters == 301 && rl.table == 0x179A3A0 &&
              rl.table_count == 0x179D1A8 && rl.unused_counters == 0x179A270 && rl.top_level == 4 &&
              rl.top_achievement == 0x24 && rl.titles_counted == 300 && rl.titles_counter == 11 && rl.titular == 10 &&
              rl.titular_achievement == 0x1A && rl.title_progress == 0xC && rl.page_rows == 15 &&
              rl.title_last_page == 0x250 && rl.title_pages == 0x254,
          "the Dog Tag screen: the emblem list step 0x1E, the title list 0x28, rows of 0x14 (+0x400, +0x7E8; a title "
          "row's progress +0xC, 15 rows a page, the last page +0x250, the pages its buttons go to +0x254); title "
          "states +0x113 and counters +0x240 (301); level 4 gives achievement 36, ten titles 26, held into counter 11");
    check(rl.info_wait == 0x4EC90 && rl.storage_busy == 0x490 && rl.storage_global == 0x17D0564 &&
              rl.gi_base == 0x3F798 && rl.gi_size == 0x3CE4 && reinterpret_cast<uintptr_t>(records::g_dirty) == 0xE43150 &&
              reinterpret_cast<uintptr_t>(records::g_commit) == 0x5687F0 &&
              reinterpret_cast<uintptr_t>(records::g_copy_info) == 0xD702A0 &&
              reinterpret_cast<uintptr_t>(records::g_busy) == 0x599770 &&
              reinterpret_cast<uintptr_t>(records::g_request) == 0x53F9E0 &&
              reinterpret_cast<uintptr_t>(records::g_after1) == 0x567D50 &&
              reinterpret_cast<uintptr_t>(records::g_after2) == 0x567DC0,
          "the profile's save: the menus' own steps (changed?, commit, cGameInfo copy, sStorage's busy?/ask, the "
          "afters)");

    // The game's globals its own functions read, made up in the copy: sInfoMan, sBioMain, Steam's achievements
    // (its unlock calls an ISteamUserStats' slot 7 with the API name) and the storage.
    const mt::Class* ic = mt::find("sInfoMan");
    Obj rinfo(ic->size, ic->vtable);
    rinfo.field<uint32_t>(rl.profile) = 1;
    Obj storage(0xA000, 0);
    static void* stats_vt[16] = {};
    stats_vt[7] = reinterpret_cast<void*>(&steam_set);
    static void* stats_obj = stats_vt;
    static uint8_t achievements[0x60] = {};
    *reinterpret_cast<void**>(achievements + 0x4C) = &stats_obj;
    const uint32_t kInfo = 0x17CF49C, kMain = 0x186E1B4, kSteam = 0x1877848, kStorage = 0x17D0564;
    const uint32_t info_was = mem::read<uint32_t>(kInfo), main_was = mem::read<uint32_t>(kMain),
                   steam_was = mem::read<uint32_t>(kSteam), storage_was = mem::read<uint32_t>(kStorage);
    mem::write<uintptr_t>(kInfo, rinfo.at());
    mem::write<uintptr_t>(kMain, main_obj.at());
    mem::write<uintptr_t>(kSteam, reinterpret_cast<uintptr_t>(achievements));
    mem::write<uintptr_t>(kStorage, storage.at());
    game::g_systems["sInfoMan"] = rinfo.at();
    records::g_sinfo = 0;
    const uintptr_t prof = rinfo.at() + rl.profile_base + rl.profile_size;  // profile 1

    // The screens, as units on a line of a made-up sBioUnit - the game's own subclass of sUnit, which the main
    // system holds (in play the log lists "sBioUnit", never "sUnit": 2026-11-06, no window came up while the mod
    // asked for "sUnit" by name).
    const mt::Class* uc = mt::find("sBioUnit");
    Obj sunit(uc->size, uc->vtable);
    sunit.field<uint32_t>(ul.lines_at) = 0x20;
    for (uint32_t i = 0; i < 0x20; ++i) sunit.field<uint32_t>(i * ul.stride + ul.line_flags) = 0;
    Obj rec(rl.record->size, rl.record->vtable), tag(rl.dogtag->size, rl.dogtag->vtable);
    rec.field<uint32_t>(ul.unit_flags) = 2;
    tag.field<uint32_t>(ul.unit_flags) = 2;
    sunit.field<uintptr_t>(0x12 * ul.stride + ul.line_head) = rec.at();
    rec.field<uintptr_t>(ul.unit_next) = tag.at();
    game::g_systems.erase("sUnit");
    game::g_systems["sBioUnit"] = sunit.at();
    check(uc && mt::find("sUnit") && game::system("sUnit") == sunit.at(),
          "sUnit asked for by name: the main system's sBioUnit (its class derives from it)");
    game::Run menus;  // no game: the menus
    menus.panel = true;
    auto screen = [&] {
      records::tick(menus);
      return records::view().screen;
    };
    // The screens come into play as the game's line frame starts them: through the mod's hooks in their classes'
    // vtables (each class's own start and destructor, in the slots the line frame calls).
    auto start_screen = [&](Obj& o) {
      return through_hook(records::g_screen_starts, o.mem.data(), reinterpret_cast<uintptr_t>(&stand_in_start),
                          [&] { records::hk_screen_start(o.mem.data()); });
    };
    auto end_screen = [&](Obj& o) {
      return through_hook(records::g_screen_dtors, o.mem.data(), reinterpret_cast<uintptr_t>(&stand_in_dtor),
                          [&] { records::hk_screen_dtor(o.mem.data(), 1); });
    };
    check(ul.start_slot == 5 && ul.destroy_slot == 0 &&
              mem::read<uintptr_t>(rl.record->vtable + 4 * ul.start_slot) ==
                  reinterpret_cast<uintptr_t>(&records::hk_screen_start) &&
              mem::read<uintptr_t>(rl.dogtag->vtable + 4 * ul.destroy_slot) ==
                  reinterpret_cast<uintptr_t>(&records::hk_screen_dtor),
          "a unit's start (slot 5) and destruction (slot 0) as sUnit's line frame calls them: the screens' classes' "
          "own hooked");
    check(screen() == records::Screen::kNone, "no screen started: no window");
    check(start_screen(rec) && start_screen(tag), "the Records and Dog Tag screens start (through the mod's hooks)");
    // A save asked for, run to its end as the storage and sInfoMan would finish their parts.
    auto finish_save = [&] {
      for (int i = 0; i < 16 && records::g_save != records::Save::kIdle; ++i) {
        storage.field<uint32_t>(rl.storage_busy) = 0;
        rinfo.field<uint32_t>(rl.info_wait) = 0;
        records::tick(menus);
      }
      return records::g_save == records::Save::kIdle;
    };
    check(screen() == records::Screen::kNone, "the Records screen opening (step 0): no window");
    rec.field<uint8_t>(rl.step) = 2;
    rec.field<uint32_t>(rl.view) = 1;
    check(screen() == records::Screen::kChapters, "PROFILES' Completed view: the chapters window");
    rec.field<uint32_t>(rl.view) = 0;
    check(screen() == records::Screen::kNone, "PROFILES' other view: none");
    rec.field<uint32_t>(rl.page) = 3;
    check(screen() == records::Screen::kMedals, "the MEDALS page: the medals window");
    rec.field<uint8_t>(rl.step) = records::kStepClosing;
    check(screen() == records::Screen::kNone, "the screen closing: none");
    tag.field<uint8_t>(rl.step) = 0x1E;
    check(screen() == records::Screen::kEmblems, "the Dog Tag screen's emblem list: the emblems window");
    tag.field<uint8_t>(rl.step) = 0x1F;
    check(screen() == records::Screen::kEmblems, "... and its Equip? prompt");
    tag.field<uint8_t>(rl.step) = 0x28;
    check(screen() == records::Screen::kTitles, "the title list: the titles window");
    tag.field<uint8_t>(rl.step) = 10;
    check(screen() == records::Screen::kNone, "the Dog Tag menu: none");
    game::Run playing;
    playing.in_game = true;
    rec.field<uint8_t>(rl.step) = 2;
    rec.field<uint32_t>(rl.page) = 0;
    rec.field<uint32_t>(rl.view) = 1;
    records::tick(playing);
    check(records::view().screen == records::Screen::kNone, "in a game: never");

    // Chapters, through the game's own gets (thiscall on the profile) and its story/whole-game tests.
    using ClearGet = uint32_t(__thiscall*)(void*, uint32_t, uint32_t, uint32_t);
    using Story = uint8_t(__thiscall*)(void*, int, int);
    using Whole = uint8_t(__thiscall*)(void*, int);
    using MedalGet = uint8_t(__thiscall*)(void*, int, int);
    using StateGet = uint8_t(__thiscall*)(void*, uint32_t);
    const auto cleared_get = reinterpret_cast<ClearGet>(0xE441B0), rank_get = reinterpret_cast<ClearGet>(0xE44200);
    const auto story = reinterpret_cast<Story>(0x564030);
    const auto whole = reinterpret_cast<Whole>(0x5640E0);
    const auto medal_get = reinterpret_cast<MedalGet>(0x565750);
    const auto title_get = reinterpret_cast<StateGet>(0xE43220);
    const auto emblem_get = reinterpret_cast<StateGet>(0xE431E0);
    void* const pv = reinterpret_cast<void*>(prof);
    void* const iv = reinterpret_cast<void*>(rinfo.at());
    check(screen() == records::Screen::kChapters, "the chapters window again");
    records::ChapterSet set;
    set.chara = 0;  // Leon, Professional, chapter 1, rank A
    set.difficulty = 3;
    set.chapter = 0;
    set.cleared = true;
    set.rank = 1;
    g_steam_unlocked.clear();
    records::set_chapters(&set, 1);
    records::tick(menus);
    check(cleared_get(pv, 0, 3, 0) == 1 && rank_get(pv, 0, 3, 0) == 1, "Leon, Professional, chapter 1: the game's own "
                                                                      "gets read it cleared, rank A");
    check(mem::read<float>(prof + rl.clear + 12 * (15 + 0) + 8) == 71999.0f,
          "its best time the game's \"none\" (the chapter clock's most), for a real clear to replace");
    check(rec.field<uint8_t>(rl.grid + 15) == 1 && rec.field<uint32_t>(rl.percent) == 0,
          "the Records grid's own copy shows it (A); its percent 1 of 175 rounds to 0");
    check(mem::read<uint8_t>(prof + rl.plates + 1) == 1 && g_steam_unlocked.size() == 1 &&
              g_steam_unlocked[0] == "ACH02" && emblem_get(pv, 1) == 1,
          "what the clear gives: Leon's chapter 1 template, \"Gone to Hell\" through the game's achievement (Steam's "
          "ACH02, the emblem with it)");
    // Its best time typed: the game's own get (0xE44250, Chapter Select's) reads it, the rank kept; a time past the
    // most held at it; the window's view shows it.
    using TimeGet = float(__thiscall*)(void*, uint32_t, uint32_t, uint32_t);
    const auto time_get = reinterpret_cast<TimeGet>(0xE44250);
    set.time = 2700.0f;
    g_steam_unlocked.clear();
    records::set_chapters(&set, 1);
    records::tick(menus);
    const records::View timed_view = records::view();
    check(time_get(pv, 0, 3, 0) == 2700.0f && rank_get(pv, 0, 3, 0) == 1 && cleared_get(pv, 0, 3, 0) == 1 &&
              g_steam_unlocked.empty(),
          "a cleared chapter's best time set (45:00): the game's own get reads 2700, its rank kept (A), nothing given "
          "again");
    check(timed_view.cell[0][3][0].time == 2700.0f && timed_view.time_most == 71999.0f,
          "... the window's view shows it, and a time's most (the chapter clock's, 71999)");
    set.time = 80000.0f;
    records::set_chapters(&set, 1);
    records::tick(menus);
    check(time_get(pv, 0, 3, 0) == 71999.0f, "a time past the most (80000 s) held at it (71999: 19:59:59)");
    set.time = 0.0f;
    records::set_chapters(&set, 1);
    records::tick(menus);
    const records::View zero_view = records::view();
    check(time_get(pv, 0, 3, 0) == 0.0f && cleared_get(pv, 0, 3, 0) == 1 && zero_view.cell[0][3][0].cleared &&
              zero_view.cell[0][3][0].time == 0.0f,
          "a best time of 0:00:00 (a clear with the clock held at 0): the game's get reads 0, the view has it cleared "
          "in 0 - a time, not the game's \"none\"");
    set.time = -1.0f;
    // Leon's story on Normal by Helena, Chris' by Piers, Jake's and Ada's on No Hope.
    std::vector<records::ChapterSet> sets;
    for (int ch = 0; ch < 5; ++ch) {
      sets.push_back({1, 1, static_cast<uint8_t>(ch), true, 0});
      sets.push_back({3, 1, static_cast<uint8_t>(ch), true, 2});
      sets.push_back({4, 4, static_cast<uint8_t>(ch), true, 0});
    }
    sets[0].time = 1500.0f;  // Helena, Normal, chapter 1: a first clear with its time typed (25:00)
    g_steam_unlocked.clear();
    records::set_chapters(sets.data(), static_cast<int>(sets.size()));
    records::tick(menus);
    check(time_get(pv, 1, 1, 0) == 1500.0f && time_get(pv, 1, 1, 1) == 71999.0f && cleared_get(pv, 1, 1, 0) == 1,
          "a first clear with its time typed gets it (Helena, Normal, chapter 1: 25:00); one without, the game's "
          "\"none\"");
    const uint32_t unlock = mem::read<uint32_t>(prof + rl.unlock);
    check(story(iv, 0, 4) && story(iv, 1, 4) && story(iv, 2, 4) && !story(iv, 3, 4) && records::story_complete(prof, 0) &&
              records::story_complete(prof, 2) && !records::story_complete(prof, 3),
          "Leon's, Chris' and Jake's stories complete, Ada's not - the game's own test and the mod's agree");
    check((unlock & 6) == 6 && (mem::read<uint32_t>(prof + rl.stages) & 6) == 6 &&
              !(mem::read<uint32_t>(prof + rl.characters) & rl.char_ada),
          "the unlocks: a story's (2), the three's - Ada's campaign (4), Chris' and Jake's stage bits; not Ada's");
    check(mem::read<uint8_t>(prof + rl.files + 0x33) == 1 && mem::read<uint8_t>(prof + rl.files + 0x34) == 1 &&
              mem::read<uint8_t>(prof + rl.files + 0x35) == 1 && mem::read<uint8_t>(prof + rl.files + 0x36) == 1,
          "Chris' chapter 1 and Jake's chapter 3 give their files");
    const bool no_whole = !whole(iv, 0) && !records::whole_game(prof, 0);
    for (int ch = 0; ch < 5; ++ch) sets.push_back({6, 4, static_cast<uint8_t>(ch), true, 5});
    g_steam_unlocked.clear();
    records::set_chapters(sets.data() + 15, 5);
    records::tick(menus);
    int whole_achs = 0;
    for (const std::string& a : g_steam_unlocked) whole_achs += a == "ACH22" || a == "ACH23";
    check(no_whole && whole(iv, 1) && records::whole_game(prof, 1) && !whole(iv, 2) && !records::whole_game(prof, 2) &&
              (mem::read<uint32_t>(prof + rl.characters) & rl.char_ada) && whole_achs == 2,
          "Ada's story too: every chapter on Normal or harder - \"Green around the Ears\" and \"Normal Is Good\" (the "
          "game's test agrees: not Veteran); Ada for the extra content");
    check(rec.field<uint8_t>(rl.grid + (6 * 5 + 4) * 5 + 2) == 5 && rec.field<uint32_t>(rl.percent) == 21 * 100 / 175,
          "the grid's copy: Ada, No Hope, chapter 3 at E; the percent 21 of 175");
    set.cleared = false;
    records::set_chapters(&set, 1);
    records::tick(menus);
    check(!cleared_get(pv, 0, 3, 0) && rec.field<uint8_t>(rl.grid + 15) == 6 && mem::read<uint8_t>(prof + rl.plates + 1),
          "a clear taken back: not cleared (the grid's copy too); its template kept");
    check(finish_save(), "... its save run to the end");

    // Medals.
    rec.field<uint32_t>(rl.page) = 3;
    rec.field<uint32_t>(rl.medal_mask) = 0x1FF;
    rec.field<uint32_t>(rl.medal_cat) = 5;
    check(screen() == records::Screen::kMedals, "the medals window");
    records::View mv = records::view();
    check(mv.medal_cat == 5 && mv.medal[0].n == 175 && mv.medal[0].id[0] == 6 && mv.medal[5].n == 46 &&
              mv.medal[1].n == 45 && !std::strcmp(records::medal_name(mv, 0, 6), "Accuracy - A Rank"),
          "the categories as the game lists them: the campaigns' 175 (from \"Accuracy - A Rank\"), Survivors' team "
          "battle 46");
    const records::MedalSet ms[4] = {{0, 6, 12}, {5, 1, 150}, {0, 0, 5}, {8, 0, 3}};
    records::set_medals(ms, 4);
    records::tick(menus);
    check(medal_get(iv, 0, 6) == 12 && medal_get(iv, 5, 1) == 99 && medal_get(iv, 0, 0) == 0 && medal_get(iv, 8, 0) == 3,
          "counts set, as the game's own medal get reads them (99 at most; no medal at an unused number)");
    check(finish_save(), "... its save run to the end");

    // Emblems.
    tag.field<uint8_t>(rl.step) = 0x1E;
    rec.field<uint8_t>(rl.step) = records::kStepClosing;
    check(screen() == records::Screen::kEmblems, "the emblems window");
    g_steam_unlocked.clear();
    const uint8_t em[2] = {3, 25};
    records::award_emblems(em, 2);
    records::tick(menus);
    check(emblem_get(pv, 3) == 1 && emblem_get(pv, 25) == 1 && g_steam_unlocked.size() == 2 &&
              g_steam_unlocked[0] == "ACH04" && g_steam_unlocked[1] == "ACH26" && tag.field<uint32_t>(rl.emblem_rows + 3 * 0x14) == 1,
          "awarded: the game's achievement gave each its emblem and Steam's ACH04/ACH26; the list's row shows it");
    main_obj.field<uint32_t>(0x1045C) = 1;  // the game's no-achievement state
    g_steam_unlocked.clear();
    const uint8_t em2 = 45;  // one no clear above gave
    records::award_emblems(&em2, 1);
    records::tick(menus);
    check(emblem_get(pv, 45) == 1 && g_steam_unlocked.empty() && std::strstr(records::view().note, "emblems only"),
          "the game giving no achievements: the emblem set by the mod, Steam untouched (noted)");
    check(finish_save(), "... its save run to the end");
    main_obj.field<uint32_t>(0x1045C) = 0;

    // Titles. The list's count and last page as the screen's start sets them, its pages page 0 only.
    tag.field<int32_t>(rl.title_count) = 301;
    tag.field<int32_t>(rl.title_last_page) = 301 / 15;
    tag.field<uint32_t>(rl.title_pages) = 1;
    tag.field<uint8_t>(rl.step) = 0x28;
    check(screen() == records::Screen::kTitles, "the titles window");
    records::View tv = records::view();
    const records::TitleRow* gp = nullptr;
    for (int i = 0; i < tv.ntitles; ++i)
      if (tv.title[i].counter == 4) gp = &tv.title[i];
    check(tv.ntitles == 255 && gp && gp->nlevels == 4 && gp->stars && gp->level[2].goal == 100 &&
              !std::strcmp(records_text::kTitleName[gp->level[0].msg], "Global Player") &&
              !std::strcmp(records_text::kTitleNeed[gp->level[1].msg], "Play with 50 other players online."),
          "255 families as the game's table has them: Global Player's four levels, the third at 100");
    g_steam_unlocked.clear();
    const records::TitleSet ts = {4, 2};
    records::award_titles(&ts, 1);
    records::tick(menus);
    const uintptr_t row = tag.at() + rl.title_rows + 3 * rl.row_size;
    check(title_get(pv, 3) == 0x82 && mem::read<uint32_t>(row) == 0x82 && mem::read<uint32_t>(row + 8) == 1 &&
              mem::read<uint32_t>(row + 0x10) == 100 && mem::read<uint32_t>(prof + rl.counters + 4 * 10) == 1 &&
              g_steam_unlocked.empty(),
          "Global Player to level 2 (NEW, as the game's check marks it); its row shows the next goal (100); one "
          "title held, in counter 11");
    check(mem::read<uint32_t>(prof + rl.counters + 4 * 3) == 50 && mem::read<uint32_t>(row + rl.title_progress) == 50 &&
              mem::read<uint32_t>(tag.at() + rl.title_rows + 10 * rl.row_size + rl.title_progress) == 1 &&
              tag.field<uint32_t>(rl.title_pages) == 1,
          "... its counter brought up to level 2's goal (50), as earning it leaves it - the row's progress 50 (the "
          "game shows what earns a title once it is above 0); counter 11's row 1; the pages: page 0");
    check(finish_save(), "... its save run to the end");
    const records::TitleSet lower = {4, 1};
    records::award_titles(&lower, 1);
    records::tick(menus);
    check(title_get(pv, 3) == 0x82 && records::g_save == records::Save::kIdle,
          "a lower level: nothing (a title is never taken back), and no save");
    std::vector<records::TitleSet> top;
    for (int c : {5, 6, 7, 8, 9, 10, 12, 13, 14}) top.push_back({static_cast<uint16_t>(c), 4});
    records::award_titles(top.data(), static_cast<int>(top.size()));
    records::tick(menus);
    check(title_get(pv, 4) == 0x84 && mem::read<uint32_t>(prof + rl.counters + 4 * 10) == 10 && g_steam_unlocked.size() == 2 &&
              g_steam_unlocked[0] == "ACH37" && g_steam_unlocked[1] == "ACH27" && mem::read<uint32_t>(tag.at() + rl.title_rows + 4 * rl.row_size + 8) == 0,
          "nine at level 4: \"Rising Up\" and, ten held, \"Titular Achievement\"; a family complete shows no next");
    check(mem::read<uint32_t>(prof + rl.counters + 4 * 4) == 300 && mem::read<uint32_t>(prof + rl.counters + 4 * 10) == 10,
          "... each one's counter at its level-4 goal (counter 5: 300); counter 11 the titles held, as the game counts "
          "them (not raised)");

    // The save those last titles asked for, through the game's own steps over the made-up storage: in the same
    // tick, the profile committed and the save asked of the storage.
    check(records::g_save == records::Save::kStorage && storage.field<uint32_t>(rl.storage_busy) == 1 &&
              !std::memcmp(reinterpret_cast<void*>(prof + 8), reinterpret_cast<void*>(prof + 0x7EF0), 0x7EE8),
          "the profile committed (its saved copy the working one), the save asked of the storage");
    records::tick(menus);
    check(records::g_save == records::Save::kStorage, "... waited for while the storage saves");
    storage.field<uint32_t>(rl.storage_busy) = 0;
    records::tick(menus);
    check(records::g_save == records::Save::kInfo && rinfo.field<uint32_t>(rl.info_wait) == 1, "then sInfoMan's afters");
    rinfo.field<uint32_t>(rl.info_wait) = 0;
    records::tick(menus);
    check(records::g_save == records::Save::kIdle && std::strstr(records::view().note, "Saved."), "... and done: Saved.");

    // A title held with its counter short of its goal (the mod's awards before 2026-11-06 left them so: the game
    // showed "? ? ?" for what earns them): flagged; its level asked for again brings the counter up only. A counter
    // above a goal is never lowered; a float family's counter takes the goal as a float. The list's pages follow.
    auto title_row_of = [&](int counter) -> const records::TitleRow* {
      static records::View tv2;
      tv2 = records::view();
      for (int i = 0; i < tv2.ntitles; ++i)
        if (tv2.title[i].counter == counter) return &tv2.title[i];
      return nullptr;
    };
    mem::write<uint8_t>(prof + rl.states + 49, 2);  // counter 50 (page 3): level 2 held, its counter 0
    mem::write<uint32_t>(prof + rl.counters + 4 * 49, 0);
    mem::write<uint32_t>(prof + rl.counters + 4 * 59, 999999);  // counter 60 (page 3): above every goal
    records::tick(menus);
    const records::TitleRow* r50 = title_row_of(50);
    check(r50 && r50->behind && r50->earned == 2 && r50->progress == 0 && !title_row_of(4)->behind,
          "a title held with its counter short (level 2, 0 of 50): flagged; Global Player (50 of 50) not");
    g_steam_unlocked.clear();
    const std::vector<records::TitleSet> more = {{50, 2}, {60, 1}, {119, 1}};
    records::award_titles(more.data(), static_cast<int>(more.size()));
    records::tick(menus);
    check(title_get(pv, 49) == 2 && mem::read<uint32_t>(prof + rl.counters + 4 * 49) == 50 &&
              mem::read<uint32_t>(tag.at() + rl.title_rows + 49 * rl.row_size + rl.title_progress) == 50 &&
              !title_row_of(50)->behind,
          "its level asked for again: its counter only, brought up to the goal (50), the row's progress 50 - no longer "
          "flagged, its state as it was");
    check(title_get(pv, 59) == 0x81 && mem::read<uint32_t>(prof + rl.counters + 4 * 59) == 999999,
          "a counter above the goal: left as it is (the title awarded)");
    check(title_get(pv, 118) == 0x81 && mem::read<float>(prof + rl.counters + 4 * 118) == 300.0f &&
              mem::read<uint32_t>(tag.at() + rl.title_rows + 118 * rl.row_size + rl.title_progress) == 300,
          "a float family's counter (119, seconds in cover): the goal as a float (300.0), its row 300");
    check(tag.field<uint32_t>(rl.title_pages) == (1u | 1u << 3 | 1u << 7) && g_steam_unlocked.empty() &&
              records::g_save != records::Save::kIdle,
          "the list's pages its buttons go to: 0, and 3 and 7 now their titles' counters have started; a save asked "
          "for (a counter brought up is a change)");
    check(finish_save(), "... its save run to the end");

    // The weapons window: the Records screen's WEAPONS page (page 1). The page's own reads run in the copy - its row's
    // record (the first of its row in the weapon table), Times Used, the row a use counts on - and the characters'
    // saved inventories set up by the game's own New Game reset (sInfoMan's, over a made-up sGameMan: the campaigns
    // mode), its weapons due by the stage made had by its own code.
    {
      const records::WeaponLay& wl = records::W;
      check(records::g_weapons && wl.page == 1 && wl.table == 0x17959B0 && wl.count == 0x1794B8C && wl.rec == 12 &&
                wl.r_entry == 0 && wl.r_always == 2 && wl.r_id == 8 && wl.r_mode == 10 && wl.uses == 0xA90 &&
                wl.kills == 0xA94 && wl.stride == 0x2C && wl.nstats == 50 && wl.name_base == 3600 && wl.rows == 43 &&
                wl.id_mask == 0xFFF,
            "the WEAPONS page: page 1, the weapon table exe+0x13959B0 (12-byte records: row +0, always named +2, item "
            "+8, mode +10), Times Used +0xA90 and Enemies Defeated +0xA94 every 0x2C (50), names from 3600, 43 rows");
      check(records::g_carried && wl.carried == 0x20D8 && wl.carried_size == 0x70 && wl.ncarried == 8 && wl.hand == 7 &&
                wl.slots == 0x10 && wl.nslots == 24 && wl.slot_size == 4 && wl.slot_count == 2 &&
                wl.weapon_slots == 7 && wl.kept == -2 && wl.had == -1 && wl.tables == 0x17948FC &&
                wl.counts == 0x179491C && wl.ntables == 8 && wl.t_size == 6 && wl.t_stage == 0 && wl.t_id == 4,
            "the saved inventories: cGameInfo +0x20D8 (8 of 0x70: in hand +7, 24 slots from +0x10, the count +2, the "
            "first 7 weapons; kept -2, had -1); the weapon tables exe+0x13948FC / counts exe+0x139491C (6-byte entries: "
            "stage +0, item +4)");
      using RowGet = uintptr_t(__thiscall*)(void*, int);
      using RowOf = int(__thiscall*)(void*, uint32_t, int);
      using UsesGet = uint32_t(__thiscall*)(void*, uint32_t);
      using CarriedGet = uintptr_t(__thiscall*)(void*, uint32_t);
      using Reset = void(__thiscall*)(void*);
      const auto row_get = reinterpret_cast<RowGet>(0x564750);
      const auto row_of = reinterpret_cast<RowOf>(0x564670);
      const auto uses_get = reinterpret_cast<UsesGet>(0xE43C20);
      const auto carried_get = reinterpret_cast<CarriedGet>(0xD70B20);
      const auto reset = reinterpret_cast<Reset>(0x56CF30);
      // The page names a row when its record's byte says always or Times Used is above 0 - the game's own test.
      auto page_names = [&](int e) {
        const uintptr_t r = row_get(iv, e);
        return r && (mem::read<uint8_t>(r + 2) || uses_get(pv, static_cast<uint32_t>(e)) > 0);
      };
      tag.field<uint8_t>(rl.step) = 10;  // the Dog Tag menu: no list up
      rec.field<uint8_t>(rl.step) = 2;
      rec.field<uint32_t>(rl.page) = 2;
      check(screen() == records::Screen::kEnemies, "the Records screen's ENEMIES page: the enemies window, not this one");
      rec.field<uint32_t>(rl.page) = 1;
      check(screen() == records::Screen::kWeapons, "the WEAPONS page: the weapons window");
      records::View wv = records::view();
      check(wv.nweapon_rows == 28 && wv.nweapon_always == 15 && wv.weapon_row[0].entry == 3 &&
                wv.weapon_row[27].entry == 40 && wv.carried_known,
            "28 rows it can draw \"? ? ?\" (from the Shotgun, 3, to Special Attack - Other, 40) and 15 it always names, "
            "as the game's table has them");
      bool none_ready = true;
      for (int c = 0; c < records::kCarriers; ++c) none_ready = none_ready && !wv.carrier[c].ready;
      check(none_ready, "a profile whose saved inventories are not set up (all zero): no character's ticks");
      // The game's own New Game reset, each campaign's two: Leon's at stage 202 (his chapter 2 - the Shotgun and the
      // Semi-Auto Sniper Rifle due by then), the others at their first stage (nothing due).
      std::vector<uint8_t> gman(0x5000, 0);
      const uint32_t kGameManG = 0x17CF454, gm_was = mem::read<uint32_t>(kGameManG);
      mem::write<uintptr_t>(kGameManG, reinterpret_cast<uintptr_t>(gman.data()));
      const uintptr_t gi = rinfo.at() + rl.gi_base + rl.gi_size;  // profile 1's cGameInfo
      const int first_stage[4] = {202, 1110, 1101, 1130};
      for (int e = 0; e < 4; ++e) {
        mem::write<uint32_t>(gi + 0x1AF8, static_cast<uint32_t>(e));  // mEpisodeNo
        mem::write<uint32_t>(gi + 0x1B0C, static_cast<uint32_t>(first_stage[e]));  // mStageNo
        reset(iv);
      }
      mem::write<uint32_t>(kGameManG, gm_was);
      records::tick(menus);
      wv = records::view();
      const records::Carrier& leon = wv.carrier[0];
      check(leon.ready && leon.n == 4 && leon.weapon[0].id == 0x104 && leon.weapon[1].id == 0x10A &&
                leon.weapon[2].id == 0x111 && leon.weapon[3].id == 0x107 && leon.weapon[0].had && leon.weapon[1].had &&
                !leon.weapon[2].had && !leon.weapon[3].had && leon.weapon[0].count == -1,
            "Leon as the game's reset leaves him at stage 202: the Shotgun and Semi-Auto Sniper Rifle had (a full "
            "magazine), the Assault Rifle RN and Lightning Hawk not - his four found on the way, in his table's order");
      int pairs = 0, kept = 0;
      bool ready = true, rows_agree = true;
      for (int c = 0; c < records::kCarriers; ++c) {
        ready = ready && wv.carrier[c].ready;
        pairs += wv.carrier[c].n;
        for (int k = 0; k < wv.carrier[c].n; ++k) {
          kept += !wv.carrier[c].weapon[k].had;
          rows_agree = rows_agree && wv.carrier[c].weapon[k].entry == row_of(iv, wv.carrier[c].weapon[k].id, 0);
        }
      }
      check(ready && pairs == 28 && kept == 24 && wv.carrier[2].n == 3 && wv.carrier[7].n == 3 && rows_agree &&
                wv.carrier[1].weapon[0].had && !wv.carrier[1].weapon[3].had,
            "every character set up: 28 weapons found on the way (Chris and the Agent 3 each), 24 not had (Helena has "
            "Leon's two too: the reset covers the campaign's pair); each one's row the game's own lookup gives it");
      // The page: a row named, one taken back, one used in play kept.
      mem::write<uint32_t>(prof + wl.uses + wl.stride * 3, 5);   // the Shotgun: Times Used 5
      mem::write<uint32_t>(prof + wl.uses + wl.stride * 4, 1);   // the Assault Shotgun: one use and a kill
      mem::write<uint32_t>(prof + wl.kills + wl.stride * 4, 1);
      mem::write<uint32_t>(prof + wl.uses + wl.stride * 19, 1);  // the Incendiary Grenade: one use
      check(!page_names(6) && page_names(3) && page_names(0) && !page_names(17), "the game's own test: the Lightning "
            "Hawk and Grenade Launcher \"? ? ?\", the Shotgun named (5 uses), the 909 always");
      const std::vector<records::WeaponSet> ws = {{6, true}, {17, true}, {3, false}, {4, false}, {19, false}, {0, false}};
      records::set_weapon_rows(ws.data(), static_cast<int>(ws.size()));
      records::tick(menus);
      check(page_names(6) && page_names(17) && uses_get(pv, 6) == 1 && uses_get(pv, 17) == 1,
            "the Lightning Hawk and Grenade Launcher named: their first use (Times Used 1), as the game's own test sees");
      check(uses_get(pv, 3) == 5 && uses_get(pv, 4) == 1 && uses_get(pv, 19) == 0 && page_names(0) &&
                records::g_save != records::Save::kIdle,
            "taken back: the Incendiary Grenade's one use (\"? ? ?\" again); kept: the Shotgun's 5 and the Assault "
            "Shotgun's use with a kill (play); the 909 always named; a save asked for");
      check(finish_save(), "... its save run to the end");
      // The saved inventories: Leon's Assault Rifle RN given, his Shotgun (in hand) taken out, an Assault Shotgun
      // (not his campaign's) refused; Helena's Lightning Hawk given.
      const uintptr_t lrec = carried_get(reinterpret_cast<void*>(gi), 0);
      auto count_of = [&](uintptr_t r, uint16_t id) -> int {
        for (int s = 0; s < 7; ++s)
          if ((mem::read<uint16_t>(r + 0x10 + 4 * s) & 0xFFF) == id) return mem::read<int16_t>(r + 0x12 + 4 * s);
        return -100;
      };
      int shotgun_slot = -1;
      for (int s = 0; s < 7; ++s)
        if ((mem::read<uint16_t>(lrec + 0x10 + 4 * s) & 0xFFF) == 0x104) shotgun_slot = s;
      mem::write<uint8_t>(lrec + 7, static_cast<uint8_t>(shotgun_slot));
      const uintptr_t lsaved = gi + 4 + (lrec - (gi + 0x1AF4));  // Leon's record in the part the save keeps
      const int saved_before = count_of(lsaved, 0x111);
      const std::vector<records::CarrySet> cs = {{0, 0x111, true}, {0, 0x104, false}, {0, 0x105, true}, {1, 0x107, true}};
      records::set_carried(cs.data(), static_cast<int>(cs.size()));
      records::tick(menus);
      const uintptr_t hrec = carried_get(reinterpret_cast<void*>(gi), 1);
      check(count_of(lrec, 0x111) == -1 && count_of(lrec, 0x104) == -2 && count_of(lrec, 0x105) == -100 &&
                count_of(hrec, 0x107) == -1,
            "Leon's Assault Rifle RN given (a full magazine) and his Shotgun taken out, in his saved inventory as the "
            "game's getter finds it; no Assault Shotgun (not his campaign's); Helena's Lightning Hawk given");
      check(shotgun_slot >= 0 && mem::read<uint8_t>(lrec + 7) == 0 && count_of(lrec, 0x103) == -1,
            "... the hand, on the Shotgun, moved to the first weapon still had (slot 0, the Wing Shooter)");
      check(!records::g_dirty(pv) && records::g_save == records::Save::kStorage &&
                !std::memcmp(reinterpret_cast<void*>(gi + 4), reinterpret_cast<void*>(gi + 0x1AF4), 0x1AF0) &&
                saved_before == -2 && count_of(lsaved, 0x111) == -1,
            "the profile's cSystemInfo unchanged, yet the save asked for: the cGameInfo copied into the part the save "
            "keeps (the game's own copy: Leon's Assault Rifle RN -2 there before, -1 now), the storage asked");
      check(finish_save(), "... its save run to the end");
      // Back as they were, for the skills window's checks below: the counts, and the Records screen closing.
      for (int e : {3, 4, 6, 17, 19}) {
        mem::write<uint32_t>(prof + wl.uses + wl.stride * e, 0);
        mem::write<uint32_t>(prof + wl.kills + wl.stride * e, 0);
      }
      rec.field<uint32_t>(rl.page) = 3;
      rec.field<uint8_t>(rl.step) = records::kStepClosing;
      check(screen() == records::Screen::kNone, "the Records screen closing: no window");
    }

    // The enemies window: the Records screen's ENEMIES page (page 2). The page's own reads run in the copy - its row's
    // record (the first of its row in the enemy table, 0x564860), Enemies Defeated (0xE43FE0) and Times Defeated
    // (0xE44000) - and the page's own test of a row it names.
    {
      const records::EnemyLay& el = records::E;
      check(records::g_enemies && el.page == 2 && el.table == 0x1795C18 && el.count == 0x1794BF4 && el.rec == 8 &&
                el.r_entry == 0 && el.r_always == 2 && el.r_no_defeats == 4 && el.kills == 0x1328 &&
                el.defeated == 0x132C && el.stride == 8 && el.nstats == 0x5A && el.name_base == 3700 && el.rows == 86,
            "the ENEMIES page: page 2, the enemy table exe+0x1395C18 (8-byte records: row +0, always named +2, no "
            "Times Defeated +4), Enemies Defeated +0x1328 and Times Defeated +0x132C every 8 (0x5A), names from 3700, "
            "86 rows - the skills' kill table and counts the same");
      using RowGet = uintptr_t(__thiscall*)(void*, int);
      using CountGet = int32_t(__thiscall*)(void*, uint32_t);
      const auto row_get = reinterpret_cast<RowGet>(0x564860);
      const auto kills_get = reinterpret_cast<CountGet>(0xE43FE0);
      const auto defeated_get = reinterpret_cast<CountGet>(0xE44000);
      // The page names a row when its record's byte says always or either count is above 0 - the game's own test.
      auto page_names = [&](int e) {
        const uintptr_t r = row_get(iv, e);
        return r && (mem::read<uint8_t>(r + 2) || kills_get(pv, static_cast<uint32_t>(e)) > 0 ||
                     defeated_get(pv, static_cast<uint32_t>(e)) > 0);
      };
      auto kills_at = [&](int e) -> int32_t& { return *reinterpret_cast<int32_t*>(prof + el.kills + 8 * e); };
      auto defeated_at = [&](int e) -> int32_t& { return *reinterpret_cast<int32_t*>(prof + el.defeated + 8 * e); };
      for (int e = 0; e < 0x5A; ++e) kills_at(e) = defeated_at(e) = 0;
      kills_at(74) = 3;      // Brzak: three kills
      defeated_at(39) = 2;   // Ustanak: no kill, it has defeated you twice
      kills_at(80) = -1;     // the Crow: a count below 0 (none of the game's)
      rec.field<uint8_t>(rl.step) = 2;
      rec.field<uint32_t>(rl.page) = 2;
      check(screen() == records::Screen::kEnemies, "the ENEMIES page: the enemies window");
      records::View ev = records::view();
      bool shown = ev.nenemy_rows == 84 && ev.nenemy_always == 2 && ev.enemy_row[0].entry == 1 &&
                   ev.enemy_row[83].entry == 85;
      for (int i = 0; i < ev.nenemy_rows && shown; ++i) {
        const records::EnemyRow& r = ev.enemy_row[i];
        const uintptr_t g = row_get(iv, r.entry);
        shown = g && !mem::read<uint8_t>(g + 2) && r.kills == kills_get(pv, r.entry) &&
                r.defeated == defeated_get(pv, r.entry) && r.shows_defeated == !mem::read<uint8_t>(g + 4);
      }
      check(shown && !ev.enemy_row[9].shows_defeated && ev.enemy_row[0].shows_defeated &&
                !std::strcmp(records_text::kEnemyName[ev.enemy_row[0].entry], "Neo-Umbrella J'avo"),
            "84 rows it can draw \"? ? ?\" (Neo-Umbrella J'avo, 1, to The Witch, 85) and 2 it always names (J'avo, "
            "Zombie); each one's counts as the game's own gets read them, Times Defeated where the page shows it (not "
            "the Chrysalid's)");
      check(!page_names(13) && page_names(39) && page_names(74) && page_names(0) && page_names(43) && !page_names(80) &&
                !page_names(85),
            "the game's own test: Glava-Begunats, the Crow and The Witch \"? ? ?\", Ustanak named by its Times Defeated, "
            "Brzak by its kills, J'avo and Zombie always");
      // A reveal asked for on another page: not made.
      rec.field<uint32_t>(rl.page) = 1;
      const uint8_t early = 13;
      records::reveal_enemies(&early, 1);
      records::tick(menus);
      check(kills_at(13) == 0 && records::g_save == records::Save::kIdle,
            "a reveal asked for with the WEAPONS page up: dropped (nothing written, no save)");
      rec.field<uint32_t>(rl.page) = 2;
      const std::vector<uint8_t> es = {13, 39, 74, 0, 80, 85, 86, 200};
      records::reveal_enemies(es.data(), static_cast<int>(es.size()));
      records::tick(menus);
      check(page_names(13) && page_names(80) && page_names(85) && kills_get(pv, 13) == 1 && kills_get(pv, 80) == 1 &&
                kills_get(pv, 85) == 1,
            "Glava-Begunats, the Crow (below 0) and The Witch named: their first kill (Enemies Defeated 1), as the "
            "game's own test sees");
      check(kills_get(pv, 39) == 0 && defeated_get(pv, 39) == 2 && kills_get(pv, 74) == 3 && kills_get(pv, 0) == 0 &&
                kills_at(86) == 0 && records::g_save != records::Save::kIdle,
            "left as they were: Ustanak (named by its Times Defeated, no kill given), Brzak's 3, J'avo (always named); "
            "nothing past the page's rows; a save asked for");
      check(finish_save(), "... its save run to the end");
      // Reveal all, as the window asks it: every row the page draws "? ? ?" - then the page names every row, and no
      // count went down.
      std::vector<uint8_t> all;
      ev = records::view();
      for (int i = 0; i < ev.nenemy_rows; ++i)
        if (ev.enemy_row[i].kills < 1 && ev.enemy_row[i].defeated < 1) all.push_back(ev.enemy_row[i].entry);
      records::reveal_enemies(all.data(), static_cast<int>(all.size()));
      records::tick(menus);
      bool every = all.size() == 79;
      for (int e = 0; e < 86 && every; ++e) every = page_names(e);
      check(every && kills_get(pv, 74) == 3 && kills_get(pv, 39) == 0 && kills_get(pv, 0) == 0 &&
                std::strstr(records::view().note, "79 enemy row(s) named"),
            "Reveal all: the other 79 \"? ? ?\" rows named - the game's own test names all 86; Brzak's 3, Ustanak's 0 "
            "and J'avo's 0 kept");
      check(finish_save(), "... its save run to the end");
      for (int e = 0; e < 0x5A; ++e) kills_at(e) = defeated_at(e) = 0;
      rec.field<uint32_t>(rl.page) = 3;
      rec.field<uint8_t>(rl.step) = records::kStepClosing;
      check(screen() == records::Screen::kNone, "the Records screen closing: no window");
    }

    // The skills window: the skill screen (uIdSkillSet10) as a unit after the Dog Tag screen, its list filled by the
    // game's own fill (its start's and each purchase's) over the profile in use (1) - which asks the game's own "can
    // be bought" test and the counts it reads, all run in the copy.
    {
      const records::SkillLay& kl = records::K;
      check(records::g_skills, "the skills understood");
      check(kl.table == 0x1795280 && kl.count == 0x1794B24 && kl.rec_size == 20 && kl.r_group == 0 && kl.r_level == 1 &&
                kl.r_top == 2 && kl.r_mask == 3 && kl.r_id == 4 && kl.r_price == 8 && kl.r_need == 12 &&
                kl.r_param == 14 && kl.r_goal == 16 && kl.states == 0x6FC && kl.nstates == 0x61 && kl.bought == 3,
            "the skill table exe+0x1395280 (records of 20 bytes: the skill's row +0, level +1, top level +2, the lists' "
            "mask +3, its id +4, price +8, requirement +12, its kind +14 and goal +16); the skill states cSystemInfo "
            "+0x6FC (0x61; 3 bought)");
      check(kl.need_previous == 1 && kl.need_kills == 2 && kl.need_weapons == 3 && kl.need_campaigns == 4 &&
                kl.campaigns_goal == 4 && kl.kinds_with_needs == 1,
            "the requirements as the game tests them: 1 the level before bought, 2 kills of an enemy kind, 3 kills "
            "with a kind of weapon, 4 the four campaigns complete; only the campaigns' list (kind 0) has any");
      check(kl.screen == mt::find("uIdSkillSet10") && kl.step == 0x38 && kl.kind == 0x220 && kl.user == 0x228 &&
                kl.rows == 0x2BC && kl.nrows == 0x2B4 && kl.rows_most == 0x2C && kl.row_state == 4 && kl.row_need == 5 &&
                kl.row_can == 6 && kl.row_price == 8,
            "the skill screen: its step +0x38, kind of list +0x220, user +0x228, rows +0x2BC (16 bytes, 0x2C at most, "
            "how many +0x2B4: a row's state +4, requirement +5, can be bought +6, price +8)");
      check(kl.kill_table == 0x1795C18 && kl.kill_count == 0x1794BF4 && kl.kill_rec == 8 && kl.k_index == 0 &&
                kl.k_key1 == 3 && kl.k_key2 == 5 && kl.k_kind == 6 && kl.kill_key1 == 0 && kl.kill_key2 == 3 &&
                kl.kills == 0x1328 && kl.nkills == 0x5A && kl.kill_group[0].kind == 0x1000 && kl.kill_group[0].n == 2 &&
                kl.kill_group[0].of[1] == 0x1400 && kl.kill_group[1].kind == 0x5000 && kl.kill_group[1].n == 6 &&
                kl.kill_group[1].of[1] == 0x5420 && kl.kill_group[1].of[5] == 0x5700,
            "the kills: the kill table exe+0x1395C18 (8 bytes: the entry +0, keys +3 (0) and +5 (3), the kind +6), "
            "counts cSystemInfo +0x1328 (0x5A); 0x1000 with 0x1400 (J'avo Killer's count), 0x5000 with five more "
            "(Zombie Hunter's)");
      bool groups = kl.nweapon_groups == 7;
      const uint16_t want[7][5] = {{0x101, 0x101, 0x102, 0x103, 0x10D}, {0x104, 0x104, 0x105, 0x106, 0},
                                   {0x107, 0x107, 0x108, 0, 0},         {0x109, 0x109, 0x10A, 0x10B, 0},
                                   {0x10C, 0x10C, 0x10E, 0, 0},         {0x10F, 0x10F, 0x110, 0x111, 0},
                                   {0x113, 0x113, 0x114, 0x115, 0x116}};
      for (int g = 0; g < 7 && groups; ++g) {
        int n = 0;
        while (n < 4 && want[g][1 + n]) ++n;
        groups = kl.weapon_group[g].kind == want[g][0] && kl.weapon_group[g].n == n;
        for (int i = 0; i < n && groups; ++i) groups = kl.weapon_group[g].of[i] == want[g][1 + i];
      }
      check(groups && kl.weapon_table == 0x17959B0 && kl.weapon_count == 0x1794B8C && kl.weapon_rec == 12 &&
                kl.w_weapon == 8 && kl.weapon_kills == 0xA94 && kl.weapon_stride == 0x2C && kl.nweapon_kills == 0x32,
            "the weapon kills: the weapon table exe+0x13959B0 (12 bytes: the entry +0, the weapon +8), kills "
            "cSystemInfo +0xA94 every 0x2C (0x32); the game's switch: handguns 0x101 (+0x102 0x103 0x10D), shotguns, "
            "Magnums, rifles, machine pistols, assault rifles, grenades 0x113 (+0x114-0x116); others alone");

      using Fill = void(__thiscall*)(void*);
      using Avail = uint8_t(__thiscall*)(void*, uint32_t);
      using Count = int(__thiscall*)(void*, uint32_t);
      using Find = int(__thiscall*)(void*, uint32_t, uint32_t, uint32_t);
      const auto fill = reinterpret_cast<Fill>(0x9F24D0);
      const auto avail = reinterpret_cast<Avail>(0x566AB0);
      const auto kills_of = reinterpret_cast<Count>(0x5666E0);
      const auto weapons_of = reinterpret_cast<Count>(0x5668D0);
      const auto kill_find = reinterpret_cast<Find>(0x5647B0);
      // A kill count and a weapon's kills written where the game's own gets read them.
      auto set_kills = [&](uint32_t kind, int32_t n) {
        const int e = kill_find(iv, 0, 3, kind);
        if (e >= 0) mem::write<int32_t>(prof + 0x1328 + 8 * e, n);
        return e >= 0;
      };
      auto set_weapon = [&](uint16_t weapon, int32_t n) {
        for (uint32_t i = 0; i < mem::read<uint32_t>(0x1794B8C); ++i)
          if (mem::read<uint16_t>(0x17959B0 + 12 * i + 8) == weapon) {
            mem::write<int32_t>(prof + 0xA94 + 0x2C * static_cast<int8_t>(mem::read<uint8_t>(0x17959B0 + 12 * i)), n);
            return true;
          }
        return false;
      };
      const mt::Class* sk = kl.screen;
      Obj skill(sk->size, sk->vtable), fresh(sk->size, sk->vtable);
      // Whether the screen's row of a skill says it can be bought.
      auto row_can = [&](uint8_t id) {
        for (int i = 0; i < skill.field<int32_t>(kl.nrows) && i < 0x2C; ++i)
          if (skill.field<uint8_t>(kl.rows + 16 * i) == id) return skill.field<uint8_t>(kl.rows + 16 * i + 6) != 0;
        return false;
      };
      // No kills counted, three campaigns complete (Leon's, Chris', Jake's: every chapter on Amateur by the first
      // character), Ada's not.
      std::memset(reinterpret_cast<void*>(prof + 0x1328), 0, 8 * 0x5A);
      std::memset(reinterpret_cast<void*>(prof + 0xA94), 0, 0x2C * 0x32);
      for (int r = 0; r < records::kChars * records::kDifficulties * records::kChapters; ++r)
        mem::write<uint32_t>(prof + rl.clear + 12 * r, 0);
      for (int st = 0; st < 3; ++st)
        for (int ch = 0; ch < records::kChapters; ++ch)
          mem::write<uint32_t>(prof + rl.clear + 12 * ((st * 2 * records::kDifficulties + 0) * records::kChapters + ch), 1);
      skill.field<uint32_t>(ul.unit_flags) = 2;
      tag.field<uintptr_t>(ul.unit_next) = skill.at();
      check(start_screen(skill), "the skill screen starts (through the mod's hook)");
      skill.field<uint32_t>(kl.kind) = 0;  // the campaigns' list
      skill.field<uint32_t>(kl.user) = 1;  // the profile in use here
      for (int i = 0; i < 0x61; ++i) mem::write<uint8_t>(prof + kl.states + i, 0);
      fill(reinterpret_cast<void*>(skill.at()));
      const int rows = skill.field<int32_t>(kl.nrows);
      check(rows > 20 && rows <= 0x2C && skill.field<uint8_t>(kl.rows) == 1,
            "the game's own fill of the screen's list in the copy: the campaigns' rows (Firearm first)");
      tag.field<uint8_t>(rl.step) = 10;
      skill.field<uint8_t>(kl.step) = 0;
      check(screen() == records::Screen::kNone, "the skill screen opening (step 0): no window");
      skill.field<uint8_t>(kl.step) = 0x1E;
      check(screen() == records::Screen::kSkills, "the skill screen's list: the skills window");
      records::View sv = records::view();
      check(sv.nskills == 19 && sv.skill[0].id == 16 && sv.skill[1].id == 18 && sv.skill[2].id == 20 &&
                sv.skill[10].id == 28 && sv.skill[11].id == 29 && sv.skill[18].id == 36 &&
                sv.skill[0].need == records::Need::kKills && sv.skill[0].goal == 30 && sv.skill[0].price == 3000 &&
                sv.skill[3].need == records::Need::kWeaponKills && sv.skill[3].goal == 1500 &&
                sv.skill[11].need == records::Need::kCampaigns && sv.skill[11].goal == 4 &&
                !std::strcmp(records_text::kSkillName[16], "J'avo Killer Lv. 1") &&
                !std::strcmp(records_text::kSkillNeed[21], "Get 1,500 kills with handguns"),
            "19 skills behind a requirement, in the list's order: J'avo Killer Lv. 1 (kill 30 J'avo, 3,000 points), "
            "Zombie Hunter Lv. 1, Grenade Power-Up, the eight masters (Handgun Master: 1,500 kills), the eight "
            "infinite weapons (the four campaigns)");

      // The counts: Zombie Hunter's through two of the six kinds the game adds up (30: met), J'avo Killer's one
      // short; the handguns through two of their four (1499: short), the shotguns (one of three: met), the grenades
      // (0x116 of 0x113-0x116: met), the grenade launcher alone (met), the crossbow one short. The campaigns: three
      // complete.
      const bool counts_set = set_kills(0x5420, 10) && set_kills(0x5700, 20) && set_kills(0x1400, 29) &&
                              set_weapon(0x102, 1000) && set_weapon(0x10D, 499) && set_weapon(0x105, 1000) &&
                              set_weapon(0x116, 800) && set_weapon(0x112, 800) && set_weapon(0x117, 799);
      fill(reinterpret_cast<void*>(skill.at()));
      records::tick(menus);
      sv = records::view();
      bool agree = counts_set && sv.nskills == 19;
      int met = 0;
      for (int k = 0; k < sv.nskills && agree; ++k) {
        const records::Skill& s = sv.skill[k];
        uint32_t rec = 0;
        while (rec < mem::read<uint32_t>(kl.count) && mem::read<uint8_t>(kl.table + 20 * rec + 4) != s.id) ++rec;
        const uint16_t param = mem::read<uint16_t>(kl.table + 20 * rec + 14);
        const int game = s.need == records::Need::kKills         ? kills_of(iv, param)
                         : s.need == records::Need::kWeaponKills ? weapons_of(iv, param)
                                                                 : s.count;
        agree = (avail(iv, rec) != 0) == s.met && s.count == game && s.state == 0;
        met += s.met;
      }
      check(agree && met == 4 && !sv.skill[0].met && sv.skill[0].count == 29 && sv.skill[1].met &&
                sv.skill[1].count == 30 && sv.skill[2].met && !sv.skill[3].met && sv.skill[3].count == 1499 &&
                sv.skill[4].met && sv.skill[9].met && !sv.skill[10].met && sv.skill[10].count == 799 &&
                !sv.skill[11].met && sv.skill[11].count == 3,
            "each skill's count and met as the game's own counts and its \"can be bought\" test have them: J'avo "
            "Killer short (29), Zombie Hunter met (30), Grenade Power-Up and Shotgun Master met through their groups, "
            "Handgun Master short (1499), Grenade Launcher Master met alone, Crossbow Master short (799), the "
            "campaigns 3 of 4 - 4 met");
      // Unlock four: Zombie Hunter (met already: sold, nothing to do), J'avo Killer, Handgun Master, Infinite
      // Handgun - their rows as the game's own fill leaves them for that profile.
      const uint32_t points_was = mem::read<uint32_t>(prof + 0x6F4);
      // The profile as a save leaves it (its saved copy the working one): the game's own "changed?" says no.
      using Dirty = uint8_t(__thiscall*)(void*);
      const auto dirty = reinterpret_cast<Dirty>(0xE43150);
      std::memcpy(reinterpret_cast<void*>(prof + 0x7EF0), reinterpret_cast<void*>(prof + 8), 0x7EE8);
      const bool saved_before = !dirty(pv);
      std::vector<uint8_t> before(rl.profile_size);
      std::memcpy(before.data(), reinterpret_cast<void*>(prof), rl.profile_size);
      const records::SkillSet un[4] = {{16, true}, {18, true}, {21, true}, {29, true}};
      records::set_skills(un, 4);
      records::tick(menus);
      int diff = 0;
      for (uint32_t i = 0; i < rl.profile_size; ++i)
        if (mem::read<uint8_t>(prof + i) != before[i] && (i < kl.states || i >= kl.states + kl.nstates)) ++diff;
      check(mem::read<uint8_t>(prof + kl.states + 16) == 2 && mem::read<uint8_t>(prof + kl.states + 18) == 0 &&
                mem::read<uint8_t>(prof + kl.states + 21) == 2 && mem::read<uint8_t>(prof + kl.states + 29) == 2 &&
                diff == 0 && mem::read<uint32_t>(prof + 0x6F4) == points_was,
            "unlocked: J'avo Killer, Handgun Master, Infinite Handgun at state 2 (the state the screen's start leaves "
            "a new one in); Zombie Hunter (met) left at 0; nothing else in the profile changed - the skill points "
            "neither");
      auto rows_agree = [&] {
        fresh.field<uint32_t>(kl.kind) = 0;
        fill(reinterpret_cast<void*>(fresh.at()));
        return !std::memcmp(reinterpret_cast<void*>(skill.at() + kl.rows), reinterpret_cast<void*>(fresh.at() + kl.rows),
                            16 * 0x2C) &&
               skill.field<int32_t>(kl.nrows) == fresh.field<int32_t>(kl.nrows);
      };
      check(rows_agree() && row_can(18) && row_can(21) && row_can(29) && row_can(16),
            "the screen's rows as the game's own fill makes them from the profile now (the three unlocked and Zombie "
            "Hunter's say they can be bought)");
      check(std::strstr(records::view().note, "3 skill(s) unlocked") && std::strstr(records::view().note, "1 kept"),
            "the note: 3 unlocked, 1 kept (its requirement is met)");
      check(saved_before && dirty(pv) && records::g_save == records::Save::kIdle,
            "the profile changed as the game's own test sees it (0xE43150, the menus' save's first step): the skill "
            "screen's close commits and saves it, as it does a purchase - no save of the mod's asked");
      // Lock again: J'avo Killer (unlocked, short of its goal) - locked; Shotgun Master (met) - kept; Magnum
      // Master bought - kept.
      mem::write<uint8_t>(prof + kl.states + 23, 3);
      fill(reinterpret_cast<void*>(skill.at()));
      const records::SkillSet lock[3] = {{16, false}, {22, false}, {23, false}};
      records::set_skills(lock, 3);
      records::tick(menus);
      check(mem::read<uint8_t>(prof + kl.states + 16) == 0 && mem::read<uint8_t>(prof + kl.states + 22) == 0 &&
                mem::read<uint8_t>(prof + kl.states + 23) == 3 && rows_agree() && !row_can(16) && row_can(22) &&
                std::strstr(records::view().note, "1 skill(s) locked again") &&
                std::strstr(records::view().note, "2 kept"),
            "locked again: J'avo Killer (0, its row \"? ? ?\" again); Shotgun Master's requirement met (sold all the "
            "same) and Magnum Master bought: kept; the rows as the game's fill makes them");
      // A level after the one the requirement unlocks: J'avo Killer Lv. 2 sells once Lv. 1 is bought (the game's
      // own rule) - the window lists only Lv. 1.
      const records::SkillSet lv2 = {17, true};
      records::set_skills(&lv2, 1);
      records::tick(menus);
      check(mem::read<uint8_t>(prof + kl.states + 17) == 0, "J'avo Killer Lv. 2 (needs Lv. 1 bought): not a skill this "
                                                            "window unlocks - nothing");
      // The Mercenaries' list (kind 1): none behind a requirement - no window; a user past the profiles: none; in a
      // game (a chapter's end): the window; the screen closing: none, a change asked then dropped.
      skill.field<uint32_t>(kl.kind) = 1;
      check(screen() == records::Screen::kNone, "the Mercenaries' skill list: no window (none behind a requirement)");
      skill.field<uint32_t>(kl.kind) = 0;
      skill.field<uint32_t>(kl.user) = 2;
      check(screen() == records::Screen::kNone, "a user past the two profiles: no window");
      skill.field<uint32_t>(kl.user) = 1;
      records::tick(playing);
      check(records::view().screen == records::Screen::kSkills && records::view().nskills == 19,
            "in a game (the skill screen at a chapter's end): the window");
      const records::SkillSet late = {20, true};
      skill.field<uint8_t>(kl.step) = records::kStepClosing;
      records::set_skills(&late, 1);
      check(screen() == records::Screen::kNone && mem::read<uint8_t>(prof + kl.states + 20) == 0,
            "the skill screen closing (its own save): no window, a change asked then dropped");
      check(records::g_save == records::Save::kIdle, "no save of the mod's: the skill screen's own close saves");
      skill.field<uint8_t>(kl.step) = 0x1E;
      check(screen() == records::Screen::kSkills && end_screen(skill) && screen() == records::Screen::kNone,
            "the skill screen destroyed (through the mod's hook): no window, at once");
      tag.field<uintptr_t>(ul.unit_next) = 0;
    }

    // The Mercenaries' unlocks window: the stage select (uIdStartUp31) and the character select (uIdStartUp32),
    // started through the mod's hooks, in the Mercenaries and No Mercy - a made-up sGameMan at the game's global (the
    // mode, the play type, the flag No Mercy's own four go by) and sId (the screens' list calls) - over the profile in
    // use (1). The game's own getters, the character select's start and the stage select's side panel fill run in
    // the copy, and the mod's view, its changes and its fills of the screens are checked against them.
    {
      const records::MercLay& ml = records::M;
      check(records::g_merc, "the Mercenaries' unlocks understood");
      check(ml.stages == 0x970 && ml.chars == 0x978 && ml.stage_always == 0x1C07 && ml.stage_copied == 0xFE3F8 &&
                ml.first[0] == 0 && ml.first[1] == 10 && ml.rows == 10 && ml.stage_table == 0x1797230 &&
                ml.stage_count == 0x179722C && ml.stage_rec == 10,
            "the stages cSystemInfo +0x970 - always open 0x1C07 (each mode's first three), the profile's bits 3-9 and "
            "13-19 (the getter's chain walked); ten rows a mode from bit 0 and bit 10; the stage table exe+0x1397230");
      check(ml.char_always == 0x20010101 && ml.char_extras == 0x3C0000000ull && ml.extras_flag == 0xB4 &&
                ml.extras_bit == 1 && ml.mode_nomercy == 0x50 && ml.char_settable == 0x1FFEFEFE,
            "the characters cSystemInfo +0x978 (64 bits): always Leon's, Chris' and Jake's Default and the Agent; No "
            "Mercy's own four (bits 30-33) by sGameMan +0xB4 bit 0; the profile's bits 1-7, 9-15, 17-28");
      check(ml.pages[0] == 0x1767324 && ml.pages[1] == 0x1767260 && ml.npages[0] == 3 && ml.npages[1] == 3 &&
                ml.page_size == 10 && ml.p_counts == 0x16C && ml.p_masks == 0x178 && ml.p_ids == 0x184 &&
                ml.p_ids_page == 0x24 && ml.char_ids[0] == 0x1797304 && ml.char_ids[1] == 0x179739C &&
                ml.char_count[0] == 0x1797444 && ml.char_count[1] == 0x1797448 && ml.char_rec == 5,
            "the stage select's side panel: counts +0x16C, masks +0x178, ids +0x184 (0x24 bytes a page); each mode's "
            "pages (3, 10 bytes each), the character tables (5 bytes a record, its select id +4)");
      check(ml.stage_screen == mt::find("uIdStartUp31") && ml.char_screen == mt::find("uIdStartUp32") &&
                ml.cs_mask == 0x168 && ml.cs_rows == 0x178 && ml.cs_cursor == 0x170 && ml.cs_nrows == 0x174 &&
                ml.cs_ccursor == 0x1AC && ml.cs_counts == 0x1E0 && ml.cs_lists == 0x214 && ml.cs_list_size == 16 &&
                ml.fam_bits == 0x1767218 && ml.fam_counts == 0x1767208 && ml.families == 13 && ml.fam_most == 4 &&
                ml.fam_stride == 4,
            "the character select: its copy of the characters available +0x168, rows +0x178 (cursor +0x170, how "
            "many +0x174), costume cursors +0x1AC, how many +0x1E0, lists +0x214 (16 bytes a character); 13 "
            "characters, 4 costumes each at most (the tables exe+0x1367218, exe+0x1367208)");
      const int want_file[][2] = {{1, 0x38},  {5, 0x39},  {9, 0x3A},  {13, 0x3B}, {17, 0x3C}, {21, 0x3D}, {25, 0x37},
                                  {2, 63},    {3, 70},    {6, 64},    {7, 71},    {10, 65},   {11, 72},   {14, 66},
                                  {15, 73},   {18, 67},   {19, 74},   {22, 68},   {23, 75},   {26, 62},   {27, 69}};
      int8_t expect[64];
      std::memset(expect, -1, sizeof(expect));
      for (const auto& wf : want_file) expect[wf[0]] = static_cast<int8_t>(wf[1]);
      check(!std::memcmp(ml.file, expect, sizeof(expect)),
            "the Collection files with the costumes: Costume 1's from the match end's seven awards (Leon 0x38 ... "
            "Ada 0x37), Costumes 2 and 3's from the online event's table (62-75); none with the rest");

      // The game's globals the screens and getters read: sGameMan (as its reset leaves its flag: 1) and sId.
      const mt::Class* gmc = mt::find("sGameMan");
      const mt::Class* idc = mt::find("sId");
      Obj gm(gmc->size, gmc->vtable), sid(idc->size, idc->vtable);
      const uint32_t kGameManG = 0x17CF454, kIdG = 0x17CF478;
      const uint32_t gm_was = mem::read<uint32_t>(kGameManG), id_was = mem::read<uint32_t>(kIdG);
      const uintptr_t sgameman_was = game::g_sgameman;
      mem::write<uintptr_t>(kGameManG, gm.at());
      mem::write<uintptr_t>(kIdG, sid.at());
      game::g_sgameman = gm.at();
      const uint32_t gmode = game::layout().mode;
      gm.field<uint32_t>(ml.extras_flag) = 1;
      gm.field<uint32_t>(gmode) = game::kModeMercenaries;
      using StagesFn = uint32_t(__thiscall*)(void*);
      using CharsFn = uint64_t(__thiscall*)(void*);
      using ScreenFn = void(__thiscall*)(void*);
      const auto game_stages = reinterpret_cast<StagesFn>(0x563A00);
      const auto game_chars = reinterpret_cast<CharsFn>(0x563AF0);
      const auto panel_fill = reinterpret_cast<ScreenFn>(0xA0FED0);
      const auto select_start = reinterpret_cast<ScreenFn>(0xA10090);
      auto set_stages = [&](uint32_t b) { mem::write<uint32_t>(prof + 0x970, b); };
      auto set_chars = [&](uint64_t b) {
        mem::write<uint32_t>(prof + 0x978, static_cast<uint32_t>(b));
        mem::write<uint32_t>(prof + 0x97C, static_cast<uint32_t>(b >> 32));
      };
      auto chars_of = [&] {
        return mem::read<uint32_t>(prof + 0x978) | static_cast<uint64_t>(mem::read<uint32_t>(prof + 0x97C)) << 32;
      };
      // The getters, as the stage select (each frame) and the character select (as it is made) call them.
      const uint32_t stage_cases[] = {0, 0xFFFFFFFFu, 0x1FFFF, 0xA5A5A5A5u, 0x5A5A5A5Au, 0x80000008u};
      const uint64_t char_cases[] = {0, 0x33333333ull, ~0ull, 0xA5A5A5A55A5A5A5Aull, 0x1000000ull, 0x3C0000000ull};
      bool getters = true;
      for (uint32_t b : stage_cases) {
        set_stages(b);
        getters = getters && records::stages_open(prof) == game_stages(iv);
      }
      for (uint32_t m : {game::kModeMercenaries, game::kModeNoMercy, 0u})
        for (uint32_t flag : {1u, 0u})
          for (uint64_t b : char_cases) {
            gm.field<uint32_t>(gmode) = m;
            gm.field<uint32_t>(ml.extras_flag) = flag;
            set_chars(b);
            getters = getters && records::chars_available(prof, m == game::kModeNoMercy) == game_chars(iv);
          }
      gm.field<uint32_t>(gmode) = game::kModeMercenaries;
      gm.field<uint32_t>(ml.extras_flag) = 1;
      check(getters, "the stages open and the characters available as the game's own getters work them out, for every "
                     "profile tried (the Mercenaries, No Mercy with and without its flag, the campaigns)");

      // The stage select, started (its side panel filled by the game's own fill, as its start fills it).
      const mt::Class* ssc = ml.stage_screen;
      const mt::Class* csc = ml.char_screen;
      Obj sel(ssc->size, ssc->vtable), sel_fresh(ssc->size, ssc->vtable);
      Obj chr(csc->size, csc->vtable), chr_fresh(csc->size, csc->vtable);
      set_stages(0x3F);                                     // Rail Yard ... The Catacombs open; Requiem for War on not
      set_chars(0x11110101ull | (1ull << 9) | 0xFull);  // Leon's four; Chris and his Costume 1, Jake, Sherry, Ada, Carla
      std::memset(reinterpret_cast<void*>(prof + rl.files), 0, rl.nfiles);
      panel_fill(reinterpret_cast<void*>(sel.at()));
      check(start_screen(sel), "the stage select starts (through the mod's hook)");
      menus.mode_known = true;
      menus.mode = game::kModeMercenaries;
      sel.field<uint8_t>(rl.step) = 0;
      check(screen() == records::Screen::kNone, "the stage select opening (step 0): no window");
      sel.field<uint8_t>(rl.step) = 10;
      check(screen() == records::Screen::kMercenaries, "the Mercenaries' stage select: the Mercenaries window");
      records::View mv = records::view();
      const uint16_t want_stage[10] = {1202, 1206, 1203, 1200, 1204, 1205, 1201, 1209, 1207, 1208};
      bool stages_ok = !mv.merc_no_mercy && !mv.merc_char_screen && mv.merc_nstages == 10;
      for (int i = 0; i < mv.merc_nstages && stages_ok; ++i)
        stages_ok = mv.merc_stage[i].bit == i && mv.merc_stage[i].number == want_stage[i] &&
                    mv.merc_stage[i].always == (i < 3) && mv.merc_stage[i].open == ((game_stages(iv) >> i) & 1);
      check(stages_ok && mv.merc_stage[5].open && !mv.merc_stage[6].open &&
                !std::strcmp(records_text::kMercStage[(mv.merc_stage[0].number - 1200) % 10], "Urban Chaos") &&
                !std::strcmp(records_text::kMercStage[(mv.merc_stage[1].number - 1200) % 10], "Steel Beast"),
            "its ten stages in the stage select's order (Urban Chaos s1202, Steel Beast s1206 ... Creature Workshop "
            "s1208), the first three always open, each open as the game's getter says");
      bool fams = mv.merc_nfamilies == 9;
      for (int f = 0; f < mv.merc_nfamilies && fams; ++f) {
        const records::MercFamily& fm = mv.merc_family[f];
        fams = fm.family == f && fm.n == (f < 7 ? 4 : 1);
        for (int k = 0; k < fm.n && fams; ++k)
          fams = fm.costume[k].bit == (f < 7 ? 4 * f + k : 21 + f) && fm.costume[k].costume == (f < 7 ? k : 0) &&
                 fm.costume[k].open == ((game_chars(iv) >> fm.costume[k].bit) & 1) &&
                 fm.costume[k].always == (fm.costume[k].bit == 0 || fm.costume[k].bit == 8 ||
                                          fm.costume[k].bit == 16 || fm.costume[k].bit == 29);
      }
      check(fams && !std::strcmp(records_text::kMercCostumeName[mv.merc_family[0].costume[2].bit], "Leon - Costume 2") &&
                !std::strcmp(records_text::kMercCharacter[mv.merc_family[7].family], "Carla Radames"),
            "the profile's nine characters in the character select's order (Leon ... Ada: Default and Costumes 1-3; "
            "Carla, the Agent), each costume open as the game's getter says; No Mercy's own four not listed");

      // Changes: Requiem for War opened, Rail Yard closed again, No Mercy's Rail Yard opened, Steel Beast (always)
      // asked too; Leon's and Helena's Costume 2 unlocked (their files with them), Chris' Costume 1 locked again,
      // Leon's Default (always) and Coach (No Mercy's own) asked too.
      const records::MercSet asks[] = {{true, 6, true},   {true, 3, false},  {true, 1, false}, {true, 13, true},
                                       {false, 2, true},  {false, 6, true},  {false, 9, false},
                                       {false, 0, false}, {false, 30, true}};
      std::memcpy(reinterpret_cast<void*>(prof + 0x7EF0), reinterpret_cast<void*>(prof + 8), 0x7EE8);
      records::set_merc(asks, static_cast<int>(std::size(asks)));
      records::tick(menus);
      const uint32_t st_now = mem::read<uint32_t>(prof + 0x970);
      const uint64_t ch_now = chars_of();
      check(st_now == ((0x3Fu | 1u << 6 | 1u << 13) & ~(1u << 3)) &&
                ch_now == ((0x11110101ull | 1ull << 9 | 0xFull | 1ull << 6) & ~(1ull << 9)),
            "in the profile: Requiem for War (bit 6) and No Mercy's Rail Yard (13) opened, Rail Yard (3) closed; "
            "Leon's Costume 2 (had already) and Helena's (6) unlocked, Chris' Costume 1 (9) locked; Steel Beast, Leon's "
            "Default and Coach left as they were");
      check(mem::read<uint8_t>(prof + rl.files + 63) == 1 && mem::read<uint8_t>(prof + rl.files + 64) == 1 &&
                mem::read<uint8_t>(prof + rl.files + 0x3A) == 0,
            "the Collection files of the costumes unlocked (63 Leon's Costume 2, 64 Helena's), as the game gives "
            "them; none for one locked");
      mv = records::view();
      check(mv.merc_stage[6].open && !mv.merc_stage[3].open && mv.merc_family[1].costume[2].open &&
                !mv.merc_family[2].costume[1].open && ((game_stages(iv) >> 6) & 1) && !((game_stages(iv) >> 3) & 1) &&
                std::strstr(mv.note, "2 stage(s) opened, 1 stage(s) closed again, 1 costume(s) unlocked, 1 costume(s) "
                                     "locked again"),
            "the view and the game's getter agree (the stage select shows them at once: it reads the getter each "
            "frame); the note");
      panel_fill(reinterpret_cast<void*>(sel_fresh.at()));
      check(!std::memcmp(reinterpret_cast<void*>(sel.at() + ml.p_counts), reinterpret_cast<void*>(sel_fresh.at() + ml.p_counts),
                         ml.p_ids + 3 * ml.p_ids_page - ml.p_counts) &&
                !(sel.field<uint32_t>(ml.p_masks + 4) & 4),
            "the stage select's side panel as the game's own fill makes it now (Chris' costumes no longer listed)");
      check(records::g_save != records::Save::kIdle && mem::read<uint32_t>(prof + 0x7EF0 + 0x970 - 8) == st_now &&
                mem::read<uint32_t>(prof + 0x7EF0 + 0x978 - 8) == static_cast<uint32_t>(ch_now) && finish_save() &&
                std::strstr(records::view().note, "Saved."),
            "the profile's save asked at once: the change committed into the profile's saved copy (the game's own "
            "commit: what a mode's start takes the profile back from), then written - Saved.");

      // The campaigns' stage select (the other modes use it too): no window, a change dropped.
      menus.mode = 0;
      const records::MercSet late = {true, 7, true};
      records::set_merc(&late, 1);
      check(screen() == records::Screen::kNone && !(mem::read<uint32_t>(prof + 0x970) & (1u << 7)),
            "the stage select in another mode: no window, a change asked then dropped");

      // No Mercy's character select: made as its constructor makes it (its copy of the characters available the
      // game's getter's), its lists filled by the game's own start (its last step, the per-frame update, cut off).
      sel.field<uint8_t>(rl.step) = records::kStepClosing;
      check(end_screen(sel), "the stage select destroyed (through the mod's hook)");
      menus.mode = game::kModeNoMercy;
      gm.field<uint32_t>(gmode) = game::kModeNoMercy;
      set_chars(1ull << 0 | 1ull << 8 | 1ull << 9 | 1ull << 16);  // Leon, Chris and his Costume 1, Jake
      uint8_t update_was = 0;
      std::memcpy(&update_was, reinterpret_cast<void*>(0xA0DA10), 1);
      const uint8_t ret = 0xC3;
      mem::write_bytes(0xA0DA10, &ret, 1);
      gm.field<int32_t>(0x3E7C) = -1;  // no character chosen yet
      auto made = [&](Obj& o) {
        const uint64_t a = game_chars(iv);
        o.field<uint32_t>(ml.cs_mask) = static_cast<uint32_t>(a);
        o.field<uint32_t>(ml.cs_mask + 4) = static_cast<uint32_t>(a >> 32);
        select_start(reinterpret_cast<void*>(o.at()));
      };
      made(chr);
      check(start_screen(chr) && chr.field<int32_t>(ml.cs_nrows) == 8 && chr.field<int32_t>(ml.cs_rows + 4) == 2 &&
                chr.field<int32_t>(ml.cs_counts + 8) == 2,
            "No Mercy's character select made: the game's own start lists Leon, Chris (Default, Costume 1), Jake, the "
            "Agent and No Mercy's own four (8 rows)");
      chr.field<uint8_t>(rl.step) = 10;
      check(screen() == records::Screen::kMercenaries && records::view().merc_no_mercy &&
                records::view().merc_char_screen && records::view().merc_stage[0].bit == 10 &&
                records::view().merc_stage[0].number == 1212 && records::view().merc_nfamilies == 9,
            "the character select up: the No Mercy window (its stages from bit 10: s1212 ...), the profile's nine "
            "characters (not No Mercy's own four)");
      chr.field<int32_t>(ml.cs_cursor) = 1;          // on Chris' row
      chr.field<int32_t>(ml.cs_ccursor + 4 * 2) = 1;  // on his Costume 1
      const records::MercSet more[] = {{false, 4, true}, {false, 10, true}, {false, 15, true}};
      records::set_merc(more, 3);
      records::tick(menus);
      made(chr_fresh);
      bool lists = chr.field<int32_t>(ml.cs_nrows) == chr_fresh.field<int32_t>(ml.cs_nrows) &&
                   chr.field<uint64_t>(ml.cs_mask) == chr_fresh.field<uint64_t>(ml.cs_mask) &&
                   !std::memcmp(reinterpret_cast<void*>(chr.at() + ml.cs_rows), reinterpret_cast<void*>(chr_fresh.at() + ml.cs_rows), 4 * 13) &&
                   !std::memcmp(reinterpret_cast<void*>(chr.at() + ml.cs_counts), reinterpret_cast<void*>(chr_fresh.at() + ml.cs_counts), 4 * 13) &&
                   !std::memcmp(reinterpret_cast<void*>(chr.at() + ml.cs_lists), reinterpret_cast<void*>(chr_fresh.at() + ml.cs_lists), 16 * 13);
      check(lists && chr.field<int32_t>(ml.cs_nrows) == 10,
            "Helena, Chris' Costume 2 and Piers' Costume 3 unlocked there: the character select's rows and lists as "
            "the game's own start makes them from the profile now (Helena's and Piers' rows in: 10), its copy of the "
            "characters available the game's getter's (No Mercy's four kept)");
      check(chr.field<int32_t>(ml.cs_cursor) == 1 && chr.field<int32_t>(ml.cs_ccursor + 4 * 2) == 1 &&
                chr.field<int32_t>(ml.cs_counts + 4 * 2) == 3,
            "... the cursor kept on its row, Chris' costume cursor on his Costume 1 (now one of three)");
      check(mem::read<uint8_t>(prof + rl.files + 65) == 1 && mem::read<uint8_t>(prof + rl.files + 73) == 1 &&
                finish_save(),
            "their Collection files (65, 73) with them; saved");
      const records::MercSet lock_one = {false, 15, false};
      chr.field<int32_t>(ml.cs_cursor) = 9;  // the last row
      records::set_merc(&lock_one, 1);
      records::tick(menus);
      check(chr.field<int32_t>(ml.cs_nrows) == 9 && chr.field<int32_t>(ml.cs_cursor) == 8 && finish_save(),
            "Piers' only costume locked again: his row gone, the cursor held to the rows");
      chr.field<uint8_t>(rl.step) = records::kStepClosing;
      check(screen() == records::Screen::kNone, "the character select closing: no window");
      chr.field<uint8_t>(rl.step) = 10;
      records::tick(playing);
      check(records::view().screen == records::Screen::kNone, "in a game: never");
      check(screen() == records::Screen::kMercenaries && end_screen(chr) && screen() == records::Screen::kNone,
            "the character select destroyed (through the mod's hook): no window, at once");

      mem::write_bytes(0xA0DA10, &update_was, 1);
      mem::write<uint32_t>(kGameManG, gm_was);
      mem::write<uint32_t>(kIdG, id_was);
      game::g_sgameman = sgameman_was;
      menus.mode_known = false;
      menus.mode = 0;
    }

    records::tick(playing);
    game::g_systems.erase("sUnit");
    game::g_systems.erase("sBioUnit");
    game::g_systems["sInfoMan"] = info.at();
    mem::write<uint32_t>(kInfo, info_was);
    mem::write<uint32_t>(kMain, main_was);
    mem::write<uint32_t>(kSteam, steam_was);
    mem::write<uint32_t>(kStorage, storage_was);
  }

  cheats::remove_hooks();
  check(mem::read<uintptr_t>(leon_vt + 4 * game::kSubLifeSlot) == game::sig_one("kSigSubLife"),
        "unload: the game's subtract-health back in the vtables");

  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
