# Testing

All offline, under Wine, in a scratch prefix (`WINEPREFIX=<scratchpad>/wpfx`, `env -u DISPLAY -u WAYLAND_DISPLAY`,
`WINEDEBUG=-all`), never the user's; `wineserver -k` afterwards. Build lines are in each test's header. The zsh of the
Bash tool does not split an unquoted `$VAR` holding a command: write each command out.

## tests/test_discover.cpp - the discovery and the cheats over BH6.exe itself (571 checks, 2026-11-15)

`ghidra_proj/BH6.records.exe` (tools/map_image.py, then tools/init_records.py) is copied to 0x401000.. inside the
test's own image (a section reserved there by a linker script; the test's code above 0x1900000), made executable, and
the mod's real `mt.cpp`, `units.cpp`, `game.cpp` and `cheats.cpp` run over it.
- every signature found as often as sigs.h says, every name its own;
- the class records: MtObject's and the root found, over 8000 records, sBioMain (its vtable, 0x104D8 bytes), uPlayer,
  uPlPlayer, uEnemy, uCharacter, a costume (uPl00LeonUsa) deriving from uPlayer;
- sUnit's lines (24 bytes, flags +0x3C, first +0x40, a unit's state +0x10, next +0x18, the count +0x634);
- game::discover: the pause sBioMain +0x1049C; health +0xF10/+0xF12; who drives +0x4E90; sPlayer's 8 at +0x24; a
  hit's damage +4, dealer +0x28; the inventory +0x46E0 (24 x 0x1C); the infinite-ammo bit 11 of +0x4E74; the skill
  points sInfoMan +0x72C + profile (+0x20) * 0xFDD8;
- cheats::install: every switch available; 54 subtract-health, 51 hit, 54 take-damage hooks (players), 191
  take-damage (enemies), an override that calls the game's hooked too. What it writes into the game's data, the
  whole .rdata and .data compared before and after: nothing but the slots it hooked (1161 on 2026-11-14), each in a
  table that is a vtable as far as that slot (every entry before it a function); zlib's fixed Huffman table
  (0x17014A0) untouched -
  uEm6527's vtables are its own, not the two places in that table holding its getDTI's address; One hit kills' slot
  126 in exactly the 191 vtables its take-damage went into. (The code before the fix fails these four: its slot-126
  hook wrote into the table at 0x1701704 +504.) A slot-0 hook (`vthook::reaches`) refuses zlib's table too: its
  first entry lies inside the code, the four after it not (2026-11-12). The event-driven hooks (2026-11-12): 54 health
  add (slot 43), 54 reset (109), 52 air add (122) - players; 191 start (slot 5), 191 destructor (slot 0) - enemies;
  all in the install's integrity check above; and (2026-11-14) 51 gauge spend (slot 123) - players; 27
  subtract-health (slot 44) - rides;
- God mode, through the game's own subtract-health (0x95DA40, run in the copy): off, 300 of 1200 taken; God mode,
  nothing from yours, a negative amount (health given) still goes on; the AI's still damaged with God mode alone, not
  with the partner's switch; the network's always damaged; a costume's vtable holds the mod's hit and take-damage; the
  hit reaches the game (a stand-in) with damage 0 and is put back; a spared player's take-damage does nothing; off,
  both reach the game as they were; the health add (slot 43, 0xA63AE0, run in the copy through the hook): with God
  mode an add taking health (-150) takes none and any add fills up (1200), off the game's own (-150: 1050);
- One hit kills: off, a player's hit keeps its damage; on, 800 of 800; no damage stays none; an enemy's hit on another
  and a hit nobody dealt keep theirs;
- One hit kills on an enemy that keeps its life: the take-damage's keep-alive flags +0x3B33 and mIsNoDead +0x3B32,
  the parts +0x3BE0 (broken +0x3CE0); Deborah's damage worked out (uEm4500's slot 126, 0x761FC0) handing on no health
  under bit 1 of +0x2A08; slot 126 hooked in every enemy vtable (191, each class's own). Her own damage code run in the
  copy through the hook (sGameMan, sBioMain, sPlayer and the may-die system made up at their globals, her slot 61 a
  stand-in for the region hit, her parameter set as her file has it): off, a front-core hit takes 150 of 600; on
  (mIsNoDead, the core fight), the front, left and right cores each broken at one hit and marked by her own code
  (+0x2A08 bits 3-5), a big hit's health handed on, the hit's damage put back; her own hold (mActStatus bit 12)
  keeps a core at 1; an enemy that can die - the hit as it is; the other keep-alive flag the same; the mine cart (her
  health held) - a 250 core broken, no health handed on, and with her health not held as it is; the last fight's
  front core (region 0x17) broken; an enemy's hit, and the switch off, as they are. The game's own take-damage on an
  uEm4800 (its slot 129 a stand-in): off, its parts see 120 and it loses 120; on and mIsNoDead, its parts see the
  big hit, it keeps its last point, the hit put back; on and able to die, its parts see 120, it dies (marked);
  Brzak's damage worked out (uEm4200's slot 126, 0x7285E0) found as handing on 1 whatever the hit; its own code run
  in the copy (a made-up resource +0x3D80 holding a cEm4200Param with its file's rates): off, a tongue hit takes 150
  of its stagger's 1000; on, one empties it (the hit put back); a body hit nothing; its take-damage the game's 1 of
  32700 taken to its last point, never marked dead, and kept there;
- the other bosses that keep their life, each found (Rasklapanje's floor 0x772270; the brain +0x3A54, its death reaction
  +0xA0, uEmIntel's none 0xA242B0; Haos' 0x8DDE80 and its mode +0x41C4; the choppers' 0x906790, stage 508 until scene
  150, cGameInfo +0x1B0C/+0x1B14) and run through the game's own take-damage (a stand-in in slot 129): Rasklapanje's
  parts see the big hit, its floor keeps 1; an uEm7000 with its brain (no death reaction) gets the big hit and stops
  at its last point, a big amount too, one with no brain dies as before; Haos on the pipes (mode 2) kept at its last
  point with the big hit (and stays there), in the chase (mode 1) and in mode 4 dies at one hit, climbing (mode 0,
  mIsNoDead) the game's point with the big hit;
  the chopper over a made-up sInfoMan kept in stage 508 before scene 150, dies from 150 on; an uEm3000 as before;
- Infinite ammo: the bit set on yours (the word's other bits kept), not on the AI's; the three tablet uses found
  (0xACC532, 0xDBBBD9, 0xDC7073) and re-pointed while on; a tablet's use reaches the game with the bit let go and the
  bit back after; off, the bit the mod set cleared and the three calls the game's again; a bit the game set itself
  kept; the players' reset (slot 109, 0xA74B30) hooked in every player vtable - a reset of yours (a stand-in for the
  game's, through the hook: the flags put at 0) gets the bit back after it, and the tick writes nothing;
- the helicopter's missiles (2026-11-14): uSm5060's mMissileNum +0x41C0 (a copy a seat), 6 from its setup (0x1772008);
  the missile action's start (0xB81440) and its two takes (0xB81475, 0xB8147E). The game's own launch run on a
  made-up helicopter (the action's owner +8, its seat +0x10): off, one off both copies (5, 5), the seat's launch a
  missile and its wait a second, dry with none left; on a made-up sBioUnit's line with Chris's jet (uSm5020): the
  tick gives nothing back while off, as Infinite ammo goes on the helicopter gets its 6 back (the jet none), both
  takes NOPs, a launch of either seat takes none, the tick after gives nothing; off, the takes back;
- God mode on a ride (2026-11-14): uSmRide's subtract-health (slot 44, 0xBB3DF0; its riders +0x30A4, +0x30A8) hooked
  in 27 ride vtables (the bike's and jeep's own too). The game's own, over a made-up sPlayer at its global: off, the
  helicopter you fly takes 150 of 300 (halved by your level); God mode, nothing - in the second seat too; the
  computer's partner riding: taken with God mode alone, nothing with Partner takes no damage; the network's taken;
  the bike you ride (its own, then the rides') nothing; an enemy's jeep a J'avo rides all of it; health given back
  goes on; off, the bike's damage again. As God mode goes on, over a made-up sBioUnit's line: the helicopter you fly
  filled up (900 to 1500), an enemy's jeep and a ride at 0 left; the tick after holds nothing;
- Infinite skill points: both subtractions NOPs while on, the game's `sub` back when off;
- the editor: the panel's points are the profile in use's (1); in play nothing is written; at the pause the profile in
  use gets the value (the other untouched);
- the tick over made-up sBioMain and sPlayer: two characters (a third, its unit on its way out, skipped), names and
  controllers and health; the pause byte read; the partner's health filled up as the switch goes on, but one at 1
  (down) left; nothing written by the tick after (no hold each frame); the panel unable to show (not paused, no
  AlwaysShow, the panel not up): no character's items, counters or match numbers read;
- Infinite air: the air (the combat gauge) +0x3B00, its most +0x3B04, under water bit 30 of +0x4E6C, read out of the
  drowning (0x484750: action 0x226, sPlayer's global 0x17D0468) and the gauge on screen (uIdOxygen, 0x9E0714). The
  game's own drowning run on Leon in the copy (its under-water test, its slot 58, sPlayer's set-health over the
  made-up sPlayer put at its global; slot 46 a stand-in recording the action): off, under water with no air - health
  0 and the drowning (0x226) played; the game's own add (slot 122, 0xA63B90, hooked in every player vtable) takes a
  dive's 8 off (112); on, nothing written by the tick; under water the game's own add of a dive's drain made a fill -
  from 112 it stops at 400 (a refill crossing 300), the next at its most (500), and keeps it there; the drowning does
  nothing; a gauge emptied is refilled at the game's next add (400), before its frame runs the drowning, full at the
  one after; on land the gauge is left to the game; the computer's partner under water is left to the game; a
  character played over the network is left (and the drowning passes it by); off again, nothing held;
- Infinite stamina (2026-11-14): the spend uPlPlayer's slot 123 (0xACE4C0, the slot after the add's; the game's own
  takes nothing under skill 0x54, Training), hooked in 51 player vtables (Leon's), not uPlEnemy's own (0xAA5390); the
  add's pause +0x3B08. Over made-up sInfoMan, sPlayer and sGameMan at their globals (the spend's skill check), the
  game's own spend (slot 123), add (122) and frame's refill (126) run on Leon in the copy: off, a block taken (1100 to
  1000), 300 or less left emptied, a drain's rate taken (1075), nothing under Training (the skill in sGameMan's
  table); on, nothing written by the tick, no spend taken, the refill fills to the most, a drain's rate and the add of
  a drain held there, an emptied gauge refilled at the next refill (400, then 1100 - a rate of 0 too), the hit's pause
  as the game's, no refill while down and full once up; left to the game - a scene's flag (+0x4E80 bit 16) and Piers'
  arm (uPl03PiersCanon) emptying it, under water the dive's drain (Infinite air's fill when that is on), the
  computer's partner, the network's; off again, the game's own;
- One hit kills on the room bombs (Leon's chapter 4's floor robots, set models): the two hit checks found (0xBB94A0
  while one rolls, 0xBB9F90 in its warning), a set model's hit record's dealer +0x2B5C, the game's instant test
  0xAD2F20 called at 0xBB94F5 and 0xBB9FD9. A made-up uSm1310 (5 points, as its setup leaves it; its slot 80 - post an
  event - and slot 46 - play an action - stand-ins in a copy of its vtable) through the game's own hit check, its
  event 9 (0xBB83D0) and its slot 81 (0xBB99B0): off, Leon's hit takes a point (event 9 with 1, action 9) and it takes
  6 hits to go off (action 10); on, both calls are the mod's and the first hit - Leon's, the computer's partner's, a
  players' weapon's (uWp1010) - empties its points and sets it off (event 9 with 0, action 10); no damage, a stage
  blast (uSm9045), an enemy's hit and a character played over the network are turned away by the game's own check.
  Its Warning frame (0xBB9F90, run over a made-up sBioMain at its global): on, Leon's hit sets it off at once (event
  10); no hit, nothing; off, one point and its second counts on; off again, both calls the game's;
- the inventories: items by slot number, in hand marked; a count set at the pause only, held at its most, an item
  moved since left alone, a network character left alone;
- the modes (sGameMan +0xAC, online +0x878, the match clock +0x486C and its fields), the chapter's numbers (cGameInfo
  sInfoMan +0x3F798, blocks of 0xB8: the clock +0x1B3C, counters +0x1B40; shots 0, hits 1, kills 2, deaths 3), the
  stage timers (+0x664), the Mercenaries' match (+0x4264 ...) and an enemy's hit record (+0x24A0; mPos +0x50);
- Steady aim's two patches in and out;
- Steady aim through a scope: the scope camera's call (the one of the three camera modes' callbacks whose camera calls
  the sway, 0x62E1DF) and the laser's call of the gun's line (0xA6CDE4) found and re-pointed while on; the game's own
  scope callback run on a made-up camera of Leon's (the scope camera stood in for): the game's view while the gun is
  raised, held 0.2 s after it settles, the gun's breathing (half a degree, 3 mm) and a shot's kick (4 degrees, 5 cm
  back) never reaching the view, the pitch's and the twist's moves followed on their own axis only, the body's turn
  carrying the view and its eye, a gun that never settles held at 1.5 s; the gun's line along the held view (as long,
  from where the game has it), the game's own once the view is gone; a character played over the network never held;
  lying on the back (the body's up along the ground), held through the breathing and the aim's turn followed whole;
  off, the game's view at once and both calls back;
- the chapter's numbers: the game's own counter and clock adds (0xD705D0, 0xD70550, run in the copy) land where the
  mod reads them; Freeze play time (the clock add adds nothing), Deaths don't count (`push 0`), Misses don't count
  (the shots' call 0x569149 and the hits' two, 0x56FA39 and 0x51AD56, of sInfoMan's counter add 0x464A90 found and
  re-pointed while on; the shots put at the hits as it goes on; a shot of yours counts nothing until it hits; a hit -
  the game's own add, run in the copy - raises the hits, then the shots with them; the computer's partner's shot
  counted by the game; the tick writes nothing); the editors at the pause only, live and in the save's copy, a counter
  held at 50000, the clock in every block;
- Automatic QTEs: the 19 scene tests found; on, the managers' check re-pointed and the tests read as on (not with a
  character online); the hook over a made-up unit: a waggle of yours answered and hidden, not the computer's, not a
  context action's press, a dodge's press yes, a press in time waited for then marked good, not with input held back
  or one already failed, the game's own yes passed on; off, all back;
- the countdowns: the game's own match clock (0x55A610, run in the copy) frozen, set at the pause (and the HUD's copy),
  running down when off, left alone online; Endless combo - the kill award (0x553590) and its one call (0x55C287)
  found, the window held and the call the mod's while on, a kill's award (250, through a stand-in at the award) put
  on the score as it is earned, at most the cap, the tick writing nothing; off, the call the game's again; Combo
  Time always on (started once, 30 s, held), the panel's match numbers; Kill enemies as they appear (2026-11-14) -
  the enemies' start and destructor hooked in every enemy class's vtable, the take-damage hooked (it asks the mod); an
  enemy coming into play (its start, through the mod's hook) known, its hit written at the next tick (all its 500, by
  Leon, on part 0), one not taken by the following tick cleared, not written again at once (100 ms), then again; its
  take-damage (`merc::lethal`): a game's hit as it is, the mod's worked out as 0 left so at first, as 30 all its 500;
  taken and alive after - a shrug, tried again only after 400 ms, then 32767, after three all its health from nothing,
  then part 1; dead - the mod's kill, never hit again; destroyed (through the hook) forgotten; through the game's own
  take-damage (uEm5200's, 0x95D320, behind the hook; the "may it die" system made up at its global) the mod's hit
  worked out as 30 takes 500 to 0 and marks it dead, a game's hit of 30 takes 30; off, ten enemies coming in known, no
  hit; on again, eight a tick and the ninth at the next, one whose record held a game's hit hit once free; at the pause
  the hits cleared and none written or counted, again after; the panel's in-play count; all forgotten as they go; off
  as the match ends (the code before fails the ninth and the busy one: never tried); a J'avo's mutation part whose
  hit the J'avo took - the J'avo's kill, the part not hit again (nor counted) while the J'avo is about, again once it
  has gone; the flow (2026-11-14 night) - the erases' 12 waits found where the build has them (the J'avo's 0x6BE413,
  a zombie's 0x7A4E9E/0x7A4E83, a dog's 0x80DCC8/0x8091E9/0x80DDBA, Napad's 0x70CA14/0x70CAE2/0x70CB48, the Whopper's
  0x7FFA18/0x7FCA32/0x7FCAA1), on each loading the mod's 0.25 s / 8 frames, off each the game's own constant again;
  AreaHit's area test (0xD5A561 -> 0xE19030) the mod's while on (every area), the game's off (nobody in a task with no
  shape); a sender's stock builder (0xD593F5 -> 0xD46150, its param's type +0x1C and groups +0x38) the mod's while on:
  over the game's builder stood in, the spawner's first sender (groups 0-2) its own groups, a boss's sender in a
  script of its own (group 15) its own, the spawner's next (groups 1-4) with group 0 too and not 15, its field put
  back; a sender of a list of its own (type 0) as it is; off, the game's call and a sender's own groups (2026-11-14
  night); a sender's wait (2026-11-15): its test found where the build has it (0xD59611; the work's timer +0x38, the
  param's mWaitTimer +0x44, mMinEmNum +0x4C), its jbe a jmp while on - the game's own EnemyAdd frame (0xD59380) run
  in the copy over a made-up script (its works), work and param (2.0 s, at most 5; no count kind, nothing to send):
  on, a sender 1.5 s from its next sends at once (the send step's reload of its 2.0 s), at its minimum it neither
  sends nor runs its wait; off, the game's jbe, its wait run by the frame's time (1.5 - 0.5); a body's end (2026-11-15):
  the enemies' brains' end step found in uEmIntel's vtable (0x942B10) and its byte (+0x3B30); at the pause a body seen
  dead left as it is, on a body whose kill has counted (reported) ended - its byte set, once - and not an unreported
  one, a live one or a part of another enemy; the game's own end step run in the copy over a made-up brain
  (uEm1000Intel, mode 3) ends it, the brain's mRno and its own 3, and not a live one; off, a body left to its own
  death; a stage's countdown: the CountDown's read comes to the mod and reads on, the timer held (in the read), set at
  the pause and held, running down when off; the SmallTimer's read the mod's too, a gauge counting
  up left to run; the CountDown's timer counting up (stage 1003's) held, time given back taken up, no time-left editor
  for it; an FSM's own countdown (2026-11-13, stage 851's): its 32 timers' words read out of their loop (+0x930,
  running +0xA4, counting down +0xA8), the CountDown's in-place read rewritten into a call of the mod's (its bytes);
  over a made-up cFSMOrder and a made-up stage's uIdStageTimer (at the game's global 0x186E1F8), the game's own Timer
  order starting it (237 s, counting down), its own loop running it down and its own CountDown order showing it - off
  it runs down, on it holds (the display's number standing still), the panel's countdown is it, set at the pause and
  held, shown again after 30 frames unread on a fresh start held there (not at the old hold), off it runs down;
- Add and Throw away through the game's own set-slot, top-up, refresh and clear (run in the copy), Leon holding the
  Wing Shooter: 40 9mm rounds a stack, 150 more topping it up and a new stack, a stack thrown away, two herbs a slot
  each, 12-gauge shells (no shotgun held) not added and said so, a weapon (the shotgun) not added (2026-11-14: Add
  offers none), remote bombs whose archive is not loaded waiting for the loader (and dropped, noted, as the pause
  closes), nothing in play;
- the game's loader, its own request, commit and frame run in the copy over a made-up sLoadMan (the copy given what
  they call: MT Framework's allocators, InterlockedIncrement/Decrement, the CRT's formatter, sInfoMan, and an sResource
  whose create hands out made-up archives): the fields (9 lists, +0x30400/+0x3040C 0x14 apart, +0x304B0; an entry's
  path +4, archive +8 (flags +0x50), state +0xC; the characters' list 3, kind 7, `arc\DX9\`), the frame sLoadMan's slot
  6; Add offers no weapon, and each grenade and the spray it offers has a class (5); an Add of 2 flash grenades: its
  archive and its thrown form's asked for into list 3, the list chosen last (7) put back, nothing added yet ("Loading
  the Flash Grenade..."); the mod's runs of the loader's frame at the pause: the job makes the archives, loading; once
  in: the entries 3, the job done, then set-slot (a stand-in) for 2 flash grenades; nothing driven when nothing is
  asked for; a pick preloading the incendiary grenade with its thrown form (the game's request), its Add then at once;
  a weapon (the crossbow) neither preloaded nor added; two Adds of remote bombs waiting together, their archive asked
  for once, both made once it is in (2, then 3); a hand grenade and its thrown form; the pause closing while loading
  (the Add dropped, the frame left alone); nothing asked for while a list is being chosen, then asked (the spray);
  the loader not understood: a grenade nobody holds never, one Leon holds there;
- the records windows (records.cpp, `docs/records.md`): the four understood; each layout as the game's code has it
  (the profiles +0x38/+0x20/0xFDD8, the chapter records +0x1650, the Records grid +0x244 and percent +0x2F4, its page
  +0x48 / view +0x320 (the grid 1) / medal category +0x340 and mask +0x348; the nine medal categories' counts, lists,
  unused tables and names; the emblems +0xE1 and the game's achievement 0x562DD0; a clear's rewards; the Dog Tag
  screen's steps and rows, the title list's pages (a row's progress +0xC, 15 rows a page, the last page +0x250, the
  pages its buttons go to +0x254), the title states, counters and table, the title check's achievements and counter;
  the save's seven steps); then over a made-up sInfoMan at the game's global, sBioMain, a Steam achievement object (its
  ISteamUserStats' SetAchievement a stand-in) and a storage, with the screens as units on a line of a made-up
  sBioUnit (registered as the game's main system lists it; "sUnit" asked for by name finds it): a unit's start (slot
  5) and destruction (slot 0) as sUnit's line frame calls them, the screens' classes' own vtables hooked; no screen
  started, no window; the Records and Dog Tag screens started through the mod's hooks;
  each window's screen and view (opening and closing steps none, never in a game); a chapter cleared at a rank (the
  game's own gets read it; its time the game's "none"; the grid's copy and percent), "Gone to Hell" through the
  game's own achievement (ACH02, its emblem) and the template; its best time typed (2700 s: the game's own get
  0xE44250 - Chapter Select's - reads it, the rank kept, nothing given again; the view shows it and the most, 71999),
  one past the most held at it, one of 0 (0:00:00: the get reads 0, the view has it cleared in 0, not the "none"); a first clear with its time typed gets it, one without the "none" (in the batch
  that follows); three stories, then Ada's: the game's own story and
  whole-game tests (0x564030, 0x5640E0, run in the copy) agree with the mod's, the unlock flags and bits, the files,
  ACH22/ACH23 once; a clear taken back (its template kept); medal counts as the game's own get (0x565750) reads them
  (99 at most, no medal at an unused number), Survivors' team battle the shared list's second; two emblems through the
  game's call (Steam's ACH04/ACH26, the screen's row), and under the game's no-achievement state the emblem set by the
  mod with Steam untouched; titles: 255 families, Global Player to level 2 (NEW, the screen's row and next goal,
  counter 11; its counter brought up to level 2's goal, 50, and the row's progress with it), a lower level and no
  save, nine to level 4 ("Rising Up", then "Titular Achievement" at ten; each counter at its level-4 goal, counter 11
  the titles held, not raised); the save through the game's own steps (the profile committed, the storage asked,
  waited for, sInfoMan's afters, "Saved."); a title held with its counter short flagged, and its level asked for
  again bringing the counter only up (the state kept); a counter above a goal never lowered; a float family's (119)
  the goal as a float; the list's pages its buttons go to worked out again (0, 3, 7); a save asked for;
- the weapons window (records.cpp, `docs/records.md` "The weapons"): the WEAPONS page understood (page 1, the weapon
  table and its record's row, always-named byte, item and mode, Times Used and Enemies Defeated every 0x2C, names from
  3600, 43 rows) and the saved inventories (cGameInfo +0x20D8, 8 of 0x70; the hand, the slots, the 7 weapon slots,
  kept -2 / had -1; the weapon tables and counts); the ENEMIES page the enemies window, the WEAPONS page this one; 28
  rows that can be "? ? ?" and 15 always named, as the table has them; records not set up (zero): no character's
  ticks; then the game's own New Game reset (0x56CF30, over a made-up sGameMan) run for the four campaigns - Leon's at
  stage 202:
  Leon (and Helena) with the Shotgun and Semi-Auto SR had (-1), the Assault Rifle RN and Lightning Hawk not; all eight
  set up, 28 pairs, 24 not had, each one's row the game's own lookup (0x564670) gives; the page by the game's own reads
  (0x564750 and 0xE43C20): the Lightning Hawk and Grenade Launcher named (Times Used 1), the Incendiary Grenade's one
  use taken back, the Shotgun's 5 and the Assault Shotgun's use with a kill kept, the 909 (always) untouched, a save;
  Leon's Assault Rifle RN given and Shotgun taken out (as the game's getter 0xD70B20 finds them), an Assault Shotgun
  (not his campaign's) refused, Helena's Lightning Hawk given; the hand moved off the Shotgun to the Wing Shooter; the
  save run with the profile's cSystemInfo unchanged - the cGameInfo copied into the save's part (the Assault Rifle RN
  -2 there before, -1 after), the storage asked, run to the end; the Records screen closing: none;
- the enemies window (records.cpp, `docs/records.md` "The enemies"): the ENEMIES page understood (page 2, the enemy
  table and its record's row, always-named byte and no-Times-Defeated byte, Enemies Defeated +0x1328 and Times
  Defeated +0x132C every 8, names from 3700, 86 rows - the skills' kill table and counts the same); the page the
  window; 84 rows that can be "? ? ?" and 2 always named, each one's counts as the game's own gets (0xE43FE0,
  0xE44000) read them, Times Defeated where the page shows it; the page's own test (0x564860 and the gets):
  Glava-Begunats, the Crow (a count below 0) and The Witch "? ? ?", Ustanak named by its Times Defeated alone, Brzak
  by its kills, J'avo and Zombie always; a reveal asked with the WEAPONS page up dropped (nothing written, no save);
  the three named by their first kill (Enemies Defeated 1), Ustanak, Brzak and J'avo left as they were, rows past the
  page's ignored, a save run to the end; Reveal all as the window asks it: the other 79, the page's own test then
  naming all 86, no count lowered; the screen closing: none;
- the skills window (records.cpp, `docs/records.md` "The skills"): understood; the skill table (92 records of 20
  bytes and each field), the skill states +0x6FC (0x61; 3 bought), the requirements as the game's test reads them (1-4;
  only the campaigns' list has any), the skill screen's step, kind of list, user and rows (16 bytes: state +4,
  requirement +5, can be bought +6, price +8), the kill table and counts with the two kinds the game adds up, the
  weapon table and kills with the switch's seven groups; then the screen as a unit after the Dog Tag screen, its list
  filled by the game's own fill (0x9F24D0, run in the copy over profile 1): opening none, its list the window; 19
  skills in the list's order (J'avo Killer Lv. 1: 30 J'avo, 3,000 points; Handgun Master 1,500; the infinite ones the
  four campaigns); with kills and weapon kills written where the game's gets read them and three campaigns complete,
  each skill's count and met equal to the game's own counts (0x5666E0, 0x5668D0) and its "can be bought" test
  (0x566AB0) - 4 met; three unlocked (state 2) and Zombie Hunter (met) left alone, nothing else in the profile
  changed (the points neither), the screen's rows byte for byte as the game's own fill makes them from the profile
  now, the game's own "changed?" (0xE43150) seeing it (the screen's close saves it), no save of the mod's; J'avo
  Killer locked again (its row "? ? ?"), a met one and a bought one kept; Lv. 2 (a requirement of 1) not this
  window's; the Mercenaries' list, a user past the two profiles: none; in a game (a chapter's end): the window; the
  screen closing: none, a change asked then dropped; the skill screen started, and destroyed, through the mod's hooks
  (no window at once);
- the Mercenaries window (records.cpp, `docs/records.md` "The Mercenaries' unlocks"; 2026-11-15): understood - the
  stages +0x970 (always open 0x1C07, the profile's 0xFE3F8: the getter's chain walked), ten rows a mode from bits 0
  and 10, the stage table; the characters +0x978 (always 0x20010101, No Mercy's four 0x3C0000000 by sGameMan +0xB4 bit
  0; the profile's 0x1FFEFEFE); the stage select's panel fields and pages, the character tables; the character
  select's copy, rows, cursors, counts and lists, its 13 characters' tables; the Collection file of each costume (the
  seven Costume 1 awards' own pushes, the online event's 14 pairs). Over a made-up sGameMan and sId at the game's
  globals: the mod's stages open and characters available equal to the game's own getters (0x563A00, 0x563AF0) for
  six profiles each - the Mercenaries, No Mercy with and without its flag, the campaigns; the stage select (its panel
  filled by the game's own fill, 0xA0FED0) started through the mod's hook - opening none, up in the Mercenaries the
  window: its ten stages in the stage select's order (s1202 Urban Chaos ... s1208), the first three always, the nine
  characters (Default and Costumes 1-3, Carla, the Agent; the always ones), each as the getters say; changes (a
  stage opened, one closed, No Mercy's opened, an always one and Leon's Default and Coach refused; Leon's Costume 2
  (had), Helena's unlocked, Chris' Costume 1 locked): the profile's bits, the files 63 and 64 given (none for the lock),
  the view and the game's getter agreeing, the note, the panel as the game's own fill makes it now, the save asked
  at once (the change in the saved copy) and run to the end; another mode: none, a change dropped; No Mercy's
  character select made as its constructor makes it and started by the game's own start (0xA10090, its per-frame
  update cut off): 8 rows, its window (No Mercy's stages from bit 10); Helena, Chris' Costume 2 and Piers' Costume 3
  unlocked: its copy, rows and lists byte for byte as the game's own start makes them now (10 rows), the cursor kept on
  its row and Chris' costume cursor on his Costume 1, files 65 and 73; Piers' only costume locked: his row gone, the
  cursor held to the rows; closing none; in a game never; destroyed through the hook: none at once;
- unload: the game's subtract-health back in the vtables.

## tests/test_panel.cpp - the panel and the records windows, headless (147 checks, 2026-11-15)

The real `overlay.cpp` with ImGui's test-engine hooks (each widget's label and place), driven by clicks; `shots`
writes a picture of each step. ImGui is built with its own asserts (not the mod's logged checks,
`src/imconfig_re6cc.h`): a check the panel fails stops the test.
- the pause: every cheat (Infinite stamina and Infinite air too), each click flipping its switch and a second
  putting it back; the Cheats section, and no Characters section; the skill points' editor - Set asks for the
  points shown, then for those typed, and the field follows the game again;
- each character's inventory: a field for each count (each mode's: the Wing Shooter's two, the launcher's three), the
  modes named by what they load - the Wing Shooter's by the game's own names, "One Gun" and "Two Guns" - and an
  unknown item by its number, Apply (its slot and id), Undo, Apply all, a count held at its most; x and Throw away
  for the case's items only (not a weapon, not the one in hand); Add (2026-11-14): no Weapons tab; the Ammo tab only
  what Leon's weapons load (9mm, the launcher's three rounds - not 12-gauge shells, not the arrows of a crossbow he is
  to pick up), a search ("grenade": the grenades, not the Grenade Launcher; "shells": none), 25 rounds asked for, a
  stale pick of ammo no weapon of his loads not added; a grenade the stage has not loaded preloaded as it is picked
  (once) and added (3), its "Loading ..." note drawn, one the game's loader cannot load greyed (not added, not
  preloaded); Helena (no gun) offered no ammo, her grenades there; a character played online shown only;
- Automatic QTEs, Steady aim, Freeze countdown timer (its Time left editor only with a countdown up), the Results'
  switches and editors (the clock's Set, a counter's Set for the character's index) in a campaign; the Mercenaries'
  section and its three switches in a match only (no Results there);
- a switch whose hook was not found: drawn greyed, a click does not flip it;
- in play with F7: the panel over the game, the editor's Set, an inventory count's Apply and Throw away disabled; F7
  again closes it;
- out of a game: nothing drawn, F7 does not open it;
- the records windows (a made-up records view): the Records chapter grid's window and never the cheat panel there,
  a tab for each character with its records held; a rank picked as a player picks it (a cell's combo opened, A
  clicked: the frame of the pick pops no colour it did not push); a cleared chapter's best time under its rank
  (1:23:45), none under one not cleared; Apply all asks for only the real changes (S for Normal 1, E for No Hope 5),
  their times left as they are, and lets them go; a time typed as a player types it (the field clicked - its text
  selected - and 45:00 typed over it) asked for by Apply all (2700 s, its rank B kept); one that is no time (1:75)
  nothing to apply, let go as the field is left (Enter: the record's time again); "Everyone's" with "-" takes the
  clears back; S with 30:00 typed beside it for Leon's: his 25 chapters at S in 1800 s - applied (the view as the
  tick leaves it), his fields show 0:30:00, nothing waiting; everyone's at S in 0:00:00, applied: the fields show
  0:00:00 (a first clear's too), not 19:59:59 (2026-11-14's play-test; the check fails on the code before); a time
  of 1234.7 s shown 0:20:34, cut as Chapter Select cuts it; F7 hides the window and
  shows it again, and a screen change brings it back; the medals: the categories the game shows, on the game's own, a
  count typed as a player types it (the field clicked, a digit typed - the game ended there, 2026-11-06), its Undo, a
  count typed and applied, "Type in" for every medal listed then Apply all; the emblems: a tick for each (the
  unlocked greyed), Award for the first locked, Tick all locked and Award the 48; the titles: Next, All and a tick box
  only for a family not complete (none for one held at every level), no "Bring their counts up" line until a title
  held has its count short, then the line asking for its held level again; the screen closed: nothing drawn;
- the skills window (a made-up view: one locked, one whose requirement is met, one unlocked, one bought): its window
  and never the cheat panel, a tick for each; the locked one's tick asks to unlock it, the unlocked one's to lock it
  again, the met one's and the bought one's fixed (nothing asked); Unlock all and Lock all ask for just what they can
  change; Unlock all with its count (2); F7 hides it and shows it again; the screen closed: nothing drawn;
- the weapons window (a made-up view: Leon with two of his four, Helena none, Jake without his Shotgun, the Agent not
  set up; seven page rows, the Shotgun's used in play): its window and never the cheat panel (Give every weapon 7,
  Name every row 4); a tick only where a character's campaign has the weapon, a page tick for each row; All for each
  character set up and each weapon; Helena's Shotgun given, Leon's taken out; the Agent's tick and the played row's
  page tick fixed; a "? ? ?" row named, a one-use row taken back; Helena's All her four; the Shotgun's All Helena's and
  Jake's only; Give every weapon the 7 (not the Agent's); Name every row the 4; F7 hides it and shows it again; the
  page closed: nothing drawn;
- the enemies window (a made-up view: Neo-Umbrella J'avo with kills, Ustanak named by its Times Defeated alone,
  Glava-Begunats, the Crow - no Times Defeated shown - and The Witch "? ? ?"): its window and never the cheat panel
  (Reveal all 3, the "? ? ?" filter), a tick for each row; a named row's tick fixed (nothing asked), a "? ? ?" row's
  reveals it, shown named until the game says so (Reveal all 2) and then fixed; Reveal all asks for just the three;
  "? ? ?" only lists the three, their ticks as before, off all five; F7 hides it and shows it again; the page closed:
  nothing drawn;
- the Mercenaries window (a made-up view: the Mercenaries' ten stages, six open, three always; the nine characters as
  the user's save held them on 2026-11-15 - every Default and Costume 1, Carla, the Agent): its window and never the
  cheat panel (Open every stage 4, Unlock every costume 14), a tick for each stage and costume; the always ones' ticks
  fixed (nothing asked); a closed stage's tick opens it, an open one's closes it, Leon's Costume 2 unlocked, Helena's
  Costume 1 locked again, each shown as clicked until the game says so; Leon's All his two locked; Unlock every
  costume the 14 (each Costumes 2 and 3); Open every stage the 4; No Mercy's ticks its own bits (its Liquid Fire 17);
  F7 hides it and shows it again; the screen closed: nothing drawn.

## tests/test_boot.cpp - the DLL as the game loads it (18 checks, 2026-11-06)

A stand-in exe importing what BH6.exe imports (steam_api's SteamAPI_IsSteamRunning, d3d9's Direct3DCreate9, kernel32's
SetUnhandledExceptionFilter and ExitProcess, user32's GetAsyncKeyState, dinput8's DirectInput8Create), run beside a
copy of build/steam_api.dll, the game's own steam_api.dll as steam_api_orig.dll, and `tests/fake_d3d9.cpp` as d3d9.dll
(another tool's, standing in for ReShade: it marks that it was called and passes on to the system's), with
`WINEDLLOVERRIDES=d3d9=n,b`:
- the five import slots point into the mod before the program's own code runs (DllMain);
- the mod's SteamAPI_IsSteamRunning is a jump into the original's, and answers;
- GetAsyncKeyState answers through the guard; the program's own exception filter is chained under the mod's, which
  stays on top (kernel32's asked straight);
- Direct3DCreate9 goes through the mod to the other tool's d3d9.dll (and on to the system's);
- the C runtime's abort handler (SIGABRT: the program and the mod share ucrtbase) is the mod's, and a raised SIGABRT
  leaves `the game ends: abort() in the mod's C runtime ...` in the log, with the stack;
- the log: loaded as the steam_api.dll proxy, forwarding 55 Steam API functions to the game's own DLL (recognised as
  it), the slots re-pointed, Direct3DCreate9 hooked leading to d3d9.dll, the program not the game (no discovery), no
  D3D object without a display (logged);
- the ini written with every switch, and the memory log and the frame log off (`MemoryLog = 0`, `FrameLog = 0`).

## tests/test_memlog.cpp - the memory log and the frame log (32 checks, 2026-11-13)

The real `memlog.cpp`, `framelog.cpp`, `config.cpp` and `log.cpp` built in, run in the scratch prefix with an empty dir:
- a made-up 32-bit mapping list: each kind (Wine's reserve with no access apart from anon, the game's files, the
  GPU's mappings, the NVIDIA libraries, Wine's, the prefix's DLLs, Steam's, shared, other files, [heap], [stack]),
  430 MB mapped, the gaps (3665 MB not mapped, the largest 3180 MB), all of the 4 GB accounted for, a last line
  without its line end;
- a 64-bit list (Proton's new WoW64): the part above 4 GB apart, a mapping across 4 GB split, the low part's gaps;
- the live list: found by the process's pid (not /proc/self - Wine's server opens the files, so "self" is the
  server), every mapping in a kind; the Windows view (committed images and private memory, the largest free block);
- the mod's heap: a 1 MB vector counted on and off, ImGui's allocator the same;
- the ini: a first start writes `MemoryLog = 0` (documented), 2 is taken as 5 s, 30 read and logged;
- the thread: the run before's dumps kept as `.prev.txt`, a line at start (Direct3D's not asked yet), the present
  hook's texture memory (a made-up device: 1536 MB), a line at a game's start (and its mappings dumped) and end;
  the crash: a last line and the mappings under their own heading;
- the frame log (2026-11-13): the ini writes `FrameLog = 0` (documented), `FrameLog = 1` read and logged; made-up
  frames through it - a 16 ms frame with a Present inside the game's frame (taken out of the game's time), a 164 ms
  one with its Present after it: the long one's own line split into the game's frame, Present, the mod and the rest,
  and naming the screen though the text tick was handed (a buffer of the caller's, as dispatch's) is overwritten by
  then (the long-frame lines printed garbage until 2026-11-13's fix);
  no line before a second; the line a second - 2 frames, the average and the longest, those over 33 and 100 ms, the
  game's frame and Present (average, longest), the draw thread's scenes and longest gap, the mod's own time a frame,
  what was made (1 texture, 3 shaders, with their time); the counts start again each second.

## tests/test_crash.cpp - the crash filter's chain (26 checks, 2026-11-10)

`src/crash.cpp` built in (the mod's own -O2; `image::text`, `memlog::on_crash` and `logf` stood in for), its filter
called as UnhandledExceptionFilter calls the top-level one, with a made-up access violation (`docs/engine.md`, the
crash filter's chain):
- installed, and the game's filter set through the hook: the mod's stays on top;
- a crash: one dump - its kind, what it touched, every register, the top of the stack raw (six rows), the code
  addresses up it - the memory log's crash line, and it is passed on to the game's filter, named;
- the same exception again: a line, not a second dump nor a second memory log, still passed on;
- a filter set over the mod's that chains to the one it replaced (the mod's), the reassert chaining to it: a crash is
  stopped as it comes back with the same exception; the filter putting itself back again is not logged again;
- a "previous filter" that is no code (0x3EA): not chained to;
- two threads, one inside the chain and the other crashing meanwhile: each its own dump, each stopped on its own
  thread;
- a new exception raised inside the filter passed to: passed on too, with its answer back; one raised every time:
  stopped three deep;
- SetUnhandledExceptionFilter made to do nothing (`ret 4` over its first bytes): said once, nothing taken for a
  filter, no line each second; neither the game's call through the hook nor install claimed to put the mod's on top;
  install still takes abort.

The build before, under the same checks: the chain-back went round 6170 times and the process died; beside the same
`ret 4` (a scratch program) it logged "had been replaced (000003e8) - ours is back on top, chaining to it" at every
reassert.

## Not tested offline

The game itself: the D3D9 device and the panel on its frames, the pause as the game sets it, the hooks' effect in
play. A headless run to the title would prove the proxy, the boot, the discovery's log lines and the hooks; it needs
the user's word (`../RE5CabbyCodes/docs/testing.md`, "Testing without playing").
