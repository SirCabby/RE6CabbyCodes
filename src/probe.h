#pragma once

#include "game.h"

// A diagnostic (Trace = 1 in the ini; reverse engineering aid only - nothing the mod
// does depends on it): the words of the game's main systems that change rarely, logged
// the moment they change - "probe: sGameMan+0x491D 00000000 -> 00000001". A word that
// changed within the last two seconds is left out (timers, counters), so what is logged
// is what a screen, a menu, a cutscene or a key switched. Watched: sBioMain, sGameMan,
// sId, sQlocInput (the keys pressed), sPlayer, sCutscene, sBioScene, sBioArea and its
// areas (aGame among them), sBioUnit's line switches and its last fields.
namespace re6cc::probe {

void tick(const game::Run& run);  // main thread, from the tick

}  // namespace re6cc::probe
