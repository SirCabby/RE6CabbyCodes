#pragma once

// The mod's two moments with the game.
//
// DllMain: BH6.exe imports steam_api.dll, so the mod's DllMain runs as the
// program loads - its imports are bound by then, but none of its code has run:
// its C runtime and its static initializers (which build MT Framework's class
// records, mt.h) come after. imports() re-points the few import slots the mod
// watches (the crash filter, ExitProcess, the DirectInput guard, Direct3DCreate9)
// there, so the game's first calls of them already come to the mod.
//
// The game's first call of Direct3DCreate9 comes from its renderer's start-up in
// WinMain, with every class record built: started() runs there, once, before the
// real call - the class records, the game's discovery, the cheats' hooks and the
// tick's.
namespace re6cc::boot {

void imports();         // from DllMain
void remove_imports();  // unload

void started();
bool done();           // started() has run
const char* status();  // what the panel says while the mod cannot see the game

}  // namespace re6cc::boot
