# History

## The play log

Each report of the user's: its cause, its fix, the log lines to look for.

### 2026-11-15 night - the Mercenaries' records filled with S ranks (the save, offline)

The user (22:2x): "fill out S rank completions for the rest of the level and character combinations for both
mercenaries modes that I haven't done yet. Use estimations from existing data to determine what values are likely
for each level without blindly using the same values. All values should be different within a small range near
existing records".
- Read (`docs/records.md` "The Mercenaries' unlocks", the records): the save of 22:18 held only solo records, all S -
  the Mercenaries: every character on Urban Chaos (1,251,555-1,252,759), Leon and Helena on the other nine stages
  (1,239,155-1,525,551); No Mercy: every character on Urban Chaos (~2,509,000), Leon on the other nine (2,527,855-
  2,695,958); kills and combo 150/150 (No Mercy's 300 stored as a byte: 44/44). It also showed the window used: all
  20 stages open, all 30 costumes, files 62-75.
- The game uploads the stage's best record of the profile to the online ranking at a match's end - so no filled
  score may be a stage's best.
- Done (`tools/merc_records.py --fill --write`, the game closed): 297 records, as in `docs/records.md`; only those
  records' bytes and the checksum changed (checked against the backup); the header passes the load's checks.
- To look for: the stage select's side panel listing an S and a score for every character on every stage (both
  modes); Steam Cloud uploading the file at the next start (if Steam asks, keep the local copy, 22:30).

### 2026-11-15 night - the Mercenaries window (stages, characters and costumes unlocked)

The user (21:5x): "add imgui window to the mercenaries and mercenaries no mercy modes to unlock levels and alternate
players / costumes".
- Read (`docs/records.md` "The Mercenaries' unlocks"): the profile keeps a stage bit each (the Mercenaries' 0-9, No
  Mercy's 10-19, in the stage select's order; the first three of each always open) and 64 character bits (each
  costume: Leon's Default and Costumes 1-3 ... Ada's, Carla, the Agent; No Mercy's Coach, Nick, Ellis, Rochelle never
  the profile's - always there, sGameMan's reset sets their flag). The game opens a stage after an A or better on the
  one above it, gives Helena, Piers, Sherry and each Costume 1 at a match's end (rank and counts), Carla once every
  Default and Costume 1 is had - and Costumes 2 and 3 only through its online event list (a linked account's
  rewards). The user's save (21:32): the Mercenaries' ten stages and No Mercy's first seven open, every Default and
  Costume 1, Carla and the Agent - every Costume 2 and 3 locked, their Collection files (62-75) not had.
- The screens: the stage select (uIdStartUp31) reads the stages open every frame; the character select
  (uIdStartUp32) copies the characters available as it is made and fills its rows at its start; neither saves, and a
  mode's start takes the profile back from its saved copy.
- Built (`records.cpp`, a records window - 9 signatures, kSigMerc*): beside the stage select and the character select
  in the Mercenaries and No Mercy (not the other modes that use them), the mode's ten stages with a tick each and the
  nine characters with a tick for each costume (the ones always there fixed); Open every stage, Unlock every costume,
  a character's All; a costume unlocked gets its Collection file as the game's own unlock does. The stage select shows
  a change at once by itself; the mod fills its side panel and the character select's copy and lists as their starts
  do (the cursor kept on its row). The profile saved after each change through the menus' own steps (the records
  windows' approved save). Field writes otherwise; no new game call.
Checks: discover 571 (28 new: the game's own getters, the character select's start and the panel fill run in the
copy against the mod's), panel 147 (15 new), boot 18; 144 signatures, 0 bad. Installed 22:06 (the game closed).
Unseen in play.

### 2026-11-15 - the Mercenaries' enemies sent faster; bodies gone as they die

The user (20:1x; the run before, 13:31-13:46, two No Mercy matches): "it's working now, however I still think we
could spawn more enemies in faster".
- Read: that run's log - s1212 (Urban Chaos' No Mercy; its senders wait 0.5-1.0 s) 300 kills in 202 s (1.5 a
  second), Steel Beast's No Mercy (s1216; 2.0 s) 300 in 524 s (0.57 a second: one each 2 s, but for its first
  sender, which has no wait) - and every stage's senders (`tools/fsm_dump.py`): with the bodies gone soon and every
  enemy killed as it comes, the pace is each sender's own wait, mWaitTimer: 0-2.0 s between enemies (No Mercy's
  s1210, s1211, s1214, s1216, s1218, s1219: 2.0 s), at most mMinEmNum in play (3-13; No Mercy's 20-60).
- **The wait** (EnemyAdd's frame, 0xD59380; `docs/modes.md`, "The senders' waits"): the work's timer (+0x38) counts
  down by the frame's time only while fewer than the minimum are in play, and at 0 the send step sends mAddEmNum (1)
  and reloads it with mWaitTimer (+ its random, 0 on every stage).
- Built (`merc.cpp`, while Kill enemies as they appear acts): the timer's test (its `jbe` to the send step,
  kSigEnemyAddWait +21, 0xD59611) made a `jmp` - a verified byte patch (the timer's place the same in the test, its
  countdown and its reload from the param's mWaitTimer; the wait, its random, the minimum and mAddEmNum one after
  another as the class has them; the constant 0) - so a sender sends whenever fewer than its minimum are in play. The
  stage's minimums (how many at once) are the game's, as its own wait-0 senders run (s1208's, each No Mercy stage's
  first). Not a game call. Simulated (`tools/merc_sim.py --nowait`, bodies 1.5-3 s): the clear in 22-67 s on the
  normal stages (45-242 s with the waits) and 10-23 s on No Mercy's (92-592 s); no stage's totals change.
Checks (that build): discover 534 (the test found where the build has it; on, a jmp; the game's own frame run in the
copy - on, a sender 1.5 s from its next sends at once, at its minimum it does not; off, its wait runs by the frame's
time), panel 132, boot 18, memlog 32, crash 26; 134 signatures, 0 bad. Not installed on its own (the game ran on).
- The user, as that was built (20:4x): "the problem with increasing the stage's limit is that we'll hit that new
  number and then this same pattern will continue with how slow it is. We need to decrease the death animation
  during this cheat to allow for faster respawns instead of just kicking the can with a higher cap". With no wait,
  a body's time in the stage's count is the pace: its death's fall and pose (motions), then its erase (short since
  2026-11-14 night).
- **A body's end** (`docs/modes.md`, "A body's end"): every erase ends by setting uEnemy +0x3B30, and the enemies'
  brains' end step (uEmIntel's slot 34, 0x942B10 - every family's brain runs it last) then ends the enemy (both
  mRno 3). Everything a death scores is done at the death itself; after it come only motions and the dissolve.
- Built (`merc.cpp`, while Kill enemies as they appear acts, not at the pause): the tick sets that byte on each body
  whose kill has been reported (not a part of another enemy, not one the game never reports) - its brain ends it in
  the game's next frame: no death animation, no dissolve. A field write; no game call. Simulated (bodies 0.25 s):
  s1200-1209 clear in 5-6 s, No Mercy's in 10 s (the simulation's 30 sends a second its own cap).
Checks: discover 543 (the senders' waits as above; the end step found in uEmIntel's vtable, its byte +0x3B30; at the
pause a body left, on a body whose kill counted ended once, not an unreported one, a live one or a J'avo's part; the
game's own end step run in the copy over a made-up brain ends it - the brain's and its mRno 3 - and not a live one;
off, a body left to its own death), panel 132, boot 18, memlog 32, crash 26; 135 signatures, 0 bad. The game was
running at the build: installed 20:53, as it closed (cmp OK).
- To look for: `merc: a stage's sender's wait (its test exe+0x959611; ...)` and `merc: a body's end: the enemies'
  brains end one whose byte +0x3B30 is set (uEmIntel's exe+0x542B10, ...)` at the start; in a match `merc: uEm1200
  ... dead (its kill counted): its body ended at once ...` at the first kill, and at its end `... N bodies ended as
  their kills counted` (a line per enemy type: `... N bodies ended`). With the switch on, enemies vanish as they die
  and the stage's next come at once - as many at once as it allows. Watch a J'avo whose arm or head has mutated as it
  vanishes (its parts), and anything left floating. If a short fall before a body goes would look better, say so.

### 2026-11-14 night - Steel Beast stopped at 89 kills

The user (23:1x, the 23:05 build): "on steel blast level I'm still capping out at 89 kills without more spawns
happening".
- The map: Steel Beast is s1206, the bridge - the exe's stage table names each stage, and the stage select's seven
  names come in stage order (`docs/modes.md`, "The stages"). The 22:33 and 23:07 matches' enemies were Edonia's J'avo
  (uEm1200, a paratrooper uEm9906), six at once at the start: s1206's (s1201, the other uEm1200 map, sends 3 first).
- **The cause**: a stage goes from wave to wave by kills, and each wave stocks only its own groups, group by group.
  Steel Beast's sixth wave (groups 5-10) gives way to the seventh at 78 of its groups 0-6 and 8 dead; with every enemy
  killed as it comes none is alive in play then, so half of group 8 is still unsent - and the seventh (groups 6 and 9)
  waits for 90 of groups 0-6, 8 and 9: 78 + its own 11 = 89, forever. In normal play up to 9 enemies are alive as the
  sixth wave ends, so group 8 has gone out by then (`docs/modes.md`, "A wave's leftovers").
- Checked with a simulation of each stage's script, each enemy killed as it comes (`tools/merc_sim.py`): s1206 stops
  at 89 in its seventh wave (the user's number), and with the fix reaches 152 (150 clears it); every other stage
  (s1200-1219) clears with or without it - the waves of the others stock what the ones before them left, or their
  counts leave room; No Mercy's stock every earlier group and count the stage's every kill.
- Built (`merc.cpp`, while Kill enemies as they appear acts): EnemyAdd's call of its stock builder (0xD593F5 ->
  0xD46150) re-pointed - a wave's groups widened for its stock's build with the groups of every wave before it in its
  script this match, its own field put back after. The builder stocks only unsent entries (nothing twice); the
  stage's minimums, waits and counts stand. Installed 23:22; then, with the boss scripts found (Em_BossPop_*, on
  other maps: an EnemyAdd of a boss's group of their own), each script's groups kept apart (by its cFSMOrder) so a
  boss's group never reaches the spawner - installed 23:28. Also: with the switch on and the stage's kills standing
  20 s, the log says so once, with what is in play.
- Not a new kind of game call: a call re-point that calls on to the game's own function with the game's arguments
  (the param's groups widened for that one call: a field write).
Checks: discover 528 (the builder's call and its param's fields found; on, the mod's; the first sender its own
groups, a boss's script its own, the spawner's next with group 0 too, not the boss's, its field put back; a list of
its own as it is; off, the game's call and a sender's own groups), panel 132, boot 18, memlog 32, crash 26; 133
signatures, 0 bad. Installed 23:28, and at 23:36 with the panel's text of it (cmp OK).
- To look for: `merc: a stage's sender stocks its groups (exe+0xD593F5 -> exe+0xD46150; ...)` at the start; on Steel
  Beast at the 78th kill `merc: the stage's next sender (groups 0x240) also stocks the groups of the ones before it in
  its script (...)`, and the kills going on past 89 to the clear. A `merc: the stage has sent no enemy for 20 s ...`
  line names what is in play if a stage stalls again.
- In play (the user, 2026-11-15): "it's working now".

### 2026-11-14 night - waits between the Mercenaries' enemies; none until the player moves

The user (22:2x, a J'avo map, after the fix below): "sometimes there are large waits before more enemies spawn. See
if you can keep the flow coming out faster or let me know what is impeding it", then "it also seems like at times
enemies stop spawning unless I move around. We've had this issue on prior RE games as well, on RE4 and RE5".
- Read: the stages' scripts (an XFS reader, now `tools/fsm_dump.py`), the EnemyAdd order's code, sEnemy's count and
  the enemies' erase actions (`docs/modes.md`, "How a stage sends its enemies").
- **The waits**: a stage sends one more enemy while sEnemy holds fewer than its minimum (3 to 13) set-placed models -
  and a body is one until its erase has run, ~10 s (a J'avo's death pose, then 7.5 s; a zombie's 70 frames, then 8
  s). Killed as they come, the enemies filled the places with bodies and the stage waited.
- **Until the player moves**: s1200, s1201 and s1204 send by the player's area (em_add_check's AreaHit -> RSC[0], the
  sender of that area's groups, re-chosen every 2 s): at s1200's far end of the platform there is no sender at all,
  and its ticket gates' groups run out at the 40 kills that move the stage on.
- Built first (22:54): EnemyAdd's count call re-pointed - the bodies of the enemies in play left out, until 32 were
  about. The user then: "instead of increasing the on map enemy limits, would it be better to fast-disolve the bodies
  instead?" - my answer: yes (the stage's own limits kept, the same flow without bursts of 20-30 or bodies piling up,
  less load), done per family (34 erase actions; the J'avo, zombies, dogs, Napad and the Whopper are nearly every
  map's enemies) by shortening each one's own waits so the whole death still plays - "go ahead".
- Built (23:05, `merc.cpp`, while Kill enemies as they appear acts): the 12 loads of those five families' erase waits
  (7.0 s, 70 frames + 8.0 s, 2.0 + 5.0 s, 420/300 frames, 2.0 + 3.0 + 7.0 s) re-pointed at the mod's 0.25 s / 8 frames
  - verified patches, found in each erase action's own code by its class; the count re-point removed. AreaHit's area
  test re-pointed - the player is in every area, so the stage sends from its first area, whose groups carry it past
  its area phase on all three stages. All put back as it stops acting (132 signatures, 0 bad). Also from the 22:21
  log's noise: a J'avo's mutation part (uEm2xxx) whose hit its J'avo took is the J'avo's - not hit again (nor counted
  in play) while the J'avo is about.
- Not a new kind of game call: verified byte patches (a constant's address in 12 loads) and one call re-point that
  calls on to the game's own function with the game's arguments.
Checks: discover 520 (the 12 loads found where the build has them; on, each the mod's short wait; off, each the game's
own again; the area test every area while on, the game's own off; a J'avo's part), panel 132, boot 18, memlog 32,
crash 26. Installed 23:05 (cmp OK).
- To look for: `merc: 6 erase actions' waits found (12 loads ...)` and `merc: AreaHit's area test ...` at the start;
  in a match the bodies of J'avo, zombies, dogs, Napad and Whoppers gone within a second or two of their death pose,
  the next enemies coming at once, and on s1200/s1201/s1204 coming wherever the player stands.

### 2026-11-14 - Kill enemies as they appear missed enemies

The user (21:4x, the Mercenaries): "it seems some enemies aren't being killed or are being missed somehow. Sometimes
it's a regular enemy that seems to get skipped, while other times it seems a certain enemy type is not killed".
- The log: four matches from 21:27. The 21:27 one a zombie map (uEm5200 hit at the pause); only the session's first 12
  hit lines were logged, then a count a match: 316 hits (3 min), **2932** (21:31-21:39), 300, ... One hit kills on
  throughout, and no `keeps its life` line: the enemies that took hits and lived were not ones the game keeps alive.
- **Skipped**: the tick tried again only an enemy a first hit had been written into. As the switch went on, the sweep
  wrote four a tick - the fifth and later enemies in play (a match's first wave, at its first pause) were never tried
  at all; and an enemy that could not be hit the moment it started (or at the sweep: its record busy with a hit of the
  game's, its first action not set yet - its brain drops a hit then) was never tried again. The hit written at an
  enemy's start was cleared at the next tick unread too (the enemies' line runs after their brains' line).
- **A type not killed**: an enemy works a hit out by the part of it the hit landed on (the record's +0x64, copied to
  uEnemy +0x2B04 by the handler; +0x60 the collision) - the mod's lands on part 0 - and some classes shrink or zero
  it: Gnezdo (uEm3300) takes 1% (under 100 nothing, so no One hit kills either), Napad's (uEm3200) own code rates
  each region of its armour, the zombies' (0x7D4080) and J'avo's (0x66FC20) limbs and states (x0.3, x0.1, 0), the
  Whopper's (0x800940) regions; a state that takes nothing. Rasklapanje's subtract-health never goes below 1
  (`docs/cheats.md`): its health never ends it.
- The Mercenaries' enemies, from the stages' set lists (`soft\stage\s12xx\s12xx_em*`, 2026-11-14): J'avo maps -
  uEm1000/1200/1400 (and their chrysalid uEm1370, made by the J'avo's own SanagiChange action), Strelats uEm3000,
  Mesets 3100, Napad 3200, Gnezdo 3300, Ubistvo 4300, Rasklapanje 4700, Ogroman 4800; zombie maps - uEm5000, 5050,
  5060, 5181, 5191, 5200, 5210, Bloodshot 5400, Shrieker 5500, Whopper 5600, the dogs 5700, Lepotitsa 4100 (and a
  Napad, Strelats or Ubistvo). `docs/modes.md`, "The Mercenaries".
- Fixed (`merc.cpp`): every enemy in play is tried until it is dead (8 hits a tick at most, each its turn); one whose
  hit went unread or was dropped is tried again after 100 ms (2 s after 50), one that took it and lives on after 400
  ms (2 s after 20); nothing is written at the pause. A hit of the mod's is lethal: the enemies' take-damage (cheats'
  hook of slot 128, which now asks `merc::lethal`) takes all the health for it whatever the class worked it out as;
  after one that lived through a hit, 32767 (so a tiny rate still gives something); after three, all its health even
  from nothing; and another part each hit (the part index cycles 0..31: Rasklapanje's limbs, an armour's gaps). The
  log: the first 8 hits of a match, a type's first enemy that lives through the mod's hits or takes none of them (its
  health, state, flags, what its code worked the hit out as), and at a match's end a line per type - in play, killed
  by the mod, died otherwise, gone, still in play, hits written, taken and lived on, not taken.
- Not a new kind of game call: field writes and the existing take-damage hook (the amount handed on changed).
Checks: discover 508 (the old Kill enemies checks replaced: no hit at an enemy's start, the next tick's; unread cleared
and written again after 100 ms; its take-damage - the mod's hit worked out as 30 takes all 500, as 0 left so at first,
a game's hit as it is; a shrug, then 32767, then all its health from nothing, part 1; its death counted; through the
game's own take-damage (uEm5200's, 0x95D320) the mod's hit worked out as 30 kills it, a game's takes 30; ten enemies
as the switch goes on - eight a tick, the ninth at the next, a busy one once free; the pause), panel 132, boot 18,
memlog 32, crash 26. Built 22:13; the game was running - installed 22:19 as it closed (cmp OK).
- To look for in play: `merc: Kill enemies as they appear: every enemy in play tried until it is dead ...` at the
  start; in a match `merc: a hit of ... written into ...`; any `merc: <class> ... took a hit of the mod's and lives on`,
  `... lives on through the mod's hits (...)`, `... has taken none of the mod's last hits`; at its end `merc: the
  match: N enemies killed by the mod's hits ...` and the lines by type - a type with "still in play" or "taken and
  lived on" is one left standing.

### 2026-11-14 - the chapters window showed the old times after Apply

The user (21:2x): "after hitting apply to change the clear times the imgui window reverts back to showing the old
times, however the game is correctly updated. The imgui window should show the updated values after applying".
- The log: two Applies of 175 best times ("Everyone's" with a time typed: 21:19:40 and 21:19:53, `records: chapters: 0
  cleared, 0 taken back, 175 best time(s) set`), then `the profile saved (2080 frames)`. The save (written in play,
  21:22) holds 0.0 in all 175 records: the time typed was 0:00:00.
- Cause: the window took a time of 0 for the game's "none" and showed 19:59:59 (the old time of 155 of them), where
  Chapter Select shows 0.0 as "0:00:00". The 20 records cleared in play under Freeze play time (the clock held at 0)
  had shown 19:59:59 in the window from the start. Also, Chapter Select cuts a time to whole seconds (`fistp`, the FPU
  set to truncate) and the window rounded it (1234.7 s: 0:20:34 there, 0:20:35 here).
- Fixed: a cleared record's time is shown as Chapter Select shows it - cut to whole seconds, at most 71999, 0 as
  0:00:00. The "none" is shown only under a chapter that Apply all is to clear and that holds no time, which is what
  apply_chapters gives it. The view keeps the record's float as it is.
Checks: discover 485 (a time of 0 set: the game's get reads 0, the view has it cleared in 0), panel 132 (after Apply,
with the view as the tick leaves it: Leon's fields show 0:30:00, then everyone's 0:00:00 - not 19:59:59, the check
failing on the code before; a time of 1234.7 s shows 0:20:34), boot 18, memlog 32, crash 26.

### 2026-11-14 - chapter times; Add without weapons; "two Wing Shooters"

The user (19:2x): "1) For setting ranks on chapters, it currently defaults to 19:59 on the time for newly completed
levels. Allow the user instead to set the clear times as well. 2) ... Remove the weapons altogether [from Add] as they
can now do that through the weapons imgui panel instead, and only allow ammo for the weapons the player currently has.
3) Finally, why does Leon have 2 Wing Shooters in his inventory? Is that a bug?"
- **The time**: a chapter record's +8 is its best time (a float of seconds), shown only on Chapter Select (0x9C0E50:
  cut to whole seconds, held at 71999, `%2d:%02d:%02d` - 71999 is "19:59:59", the game's "none", which the mod gave every first
  clear). The user's save had 155 records at 71999 and 20 at 0.0 (flags 7; "0:00:00"). Built: under each chapter
  that is (or will be) cleared, its time (h:mm:ss; m:ss or seconds typed work too), and a time beside the "Every
  chapter" rank for a character or everyone; whole seconds, held at the game's most; a first clear with none typed
  still gets 19:59:59. The game's clear keeps the better time (0x563CA0), so a set time stands until a real clear
  beats it. `docs/records.md`, "The chapter records".
- **Add**: no Weapons tab (no weapon offered, even in a search); the Ammo tab lists only what the character's own
  weapons in use load - the game's (weapon, mode) -> ammo function 0x574E90, transcribed into items.h by
  `tools/gen_items.py` (the Grenade Launcher's three rounds, the Bear Commander's 5.56mm and 40mm, the Crossbow's two
  arrows, ...); the tick checks the live inventory again ("Nothing Leon holds takes 12-Gauge Shells."). Grenades, the
  spray and herbs are offered as before (the loader still loads a grenade or the spray first). The log showed the
  user's last weapon Add (19:11: the Crossbow for Leon, "no free weapon slot").
- **Two Wing Shooters**: not a bug - one gun. The game keeps a count for each of its two modes, which its inventory
  screen calls "One Gun" (mode 0, 18 rounds) and "Two Guns" (mode 1, 36: one in each hand) - its Mode Change reads
  them from a table (0x159AE48: weapon, mode, icon, mes_system 1667/1666). The panel lists one row a count, and
  labelled the Wing Shooter's "(1)" and "(2)" (gen_items.py had no names for its modes, which load the same 9mm). The
  user's save: one Wing Shooter (slot 0, its two counts 18 and 18). The rows now read "Wing Shooter (One Gun)" and
  "Wing Shooter (Two Guns)". `docs/inventory.md`, "The modes' names".
Checks: discover 484 (a time set read by the game's own get 0xE44250, held at the most, a first clear's; Add's ammo
rule and no weapon; the loader's checks moved from weapons to grenades and the spray), panel 129 (the time field as a
player types it, the bulk time, an invalid time let go; the Ammo tab by Leon's weapons, no Weapons tab, Helena offered
no ammo; the One Gun / Two Guns labels), boot 18, memlog 32, crash 26. Log: `records: chapters: ... n best time(s)
set`; `inventory: 12-Gauge Shells not added to Leon: no weapon Leon holds loads it`. Installed 19:41 (the game closed;
cmp OK).

### 2026-11-14 - the enemies window (the Records ENEMIES page)

The user: "for the enemies page, have an imgui window to reveal all enemies (revealing should set kill count to 1 if
less than 1)". Read in the game's code (`docs/records.md`, "The enemies"): the ENEMIES page (the Records screen's page 2)
names a row when its record's always-named byte is set (J'avo and Zombie only) or its Enemies Defeated or Times
Defeated count is above 0, and draws "? ? ?" otherwise; the counts are cSystemInfo's (+0x1328 / +0x132C, 8 bytes a
row), read live each frame. The user's save (19:00) had 12 rows "? ? ?" (Glava-Begunats, Glava-Dim, Noga-Oklop,
Telo-Krljusht, Telo-Magla, Zombie - Crank, Zombie - Liquid Nitrogen Tank, Crow, Spider, Snake, The Mini Tank, The
Witch) and no row named with no kill.
- Built: the enemies window beside the ENEMIES page - the page's 84 rows that can be "? ? ?" (the two always named not
  listed), each with Enemies Defeated (and Times Defeated, where the page shows it) and a tick; Reveal all; a "? ? ?"
  filter. A reveal makes a "? ? ?" row's Enemies Defeated (below 1) 1: the page names it at once. A row named already
  keeps its counts (one named by Times Defeated alone gets no kill: it is revealed already), and nothing is taken back
  (a boss's one real kill and the mod's look the same). The profile is saved through the menus' own steps after each
  change. Two new signatures (kSigRecordEnemyRows, kSigRecordEnemyDefeats: 131 in all), the page's lookup and gets
  matched by the weapons window's patterns; the names (mes_file 3700 + row) and "Times Defeated" by
  `tools/gen_records.py`. No game call beyond the save's steps.
- Side effect, noted in `docs/records.md`: a reveal can add one to J'avo Killer's count (Neo-Umbrella J'avo) and up to
  five to Zombie Hunter's (Bloodshot, Shrieker, the Whoppers, Zombie Dog C) - the skills' requirements read these
  rows; the user's counts are far past both goals already.
Checks: discover 477 (the page's own lookup, gets and test run in the copy see each reveal; a reveal asked with another
page up is dropped), panel 118, boot 18, memlog 32, crash 26. Log: `records: the ENEMIES page (the Records screen's
page 2, exe+0x5E4E20): 86 rows ...`, `records: understood - ... enemies yes`; on the page `records: screen: the Records
enemies`, a reveal `records: the ENEMIES page: n row(s) named (their first kill; ...)`, then `the profile saved`. The
game was running at the build: the tested DLL (sha 5923e9a5...) was installed as it closed, 19:13 (cmp OK).

### 2026-11-14 - the weapons window (the Records WEAPONS page and each character's saved weapons)

First a question: "how does the game track weapons? Do you keep it once you've found it for the next time you do
chapter select missions?" - read in the game's code (`docs/inventory.md`, "Carried over"): each character's saved
inventory (cGameInfo +0x20D8), written at each checkpoint and a chapter's end, is what Chapter Select starts from: a
weapon found stays, in any chapter, on any difficulty; only New Game on a completed campaign asks "Start a new game
without the weapons you've obtained?". Then: "on the weapons screen under records menu, add an imgui window to toggle
having acquired weapons, which should also remove the "???" entries for missing ones". The WEAPONS page has no
"acquired" flag: it names a row once the game has counted a use of it (Times Used above 0) or when its record says
always (each character's first weapons, the tablets, the spray) - `docs/records.md`, "The weapons". The user: "I think
you're wrong. I have weapons that show up with 0 usages" - their save (17:43) showed those were the always-named ones
(Picador, Hydra, Hand Grenade, Survival Knife, First Aid Spray at 0 uses), while Leon's saved Lightning Hawk and
Piers' Grenade Launcher were "? ? ?" (never fired). Asked what a tick should change, they chose "Page + campaign
inventory"; then asked "how many weapons are shared between characters ... and how would we see that in the UI page
to be able to ensure all characters have their weapons?" - all 9 weapons the campaigns give on the way are shared
(28 character-weapon pairs): a grid.
- Built: the weapons window beside the WEAPONS page. A grid of the 9 weapons (a row each: its page tick, a tick for each
  character whose campaign has it, All), a column a character (its count had, All), Give every weapon; below, the
  page's other 19 rows that can be "? ? ?" (a page tick each), Name every row. A page tick gives a row its first use
  (Times Used 1) or takes it back (not a row played: more than one use, or a kill); a character's tick puts the weapon
  in its saved inventory (a full magazine) or takes it out (the hand moved off it). The profile is saved through the
  menus' own steps after each change - run for a change to the cGameInfo alone though the game's "changed?" says no.
  Five new signatures (kSigRecordWeaponRows, kSigRecordWeaponKills, kSigWeaponEntry, kSigCarriedReset,
  kSigCarriedDue: 129 in all) and three patterns of the getters they call; no game call more than the save's steps.
- The user's save at 17:43, for the first look: Leon, Piers and Ada had all their weapons; Helena, Chris and the Agent
  none; Jake not his Elephant Killer and Shotgun; Sherry not her Bear Commander. The page's "? ? ?": the Lightning
  Hawk, Grenade Launcher, Incendiary and Flash Grenades and seven hand-to-hand moves.
Checks: discover 466 (the game's own New Game reset sets up the eight saved inventories in the copy; the page's own
reads and the game's lookup and getters see every change), panel 105, boot 18, memlog 32, crash 26. Log: `records: the
WEAPONS page (the Records screen's page 1, exe+0x5E4660): 43 rows ...`, `records: the saved inventories: cGameInfo
+0x20D8 ...`, `records: understood - ... weapons yes (saved inventories yes)`; on the page `records: screen: the Records
weapons`, a change `records: the WEAPONS page: n row(s) named ...` or `records: the saved inventories: n weapon(s) put
in ...`, then `the profile saved`.

### 2026-11-14 - Infinite stamina; Ada's helicopter: its missiles and God mode

The user: "add a cheat for infinite stamina". While it was built, from Ada's campaign (the save of 15:51: episode 3,
chapter 4 - her fifth -, stage 706, scene 10; the log's Simmons, uEm6300 and uEm6040, at 15:52): "infinite ammo should
also work for helicopter rockets", then "god mode also doesn't work for the helicopter". Read in the game's own code
(`docs/cheats.md`, "The combat gauge", "The rides"):
- Stamina is the combat gauge (the air under water's float): 300 below its blocks, 100 a block (800, 1100, 1300 by
  Combat Gauge Boost). A move with a cost (sPlayer's action table: melee, a quick shot) is refused while the blocks
  held are short, else paid through uPlPlayer's spend (slot 123) - which already takes nothing under the Training
  skill. The frame's refill goes through the add (slot 122, Infinite air's hook). Built: Infinite stamina
  (`InfiniteStamina` in the ini; the panel's Characters heading, after Partner takes no damage): the spend hooked in
  51 vtables takes nothing from a character this machine drives, and on land any add fills the gauge to its most (a
  gauge the game empties as you go down is full at the next refills once you are up). Under water it stays Infinite
  air's. Left to the game: the AI's partner, the network's, a scene's script turning the gauge off, Piers' mutated arm
  (no gauge at all), Agent Hunt's creatures (their own gauge).
- The helicopter is uSm5060 (`sm5060_Ada_Heli`: stages 701, 703, 706, 751, 771, 773), a players' aircraft of the
  Harrier jet's class (uSmPlHarrier) - a set model, not a character: neither switch's hooks saw it. Its missiles are a
  count of its own (mMissileNum, 6 from its setup, refilled by nothing), not an item: the infinite-ammo bit never
  reaches them. Its health (1500) goes through uSmRide's subtract-health (slot 44), which every ride shares.
- Built: Infinite ammo now NOPs the missile action's two takes while on (6 stay 6), and as it goes on gives a
  helicopter in play its 6 back; God mode (and Partner takes no damage) hooks every ride's slot 44 (27 vtables): a
  ride a spared character rides, in either seat, takes nothing, and as the switch goes on is repaired. Enemies' rides
  take their damage. Five new signatures (kSigGaugeSpend, kSigMissileLaunch, kSigMissileLoad, kSigRideSubLife,
  kSigRideOn: 124 in all); no new game call - two hooks, two verified byte patches, field writes at the switches'
  events.
Checks: discover 449 (the game's own spend, add and frame's refill run in the copy: off a block taken and 300 or
less emptied, on nothing taken and filled on land, under water left to Infinite air; the game's own missile launch on
a made-up helicopter: off 6 to 5 and dry at 0, on none taken, the load given back as the switch goes on, Chris's jet
none; the game's own subtract-health of the helicopter, a bike and an enemy's jeep: yours spared, the partner's with
its switch, the network's and the enemy's not; the repair as God mode goes on), panel 91, boot 18, memlog 32, crash
26. Installed when the game closed (it ran throughout). Log: `cheats: Infinite stamina: the combat gauge's spend
(slot 123, exe+0x6CE4C0; ...) hooked in 51 vtables ...`, `cheats: Infinite ammo: uSm5060's missiles (+0x41C0, ...)
- the missile action's two takes (exe+0x781475, exe+0x78147E) NOPs while on`, `cheats: God mode: the rides'
subtract-health (slot 44; uSmRide's exe+0x7B3DF0, ...) hooked in 27 vtables ...`; in play `... Leon's combat gauge
(n of m) - a spend of 1 block(s) not taken`, `... uSm5060 <addr> - its missiles (a, b) back to 6`, `... uSm5060 <addr>
ridden by a spared character - its health n filled up to m`.

### 2026-11-13 - One hit kills and Haos (Chris's chapter 5): the chase could not be ended

The user, at 16:15:45: "I think the one hit kill cheat is broken for the Haos boss fight in 5-3 with chris and piers.
The enemy never shows the waist blisters for me to shoot to make it go into the cocoon". Where they were: the log's
Haos then was uEm8001 with mIsNoDead (15:57-16:15, retried six times) - only stage 972 places uEm8001: em00, the
climb (mSetType 0, mIsNoDead) and em01, the chase (1). The waist blisters and the cocoon are the last fight's (s0973,
mode 3): every other mode's setup hides the blisters (0x8DAC60: hit groups 2 and 3 off). After the report the log has
the climb ended by 16:20, the chase at 16:20:59 (held at its last point), a 3 s pause at 16:21:07, the stage ended
16:22:12; stage 903 at 16:22; then s0973 at 16:26:56-16:27:53 under One hit kills: a blister, the head and the other
blister broken (each at one hit), the next Haos (opened) 10 s later, then a whole one again, its blisters broken -
and One hit kills switched off at 16:28:01 (the ini). Read in the game's own code and the stages' scripts
(`docs/cheats.md`, "Haos"):
- The chase (mode 1) must die: `em01_set` waits for it gone or dead (EnemyCheck act 0x2A), and its death reaction is
  a death (DeadKuzure, DeadErase). The audit of 2026-11-06 had modes 1 and 2 as "its death has no script" and took
  both to their last point - so with One hit kills on, the chase could never end (the 16:21 pause most likely the
  switch going off). Fixed: mode 1 is left to its health - it dies at one hit.
- On the pipes (mode 2, s0903) stays at its last point: the stage's barriers wait on its hand's flags (RSF 80/81),
  which its death would end. Mode 4 dies at one hit (RSF 310 is what s0903 and s0973 wait on), as before.
- The last fight under One hit kills: each blister, the head's down, the cocoon and the hearts' parts at one hit; the
  hearts are taken by a melee (the opened Haos' catch points), as without the mod. Nothing found holding it up. In
  the log the opened Haos (16:27:13) had its head and a heart shot, its hearts not all taken, and a whole one followed
  (16:27:45). The catch's condition (0x952600) needs its health at 1 or more (mIsNoDead keeps 1) and reads no part.
- The rest of the run: no One hit kills line after 16:28 (the fight played on with it off); the game ended 16:41:15
  and the next started 16:51:21; the 16:54 save is episode 3's chapter 5 (s1140). The game closed at 18:08.
- The climb (mode 0): its knock-back now comes at any hit in its block or a side move's end, and writes the stage's
  scene each time - noted [I].
Checks: discover 399 (the chase dies, the pipes kept and staying kept, the climb's point kept with the big hit), panel
90, boot 18. Installed at 18:08, when the game closed (it ran until then). Log: `cheats: One hit kills: uEm8000's damage worked out (exe+0x4DDE80): in mode 2 (+0x41C4; on the
pipes) the stage moves on through its hand - taken to its last point; ...`, and in s0903 `... keeps its life (Haos on
the pipes: the stage moves on through its hand, not its death)`; the chase has no line now (it dies).

### 2026-11-13 - a cutscene showed a blank screen (Proton's movie converter, not the mod)

The user, in the 15:21 run (stage 851, Chris's chapter 4): "one of the cutscenes couldn't load and showed a blank
screen instead". The mod's log has no error. Proton's own traces in the game's shader cache
(`steamapps/shadercache/221040`):
- `fozmediav1/video.foz` was created 15:26:57. It is the movie converter's dump of a movie it had no transcode for:
  9 chunks of 8 MiB, each with the hash of the same bytes of `nativePC/movie/e5120.wmv` (75.6 s), under the stream
  key c8080aea74962150c331b46f2334061e.
- `placeholder-video-used` was touched 15:27:53 (the converter's mark when it plays its blank video). The mod's
  `game ended` followed at 15:27:54.
Valve's `transcoded_video.foz` (downloaded 2026-11-12) has a 75.6 s Theora transcode of e5120, but under
b73e3e45834ae8a819c43a104ea2114b. The converter's source (Proton's wine, `media-converter/videoconv.c`) explains the
two keys: an x86 hash for a 32-bit unix side, x64 under WoW64. The user's `PROTON_USE_WOW64=1` (the memory room,
2026-11-05) makes it x64. Both keys of all seven movies were worked out (`docs/engine.md`, "Proton: the game's
movies"): Valve's file has e5120, projector (Ada's stage 1000) and the BH6 logo and title under x86 only; e5110 and
the RE6 logo and title under both.
Fixed outside the mod: `tools/proton_videos.py --fix` (15:44) added the four x64 keys to Steam's file, each as a
second name for Valve's own transcode (the same header and data). Checked on a copy first: the first 236,202,650
bytes unchanged, the added entries byte-identical to Valve's, e5120's decoding as 75.6 s of Theora 1280x720, a second
run adding nothing. The file is now 342,015,410 bytes; `truncate -s 236202650` undoes it. Unseen in play: the next
e5120 should play (Leon's event e1210 uses it too), with no new `placeholder-video-used`. Steam may download the file
again; run the tool again then. RE5's cache has `placeholder-video-used` too (2026-10-29), not looked into.
The same run's log has the countdown fix's line (below): `cheats: a countdown on screen shows its FSM's own timer 0
(cFSMOrder 19408B70)` at 15:23:40, and again at 15:26:40 with a new cFSMOrder, after the log's `game ended` and
`game started` at 15:26:06-07.

### 2026-11-13 - still laggy: what was checked, the frame log built

The user: "the game is still very laggy. This is very abnormal for my machine to struggle with this game. It's laggy
when loading a save into the game and remains laggy while playing it". Checked, without the game:
- The mod's log of the 09:37 run: no error; at the title 64-120 presents a second (the 20:09 run's 26-98) - gamescope
  holds the game to 120 (`-r 120`). The mod's work a frame with the panel hidden: Present takes the back buffer and
  lets it go; the input hooks a table lookup; the window procedure hands every message to ImGui, whose input queue
  empties only when the panel draws - but the memory log of 2026-11-12's run has the mod's heap (ImGui's allocations
  counted in it) flat at 835 KB through 12 minutes of play: no growth.
- The machine: i9-11900K (intel_pstate, EPP performance, turbo on), 64 GB (21 GB in zram swap, 23 GB free), RTX 3090
  (615.78.08). The launch: `PROTON_USE_WOW64=1 __NV_DISABLE_EXPLICIT_SYNC=1 GAMESCOPE_ARGS="-w 1920 -h 1080 -W 1600 -H
  900 -r 120 --force-windows-fullscreen -S fit --force-grab-cursor" ~/.local/bin/gamescope-launch %command%`, Proton
  Experimental 11.0-100, DXVK 3.2.0 (gamescope's WSI line: engine version 12591104).
- RE6's shader caches (`steamapps/shadercache/221040`, 16 GB): Steam's two downloaded Fossilize caches (shader
  pre-caching, other players' recordings - both in config.vdf's shader depot entry for 221040): the one for this
  GPU's driver (`fozpipelinesv6/steamapprun_pipeline_cache.923fa87291dfec1c/steam_pipeline_cache.foz`, 3.55 GB, dated
  2026-11-09, 7 DXVK versions 3.0.0 to 3.2.0) holds 312,765 shader modules and 809,794 graphics pipelines, the general
  one (13 recordings, DXVK 1.10.3 to 3.1.1) 542,501 and 971,678. (Corrected 2026-11-13 afternoon: the first was taken
  for the user's own recording that morning; the user's own is `steamapp_pipeline_cache.foz` there, 211 MB, 32,564
  pipelines, DXVK 3.1.1.) NVIDIA's cache 4.3 GB, 664,511 entries after the 09:37 run, 11,483 more than after the run
  before (a two-minute run). So RE6 under DXVK makes pipelines by the hundred thousand (every player's recording too:
  not the mod's doing), and a new DXVK version starts the compiling over. Steam's own processing (fossilize_replay)
  was not running during the 09:37 run.
- gamescope's logs: RE6's three runs have `vkImportSemaphoreFdKHR failed`, `SyncobjFdToHandle failed` and `Importing
  a dma-buf sync file failed` (09:37's also `vkAllocateMemory failed (VkResult: -2)`), each once at start; Baby
  Steps' runs under the same wrapper (without RE6's `PROTON_USE_WOW64`/`__NV_DISABLE_EXPLICIT_SYNC`) have none.
- The journal: Discord's game detection and its Clips were up during the run; no kernel or GPU error.
Built: the frame log (`FrameLog = 1`, `src/framelog.cpp`, `docs/engine.md` "The frame log") - a line a second on
where the frames' time goes (the game's frame, Present, the draw thread, the mod's own, the textures, buffers and
shaders the game made), a frame over 100 ms a line of its own. Checks: built (no warnings); memlog 32 (9 new), boot 18
(the ini's key), discover 380, panel 90, crash 26. Installed 10:02:49 (cmp OK, 4641823 bytes); the user's ini has
`FrameLog = 1` for the next run (set it back to 0 after). Beside it, a sampler outside the mod (the session's
scratchpad, `/proc` only) records the game's threads' CPU, wineserver's, gamescope's and the busiest other programs
each second. The lines to look for: `dx9: FrameLog - the device's calls that make ... hooked` at the start, then
`frame: <screen> | N frames in 1.00 s ...` each second and `frame: a N ms frame (...)` at each long one.

The run after (10:40, FrameLog on, the sampler beside it). The user: "it's running fine this session". What the log
and the sampler showed (49 minutes read, the game still running):
- 120 frames a second (gamescope's cap) almost every second, at the title, paused and in play; ~8 frames a minute
  of 17-26 ms in play, no pattern. The mod's own time a frame 0.03 ms in play (the longest 4.8), 0.3 ms paused with
  the panel up.
- Every stall was DXVK compiling what the game met for the first time:
  - Loads. The game makes its shaders at a load (CreateVertexShader/CreatePixelShader: 7,952 at start, 3,960 at the
    first save loaded); each call waits for DXVK's compile. 10:54's load: 3,960 shaders in 0.8 s (compiled before).
    11:05's: 3,786 in 10.5 s - one 10.5 s frame, DXVK's eight compile threads (4 `dxvk-shader-n`, 4
    `dxvk-shader-l`) at 100% for 11 s, the game at 880% CPU. 11:27's: 2,839 in 3.7 s.
  - Play. 11:25:09-19: 54 frames over 100 ms (down to 7 a second), each the main thread blocked in Present 95-460 ms
    (the game's frame 3-9 ms) while DXVK's command thread (`dxvk-cs`) ran at 97-99% and the four low-priority
    compile threads at 20-60%; the main thread 2-3%. 11:07 and 11:10 the same, smaller (Present 100-370 ms).
  - This run's NVIDIA cache file (`steamapp_shader_cache3.bin`) grew to 154 MB; Steam's recording of it 8,125 new
    pipelines.
- Why the runs before were laggy: gamescope's log records the DXVK version at each start (`[Gamescope WSI]
  Application info`, `engineVersion`). RE6's runs of 2026-11-06 had 12587009 (DXVK 3.1.1); from 2026-11-12 18:44 on,
  12591104 (3.2.0: Proton Experimental's bundled `v3.1.1-188-gcf0a0d90`, updated by Steam 2026-11-10 09:51, the
  prefix upgraded at that 18:44 start). RE6 was not run between 2026-11-06 21:49 and 2026-11-12 18:44 (Steam's
  `compat_log.txt`). With a new DXVK every pipeline is compiled again (NVIDIA's cache keys on DXVK's output): the
  18:44, 20:09 and 09:37 runs compiled what they met (09:37 added 11,483 cache entries in two minutes); by 10:40 the
  ground already played was compiled, and only new ground stalled. No driver or kernel change since 2026-11-05
  (`/var/log/pacman.log`); the gamescope errors are in the good run's log too; docker's `containerd`/`dockerd` take
  ~3.4 cores for a second every 5 minutes (xx:x0:07, xx:x5:07), Chromium a core on average - neither lined up with a
  stall.
- The frame log's own bug, found in this run: the long-frame lines printed garbage for the screen (`frame: a 326.5
  ms frame (<binary>)`, and grep then took the log for a binary file). `framelog::tick` kept the pointer to the
  tick's own stack buffer and `begin()` read it after it was gone. Fixed: tick copies the text; test_memlog's long
  frame now gets its screen from a buffer overwritten after tick (the check fails on the old code, passes on the new).
  Checks: discover 380, panel 90, boot 18, memlog 32, crash 26 - all passing; built with no warnings.
- The run ended 13:55:14 (3 h 13 min; the sampler's totals: the game's main thread 38% of a core on average,
  `dxvk-cs` 24%, `dxvk-frame` 20%). The fix installed with the countdown's (below) at 14:20 (cmp OK); `FrameLog` set
  back to 0 in the user's ini.

### 2026-11-13 - Freeze countdown timer did not hold stage 851's countdown

The user, in Chris's campaign (the save at 13:54: episode 1, chapter 3 - the fourth -, stage 851): "The freeze
countdown timer cheat isn't working for the area I'm currently on in game, in chapter 2-4 chris and piers' section".
The mod's log of that run (10:40-13:55, the switch on from the start): `cheats: a countdown on screen shows the
stage's timer N` only at 10:57-11:04 (s0302's bombs: timers 0-2 the gauges, 3 the 8 s countdowns), none after.
Stage 851's scripts (`s0851Soft.arc`: `SN_140_countdown`, 237 s then 240 s, and `SN_220_countdown`, 40 s - each a
Game Over at 0) start their countdown on the FSM's own timer 0 and show it with the CountDown order's timer kind 0:
the order reads `this +0x930 + n*4` in place, no call - and the mod saw only the stage get's calls (kind 1). Every
stage script scanned (`docs/modes.md`, "The countdowns"): the own kind is shown only by s0851's (and s0301's gauge);
s1003's countdown counts up (2301 -> 2508, a Game Over there), which the hold (counting down only) left to run.
Fixed: the CountDown's in-place read (kSigCountDownOwn, kSigCountDown +137) rewritten into a call of the mod's own
read (19 bytes: the number pushed, the FSM as `this`, the float stored as the stage read beside it does); the FSM's
timers' words read out of the loop that runs them (kSigOwnTimers: +0x930, running +0xA4, counting down +0xA8,
cross-checked with the read and cFSMOrder's size). The hold covers both kinds, the CountDown's whichever way it runs
(a gauge's only counting down - every gauge counts a partner's progress up), time given back taken up, a hold lapsing
after 30 frames unread; the editor sets an FSM's own countdown too. A verified byte patch and the mod's own read: no
game call. Two signatures (119 now). Checks: discover 397 (17 new: the game's own Timer order, its own timers' loop and
its own CountDown order run in the copy over a made-up cFSMOrder and stage display - the 7 own-timer ones fail with
the read left as the game has it), panel 90, boot 18, memlog 32, crash 26; built with no warnings. Installed 14:20:27
(cmp OK). The log lines: `game: an FSM's 32 own timers +0x930 (running +0xA4, counting down +0xA8; cFSMOrder, 2596
bytes); the CountDown's read of one exe+0x93F929`, `cheats: Freeze countdown timer: ... an FSM's own found`, then in
play `cheats: a countdown on screen shows its FSM's own timer 0 (cFSMOrder ...)`.
Note for play: a countdown whose end is the way on stands still too while the switch is on - s0302's bombs (passed)
and s0901's hold-out (120 s, then 90 s; next in the stage order after 851): switch it off there.
Seen in the 15:21 run's log (the entry above): `a countdown on screen shows its FSM's own timer 0` at 15:23:40 in
stage 851. The countdown standing still is not yet confirmed by the user.

### 2026-11-12 - the cheats made event-driven (the tick no longer writes each frame)

The user, in a lag audit ("I notice a heavy spike every 5 seconds or so"): the mod's own periodic work measured tiny
(the memory log's 2-3 ms each 30 s on its own thread, DXVK's texture-memory query each 5 s an atomic read; no tight
loop; `MemoryLog` set back to 0); outside the game, a logger service running nvidia-smi each second and liquidctl
each 3 s, two stuck `mysqladmin status`. Even so the user asked for both steps offered - the patches only as their
switches change, and the per-frame holds turned into hooks: "do both". Built (`docs/engine.md`, "The tick is an
observer"):
- The patches and re-pointed calls go in and out only as what they follow changes (a mask of wants); what only the
  panel shows - the inventories, the counters, the match's numbers - is read only while it can show
  (`game::Run::panel`).
- Each hold each frame became a hook where the game does the thing: God mode - uPlayer's health add (slot 43), the
  health filled up once as the switch goes on or a character comes in; Infinite ammo - the bit set at those events
  and put back after uPlayer's reset (slot 109: a Continue, a stage's start); Infinite air - the gauge's add (slot
  122), a drain under water made a fill; Misses don't count - sInfoMan's counter add's three calls (the shots' not
  made, the hits' followed by the shots put at them); Freeze countdown timer - the hold inside the re-pointed read;
  Endless combo - the kill award's call (the earnings on the score as earned); Combo Time always on - started once;
  Kill enemies as they appear - the enemies' start and destructor (slots 5 and 0) hooked, each tried as it comes in,
  no walk over the units; the records and skill screens known the same way. `src/vthook.h`: the vtable hooks shared
  by the three modules - a hook asks a table for its first five entries whatever its slot (a destructor's is slot 0,
  and zlib's table has a first entry inside the code; the class index already refused it).
- No new kind of game call: vtable hooks and call re-points that call on to the game's own functions with the game's
  arguments, field writes. The calling conventions read from the game's code (each `ret n`; slot 109 ends in tail
  jumps to 0xA73F00 and 0xA55010, a plain `ret`).
- What changes in play: God mode no longer fills up health that a function of the game's writes past these hooks
  (none seen) until the next event; under water a refill from below 300 goes to 400, then 500, over two adds (the
  game's own rule); Kill enemies as they appear tries an enemy as it starts (before: at the next tick).
- Checks: built (no warnings), `tools/sig.py --check` 117 signatures; discover 380 (38 new: the health add, the
  drowning's add and the counter adds run in the copy through the hooks; the reset, the units' start and destructor
  and the award through stand-ins), panel 90, boot 18, memlog 23, crash 26. Installed 22:38, and again 22:41 with
  the slot-0 hardening (cmp OK, 4616674 bytes).
- The log lines to look for, at the start: `cheats: the players' damage hooked in 54 vtables (subtract-health), 51
  (hit), 54 (take-damage), 54 (the health add, slot 43)`, `cheats: Infinite ammo: ... the bit put back after the
  players' reset (slot 109, 54 vtables)`, `cheats: Infinite air: the combat gauge's add (slot 122) hooked in 52
  vtables - a drain under water made a fill`, `cheats: Misses don't count: the shots' count (exe+0x169149) and the
  hits' two (exe+0x16FA39, exe+0x11AD56) of sInfoMan's counter add (exe+0x64A90) re-pointed while on`, `units: ...
  started through slot 5, destroyed through slot 0`, `merc: the enemies' start (slot 5) and destructor (slot 0) hooked
  in 191 vtables - each enemy known as it comes and goes`, `records: the screens' start (slot 5) and destructor (slot
  0) hooked in 3 vtables - each screen known as it comes and goes`.

### 2026-11-12 - Steady aim did not steady a sniper rifle's scope

The user: "the steady aim is not working for sniper scopes" (the 20:09 run, Steady aim on; its start had `cheats:
Steady aim: the sway's way out exe+0x8C6D41, the kick's exe+0x6BB255`). The cause (`docs/cheats.md`, "Through a
scope"; it was marked [I] there): the switch zeroed the game's sway, but through a scope the view is the gun model
itself - the scope camera sets its eye at the gun's scope joint and looks down it, and the gun is in the hands of the
aim pose, an animation that breathes and kicks at a shot; the sway is only the game's swing on top. Built
(`src/cheats.cpp`, "Steady aim through a scope"; seven signatures): the scope camera's call and the gun's line's are
re-pointed while the switch is on; the view held in the body's frame while the aim's pitch and twist stand, the gun's
own motion followed on the axis that moves, the game's view while the scope comes up until the gun settles; the gun's
line (the shot's, the laser's) along the held view. No new kind of game call: two call re-points that call on to the
game's own functions, and field writes on the camera and the player they hand over. Checks: built (no warnings),
`tools/sig.py --check` 106 signatures; discover 342 (18 new), panel 90, boot 18, memlog 23, crash 26. Not installed
at first: the game was running (its 20:09 run); installed as it closed, 21:32 (cmp OK). The log lines to look for: at the start `cheats: Steady aim: the
scope's view (exe+0x22E1DF: its camera exe+0x22C840, the camera's player +0x80, eye +0x50, looking at +0x70) and the
gun's line (exe+0x66CDE4: from +0x4A30 to +0x4A40) re-pointed while on; ...`; in play, as a scope comes up (eight at
most), `cheats: Steady aim: a scope's view held 0.2x s after it came up (the gun settled)`.

### 2026-11-12 - the game stopped itself on the pads' rumble data (the game's own check, not the mod)

The user: "the game just crashed". The log: `game ended` 19:37:45, `game started` 19:37:46 (an area change; the save
written then has Chris' chapter 2, stage s0301, scene 25 - past the Ogroman fight of 19:26), then at 19:37:56 `CRASH:
exception (0x80000003) at ntdll.dll+0xE2A8 (thread 576)`: a breakpoint, BH6.exe's own DebugBreak (import 0x15021A4).
53 min into the run, the 64-bit process (`PROTON_USE_WOW64=1`) with room (2424 MB free under 4 GB); the journal has no
GPU error and no OOM, only the game's exit.
The check: sResource's add-reference (0xF2BF70, through 0xF34B50) breaks when a resource's count (+0x48) is not above
0; its release (sResource slot 14, 0xF2D1B0) deletes a resource as its count reaches 0. The resource: sVibration's (the
pads' rumble; global 0x186EB30, the game's sBioVibration at sBioMain +0x10190) default data +0xA0 - rVibration
`etc\bh6_vibration` (in Core.arc), loaded once at boot (0x472280, the start-up's step 2) and handed over (0xF56A70:
the old released, the new held); nothing else holds it. The request (the dump's raw stack): Chris' frame (uPlayer slot
157, 0xA640E0: its motion's rumble events) -> 0xA5EF60 -> sBioVibration's request 0x542360 (rumble 0x1D0, character 2)
-> 0xF58B30 (its lock) -> 0xF57F30 (a free slot; the data argument 0: the default) -> the slot's set-up 0xF56D90 (it
holds the data) -> the check. It took slot 0 (sVibration 0x16C2A0E0, slot +0xB0): nothing was rumbling, so the
reference sVibration keeps had been lost before.
Not found: which release went once too often, or what wrote over the count (the dump has the registers and the stack,
no memory). Read and ruled out: each of sVibration's releases (a slot's end 0xF58BA0, stop one 0xF573D0, stop all
0xF57430, cancel 0xF58D90, reset 0xF57370, a slot reused 0xF56D90) clears its pointer; the requests take its lock, and
the units' moves run on workers inside a fork and join (0xEF9140 waits for every worker) with the engine's
lock-everything flag 0x186E5D0 set; its frame (0xF58E40, from sBioVibration's 0x5427D0) runs on the main thread in
sBioMain's frame, before the units'; the stops come from the pause switch, an area's exit (0x4DE6F0, the aS* classes'
slot 8) and the game's flow (0x46A74F, 0x46DD20, 0x5462A0, 0x5465F0).
Not the mod's: no mod frame on the stack; the mod never touches sVibration or any resource; none of its game calls ran
(no Add, edit or save in the log); its writes are the characters' fields, the hits and QTEs the game hands its hooks,
and fixed fields of sInfoMan and sGameMan; the countdown it holds is one a display read in the last frame. The game's
own filter (BH6.exe+0x1008C35) is the C runtime's __CxxUnhandledExceptionFilter (C++ exceptions only, the rest to
Windows' default), so on Windows the same check ends the game too. Not seen in the runs before (the 2026-11-06
21:50-23:50 run, 64-bit too, had none). No change to the mod.

### 2026-11-10 - the crash filter's chain (found in a crash report of RE4OGCabbyCodes')

Found reading a user's crash report of RE4OGCabbyCodes' (one crash logged three times), whose filter, chaining and
reassert this mod's are; fixed here at the user's word ("did you fix the bugs you found yet? if not do so"). Two bugs
(`docs/engine.md`, the crash filter's chain): with SetUnhandledExceptionFilter made to do nothing (`ret 4` over its
first bytes - a crash handler keeping the slot), each reassert took the leftover eax for the previous filter, chained
to it and logged "ours is back on top" every second; and a filter set over the mod's that chains to the one it
replaced (the mod's), once the reassert chains to it, made a crash go round the two until the stack ran out. Built
(`src/crash.cpp`): the inert API said once and stood aside from; nothing that is not code chained to; per thread, the
exceptions being passed on - the same one handed back stopped with a line, a new one raised inside passed on, three
deep at most; the same exception again a line; each dump with every register, the stack's top raw and the filter it
is passed on to. Checks: built (no warnings); `tests/test_crash.cpp` 26 (new; the build before fails them - 6170
dumps round the chain-back, then the process died); every other test rebuilt and rerun: discover 324, panel 90, boot
18, memlog 23. Installed 2026-11-10 (cmp OK). The log lines to look for: "crash logger installed (the game's own
filter is chained once it sets one; ...)" as before; a crash's dump now ends "passed on to the filter at ..."; beside
another crash handler keeping the slot, "crash: SetUnhandledExceptionFilter has been made to do nothing ..." once.

### 2026-11-07 - the skills window (unlock what the skill screen sells)

The user: "add a new window to the skill settings page that unlocks the ability to purchase skills (marks their
requirements for purchase unlocked)". Found (`docs/records.md`, "The skills"): the skill screen (uIdSkillSet10) lists
the game's skill table for its kind of list; each skill's state is a byte of the profile (0 locked, 1 new, 2 seen, 3
bought), and the game's own test (0x566AB0) sells a skill whose state is not 0 or whose requirement its own counts
meet. 19 campaign skills have a real requirement: J'avo Killer and Zombie Hunter Lv. 1 (30 kills of the kinds the game
adds up), Grenade Power-Up and the eight weapon masters (800-1,500 kills with a kind of weapon), the eight infinite
weapons (every campaign complete); every other skill sells from the start or once its level before is bought. Built
(the unlock shop convention: tick to unlock, untick to lock again, never a bought one): the skills window beside the
skill screen, `src/records.cpp` (2 signatures and 12 patterns; the requirement counts read from the same tables the
game's test reads, its groups read out of its own code), the texts by `tools/gen_records.py` (mes_file: the skill's
name, what it does, what the game asks). Unlock writes the state the screen leaves a new skill in (2) and the screen's
row as its own fill makes it (it sells at once); no new game call - the skill screen's own close runs the menus' save,
as it does for a purchase. It shows wherever the skill screen comes up - the menus and between chapters (in a game:
found by its vtable). Tested offline: test_discover 324 (the game's own fill, test and counts run in the copy: the
mod's counts and rows agree with them byte for byte), test_panel 90, test_boot 18, test_memlog 23. Log lines:
`records: the skill table ... 19 behind a requirement`, `records: the skill screen (uIdSkillSet10): ...`, `records:
understood - ... skills yes`, then `records: the skill screen <addr>: step 30, kind of list 0 (skills behind a
requirement), user 0`, `records: screen: the skill screen`, `records: skills: N unlocked, N locked again ...`.

### 2026-11-06 - the floor robots that blow up took 6 hits under One hit kills

The user, at 22:40 in Leon's chapter 4: "there are these floor robot enemies that explode that seem to take more than
one hit still". Where they were: the save the game wrote at 22:35 (`userdata/<id>/221040/remote/savedata.bin`, plain)
holds the profile's cGameInfo base fields (mNewGame, mEpisodeNo, mStartChara, mDifficulty, mChapterNo, mChapterStart,
mStageNo, mStartPosIdx, mSceNo: nine dwords, at 0x7F08 in it) - episode 0 (Leon), chapter 3 (the fourth), stage 512,
scene 10; the mod's log had the stretch before it (Ustanak's uEm7300 at 22:18, s0510; Rasklapanje at 22:23-22:34,
s0511). Stage 512's own files (`s0512Soft.arc`: the set list `s0512_k19` places six cSetInfoSmRoomBomb of uSm1310;
the scripts `Rumba_4`, `Rumba_6`, `SN_158_Last_Rumba_1Shot_DEMO`) name them: the room bombs ("Rumba"), set models -
uSm1310 < uSmRoomBomb < uSetModel - not enemies, so no enemy hook of One hit kills ever saw them. A bomb keeps 5 points
in the characters' health word and takes 1 a hit whatever the damage, going off at the hit after its last: 6 hits
(`docs/cheats.md`, "The room bombs"). Only a hit the game's own instant test passes (0xAD2F20, the hit's attributes)
empties its points at once. Fixed: while One hit kills is on, the bombs' two calls of that test (while one rolls,
0xBB94F5; in its second of warning, 0xBB9FD9 - each reached only for a hit the game counts as the players') are
re-pointed at the mod, which says yes; the game's own code then empties its points and blows it up (action 10, event
10 in the warning). Two new signatures (kSigBombHit, kSigBombWarnHit: 97); a call re-point, no new game call. Tests:
test_discover runs the bomb's own code in the copy - off, 6 hits (event 9, actions 9 then 10); on, the first hit of
yours, your partner's or a players' weapon's sets it off, in its warning too; no damage, an enemy's hit, a stage
blast and a character played over the network turned away by the game's own check (302); test_panel 79, test_boot 18,
test_memlog 23. The game was running: installed when it closed (sha 9197e915...). Log: `cheats: One hit kills: the
room bombs (uSmRoomBomb, set models) - their hit checks exe+0x7B94A0 and exe+0x7B9F90 call the game's instant test
(exe+0x6D2F20; a hit's dealer +0x2B5C): ...` at the start, then once a bomb `cheats: One hit kills: a room bomb
(uSm1310 ...) hit by uPl00LeonUsa - blown up at one hit`. The game's other callers of that test are breakable scenery
(0xB25090, 0xB25BD0), left as they are. Lesson: "an enemy" to the user can be a set model - check the stage's set
lists (`cSetInfoSm*`) as well as its enemies.

### 2026-11-06 - the game crashed at every start since 17:09 (zlib's table hooked as a vtable)

The user, after a start at 18:47: "did you break my graphics card again?". The GPU was fine: no Xid in the kernel log
since the boot, `nvidia-smi` and `vulkaninfo` working. But the mod's log ended in `CRASH: ACCESS_VIOLATION ... at
BH6.exe+0xA7D105 (thread 376)`, 140 ms after the first tick. Its two starts at 17:09 (the gamescope logs: 12 s each)
had crashed the same way: three loader threads at once in inflate (BH6.exe+0xFE5BEA), with the same frames on the
stack (the XFS reader 0xE7D060 - the `XFS` magic 0x534658 - and 0xE7D2C0). The 14:35 session (the build before) had
played 2.5 hours. Cause: the One hit kills build installed at 17:09 hooked slot 126 in every vtable of an enemy class
with "whatever function of the game's each class has there"; the class index (`mt.cpp`) takes any `.rdata` dword equal
to a class's getDTI for a vtable's slot 4, and zlib's fixed Huffman table (inflate's `lencode`, 0x17014A0) holds
uEm6527's getDTI address (0x890800 = {0, 8, 0x89}) twice. One of those two "vtables" (0x1701704) held a value inside
the code's range at +504 (entry 279, {0, 9, 209} = 0xD10900), so the hook wrote its own address there: every block
inflated with the fixed codes after that decoded garbage - archives came out broken and the loaders crashed (inflate
itself at 17:09, the XFS reader at 18:47). Every build since (Brzak 17:20, the bosses 17:46, Infinite air 18:03/18:05)
carried it; slot 128's hook (the game's take-damage only) never touched the table. Fixed: the index takes a table as a
vtable only when its slots 0-4 are all functions (3 of 8816 hits left out: those two and one for cEm9900ActRide); a
hook is written only into a table that is a vtable up to its slot. test_discover now compares the game's whole .rdata
and .data before and after install - nothing written but the hooked slots, each in a vtable reaching it, zlib's table
intact, uEm6527's vtables its own, slot 126 in exactly the 191 vtables slot 128 went into; the old code fails those
four (283 checks). test_panel 79, test_boot 18, test_memlog 23. Installed 18:55 (sha 00cd943f...). Log: `mt: 8284 class
vtables found from their getDTI (529 more vtables answer a record of another class's; 3 getDTI addresses in data that
is no vtable left out)`, `cheats: One hit kills: the enemies' damage worked out (slot 126) hooked in 191 vtables`
(192 before). Lesson: never write into a table the index found without checking it is a vtable that far.

### 2026-11-06 - Infinite air

The user: "add a cheat for infinite air for the underwater portions where we hold our breath". Found (`docs/cheats.md`,
"The air under water"): a character's air under water is its combat gauge (uPlayer +0x3B00, its most +0x3B04). A dive
sets the gauge's refill rate to sPlayer's dive rate (-8) for a character this machine drives (0 for the AI's partner,
which never runs short), and the gauge on screen (uIdOxygen) shows it while bit 30 of +0x4E6C (under water) is set.
In two stages - 250 (Leon's chapter 3, Brzak's water) and 1003 (Ada's chapter 1, the submarine) - the player's frame
runs the drowning (0x484750): under water with the gauge at 0, sPlayer's set-health puts the character at 0 and the
drowning (action 0x226) plays. There is no hit, so God mode never stopped it. Built: Infinite air (`InfiniteAir` in
the ini; the panel's Characters heading, after Partner takes no damage): the tick, which runs before the game's frame,
holds the gauge at its most for each character this machine drives while it is under water; on land the gauge (the
combat gauge) is left to the game. Two new signatures (kSigDrown, kSigAirGauge: 95); field writes only, no new game
call. Tests: test_discover runs the game's own drowning in the copy - off, health 0 and the drowning; on, the gauge
held, the game's own add drains a dive's 8 and the drowning does nothing; land, the AI's partner and the network's
character left alone (278); test_panel 79; test_boot 18; test_memlog 23. Installed 18:05 (sha bfdb9781...), then with the fix above at 18:55. Log:
`cheats: Infinite air: a character's air (the combat gauge) +0x3B00, its most +0x3B04; under water: bit 30 of +0x4E6C;
the drowning exe+0x84750 (action 0x226)` at the start, then at each dive `cheats: Infinite air: Leon is under water -
the air (n of m) held at its most`.

### 2026-11-06 - One hit kills on every boss: the audit

The user: "look for other bosses in the game that have these kinds of mechanics and update them for one hit kills".
Every boss class audited (four read-only research passes over the code, the stage placements and scripts, each finding
re-checked; the table in `docs/cheats.md`, "The bosses and One hit kills"). Already fine: Lepotitsa, Ubistvo, Iluzija
(a phase a hit through its own caps), Ogroman (its own code sets mIsNoDead where it must live: each part at one hit),
the whole Simmons family (each form's setup sets mIsNoDead), Gnezdo, Napad, Strelats, Mesets, Oko, the machines.
Fixed:
- Rasklapanje: its own subtract-health never lets health below 1, so One hit kills left it at 1 and its parts - the
  halves and arms it severs and splits by - took their usual 1000/1000/500/500. Now it counts as keeping its life
  (kSigEnemyFloor): a player's hit is worked out big, each part at one hit.
- Ustanak (uEm7000-7500): its brain has no death reaction; where it is not mIsNoDead, One hit kills took it to 0 - a
  dead mark it never leaves, and no reaction to any hit after (no knockdown, no stagger). Now an enemy whose brain has
  no death reaction is taken to its last point, never 0, and gets the big hit: its knockdown count, its guard (uEm7100)
  and its gauges at one hit; s0510's health <= 1 checks still pass and the scripts kill it.
- Haos before its last fight (modes 1, 2: the chase s0972, its first meeting s0903): its death there has no script
  (the s0903 `die_demo` is the player's); now to its last point, with the big hit (its damage count at one hit).
- The s0508 attack chopper keeps a point until scene 150 by its own code (the scripted shoot-down), which One hit
  kills' raise overrode; now honoured (kSigEnemyStageHold: stage and scene read where the chopper reads them).
- Brzak now goes the same way (to its last point, not 1 a hit).
Six new signatures (93). test_discover runs the game's own take-damage on each (a stand-in seeing what its parts get):
Rasklapanje's floor keeps 1 and its parts see the big hit; Ustanak with its brain kept at 1 (a big amount too), with
none as before; Haos modes 1, 2 kept, mode 4 dies; the chopper kept before scene 150 in stage 508, not after; an
ordinary enemy as before (266 checks). Installed 17:46. Left as notes ([I] in docs/cheats.md): Ogroman's stagger in
s0851 before scene 221, Napad's and Strelats' mid-sequence keep-a-point, the s0579 Ubistvo's threshold reaction.
Log: `cheats: One hit kills: ... keeps its life (<why>) ...` with the why for each rule.

### 2026-11-06 - One hit kills and the water's Brzak ("Jaws")

The user: "this might be happening with an encounter with a sea monster in the next chapter as well" (the Deborah fix
not yet in their game: it installed at 17:09 when they closed it). The sea monster is Brzak, uEm4200 - the stage
scripts' "Jaws" - in s0204 (it lurks in the water) and s0250 (it surfaces and retreats three times, transforms, and
the TNT kills it); the game's stage order puts s0204 right after Deborah's s0203 (`docs/cheats.md`, "Brzak"). Its damage
code hands on 1 whatever the hit (its life, 15000-32700, counts hits; it has no death action), and its fight runs on
its stagger, which only its tongue feeds (gauge 1000; body 0). One hit kills on its health either did nothing (its own
actions set the take-damage's keep-alive flag; jaws_water is mIsNoDead) or took all 32700 - a dead mark the game never
gives it, its death reaction in place of the one the game picks - and its stagger took its usual tongue hits. Fixed:
an enemy whose damage code hands on 1 whatever the hit (kSigEnemyHitCount: only Brzak's) keeps its life - One hit
kills leaves its amount to the game - and counts as one that keeps its life, so a player's hit is worked out as 32767:
one tongue hit empties its stagger and the game's own reaction follows; a body hit still does nothing, as the game has
it. test_discover runs its own damage code (0x7285E0, its stagger through uEnemy's 0x952F70 and its slot 127) in the
copy: off, a tongue hit takes 150 of 1000; on, one empties it; its body nothing; its take-damage the game's 1, never
dead (249 checks). Installed 17:20 (the game closed). Log: `cheats: One hit kills: uEm4200's damage worked out
(exe+0x3285E0) hands on 1 whatever the hit - its life counts hits ...`, then in its fight `uEm4200 ... keeps its life
(its own damage takes 1 a hit, whatever the hit) ...`.

### 2026-11-06 - One hit kills and mutated Deborah's cores

The user: "I don't think all the mutated deborah fights have one hit kills working for damagable parts". The game's
own kill table names enemy 0x4500 "Deborah" (uEm4500); stage s0203 places her for four fights (`docs/cheats.md`,
"Mutated Deborah"). One hit kills took all her health, but she is mIsNoDead in three of them (kept at 1 point) and on
the mine cart her own damage code hands on no health at all; those fights go on through her three cores - parts her
damage code works a hit into besides the health (600 each, 250 on the cart) - which still took their usual hits. Only
the first meetings (her health only) moved at one hit. Fixed for every enemy that keeps its life through a hit
(mIsNoDead, the take-damage's other keep-alive flag, or Deborah's held health): a player's damaging hit on one is
worked out as 32767 - in the damage worked out (slot 126, now hooked in every enemy vtable) and around the
take-damage (for the parts a class works the hit itself into, slot 129: Ogroman's, Haos', a J'avo's ...), put back
after - so each core breaks at one hit through the game's own code, and her stagger gauges empty at each hit too. A
core her own code holds at 1 (mActStatus bit 12) stays at 1; an enemy that can die is left as before. Four new
signatures (kSigEnemyKeepAlive, kSigEnemyWork, kSigEnemyPart, kSigEnemyNoLife: 87). test_discover runs her own damage
code (0x761FC0) in the copy through the hook: each core broken at one hit in the core fight, on the cart (with no
health handed on) and in the last fight; the game's own hold kept; an enemy that can die, an enemy's hit and the
switch off as before; the take-damage's parts seeing the big hit on an mIsNoDead uEm4800 (243 checks). Log: `cheats:
One hit kills: the enemies' damage worked out (slot 126) hooked in 192 vtables; ...`, `... uEm4500's damage worked out
(exe+0x361FC0) hands on no health while bit 1 of its +0x2A08 is set ...`, then in a fight `cheats: One hit kills:
uEm4500 ... keeps its life (mIsNoDead: it may not die) - a player's hit on it is worked out as 32767 ...` and `...
part(s) 0x1 broken at one hit`.

### 2026-11-06 - the titles window: tick boxes for incomplete titles only

The user: "only show the tick boxes for incomplete titles". A title held at every level had its tick box greyed; it
is left out now (the column kept, so the rows line up) - as the hide-don't-grey rule has it for what can never apply.
test_panel 78.

### 2026-11-06 - an awarded title's criteria stayed "? ? ?"

The user: "awarding titles does not reveal the criteria for the title". The log: `records: titles: 235 awarded (224 at
the top level), 247 held`, then `... 1 awarded`, both saved - the awards worked. Cause (`docs/records.md`, the title
list): the Dog Tag screen shows what earns a title only while its row's progress - the title's counter - is above 0
("? ? ?", mes_system 0x3E0, before), and its LB/RB page buttons skip a page where no title's counter has started.
The game awards a title only once its counter reaches the goal, so its own titles always show it; the mod set the
title's level and left the counter at 0. Fixed: an award brings the family's counter up to the title's goal (never
down; the two float families as a float; the titles-held counter is recounted instead), the screen's row takes the
progress and the list's pages are worked out again (kSigDogTagTitlePages: 83 signatures) - so it shows at once. A
level held already, asked for again, brings its counter up only: the titles window flags a title held with its count
short and offers "Bring their counts up" for them (the 236 awarded before). Also: a family held to its top shows what
earned its last level in the window, not just "All held.". Tests: test_discover 223, test_panel 77. Log: `records:
titles: N awarded (...), M held; K counter(s) brought up to their title's goal`.

### 2026-11-06 - the game closed at a medal's count typed

The user: "trying to modify the medal count crashed the game". The log: the medals window up (`records: screen: the
Records medals`, `input: the panel is up`), then only `process exiting` - no `CRASH:`, no `the game ends:
ExitProcess`, no `records: medals:`. Cause: the window's count field turns orange while a count is typed - `if
(typing) PushStyleColor`, the field, `if (typing) PopStyleColor` - and the first key typed set `typing` between the two:
a pop with no push, which ImGui checks with its assert (`Calling PopStyleColor() too many times!`). That assert is the
C runtime's (`_assert`, then `abort()`), and it ends the process through the runtime's own ExitProcess: neither the
crash filter nor the game's ExitProcess hook sees it, so the log said nothing. The chapters window's rank picks had the
same mistake (the first pick in a cell). The panel test set the edits straight into the window's state instead of
typing and picking, so it never ran the frame where it happens. Fixed: each pop goes with the push made in that frame
(the orange shows from the next). So that no ImGui check ends the game again, ImGui is built with the mod's IM_ASSERT
(`src/imconfig_re6cc.h`): a check that fails is logged once - `ERROR: overlay: ImGui's check failed - ...
(imgui.cpp:N); logged, and the panel goes on` - and ImGui recovers (the old medal code built that way: that line, and
every check still passing). The crash logger now also takes the C runtime's abort: `the game ends: abort() in the
mod's C runtime ...; where it came from:`, with the stack. Tests: test_panel picks a rank in a cell's combo and types a
digit into a medal's field as a player does (75; the old code hits the assert at both), test_boot the abort handler
(18).

### 2026-11-06 - no records window on the Records and Dog Tag screens

The user: "there are no imgui windows appearing on those associated screens". The log: `records: understood - chapters
yes, medals yes, emblems yes, titles yes ...` but never a `records: screen:` line. Cause: the screens are found as
units on the unit system's lines, and the mod asked the main system's list for "sUnit" by name - but the game's unit
system is its own subclass, `sBioUnit` (the log's `game: the main system ... holds 53 systems: ..., sBioUnit, ...`), so
the lookup gave nothing and no unit was walked. The offline test had registered its made-up unit system as "sUnit",
which hid it. "Kill enemies as they appear" (merc.cpp) walks the same lookup: in play it would have found no enemy
either (still unseen in play then). Fixed in `game::system`: a name no system has finds the one whose class derives
from it (`game: sUnit asked for: the main system's sBioUnit (...), of a class derived from it`, logged once); the test
now makes the unit system an sBioUnit and checks the lookup (test_discover 215). Also logged now, as they change: `records:
the Records screen ...: step S, page P, view V` and `records: the Dog Tag screen ...: step S` (the chapters window wants
step 2, page 0, view 1; the medals page 3; the Dog Tag lists step 30/31 (emblems) or 40/41 (titles)).

### 2026-11-06 - the records windows (Records and Dog Tags)

The user: "Add new imgui windows for a few different game screens: 1) Records screen while on the chapters completed
view - give options to mark chapters completed. If the indicators for completion can be different (maybe based on
difficulty), allow the user to set which ones they want. 2) Records screen while on the Medals view - allow user to set
medal counts for medals. 3) Dog Tags emblem view - allow user to enable achievements to get emblems, and list what the
achievements are for in the window to understand what the emblem is for before awarding them. 4) Dog Tags Titles -
window to show title list and what is needed for each to be awarded, and allow user to award them". Found
(`docs/records.md`): the Records screen (uIdRecord00) keeps a record for each character, difficulty and chapter (its
mark on the grid the best rank, S to E - the "indicators"); its medal page nine categories of counts; each of the 50
emblems is an achievement, given with Steam's by sInfoMan's achievement call; titles are 255 counter families of up to
four levels. The Records screen never saves the profile, and a game's start takes back what is not saved. Asked (two
new call kinds), the user chose "Emblem + Steam achievement" and "Save at once". Built: `src/records.cpp` (24 new
signatures), the four windows in `overlay.cpp`, the texts by `tools/gen_records.py` (the game's messages; the
achievements' descriptions from Steam's schema in the user's Steam folder). A chapter marked cleared gets what the
game's chapter end gives (its template, files, the campaign unlocks - Ada's campaign among them - and the achievements);
a level-4 title "Rising Up", the tenth "Titular Achievement"; each window's change shows at once on its screen (the
screens' own copies written), then the game's own save. Tested offline: test_discover 214, test_panel 72, test_boot 16,
test_memlog 23. Log lines: `records: understood - chapters yes, medals yes, emblems yes, titles yes; the game's
achievement found, its profile save found`, then `records: screen: ...` as a window's view comes up, `records:
chapters: N cleared ...` / `medals:` / `emblems:` / `titles:`, `records: the profile's save asked for`, `records: the
profile saved (N frames)`.

### 2026-11-05 - Add any weapon: the game's loader loads it first

The user: "I previously told you that I want the ability to add any weapon to inventory, but it's still telling me that
the game would quit out on some models. You should fix the game to be able to handle these adds instead". Add had
offered a weapon, grenade or spray only when someone in play held one (greyed otherwise, the hover saying the game would
quit): set-slot makes such an item a unit of its own, whose model, motions and sounds come from its class's archive
(`uWp1020.arc` for the shotgun), and a stage loads only the archives of what is carried, lies there or is fought there -
a unit made without its archive looks for loose files and the game quits ("Failed open file"). Built: the game's own
loader asked first (`docs/inventory.md`, "The loader"), as a stage's load asks for what the characters carry - sLoadMan's
request for the item (which asks for what it needs with it: rounds, arrows, shells, a grenade's thrown form) into the
characters' list, its commit - and, since sBioMain's paused frame leaves the loader's frame out, the mod runs that frame
after the game's while the pause lasts and something it asked for is still loading. The Add waits ("Loading the
Shotgun...") until it is in; a pick in the Add list asks for it at once, so the Add usually acts at once. Every weapon
archive holds its motions for the shared player skeleton (pl0000), so any character can hold any weapon. Found on the
way: `tools/init_records.py` left the class records' hash buckets empty, so the game's own lookup of a class by its key
found nothing offline; it fills them now (and the names' hashes), as MtDTI::MtDTI does. Tested offline: the game's own
request, commit and frame run in test_discover over a made-up sLoadMan (166 checks), test_panel 55, test_boot 16,
test_memlog 25. New game calls: the loader's request, commit and frame - the user's word above. Log lines: `inventory:
the game's loader: 9 lists ...` at the start; `inventory: the game's loader asked for Shotgun (uWp1020) into the
characters' list (3; ...): 1 archive(s) to load, new: uWp1020`, then `inventory: the Shotgun loaded after N frame(s)` and
`inventory: the Shotgun added to Leon (slot n)`.

### 2026-11-05 - the panel did not come up with the Esc menu

The user (keyboard and mouse): "It doesn't seem like the cheats menu knows when to appear. I can press F7 to make it
appear but when pulling up the menu in game it doesn't show automatically. I also don't seem to be able to change any
of the number input boxes for things like time or deaths." The log: `screen: play` and never `screen: pause`; the panel
only "opened over the game by the toggle key", where the editors are greyed by design (edits at the pause only). Cause:
Esc is not RE6's pause. The game's pause is requested in one place only: the HUD's check 0x993390 -> 0x9920E0, when the
pad's Start bit (0x8 of sQlocInput's pressed bits, +0x3BC) is pressed - a tail jump (`jmp 0x511220`) to sBioMain's
pause request (+0x104A9), which sBioMain's frame turns into +0x1049C, the byte the mod reads. On the keyboard Start is
whatever key the game's controls bind to Pause; Esc is a "back" bit (0x2000) in the PC port's key tables and opens
another menu. The user rebound the Pause key: "I remapped the pause key button and the menu properly displayed
there" - the log has `screen: pause` (16:04:23) and the panel up at once. No change to the mod; the README now says
which key pauses. (A diagnostic built to find the Esc menu, not needed in the end, is kept: with `Trace = 1`,
`src/probe.cpp` logs the main systems' rarely changing words as they change.)

### 2026-11-05 - the first version

Asked: "Make a CabbyCodes mod for resident evil 6 that follows the patterns we've done for prior RE mods". Built from
RE5CabbyCodes' engine-independent parts and RE6's own reverse engineering: the panel at the pause, God mode, Partner
takes no damage, One hit kills, Infinite ammo (the tablets kept out of it), Infinite skill points with an editor, the
characters' health. Tested offline (61 + 16 + 12 checks), installed, not played.

Found on the way: RE5CabbyCodes' `adopt.cpp` knows the game window by the class name "RE4HDWnd" (RE4OG's), while
RE5's is "MTFramework" - on Windows its device is never adopted and its panel never drawn. Told the user, who asked
for it to be fixed: RE5's `adopt.cpp` now looks for "MTFramework" (its own `tests/test_adopt.cpp` proves it), built
and installed there the same day.

### 2026-11-05 - steam_api.dll in place of d3d9.dll

The user: "isn't d3d9 used by ReShade? We should find a mod vector that doesn't have compatibility issues with other
common mods". It is: ReShade's DX9 build, DXVK on Windows and other D3D9 wrappers are all d3d9.dll, so the first build
(a d3d9.dll, as RE5's) needed a manual rename to sit beside them, and a ReShade install would have overwritten it.
Of BH6.exe's imports, steam_api.dll is the one that ships with the game, is no common mod's (ASI loaders take
dinput8/version/winmm.dll, controller wrappers xinput1_3.dll) and is not a Wine builtin (Proton loads it with no
launch options). It is what RE0, RE2, RE3, RE4 and RE4OG use; RE5 could not (Enigma carries its steam_api.dll inside
the exe). RE4OG's proxy and install were brought over; Direct3DCreate9 is now hooked in the exe's import table, so a
d3d9.dll beside the game (ReShade's) is called through the mod. test_boot covers the chain with a stand-in d3d9.dll.
Installed: the game's steam_api.dll kept as steam_api_orig.dll, the old d3d9.dll removed from the folder.

### 2026-11-05 - the rest of the siblings' set

The user: "you don't need to do a run of the game for testing, I'll do that. If you found a bug with RE5 for windows
I want you to fix that as well. Then go ahead and finish up the other tasks for this mod for re6". RE5's adopt fix
above; then, from six research passes over BH6.exe (each finding re-checked against the code): each character's
inventory (counts, Throw away, Add - the names from the game's own message archive, `tools/gen_items.py`), Steady aim,
Automatic QTEs, Freeze countdown timer (a stage's and the Mercenaries' clock) with a time-left editor, the Results
(Freeze play time, Deaths don't count, Misses don't count, the clock's and the counters' editors), the Mercenaries'
section (Endless combo, Combo Time always on, Kill enemies as they appear, the match's numbers). Every patch a verified
byte patch or a call re-point that calls on to the game's own function; the game calls are the inventory's own
(set-slot, top-up, refresh, clear - the approved kind). Tested offline (140 + 53 + 16 checks), installed, not played.

### 2026-11-05 - the Characters section removed

The user: "what is the point of the characters section?" It had nothing to act on: each character in play, who plays
them (you, the computer, online) and their health. The names already head the inventories, an online character's
inventory says it is played online, and the HUD shows the health. The user: "remove it". Gone: the section and its
fold line in the imgui ini (`CharactersOpen`; an old file's line is ignored). The character list it drew stays - every
switch goes by who plays each character. test_panel checks the section is absent (53 checks); test_boot 16 against
the new DLL. Installed as the game closed (17:15; the crash below).

### 2026-11-05 - the game closed after a chapter ended

The user: "after a chapter ended the game crashed". The log: `game ended` 17:12:55, `game started` 17:13:01, then at
17:14:54 `CRASH: ACCESS_VIOLATION (0xC0000005) at d3d9.dll+0x7E129 (thread 336)`, reading address 00000028 - 1 h 40
min into the run (from 15:32), every switch on but Infinite skill points and the Mercenaries'. The d3d9.dll is Proton
Experimental's DXVK (v3.1.1-97-ga78a467cadb22be; Proton 11.0-20261027b). Thread 336 is the game's main thread; the
stack's game frames are its renderer refilling one of its own buffers: 0x10EE800 -> 0x10EE690 -> 0x1291830, the
buffer's Lock (slot 11; 0x1291890 is its Unlock, slot 12), argument 3 on a dynamic buffer = DISCARD | NOSYSLOCK
(0x2800), the result checked through sRender (0x186E8BC). DXVK's LockBuffer at that commit: on DISCARD, `auto
bufferSlice = pResource->DiscardMapSlice();` (`m_allocation = GetMapBuffer()->allocateStorage()`), then
`bufferSlice->mapPtr()` with no null check - +0x7E104 is that call, +0x7E129 the read of +0x28: the allocation came
back empty. The journal (Steam's output) has the NVIDIA driver's `NVVM compilation failed: 3` 175 times from 17:13:22
to the crash and never before in the session; no kernel error (no GPU fault, no OOM kill). So the 32-bit process
(large-address-aware: 4 GB) ran out of memory at the next chapter's loading - a known weakness of 32-bit games under
DXVK; ProtonDB has an RE6 report of the game closing at the end of episodes. Not the mod's: its panel was hidden from
16:54 (its Present then only takes the back buffer and releases it), none of its hooks is on that path, and the
stack's 8 listed entries hold no mod frame (the crash logger lists 8 of 128 slots). No change to the mod.

### 2026-11-05 - the memory log

The user: "update the ini to track the cause of the crash. It shouldn't have ran out of memory this easily so do some
diagnostics to figure it out". Found offline: Proton Experimental runs RE6 in the old WoW64 (its `proton` script sets
`WINEARCH=wow64` only with `PROTON_USE_WOW64=1`, forced for two other app ids) - a 32-bit Linux process, so the game's
4 GB also holds Wine's 32-bit Unix side and the 32-bit NVIDIA Vulkan driver (615.78.08, installed 2026-10-16):
libGLX_nvidia -> libnvidia-glcore (30.5 MB) -> libnvidia-gpucomp (149.8 MB, the shader compiler), libnvidia-glvkspirv
(14.2 MB), and libnvidia-ptxjitcompiler (48.3 MB) that gpucomp names - about 245 MB of code before the driver
allocates anything - and the GPU memory DXVK maps in. Built: the memory log (`MemoryLog = <seconds>`,
`src/memlog.cpp`, docs/engine.md): each line the mappings by kind, the Linux side's room (not mapped) and the Windows
view's, Direct3D's texture memory, the mod's own heap; the whole list into RE6CabbyCodes.maps.txt at the first game,
as the room falls under 512/256/128/64 MB and at a crash. On the way: /proc/self opened from a Windows program under
Wine is wineserver's (the server opens the files) - the game's list is found by its pid. Tested offline (test_memlog
25, test_panel 53, test_boot 16), installed; the user's ini has `MemoryLog = 30` - set it back to 0 once the cause is
known.
The user then: "let's go ahead and try the expanded memory mode for the next run" - `PROTON_USE_WOW64=1` added at the
front of RE6's launch options (the user sets them in Steam): Proton copies its environment and sets `WINEARCH=wow64`,
so the game runs as a 64-bit process with the Linux side above 4 GB. The memory log's first line says which mode a
run had ("64-bit process" here; the crash's run was "32-bit"). Taking it out of the launch options goes back.

## In-game checklist

What to look for in the first play (RE6CabbyCodes.log beside the game):
- The start: `loaded (steam_api.dll proxy)`, `forwarding 55 Steam API functions -> ...steam_api_orig.dll (... the
  game's own)`, `imports: BH6.exe's kernel32.dll!SetUnhandledExceptionFilter ... now comes to the mod` (and
  ExitProcess, DirectInput8Create), `dx9: BH6.exe's Direct3DCreate9 import hooked (... it led to d3d9.dll...)` (DXVK's
  under Proton), `boot: the game's renderer starts`, `mt: 8344 class
  records`, `units: lines of 24 bytes ...`, `game: the game is paused while sBioMain +0x1049C is set`, `game: health
  +0xF10 ...`, `cheats: the players' damage hooked in 54 vtables ...`, `the enemies' take-damage hooked in 191
  vtables`, `Infinite ammo: ... at 3 call(s)`, `the skill screen's purchases found`, `dispatch: sBioMain's frame ...
  hooked`, then `main-thread tick running` and `dx9: device ... created`, `overlay ready`.
- Steam starts the game as before (its Steam features - achievements, the overlay - work through the original).
- With ReShade installed (its d3d9.dll in the folder): both draw.
- The panel at the pause in a campaign chapter (the game's Pause key - not Esc; `screen: pause` in the log), gone on
  resuming; F7 hides it there, and opens it over the game in play. Seen 2026-11-05. Do the number boxes take what is
  typed there (and the game not react to those keys)?
- God mode: a zombie's bite, a J'avo's shot, a grab, a fall into the "down" state - no health lost, a flinch still
  plays. The partner with Partner takes no damage (in the Mercenaries, where the game does not spare the AI).
- One hit kills: a zombie, a J'avo (a mutated part too), a boss with stages (one stage a hit); allies left alone.
  Mutated Deborah (Leon's chapter 2): each of her three cores breaks at one shot - in the fight where she opens to
  show them, on the mine cart and in the last fight - and the fights move on; does she stagger at every hit, and does
  that get in the way? The log's `keeps its life` and `part(s) ... broken at one hit` lines. Other bosses the game
  keeps alive the same way (Ogroman, Haos ...): their weak spots at one hit?
  Brzak, the water's "Jaws" (Leon's chapter 3): one shot to its tongue makes it react (retreat, fall back on the
  slide); shots to its body do nothing; does its fight still end with the TNT? The log's `uEm4200 ... keeps its life`.
  Rasklapanje: each shot severs a part (an arm, the halves split); Ustanak: each shot knocks it down or staggers it and
  it never stops reacting, the scripts still finish it; Haos (Chris's chapter 5): the chase (stage 972, after the
  climb) dies at one hit and the stage goes on to its cutscene; on the pipes (stage 903) a hand lets go at one hit,
  the barriers open; the last fight (stage 973): the head's down, each blister, the cocoon at one hit, then the
  heart's melee prompt, three times, the finale; the s0508 attack chopper is still shot down by the script (not
  before).
  The room bombs (the floor robots of Leon's chapter 4, stage 512): the first hit, yours or your partner's, sets one
  off - rolling or beeping; off, 6 hits. The log's `a room bomb (uSm1310 ...) hit by ... - blown up at one hit`.
- Infinite ammo: firing does not empty the gun, no reload; a grenade stays; a health tablet is still used up (its
  count goes down).
- Infinite air: Leon's chapter 3 (the water where Brzak lurks) and Ada's chapter 1 (the submarine) - under water the
  air gauge stays full however long you stay down, and you never drown; switched off, it runs down again. On land the
  combat gauge (melee) works as usual. The log's `Infinite air: <name> is under water - ... held at its most`.
- Infinite stamina (2026-11-14): melee moves and quick shots one after another with the combat gauge full throughout
  (no block lost on the HUD, never refused for want of blocks); going down and back up, full at once; under water the
  air still runs down unless Infinite air is on; switched off, melee spends blocks again. The log's `Infinite
  stamina: <name>'s combat gauge (...) - a spend of n block(s) not taken`.
- Ada's helicopter (her chapter 5, stage 706; 2026-11-14): with Infinite ammo its missiles stay at 6 however many are
  fired (the HUD's count), the second a missile apart as before; switched on after some were fired, back to 6 (the
  log's `uSm5060 ... - its missiles (a, b) back to 6`). With God mode it takes no damage (its health bar full, no
  crash); switched on with it damaged, repaired (`... ridden by a spared character - its health ... filled up`).
  Chris's jet (his chapter 4, stage 851) and the bikes, snowmobiles and jeeps the campaigns put you on: no damage
  under God mode either. An enemy's bike or jeep still goes down to your shots.
- Infinite skill points: at the skill screen (between chapters) a purchase leaves the points as they were; the
  editor at the pause changes them (and the skill screen shows the new number).
- The start's later lines: `game: sGameMan: the mode +0xAC ...`, `game: the chapter's numbers: ...`, `game: a stage's
  32 scenario timers ...`, `cheats: Steady aim: ...`, `cheats: Automatic QTEs: ... 19 automatic scenes' tests`,
  `cheats: the chapter's clock's add ...`, `merc: the match (modes 0x1E, 0x50) ...`, `merc: an enemy's hit record
  +0x24A0 ...`, `game: an FSM's 32 own timers ...`, `cheats: Freeze countdown timer: the match clock found, a stage's
  countdowns found, an FSM's own found`, `inventory:
  set-slot ... the summary slots 8-12, the case from 13`.
- Inventory: the names right (the Wing Shooter's two counts "(One Gun)" and "(Two Guns)" - switch its mode in the
  game's inventory screen and the other count is the one in use; the grenade launcher's three loads); a count applied
  shows in the game's own inventory screen; x / Throw away on a case item (the grenade's summary count drops with its
  stack); Add 30 9mm rounds (onto the stack, then a new slot), a herb, a grenade of a kind you carry.
- Add (2026-11-14): no Weapons tab; the Ammo tab lists only what that character's weapons load (Leon early on: 9mm
  only; after the Shotgun, 12-gauge too; Chris with the grenade launcher: its three rounds), the partner's by the
  partner's weapons. A grenade kind and the spray nobody carries (the game's loader first) - each goes in at once or
  after a moment ("Loading the ..."), with its model and its throw. The log's `asked for ... into the characters'
  list`, `loaded after N frame(s)`, `added to`.
- Steady aim: the laser dot holds still while aiming; no climb when firing a magnum or an assault rifle. Through a
  sniper's scope (2026-11-12): the view stands still while you hold the aim (no breathing), a shot does not jolt it,
  it follows the mouse and the stick as fast as before (slow tracking too), and a shot hits the reticle's centre; the
  Sniper Rifle drops out of the scope for its bolt and comes back - steady again after a moment (the log's `a scope's
  view held ...` each time); the Semi-Auto's and the Anti-Materiel's scopes; a scope while lying on the ground.
- Automatic QTEs: a zombie's grab (the stick waggle) broken at once, a mash, a crank (its gate fully open), a dodge
  (a press in time at the right moment), the bike/snowmobile scenes; the log's `a QTE (kind 0x..) answered` lines.
  Doors and pickups still need your press.
- Results: Freeze play time - the results screen's time as it was when switched on; Deaths don't count - a Game
  Over and Continue leaves the deaths; Misses don't count - 100% accuracy; the editors' numbers on the results screen.
- Freeze countdown timer: a stage countdown (stages 302, 552, 851, 901, 1003: the log's `a countdown on screen shows
  the stage's timer n`, or for 851 `... shows its FSM's own timer 0`) stands still and its time-left editor works
  (1003's, counting up, stands still with no editor); the Mercenaries' clock stands still, a time bonus still adds.
- The Mercenaries (offline): Endless combo (the combo does not end, the score climbs as you kill), Combo Time always
  on (its HUD up), Kill enemies as they appear (enemies die as they come, scored as hand-to-hand kills with +5 s each:
  the log's `merc: a hit of ... written into ...`; how do the deaths look? any enemy left standing? - since
  2026-11-14's fix every one in play, the first wave too, and the armoured and resistant kinds: Napad, Gnezdo, the
  Whopper, Ogroman; Rasklapanje losing a limb a hit; the log's line per enemy type at the match's end - "still in
  play" or "taken and lived on" names one left standing), switched off at the match's end. The flow (2026-11-14
  night): bodies gone soon after their death pose, the next enemies at once, sent wherever you stand on s1200, s1201
  and s1204, and Steel Beast (s1206) going past 89 kills to its clear (`merc: the stage's next sender ... also
  stocks ...` at the 78th); no `merc: the stage has sent no enemy for 20 s` line. Since 2026-11-15 no sender's wait:
  as many as the stage allows at once, the next as each body goes - and each body vanishing as its kill counts (no
  death animation): a clear in seconds.
- The event-driven switches (2026-11-12; the play log's entry above): God mode - a heal (a herb, a partner's
  revive) and getting up leave you full, nothing takes health, a Continue starts you full; Infinite ammo still holds
  after a Continue and at a new stage (the reset's hook); Infinite air - a long dive with the gauge full throughout
  (from a gauge melee had spent: 400, then full); Misses don't count - shots that miss leave 100% (the results
  screen), a partner's misses still count for them; the countdowns frozen as before; the Mercenaries - Endless
  combo's points on the score at each kill, Combo Time up all match, Kill enemies as they appear killing each enemy
  as it comes in (and the ones already in play as it is switched on); the Records, Dog Tag and skill windows coming up
  at their screens as before. Any stutter left with the mod in (and with `Disable = tick` as a bisect)?
- The memory log (while `MemoryLog` is on): `memlog: this process's mappings: \\?\unix\proc\<pid>\maps`, then
  `mem: at start: 32-bit process: ...` and a line every 30 s and at each `game started`/`game ended`. The trend: which
  kind grows chapter by chapter (anon, gpu, nvidia libs), and the room left - "not mapped" (the Linux side's: the
  driver, its GPU mappings) and the Windows view's free; `RE6CabbyCodes.maps.txt` holds the whole list at the first
  game, as the room passes 512/256/128/64 MB and at a crash.
- The records windows (the menus: Special Features' Records and Dog Tags): the chapters window on Records > PROFILES >
  Completed (a tab a character, a pick for each difficulty and chapter: - or a rank) - Apply all: the grid's marks and
  "Completed %" change at once on the screen, the log's `records: chapters:` line, then `the profile saved`; quit to
  Windows and start again: still there. A best time typed under a cleared chapter (2026-11-14; or beside "Every
  chapter" for a character) shows on Chapter Select for that chapter (its character page: a row a character, a
  column a difficulty), not 19:59:59, and the window shows the time applied (0:00:00 too, not 19:59:59); `records:
  chapters: ... n best time(s) set` in the log. A first clear of a story's chapter: its dog tag template in Dog Tags
  (TEMPLATE, marked new), its achievement on Steam; Leon's, Chris' and Jake's stories complete: Ada's campaign in the
  campaign menu. The medals window on Records > MEDALS: its tabs the categories the game shows (following the game's
  own as it changes), a count applied shows at once on the page. The emblems window on Dog Tags > EMBLEM: each
  achievement's name and what earns it; Award: the emblem unlocked on the list at once (NEW), the achievement on
  Steam (the overlay's pop-up?). The titles window on Dog Tags > TITLE: Next / All, the list's stars at once; a
  level-4 title gives "Rising Up". F7 hides a window until its screen changes; clicks in a window do not reach the
  game's screen behind it (and outside it, the screen works as usual). The "Saving" icon after each Apply?
- The skills window (the skill screen - from the menus, and between chapters): it comes up with the campaigns' list
  (not the Mercenaries'), its 19 skills with what each needs and the counts the game keeps (do they match the Records
  screen's kills?). Tick a locked one: on the skill screen its "? ? ?" becomes the skill with its price at once, and it
  buys with skill points (the points go down as usual, its next level then on sale); untick one not bought: "? ? ?"
  again. Leave the skill screen, quit to Windows, start again: an unlocked skill still on sale (the screen's own save).
  The log's `records: the skill screen ...: step ..., kind of list 0` and `records: skills: ...` lines.
- The weapons window (2026-11-14; Records > WEAPONS): it comes up on the WEAPONS page (not ENEMIES), its grid's counts
  as the save had them (Helena 0/4, Chris 0/3, Jake 2/4 ...). A page tick: the row's "? ? ?" becomes its name at once,
  Times Used 1. A character's tick, then Chapter Select with that character: the weapon in hand's list, a full
  magazine; untick it and it is gone there. Quit to Windows and start again: still so (the save after each change -
  the "Saving" icon?). The log's `records: screen: the Records weapons`, `records: the saved inventories: ...` lines.
- The enemies window (2026-11-14; Records > ENEMIES): it comes up on the ENEMIES page (not WEAPONS), "Named 72 of 84"
  for the save of 19:00. Reveal all: every "? ? ?" on the page becomes its name at once, each with 1 beside it; the
  other rows' counts unchanged. Quit to Windows and start again: still named (the save after each change). The log's
  `records: screen: the Records enemies`, `records: the ENEMIES page: 12 row(s) named ...`, `the profile saved`.
- The Mercenaries window (2026-11-15; Extra Content > The Mercenaries or No Mercy > Solo): it comes up at the stage
  select and stays at the character select (not in the other modes' menus), "Stages open 10 of 10" in the Mercenaries
  and 7 of 10 in No Mercy, "Costumes unlocked 16 of 30" for the save of 21:32. Open every stage in No Mercy: Liquid
  Fire, Rooftop Mission, Creature Workshop pickable at once (and playable). Unlock every costume at the character
  select: each character's Costume 2 and 3 offered at once - the cursor where it was - and in a match the costume's
  model; the Collection (Special Features) showing their files new. Quit to Windows and start again: still unlocked
  (the save after each change). The log's `records: the Mercenaries' unlocks: ...` at the start, `records: the stage
  select ...: step 10, mode 0x1E`, `records: screen: the Mercenaries' stage or character select`, `records: the
  Mercenaries' unlocks: n stage(s) opened ...`, `the profile saved`.

## Next to build

The siblings' set is in. What would take it further (each its own decision):
- Kill enemies as they appear: the flow is the stages' own (their EnemyAdd orders' minimums and waits, 0-2 s); a
  switch of its own to send faster with or without Kill enemies (the minimums raised: their FuncParamAddEnemy's +0x4C
  in each running order) would need its own word from the user.
- The skill sets (cSystemInfo's mSkillSet: 8 sets once a campaign is complete, the profile's unlock flag 2) and an
  editor of each set's skills.
