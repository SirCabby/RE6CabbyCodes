#pragma once

#include <cstdint>

#include "game.h"

// The records windows: beside the game's Records screen (uIdRecord00) - its chapter grid (PROFILES, "Completed")
// and its MEDALS page - and its Dog Tag screen (uIdCustomize10) - the EMBLEM and TITLE lists; and the skills window
// beside its skill screen (uIdSkillSet10). Each window opens by itself while its view is up, and changes what the
// profile (sInfoMan's cSystemInfo) holds:
//
// - a chapter cleared, by character (the Agent shares Ada's records), difficulty and chapter, with its rank (the
//   look of its mark on the grid: S to E) and its best time (Chapter Select's; a clear made here without one gets
//   the game's own "no time", its clock's most) - and what the game gives for that clear: the chapter's dog tag
//   template, the Collection's files two chapters give, the unlocks a campaign's completion gives (the skills'
//   eight, two Mercenaries stages, Ada's campaign, Ada for the extra content), and the achievements (the chapter's,
//   a difficulty's whole game) through the game's own achievement call. A clear taken back leaves those as they are;
// - a medal's count (0-99), in each category the Records screen shows;
// - an emblem awarded: its achievement through the game's own call (the emblem with it, and Steam's);
// - a title awarded, up to a level of its family: its state, its counter brought up to the title's goal (as earning
//   it leaves it: the game shows what earns a title, and its page in the list, once its counter has started), and
//   what the game's own title check does with a new title - "Rising Up" at a level-4 title, the count of titles
//   held, "Titular Achievement" at ten. A level held already, asked for again, only brings its counter up;
// - a skill the game sells only once a requirement is met (kills of J'avo or zombies, kills with a kind of weapon,
//   every campaign completed) unlocked - the skill screen then sells it for skill points, as it would once the
//   requirement was met - or locked again (not one bought, nor one whose requirement the game's own counts meet).
//   Nothing is bought here and the skill points stay as they are; the screen's own save as it closes keeps the
//   change, as it keeps a purchase. The skill screen's list shows it at once (its rows, as the game fills them).
// - beside the Records screen's WEAPONS page: a row named instead of "? ? ?" - the page names a row once the game
//   has counted a use of it (or always: each character's first weapons, the tablets, the spray), so the mod gives
//   it its first use (Times Used 1, as one shot that missed leaves it), or takes that use back (never a count of
//   more than one use, nor one with a kill: play the mod did not make); and a weapon a character's campaign gives
//   on the way, put in that character's saved inventory (a full magazine, as a chapter's end gives a weapon walked
//   past) or taken out of it (its loaded ammo with it) - what Continue, New Game and Chapter Select start from.
// - beside the Records screen's ENEMIES page: a row named instead of "? ? ?" - the page names an enemy once the game
//   has counted one defeated (or one that defeated you; J'avo and Zombie always), so the mod gives it its first kill
//   (Enemies Defeated 1). A row the page names already is left as it is, and no count is ever lowered: a reveal is
//   never taken back (a boss's one real kill could not be told from the mod's).
// - beside the Mercenaries' (and No Mercy's) stage select and character select: a stage opened (or closed again)
//   - each mode's ten its own bits, the first three always open - and a character's costume unlocked (or locked
//   again) - one set of bits both modes share: each character's Default and Costumes 1-3, Carla, the Agent; Leon's,
//   Chris', Jake's and the Agent's Default always there - with the Collection file the game gives with a costume.
//   The stage select shows a stage at once (it reads the profile each frame); its side panel and the character
//   select's rows (filled at their start) are filled again as their start fills them.
//
// The game's achievement call (sInfoMan's) and its profile save (the steps the menus take as they close with the
// profile changed) are the two game calls here, both the user's choice (2026-11-06); the rest is field writes,
// under sInfoMan's own lock as the game takes it. The screens' own copies of what they show (the Records grid and
// its percent, the Dog Tag lists' rows, the skill list's rows) are written too, so a change shows at once.
// Everything happens in the tick, while the screen it belongs to is up; the profile is then saved through the
// game's own save (the skill screen's: its own, as it closes).
namespace re6cc::records {

enum class Screen : uint8_t { kNone, kChapters, kMedals, kEmblems, kTitles, kSkills, kWeapons, kEnemies, kMercenaries };

constexpr int kChars = 7;        // the records' characters: Leon, Helena, Chris, Piers, Jake, Sherry, Ada
constexpr int kDifficulties = 5; // Amateur, Normal, Veteran, Professional, No Hope
constexpr int kChapters = 5;
constexpr int kRanks = 6;        // S, A, B, C, D, E
constexpr int kMedalCats = 9;
constexpr int kMaxMedals = 200;  // a category's most (the campaigns')
constexpr int kMedalMost = 99;   // a medal's most, as the game counts it
constexpr int kEmblems = 50;
constexpr int kMaxTitleRows = 301;
constexpr int kMaxLevels = 4;
constexpr int kMaxSkills = 32;  // the skills a list sells only after a requirement (the campaigns' list has 19)
constexpr int kMaxWeaponRows = 64;  // the WEAPONS page's rows (the game's: 43)
constexpr int kCarriers = 8;        // the characters with a saved inventory: Leon, Helena, Chris, Piers, Jake,
                                    // Sherry, Ada, the Agent (records_text kCharacter's order)
constexpr int kMaxCarried = 8;      // a character's weapons found on the way (the game's: 3 or 4)
constexpr int kMaxEnemyRows = 96;   // the ENEMIES page's rows (the game's: 86; the profile keeps 90 rows' counts)
constexpr int kMercStages = 10;     // a Mercenaries mode's stages (the stage select's rows)
constexpr int kMercFamilies = 16;   // the character select's characters (the game's: 13 - nine the profile's)
constexpr int kMercCostumes = 4;    // a character's costumes at most (Default, Costume 1-3)

struct Cell {
  bool cleared = false;
  uint8_t rank = 0;  // 0-5: S to E (the best this record holds)
  float time = 0;    // its best time in seconds, as the record holds it (0 in a record never cleared, and in one
                     // cleared with the clock held at 0: Chapter Select shows that "0:00:00")
};

struct MedalCat {
  bool shown = false;  // the Records screen lists it (its extra content is there)
  int base = 0;        // its medal 0's message (records_text.h kMedal: base - kMedalFirst)
  int n = 0;           // its medals, as the game lists them
  uint8_t id[kMaxMedals] = {};
  uint8_t count[kMaxMedals] = {};
};

struct Level {
  uint32_t goal = 0;  // what the counter must reach (the game's own number)
  uint16_t msg = 0;   // its name and need (records_text.h kTitleName / kTitleNeed)
  uint8_t level = 0;  // 1-4
};

struct TitleRow {
  uint16_t counter = 0;   // its family's counter (1-based)
  uint8_t earned = 0;     // the level held (0: none)
  bool fresh = false;     // the game's NEW mark
  bool stars = false;     // a family of levels (not a single title)
  bool behind = false;    // held, its counter short of the held level's goal: the game shows "? ? ?" for its need
  uint32_t progress = 0;  // the counter now
  uint8_t nlevels = 0;
  Level level[kMaxLevels];
};

// What the game asks before it sells a skill (its skill table's requirement: the screen shows "? ? ?" until then).
enum class Need : uint8_t { kKills, kWeaponKills, kCampaigns };

struct Skill {
  uint8_t id = 0;     // the skill: its state's place in the profile, its texts (records_text.h kSkillName ...)
  uint8_t state = 0;  // the profile's: 0 locked, 1 unlocked (new), 2 unlocked, 3 bought
  Need need = Need::kKills;
  bool met = false;   // the game's own count has reached the goal: it sells the skill whatever its state
  int count = 0;      // what the game counts toward it (kills, or campaigns completed)
  int goal = 0;
  uint32_t price = 0;  // in skill points
};

// A row of the Records WEAPONS page the game can draw "? ? ?" (not one it always names).
struct WeaponRow {
  uint8_t entry = 0;   // the page's row: its name records_text kWeaponName[entry]
  uint32_t uses = 0;   // Times Used: the page names the row once it is above 0
  uint32_t kills = 0;  // Enemies Defeated
  bool played = false;  // more than the one use the mod gives, or a kill: the game's own count, never taken back
};

// A weapon a character's campaign gives on the way (not one it starts with), in its saved inventory or not.
struct Carried {
  uint16_t id = 0;       // the item (items.h)
  int8_t entry = -1;     // the WEAPONS page row its uses count on (-1: none)
  bool had = false;      // in the character's saved inventory
  int16_t count = 0;     // its count there (-1: a full magazine; -2: not had)
};
struct Carrier {
  bool ready = false;  // its saved inventory holds a slot for each of these (the game sets one up at a game's start)
  int n = 0;
  Carried weapon[kMaxCarried];
};

// A row of the Records ENEMIES page the game can draw "? ? ?" (not one it always names): named while either count
// is above 0.
struct EnemyRow {
  uint8_t entry = 0;     // the page's row: its name records_text kEnemyName[entry]
  int32_t kills = 0;     // Enemies Defeated
  int32_t defeated = 0;  // Times Defeated (you, by it)
  bool shows_defeated = false;  // the page shows its Times Defeated (not the animals', the Chrysalids', Oko's)
};

// A Mercenaries stage, as the stage select lists it: its row's bit in the profile's stages (the mode's first + the
// row), the stage itself (1200 ...).
struct MercStage {
  uint8_t bit = 0;
  uint16_t number = 0;
  bool open = false;    // the stage select lets you pick it
  bool always = false;  // the game opens it whatever the profile holds
};
// A costume of a character of the character select: its bit in the profile's characters (64 bits), the game's own
// number for it (0 Default, 1-3 Costume n).
struct MercCostume {
  uint8_t bit = 0;
  uint8_t costume = 0;
  bool open = false;    // the character select offers it
  bool always = false;  // the game offers it whatever the profile holds
};
struct MercFamily {
  uint8_t family = 0;  // the character select's character (records_text kMercCharacter)
  int n = 0;
  MercCostume costume[kMercCostumes];
};

struct View {
  bool known = false;         // the records were understood in the game (the windows can be offered)
  Screen screen = Screen::kNone;
  bool achievements = false;  // the game's achievement call was found
  bool can_save = false;      // ... and its profile save
  bool saving = false;        // a save is under way
  char note[200] = "";        // what the last change did, or why it could not be made
  // kChapters
  Cell cell[kChars][kDifficulties][kChapters];
  int percent = 0;  // "Completed" as the game works it out
  float time_most = 0;  // a best time's most - the chapter clock's, the game's "no time" (0: not known, no times)
  // kMedals
  int medal_cat = 0;  // the category the screen shows
  MedalCat medal[kMedalCats];
  // kEmblems: 0 locked, else unlocked (1 still marked NEW)
  uint8_t emblem[kEmblems] = {};
  // kTitles
  int ntitles = 0;
  TitleRow title[kMaxTitleRows];
  // kSkills: the skills its list sells only after a requirement, in the list's order
  int nskills = 0;
  Skill skill[kMaxSkills];
  // kWeapons: the page's rows that can be "? ? ?", in its order; how many it always names; the characters' saved
  // inventories (known: understood, and the game's save found to keep a change in them)
  int nweapon_rows = 0, nweapon_always = 0;
  WeaponRow weapon_row[kMaxWeaponRows];
  bool carried_known = false;
  Carrier carrier[kCarriers];
  // kEnemies: the page's rows that can be "? ? ?", in its order; how many it always names
  int nenemy_rows = 0, nenemy_always = 0;
  EnemyRow enemy_row[kMaxEnemyRows];
  // kMercenaries: the mode (No Mercy, else the Mercenaries), the screen up (the character select, else the stage
  // select); the mode's stages in the stage select's order; the profile's characters in the character select's
  // order, with their costumes (both modes share them; No Mercy's own four are always there, not the profile's)
  bool merc_no_mercy = false, merc_char_screen = false;
  int merc_nstages = 0;
  MercStage merc_stage[kMercStages];
  int merc_nfamilies = 0;
  MercFamily merc_family[kMercFamilies];
};

// Any thread: what the tick last read (the view of the screen that is up), and just which screen.
View view();
Screen screen();

// Any thread: changes for the next tick, made only while their screen is up.
struct ChapterSet {
  uint8_t chara = 0, difficulty = 0, chapter = 0;
  bool cleared = false;
  uint8_t rank = 0;
  float time = -1;  // a cleared one's best time in seconds (held at the most); below 0 left as it is
};
void set_chapters(const ChapterSet* sets, int n);
struct MedalSet {
  uint8_t cat = 0, medal = 0, count = 0;
};
void set_medals(const MedalSet* sets, int n);
void award_emblems(const uint8_t* emblems, int n);
struct TitleSet {
  uint16_t counter = 0;
  uint8_t level = 0;
};
void award_titles(const TitleSet* sets, int n);
struct SkillSet {
  uint8_t id = 0;
  bool unlocked = false;  // unlock it, or lock it again
};
void set_skills(const SkillSet* sets, int n);
struct WeaponSet {
  uint8_t entry = 0;
  bool named = false;  // its first use given, or taken back ("? ? ?")
};
void set_weapon_rows(const WeaponSet* sets, int n);
struct CarrySet {
  uint8_t chara = 0;  // 0-7 (kCarriers)
  uint16_t id = 0;
  bool had = false;   // put in the character's saved inventory, or taken out
};
void set_carried(const CarrySet* sets, int n);
// The ENEMIES page's rows (by row) named by a first kill, where the page draws "? ? ?".
void reveal_enemies(const uint8_t* entries, int n);
// Beside the Mercenaries' stage select and character select: a stage opened or closed again (by its bit), a costume
// unlocked or locked again (by its bit) - never one the game always gives.
struct MercSet {
  bool stage = false;  // a stage's bit (else a costume's)
  uint8_t bit = 0;
  bool open = false;
};
void set_merc(const MercSet* sets, int n);

void init();                      // from DllMain: the lock
void install();                   // from boot::started(), after game::discover()
bool known();
void remove_hooks();              // the screens' hooks out (unloading)
void tick(const game::Run& run);  // main thread: the menus' screens out of a game, the skill screen anywhere

// The names the windows use (records_text.h, by what the game reads).
const char* medal_name(const View& v, int cat, int medal);

}  // namespace re6cc::records
