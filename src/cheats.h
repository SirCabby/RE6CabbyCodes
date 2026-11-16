#pragma once

#include "game.h"
#include "switches.h"

// The cheat switches (switches.h), what each does to the game, and the editor
// beside them. The panel (render thread) only flips a switch or posts an edit;
// the tick (main thread) applies it - an edit only while the game is paused.
namespace re6cc::cheats {

void init();     // from DllMain: the switches as the ini has them
bool install();  // from boot::started(), after discovery: the hooks and patches prepared
void tick(const game::Run& run);

bool enabled(sw::Kind k);
void set(sw::Kind k, bool on);  // any thread: takes effect at the next tick, and is written to the ini

// Whether a switch can work in this game (its hooks were found), and why not.
bool available(sw::Kind k);
const char* unavailable_reason(sw::Kind k);

// A character's numbers in the chapter's results.
struct Counters {
  int index = -1;  // in sPlayer's array (the edits' `chara`)
  const char* name = "";
  int shots = 0, hits = 0, kills = 0, deaths = 0;
};

// What the panel shows, published by the tick.
struct Status {
  bool points = false;  // the profile's skill points are known
  int skill_points = 0;
  // The chapter's numbers (the profile in use's): one clock, which the game adds to every
  // character's alike, and the counters of each character this machine plays.
  bool results = false;
  float clock = 0;   // seconds
  int counter_most = 0;
  float clock_most = 0;
  int ncounters = 0;
  Counters chars[2];
  // A countdown on screen: a stage's, or the modes' match clock.
  bool countdown = false;
  bool match_clock = false;  // ... it is the match clock
  float countdown_left = 0;  // its seconds left
  float countdown_most = 0;  // the most it can be set to: what the HUD can show
};
Status status();

// Edits (any thread); the tick applies them while the game is paused. The counters'
// `chara` is the character's index in sPlayer's array; kTime sets the chapter's clock
// (every character's copy) and kCountdown the seconds left of the countdown on screen
// (neither uses `chara`).
enum class Edit { kSkillPoints, kDeaths, kKills, kShots, kHits, kTime, kCountdown, kCount };
void edit(Edit what, int chara, double value);

constexpr int kMostSkillPoints = 99999999;  // what the editor takes (the game keeps a 32-bit count)

// Why an enemy keeps its life through a hit by the game's own rules (mIsNoDead, its keep-alive flag, its own damage
// code or subtract-health, its end the stage's script) - One hit kills' - or null when its health can end it.
const char* keeps_life(const void* enemy);

void remove_hooks();  // unload

}  // namespace re6cc::cheats
