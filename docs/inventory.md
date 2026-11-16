# The inventory: its items, their names, the game's own functions

Addresses are Steam build 10112070's; the mod finds each by signature (`src/sigs.h`). What only play can confirm is
marked [I].

## The items and their names

- An item's id is category << 8 | number: 0x100-0x128 weapons and slot items, 0x201-0x20E ammo, 0x301-0x307 herbs
  (0x301/0x302 are the floor's: picked up as 0x303/0x304), 0x400-0x40B skill-point pickups, 0x500-0x525 key items.
  Only categories 1-3 go into the 24 inventory slots.
- The names: `nativePC/arc/DX9/Msg_<lang>.arc`, file `soft\message\mes_item_<lang>` (a GMD; MT Framework 2 ARC v7,
  zlib). An item's name is message (category - 1) * 100 + number (0x4E85E0; the item-get banner 0x9A7D30 the same).
  `tools/gen_items.py` reads them and writes `src/items.h` (62 items, 24 that Add offers since 2026-11-14: no
  weapon): the kind the Add list files each under, a case item's most a slot, what each weapon's modes load, and what
  each count of a weapon that keeps a count for each mode is called.
- Modes (0x575010): 3 for 0x112, 2 for 0x103, 0x10B, 0x10D, 0x10E, 0x110, 0x116, 0x117, 0x128. Only 0x103 (the Wing
  Shooter: 18 / 36), 0x110 (the Bear Commander: rifle / M203), 0x112 (the grenade launcher: explosive / acid /
  nitrogen rounds) and 0x117 (the crossbow: normal / pipe bomb arrows) keep a count for each (0x574B80 packs their
  mosts a byte a mode: the Wing Shooter's record says 36, mode 0 gets half).
- **The modes' names** (2026-11-14): the inventory screen's Mode Change (its lookup 0xA251F0 -> 0x9AFDE0) reads a
  table at 0x159AE48 - 17 records of 16 bytes {weapon, mode, an icon, a mes_system message} - for the Triple Shot
  (both copies), MP-AF, Bear Commander, crossbow, Anti-Materiel Rifle, Wing Shooter and grenade launcher: mes_system
  1666 "Two Guns", 1667 "One Gun", 1668/1669 "Normal Scope"/"Thermal Scope", 1670 "3-Round Bursts", 1671 "Semi-Auto",
  1672 "Auto", 1673-1675 the 40mm rounds, 1676/1677 the arrows. **The Wing Shooter's mode 0 is One Gun (18), mode 1
  Two Guns (36)** - two of them held at once; each mode keeps its own count, so the panel lists it twice (the user,
  2026-11-14: "why does Leon have 2 Wing Shooters in his inventory? Is that a bug?" - no: one gun, a count a mode).
  The panel names those two counts "One Gun" / "Two Guns"; the others by what they load. A saved inventory packs such
  a weapon's counts a byte a mode too (the user's Leon: 0x1212, 18 and 18).
- **What each weapon loads** (0x574E90: (weapon, mode) -> the ammo; the tablets' herbs 0x303-0x307 besides): 9mm
  0x201 the 909, Picador, Wing Shooter, Ammo Box 50, Triple Shot (both), MP-AF; 12-gauge 0x202 the Shotgun and
  Assault Shotgun; 10-gauge 0x20E the Hydra; 0x206 the Lightning Hawk; 0x207 the Elephant Killer; 7.62mm 0x203 the
  two sniper rifles; 12.7mm 0x205 the Anti-Materiel Rifle; 5.56mm 0x204 the AR for Special Tactics, Assault Rifle RN
  and the Bear Commander's mode 0 (its mode 1 the 40mm explosive 0x208); the grenade launcher 0x208-0x20A by mode; the
  crossbow 0x20C/0x20D; the rocket launcher 0x20B. Knives, the Stun Rod, the fists and the scenes' weapons none.
  Transcribed into items.h (`ammo`); Add offers a character only what its own weapons load.
- A slot's most (0x574B80): a weapon's magazine (the first int of its 0x190-byte record, 0x180D0B0 + n*0x190, 48 of
  them); ammo 30-150; grenades 5, the spray 3, a herb 1, the tablets 12.

## The slots

uPlayer +0x46E0, 24 cItems of 0x1C (+4 in use, +5 mSlotNo, +6 mId, +8 mModeNo, +9 mModeMax, +0xA mNum[3], +0x10
mMax[3], +0x18 mpUnit); +0x46D8 the slot in hand. Every lookup goes by the slot number (an item's place in the array
is not its slot; the inventory screen swaps the numbers):
- slots 0-6 the weapons (a count of -2 in a set: kept for a weapon to be picked up later - its unit made, not in use);
- slot 7 the tablet case (0x11D);
- slots 8-12 the summary slots of the grenades (0x113-0x116) and the spray (0x120), each the total of its stacks,
  worked out by the refresh 0xA52780 (which also sets profile unlock bits for a local player holding grenades, as every
  pickup does);
- slots 13-23 the case (the top-up fills 13-23; new stacks go into 15-23 [I: what 13-14 hold]).
The panel lists the weapons, the tablets and the case, not the summary slots.

## The game's own functions (the approved kind: the inventory's own)

- set-slot 0xA51D90 (thiscall on the player: slot or -1, id, count (-1 a weapon's own), mode, unit or 0; the slot back
  or -1): a weapon first takes a slot kept for it (its unit with it); otherwise find-free-slot 0xA43BA0 (weapons 0-6,
  the rest 15-23); a category-1 id gets a unit made at once (sItem 0x575C70 with kind 7, `kSigItemMake`: the class
  keyed (7 << 16 | id) << 8, on sUnit line 13) - which fetches its resources as it starts, and quits the game
  (0xEE1940, "Failed open file") when its archive is not loaded. It does not clamp the count.
- top-up 0xA44080 (id, count; what did not fit back), refresh 0xA52780, cItem clear 0xD7F0D0 (its unit let go, the
  fields zeroed) - the game's own discard (0xA25F30) is clear + refresh.

## The loader (sLoadMan) - what a stage has loaded, and loading more (`inventory.cpp`) - built 2026-11-05

The user (2026-11-05): "I previously told you that I want the ability to add any weapon to inventory, but it's still
telling me that the game would quit out on some models. You should fix the game to be able to handle these adds
instead". Until then Add offered a weapon, grenade or spray only when someone in play held one. Since 2026-11-14 Add
offers no weapon (the Records WEAPONS window gives them): the loader serves the grenades and the spray.

- **An item's archive**: an item's unit class is keyed (7 << 16 | id) << 8 - 0x101 the 909 `uWp1010`, 0x104 the
  shotgun `uWp1020`, 0x112 the launcher `uWp1080`, 0x113-0x116 the grenades `uWp1090/1092/1094/1096` (variant 1 their
  thrown forms, `uWp1091`...), 0x117 the crossbow `uWp1100`, 0x11B the rocket launcher `uWp1120`, 0x120 the spray
  `uWp1173` (a `uWpLuggage`); ammo `uIt0200`+ (category 2: no unit). Each class's model, textures, sounds, effects and
  the players' motions for it are in `nativePC/arc/DX9/<class>.arc` (163 `uWp*`, 28 `uIt*`). The motions are the shared
  player skeleton's (`soft\chara\pl\pl0000\motsoft\pl0000BhA` in the 909's, `pl0000ArA` in the crossbow's, ...): every
  character uses pl0000's, so any character can hold any weapon once its archive is in.
- **sLoadMan** (0x304C0 bytes; the global 0x17D0218; sBioMain +0x10208): **9 lists** of archive entries (MtArrays at
  +0x303FC + k x 0x14: count +4 = +0x30400, items +0x10 = +0x3040C), the one added to at +0x304B0. An entry (0x18 bytes,
  vtable 0x15228F0): +4 the path (an MtString - refcount, length, characters from +8: `arc\DX9\<class>`), +8 the archive
  (an rArchive; its flags +0x50: bit 0 loaded, bit 4 failed), +0xC the state (1 or 4 asked for, 2 loading, 3 loaded; 0
  once its list is chosen again), +0x10 the class record, +0x14 -1.
- **The request** 0x58AD50 (thiscall (kind, number, variant, flag), ret 0x10; `kSigLoadRequest`): the class by its key
  (0xE67750: the 256 hash buckets 0x18145C8 MtDTI::MtDTI fills); not 27 `uSm90xx` sets, and only a `uPl/uEm/uSm/uWp/uIt`
  name with digits after the prefix; then the add 0x589D80 (`kSigLoadAdd`: nothing if any of the 9 lists holds that path
  - 0x589C60 makes such an entry 3 again if it was 0 - else a new entry in state 1 in the list +0x304B0 names), then
  what the class needs with it, kind 7: the Hydra 10-gauge shells (0x20E), the Lightning Hawk and Elephant Killer their
  magnum rounds (0x206, 0x207), the Anti-Materiel Rifle 0x205, the AR for Special Tactics 0x185 (`uWp1331`), the Bear
  Commander 0x133 + 0x208, the grenade launcher its three shells 0x133-0x135 and rounds 0x208-0x20A, a grenade its thrown
  form, the crossbow its two arrows' shells 0x131/0x132 and arrows 0x20C/0x20D, the rocket launcher 0x136 + 0x20B. All
  51 archives the 28 offered items need are on disk (checked 2026-11-05).
- **The commit** 0x589250 (thiscall, no arguments; `kSigLoadCommit`, `kSigLoadJob`): the chosen list's entries in state
  0 let go (archive released, entry deleted); when any is 1 or 4, the list's job (+0x30040 + k x 0x54) set: +0x40..+0x50
  zeroed, +0x48 = 1 (asked for). **The choice** 0x588C30 (`kSigLoadChoose`): +0x304B0 = k, every entry of k state 0.
- **The frame** (sLoadMan's vtable slot 6, 0x589730; `kSigLoadFrame`): +0x20 its state - 0 idle (takes up every list
  job asked for), 1 three frames, 3, 4 the jobs' steps; a list job (0x588950, `kSigLoadPoll`) makes each entry in state
  1 or 4 an archive (`sResource::create(rArchive, path, 2)` - async: sResource's loading thread 0xF2D210 does the work),
  state 2, and polls it to 3 (bit 0; bit 4 failed: made again - forever, so a missing file would never finish). The busy
  tests 0x588BF0/0x588B60 (+0x20, the jobs, sResource's queue) are what a stage's load waits on.
- **The paused frame does not run it**: sBioMain's frame (0x511D70) calls every system's slot 6 in play; paused it
  calls 0x511950, whose list leaves sLoadMan (+0x10208) out. RE5's ran paused or not.
- **The lists' roles** (their choosers): 0 the core (0x589EA0), 1 the title and menus (0x589FE0, 0x589FC0 the trial),
  2 the game's (0x58D920: the UI, the mode's UI, the common items 0x58CD10 - tablets, the spray, ammo 0x201-0x204, the
  floor's herbs, skill points - and PlayerBase), **3 the characters'** (0x4DD5C0, `kSigCharsLoad` - also the menus'
  characters 0x58A590), 4 the enemies (0x4E5BE0), 5 the stage's sets and objects (0x4DE090, then the item sets
  0x4DE1C0), 6 the stage (0x58CE90: `s%04dBase/Scr/Etc/Soft/Stq_v%s`), 7 an event (0x58D4C0, `e%04d`) or the figures
  (0x58D830), 8 the stage's demo (0x58D380). A stage's load (0x4E6180's step) runs 6, 3, 5, 4 in that order, each
  chosen, asked and committed; the whole of a list is let go (0x588C80) only out of play (the start-up screens, the
  title, quitting). List 3's load asks for each present player, their gadget (0x11F) and every category-1 id of each
  one's record (0xD70B40: 8 records of 0x70, the items at +0x10 as id | mode << 12, count) - by kind 7, the one set-slot
  makes an item by (`kSigCharsItems`). The records are what a player is made from at a stage's start (0x5936D0: set-slot
  for each); how the game fills them and carries an inventory over is "Carried over" below.
- **The mod** (`inventory.cpp`, the user's word above): an item's archive is "here" when some list's entry for it is
  loaded (flags bit 0, not bit 4) - read as the pause opens, every half second while it is up, and every frame while
  something asked for is loading. One that is not: the game's own request for the item (kind 7, number, 0, 0) with
  the list added to set to the characters' (3) - the list chosen last put back after - then one commit; never while a
  list has an entry in state 0 (the game choosing a list: a commit would let it go). The loader's frame is run by the
  mod after the game's (`dispatch.cpp` hk_frame -> `inventory::after_frame`) while the game stays paused and something
  it asked for is not in (at most 20 s from the last ask). An Add waits (the panel: "Loading the Shotgun...") until its
  archive is loaded and no list's entry is still loading (the companions too), then goes in by set-slot; dropped if the
  pause closes first ("...the pause closed while it was loading") or after 20 s. **Preloading** (the "no waiting on
  the mod" rule): an Add list's pick asks for its archive at once, so its Add usually acts at once - not past 4 picks
  no one holds at a time (each weapon's archives are 0.4-4.3 MB on disk; the 32-bit game ran out of memory once), the
  rest load on Add. What the mod loads stays in list 3 until the next stage's load, which keeps what the characters
  then carry and lets the rest go. Discovery checks every field against each other function's (the commit's, the
  choice's, the job's, the frame's, the add's, the poll's), the frame against sLoadMan's slot 6, the characters' load
  against the choice and the request, set-slot's kind against the characters' load's; with the loader not understood
  Add falls back to what someone in play holds (logged).

## Carried over: a character's record, checkpoints, Chapter Select (traced 2026-11-14)

The user (2026-11-14): "how does the game track weapons? Do you keep it once you've found it for the next time you do
chapter select missions?" Read from the code; nothing here is seen in play yet [I].

- **Each character's weapons** (a table each, 0x17948FC[c], how many 0x179491C[c]; 6 bytes: +0 the stage the weapon
  is found in (-1: from the start), +2 its chapter (0-4), +4 the id) are fixed, 5 or 6 each, and each has a weapon
  slot from the start (the default records 0x1794D98[c]: in hand +0, the 24 slots from +4): count -2 there (kept, not
  had) until the weapon is found. A partner finds their own (Leon's and Helena's tables list the same pickups).
  Leon: Wing Shooter, Survival Knife; Shotgun (s103, ch 1), Semi-Auto Sniper Rifle (s202, 2), Assault Rifle RN
  (s206, 3), Lightning Hawk (s510, 4). Helena: Hydra, Picador, then Leon's four. Chris: Combat Knife, AR for Special
  Tactics, 909; Assault Shotgun (s301, 2), Grenade Launcher (s507, 3), Semi-Auto Sniper Rifle (s801, 4). Piers:
  Anti-Materiel Rifle, MP-AF, then Chris' three. Jake: Hand-To-Hand, 909; Elephant Killer (s307, 1), Sniper Rifle
  (s402, 2), Bear Commander (s602, 3), Shotgun (s510, 4). Sherry: Stun Rod, Triple Shot, then Jake's four. Ada:
  Crossbow, Ammo Box 50; Assault Shotgun (s1003, 1), Sniper Rifle (s203, 2), Bear Commander (s871, 4). The Agent: 909,
  Survival Knife, then Ada's three. Grenades go by pairs (0x17420E8: hand Chris/Piers/Ada/Agent, incendiary
  Leon/Helena/Ada/Agent, flash Chris/Piers/Jake/Sherry, remote bomb Leon/Helena/Jake/Sherry).
- **Three records of each character in a profile's cGameInfo** (0x70 bytes, the layout above; 0x5936D0 reads +0 the
  model, +1 the costume, +2/+4 health and its most, +7 the slot in hand, +0x10 the slots): +0x20D8 + c * 0x70 (0xD70B20,
  inside mTempData: what the save holds - the character's carried inventory), +0x35E4 (0xD70B40: the stage's, a player
  is made from it), +0x3964 (0xD70B60: an online partner's, from the network, 0x518BC0). One record a character, not a
  campaign or a difficulty.
- **Written back** at each checkpoint (aGame's 0x468110, campaign only) and a chapter's end (0x470240 step 4, 0x471380
  step 3): 0x56D070 has 0xD71690 write each present player's live slots into the stage record (a slot not in use as
  -2; health at least 150), then copies it into the carried one for the story's two characters - whole, or only health
  and two words on stages 550, 551, 600, 751 and Ada's 706 (Leon's 209 and Chris' 973: the first character whole).
- **A chapter's end** (0x471380 step 3, 0x56D2F0): each of the two who played it from its start (their block's +0x20,
  set when mChapterStart, +0x1B08, was set: 0xD70730) gets every weapon of that chapter (the table's +2) still at -2
  as -1 (had, magazine full), and the results' "new" mark (sGameMan +0xBF8 + (c & 1) * 0x46): a weapon walked past is
  given at the chapter's end.
- **A stage's load** (0x4DD2D0, the first step of 0x4E6180's, campaign only): 0x5676F0 makes each stage record from
  the carried one - a player's own profile's (a split-screen player's pushed in use), an online partner's +0x3964; a
  character nobody plays (the AI partner) is rebuilt from its default with every table weapon whose stage comes at or
  before this one in the story's order (0x1794230[c / 2], 24-byte entries, +0 the stage) - then 0x5675E0 (stages 100
  on) clears from slots 0-7 any weapon that is not on that character's table (the tablets kept) and, from slots 15-23,
  a grenade not that character's. Then 0x56D490 puts in a stage's own loadout, in the stage record only - for players:
  Helena carrying Deborah (0x127) in s209, Chris a Combat Knife and Piers the Anti-Materiel Rifle in s550, Jake his
  fists and Sherry the Triple Shot in s551 (s600: both fists, Sherry the Stun Rod), Ada and the Agent a Survival Knife
  in s751 and s706, Piers the Mutated Hand (0x126) in s973; the rocket launcher kept for Leon and Helena in s773; the
  rest (s303, s508, s512, s801, s871 ...) are characters met as others' NPCs. Those players' checkpoints there write
  back health only (above), so the carried inventory comes back after.
- **The campaign menu** (uIdStartUp10; sGameMan +0x50 its choice, the help texts 0x1766FA8: mes_system 672-677, 980):
  0 Continue (a game in progress, mNewGame 1: 0xD27B20) or New Game (0xD27690), 1 Chapter Select (0xD28050 in
  0xD28D80), 2 Join, 3 Skills. **Chapter Select** (uIdStartUp1c the campaign, 15 the chapter - its OK sets the start,
  0x566EB0 - then 12, 1b, 14, 1e) resets nothing: each character starts the chapter with its carried record, health
  filled (+2 = +4) - every weapon found, in any chapter, on any difficulty, with its ammo and the case. **New Game**:
  the campaign screen (uIdStartUp1c, 0xA0A690) asks, for a campaign complete (0x5640A0) and not from Chapter Select
  (+0x174), "Start a new game without the weapons you've obtained?" (mes_system 686); Yes sets sGameMan +0xC4, and the
  start (0xD27690 step 10) then runs 0x56CF30: the campaign's two carried records made the default with the weapons
  due by the start stage. No (or a campaign not complete) keeps them. A game start (0x561B70) resets a carried record
  to the default only when it lacks one of the default's slot 0-7 items (a new profile).
- **The mod's Add** (no weapon since 2026-11-14): a grenade of another pair is written back at the next checkpoint
  but cleared at the next stage's load (a new area, a Continue, Chapter Select): it lasts until the stage changes. (A
  weapon the Add put in until then: one on the character's table took its kept slot and was carried like a found
  one; another character's, or the rocket launcher outside s773, lasted until the stage changed.)
- **The mod's weapons window** (beside the Records screen's WEAPONS page, `docs/records.md` "The weapons"): each
  character's weapons found on the way, put in its saved inventory (kept -> had) or taken out, the profile then saved.

## Add, Throw away, the counts (inventory.cpp)

- Counts: written into the cItem at the pause (up to its most), the item still the one typed for; a case item's
  change refreshes the summary.
- Throw away: a case item (slot 13 on) not in hand - cleared, the summary refreshed.
- Add: ammo, grenades, the spray and herbs - a stack onto the stacks there are (top-up), the rest a slot's most at a
  time; the summary refreshed. Ammo only of a kind a weapon the character holds in use loads (above; the panel lists
  only those, and the tick checks the live inventory again: "Nothing Leon holds takes 12-Gauge Shells."). A grenade or
  spray whose archive the stage has not loaded is loaded first (the loader, above). Never the tablets (0x11D: a second
  case would go into a weapon slot), and **no weapon** (2026-11-14, the user: "Remove the weapons altogether as they
  can now do that through the weapons imgui panel instead, and only allow ammo for the weapons the player currently
  has" - the Records WEAPONS window gives a character's campaign weapons to its saved inventory). Built 2026-11-05 to
  2026-11-14, a weapon went in through set-slot (-1, id, -1): its kept slot or a free one, loaded (the user's last
  try, 19:11 that day: the Crossbow for Leon - no free weapon slot).
- Add and Throw away are offline only: none of it is sent to an online partner (a pickup's network message, 0x526790,
  is not made).
