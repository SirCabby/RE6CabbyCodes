#pragma once

#include <cstdint>
#include <vector>

// What the mod knows about the game: its systems (MT Framework's singletons, the
// s* classes, found by their class names in the main system's own list of them),
// whether the game is paused, the characters and the profile's skill points.
// Everything is found by name (mt.h) or by the code patterns in sigs.h, read out
// of the matched instructions and checked against each other - never by address.
namespace re6cc::game {

enum class Screen {
  kNone,   // no game in play (start-up, loading, the title and the menus)
  kPlay,   // a campaign chapter or a mode's match, playing
  kPause,  // the same, paused (the game stands still behind its pause screen)
};

// Who drives a character (uPlayer's own word for it, as sPlayer hands it over):
// this machine's pad or keyboard, the AI, or the other player over the network.
enum class Control { kLocal = 0, kAi = 1, kNetwork = 2 };

constexpr int kMaxChars = 8;  // sPlayer's characters
struct Character {
  uintptr_t obj = 0;
  int index = 0;  // in sPlayer's array
  Control control = Control::kLocal;
  const char* name = "";  // "Leon", "Helena", ...
  int hp = 0, hp_max = 0;
};

// The game's modes, as sGameMan keeps them (its mode word): the campaigns (and their chapters' menus), the
// Mercenaries, No Mercy; the online-only modes (Agent Hunt, Versus ...) have others.
constexpr uint32_t kModeCampaign = 0;
constexpr uint32_t kModeMercenaries = 0x1E;
constexpr uint32_t kModeNoMercy = 0x50;

struct Run {
  uintptr_t main = 0;    // the main system (sBioMain) whose frame is running
  bool in_game = false;  // characters are in play
  bool paused = false;   // the game is paused (sBioMain's own state)
  Screen screen = Screen::kNone;
  bool mode_known = false;  // sGameMan's mode was read (else mode is 0)
  uint32_t mode = 0;        // kModeCampaign ...
  bool online = false;      // a session over the network
  bool panel = false;       // the panel (or a records window) can show: the numbers it shows are read
  int nchars = 0;
  Character chars[kMaxChars];
};

// From boot::started(): the class records and what the mod needs of them.
bool discover();
bool ready();
const char* status_text();  // why not ready, for the panel

// The tick: `main` is the sBioMain whose frame is running.
void tick(void* main, Run* out);
const char* screen_name(Screen s);

// The matches of a signature (sigs.h) in the game's code - logged when their
// number is not the one sigs.h expects - and the one match of a unique one (0
// otherwise). call_target: where an `E8 rel32` at `at` goes (0 for no call).
std::vector<uintptr_t> sig_all(const char* name);
uintptr_t sig_one(const char* name);
uintptr_t call_target(uintptr_t at);

// A system by its class name ("sPlayer", "sInfoMan", ...), once the first tick has seen the main system's list: the
// instance, or 0. A base's name finds the game's own subclass's ("sUnit": the sBioUnit the main system holds).
uintptr_t system(const char* name);

// --- what was read out of the game's code ------------------------------------------------
struct Layout {
  uint32_t paused = 0;             // sBioMain: the game is paused (a byte)
  uint32_t hp = 0, hp_max = 0;     // uCharacter: health and its most (16-bit)
  uint32_t control = 0;            // uPlayer: who drives it (Control, 32-bit)
  uint32_t players = 0;            // sPlayer: the characters' array ...
  int nplayers = 0;                // ... of this many
  uint32_t hit_damage = 0;         // a hit (HitInfo): its damage (32-bit; the game reads its low word)
  uint32_t hit_attacker = 0;       // ... and who dealt it
  uint32_t inventory = 0;          // uPlayer: the items (cItem) ...
  uint32_t item_size = 0;          // ... of this size ...
  int nitems = 0;                  // ... this many
  // An item (cItem; 0 when not found): in use (a byte), its slot number (a byte), id (a
  // word), the mode in use (a byte), the counts and the mosts (three words each, one a mode).
  uint32_t it_used = 0, it_slot = 0, it_id = 0, it_mode = 0, it_num = 0, it_max = 0;
  uint32_t in_hand = 0;            // uPlayer: the slot number of the item in hand (a dword)
  uint32_t ammo_flags = 0;         // uPlayer: the word with the game's own "infinite ammo" bit ...
  uint32_t ammo_bit = 0;           // ... this one
  uint32_t profile = 0;            // sInfoMan: the profile in use (its index) ...
  uint32_t profile_size = 0;       // ... a profile's size (cSystemInfo's) ...
  uint32_t skill_points = 0;       // ... and the skill points, counted from sInfoMan's start
  // sGameMan: the mode (a dword), a session online (a byte), and the modes' match: the clock (seconds left, a
  // float), the HUD's copy of it, the match's step (1 while it plays) and end state (0 while it runs), the
  // host's number and this machine's (equal when this machine runs the match).
  uint32_t mode = 0, online = 0;
  uint32_t match_clock = 0, match_hud = 0, match_step = 0, match_end = 0, match_host = 0, match_me = 0;
  float match_most = 0;            // the most the match clock holds (the game's own cap, seconds)
  // The chapter's numbers - the results' - in the profile's cGameInfo (sInfoMan's, one a profile): a block for
  // each character (sPlayer's index), its clock (seconds, a float) and its counters (16-bit); the live ones,
  // and a copy a save takes (and a quit to the title brings back) that lies saved_back before them.
  uint32_t game_info = 0, game_info_size = 0;  // sInfoMan: the profiles' cGameInfo, each this size
  uint32_t saved_back = 0;
  uint32_t clock = 0, counters = 0, block = 0;  // cGameInfo: character 0's clock and counters; a block's size
  int nblocks = 0, ncounters = 0;
  int counter_most = 0;            // the most a counter holds
  float clock_most = 0;            // ... and the clock
  int c_shots = -1, c_hits = -1, c_kills = -1, c_deaths = -1;  // the counters the results grade
  // A stage's scenario timers (aStageBase's): floats, each running (a bit of a word) counting down (a bit of
  // another) or up. The countdowns on screen show one of them.
  uint32_t timers = 0, timers_running = 0, timers_down = 0;
  int ntimers = 0;
  // An FSM's own timers (cFSMOrder's - a scenario's, a character's script): laid out as the stage's. The CountDown
  // order shows one of them when the order's timer kind is 0.
  uint32_t own_timers = 0, own_running = 0, own_down = 0;
  int nown = 0;
};
const Layout& layout();

// The vtable slots the cheats hook, each checked against its signature.
constexpr int kSubLifeSlot = 44;      // uCharacter: subtract-health (amount)
constexpr int kPlayerHitSlot = 117;   // uPlPlayer: a hit (hit)
constexpr int kPlayerApplySlot = 121; // uPlayer: take damage (6 arguments)
constexpr int kEnemyWorkSlot = 126;   // uEnemy: the damage worked out (the handler's copy of the hit)
constexpr int kEnemyApplySlot = 128;  // uEnemy: take damage (hit, amount)
constexpr int kEnemyMayDieSlot = 58;  // uEnemy: whether it may die (the take-damage asks)

// The characters as sPlayer has them (main thread).
int characters(Character out[kMaxChars]);
const char* character_name(uintptr_t obj);
void set_health(uintptr_t obj, int hp);

// Where the profile in use keeps its skill points (sInfoMan's), or 0.
uintptr_t skill_points_at();
// The profile in use's cGameInfo (the chapter's numbers), or 0.
uintptr_t game_info_at();

// sPlayer's character at `index` (main thread; a hook's player number), or 0; whether the game holds that
// player's input back (sPlayer's own test, read).
uintptr_t character_at(int index);
bool input_blocked(int index, uint32_t blocked_field);

uintptr_t sgameman();  // sGameMan (main thread), or 0

}  // namespace re6cc::game
