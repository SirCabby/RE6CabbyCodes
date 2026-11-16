# RE6CabbyCodes — project guide

A client-side mod for **Resident Evil 6** — Steam app 221040, `BH6.exe` build **10112070** (PE32, image base
0x400000, relocations stripped, **not packed**: the file on disk is the code that runs; MT Framework 2; Direct3D 9;
the PC port by QLOC) — cross-built on Linux with mingw-w64. It draws a Dear ImGui panel while the game is **paused**:
- **The cheats**: God mode (the vehicle you ride too), Partner takes no damage, Infinite stamina, Infinite air (under
  water), One hit kills, Automatic QTEs, Infinite ammo (the game's own, with the health tablets kept out of it; Ada's
  helicopter's missiles), Steady aim, Infinite skill points with a skill points editor, Freeze countdown timer with
  a time-left editor.
- **The Mercenaries** (in a match only): Endless combo, Combo Time always on, Kill enemies as they appear, the match.
- **The results** (in a campaign): Freeze play time, Deaths/Misses don't count, the clock's and counters' editors.
- **Each character's inventory**: counts, Throw away, Add (ammo its own weapons load, grenades, the spray, herbs - no
  weapon since 2026-11-14: the WEAPONS window gives them; the game's loader loads a grenade's archive first).
- **The records windows** (the menus): beside the Records screen's chapter grid (chapters cleared, with their rank,
  best time and what a clear gives), its medals (counts) and its WEAPONS page (a row's "? ? ?" named by its first use; each
  character's campaign weapons put in or taken out of its saved inventory - a grid, the 9 weapons shared by 2 or 4)
  and its ENEMIES page (a row's "? ? ?" named by a first kill; Reveal all), and the Dog Tag screen's emblem list (each emblem's achievement, awarded through the game's own call) and title list
  (titles awarded), the profile saved by the game's own save.
- **The skills window** (the skill screen, menus and between chapters): the 19 skills it sells only after a
  requirement, unlocked (or locked again) for purchase with skill points; the screen's own close saves it.
- **The Mercenaries window** (the Mercenaries' and No Mercy's stage select and character select): the mode's ten
  stages opened (or closed again) and each character's costumes - Default, Costumes 1-3, Carla, the Agent; one set for
  both modes - unlocked (or locked again) with their Collection files; the profile saved by the game's own save.

Every switch is written back to the ini. `README.md` is the user doc (`README_BBCode.txt` the same for forums).

The engine-independent parts come from `../RE5CabbyCodes` (also MT Framework, 32-bit, D3D9): the overlay, the D3D9
hooks and the Windows "adopt" path, the input guard, config, the log, the crash logger, `mem.h`, the class walk
(`mt.cpp`), the units (`units.cpp`) and the tick (`dispatch.cpp`). What differs: no Enigma (the imports are hooked
straight from the PE's named import table at DllMain), MT Framework 2's class records (built at start-up by static
initializers, a different layout), sUnit's line layout, and the game window known by its class's window procedure
(RE5's `adopt.cpp` compared the class name to RE4's "RE4HDWnd" until 2026-11-05: fixed there, at the user's word).
The proxy is `../RE4OGCabbyCodes`' steam_api.dll (its `proxy.cpp`, `tools/gen_proxy.py`, the Makefile's install):
the first build was a d3d9.dll as RE5's, and the user asked for a vector free of other mods (2026-11-05: "isn't d3d9
used by ReShade? We should find a mod vector that doesn't have compatibility issues with other common mods").
The conventions (windows per screen, folding, F7) follow `../RE4CabbyCodes`.

**This file is the guide; what was reverse-engineered is in `docs/`** (see "Notes" below; read the section a task
needs). Keep this file short. A finding goes into its `docs/` section. A new feature gets a section there, one line
under "Status", and its report in `docs/history.md`.

## Build & deploy

```sh
make            # -> build/steam_api.dll (config.mk sets GAME_DIR; gitignored)
make install    # installs it; the game's own steam_api.dll is kept as steam_api_orig.dll (once)
make uninstall  # removes the mod (and puts the game's steam_api.dll back)
make package    # dist/RE6CabbyCodes_v<version>.zip (no VERSION file: 0.0.0)
python3 -I tools/gen_proxy.py --exe "$GAME_DIR/BH6.exe" --dll steam_api.dll --from-dll "$GAME_DIR/steam_api_orig.dll" \
    --def steam_api.def --inc src/proxy_exports.inc --prefix steam   # the proxy's export list (55 + 1 data)
# reverse engineering (BH6.exe is not packed: everything works on the file, laid out as in memory):
python3 -I tools/map_image.py --exe "$GAME_DIR/BH6.exe" --out ghidra_proj/BH6.mapped.exe   # raw offsets == RVAs
python3 -I tools/init_records.py --image ghidra_proj/BH6.mapped.exe --out ghidra_proj/BH6.records.exe  # + the class records
python3 -I tools/sig.py --image ghidra_proj/BH6.mapped.exe --check        # every signature in src/sigs.h
python3 -I tools/sig.py --image ghidra_proj/BH6.mapped.exe --bytes 0xa62730 0x40   # bytes to write a pattern from
python3 -I tools/mt_classes.py --image ghidra_proj/BH6.mapped.exe [--grep uIdMenu | --vtable uPlPlayer | --tree uEnemy]
python3 -I tools/mt_props.py --image ghidra_proj/BH6.mapped.exe --asm ghidra_proj/BH6.asm cSystemInfo DamageInfo
python3 -I tools/globals.py --image ghidra_proj/BH6.mapped.exe --asm ghidra_proj/BH6.asm 0x17cf454   # -> sGameMan
python3 -I tools/gen_items.py      # src/items.h from the game's own item names (Msg_eng.arc; reads the game folder)
python3 -I tools/gen_records.py    # src/records_text.h: the records windows' texts (Msg_eng.arc, Steam's schema)
python3 -I tools/fsm_dump.py "$GAME_DIR/nativePC/arc/DX9/s1200Soft.arc" EmSet em_add_check   # a stage's scripts
python3 -I tools/merc_sim.py "$GAME_DIR/nativePC/arc/DX9/s1206Soft.arc" [--widen] [--nowait]   # its waves, each killed at once
python3 -I tools/proton_videos.py [--fix]   # a movie blank under Proton (WoW64): Valve's transcode under both keys
python3 -I tools/merc_records.py [--fill [--write --backup DIR]]   # the save's Mercenaries records (game closed)
tools/ghidra/run.sh dec 0xacc1a0           # decompile (several addresses per run; x:ADDR for references)
```

**Always install after a change.** Once a change builds and its checks pass, install it yourself:
- First check the game is closed: `pgrep -f 'steamapps/common/Resident Evil 6/BH6\.exe'` (under Proton `pgrep -x`
  can miss it - the Makefile's guard checks both).
- If it is running, say so and install as soon as it closes.
- Run `make install`, confirm with `cmp build/steam_api.dll "$GAME_DIR/steam_api.dll"`, and say in the reply that it
  is installed.

Installing is not a release. The VERSION file is the user's: never create or change it, never run `make
version`/`make rev`, never suggest a bump.

**Beside the DLL**: `steam_api_orig.dll` (the game's own Steam DLL, renamed), `RE6CabbyCodes.log` (the run before it
in `.prev.log`), `RE6CabbyCodes.ini`, `RE6CabbyCodes.imgui.ini` (the window `###RE6CC panel` and which sections are
folded).

**Diagnostic ini keys**: `Trace = 1` (every system the main system holds, and `probe.cpp`: the main systems' rarely
changing words logged as they change - what a screen or a key switches), `AlwaysShow = 1` (the panel everywhere),
`MemoryLog = 30` (where the 32-bit game's 4 GB goes, every 30 s and at each load; the mappings into
`RE6CabbyCodes.maps.txt` as room runs low and at a crash: `docs/engine.md`), `FrameLog = 1` (where the frames' time
goes, a line a second: the game's frame, Present, the draw thread, the mod's own, the shaders and textures made; a
frame over 100 ms its own line: `docs/engine.md`), `Watch = 0x...:4` (logs values at offsets from BH6.exe's base as
they change), `Disable = overlay,tick,game,input` (bisects a fault). Keep them on only for a diagnostic run, and reset
them after (`FrameLog` was on for the run after 2026-11-13's lag report, and set back to 0 when it ended).

`ghidra_proj/` (gitignored) holds a copy of BH6.exe, `BH6.mapped.exe`, `BH6.records.exe`, the Ghidra project `RE6`,
`BH6.asm` (a full objdump: "who touches this field" is a grep) and `classes.txt` (every class record).

## Tests (offline, under Wine)

Run them in a scratch prefix (`WINEPREFIX=<scratchpad>/wpfx`, `env -u DISPLAY -u WAYLAND_DISPLAY`), never the
user's, and `wineserver -k` afterwards. Each test's build line is in its header. What each checks in full is in
`docs/testing.md`.

| Test | Builds, over | Covers | Checks |
|---|---|---|---|
| `tests/test_discover.cpp` | the real `mt.cpp`, `units.cpp`, `game.cpp`, `cheats.cpp`, `inventory.cpp`, `merc.cpp`, `records.cpp` over BH6.exe itself (`BH6.records.exe` copied to its own addresses: a linker script, see its header) | every signature once, the class records, sUnit's lines, every layout read; every hook and patch in and out; the game's own subtract-health, chapter adds, match clock, inventory functions and loader (request, commit, frame over a made-up sLoadMan) run in the copy through the mod; One hit kills, Infinite ammo, the results, the QTE hook, the countdowns, the Mercenaries' switches, Add (ammo by the weapons held, no weapon; a grenade loading first) and Throw away; the editors at the pause only; the tick; the records windows' screens (the unit system as the game makes it, an sBioUnit), edits, rewards and save through the game's own gets, achievement and save steps; One hit kills on a boss that keeps its life (Deborah's and Brzak's own damage code run: each core, its stagger at one hit; Rasklapanje, Ustanak, Haos, the shot-down chopper through the game's take-damage); Infinite air (the game's own drowning run on a made-up character: off it drowns, on its air is held); One hit kills on the room bombs (Leon's chapter 4's floor robots, set models: their own hit checks run - 6 hits off, one on); install writes nothing in the game's data but its hooked vtable slots, each in a real vtable (zlib's table, once hooked as one, intact); the skills window (the game's own skill list fill, "can be bought" test and counts run in the copy: the mod's counts and rows equal theirs); Steady aim through a scope (the game's own scope callback over a made-up camera: the view held while the aim stands, followed as it moves, the gun's line along it); the event hooks (the health add, the gauge's add, the counter adds run in the copy through them; the reset, the units' start and destructor, the kill award through stand-ins; the tick writing nothing); the countdowns of an FSM's own (the game's own Timer order, timers' loop and CountDown order run in the copy through the rewritten read); Infinite stamina (the game's own spend, add and frame's refill run in the copy); Ada's helicopter - the game's own missile launch on a made-up uSm5060 (none taken, its load given back as the switch goes on) and God mode on a ride (the game's own subtract-health of the helicopter, a bike, an enemy's jeep); the weapons window (the game's own New Game reset sets up the eight saved inventories in the copy; its page reads, row lookup and record getter see each row named or taken back, each weapon given or taken out; the save run for a change to the cGameInfo alone); the enemies window (the ENEMIES page's own lookup, gets and test run in the copy: each "? ? ?" row named by a first kill, named rows kept, a reveal with another page up dropped); a chapter's best time set (the game's own get reads it; 0 too); Kill enemies as they appear (every enemy in play tried until dead - ten at once, a busy one once free; the mod's hit lethal through the game's own take-damage, harder after a shrug; a J'avo's mutation part left to its J'avo; the flow: the erases' 12 waits found and re-pointed at short ones while on, every area for AreaHit, a sender's stock widened with its script's earlier groups - a boss's script kept apart; the game's own sender frame run in the copy: no wait while on, the stage's minimum kept; a body whose kill counted ended at once - the game's own brain end step run in the copy ends it); the Mercenaries window (the game's own stage and character getters, the character select's start and the stage select's panel fill run in the copy: the mod's view, changes and screen fills equal theirs; the Collection files; the save) | 571, all passing 2026-11-15 |
| `tests/test_panel.cpp` | the real panel (`overlay.cpp` built in), headless, with ImGui's test-engine hooks | every cheat's click (Infinite stamina's and Infinite air's too), every editor's Set, the inventories (counts, Throw away, Add - the ammo a character's weapons load, no weapon, a pick preloading; the Wing Shooter's One Gun / Two Guns), the Results in a campaign and the Mercenaries in a match only, a greyed switch, F7 over play, nothing out of a game; the four records windows at their screens (a rank picked, a best time and a count typed as a player does, Apply all - the times shown after it as the game holds them, 0:00:00 too - F7, titles held short); the skills window (ticks, the fixed ones, Unlock/Lock all, F7); the weapons window (the grid's ticks where a campaign has the weapon, the fixed ones, each All, Give every weapon, Name every row, F7); the enemies window (a "? ? ?" row's tick, the named ones fixed, Reveal all, the "? ? ?" filter, F7); the Mercenaries window (stage and costume ticks, the always ones fixed, a character's All, Open every stage, Unlock every costume, No Mercy's bits, F7) | 147, all passing 2026-11-15 |
| `tests/test_boot.cpp` | the real `build/steam_api.dll` beside a stand-in exe importing what BH6.exe imports, the game's Steam DLL as steam_api_orig.dll and another tool's d3d9.dll (`tests/fake_d3d9.cpp`, ReShade's place) | DllMain's import hooks (crash filter, ExitProcess, DirectInput8Create, GetAsyncKeyState, Direct3DCreate9) bound and passed on, the Steam API's thunks into the original, Direct3DCreate9 going through the mod to the other d3d9.dll, the boot refusing a program that is not the game, an abort in the C runtime logged, the log and ini | 18, all passing 2026-11-15 |
| `tests/test_memlog.cpp` | the real `memlog.cpp`, `framelog.cpp`, `config.cpp`, `log.cpp` | the memory log: made-up 32- and 64-bit mapping lists, the live one found by pid, the Windows view, the mod's heap count, the ini key, its lines and dumps at start, a load and a crash; the frame log: its ini key, made-up frames - its line a second, a long frame's own line | 32, all passing 2026-11-14 |
| `tests/test_crash.cpp` | `crash.cpp` built in (the mod's -O2), its filter given made-up exceptions as UnhandledExceptionFilter gives them | the crash filter's chain (2026-11-10): one dump each with every register and the stack's top, the same exception again a line, a filter that chains back stopped (no loop), a new one raised inside passed on but not past three deep, a non-code filter not chained to, two threads, SetUnhandledExceptionFilter made to do nothing (said once, no line a second; abort still taken) | 26, all passing 2026-11-14 |

## Testing without playing

- **The user play-tests.** Never drive the game in their session, send it input or take screenshots there.
- **A headless run needs the user's word in that conversation** (`../RE5CabbyCodes` "Testing without playing": on
  this NVIDIA driver a gamescope's exit can wedge the GPU until a reboot). When the mod was made (2026-11-05) RE6 had
  just been installed: its Proton prefix set up at install (00:19), no launch options, no save in
  `userdata/<id>/221040/remote/` yet.
- **Never past the title**: a game started writes the user's Steam Cloud save and can unlock achievements.

## ⛔ Rules

- **Never patch `BH6.exe` on disk.** Every code patch is in memory, prepared from a signature with its bytes
  re-checked (`mem::Patch::prepare`), put in and taken out with its switch, reverted on unload.
- **Nothing that needs the game's classes before its renderer starts.** DllMain runs before the game's C runtime and
  its static initializers, which build MT Framework's class records; it only hooks import slots (already bound).
  Everything else waits for the game's first `Direct3DCreate9` (`boot.cpp`).
- **Never use absolute addresses or hard-coded offsets in code.**
  - Classes are found by name in MT Framework's own type records (`mt.h`).
  - Fields and functions are found by the signatures in `src/sigs.h`: read out of the matched instructions, then
    cross-checked against each other and against the classes' sizes and vtables.
  - The addresses in `docs/` document build 10112070 only.
- **Write only into a real vtable, and only as far as it goes.** The class index takes a table for a vtable only when
  slots 0-4 are code, and `hook_vtables` writes a slot only when every entry before it is code: data can hold a
  getDTI's address (zlib's inflate table holds uEm6527's - hooking it broke every start, 2026-11-06).
- **Game memory from the tick or a hook only.** The panel runs on the render thread: it flips atomics and posts
  requests, and the tick applies them (`dispatch.cpp`). Hooks touch only the objects the game handed them.
- **Edits only while the game is paused** (sBioMain's own pause byte: the world stands still) - in play; the records
  windows edit the profile at their menu screens only (never in a game); the skills window at the skill screen only
  (the menus, or a chapter's end).
- **Game calls: only the approved kinds. Ask before adding any other call.** The siblings' user approved: the
  inventory's own functions, the item box's Take and Store, the difficulty switch, the save files' delete and refresh,
  and event hooks. This user approved the game's loader for Add (2026-11-05: "You should fix the game to be able to
  handle these adds instead"): sLoadMan's request and commit, and its frame run at the pause. On 2026-11-06, for the
  records windows, the user chose "Emblem + Steam achievement" (the game's own achievement call, sInfoMan's) and "Save
  at once" (the game's own profile save after each Apply: the steps its menus take as they close). The mod's only
  game calls are the inventory's own (set-slot, top-up, refresh, clear: Add and Throw away), the loader's (an Add's
  archive), the achievement and the profile save's steps (the records windows, the Mercenaries window too - a mode's
  start reverts what is not saved); the rest is field writes, vtable
  hooks, call re-points that call on to the game's own function with the game's arguments (the tablets', the QTE
  check's, the countdown displays'; the room bombs' instant test, answered yes under One hit kills), and verified byte
  patches (one, the CountDown's in-place read of its FSM's timer, made a call of the mod's own read: no game call).
  The skills window calls nothing: field writes, saved by the skill screen's own close.
- Back up `Steam/userdata/<id>/221040/remote/` before experiments that write saves.
- The user commits every repo themselves - do not `git commit`/`push` unless asked.

## How it works (in brief; `docs/engine.md` has the detail)

- **The proxy is steam_api.dll**, in front of the game's own (renamed steam_api_orig.dll): 55 jump thunks and one data
  forwarder, generated from the original's export table. It is the one DLL BH6.exe imports that ships with the game
  and is no common mod's - d3d9.dll (ReShade, DXVK), dinput8/version/winmm.dll (ASI loaders) and xinput1_3.dll stay
  free - and not a Wine builtin, so Proton loads it with no launch option. Steam's "Verify integrity" puts the game's
  own back over it (steam_api_orig.dll stays): installing again fixes that.
- **DllMain** loads steam_api_orig.dll (a message box with the fix if it is missing or not a Steam DLL the game can
  use), the crash logger and the ini, and re-points BH6.exe's own import slots for SetUnhandledExceptionFilter,
  ExitProcess, DirectInput8Create, GetAsyncKeyState and Direct3DCreate9 (found by name; only slots already bound) -
  the last calls on to whichever d3d9.dll the game was bound to (the system's, ReShade's, DXVK's).
- **Boot** happens at the game's first `Direct3DCreate9`, on the main thread: the class records (found from
  MtObject's, built by then), the discovery (`game::discover`, `cheats::install`), the tick's hook.
- **The tick** is sBioMain's own vtable slot 6 (its frame), on the main thread. It observes (the pause, the
  characters, the match) and fires events - a switch turned, a character come in; the cheats' work is in hooks and
  re-pointed calls (2026-11-12: no writes each frame; `docs/engine.md`). The panel draws in Present from the snapshot
  the tick publishes (read only while the panel can show). The window procedure is subclassed for ImGui.
- **Screens**: the panel shows while sBioMain's pause byte is set in a game (characters in sPlayer's array). The
  records windows show beside their menu screens (uIdRecord00's page and view, uIdCustomize10's list step), the
  skills window beside uIdSkillSet10 (in a game too), the Mercenaries window beside uIdStartUp31/32 (the stage and
  character selects; in modes 0x1E and 0x50 only) - each screen known by its own start and destructor, hooked; F7
  hides one until its screen changes, and the game's mouse is held only while over the window.
- **Windows** is audited by design and never run there: the adopt path (the Steam overlay), the window known by its
  class's procedure, files written whole and moved over (virus scanners), no ASLR in the exe.

## Notes (`docs/`)

**`docs/engine.md`** - the proxy and the boot; MT Framework 2's class records and how the mod and the tools read them;
the systems (their globals), the units, the tick and the pause; threads; Windows; Proton's movies.

**`docs/cheats.md`** - the characters (health, who drives a player, sPlayer), the damage paths (a player's hit, take-
damage and subtract-health; an enemy's), each switch's hooks and patches, the game's infinite-ammo bit and what else
it spares (the tablets), Steady aim (the sway, the kick), Automatic QTEs (sActionButton, the scenes' option), the
combat gauge (stamina, the air under water), the rides (Ada's helicopter: its health, its missiles), the skill points
and the skill screen.

**`docs/modes.md`** - sGameMan (the mode, online, the match), the chapter's numbers and their copies, the countdowns
(the match clock, a stage's timers and their displays), the Mercenaries (a kill's way, the combo, Combo Time).

**`docs/inventory.md`** - the items' ids and names (`tools/gen_items.py`), the slots, the inventory's own functions,
the loader (sLoadMan: an item's archive, the lists, loading one at the pause).

**`docs/records.md`** - the profile and its two copies, the menus' save; the chapter records and a clear's rewards; the
medals; the emblems and the game's achievement; the titles and their table; the Records and Dog Tag screens; the
skills (the skill table, each skill's state, the game's "can be bought" test and its counts, the skill screen); the
Mercenaries' unlocks (the stage and character bits, the getters, the stage and character selects, the costumes'
Collection files, what unlocks each in play).

**`docs/testing.md`** - each test's full check list.

**`docs/history.md`** - the play log (empty until the first report), the in-game checklist, next to build.

## Status (2026-11-15)

Everything above is built, tested offline (571 + 147 + 18 + 32 + 26 checks) and installed (as steam_api.dll; the siblings' set
completed 2026-11-05 afternoon, the records windows 2026-11-06, the skills window 2026-11-07). Nothing has been seen in play yet - what to look for is
in `docs/history.md`, "In-game checklist".

Open:
- The save's Mercenaries records filled (2026-11-15 22:30; `docs/records.md` "The Mercenaries' unlocks"): the user asked
  for S ranks on every stage x character not played, both modes, scores estimated from their own records - 297 solo
  records written offline by `tools/merc_records.py` (the save's checksum: the sum of its dwords), each under its
  stage's best (the online ranking uploads the stage's best record). Backup: `~/RE6-save-backups/2026-11-15_before-merc-
  s-ranks/`. Unseen in play (Steam Cloud uploads the file at the next start).
- The Mercenaries window (2026-11-15 night; `docs/records.md` "The Mercenaries' unlocks", `docs/history.md`): "add
  imgui window to the mercenaries and mercenaries no mercy modes to unlock levels and alternate players / costumes".
  Beside the stage select (uIdStartUp31) and character select (uIdStartUp32) in modes 0x1E/0x50: the mode's stages
  (cSystemInfo +0x970; the first three of each always open) and the characters' costumes (+0x978, 64 bits; Leon's,
  Chris', Jake's Default and the Agent always; No Mercy's L4D2 four always there, not the profile's), each with its
  Collection file; the stage select's panel and the character select's lists filled as their starts fill them; saved
  at once. Costumes 2 and 3 came only from the online event list - the user's save had none. Built, tested offline,
  installed 22:06; unseen in play.
- The Mercenaries' enemies sent faster, bodies gone as they die (2026-11-15; `docs/modes.md` "The senders' waits", "A
  body's end"; `docs/history.md`): "we could spawn more enemies in faster", then "decrease the death animation ...
  instead of just kicking the can with a higher cap" (not the stage's limits). While Kill enemies as they appear acts:
  EnemyAdd's timer test sends at once (its jbe a jmp, a verified byte patch) - a sender sends whenever fewer than its
  minimum are in play; and a body whose kill has counted has its end byte set (uEnemy +0x3B30, what every erase sets
  last), so its brain's own end step (uEmIntel's 0x942B10) ends it the next frame - no death animation, no dissolve.
  Simulated: a clear in 5-6 s (No Mercy's 10 s). Built and tested offline; the game was running at the build -
  installed 20:53, as it closed. Unseen in play.
- Steel Beast stopped at 89 kills (2026-11-14 night; `docs/modes.md` "A wave's leftovers", `docs/history.md`): s1206
  (Steel Beast - the stages' names in `docs/modes.md` "The stages") goes wave to wave by kills, each wave stocking
  only its own groups; killed as they come, the sixth wave ends with half of group 8 unsent and the seventh waits for
  90 of 95 - 78 + 11 = 89 (`tools/merc_sim.py`: 89 there; every other stage clears). While Kill enemies as they
  appear acts, each sender's stock takes the groups of every sender before it in its script too (EnemyAdd's builder
  call re-pointed; a boss's script kept apart): simulated 152. Built, tested offline, installed 23:36; in play the
  user's word on 2026-11-15: "it's working now".
- The Mercenaries' flow (2026-11-14 night; `docs/modes.md` "How a stage sends its enemies", `docs/history.md`):
  "large waits before more enemies spawn" - a stage sends while fewer than its minimum (3-13) set-placed enemies are
  in sEnemy, bodies too (~10 s each till erased); "stop spawning unless I move around" - s1200/s1201/s1204 send by the
  player's area (AreaHit -> RSC[0]). While Kill enemies as they appear acts: the J'avo's, zombies', dogs', Napad's and
  the Whopper's erase waits load 0.25 s / 8 frames (the user's choice over leaving bodies out of the count: the stage's
  limits kept), AreaHit finds the player in every area (the first's groups sent). Built, tested offline, installed
  23:05; unseen in play.
- Kill enemies as they appear missed enemies (2026-11-14 night; `docs/history.md`, `docs/modes.md` "The Mercenaries"):
  "sometimes it's a regular enemy that seems to get skipped, while other times ... a certain enemy type is not
  killed". Skipped: only an enemy a first hit had been written into was tried again (the sweep as the switch went on
  wrote 4, the rest never). A type: a class works the mod's hit out by the part it lands on (Gnezdo 1%, armour,
  limbs). Now every enemy in play is hit until dead, and the take-damage hook takes all the health for a hit of the
  mod's (harder after a shrug, another part each time); a line per enemy type at each match's end in the log. Built,
  tested offline and installed (22:19, as the game closed). Rasklapanje can never die by health (its own floor): hit
  on another part each time - unseen in play.
- Chapter times; Add without weapons; the Wing Shooter's two rows (2026-11-14 evening; `docs/history.md`): a cleared
  chapter's best time typed in the chapters window (Chapter Select shows it - in play 21:19: "the game is correctly
  updated"; a first clear without one still 19:59:59, the game's "none"). The window then showed 19:59:59 for the
  0:00:00 applied (a 0 taken for the "none"): fixed, a cleared record's time shown as Chapter Select shows it (cut
  to whole seconds, 0 as 0:00:00) - built and tested offline, to install as the game closes (running at the build).
  Add offers no weapon and only the ammo the character's own weapons load (the game's 0x574E90, in items.h); "two
  Wing Shooters" was one gun's two counts (One Gun 18 / Two Guns 36), now named so - installed 19:41; unseen in play.
- The enemies window (2026-11-14; `docs/records.md` "The enemies"): beside Records > ENEMIES, each "? ? ?" row named by
  its first kill (Enemies Defeated below 1 made 1; a named row's counts kept, nothing taken back), Reveal all, the
  profile saved after each change. Built and tested offline only; installed 19:13, as the game closed (it was
  running at the build).
- The weapons window (2026-11-14; `docs/records.md` "The weapons"): beside Records > WEAPONS, a row's "? ? ?" named by
  its first use (the page names a row only once used, or always: the starting weapons), and each character's
  campaign weapons put in or taken out of its saved inventory (what Continue and Chapter Select start from) - a grid,
  the profile saved after each change. Built and tested offline only.
- Infinite stamina; Ada's helicopter under Infinite ammo and God mode (2026-11-14; `docs/cheats.md` "The combat
  gauge", "The rides"): uPlPlayer's spend (slot 123) takes nothing and the gauge's add fills it on land; the
  helicopter's own missile count (6) not taken (two verified byte patches), given back as the switch goes on; every
  ride's subtract-health (slot 44, 27 vtables) spares a ride a spared character rides - built and tested offline,
  installed when the game closed (it was running); unseen in play.
- The pause: the panel shows while sBioMain's pause byte is set - the game's Pause key (pad Start; on the keyboard
  the key bound to Pause). Seen in play 2026-11-05. Esc opens another menu (not the pause): no panel there.

- Every start crashed loading from 17:09 to 18:47 on 2026-11-06: One hit kills' slot-126 hook wrote into zlib's table,
  which the class index had taken for a vtable (`docs/history.md`). Fixed 18:55; seen in play 21:50-22:40 (the game
  loads and plays; Rasklapanje's parts broken at one hit in s0511).
- The records windows (2026-11-06, `docs/records.md`): built and tested offline only - their screens, a change showing
  at once there, the game's save after Apply, Steam's achievements through the game's call.
- One hit kills on a boss that keeps its life (2026-11-06; `docs/cheats.md` "The bosses and One hit kills"): a player's
  hit on it is worked out as 32767, so its parts, stagger and counters go at one hit (Deborah's cores, Brzak's tongue,
  Rasklapanje's limbs, Ustanak's knockdowns, Haos' damage count); a boss only the stage's script ends (Brzak, Ustanak,
  Haos on the pipes, the s0508 chopper) is taken to its last point, never 0 - built and tested offline only.
- One hit kills and Haos (2026-11-13; `docs/cheats.md` "Haos"): "never shows the waist blisters ... to make it go
  into the cocoon", reported from stage 972's climb (the blisters and the cocoon are stage 973's last fight; seen
  there at one hit each at 16:26-16:28). Found: the chase after the climb (mode 1) was held at its last point, but
  stage 972 waits for its death - it dies at one hit now; on the pipes (mode 2) still kept. Built, tested
  offline and installed (18:08, as the game closed); the chase's end unseen in play.
- Infinite air (2026-11-06; `docs/cheats.md` "The air under water"): the air under water is the combat gauge; the tick
  holds it at its most while a character this machine drives is under water, so the game's drowning (stages 250 and
  1003: Leon's chapter 3, Ada's chapter 1) never comes - built and tested offline only.
- One hit kills on the room bombs (2026-11-06; `docs/cheats.md` "The room bombs"): the floor robots of Leon's chapter 4
  (s0512) are set models (uSm1310), 5 points and a point a hit; their two calls of the game's instant test are
  re-pointed at the mod while the switch is on, so the players' first hit sets one off - built and tested offline only;
  installed when the game closed (it was running).
- The skills window (2026-11-07; `docs/records.md` "The skills"): the skill screen's 19 skills sold only after a
  requirement (kills, weapon kills, every campaign), unlocked or locked again; built and tested offline only - its
  screen (menus and between chapters), a change selling at once, the screen's close saving it.
- Steady aim through a scope (2026-11-12; `docs/cheats.md` "Through a scope"): a sniper's scope still swayed (the
  view is the gun model, in the breathing aim pose); the scope's view is now held in the body's frame while the aim's
  pitch and twist stand, the gun's line along it - built and tested offline only.
- The lag (2026-11-13; `docs/history.md`): "still very laggy ... when loading a save ... and while playing". The
  next run ("running fine this session"), read with the frame log and a `/proc` sampler: every stall was DXVK
  compiling what the game met first (a load 10.5 s with its 8 compile threads at 100%; ~10 s of play at down to 7
  fps, its command thread at 100%); the rest 120 fps, the mod 0.03 ms a frame. Why the runs before lagged: Proton
  Experimental's update (2026-11-10) brought DXVK 3.2.0 - RE6's 2026-11-06 runs had 3.1.1 (gamescope's log has each
  start's `engineVersion`) - so every pipeline compiles again, run by run. The frame log's long-frame lines printed
  garbage (a dead stack buffer's): fixed, tested and installed (14:20).
- Freeze countdown timer and stage 851's countdowns (2026-11-13; `docs/modes.md` "The countdowns"): "isn't working
  for the area I'm currently on ... chris and piers' section". Stage 851's scripts count down a timer of the FSM's
  own, which the CountDown order reads in place - the mod saw only the stage timers' reads. That read is now
  rewritten into a call of the mod's (kSigCountDownOwn; the timers' words from kSigOwnTimers), the hold covering
  both kinds and the CountDown's timer counting up (stage 1003's) - built, tested offline and installed; its read
  seen in the 15:21 run's log (`... shows its FSM's own timer 0`), the countdown standing still not yet confirmed. A
  countdown whose end is the way on (stage 901's hold-out) stands still too: the switch off there.
- A cutscene played blank (2026-11-13, stage 851; `docs/engine.md` "Proton: the game's movies"): not the mod.
  Proton's movie converter keys Valve's transcodes by a hash whose variant follows WoW64 (the user's
  `PROTON_USE_WOW64=1`), and Valve's file had e5120 (and three others) under the 32-bit key only.
  `tools/proton_videos.py --fix` added the missing keys to Steam's file (15:44); unseen in play. Run it again if
  Steam downloads the file again.
- The cheats made event-driven (2026-11-12; `docs/history.md`, `docs/engine.md` "The tick is an observer"): the
  per-frame holds became hooks (uPlayer's health add, reset and gauge add; sInfoMan's counter adds; the kill award;
  the enemies' and screens' start and destructor) - built, tested offline and installed; unseen in play.
- The crash filter's chain (2026-11-10; `docs/engine.md`): no loop through a filter that chains back, standing aside
  when another crash handler keeps SetUnhandledExceptionFilter's slot, a dump with every register - tested offline
  only.
- Unseen in play (marked [I] in docs/): Kill enemies as they appear's deaths (a hit written into each enemy's record),
  the scope's own motion under Steady aim, a stage countdown read through the mod, the QTE scenes' Amateur paths; Add
  of a grenade or the spray nobody holds (the game's loader run at the pause, built 2026-11-05 at the user's word;
  Add's weapons removed 2026-11-14 at the user's word).

Next to build: `docs/history.md`, "Next to build".

## Tooling notes

- **The Ghidra project**: `MAXMEM=12G /opt/ghidra/support/analyzeHeadless ghidra_proj RE6 -import
  ghidra_proj/BH6.exe -overwrite -loader PeLoader` (~8 min). `tools/ghidra/run.sh dec|xref|funcs ADDR...` takes about
  6 s a run.
- **The asm**: `i686-w64-mingw32-objdump -d -M intel --no-show-raw-insn ghidra_proj/BH6.exe > ghidra_proj/BH6.asm`.
- **Field names**: MT's reflection names many fields (`tools/mt_props.py`): cSystemInfo's mSkillPoint and
  mOption.mInfiniteAmmo, cGameInfo's mStageNo/mChapterNo/mDifficulty, HitInfo's Damage, uEnemy's mIsNoDead and
  LifeMax. uCharacter and uPlayer register none of their own.
- **Systems by name**: the main system's list names each by its own class - the game's subclasses (sBioUnit for sUnit,
  sBioArea, sBioStorage ...); `game::system("sUnit")` finds the subclass's. A test's made-up system takes the class the
  game's has (a "sUnit" stand-in hid the records windows' first bug, 2026-11-06).
- **ImGui's checks**: the mod builds ImGui with its own IM_ASSERT (`src/imconfig_re6cc.h`): a failed check is logged
  (`ERROR: overlay: ImGui's check failed`), not the C runtime's assert, which ends the game with no word in the log.
  The tests keep ImGui's asserts, and drive a widget as a player does (a field typed into, a combo's item clicked),
  not by writing its state: a colour pushed on a condition the widget itself changes was popped unpushed at the first
  key (the medals' crash, 2026-11-06).
- **Globals**: each singleton's pointer is a global its constructor stores (`tools/globals.py`): sMain 0x186E1B4,
  sUnit 0x186E5F4, sPlayer 0x17D0468, sGameMan 0x17CF454, sInfoMan 0x17CF49C, sId 0x17CF478, sQlocInput 0x17C1384.
  The mod finds the systems through sBioMain instead (its fields).
