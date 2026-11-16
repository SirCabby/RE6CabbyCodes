#pragma once

#include "game.h"

// The Mercenaries (The Mercenaries and No Mercy: sGameMan's modes): the match's numbers
// the panel shows, and what Endless combo, Combo Time always on and Kill enemies as they
// appear do to it. Everything the game keeps for a match is sGameMan's: its step (1 while
// it plays), the clock, the stage's kills and their target, and a score block for each
// player - the combo running, its count and clock, the best combo, Combo Time (a kill's
// combo bonus at least 1000 while it runs) and its clock, what the combo has earned (put on
// the score only as the combo ends) and the score. Both modes keep one block for the whole
// team (block 0). Found by the code that runs them (sigs.h); never by address.
namespace re6cc::merc {

// From cheats::install(), after game::discover(): the match's fields, and the hit record a
// hit leaves on an enemy, read out of the game's code and checked against each other. Each
// says in the log whether it was understood.
bool discover_match();  // Endless combo, Combo Time always on, the panel's numbers
// Kill enemies as they appear; told whether the enemies' take-damage is hooked (cheats.cpp: it asks lethal()).
bool discover_kills(bool take_damage_hooked);

struct Wanted {
  bool endless_combo = false;
  bool combo_time = false;
  bool kill = false;
};

// The tick (main thread): the switches applied (the patches go in and out with them, and
// act only while the match plays offline on this machine); true when Kill enemies as they
// appear is to be switched off (a match has ended or begun: it lasts one match).
bool tick(const game::Run& run, const Wanted& want);

// The match's numbers (for the panel), published by the tick.
struct Status {
  bool match = false;     // a match is on (mode the Mercenaries or No Mercy, its step 1 to 3)
  bool no_mercy = false;
  bool offline = false;   // not online, this machine running the match: the switches act
  int step = 0;           // 1 playing, 2 time up or cleared, 3 the end
  float time_left = 0;    // the clock, seconds
  int score = 0;
  int waiting = 0;        // what the running combo has earned, not on the score yet
  int combo = 0, best_combo = 0;
  float combo_left = 0;   // the combo's window
  int kills = 0, target = 0;
  bool combo_time = false;
  float combo_time_left = 0;
  int combo_time_kills = 0;
  int killing = 0;        // Kill enemies as they appear: the enemies in play not dead yet (while it acts)
  int killed = 0;         // ... the enemies its hits have killed this match
};
Status status();

bool match_found();
bool kills_found();

// Kill enemies as they appear's hits are lethal: an enemy's take-damage (cheats.cpp's hook of slot 128, on the main
// thread in the game's frame) hands its hit and the amount the enemy's own damage code worked out here; for a hit of
// the mod's (in the record of an enemy it was written into) the amount becomes all the enemy's health - once the enemy
// has lived through a few, even from nothing. Any other hit's amount comes back as it is.
int lethal(void* self, void* hit, int amount);
bool endless_found();     // Endless combo's window and its earnings' bank found
bool combo_time_found();  // Combo Time's clock found

void remove_patches();  // unload

}  // namespace re6cc::merc
