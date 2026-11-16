# The modes: the chapter's results, the countdowns, the Mercenaries

Addresses are Steam build 10112070's; the mod finds each by signature (`src/sigs.h`) and class name. What only play can
confirm is marked [I].

## sGameMan: the game's mode, online, the match

sGameMan (0x4934 bytes; global 0x17CF454, sBioMain +0x10200; the mod takes it from sBioMain's systems):
- +0xAC the mode: 0 the campaigns (and their menus), 0x1E The Mercenaries, 0x50 No Mercy (300 kills, score cap
  99,999,999). 0x46 is Siege (its HUD loads `dlchud_siege`); 0x28/0x32/0x3C are the other online-only modes [I].
  Read by the players' take-damage (kSigPlayerApply +10) and the match clock (kSigMercClock +2).
- +0x878 online (a byte; every network send tests it). +0x87C / +0x880: this machine's id and the match runner's -
  equal offline or on the host (the game's own "this machine runs the match", 0x5504F0). +0x3E48 the play type (0 solo,
  1 online duo, 2 split screen).
- The match: +0x3FC8 its step (0 reset, 1 playing, 2 time up or cleared, 3 the end, 4 over), +0x4864 its end state (0
  running, 1 time up or cleared, 2 all acknowledged), +0x486C the clock (seconds left, a float; 120 at the start; at
  most 5999.99, the float at kSigMercBonus +4), +0x3FCC the HUD's copy (offline copied from the clock every frame),
  +0x4874 the time elapsed, +0xA34 the stage's kills (150 clear a Mercenaries stage, 300 No Mercy: the ints at
  kSigMercTarget +25 / +51).
- Eight score blocks of 0xBC from +0x4260 (both Mercenaries modes score into block 0, the team's): +0x4264 the combo
  running (a byte), +0x4268 the combo, +0x426C the best combo, +0x4270 the combo's window (15 s, +2 a player with
  skill 0x3E while the clock is under 30 s), +0x4274 Combo Time running, +0x4278 its clock (30 s, +10 with skill 0x40;
  the float at kSigMercComboTimePickup +88), +0x427C the kills in it, +0x4280 what the running combo has earned (put
  on the score only as the combo ends: 0x55CE60), +0x4284 the score (cap 9,999,999; No Mercy 99,999,999).

## The chapter's numbers (the results)

- Each profile's cGameInfo at sInfoMan +0x3F798 + profile * 0x3CE4 (the profile in use sInfoMan +0x20). Its live
  data (mTempData, +0x1AF4, 0x1AF0 bytes) holds 8 character blocks of 0xB8 from +0x1B18, block c the character in
  sPlayer's slot c: +0x24 the chapter clock (seconds, a float, at most 71999.0), +0x28 63 counters (u16, at most
  50000) - 0 shots, 1 hits, 2 kills, 3 deaths (the grading's reads, kSigChapterGrade), 4/5 damage dealt/taken, 8
  hand-to-hand kills.
- Three copies: the live blocks; the copy a save takes at cGameInfo +4 (0x1AF0 before the live ones: taken at each
  area start and at the chapter's end, brought back on a quit to the title and on loading a save); a checkpoint's
  (a cGameInfo of aGame's, +0x658: taken at each area start and checkpoint; a Game Over's Continue and a restart bring
  everything back but the character blocks, which stay live - so the clock, shots, hits, kills and deaths carry over
  a death, the death counted).
- The clock: sInfoMan's 0x563700 adds the frame's time (sBioMain +0x10234, seconds) to all 8 blocks through the
  add-clock 0xD70550 (its one caller, 0x563950: kSigChapterClockLoop, RE6CC_CLOCK_ADD), called from aGame's frame
  only - not at the pause, nor in aGame's state 8 (the chapter's end) or from 0xB.
- The counters: the add 0xD705D0 (thiscall (c, k, n)), mostly through sInfoMan's locked 0x464A90. Shots 0x569090
  (once a shot, weapons whose record +3 is set [I: those that count for accuracy]), hits 0x56F920 (a shot's first hit
  on an enemy), kills 0x5725F0, deaths only at the Game Over 0x4719A8 (`push 1; push 3; push esi; call 0x464A90`,
  kSigDeathCounted; Agent Hunt and Versus count elsewhere).
- The grade (0x550EA0): accuracy (hits*100/shots), deaths, time and kills each graded against the chapter's table
  (0x1794954[episode] + chapter*16; accuracy 70/60/50 everywhere), 25/20/15/10 points each, the rank the first of
  {100, 90, 75, 55, 45} reached (S to D, else E). The results screen (uIdResult00) shows the live block's numbers.
- The switches:
  - Freeze play time: the add-clock's first bytes `8B 44 24` -> `C2 08 00` (`ret 8`): nothing adds to the clock.
    A quit and Continue brings back the area start's value, never more.
  - Deaths don't count: the Game Over's `push 1` -> `push 0` (the profile's lifetime count, 0x562C20, still counts).
  - Misses don't count: sInfoMan's counter add (0x464A90, kSigInfoCount - of its two of that shape, the one whose own
    call is the chapter's counter add, kSigChapterCounterAdd, on the profile in use's cGameInfo; thiscall (character,
    counter, amount), ret 0xC) has three calls re-pointed at the mod while the switch is on: the shots' (0x569149,
    kSigShotCount +30, the counter at +26 the shots') is not made for a character this machine plays; the hits' two -
    a shot's hit (0x56FA39, kSigHitCount +35: once a shot, its flag at +21 set at +42; after the same shooter's record
    as the shots') and a blow's (0x51AD56, kSigHitCountBlow +15) - are passed on, then that character's shots put at
    its hits. As the switch goes on, and as a character of this machine's comes in, the shots are put at the hits
    once. Nothing is written each frame (until 2026-11-12 the tick held the shots at the hits every frame). A chapter
    without a hit still shows 0%, as in the game. [I: in play.]
  - The editors (at the pause): a counter written live and in the save's copy (a quit and Continue keep it), the clock
    in every block, both copies. The AI partner's block is cleared at every area start in single player and is not
    shown.

## The countdowns

- **The Mercenaries' clock**: the match frame 0x55D3F0 (aGame's play step calls it when the mode is not 0; never at the
  pause) runs 0x55A610 (kSigMercClockHead) - on the machine running the match, while the end state is 0: clock -= the
  frame's time (`subss xmm0,xmm1` at 0x55A69B, kSigMercClock +17), time up at 0. Time bonuses add elsewhere (0x5585F0:
  the hourglass's seconds, 5/7/10 s a hand-to-hand or special kill), capped at 5999.99. Freeze countdown timer NOPs
  that `subss` while the match plays offline on this machine; the editor writes the clock and the HUD's copy.
- **A campaign's countdowns** are scenario timers of two kinds, laid out alike: the stage's (aStageBase +0x65C running
  bits, +0x660 counting-down bits, +0x664 32 floats, seconds; the stage's frame 0x4DE280, each stage's slot 7, runs
  them) and each FSM's own (cFSMOrder, 0xA24 bytes - every scenario script and character script: +0xA4, +0xA8, +0x930;
  run each frame by 0xD3EA70, kSigOwnTimers, with their ends at +0xB0 + n*0x20 - the end set, the end, the flag it
  sets - and the ends of the stage timers a script started at +0x4B0 + n*0x20, checked there too).
- The scripts' processes are registered by name (0xD5Dxxx, through 0x1257350): an "update" function runs once as a
  node is entered, a "state" function each frame while in it. Timer (0xD507B0, update; its parameter +4 0 run up, 1
  stop, 2 run down, +5 the kind - 0 its own, 1 the stage's, +6 the number, +7/+8 set the start/end, +0xC the start,
  +0x10 the end, +0x14 the flag the end sets). CountDown (0xD3F8A0, state: +4 0 show, 1 hide, +5 0 seconds (a float,
  shown x100), 1/2 a whole number, +6 the kind, +7 the number, +8 a factor) shows it on the stage's uIdStageTimer
  (stage +0xB80, from [0x186E1F8] +0x3830; made only for stages 302, 552, 851, 901, 1003). SmallTimer (0xD5A100,
  state) a gauge over the players (uPlayer +0x49D4): the timer over its most.
- Every one in the stage scripts (all of `nativePC/arc/DX9`'s `*Soft.arc` scanned, 2026-11-13) - the CountDown's:
  s0302's three bombs (CDbom1-3: 8 s, stage timer 3, down - its end the bomb going off), s0552's altitude
  (RSF204_AltitudeCounter: stage timer 0 down 130 -> 93, shown x9 - its end a death), s0851's two (SN_140_countdown
  237/240 s, SN_220_countdown 40 s: its own timer 0, down - a Game Over, RSF 23), s0901's two (SETUZOKU_CD 120 s,
  then 90 s: stage timer 0, down - their end the next part: a hold-out), s1003's (sce200: stage timer 0 counting UP
  2301 -> 2508, a whole number - a Game Over). The gauges (s0300, s0301 its own timer 20, s0302's stage timers 0-2)
  all count up a partner's bomb being set (their end: set).
- The mod: the CountDown's two reads - the stage get (0x4DC2D0, kSigStageGet) called at kSigCountDown +126, and its
  own timer read in place at +137 (kSigCountDownOwn: `movzx eax,[esi+7]; movss xmm0,[edx+eax*4+0x930]; movss
  [esp+14h],xmm0`, 19 bytes rewritten into `movzx; push eax; mov ecx,edx; call` the mod's read; `fstp [esp+14h]` as the
  stage read beside it does) - and the SmallTimer's get call (kSigSmallTimerRead +39) come to the mod from the start:
  it notes the timer and reads on. Freeze countdown timer holds that timer where it was in the read, each frame the
  display reads it: the CountDown's whichever way it runs, a gauge's only counting down (so none: they count
  progress up); time the game gives back (a raise counting down, a drop counting up) is taken up; a hold lapses after
  30 frames unread; not online, where each machine runs its own. The editor writes one counting down (its value the
  seconds left; one counting up has none). Until 2026-11-13 the own read was not seen (s0851's countdowns ran on: the
  user's report) nor one counting up held. [I: in play.]
- A countdown whose end is the player's way on stands still too while the switch is on - s0302's bombs never go off,
  s0901's hold-out never ends: switch it off there.
- Not countdowns: uIdCountDown (the online Game Over's "continue?" count), uIdBomb (a grenade's arc), uIdOxygen.

## The Mercenaries

- A kill's way (all verified statically): an enemy's brain (its uXxxIntel unit, on sUnit line 3 - before the enemies'
  line 7) takes the enemy's hit record (a DamageInfo at uEnemy +0x24A0, valid +0x24BC) in its damage check 0x9425F0
  and hands it to the damage handler 0x95CF00: the amount (slot 126), take-damage (slot 128), and at the death the kill
  report 0x56FDE0 once (+0x3B2F died, +0x2BA4 reported) - the record of the kill (its kind, the credited player from
  the dealer at +0x28, the weapon from the attack id at +0x2C), then 0x5725F0 for the score (0x55C110: the kind's
  points, the combo +1 and its window, the combo bonus earned), the stage's kill, and the time (a hand-to-hand kill: the
  attack id equal to the player's own class id, 5 s; special actions 7/8/10 s).
- Endless combo: the combo window's `subss` (kSigMercCombo +47, 6 bytes) NOPed - the window stays where the last kill
  set it, so the combo never ends. What it earns goes on the score as it is earned: a kill's award (0x553590 on
  sGameMan, thiscall (player, amount), ret 8 - it adds the award to the combo's earnings, which the game puts on the
  score only at the combo's end) is called once (0x55C287, kSigComboAwardCall +15, after the combo marked running
  +10), and that call is re-pointed at the mod while the switch is on: the game's award, then the earnings put on the
  score at once, at most the score's cap (kSigComboAwardEnd: the earnings +6/+25, the cap +12/+19, checked against
  the match's fields; No Mercy's cap in No Mercy). Until 2026-11-12 the tick moved them each frame.
- Combo Time always on: its clock's `subss` (kSigMercComboTime +99) NOPed; started once, as its hourglass starts it
  (the clock, the kills in it, then running), as the switch goes on in a match or a match begins with it on - the
  held clock keeps it running till the match ends. It adds no time (none of RE6's Combo Time does).
- The enemies in play are known by their own coming and going (2026-11-12; before, the tick walked every unit each
  frame): every enemy class's start (slot 5: sUnit's line frame calls it as it makes a unit running, kSigUnitStart)
  and destructor (slot 0: called with 1 as a going unit is taken off its line, kSigUnitDestroy) hooked - 191 vtables
  each (`docs/engine.md`, "The units").
- **The hit check** (0x9425F0, called by a brain's slot 33 - uEmIntel's 0x942A80, the J'avo's 0x6CB110, Rasklapanje's
  0x774010, the NPCs' 0x7F0640, uEm5800's 0x8280F0; every Mercenaries enemy's brain has one): the enemy's own record
  and its attached parts' (up to 5 units at uEnemy +0x3D0C - a J'avo's mutations: a part's hit gets +0x64 += 70 x (its
  slot + 1)), the strongest handed to the damage handler. A record is taken only if (0x95CE00) valid, its attribute
  bit 19 clear, the per-player cool-down (+0x3B3C + 4 x the dealer's player number, floats; set from +0x3B60 = 0.1 s
  after a damaging hit) run out, not mid-grab (+0x3B33), not dying (+0x38 == 2: such a one dies at any hit, but the
  brains pass 1, which refuses it), a dealer. Before it: the J'avo's brain skips the check a frame its pending action
  (+0x5C) is set; every brain drops a hit (valid 0) while the enemy has no action yet (+0xF58 == INT_MIN, brain slot
  24) and +0x3BA0 is 0. The chrysalid (uEm1370, made by the J'avo's SanagiChange action on line 8) has no brain: its
  move (slot 9, 0x6E92B0) takes its own record, and at no health sets +0x2A08 bit 31 (not reported) and dies.
- **Where a hit lands**: the handler copies the record's +0x60 (the collision) and +0x64 (its part, a word) to uEnemy
  +0x2B00/+0x2B04 (0x5E6370); slot 60 answers +0x2B04 (the J'avo's with left and right swapped), slot 61 the region
  the collision gives that part (0x94D600 -> 0xD73020: -1 for none). Each class's slot 126 rates the hit by it: the
  J'avo (0x66FC20) part 0 x1.2, limbs x0.9/x0.6/x0.5, actions 0x7C-0xDA x0.3, 0xCA-0xCE x0.1, some parts 0; the
  zombies (0x7D4080) by region from their parameter file, nothing at +0x2A08 bit 19 or +0x3A88 bit 1; Gnezdo
  (0x712210) x0.01; Napad (0x70A120 -> 0x707EC0) a rate a region of its armour; the Whopper (0x800940) a rate a region;
  the chrysalid (0x6E9320) the hit as it is (nothing from an enemy). The mode's scaling (0x550280) is none in the
  Mercenaries, the attribute rates (0x952F70) none for a hit with none.
- **The stages** (2026-11-14): the exe's table 0x173FB68 (8 bytes a stage: its number, its developer's name) - s1200
  地下鉄～駅 (the subway station), 1201 市庁舎前市街広場 (the city hall square), 1202 メインストリート (the main
  street), 1203 洞窟 (the cave), 1204 空母 (the carrier), 1205 カタコンベ (the catacombs), 1206 橋 (the bridge), 1207
  クアッドタワー屋上 (the Quad Tower's roof), 1208 研究施設 下層部 (the research facility's lower floors), 1209
  マグマ制御室 (the magma control room); s1210-1216 the first seven again (No Mercy's; s1217-1219 have a Soft archive
  only). The stage select's names (mes_system_eng 807-813, again 2460-2466) come in that order: Rail Yard, Requiem for
  War, Urban Chaos, Mining the Depths, High Seas Fortress, The Catacombs, **Steel Beast** - so s1206 is Steel Beast,
  s1204 High Seas Fortress, s1200 Rail Yard (the medals' "Cleared ..." texts, mes_file_eng 3306-3312, are in another
  order). s1207-1209's are not among those seven.
- **The Mercenaries' enemies** (the stages' set lists, `soft\stage\s12xx\s12xx_em*`: cSetInfoEm1000 ... with their
  unit class; 2026-11-14): J'avo maps s1200/1201/1204/1206/1208/1209 (and No Mercy's s1210-1219, s13xx) - uEm1000,
  uEm1200, uEm1400 (J'avo; their chrysalid uEm1370), Strelats uEm3000, Mesets 3100, Napad 3200, Gnezdo 3300 (the
  secret one), Ubistvo 4300, Rasklapanje 4700 (s1209, s1219, s1309), Ogroman 4800 (s1201, s1211); zombie maps
  s1202/1203/1205/1207 - uEm5000, 5050, 5060, 5181, 5191, 5200, 5210 (zombies), Bloodshot 5400, Shrieker 5500, Whopper
  5600, the dogs 5700, Lepotitsa 4100, and a Napad, Strelats or Ubistvo. The kill table names them (`docs/records.md`).
- Kill enemies as they appear (2026-11-14): every enemy in play - known by its start and destructor - is hit until it
  is dead (no health, dying, died or reported), at most 8 a tick, each its turn; one whose kill the game would not
  report (+0x2A08 bit 31: a chrysalid hatched) or whose record is busy waits. The hit, written at the tick (before the
  game's frame: the brains' line runs before the enemies'), as the network writes a partner's (0x5E6230): all its
  health as the damage, no attributes, where it stands (uCoord's mPos, +0x50 - the place uCoord's own field
  registration gives it, by name) and the direction from the dealer, part 0, the attack the dealer's class id
  (hand-to-hand), the dealer the first character this machine plays, a random byte, valid last. At the next tick:
  still the mod's, it is cleared; not taken (unread, or dropped) - tried again after 100 ms (2 s after 50); taken and
  it lives on (a shrug) - after 400 ms (2 s after 20). The take-damage (slot 128, cheats.cpp's hook) asks
  `merc::lethal` first: a hit of the mod's - in the record of an enemy it was written into, its fields as written -
  takes all the health whatever the class worked it out as (it is reported as taken); after a shrug the damage is
  32767; after three, all the health even from nothing; and the part index cycles 0..31 (below 70: the J'avo's
  attached parts). An enemy the game keeps alive (mIsNoDead, its keep-alive flag, Rasklapanje's floor) lives on: the
  log names it once a match (One hit kills' reason). Nothing is written at the pause. Only offline, while the match
  plays. Until 2026-11-14 an enemy was tried again only once a first hit had been written into it (the sweep as the
  switch went on wrote 4 a tick and left the rest), the hit at an enemy's start was cleared unread, and the hit was
  the class's to work out (`docs/history.md`).
  A J'avo's mutation is a unit of its own (uEm2000-2320, uEmParts1000) whose record its J'avo's brain reads: a hit
  of the mod's there is taken by the J'avo's take-damage (lethal() sees the hit's record is the part's, its `self` the
  J'avo) - the J'avo dies of it; the part is marked the J'avo's and not hit again while the J'avo is about (until
  2026-11-14 night it "lived on" through every hit, the log's noise), again once the J'avo has gone (a part may go on
  alone).
  [I: the death's look with such a hit; a part of Rasklapanje's broken at each hit, and what its brain does once all
  are; enemies far off waking with a hit waiting.]
- **How a stage sends its enemies** (2026-11-14; `tools/fsm_dump.py` over `s12xxSoft.arc`: `soft\stage\s12xx\fsm\plan\
  fix\*`). The set lists `s12xx_em00..em20` are groups (a list a group, each entry a set placement: its unit class,
  mLife, ...; s1208's 157 entries, 152 J'avo), each entry sent once. The script `EmSet12xx` (a cFSMOrder) runs one
  node at a time, each with an **EnemyAdd** order (registered by name through 0x1257350: entry 0xD5CD10, each frame
  0xD59380, exit 0xD41180; its parameters cFSMOrder::FuncParamAddEnemy - mCheckType +0x18, mStockType +0x1C,
  mStockGroup +0x38 (a bit a group), mWaitTimer +0x44, mMinEmNum +0x4C, mAddEmNum +0x50): at its first frame it
  stocks its groups' unsent entries (0xD46150); then, **while sEnemy holds fewer than mMinEmNum set-placed models**
  (mCheckType 1: sEnemy's count, 0x516740 - with no flag sBioModel's +0x68, which its job 0x5162D0 counts each frame:
  every model in its list whose set id - uCharacter +0xED8 a byte, +0xEDA a word - is not "none", 0x5E12B0) and
  mWaitTimer has run, it sends mAddEmNum (1) from its stock (0xD460D0: sEnemy's 0x54C110 per entry; the entry is
  used up whether or not the enemy was made). Nodes move on by kills (an EnemyCheck order, act 0x2A "gone or dead",
  counting its target's entries into FreeS32[n]: "A wave's leftovers" below). The minimums: s1208 3, 5, 6, 6, 8
  (waits 0-1 s), s1202 5-13, s1200 4-9, s1203 5-11; No Mercy's (s1212, s1218) 20 to 60. The other scripts:
  Attack_Control_* (AttackRights by kills), RankUp (RankPoint at 81), Em_BossPop_* (an EnemyAdd of a boss's group,
  15-19, once the stage's kills reach a count - 55 on s1201) and SecretEnemy_Pop (EnemySet: group 20 at once, on
  RSF[0]).
- **A body holds its place**: a model leaves sEnemy's list only as its unit goes (sBioModel's slot 10, 0x517570, the
  removal list +0x58), and a dead enemy's unit goes when its erase has run - the J'avo's death pose waits for its
  motion (cEm1000ActDeadFaceDown 0x6BC400: +0x3A8C), then cEm1000ActDeadErase (0x6BE400 / 0x66A8B0) 7.0 s and 0.5 s
  (2.5 s in Siege), the zombies' cEm5000ActDeadErase (0x7A4E80) 70 frames and 8.0 s; each erase ends with +0x3A8C and
  +0x3B30 set, the brain's frame (0x942B10) then sets mRno (+0x38) to 3 and the unit ends. Every enemy family has an
  erase of its own (34 classes). So with each enemy killed as it comes, the places fill with bodies and the stage
  waits some 10 s for them: the user's "large waits before more enemies spawn".
- **By area** (s1200, s1201, s1204): `em_add_check` runs AreaHit (0xD5A510; act 0: a player in area task mNo -
  0xE19030 asked at 0xD5A561 - its flag set or cleared) for each area and puts the first the player is in into RSC[0]
  (sBioMain's 16 byte registers, shared by every script: the RSC order 0xD3F1A0, its property's getter 0xD3ED10), every
  2 s; EmSet's area node sends from that area's sender (its node re-checked every 2 s by its own Timer). s1200: area 0
  (the platform) groups 0-4 (52 entries), area 1 (the ticket gates) 0,5-8 (40), area 2 (the far end) **none** - and
  the 40 kills that move it on count over groups 0-8; s1201: 0 (underground) 41, 1 (ground) 62, 35 kills; s1204: 0
  (deck) 52, 1 (hangar) 49, 45 kills. Standing in s1200's far end nothing is sent; at its ticket gates the stock runs
  out at 40; elsewhere only that area's groups come: the user's "enemies stop spawning unless I move around" (RE4's
  and RE5's own spawners had their versions: RE5's area boxes, `../RE5CabbyCodes` "Every area sends"). FreeFlag is a
  script's own (+0x9B0, a bit each). The other stages' em_add_check sets RSC[0] for nothing; no set entry sleeps
  (isScrAdjSleep, isObjAdjSleep, MotionCalcOff all 0).
- **The erases' waits** (each family's own erase action - its class by name, the function from its vtable, each wait
  a `movss xmm0, [constant]`): the J'avo (cEm1000ActDeadErase's start 0x6BE400) 7.0 s into its timer (+0x24), then
  0.5 s; zombies (cEm5000ActDeadErase's frame 0x7A4E80; the uEm5000 family: 5000-5210, Bloodshot 5400, Shrieker 5500)
  70 frames lying (sBioMain +0x70 a frame), then 8.0 s; dogs (cEm5700ActDeadErase: start 0x80DCC0 2.0 s, frame 0x809170
  5.0 s; frozen, cEm5700ActDeadEraseIce 0x80DDB0, 7.0 s); Napad (cEm3200ActErase's start 0x70C950: 420 frames, or 300
  after an explosive death); the Whopper (cEm5600ActDeadErase: start 0x7FFA10 2.0 s, frame 0x7FC9F0 3.0 s then 7.0 s).
  Siege (mode 0x46) loads others (2.5, 4.0/20.0, 0.5, 150 ...). The rest of the 34 erase actions (Strelats, Mesets,
  Gnezdo, Lepotitsa, Ubistvo, Ogroman, the chrysalid, Rasklapanje's ...) are one to three enemies a map.
- **The flow** (while Kill enemies as they appear acts; `merc.cpp`, 2026-11-14 night): each of those 12 loads
  re-pointed at the mod's short wait - 0.25 s (or 8 frames) - a verified 8-byte patch each (the load found in its
  function, up to the int3 padding, its constant's value checked), so the whole death plays with short waits and a
  body soon leaves sEnemy: the stage sends its next at once and its own limits (3-13) stand. AreaHit's area test
  (0xD5A561, kSigAreaHitInside +17) re-pointed - every area has the player in it, so em_add_check takes its first (RSC
  0), whose groups carry each of the three stages past its area phase (52 >= 40, 41 >= 35, 52 >= 45). All put back as
  it stops acting. Logged: `merc: 6 erase actions' waits found (12 loads ...)`, `merc: AreaHit's area test ...`. The
  first build of that night (22:54) left the bodies out of EnemyAdd's count instead (its one call, 0xD595DC, re-pointed:
  sEnemy's count 0x516740 less the dead set-placed enemies in play, up to 32 about) - it raised the stage's limits
  (bursts of 20-30, bodies piling up); the user chose the fast dissolve (23:0x). [I: in play - how the cut dissolve
  looks; the first kills' bodies of the other kinds still holding places.]
- **A wave's leftovers** (2026-11-14 night; `tools/merc_sim.py` runs a stage's EmSet with each enemy killed as it
  comes). EnemyCheck (entry 0xD58FC0, frame 0xD590C0) counts by its target (+0x1C): 0 its list (+0x24, count +0x2C), 1
  one enemy (0xD56FE0's), 2 a group (mGroupNo +0x20), 5 every set entry of the stage - into its script's FreeS32
  (cFSMOrder +0x8B0, 4 bytes each; n = +0x40). A sender's stock is its own groups' unsent entries, built group by
  group at its first frame, so a wave that moves on leaves its last groups' tail unsent, and only a later sender with
  that group in its own mask sends it. Steel Beast (s1206): node 4 (group 0, min 5) -> at 2 dead node 5 (0-2, min 8)
  -> 25 of groups 0-1: node 1 (1-4) -> 48 of 0-4: node 2 (4, 5) -> 55 of 0-5: node 6 (5-10, min 9, 1.5 s) -> **78 of
  its 84** (groups 0-6, 8): node 7 (6, 9, min 7) -> **90 of 95** (groups 0-6, 8, 9): node 3 (0-16, the rest). In play
  up to 9 are alive as node 6 moves on, so group 8 has mostly gone out by then; with each enemy killed as it comes
  none is, and half of group 8 (12 entries) is still in node 6's stock - node 7 stocks group 6 (all sent) and 9 (11):
  78 + 11 = **89**, and it never moves on (the user's "capping out at 89 kills"). Simulated (bodies 1.5 s): s1206 89
  at node 7; every other stage reaches its clear with or without the fix - s1200-1209 149-156 of 153-157 entries
  (s1201's nodes 5 and 6 stock every earlier group; the others' thresholds leave room), No Mercy's s1210-1219 284-331
  from EmSet alone (each wave stocks every group before it, each check counts the stage's every kill, target 5; the
  bosses' and the rest's groups are other scripts'). The other scripts with senders: Em_BossPop_* (s1201-1203,
  s1207-1209, s1211-1213, s1217-1219), a boss's group each (15-19).
- **The waves' fix** (while Kill enemies as they appear acts; `merc.cpp`, 2026-11-14 night): EnemyAdd's call of its
  stock builder (0xD593F5, kSigEnemyAddStock +11 -> 0xD46150; thiscall - the script's cFSMOrder, then the stock (its
  work +8), the list (+0x20), the param; RE6CC_STOCK_BUILD: the builder reads the param's type at +0x1C, 2 by groups,
  and its groups at +0x38, inside FuncParamAddEnemy's size) re-pointed: a by-groups sender's mask is widened for the
  build with the groups of every sender before it **in its script** this match, and put back after. The scripts are
  told apart by their cFSMOrder (FreeS32, FreeFlag and mCntState are each script's own), so a boss's script keeps its
  groups (the first build, 23:22, took every script's: a boss's group could have reached the spawner). The builder
  stocks only unsent entries (nothing is sent twice); the stage's minimums, waits and thresholds stand; reset as a
  match begins. The senders built before the switch went on (it goes on in the match's first pause) are not known to
  it - simulated, every stage still clears with its first three unknown. Simulated with the fix: s1206 152 (node 3).
  Logged: `merc: a stage's sender stocks its groups (exe+0xD593F5 -> exe+0xD46150; its param's type +0x1C, groups
  +0x38) ...` at the start, `merc: the stage's next sender (groups 0x240) also stocks the groups of the ones before it
  in its script (0x5BF) ...` as one is widened (6 a match at most). And a stall: with the switch on and the stage's
  kills standing 20 s, `merc: the stage has sent no enemy for 20 s (kills ..., the clock ...; its scripts' senders'
  groups so far: ...)` with up to 12 of the enemies in play (health, state, unit, flags, record), once a stall. [I:
  Steel Beast past 89.]
- **The senders' waits** (2026-11-15; the user: "we could spawn more enemies in faster"): EnemyAdd's frame (0xD59380:
  the work, 0xD46840 - the script's works +0x9E0, how many +0x9D4, each its param at +4; its state +0x3C, 0 to stock,
  1 sending, 2 done) - while fewer than mMinEmNum are in play its timer (work +0x38) has the frame's time taken off it
  (the order's slot 8, +0x20) and at 0 or below (`comiss` with 0.0, `jbe` 0xD59611 to the send step) it sends
  mAddEmNum (0xD460D0 each: the stock's first entry through sEnemy's 0x54C110, used up whatever comes of it; false
  once the stock is empty, and then the order's flag and state 2) and reloads the timer: mWaitTimer + (r - 0.5) x 2 x
  mWaitRandom (r of the game's random, 0x186DEA4; mWaitRandom 0 on every stage) - the timer runs only while there is
  room, so the wait is between sends. The senders' waits (`tools/fsm_dump.py`): s1200-1209 0-2.0 s (2.0 on s1200,
  s1204 and s1209; s1206's 0-1.5), the bosses' scripts 0-5.0 s; No Mercy's 0-2.0 s (2.0 on s1210, s1211, s1214, s1216,
  s1218, s1219), each one's group-0 sender none (19-30 at once). With the bodies short and each enemy killed as it
  comes, the wait is the pace: the 13:3x run's s1212 (0.5-1.0 s) 300 kills in 202 s, s1216 (2.0 s) in 524 s. While
  Kill enemies as they appear acts, that `jbe` is a `jmp` (kSigEnemyAddWait +21; a verified byte patch, checked: the
  timer's place the same in the test, its countdown and its reload - `fld [mWaitRandom]; fadd st, st; fmulp; fadd
  [mWaitTimer]; fstp [timer]`, the wait +0x44, its random +0x48 and mMinEmNum +0x4C one after another, the send step's
  mAddEmNum +0x50 - the constant 0.0, in the stock call's function): a sender sends whenever fewer than its minimum
  are in play, a frame at a time, as the game's own wait-0 senders do. How many at once stays the stage's. Simulated
  (`tools/merc_sim.py --widen --nowait`; bodies 1.5 / 3 s): s1200-1209 clear in 22-34 / 42-67 s (45-242 s with the
  waits), No Mercy's in 10-12 / 17-23 s (92-592 s); every stage's kills the same as with the waits (s1202, s1216 and
  s1218 clear only with their other scripts' enemies - Em_BossPop_*'s, L4D_em_set*'s EnemySets -, which the simulation
  leaves out: 149, 295, 284 from EmSet alone). Logged: `merc: a stage's sender's wait (its test exe+0x959611; the
  work's timer +0x38, from its param's mWaitTimer +0x44) - ...`. [I: the pace in play; a whole No Mercy wave at once.]
- **A body's end** (2026-11-15; the user, after the waits: "We need to decrease the death animation during this cheat
  to allow for faster respawns instead of just kicking the can with a higher cap"): every erase's last step sets
  uEnemy +0x3B30 (a byte; +0x3A8C with it), and the enemies' brains' end step - uEmIntel's slot 34, 0x942B10: its own
  work (0x9428F0, by its mode +0x44: 0, 1, 2; none from 3), then its enemy (brain +0x40) with that byte 1 has the
  brain's mRno (+0x38) and the enemy's made 3 - ends the unit. Every enemy brain's slot 34 is that step or ends with
  it (`jmp 0x942B10`): the J'avo's uEm1000Intel 0x6CB280 (its parts' hits first), uZombieIntel 0xCFEFF0 (the zombies,
  Lepotitsa, Deborah, the Whopper, uEm8100: its enemy's hit record first), uEmAnimalIntel 0x936EF0 (uEm9000-9050).
  What a death does before it: the kill report, the score, the combo and the stage's kill at the death itself (the
  damage handler); then motions only - the death's fall and pose (the J'avo's ends at its motion's end, the motion's
  flags +6 bit 2, or its frame +0x3FC at 15) - and the erase (its timers on sBioMain's seconds +0x10234; the J'avo's
  dissolves its two parts' models, +0x422C, +0x4230; read, it calls nothing else: no drop, no stage event). While Kill
  enemies as they appear acts (not at the pause), the tick sets that byte on a body whose kill has been reported
  (+0x2BA4) - not a part of another enemy, not one the game does not report (a chrysalid's J'avo) - so its brain ends
  it in the game's next frame: no death motion, no dissolve, its place in the stage's count free in two or three
  frames. kSigBrainEnd (the byte +13; mRno +22, +29), checked: the match in uEmIntel's vtable, the byte inside uEnemy.
  The erases' short waits stay for the bodies not ended so. Simulated (bodies 0.25 s): s1200-1209 clear in 5-6 s, No
  Mercy's 10 s - there the simulation's own frame (a send each 1/30 s) is the pace; the game sends a frame at a time
  (120 a second at 120 fps). Logged: `merc: a body's end: the enemies' brains end one whose byte +0x3B30 is set
  (uEmIntel's exe+0x542B10, ...)`, the first body of a match, and the match's `... N bodies ended as their kills
  counted`. [I: a J'avo's mutations and weapon as it vanishes; the pace in play.]
- One match: Kill enemies as they appear switches itself off as a match ends (or begins with it already on).
