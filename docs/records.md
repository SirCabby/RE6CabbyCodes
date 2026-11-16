# The records: chapters, medals, weapons, enemies, emblems, titles, the skills, the Mercenaries' unlocks (the windows beside the Records, Dog Tag, skill and Mercenaries select screens)

Addresses are Steam build 10112070's; `src/records.cpp` finds every one by the signatures in `src/sigs.h` (the
"records windows" and "skills window" groups and the `RE6CC_*` patterns checked at the call targets they lead to).
The texts come from `tools/gen_records.py` (`src/records_text.h`).

## The profile and its two copies

- sInfoMan (global 0x17CF49C; the mod takes it from sBioMain's systems) holds two profiles, cSystemInfo (0xFDD8 bytes)
  from +0x38, the one in use by its dword +0x20. Everything below is counted from the profile's start.
- A profile is two halves: the working copy (+8, 0x7EE8 bytes: what the game reads and changes) and the saved copy
  (+0x7EF0). The menus' close commits the first into the second (0xE43110, through sInfoMan's 0x5687F0, which first
  gives the skills their unlocks) and the save writes it; a load and the start of a game or mode take the working copy
  back from the saved one (0xE43130: 0xD25EB0, 0xD27B20, 0xD28A50 ... reached from the campaign's and the modes'
  starts). So a change only in the working copy is lost at the next game unless a save comes first.
- The menus' save as they close (uIdCBase's 0x9756C0, kSigProfileSave), a step a frame: 0) the profile changed
  (0xE43150 compares the halves)? - commit (0x5687F0), the profile's cGameInfo (sInfoMan +0x3F798 + 0x3CE4 * it)
  copied to its own saved copy (0xD702A0), and unless sStorage (global 0x17D0564, a class larger than the sStorage
  record) says it is busy (0x599770) its save asked for (0x53F9E0: +0x490 = 1, +0x484 = 2); 1) wait for sStorage
  +0x490 to leave 1, then sInfoMan's two afters (0x567D50 (0, 1), 0x567DC0 (2)); 2) wait for sInfoMan +0x4EC90 to be
  0, then 0x567DC0 (0). Options (uIdOption10), Dog Tags (uIdCustomize10), Skills (uIdSkillSet10), uIdSpecial04 and the
  start-up menus 30-70 close through it; the Records screen does not (it never saves).
- sInfoMan's lock: a CRITICAL_SECTION at +4, taken by every setter when its byte +0x1C or the global 0x186E5D0 is set.
- The save file (`Steam/userdata/<id>/221040/remote/savedata.bin`, plain, 39416 bytes): a 0x20-byte header, the
  profile's saved half (cSystemInfo +8 on: a field at +X is at X + 0x18 in the file - the emblems +0xE1 at 0xF9, the
  skill points +0x6F4 at 0x70C, the WEAPONS page's counts +0xA90 at 0xAA8), then cGameInfo's saved copy of its live
  data at 0x7F08 (the base fields; the saved inventories at 0x84EC + character * 0x70). Read it to see a profile as
  the game last saved it (2026-11-14: the user's weapons, `docs/history.md`).

## The chapter records (the Records screen's PROFILES > Completed)

- A 12-byte record for each character (7: Leon, Helena, Chris, Piers, Jake, Sherry, Ada - the Agent is read as Ada,
  0xE441B0's `cmp edx, 7` / `mov edx, 6`), difficulty (5: Amateur, Normal, Veteran, Professional, No Hope - the last a
  flag on top, cGameInfo +0x2564) and chapter (5), from +0x1650: index (character * 5 + difficulty) * 5 + chapter.
  +0 cleared (bit 0; bit 1 a clear without dying, bit 2 a second condition - cGameInfo's per-character flag 2, counted
  by 0x564260/0x5642E0), +4 the best rank (0 S .. 5 E), +8 the best time (a float of seconds; 71999.0, the chapter
  clock's most, is the game's "none": 0xE44250's default, the chapter select's cap; a record never cleared holds 0).
- Where the best time shows (2026-11-14): Chapter Select's character page (0x9C0E50, its step +0x220 == 4: a row a
  character, a column a difficulty, for the chapter picked +0x248) - each cleared record's rank, and its time cut
  to whole seconds (0x9C1567: `fistp` with the FPU set to truncate - not rounded), held at 71999 (an unsigned
  compare) and drawn `%2d:%02d:%02d` (0x4618F0; another language `%2d:%02d'%02d"`): 71999 is "19:59:59", 0.0
  "0:00:00". The two get's callers are that page and the clear's update (0x563CA0: a first clear sets the rank and
  time, a later one only a better rank or a lower time - so a 0 stands for good) - no title, achievement or percent
  reads the time. The user's save that day: 155 records at 71999 (the mod's first clears) and 20 with flags 7 and
  time 0.0 ("0:00:00": cleared in play with Freeze play time holding the clock at 0).
- Gets: cleared 0xE441B0, rank 0xE44200, time 0xE44250 (thiscall (character, difficulty, chapter)); sets 0xE44350,
  0xE443A0, 0xE443F0; sInfoMan's 0x563CA0 at a clear (rank and time kept when better; bit 0 |1, |2, |4).
- The chapter's end (sInfoMan 0x466770, for each local player who played it): the records (0x563E00: the grade
  0x550EA0 gives the rank), then for a story (mEpisodeNo < 4) its chapter's achievement story * 5 + chapter + 1
  (Steam's ACH02-ACH21) and its dog tag template (the same number, +0xC8 + n: 0 locked, 1 new, 3 seen; 25 templates)
  with the "Earned a dog tag" note; Chris' chapter 1 the Collection files 0x33, 0x34 and Jake's chapter 3 0x35, 0x36
  (sInfoMan 0x4646B0 -> 0xE43750: +0x818 + n set to 1 if 0, 80 of them); then each story complete (0x564030: every
  chapter cleared on some difficulty by either of its characters): any - the unlock flags (+0xB8, mUnlockFlag) |= 2
  ("8 skills now available"); Chris' - +0x970 |= 2 (the Steel Beast stage); Jake's - +0x970 |= 4 (Mining the Depths);
  Leon's, Chris' and Jake's - +0xB8 |= 4 (Ada's campaign); Ada's - +0x978 |= 0x1000000 (Ada for the extra content);
  and for each difficulty 0-3 whose every chapter of every story is cleared on it or a harder one (0x5640E0) the
  achievement 0x15 + it (ACH22-ACH25).
- The Records screen (uIdRecord00, 0xAF0 bytes): its constructor (0x9E5430) copies the records into its own grid
  (+0x244, a byte a record: the rank, capped at 5, or 6 not cleared; kSigRecordGridFill) and the percent (+0x2F4:
  records held * 100 / 175, 0x563D90); its frame (vtable slot 8, 0x9E8060) draws page +0x48 (0 PROFILES, 1 WEAPONS, 2
  ENEMIES, 3 MEDALS, 4 EXTRAS; kSigRecordPages); PROFILES (0x9E7F80) shows view +0x320 (0 the profile, 1 Completed -
  the grid 0x9E4310, a row a character, a column a difficulty, five chapter marks each - 2 rescued, 3 saved, 4
  distances). The screen's step is uIdCBase's byte +0x38 (0-1 opening, 2 up, 0x5A-0x5C closing).

## The medals (the Records screen's MEDALS)

- A byte each (the times earned, at most 99), in nine categories; 2 in a category's table marks a number that is no
  medal. The medal page (0x9E6380) reads the category +0x340 (stepped by the input 0x9E39F0 through the mask +0x348:
  0xF, Survivors 0x30 with its DLC, Onslaught 0x40, Predator 0x80, Siege 0x100) and asks sInfoMan's 0x565750
  (thiscall (category, medal); a switch) for a count, 0x5656B0 for its name's message:

  | category | label | counts (cSystemInfo) | medals | unused table | names (mes_file) |
  |---|---|---|---|---|---|
  | 0 | CAMPAIGNS | +0x1FB0 | 200 | 0x1796CC0 | 3000 + n |
  | 1 | AGENT HUNT | +0x209C | 50 | 0x1796D88 | 3200 + n |
  | 2 | THE MERCENARIES | +0x20E8 | 50 | 0x1796DBC | 3300 + n |
  | 3 | NO MERCY | +0x23B0 | 50 | 0x1796DF0 | 3300 + n |
  | 4 | SURVIVORS (DEATH MATCH) | +0x2284 | 50 | 0x1796E58 | 3400 + n |
  | 5 | SURVIVORS (TEAM BATTLE) | +0x22B6 | 50 | 0x1796E8C | 3450 + n |
  | 6 | ONSLAUGHT | +0x222C | 50 | 0x1796E24 | 3350 + n |
  | 7 | PREDATOR | +0x2308 | 50 | 0x1796EC0 | 3500 + n |
  | 8 | SIEGE | +0x2364 | 50 | 0x1796EF4 | 3550 + n |

  Categories 4 and 5 share one get (0xE44FE0, (list, medal)). The label table (0x175DE34) gives category 5 message
  3298, which mes_system has no text for: the window uses 2304 ("SURVIVORS (TEAM BATTLE)"). The page reads the counts
  live: a change shows at once.

## The emblems and the achievements (the Dog Tag screen's EMBLEM list)

- 50 emblems, a byte each at +0xE1 (0 locked, 1 new, 3 seen): emblem n is achievement n (Steam's ACH<n+1>). The
  game's achievement (sInfoMan 0x562DD0, thiscall (n), kSigAchievement): nothing under sBioMain +0x1045C bits 0 or 1
  (its no-achievement state); else, under sInfoMan's lock, the emblem set to 1 if 0, and 0x403AA0 on the achievement
  object (global 0x1877848): its cached "unlocked" (0x17B9CA0 + n * 400) set, SetAchievement("ACH<n+1>") on its
  ISteamUserStats (+0x4C, slot 7), and +0x52 set for the store. 29 callers (the chapter end, the title check, the
  Dog Tag screen's close - ACH26 "Check Out My Dogs" - its start - ACH27 at ten titles - the skills, ...).
- The Dog Tag screen's start (0x984D30) gives an emblem whose achievement Steam already reports unlocked
  (0x562DA0 -> the cached flag) the byte 1: emblems follow Steam's achievements, never the other way.
- Achievements 51-70 (Onslaught, Survivors, Predator, Siege) have no emblem.

## The titles (the Dog Tag screen's TITLE list)

- A family for each counter 1-301 (46 unused: 16-45, 196-210, 301 - 0x179A270, 2 for unused): its state a byte at
  +0x113 + counter - 1 (the level held 0-4; bit 7 NEW), its counter a dword at +0x240 + 4 * (counter - 1) (some
  families a float: the title's float flag). Gets 0xE43220 / 0xE43260 / 0xE43280, sets 0xE43240 / 0xE432A0.
- The title table (0x179A3A0, 982 records of 12 bytes, the count at 0x179D1A8; 0x54EA30 a record, 0x54EA50 the find
  by (counter, level)): +0 its message number n (the name mes_file 600 + n, what earns it 1800 + n), +2 the counter,
  +4 the level (1-4), +5 a family of stars (the name repeats with a star more), +6 the counter is a float, +8 the goal.
  Some DLC titles' texts name other numbers than their goals (Relic Hunter, Bodyguard, ...): the game counts the goals.
- The game's title check (0x568A60): for each title whose counter reached its goal and whose level is above the
  family's, the state = level | 0x80, and a level-4 one gives achievement 36 ("Rising Up"); then the families held
  (of the first 300) into counter 11 (the Title Collector's). The Dog Tag screen's start gives achievement 26
  ("Titular Achievement") at ten held.
- The Dog Tag screen (uIdCustomize10, 0x1FA0 bytes): its step (+0x38) 10 the menu (its choice +0x48), 0x14/0x15 the
  template list, 0x1E/0x1F the emblem list, 0x28/0x29 the title list (each list, then its Equip? prompt;
  kSigDogTagLists); its start (0x984D30) copies the lists into rows of 0x14 bytes - templates +0x25C, emblems +0x400
  (how many +0x23C: 50), titles +0x7E8 (how many +0x244: 301, a row a counter: +0 the state, +4 the counter's number,
  +8 whether the detail shows the progress (the level-1 title's stars flag; 0 once every level is held), +0xC the
  progress - the counter's value (a float family's cut to a whole number, `__ftol2`), +0x10 the next goal (the held
  one's when none)) - and its close saves the profile (0x9756C0) when it changed.
- The title list (0x9869F0, 15 rows a page): a row's name is its held title's (mes_file 600 + n), or "? ? ?" (mes_system
  0x3E0) while none is held. The detail of the row under the cursor (0x987150) shows what earns the next level (the
  last when all are held: mes_file 1800 + n) only while the row's progress is above 0 - "? ? ?" before, held or not -
  then "(progress/goal)" when +8 is set. So the game reveals what earns a title once its counter has started, and a
  title it awards (the check above: counter at the goal) always has. The pages (kSigDogTagTitlePages, right after the
  rows are filled): +0x24C the page shown (+0x248, the first row shown, / 15), +0x250 the last page (the rows / 15),
  +0x254 the pages the LB/RB buttons go to - page 0, and a bit for each page with a used row whose progress is above
  0: a page of titles nobody has started is skipped.
- Two families count in a float: 119 (seconds in cover: 300, 600, 1800, 6000) and 164 (seconds in cover behind
  chrysalids: 60, 180, 600, 1200); the check rounds them, the screen's start cuts them. Every goal is at least 1 and
  rises with the level (982 titles: 242 families of four levels, 12 single, one of two).

## The skills (the skill screen, uIdSkillSet10)

- The skill table (0x1795280, 92 records of 20 bytes, the count a global 0x1794B24; sGameMan's 0x54E570 a record,
  0x54E590 the count): +0 the skill's row (a skill's levels share it), +1 its level (0-2), +2 the top level, +3 a mask
  of the lists it is on (bit = the screen's kind of list, +0x220: 0 the campaigns, 1 the Mercenaries and No Mercy (modes
  0x1E, 0x50), 4-7 the modes 0x28, 0x46, 0x3C, 0x32 - its constructor's; bit 2 marks the Agent Hunt creature skills, a
  kind the constructor never sets), +4 the skill's id (its state's place and its texts: mes_file n its name, 100 + n
  what it does, 200 + n what the game asks), +8 its price, +0xC its requirement, +0xE (a word) what it counts, +0x10 (a
  word) the goal.
- Each skill's state, a byte at cSystemInfo +0x6FC + id (0x61 of them; get 0xE43390, set 0xE433B0, sInfoMan's set under
  its lock 0x561340): 0 locked, 1 new, 2 seen (the screen's start makes a level-0 row's 1 a 2, 0x9F3ED0, and then shows
  "new skills" - +0x57C, 0x9F22B0), 3 bought (the purchases, 0x9F5B4C and 0x9F5D76).
- The game's test of whether a skill can be bought (sInfoMan 0x566AB0, thiscall (record)): any state but 0 says yes;
  else by its requirement - 0 (and past 4) yes; 1 the skill before it (id - 1) bought: a skill's next level; 2 the kills
  of an enemy kind (0x5666E0: 0x1000 counts 0x1000 + 0x1400 - J'avo Killer's; 0x5000 counts 0x5000, 0x5420, 0x5500,
  0x5600, 0x5610, 0x5700 - Zombie Hunter's; any other alone) at least the goal; 3 the kills with a kind of weapon
  (0x5668D0: a switch adds up 0x101 + 0x102 + 0x103 + 0x10D (handguns), 0x104-0x106 (shotguns), 0x107-0x108 (Magnums),
  0x109-0x10B (sniper rifles), 0x10C + 0x10E (machine pistols), 0x10F-0x111 (assault rifles), 0x113-0x116 (grenades);
  any other alone - 0x112 the grenade launcher, 0x117 the crossbow) at least the goal; 4 every campaign complete (each
  story's test 0x564030 to its chapter 4 - the one a chapter's clear asks, 0x5640A0 - for all 4). The kills: the
  Records' enemy table (0x1795C18, 8 bytes: +0 the entry (a signed byte), +3 and +5 two keys (0 and 3), +6 the kind;
  0x5647B0 the find) into cSystemInfo +0x1328 + entry * 8 (0x5A of them, 0xE43FE0); the weapon kills: the weapon table
  (0x17959B0, 12 bytes: +0 the entry, +8 the weapon; 0x566880) into cSystemInfo +0xA94 + entry * 0x2C (0x32, 0xE43C40).
  Nothing else counts toward a requirement.
- 19 skills have a requirement of 2-4, all on the campaigns' list only: J'avo Killer Lv. 1 (id 16, 30 J'avo) and Zombie
  Hunter Lv. 1 (18, 30 zombies) - their Lv. 2 need Lv. 1 bought; Grenade Power-Up (20, 800 grenade kills), Handgun
  Master (21, 1,500), Shotgun Master (22, 1,000), Magnum Master (23, 800), Sniper Master (24, 1,000), Machine Pistol
  Master (25, 1,500), Assault Rifle Master (26, 1,500), Grenade Launcher Master (27, 800), Crossbow Master (28, 800);
  the eight infinite weapons (29-36: every campaign complete). The rest are 0 ("Default") or 1 ("Possess ... Lv. n").
- The menus' commit (0x5687F0, before every save) first gives each record with a row below 0x41, state 0 and its
  requirement met the state 1 - so the game unlocks a skill by itself at the next save once its count is there.
- The skill screen (uIdSkillSet10, 0x600 bytes; made by the menus (0xD26950, 0xD2C650) and by a game's flow (0x466250, a
  step of 0x471E50's - the chapter's end, as read [I]; +0x5C 1, or 2 for a second player's): its kind of list +0x220
  from sGameMan's mode (its constructor 0x9F2020); its user +0x228 (sQlocInput +0x350; the profile its list is filled
  from and its purchases go to: the screen pushes it as the one in use, 0x561A50, around each); its copy of the points
  +0x588; its list's rows +0x2BC (16 bytes each, at most 0x2C, how many +0x2B4), filled by 0x9F24D0 (at its start and
  after each purchase): a row a skill's row of records on this list - +0 the id (the level held, else the first), +1 its
  level, +2 the top level, +3 the next level to buy (once the one before is bought), +4 its state, +5 its requirement,
  +6 can be bought (0x566AB0), +8 its price, +0xC 0. The rows draw (0x9F33E0) "? ? ?" (mes_system 0x3E0) where +6 is 0,
  else the name and price; the detail (0x9F4E80) what it does - or, for a locked one with a requirement of 2 or more,
  what the game asks. A purchase (step 0x1E) is refused while +6 is 0 or the points are short. The screen's frame (slot
  8, 0x9F50D0) steps on +0x38 (uIdCBase's); its closing step 0x5A runs the menus' save (0x9756C0): the profile is saved
  when the screen closes if anything changed (a purchase, or the mod's change).

## The weapons (the Records screen's WEAPONS page) - traced 2026-11-14

- The page (page 1 of uIdRecord00's frame, 0x9E4660; kSigRecordPages' case +45): 43 rows (the screen's count
  0x5645F0: the weapon table's largest row + 1), 10 a page from +0x330 (the first shown; the cursor +0x328, the rows
  +0x32C). A row is the first record of the weapon table (0x17959B0, 51 records of 12 bytes, the count a global
  0x1794B8C; sInfoMan's 0x564750) whose row (+0, a signed byte) is it: **named** (mes_file 3600 + the row, 0x9E483C)
  with its Times Used beside it **when the record's byte +2 is set or Times Used is above 0**, else "? ? ?" (mes_system
  0x3E0) - kSigRecordWeaponRows. The cursor's detail: Times Used (mes_system 0x8CA), Enemies Defeated (0x8CB,
  kSigRecordWeaponKills), and by the record's bytes +3..+6 Accuracy (0x8CC: hits over the last min(Times Used, 100)
  uses, a ring of bits), Headshots, Reloads, Quick Shots; the tablets' and spray's rows (items 0x11D, 0x120) Times
  Used only.
- The record: +0 its row, +1 an icon, +2 **always named**, +3..+6 which counts the detail shows, +7 counts for
  achievement 35 (0x563270: every such row's kills at the goal), +8 the item (a word), +10 the mode (-1: any). The
  row a use counts on: sInfoMan's 0x564670 (item, mode) - the first record of the item whose mode is it or any
  (kSigWeaponEntry; the shots' and hits' counters call it). The Bear Commander's mode 1 (its M203) counts on the
  Grenade Launcher's row (17), the hand-to-hand moves on 28-40, the turrets on 27, the Mutated Hand on none (-1).
- Always named (15): the 909, Picador, Wing Shooter, Hydra, Anti-Materiel Rifle, Triple Shot, MP-AF, AR for Special
  Tactics, Hand Grenade, Survival Knife, Combat Knife, Stun Rod, Physical Attack, Health Tablet, First Aid Spray - each
  character's starting weapons and the rest. The table is in .data but nothing writes it (every reference reads).
- The counts (cSystemInfo, a record of 0x2C a row, 50): +0xA90 Times Used, +0xA94 Enemies Defeated, +0xA98 ?, +0xA9C
  Headshots, +0xAA0 Reloads, +0xAA4 Quick Shots, +0xAA8..+0xAB7 the accuracy ring (100 bits), +0xAB8 its place. A use
  (0xE43D20, thiscall (row, n): its one caller 0x566077) adds n to Times Used (at most 9,999,999), moves the ring on
  and clears the new bit (a hit sets it). **Owning a weapon names nothing**: the user's save (2026-11-14) had Leon's
  Lightning Hawk and Piers' Grenade Launcher in their saved inventories, their rows "? ? ?" (0 uses; the Grenade
  Launcher's a kill, still "? ? ?"); the rows named at 0 uses were the always-named ones.
- The saved inventories are `docs/inventory.md` "Carried over": cGameInfo +0x20D8 + character * 0x70 (0xD70B20), the
  slot in hand +7, 24 slots from +0x10 (an item word and a count, -2 kept/not had, -1 a full magazine), the first 7
  the weapons. The New Game reset (0x56CF30, kSigCarriedReset; its weapons due by the stage kSigCarriedDue) reads every
  table: the characters' defaults (sGameMan's 0x552090: 0x1794D98, 10), weapon tables (0x552750: 0x17948FC, 8) and
  their counts (0x552770: 0x179491C; 6-byte entries: +0 the stage, below 0 from the start, +2 the chapter, +4 the
  item), the stage's place in the story (0x551AF0). The 9 weapons found on the way are all shared: on four
  characters the Shotgun (Leon, Helena, Jake, Sherry), Semi-Auto Sniper Rifle (Leon, Helena, Chris, Piers), Assault
  Shotgun (Chris, Piers, Ada, the Agent), Sniper Rifle and Bear Commander (Jake, Sherry, Ada, the Agent); on a
  campaign's pair the Assault Rifle RN and Lightning Hawk (Leon, Helena), Grenade Launcher (Chris, Piers), Elephant
  Killer (Jake, Sherry) - 28 character-weapon pairs.
- The menus' save keeps a change there: its cGameInfo copy (0xD702A0: the live data, +0x1AF4, into the part the save
  file keeps, +4) runs in its first step - but only once the game's "changed?" (0xE43150: cSystemInfo's two halves)
  says yes; for a change to the cGameInfo alone the mod runs the steps anyway.

## The enemies (the Records screen's ENEMIES page) - traced 2026-11-14

- The page (page 2 of uIdRecord00's frame, 0x9E4E20; kSigRecordPages' case +54): 86 rows (the screen's count 0x564780:
  the enemy table's largest row + 1, stored +0x338; 0x59 if it were 0x5A or more), 10 a page from +0x33C (the first
  shown; the cursor +0x334). A row is the first record of the enemy table (0x1795C18, 87 records of 8 bytes, the count
  a global 0x1794BF4; 0x564860 - the WEAPONS page's lookup's shape) whose row (+0, a signed byte) is it: **named**
  (mes_file 3700 + the row, 0x9E4FC0) with its Enemies Defeated beside it **when the record's byte +2 is set, or
  Enemies Defeated or Times Defeated is above 0**, else "? ? ?" (mes_system 0x3E0) - kSigRecordEnemyRows. The cursor's
  detail: Enemies Defeated (mes_system 0x8CB), and Times Defeated (0x8D0) unless the record's byte +4 is set
  (kSigRecordEnemyDefeats: the Chrysalid, Oko and the six animals - they defeat nobody). Rows 0x54/0x55 (The Mini Tank,
  The Witch) take another icon.
- The record: +0 its row, +1 an icon, +2 **always named** (J'avo, 0, and Zombie, 43: the only two), +3 and +5 the two
  keys a kill is counted by (0/3 for a kind of enemy; 1/7 the J'avo's weapons, 2/7 and 2/14 the zombies'), +4 no Times
  Defeated, +6 the kind (a word). Record 36 (kind 0x9902) has no row (-1).
- The counts (cSystemInfo, 8 bytes a row, 0x5A): +0x1328 Enemies Defeated (0xE43FE0; added by 0xE44020, at most
  9,999,999), +0x132C Times Defeated (0xE44000; 0xE44050) - sInfoMan's 0x5663C0 (which, player, row) adds to one or
  the other (which 0 or 1) for a player of this machine. Only the page and the skills' kill count (0x5666E0: J'avo
  Killer's rows 0 and 1, Zombie Hunter's 43 and 69-73) read them: no title, achievement or other screen.
- The user's save at 19:00 (file 0x1340 + 8 * row): 12 rows "? ? ?" - Glava-Begunats, Glava-Dim, Noga-Oklop,
  Telo-Krljusht, Telo-Magla, Zombie - Crank, Zombie - Liquid Nitrogen Tank, Crow, Spider, Snake, The Mini Tank, The
  Witch; none named with no kill.

## The Mercenaries' unlocks (the stage select and the character select) - traced 2026-11-15

- **The stages**: cSystemInfo +0x970, a bit a stage - the Mercenaries' bits 0-9, No Mercy's 10-19, in the stage select's
  row order. The getter (sInfoMan's 0x563A00, kSigMercStageGetter; a chain the mod walks) gives `0x1C07 | (bits &
  0xFE3F8)`: each mode's first three always open (bits 0-2, 10-12), the rest the profile's. The stage select
  (uIdStartUp31, 0x2B8 bytes - "Select a stage.", mes_system 0x46B; its frame 0xA124C0, kSigMercStageFrame) reads it
  **every frame** into +0x50 (No Mercy's shifted right 10) for its 10 rows (+0x4C): a locked row draws "locked" (its
  name byte 10 of 0x1767318) and refuses the pick. The stage table (0x1797230, 20 records of 10 bytes, the count
  0x179722C; sGameMan's 0x556120, kSigMercStageTable) gives each bit its stage: rows 0-9 s1202 Urban Chaos, s1206 Steel
  Beast, s1203 Mining the Depths, s1200 Rail Yard, s1204 High Seas Fortress, s1205 The Catacombs, s1201 Requiem for
  War, s1209 Liquid Fire, s1207 Rooftop Mission, s1208 Creature Workshop; No Mercy's rows the same maps (s1212 ...:
  their Soft archives over the Mercenaries' maps - s1217-s1219 included). The names: mes_system 807 + (stage - 1200)
  % 10, the order the match end's unlock notices give (2499 + the same).
- How the game opens them: a story's completion (Chris' bit 1, Jake's bit 2 - both always open in this build anyway);
  the match end (sGameMan's 0x5597D0 the Mercenaries', 0x559E00 No Mercy's): a rank below 2 (S, A: the ranks S-E are 0-5,
  sGameMan +0x4288 by the score table 0x17987AC) on stage row k (2-8, sGameMan +0x3E50) opens row k+1 (its notice
  "... can now be used with the extra content").
- **The characters**: cSystemInfo +0x978, 64 bits - a bit a costume, the place of its record in the character table
  (0x1797398, 5 bytes: +0 the body, +1 the costume model (0x00, 0x10-0x12, 0x02 Carla, 0x13 No Mercy's four), +2 the
  character, +3 the costume's own number (0 Default, 1-3 Costume n), +4 the select id; 34 records in No Mercy
  (0x1797448), 30 elsewhere (0x1797300's copy, 0x1797444)): 0-3 Leon's Default and Costumes 1-3, 4-7 Helena's, 8-11
  Chris', 12-15 Piers', 16-19 Jake's, 20-23 Sherry's, 24-27 Ada's, 28 Carla, 29 the Agent, 30-33 Coach, Nick, Ellis,
  Rochelle (mes_system 1154 + bit names each: "Leon", "Leon - Costume 1" ... "Carla", "Agent"). The getter (sInfoMan's
  0x563AF0, kSigMercCharGetter) gives `bits | 0x10101 | 0x20000000` (Leon's, Chris', Jake's Default and the Agent
  always), with 30-33 set in No Mercy while sGameMan +0xB4 bit 0 is set, else cleared - sGameMan's reset (0x553CA0)
  sets +0xB4 to 1 and nothing clears that bit: No Mercy's four are always there, never the profile's.
- How the game unlocks them: Ada (24) by Ada's campaign; at the match end, a rank below 3 on row 0, 1 or 2 gives Helena
  (4), Piers (12) or Sherry (20), a rank below 2 the played character's Costume 1 (the switch at 0x559CE6 on the
  character's bit, kSigMercCostumeAwards: seven functions 0x5508E0 ... 0x550D90, each its bit and a Collection file -
  Leon 0x38, Helena 0x39, Chris 0x3A, Piers 0x3B, Jake 0x3C, Sherry 0x3D, Ada 0x37 - set even when the costume is had),
  Carla (28) once every character's Default and Costume 1 are (the mask 0x3333333); the other modes' counts give the
  same (0x555840, 0x5559E0, 0x555B70: Onslaught, Survivors, Predator's [I]). **Costumes 2 and 3** come only from the
  online event list (sStorage +0x9FBC: sGameMan's 0x555EA0 copies it, the menus' notice screen 0xA032A0 reads it - a
  type-2 entry's mask through the table 0x1766EA0 of 14 (bit, file) pairs: Costumes 2 and 3 of each, files 62-75;
  kSigMercCostumeEvents) - gated on sBioMain +0x1045C bit 18 and a linked account (cSystemInfo +0xCC). The user's save
  (2026-11-15): stages 0x1FFFF (No Mercy's 17-19 shut), characters 0x33333333 (every Default and Costume 1, Carla, the
  Agent), files 0-61 held and 62-75 not.
- **The character select** (uIdStartUp32, 0x320 bytes - "Select a character.", 0x46C): its constructor (0xA0D9A0)
  copies the getter into +0x168 (64 bits; the player +0x60, the stage +0x160), its start (slot 5, 0xA10090,
  kSigMercCharFill) fills the lists from that copy - for each of 13 characters (0x1767208 how many costumes, 0x1767218 a
  signed byte each: the bits; 0-6 four, then Carla, the Agent, No Mercy's four) the costumes the copy has (+0x214 + 16 x
  the character: each one's place among its character's, +0x1E0 how many, +0x1AC the costume cursor), a character
  with any a row (+0x178, +0x174 how many, +0x170 the cursor: the chosen character's, sGameMan +0x3E7C + 4 x player) -
  then its per-frame update (0xA0DA10) pushes the rows to sId each frame. Picking writes the bit to sGameMan +0x3E7C.
- **The stage select's side panel** (each character's best score on the stage): its start's fill (0xA0FED0, the
  Mercenaries' - kSigMercStagePanel; No Mercy's 0xA0D7C0 - kSigMercStagePanelNoMercy) - three pages (0x1767324 /
  0x1767260: a count byte then the select ids - the Defaults, the costumes, No Mercy's four) into +0x16C (counts),
  +0x184 (ids, 9 a page) and +0x178 (a bit an id with an available costume: the Mercenaries' test the low dword).
- **The records** (each stage's best per character; 2026-11-15): cSystemInfo +0x24D0 the Mercenaries' (0xE45BA0), +0x4690
  No Mercy's (0xE45BE0): [group 0 solo, 1 duo][the stage row 0-9][the select id 0-35] x 12 bytes - +0 had, +1 the rank
  (0 S .. 5 E: the score against 0x17987AC + 0x18 x group - the Mercenaries' solo S 140,000, duo 220,000; No Mercy's
  0x17987DC), +4 the score (at most 9,999,999), +8 the kills (sGameMan +0x4260, a kill's +1; a byte: No Mercy's 300
  is 44), +9 the best combo (+0x426C, a byte), +10 the costume played (its bit), +11 the partner's. The match end
  (0x55AFD0, No Mercy's twin) writes one when it has none or the score beats it, and with it the mode's best (+0x20DC)
  and the stage's (+0x2160 + 4 x (stage + 14 x group), its rank +0x21D0, character +0x21EC). The stage select's side
  panel shows each id's rank and score. The online ranking (0x52E8C0 at a match's end -> 0x52ADC0 / 0x52AF30) uploads
  the stage's best record of the profile (the highest of its 36: 0x563BC0). The game's title recount (0x56E220) counts
  the stages with an S (counter 0xD6) and the ids with one (0xD9) from the Mercenaries' table.
- **The save file** (`savedata.bin`, 0x99F8 bytes): the header (sStorage +0x498: the magic 0x12071801, the size, the
  checksum - the 32-bit sum of every dword after the header, 0x53FAB0 - and the Steam ID's two words xor'ed with
  0xCC013017 / 0xA1C15241 at +0x10), checked as it loads (0x540E50: any mismatch and the profile is not taken).
  `tools/merc_records.py` reads and fills the records offline (the game closed) and writes the checksum again.
- 2026-11-15 (the user: "fill out S rank completions for the rest of the level and character combinations for both
  mercenaries modes ... estimations from existing data ... All values should be different within a small range near
  existing records"): `tools/merc_records.py --fill --write` - 297 solo records (the Mercenaries' 126: every
  character but Leon and Helena on stages 1-9; No Mercy's 171: every one but Leon) set S, each score drawn from that
  stage's own records (2.5% under their lowest up to their best, each its own, its last two digits as the stage's
  end), the kills and combo the mode's records hold (150/150, 44/44), the costume each character's Urban Chaos
  record was played in, no partner; every score under the stage's best, so the record the ranking uploads stays one
  played. The save before it: `~/RE6-save-backups/2026-11-15_before-merc-s-ranks/`.
- Both screens close through step 0x5A, and neither saves: the Mercenaries' play-type menu (uIdStartUp30) saves as it
  closes (0x9756C0), and a mode's start takes the profile back from its saved copy - so the window saves at once.

## What the mod does (records.cpp)

- The windows show while their view is up (the records' never in a game, the skills' wherever the skill screen comes
  up). The screens are known by their own coming and going (2026-11-12; before, the tick walked sUnit's lines each
  frame): each screen class's start (slot 5) and destructor (slot 0) - the slots sUnit's line frame calls
  (`docs/engine.md`, "The units") - hooked in that class's own vtable only (uIdRecord00, uIdCustomize10, uIdSkillSet10:
  one each); the start notes the screen up, the destructor forgets it before its memory goes. The tick reads the views
  of the ones up.
- Chapters: a record set or cleared (bit 0 and the rank; a first clear's time the game's "none", so a real clear
  replaces it); the screen's grid byte and percent rewritten; for a first clear what the chapter's end gives (the
  template, the files, the unlock flags, and the achievements through the game's own call - the chapter's, and a
  difficulty's whole game when the batch completes it). A clear taken back leaves its rewards (and its time).
- Chapter times (2026-11-14, the user: "it currently defaults to 19:59 on the time for newly completed levels. Allow
  the user instead to set the clear times as well"): a cleared record's best time typed under its rank (h:mm:ss, m:ss
  or seconds; the field only under a chapter that is or will be cleared) or beside the "Every chapter" rank for a
  character or everyone - whole seconds, held at the game's most (the chapter clock's, read from its add: 71999). A
  first clear with no time typed still gets the game's "none" (unless it kept one from a clear taken back). The game's
  own clear keeps the better time (0x563CA0: replaced only by a faster one), so a time set here stands until a real
  clear beats it. The window shows a cleared record's time as Chapter Select does (cut to whole seconds, 0 as
  "0:00:00"); the "none" only under a chapter Apply is to clear that holds no time (2026-11-14 21:19: it took a 0 for
  the "none" and showed 19:59:59 after 0:00:00 was applied, `docs/history.md`).
- Medals: the counts (0-99), only where the category has a medal.
- Emblems: the game's own achievement for each (the emblem and Steam's); under the game's no-achievement state the
  emblem is set by the mod. The screen's row rewritten.
- Titles: a family raised to a level (never lowered), NEW as the game marks it, with its counter brought up to that
  title's goal (never down; a float family's as a float; counter 11 is recounted instead) - as earning it leaves it, so
  the game shows what earns it and goes to its page; a level held already, asked for again, brings its counter up
  only. The screen's row rewritten as its start fills it (the progress with it), the pages its buttons go to worked
  out again; counter 11 recounted; "Rising Up" at a level 4 and "Titular Achievement" at the tenth through the game's
  call. The window flags a title held with its counter short of its goal (the mod's awards before 2026-11-06 left
  236 so: "? ? ?" in the game) and offers "Bring their counts up" for them.
- Skills (beside the skill screen while its list has skills behind a requirement - the campaigns'; in the menus and
  between chapters alike): each of those 19 with what the game asks and its count (the mod reads the same tables the
  game's test reads, its groups read out of the test's own code). Unlock: state 0 -> 2 (as the screen leaves a new
  one); lock again: back to 0. Never a bought one; never one whose requirement the game's own count meets (the game
  sells it whatever its state). The screen's row is set as its fill sets it (state, can be bought); nothing else
  changes (the points, the sets, the levels). No save of the mod's: the screen's close saves it, as it does a
  purchase.
- Weapons (2026-11-14, the user: "add an imgui window to toggle having acquired weapons, which should also remove the
  "???" entries for missing ones"; told the page has no "acquired" flag, they chose "Page + campaign inventory", then
  asked how shared weapons would show: a grid). Beside the WEAPONS page: the 28 rows the page can draw "? ? ?" - a row
  named by its first use (Times Used 0 -> 1, as one missed shot leaves it: Accuracy 0%), or that use taken back (1 ->
  0) but never a row played (more than one use, or a kill); and the 28 character-weapon pairs as a grid (a row a
  weapon with its page tick, a column a character) - a weapon put in a character's saved inventory (kept -> had, a
  full magazine) or taken out (had -> kept, its loaded ammo with it; the slot in hand moved off it to the first weapon
  still had). Only a weapon on the character's own table, in a record that holds its slot (a profile's records are
  set up by the game at a game's start: 0x561B70). All: a character's, a weapon's (its page row too), every weapon,
  every row. The saved inventories need the game's save (discover_carried: the copy into the save's part).
- Enemies (2026-11-14, the user: "for the enemies page, have an imgui window to reveal all enemies (revealing should
  set kill count to 1 if less than 1)"). Beside the ENEMIES page: its 84 rows that can be "? ? ?", each with its counts
  and a tick; a "? ? ?" row (neither count above 0) revealed by its first kill - Enemies Defeated, below 1, made 1 -
  and Reveal all for every one. A row the page names already (a kill, or Times Defeated alone) keeps its counts, and
  nothing is taken back (a boss's one real kill and the mod's look the same). The page reads the counts live: a row is
  named at once. A reveal can add one to J'avo Killer's (row 1) and up to five to Zombie Hunter's (69-73) count.
- The Mercenaries' unlocks (2026-11-15, the user: "add imgui window to the mercenaries and mercenaries no mercy modes
  to unlock levels and alternate players / costumes"). Beside the stage select and the character select while the
  mode is the Mercenaries or No Mercy (both screens are other modes' too: no window there): the mode's ten stages with
  a tick each (a bit of +0x970 set or cleared; the three always open fixed) and the profile's nine characters with a
  tick for each costume (a bit of +0x978; Leon's, Chris', Jake's Default and the Agent fixed, No Mercy's four not
  listed - always there). A costume unlocked gets its Collection file (1 if 0), as the game's own unlock gives it; a
  lock leaves the file. The stage select shows a stage at once (its frame reads the getter); the mod fills its side
  panel's masks, and the character select's copy (its extras as the game left them, the rest the profile's) and lists,
  as their starts fill them - the cursor kept on its row (held to the rows: sId's highlight, set only by the game's
  own moves, stays on the row number), each character's costume cursor on the costume it was on. Everything read out
  of the game's code: the getters' rules (the stage getter's chain walked; the character getter's always and extras
  masks), the tables, the screens' fields (kSigMerc*), the files from the awards' own pushes and the events' table.
  Logged: `records: the Mercenaries' unlocks: ...` at the start, `the stage select ...: step N, mode 0x1E` as the
  screens change, each change's count.
- After each Apply (not the skills): the menus' own save steps above, run by the tick (a busy storage: committed, not
  written - the game's next save writes it).
- Game calls: the achievement (0x562DD0) and the save's steps (0xE43150, 0x5687F0, 0xD702A0, 0x599770, 0x53F9E0,
  0x567D50, 0x567DC0), both the user's choice (2026-11-06; the Mercenaries window saves through the same steps, its
  change otherwise lost at a mode's start); everything else field writes under sInfoMan's lock.
- [I: unseen in play - the windows' screens and views, a change showing at once on each screen, a time set showing on
  Chapter Select, the save (and its
  "Saving" icon), Steam's achievements from the game's call, the rewards (Ada's campaign, the templates in the Dog Tag
  list), a medal page's category labels; the skills window at the skill screen (menus and between chapters), an
  unlocked skill's row showing at once and selling, the skill screen's close saving the change; the weapons window at
  the WEAPONS page, a row named at once, a weapon given there in hand at Chapter Select (and taken out gone), the save
  of a change to the saved inventories alone; the enemies window at the ENEMIES page, a row named at once, saved; the
  Mercenaries window at the stage select and the character select (both modes), a stage opened there pickable at once,
  a costume unlocked there offered at once (and in a match: its model), the rows' refill with the cursor on its row,
  the Collection files showing in the Collection, the save.]
