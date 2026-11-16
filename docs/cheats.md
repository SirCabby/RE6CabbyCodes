# The cheats: the characters, the damage, the room bombs, the ammo, the aim, the QTEs, the air under water, the combat gauge, the rides and the skill points

Addresses are Steam build 10112070's; the mod finds each by signature (`src/sigs.h`) and class name.

## The characters

- Classes: uCharacter < uBioModel < ... < cUnit; uPlayer < uCharacter, with uPlPlayer (the campaigns' and the
  Mercenaries' characters: uPl00Leon, uPl01Helena, uPl02Chris, uPl03Piers, uPl04Jake, uPl05Sherry, uPl06Ada,
  uPl07Helper = the Agent, uPl30Ellis, uPl40Coach, uPl50Nick, uPl60Rochelle; each with costume classes under it,
  uPl00LeonUsa ...) and uPlEnemy (Agent Hunt's player creature); uEnemy < uCharacter (uEm*, with uEmNpcBase among
  them: allies and the J'avo alike).
- Health: uCharacter +0xF10 (s16), its most +0xF12. Setters: 0x46DEE0 set, 0x4AAE30 set both, slot 43 add (0x48E240;
  uPlPlayer's own 0xA63AE0 clears a "down" bit when above 1), slot 44 subtract (0x95DA40, every character's - the one
  a hit's damage comes to). A player's health is in blocks of 150 (0x96); at 1 the player is down (dying), at 0 dead
  (0xA532C0: 3 up, 2/1 down, 0 dead).
- sPlayer: its characters at +0x24, 8 of them (0x590B00). Who drives each: uPlayer +0x4E90 - 0 this machine, 1 the AI,
  2 the network (sGameMan +0x5B0[8] is where sPlayer's hand-over 0x593920 takes it from; the AI taking a character over
  restores its health). kSigPlayerControl reads the array, the count and the field from that hand-over.
- The inventory: uPlayer +0x46E0, 24 cItem of 0x1C; +0x46D8 the slot in hand - docs/inventory.md.

## The damage

- **A player** (uPlPlayer's slots): 117 (0xACC1A0) is a hit (DamageInfo: HitInfo.Damage +4, read as a word; +0x10 and
  +0x14 attributes; +0x28 who dealt it; mDirection +0x50). It leaves at once for a player marked down/dead (+0x2A08
  bit 14), scales the damage by mode, asks slot 118 (0xACC690) for the reaction (worked out from the health the hit
  would leave: a hit that would put the player down picks a falling reaction), takes the damage through slot 121
  (0xA63570, 6 arguments), plays the reaction (slot 46) and notes the damage taken. Slot 121 is also what the
  enemies' grabs and blows call (115 places): it lets off a character not driven here (in a campaign the AI's
  partner), puts a player straight down for a blow that does that (flag 8: health 1), otherwise calls slot 120
  (0xA63480: the damage worked out by 0xA53050 - difficulty, the blocks - and taken through sPlayer's locked
  subtract, 0x591580 -> slot 44).
- **An enemy**: its damage handler 0x95CF00 (called directly by the hit system) copies the hit, scales the copy's
  damage to the mode (0x550280), works the damage out on the copy (slot 126; uEnemy's 0x95D290) and takes it through
  slot 128 (0x95D320: hit, amount; ret 8): slot 129 (the parts a class works the hit itself into), slot 44
  subtract-health, then if the health is gone, keeps 1 point when the enemy may not die (slot 58 says no, +0x3B33,
  mIsNoDead +0x3B32, or the hit says so - 0x953A70), else marks it dead (+0xF24). 190 of 193 enemy vtables hold
  0x95D320 in slot 128; uEm5700's own (0x806550) calls it first. Bosses with stages override subtract-health instead:
  uEm4400 (0x74D9B0) caps a hit at its stages (6000, 3000), the uEm4700 family (0x772270) never goes below 1.
- **What a hit is worked into besides the health**:
  - Parts: 0x94D560 (thiscall (part, amount), ret 8) adds to one of 32 parts - each (health, most) at +0x3BE0 + 8n,
    a bit each in the broken mask +0x3CE0 (a part at 0 is broken and stays so: it returns 1 then); 0x94D500 sets a
    part's health, 0x94D540 its most. From slot 126: Deborah (0x761FC0, her cores), the zombies (0x7D4080, their
    limbs), the Whopper (0x800940). From slot 129, the hit's own damage: the J'avo family (0x66F9F0: the part a
    mutation grows from), Rasklapanje uEm4700 (0x76B920), Ogroman uEm4800 (0x78CBD0, which runs slot 126 again on the
    hit), uEm6300 (0x865950), uEm7600 (0x8D4890), Haos uEm8000/8001/8010 (0x8DAFE0). Also uEnemy's slots 172 and 185
    (0x965000, 0x968390: a grab's), and scripted blows (Deborah's 0x762970 event 0x52, her 0x7624C0).
  - Stagger: two gauges (floats +0x3B84, +0x3B88; their most +0x3B8C, +0x3B90; used +0x3B9C/+0x3B9D; standing
    +0x3B9E/+0x3B9F, 0 once emptied). The base slot 126 (0x95D290), with a parameter set of the enemy's own (+0x3D80:
    cEmParam, DMG_RATE_* and RESIST_* by weapon), runs 0x952F70: the damage by the hit's attributes, and each gauge
    less what slot 127 makes of it (a class's RESIST_RATE_* by region and state). An emptied gauge is the class's
    brain's to react to (a flinch, a fall): its slot 39, the damage reaction (Deborah's uEm4500Intel 0x766150;
    Iluzija's 0x75ADD0), or at no health its slot 40, the death reaction (Deborah's 0x766500).
- uEnemy's slot 117 is something else (0x95DA80: it drops the enemy's weapon), not a hit.

## Mutated Deborah (uEm4500)

- The game's own kill table (0x1795C18: 87 records of 8 bytes, +1 the title counter, +5 the kind - 3 an enemy - and
  +6 its class number; counted at 0x1794BF4) names enemy 0x4500 "Deborah" (title counter 150, "Kill 3 Deborahs");
  0x4510 is her cutscene model (uEm4510Cut). uEm4500 < uEmZombieBase, 0x4330 bytes; only stage s0203 (Leon's
  chapter 2) places her - `soft\stage\s0203\s0203_em*` (XFS lists of cSetInfoEm4500):

  | placement | mThinkBit | mIsNoDead | life | her fight |
  |---|---|---|---|---|
  | SN10_Deborah0, SN11_Deborah1, SN11_Deborah0_MD | 0x9000 | yes | 5000 | the first meetings: her health only |
  | Deborah2_coopdemo, dummy_Deborah2_battle, Deborah2_after1stDown | 0x9004 (bit 2) | yes | 30000 | three cores |
  | Deborah_Truck1/2/3 (Deborah_Truck_Appear 0x8000) | 0x8001 (bit 0) | no | 5000 | the mine cart: three cores |
  | Deborah_Last | 0x8002 (bit 1) | yes | 3000 | the last: hits where her cores were go to the front one |

- mThinkBit becomes mThinkAttr (+0x3A88). Her setup (slot 5, 0x7639E0): bit 0 sets bit 1 of her flags word +0x2A08
  (again each frame, slot 77 0x765300) - on the cart her damage code hands on no health at all; bit 1 sets +0x2A08
  bit 2, bit 2 its 0x1000. 0x761370 gives the cores their life from her parameter set: LIFE_CORE_FRONT/LEFT/RIGHT
  (+0x118.., 600 each in her file `soft\chara\em\em4500\em4500`, a PRPZ-wrapped XFS) or on the cart
  LIFE_CORE_TRUCK_* (+0x124.., 250 each); her file's damage rates: body, head, arms, legs and feelers 0, WEAK 1.0 -
  outside the last fight only a core takes health; the last fight's rates 1.0, its WEAK 2.0.
- Her damage worked out (slot 126, 0x761FC0) by the region hit (slot 61: 0x14 the front core, 0x15 the left, 0x16
  the right, 0x17): in the core fight and on the cart a core's region takes rate x damage off that core (part 0-2),
  held at 1 while mActStatus (+0x3A90) bit 12 is set; a core broken (and slot 58 yes) sets +0x2A08 bit 3/4/5. In the
  last fight region 0x17 takes the damage as it is off the front core. The health it hands on: 0 while +0x2A08 bit
  1 (kSigEnemyNoLife, its end). With all three bits set the cores' markers go (0x94CAD0).
- So One hit kills on her health alone moved only the first meetings: she is mIsNoDead in every fight but the cart's
  (kept at 1 point), and on the cart no health is taken at all - the cores took their full 600 / 250 each.

## Brzak (uEm4200), the water's "Jaws"

- The kill table's enemy 0x4200 is "Brzak" ("Kill 2 Brzaks", title counter 147); the stage scripts call it Jaws
  (`soft\stage\s0204\...` em4200_Jaws, `s0250`: jaws0, jaws_catch, jaws_slider, jaws_inWater, jaws_last, jaws_water,
  jaws_crashWall, Coop1-3_Jaws). The game's stage order (0x1794718, words) runs `... 202, 272, 203, 204, 206, 207, 209,
  279, 210, 250`: s0204 (it lurks, "tracking its prey by sensing movement in the water"; mThinkAttribute 0) comes
  straight after Deborah's s0203; s0250 is its fight - it surfaces and retreats three times (FSM RSF_60/61/63
  JawsAppear, RSF_70/71/72 JawsRetreat), transforms (RSF_83), and the TNT ends it (RSF_86 TNT, RSF_87 JawsDead,
  LeonKilledJaws). Medals: "Defeated the B.O.W. lurking in the water" (108), "Fed 5 zombies to the B.O.W. in the
  water" (165).
- Its damage worked out (slot 126, 0x7285E0; kSigEnemyHitCount) runs uEnemy's own on the hit (0x95D290: its stagger)
  and hands on 1 whatever the hit; its life 15000 at its setup (0x72FC82), 32700 in s0250's placements - never ended
  by damage, and it has no death action. Its file (`soft\chara\em\em4200\em4200`): RESIST_MAX 1000 (gauge 0), RESIST_*
  by weapon (handgun and shotgun 1.5, sniper 2.0, knife 50); its slot 127 (0x729EE0) by state and region (10 the
  tongue; FUN_0062E420(0, 0x17) which tongue): in the water (mThinkAttr bit 2) RESIST_WT_BODY 0, _TONGUE 1.0, _SP
  1.0; on the slide (bit 3) RESIST_SLD_BODY 0, _TONGUE1 1.0, _TONGUE2 0.4, _TONGUE3 0, _SP 0 - only its tongue feeds
  its stagger. WT_DMG_CNT 6 and SLD_DMG_CNT_MOVE/NEARMOVE/ATTACK 50/8/15 are in its file but no code reads them.
- The pending hits (0x9425F0, the brain's: the hit records, then 0x95CF00) choose a reaction unless +0x3B33 is set:
  its brain's (uEm4200Intel) slot 39 (0x736E30) - on the slide (0x736120) a broken gauge 0 re-armed and 0x51/0x5E/0x65
  by the action; in the water (0x736090) the same for action 0x50 when the second character is not the AI's, 100 -
  or, at no health, its slot 40 (0x7361B0): 0x65 on the slide, 0x55 in the water. Its own actions set +0x3B33
  (0x72C1A0 always, 0x72C3DB for actions 0x11, 0x14, 0x3C); jaws_water is mIsNoDead.
- So One hit kills on its health either did nothing (+0x3B33, mIsNoDead: kept at 1) or took all 32700 - a dead mark
  the game never gives it, and the death reaction at each hit in place of the one the game picks; its stagger took its
  usual tongue hits. Now its life counts as kept - One hit kills takes it to its last point, never 0 - and a player's
  hit is worked out big: one tongue hit empties its stagger, and the game's own reaction follows.

## Haos (uEm8000, uEm8001; its lower body uEm8010), Chris's chapter 5

- Its mode +0x41C4 is its placement's `mSetType` (uEm8000's one reflected property). Played in the order s0972, s0903,
  s0973 (the stage files' `_em*` lists, read 2026-11-13; their scripts `fsm\plan\*` - XFS, the conditions in each
  plan's `mpConditionTree`):

  | stage | placement | mode (its actions) | what moves it on |
  |---|---|---|---|
  | s0972 | em00, uEm8001, mIsNoDead | 0, the climb (`Climb_*`) | its damage count (+0x41C8 += the damage; the flag +0x41CC at 2000) passing during its block (0x15/0x16 Climb_Atk_Punch_Stopper) or a side move's end (5, 0xE): Climb_Dmg_Small (0x17), which writes the stage's scene itself - 30, or 60 once RSF 81; the script removes it at scene 100 |
  | s0972 | em01, uEm8001 | 1, the chase (`Upper_*`) | its death: `em01_set` waits for it gone or dead (EnemyCheck act 0x2A: the unit not active, or +0x2A08 bit 31), then scene 190, 200, cutscene 2270; its death reaction (0x8ED450) is DeadKuzure 0x65, then DeadErase 0x66 |
  | s0903 | em00, uEm8000 | 2, on the pipes (`Pipe_*`) | the stage's RSF 100-106 move it (its think 0x8EC730); its count (to 1000) passing while a hand holds on (0x27/0x29) sets RSF 80/81 and Pipe_Dmg_Hand (0x2E) - the barriers (`block1/2`) wait on those; nothing waits for its death |
  | s0903 | em01 | 4 (`Piers_*`) | its death: RSF 310 (0x136), which `Tji_MiniDemo` waits on |
  | s0973 | em00 | 4 | the same (`em0_set`: RSF 310, then EnemyKill) |
  | s0973 | em01 (+ an uEm8010), mThinkAttr 0x10 | 3, the last fight (`Final_*`) | below; set at scene 118, its end RSF 300 |
  | s0973 | em20 | 3 opened (mThinkAttr 0x6) and 4 | named "for the finishing blow" and "for damage" |

- The last fight (mode 3; its setup 0x8EA720 and 0x8DAC60 set mIsNoDead and 10000 health). Its actions (the table
  0x17534B0, a record and its number each): 0x44 Final_Dmg_Head, 0x45/0x46 Final_Dmg_Weak, 0x4A/0x4B
  Final_Dmg_Separate, 0x52 Final_SanagiChange, 0x53 Final_SanagiMozomozo, 0x54-0x56 Final_SanagiBorn, 0x5C/0x5D
  Final_Atk_HeartCatch_Hit, 0x60 Underbody_Dmg_Separate (uEm8010).
  - Born (SanagiBorn's start, 0x8E5330) whole (+0x2A08 bit 4 clear): its head (part 0; hit group 1, mesh +0x140 bit
    7) and its two waist blisters (parts 4 and 5; groups 3 and 2, mesh bits 10 and 11) shown, every part 1000 (0x952190).
    Its damage worked out (slot 126, 0x8DDE80) counts the head x2, the rest x0.1; its parts (slot 129, 0x8DAFE0):
    region 1 part 0, 2 part 4, 3 part 5, 4-6 parts 6-8 (a region its slot 61's, a linked model's - the cocoon's - too).
  - Its damage reaction (uEm8000Intel's slot 39, 0x8EE770): the count's flag with part 0 broken - down (0x44, its
    head); a blister's part broken - that blister bursts (0x46/0x45: hidden, +0x2A08 bit 8/9), the second Separate
    (0x4A/0x4B: the lower body falls off - uEm8010, 800 health, which blows up on its own; no reactions meanwhile,
    +0x3B33). Separate done: SanagiChange (the cocoon, an uEm8030 linked at +0x41E0; health 10000, +0x2A08 bit 10),
    then Mozomozo, in which health below 2 (scene 120 on) sets +0x2A08 bit 23; it wriggles six times (once, opened),
    and its exit (0x8DF290) makes a new uEm8000: opened if bit 23 (+0x2A08 bit 4: three hearts, parts 6-8 at 100,
    groups 4-6), else whole again ("He just keeps coming back! We have to kill him when he's in his cocoon!").
  - Opened (SanagiBorn 0x55): its slot 11 (0x8DCAF0) puts out the hearts' catch points - the player's melee on a heart
    is HeartCatch_Hit, whose end (0x8DD0A0) takes a heart (+0x2A08 bits 7, 6, 5); the third sets RSF 300 (as slot 156,
    0x8DB050, does), and the finale follows (0x57, its QTE). A shot heart does nothing. The catch's condition
    (0x952600, handed to the catch points) needs Haos' health at 1 or more (mIsNoDead keeps 1) and reads no part; its
    start (0x95C3F0) checks the player's health only. An opened Haos has no damage reaction (with +0x2A08 bit 4 set
    every region's case gives none) - so a head or heart broken at one hit leaves its prompts as they were.
- One hit kills: the climb and the last fight are mIsNoDead - a player's hit on them is worked out big (the climb's
  knock-back, the last fight's down, each blister, the cocoon and the hearts' parts at one hit; seen in the log of
  2026-11-13 16:26-16:28: a blister, the head, the other blister, the next Haos 10 s later - opened, its head and a
  heart shot, not taken - and a whole one at 16:27:45); on the pipes (mode 2) to
  its last point with the big hit; the chase (mode 1) and mode 4 die at one hit.
  [I: the climb's knock-back at any hit in its block or a side move's end - each writes the stage's scene (30/60), so
  it can be written back from 40-45 to 30.]

## The bosses and One hit kills (the audit of 2026-11-06)

Every boss's fight checked for what moves it on (its health, parts, stagger, counters, the stage's scripts - the FSM
process EnemyCheck 0xD45AF0 tests a unit's health: act 0x2F health <= n, 0x30 health% <= n, 0x2A gone or dead) and
what One hit kills does to each piece. The stage order (0x1794718, words): `101-105, 200, 201, 202, 272, 203, 204, 206,
207, 209, 279, 210, 250, 300-307, 400-402, 500-516, 550-552, 600-602, 700, 770, 701, 751, 771, 702, 703, 773, 705, 706,
800, 801, 871, 802, 872, 804, 851, 901, 902, 972, 903, 973, 904, 905, 950, 1000, 1001, 1003, 1100.., 1200..` - stage
numbers go by place, not campaign (s02xx Tall Oaks underground, s03xx Edonia, s05xx Lanshiang ...).

| boss | class(es) | stages | what moves its fight | One hit kills now |
|---|---|---|---|---|
| Lepotitsa | uEm4100 | s0201, s0552 | its death (FSM `LepoDeadCheck`, `Carrier1DeadCheck`) | dies at one hit; `Lepo2` (mIsNoDead) keeps 1, gauges big |
| Deborah | uEm4500 | s0203 | cores (parts 0-2); health in the first meetings | each core at one hit (above) |
| Brzak ("Jaws") | uEm4200 | s0204, s0250 | its tongue's stagger; the TNT script ends it | to its last point; one tongue hit staggers it |
| Ubistvo | uEm4300 | s0509, s0578, s0579 | its death; the mIsNoDead ones' scripted kills | dies at one hit; the kept ones' stagger at one hit |
| Iluzija | uEm4400(_s504) | s0504, s0507 | its own slot 44 caps (6001, 3000, then 1 until the script lifts it) | a phase a hit |
| Rasklapanje | uEm4700Base family | s0511, s0802, s0872, s0904 | parts: upper and lower body, arms (1000/1000/500/500; uEm4700 slot 129 0x76B920, its brain 0x775060 severs and splits) | its floor keeps 1; each part at one hit |
| Ogroman | uEm4800(_s300/_s301/_s851) | s0300-s0302, s0851 | parts: heart 420, electric 800, abdomen 7000, stagger 500 (slot 129 0x78CBD0); its own slot 77 sets mIsNoDead in its kept states | kept: each part at one hit; else its own `Die` at one hit |
| Simmons | uEm6000/6040/6100/6200/6300/6500-6502 | s0514, s0702, s0706, s0773 | every form's setup sets mIsNoDead; FSM `EnemyCheck` health <= 10; gauges; parts (6300 0x865950, 6500 family 0x87A6D0) | health to 1 passes the checks; gauges, parts at one hit; the half-human 6500 form 0 dies at one hit |
| Ustanak | uEm7000-7500 | s0305, s0306, s0402, s0510, s0905, s0950 | knockdown count (+0x446C >= 300), guard (uEm7100 +0x47D0 vs 1500), gauges; s0510 `HP_chk` health <= 1; the scripts' `EnemyKill`; its brain has no death reaction | to its last point; each count and gauge at one hit |
| uEm7200 | uEm7200 | s0402 | can never be hurt (slots 126, 127 hand on 0) | as the game |
| Haos | uEm8000/8001/8010 | s0972, s0903, s0973 | mode +0x41C4 (mSetType), "Haos" below: the climb's knock-back, the chase's death, the pipes' hand flags, the last fight's head, blisters, cocoon and hearts | mode 2 to its last point; the chase (1) and mode 4 die at one hit; the climb and the last fight (mIsNoDead) the big hit |
| Gnezdo, Napad, Strelats, Mesets, Oko | uEm3300/3320, 3200/3700, 3000, 3100, 7900 | various | their deaths | die at one hit (Gnezdo's swarm body: under 100 raw damage nothing, as the game) |
| choppers, cars, bikes, tanks | uEm9900-9912 | s0301-s0306, s0400, s0506, s0508, s0550, s0551, s0602, s0751 | their deaths; tanks (setup mIsNoDead, slot 126 0) and the s0306 helicopters (mIsNoDead, scripted crash) only by script | die at one hit; the s0508 chopper to its last point until scene 150 |

- [I: Ogroman in s0851 before scene 221 - its slot 126 hands on no health and it is not mIsNoDead, so its stagger
  (gauge 3000) takes its usual hits; Napad in its scripted state (+0x4C0 bit 31, actions 0x27/0x28/0x3B/0x3C) keeps a
  point by its own slot 126 (0x707EC0) for a killing hit, which One hit kills' raise overrides; the same for Strelats
  mid catch-kick (0x6EE391) and some zombies (0x7D4ED0): they die mid-grab; the s0579 Ubistvo's threshold reaction at
  health < 6001 (stage flag 200) skipped when it dies at one hit; uEm9910 (an s0551 set piece) marked dead with no death
  action.]

## The room bombs (Leon's chapter 4's floor robots)

- The robots that roll along the floor at you and blow up are set models, not enemies: uSm1310 < uSmRoomBomb <
  uSetModel (0x30E0 bytes), placed by cSetInfoSmRoomBomb - six in stage 512's set list `s0512_k19`; the stage's scripts
  call them "Rumba" (`Rumba_4`, `Rumba_6`, `SN_158_Last_Rumba_1Shot_DEMO`, `MES_SN_158_Rumba_Loser`). None of the
  enemies' hooks see them.
- Its actions (slot 101's table, 0x1649098: 11 entries): 0 and 1 Wait, 2 Start, 3 Move, 4 Hit, 5 Warning, 6 Bomb, 7 and
  8 Sleep, 9 Hit, 10 Bomb. Its events: slot 80 (uSetModel's 0xAD5D20) posts one - to the network when this machine is
  not the host, else straight to slot 81 (0xBB99B0, its own): 4 Hit, 5 Warning, 9 a hit (a point taken; then action 9
  with arg 1, action 10 with 0); the others through uSetModel's (0xAD5D90 -> slot 82, 0xBB78E0: the action of that
  number).
- Its points: its setup (slot 5, 0xBB8D40) writes 0x50005 at +0xF10 - 5, its most 5 (the characters' health word) -
  and so does each start over (Wait's start, 0xBB8E80: back to its start point, its flags +0x2A08 cleared - bit 0 a
  Hit, bit 1 a Warning, bit 2 a hit pending). Bomb's frame (0xBB79D0) sets Wait (1) after a while, and Wait (1) Start:
  a bomb that went off comes round again unless the stage puts it to sleep [I].
- A set model's hit record (0x5E64D0 copies a hit into it; a hit of 0 clears it): +0x2B10 valid, +0x2B14 the damage,
  the attributes +0x2B18 (three dwords, the hit's +0x10..), +0x2B5C the dealer (the hit's +0x28).
- While it rolls (Move's frame, 0xBB9B90): a player in its collision -> event 5, its warning. Its hit check (0xBB94A0,
  kSigBombHit): a hit pending; or a hit with damage whose dealer's class id (0xAD2EC0) is the players' side - type 1
  (the players' characters), 0x062005xx (uSm5009, uSm5034-5036 and ten more set models) or type 7 below 0x150 (uWp*,
  uIt0000) - and is not played over the network (+0x4E90 != 2); then, if the game's instant test (0xAD2F20: the
  record's attribute bit 32 or 34) says so, its points to 0 (0xBB8BE0). 0xBB83D0 posts event 9 - with 0 when no points
  are left, else 1: a hit takes a point (action 9), and the hit after its last sets it off (action 10). 6 hits in all.
- Its warning (Warning's frame, 0xBB9F90, kSigBombWarnHit): the same check, then 0xBB8BE0 (all its points) or 0xBB8BB0
  (one); none left -> event 10 (it goes off as shot: action 10, effect 3); else after a second, event 6 (it goes off by
  you: action 6 - its slot 77 and effect 1).
- One hit kills: the two calls of the instant test (0xBB94F5, 0xBB9FD9) are re-pointed at the mod while it is on, and
  the mod says yes - so each hit the game counts as the players' empties the points through the game's own 0xBB8BE0,
  and the bomb goes off as shot. The test's other callers are breakable scenery (0xB25090: one of a breakable's kinds;
  0xB25BD0, a breakable taking 300 damage), left alone.

## The switches

- **God mode / Partner takes no damage** (`cheats.cpp`): every vtable of a uPlayer class gets the mod's slot 44
  (subtract-health: a spared character keeps its health), 117 (a hit on a spared player reaches the game with its
  damage 0 - put back after, as one hit can be another's - so the reaction is a hit's that does nothing: a flinch, never
  a fall) and 121 (take-damage: a spared player is let off as the game lets off a campaign's AI partner - nothing
  taken, not down). 54 vtables for 44 and 121, 51 for 117 (uPlayer, uPlEnemy and uPlEm1000A have the base's empty
  117). Spared: driven here with God mode, by the AI with the partner's switch; never the network's. And slot 43,
  uPlayer's health add (0xA63AE0, kSigPlayerHealthAdd; thiscall (amount, a word), ret 4: a heal, getting up from
  dying - every change the game makes as an add; 54 vtables): a spared character loses nothing through it, and any
  add it is given fills it up. No hold each frame (until 2026-11-12 the tick put the health back at its most every
  frame): as the switch goes on, and as a spared character comes into play, one between 2 and its most is filled up
  once (one the game itself puts down by setting 1 is left to it); the hooks keep it there. Health a function of the
  game's writes without these (none seen) stays as written until the next of those. A ride a spared character rides
  (Ada's helicopter, Chris's jet, the bikes, jeeps and snowmobiles) takes nothing either: "The rides" below.
- **One hit kills**: every enemy vtable's slot 128 (the game's or an override that calls it first): a hit dealt by a
  uPlayer (yours or your partner's - the hit's +0x28) with damage takes all the health there is; the game's own death
  follows, or the point it keeps. Damage between enemies, and to allies, stays as it is. The same hook asks
  `merc::lethal` first (2026-11-14), whatever this switch: a hit Kill enemies as they appear wrote takes all the
  health (`docs/modes.md`, "The Mercenaries").
  - An enemy that keeps its life through such a hit has its fight go on through its parts, its stagger and its own
    counters. A player's damaging hit on one is worked out as 32767 (the most health a character holds; a word, as
    the game reads a hit's damage): in slot 126 (hooked in every enemy vtable, whatever each class has there - 191; a
    table only where it is a vtable that far: `docs/engine.md`) on the handler's copy of the hit, and around slot 128
    on the hit itself (for slot 129), the damage put back after.
    Each part then breaks, each gauge empties and each counter passes at one hit through the game's own code - with
    its own limits kept (Deborah's core held at 1 under mActStatus bit 12 stays at 1). Which keep their life, each
    found from the game's own code:
    - the game keeps a point: mIsNoDead (+0x3B32) or the take-damage's other flag (+0x3B33) set (kSigEnemyKeepAlive,
      read where slot 128 reads them; +0x3B33 is also what the generic grab (uEnemy slot 172) and the J'avo's, zombies'
      and dogs' catch actions set while they hold a player - a hit on one then is worked out big too: it staggers, and
      a part the hit lands on can break, as from any big hit);
    - its own code keeps it: Deborah while her damage code hands on no health (slot 126 = the function holding
      kSigEnemyNoLife, and +0x2A08 bit 1); Rasklapanje, whose subtract-health (slot 44 = kSigEnemyFloor, 0x772270)
      puts health back to 1;
    - its end is the stage's script, so One hit kills takes it to its last point (amount = health - 1; never 0, never
      the dead mark the game never gives it): Brzak (slot 126 = kSigEnemyHitCount: 1 a hit); an enemy whose brain has
      no death reaction (its mpIntel +0x3A54, kSigEnemyIntel; the brain's vtable +0xA0 - kSigEnemyReaction, the pending
      hits' choice - is uEmIntel's own `or eax,-1; ret` 0xA242B0: the Ustanak family uEm7000-7500, and the NPCs
      uEm5100/5120); Haos on the pipes, mode 2 (slot 126 = the function holding kSigEnemyHaosMode, its mode +0x41C4,
      the placement's mSetType: stage 903 moves on through its hand's reactions, which its death would end - "Haos"
      below; until 2026-11-13 mode 1 too, which stage 972 needs dead); the attack
      choppers (slot 126 = the function holding kSigEnemyStageHold, 0x906790) while their own hold applies - the
      profile's cGameInfo stage (+0x1B0C) 508 and scene (+0x1B14) below 150, read where the hold reads them.
    An enemy that can die is left as before: its health ends it. Logged at the start: each rule found (`cheats: One hit
    kills: ...`); in play, once an enemy, `cheats: One hit kills: uEm4500 ... keeps its life (<why>) - a player's hit
    on it is worked out as 32767`, and `... part(s) 0x1 broken at one hit` at each break.
  - [I: the stagger at every hit on such a boss; the bosses below whose pieces are only seen offline.]
  - The room bombs (set models, above): their two calls of the game's instant test re-pointed at the mod while the
    switch is on (`follow`, as the tablets' calls) - a hit the game counts as the players' sets one off at once.
    Logged at the start (`cheats: One hit kills: the room bombs (uSmRoomBomb, set models) - their hit checks ...`) and
    once a bomb (`... a room bomb (uSm1310 ...) hit by <dealer's class> - blown up at one hit`). [I: in play.]
- **Infinite ammo**: the game's own bit - uPlayer +0x4E74 bit 11 (0x800) - under which its use-an-item (0xA62730:
  slot, count) takes nothing, and its reload (0xA523D0) fills the gun from nothing. The game sets it itself for a
  scene's mounted gun (0xB6AF40). The skill 0x54 does the same in the use function (0x54FD20 asks the skills).
  The mod sets the bit on the characters driven here as the switch goes on and as one comes into play, and clears
  only what it set (as the switch goes off, or the character goes to another) - events, not a write each frame. The
  game clears the whole word only as it sets a character up and in uPlayer's reset (slot 109, 0xA74B30,
  kSigPlayerReset - a Continue, a stage's start; no arguments: it ends in tail jumps to 0xA73F00 and 0xA55010, whose
  `ret` ends it), through the flags' reset it calls (0xA73480: `mov [esi+0x4E74], ebx`, found and checked against the
  bit's word; the reset's own store of the slot in hand, +0x46D8, checked against the inventory's). The reset is
  hooked in 54 vtables: the bit is put back after it where the mod had set it. Nothing else of the game's clears it.
  - The bit spares every item the use function takes, the health tablets (item 0x11D) too: a heal of yourself
    (0xDC7073), of your partner (0xDBBBD9) and the tablets set aside that a hit takes (0xACC532). Those three calls
    are re-pointed at the mod while the switch is on (found as a call of the use function after `push <tablets>;
    call find-slot (0xA51AF0)`, the tablets' id read from kSigTabletUse): it lets the bit go for the call, so the
    tablets are used up as usual. The other ten calls are the guns' and the ammo types' (0x113-0x116, 0x120).
  - Ada's helicopter's missiles are its own count, which the bit never reaches: their two takes are NOPs while the
    switch is on ("The rides" below).
- **Infinite skill points**: the skill screen's two purchases (0x9F5B4C a skill, 0x9F5D76 its next level) take the
  price off its copy of the points (`sub [esi+0x588], reg`, 6 bytes each), then hand the copy to sInfoMan's
  set-skill-points (0x9F1FD0). Both subtractions are NOPs while the switch is on.

## Steady aim

- The sway (0xCC6D30, thiscall (player, a, b) on the weapon, ret 0xC): two slow random swings of the player's (+0x4AAC,
  +0x4AD4; their size the weapon record's +0x20 + 4*level, degrees, by the sway skill 0x2E/0x2F) give two angles. Its
  three callers: the over-the-shoulder aim ray (0xCC979D), the gun's line in a scope or first-person view (0xA644D3),
  the scope camera (0x62CA63). It has a way out for no player that writes 0 and 0: its `jne` past it (+17) NOPed, every
  caller gets 0.
- The kick (0xABAA40, uPlIntel's pad handling): a shot (+0x4E74 bit 6) adds the record's +0x2C + 8*level (skills
  0x30/0x31) to the climb (+0x4AFC, 0..1: the reticle lifts by up to 0.2 of the half screen; over 1 the camera pitches
  up). Its `je` past the kick (+32, `0F 84` -> `90 E9`, the same place) takes the frame's own "no shot" way; what has
  climbed still comes down.
- Both global (the AI's aim may steady too); the network's aim is its own machine's. RE6 has no bloom: the reticle's
  ring is a fixed size per weapon and skill; only a shotgun's pellets are random.

### Through a scope

- Seen in play 2026-11-12: with the sway at 0 a sniper's scope still swayed (`docs/history.md`). Through a scope the
  view is the gun model itself, and the gun is in the hands of the aim pose - an animation that breathes and kicks.
- The scope (0xA50840 says when: a weapon of the sniper rifles' kind - 0x109-0x10B, 0x151 - or the Rocket Launcher's
  - 0x11B, 0x154 - in an aim or fire state; the Sniper Rifle's drops once its fire state's time +0xF68 reaches 0.6,
  the Anti-Materiel Rifle's 0.5, for the bolt): the scope's start (0xA54AF0; the player's scope byte +0x4DE0) puts
  the scope camera (the player's camera +0x3324, its +0x47D0) in as its camera mode 2 (0x5FD9C0 -> 0x5FD200 with the
  callback 0x62E1D0; stage 550 scene 166 another, 0x62D5F0 by 0x62E250, which does not call the sway). The camera
  (0x62C840, fastcall (camera) -> al; its player +0x80, the zoom from the player's +0x4EDC) sets its eye at the
  weapon's joint 100 (its matrix by the weapon's joint table +0x398 into its joints +0x394, 0xA0 each) plus an offset
  along it (110 for 0x109, 100 for 0x10A, 35 for 0x11B, 120 else; +12 unless lying), looking at a point down the
  joint's axis (the weapon record's +8), the sway turning both. The callback copies its eye +0x50 and point +0x70 (fov
  +0x4C) out to the camera list. Three callbacks of that shape (0x62BC80, 0x62E1D0, 0x62E250): the scope's is the one
  whose camera calls the sway.
- The gun's line (0xA64240, from the laser's 0xA6CD80 at +0x64; fastcall (player)): in a scope or first person
  (0xA55100) down joint 100's axis turned by the sway, from the weapon's joint 103 (or 100): +0x4A30 from, +0x4A40 to
  (+0x4A50 its copy, the next frame's blend; +0x4A80 the direction, its own; +0x4A70 the raw point, which the hand's
  calibration 0xA4C7B0 reads - left alone). A shot (0xCCBAF0, the weapon's slot 107) fires from +0x4A30 toward
  +0x4A40; the laser draws it (0xA6CD80, and 0xA64F70 stopping it at a wall).
- The aim through a scope is the pad handling's second way (0xABAA40 after its first: the player's scope bit +0x4E6E
  bit 0, lying +0x4E68 bit 4 or the grenades take it): the brain's (+0x4E50) pad part (+0x480) has the pitch
  `mAimRateXOrg` +0x84C (degrees / 90: -1 up .. 1 down; its copy +0x838) and the waist's twist `mWaistAngleYOrg`
  +0x848 (degrees, to a most of sPlayer's +0x16C, +0x170 in cover); only the pad and the mouse move them. 0xA45DA0
  copies them to the player each frame: +0x4ECC the turn (+0x834), +0x4ED0 and +0x4ED4 the pitch, +0x4ED8 the twist,
  +0x4EDC the zoom (+0x83C). The pose blends `normal-U` / `normal-D` (and `lie-`, `coverH-`, `coverL-`) by the pitch
  (0xA56180). The first way (the over-the-shoulder guns): the player's camera's angles (+0x1800 pitch / 90, +0x1804
  yaw; this frame's +0x1818 / +0x181C) and the aim point (+0x4E00: 300000 along the camera's view, 0x60AB40), the pose
  aimed at it from joint 2 (0xABC150; with a scope up the point follows the scope's own view, 0x60AB40 by mode 2).
- The game's angles (0xE4E7E0, 0xE4E8E0): yaw atan2(x, z), pitch atan2(-y, across): down positive. A model's world
  matrix +0xC0 (uModel's joint matrix, slot 20, for a joint below 0; the players' slot 20 is uModel's): rows the axes,
  then the position.
- The switch (`cheats.cpp`, "Steady aim through a scope"): the callback's call of the scope camera (0x62E1DF) and the
  laser's of the gun's line (0xA6CDE4) are re-pointed at the mod while it is on. After the game's camera has worked
  its view out, the mod holds it in the body's frame (the eye and the direction) while neither the pitch nor the twist
  moves, and while one does follows the gun's own motion on that axis (the twist the yaw, the pitch the pitch, the
  held pitch kept within 88 degrees; the eye with either) - the whole of the gun's turn while the body is not upright
  (lying: its up along the ground); the body's own turn carries the held view. As the scope comes up the view is the game's until the
  gun is still in the body's frame (below 0.1 rad/s) for 0.2 s, or 1.5 s at the latest. After the game's gun's line,
  the mod points it along the held view (as long, from where the game has it) while a view of that player's is held
  (seen this frame or the last; the frames counted while not paused). Logged at the start (`cheats: Steady aim: the
  scope's view (exe+0x22E1DF ...) and the gun's line (exe+0x66CDE4 ...) re-pointed while on ...`) and as a view is
  held (`... a scope's view held 0.2x s after it came up (the gun settled)`, eight at most). [I: in play - how the
  hold feels, a moving target tracked, the bolt's return to the scope, a scope while lying (its eye set by the
  camera's own `+0x200` path too), the stage 550 scene 166 camera left to the game.]

## Automatic QTEs

- Every button prompt (doors, items, melee finishers, every QTE) is a request (cActionButton, 0x8C) of sActionButton's
  (0x17C2CF0). Each frame each player's manager (ButtonMgr, 0x330) asks its two units (0x84) through one check,
  0x4EA2B0 (thiscall (pad, player) on the unit; called only at 0x4EA8C4), then calls the success 0xD03EC0. The check
  switches on the unit's kind (+0x14): 1 press, 2 hold, 3 let go, 4/0x17 two at once, 5/0x12 mash, 6 double press
  (counters), 7/8 stick waggled, 9/0xA/0x10/0x11/0x18/0x19 stick turned (cranks), 0xB hold for a time, 0xC/0x13 press
  in time (0xC with a good window from +0x40 %, marking the request's +0x78 "good"), 0xD-0xF timing rings, 0x14 the AI's.
  A dodge made at a low level is a kind-1 press with the request's flag 4. The AI answers its own character's itself.
- The switch re-points that call at the mod: the game's check first; when it says no, a QTE (kinds 4-0x13, 0x17-0x19,
  a dodge's press) of a character this machine drives, not failed, not answered, input not held back (sPlayer +0x418C,
  a bit a player) is answered yes and not drawn (+9). A press in time waits for its window (or answers before it would
  run out) and is marked good. Plain presses, holds and let-gos (doors, items, cover, melee, the partner's help) are left.
- The PC port's option mExOptAutoActBtn (cSystemInfo +0x7084: sInfoMan +0x70BC + profile * 0xFDD8; the options page
  uIdOption13's sixth row) is read only by 19 tests in about 11 scenes (`difficulty == Amateur || option`: uEm4200 and
  its brain, cEm4400's classroom attack, uEm4700's brain, cEm7500's punch, cSmMiniGame, the bike and snowmobile
  vehicles, cSm5000, cSm9062, the revolver cSm9064, the catch cSm5905): while the switch is on and no character is
  online, each reads it as on (`cmp [..],0` -> `cmp [..],-1`; `cmp [..],ebp/ebx` -> `cmp [..],esp`). The saved setting
  is never written.
- [I: a counter or a crank answered every frame goes at top speed; a timing ring answered at once skips its windows'
  own callbacks; what the 19 scenes' Amateur paths look like.]

## The air under water (Infinite air)

- A character's air under water is its combat gauge (what melee and quick shots spend: "The combat gauge" below):
  uPlayer +0x3B00 (a float), its most +0x3B04 (300 below the blocks, then 100 a block: 800, 1100 or 1300). 0xA63B50
  sets it (kept to 0..most). uPlayer's slot 122 (0xA63B90) adds to it (nothing while the pause after a hit, +0x3B08,
  lasts), uPlPlayer's 123 (0xACE4C0) spends blocks (none under skill 0x54, Training), 124/125 give the blocks held and
  the most, 126 (0xA63C50) is the frame's refill: its rate +0x3B10 (each action sets its own) times the frame and the
  modes' factors (sPlayer +0x174/+0x178), through slot 122.
- The swim (cPlAction_pl_act_water_action; its enter 0xDD06E0): each state sets bit 29 of the flags word +0x4E6C (in
  water; 0xA45ED0 ORs into it), the dive's states (0xD-0x1B) bit 30 too (under water; 0xA4AA10 tests it). In states
  0xD-0x19 and 0x1C the gauge's rate +0x3B10 becomes sPlayer's dive rate (+0x184: sPlayer's cPlParam at +0x4C, its
  +0x138 - -8 by its constructor's default, the player's file may set another) for a character this machine drives
  (+0x4E90 = 0), and 0 for any other: the AI's partner never runs short. State 1 (the dive's start) and 0x1A
  (surfacing, at its motion's frame) put the gauge back at its most; 0x1A clears bit 30.
- The gauge on screen: uIdOxygen (`data\id\cockpit\oxygen_00`, `_03`; its frame, slot 8, 0x9E06A0) shows air over most
  while bit 30 is set and the character's health is above 0; below 0.25 it warns.
- The drowning (0x484750, cdecl (player)) is run by uPlPlayer's frame (slot 112, 0xACA330) in two stages only: 250
  (Leon's chapter 3, Brzak's water) and 1003 (Ada's chapter 1, the submarine) - the stage caches' names say whose
  (`S0250_s250_e0_c2`, `e4040_s1003_e3_c0`: the episode, the chapter from 0). When slot 58 says this machine runs the
  character (0xA4F9B0: +0x4E90 is not 2, the network's), bit 30 is set, the gauge is at 0 and bit 1 of +0x4E68 is
  clear: sPlayer's set-health (0x5939A0) puts its health at 0 and slot 46 (0xA45EF0) plays action 0x226 - the drowning
  (cPlAction_pl_act_water_die_action). No hit and no damage: God mode cannot stop it.
- Stage 552 (Lepotitsa's) runs another check there (0x4B2B50): in scenes 0x53-0x77 a take-damage (slot 121) every so
  often, and the down at under 2 points - a hazard's damage, not air; God mode covers it.
- The add (slot 122, 0xA63B90; thiscall (amount, a float), ret 4): nothing while the pause after a hit lasts (a hit
  with damage sets it to a second, slot 117 at 0xACC2A8; the player's frame counts it down, 0xACE696); else the gauge
  plus the amount, kept to 0..most - and a refill from 300 or below that reaches 300 lands at 400 (or the most, if
  less: the constants 0x1511B90 and 0x1521928), so from below 300 a fill to 500 takes two adds (400, then 500).
- **Infinite air** (kSigDrown, kSigAirGauge, kSigAirAdd): the add is hooked (52 vtables: every uPlayer class's that
  holds 0xA63B90): for a character this machine drives with bit 30 set, a drain is turned into a fill to its most -
  the air never runs down, so the drowning never comes. Nothing is written each frame (until 2026-11-12 the tick put
  the gauge at its most every frame). On land the gauge is the combat gauge: Infinite stamina's (below; the same hook);
  the AI's partner is the game's (no drain); a character played over the network is its own game's (the drowning
  passes it by: slot 58). The gauge, its most and the under-water word and bit are read out of the drowning (its
  test's own code) and checked against the gauge on screen (the same test, the gauge over the float after it, the
  characters' health). Log: `cheats:
  Infinite air: a character's air (the combat gauge) +0x3B00, its most +0x3B04; under water: bit 30 of +0x4E6C; the
  drowning exe+0x84750 (action 0x226)`, then at each dive `cheats: Infinite air: Leon is under water - the air (n of m)
  held at its most`.
- [I: in play - the gauge on screen full through a dive, and no drowning.]

## The combat gauge (Infinite stamina)

- The gauge (uPlayer +0x3B00, a float; its most +0x3B04) holds 300 below its blocks, then 100 a block. The players'
  most is sGameMan's 0x5500D0's (uPlayer's setup 0xA73917 and sPlayer's 0x5937A9/0x593DA7 put the gauge and its most
  at it): 800 - 5 blocks -, 1100 with Combat Gauge Boost Lv. 1 (skill 0x36: 8), 1300 with Lv. 2 (0x37: 10). Slot 124
  (0xACAA20) the blocks held: (gauge - 300) / 100, at least 1 above 300; 125 (0xACAA60) the most's. (0x559650 is the
  Agent Hunt creatures': 5 to 10 blocks by the creature stamina skills 0x5E-0x60, nothing below them.)
- What costs blocks is data: sPlayer's action table (+0x1B8, 12 bytes an action, 0x2C1 of them) - its first word's
  low nibble the cost, 0x10 no cost check, 0x20 paid later. The player's play of an action (0xA45EF0, thiscall
  (action, four more)), when slot 58 says this machine runs the character: refuses the action while the blocks held
  are fewer than its cost, else pays it through slot 123 at once - or, with 0x20, once the action's time (+0x4F48)
  passes a mark (0xA500A0, setting +0x4E68 bit 21). Melee moves, a quick shot.
- The spend (uPlPlayer's slot 123, 0xACE4C0; thiscall (blocks), ret 4; kSigGaugeSpend; in 51 vtables - uPlayer's slot
  123 is an empty 0xEE2A50, uPlEnemy's its own): nothing for 0 blocks, under the Training skill (sGameMan's 0x54FD20
  (player, 0x54): the three skills at sGameMan +0x728 + player * 12 - none on No Hope, cGameInfo +0x1B00 = 4, nor in
  stage 0x303), or with no blocks held; else 100 a block off, kept to 0..most - and 300 or less left empties it.
- What else empties it: the down - slot 118's reaction for a hit that would down the player (0xACD253: health 1, the
  gauge 0) and 0xA6C2E0 (a scripted down - Iluzija's, Deborah's blow, the players' own actions: take-damage with flag
  8, then the gauge 0 when slot 58 says this machine runs it); in mode 0x32 a hit with its attribute bit 26
  (0xACC28E); and the refill itself, which puts it at 0 every frame and never adds while +0x4E80 bit 16 is set (a
  stage script's player flag: 0xD3F4C0's case) or the character's class id is 0x01000303 (uPl03PiersCanon: Piers with
  his mutated arm). While a character is down (0xA532C0 not 3) the refill does nothing. A hit dealt by another player
  with an attribute 0x118-0x11A costs a block (0xACC248, slot 123).
- The rates (+0x3B10, each action's): 0 while an action that paid runs; the drains of -50 and -100 a second (-10 in
  mode 0x46) and the raw spend 0xAA52F0 (15 calls) are the Agent Hunt creatures' actions' (cPlEnemyAction:
  0xA79950 ...; uPlEnemy's own add 0xAA5290 and spend 0xAA5390).
- **Infinite stamina** (kSigGaugeSpend, and Infinite air's kSigAirAdd, kSigDrown, kSigAirGauge for the gauge):
  uPlPlayer's spend hooked in the 51 vtables holding it - found in the slot after the add's, asking the slot after its
  own, taking from the add's gauge and keeping to the same most: for a character this machine drives nothing is taken
  (the game's own way under Training). The add's hook (Infinite air's): on land (bit 30 clear) any add is made a fill
  to the most - a drain too; at the most nothing; from 300 or less two adds (400, then the most). The player's frame
  runs the refill every frame (0xACE6D3, after counting the hit's pause down), so a gauge the game emptied is full
  again at its next refills once the character is up. Under water: Infinite air's. Left to the game: the AI's and
  the network's characters, what the refill keeps at 0 (the script's flag, Piers' arm), the creatures. Nothing written
  by the tick. Log: `cheats: Infinite stamina: the combat gauge's spend (slot 123, exe+0x6CE4C0; the game's own takes
  nothing under skill 0x54) hooked in 51 vtables - nothing spent; on land its add (slot 122) fills it`, and the first
  spend not taken of each character: `cheats: Infinite stamina: Leon's combat gauge (1100 of 1100) - a spend of 1
  block(s) not taken`.
- [I: in play - melee and quick shots with the gauge full throughout; which moves cost what (sPlayer's table).]

## The rides: Ada's helicopter, Chris's jet, the bikes, jeeps and snowmobiles

- uSmRide < uSetModel (0x3E50 bytes): uSmAircraft < uSmPlAircraft < uSmPlHarrier - uSm5020 Chris's jet (stages 851,
  872), uSm5060 Ada's helicopter (`sm5060_Ada_Heli`: stages 701, 703, 706, 751, 771, 773); uSmVehicle < uSmPlVehicle
  (uSmPlBike uSm5012, uSmPlJeep uSm5034/5036/5036i/9065, uSmPlSnowMobile and the enemies' uSmEmSnowMobile uSm5009),
  uSmEmBike (uSm5011), uSmEmJeep (uSm5028v-5031v, uSm5035, uSm5036e).
- Its two riders, +0x30A4 and +0x30A8, in the order they got on (0xB37CC0, thiscall (character, how), ret 8;
  kSigRideOn; 0xB39A6C sets the first).
- Its damage: a set model's hit record (+0x24BC valid, +0x24A4 the damage, a word) is handed to slot 44 by uSetModel's
  0xAD2E50; the helicopter's own damage step (its slot 76, 0xB7CA50) does so unless its first rider is a player the AI
  drives (0xA501A0) - in stage 751 from scene 0x3C with its own amounts (x2.5, or /10 by the hit's attributes).
  uSmRide's subtract-health (slot 44, 0xBB3DF0; thiscall (amount, a word), ret 4; kSigRideSubLife) scales it by its
  first rider's level when that is a player (sPlayer +0x4120 + player * 4, / 1000: 0-2 half, 3-4 three quarters) and
  takes it off +0xF10 (kept to 0..+0xF12) - with a hit record, only when the record's +8 has 0xFF in its top byte, in a
  campaign (sGameMan's 0x54E2D0) [I: what that byte is]. The players' bike (0xB66CF0) and jeep (0xB89B80) have their
  own rules for a computer rider first (less, or none outside stage 550), then call it. The helicopter's frame
  (0xB7DA10): at no health its action 7 in stage 706, 10 elsewhere (none in stage 751). Its setup: 1500 health, 3000
  in stage 751.
- Its missiles - uSm5060's mMissileNum (+0x41C0) and mMissileWait (+0x41C8), two each, a seat each; its constructor
  (0xB85740) gives each copy the global 0x1772008 (6; kSigMissileLoad). The missile action (cSmPlHarrierAct_Missile)
  starts (slot 6, 0xB81440; kSigMissileLaunch) on the aircraft its base start (0xB7FCC0) finds from the action's owner
  (+8) by class, at +0x18; the seat is the action's +0x10 (1 or not): the seat's copy at 0 - a dry launch (+0x4144 +
  seat * 4 = 2) - else 1, one off both copies and the seat's wait a second (0x1772004). The aircraft's frame for a
  seat (0xB72760) counts the wait down and plays the launch (0xCCC9D0, kind 0x14); "can launch" (0xB76EC0) asks the
  wait only. Nothing refills the count; uSm5020's constructor (0xB81AF0) and uSmPlHarrier's (0xB7D0F0) never write it.
- **God mode / Partner takes no damage on a ride**: slot 44 hooked in the 27 ride vtables (uSmRide and every class
  deriving from it, whatever each holds there): a ride with a spared rider in either seat (a uPlayer God mode or the
  partner's switch spares) takes nothing; the enemies' rides, and a ride without one, as the game has them. As either
  switch goes on (the tick, in a game), each ride in play with a spared rider and health above 0 is filled up once (a
  walk of sUnit's units). Log: `cheats: God mode: the rides' subtract-health (slot 44; uSmRide's exe+0x7B3DF0, its
  riders +0x30A4 and +0x30A8) hooked in 27 vtables - a ride a spared character rides takes nothing`, and `cheats: God
  mode: uSm5060 <addr> ridden by a spared character - its health n filled up to m`.
- **Infinite ammo on the helicopter**: the launch's two takes (`add [ecx+0x41C0], edx` at 0xB81475, `add
  [ecx+0x41C4], edx` at 0xB8147E, 6 bytes each) are NOPs while the switch is on (verified byte patches); as it goes on,
  each helicopter in play - of the class whose setup gives the load, uSm5060 - holding less than its load gets it
  back. Chris's jet gets none (its setup gives it none). Log: `cheats: Infinite ammo: uSm5060's missiles (+0x41C0, a
  copy a seat; its setup gives 6) - the missile action's two takes (exe+0x781475, exe+0x78147E) NOPs while on`, and
  `cheats: Infinite ammo: uSm5060 <addr> - its missiles (a, b) back to 6`.
- [I: in play - the helicopter's health held through stage 706's fight; its missiles at 6 on the HUD; whether Chris's
  jet fires missiles through this count at all (its setup writes none).]

## The profile and the skill points

- sInfoMan (0x4ECA0 bytes): +0x20 the profile in use (0 or 1), mSystem[] cSystemInfo (0xFDD8 bytes each) from +0x38,
  mGame[] cGameInfo (0x3CE4 each) from +0x3F798, mType +0x4EC88.
- cSystemInfo: mTempData.mOption (the options: +0xA8 mInfiniteAmmo among them), mTempData.mHistory.mUnlockFlag +0xB8,
  mSkillPoint +0x6F4, mSkillSetNo[] +0x760, mSkillSet[][] +0x768. So the points are sInfoMan +0x72C + profile * 0xFDD8.
- cGameInfo: mTempData.mBase mNewGame +0x1AF4, mEpisodeNo +0x1AF8, mStartChara +0x1AFC, mDifficulty +0x1B00,
  mChapterNo +0x1B04, mChapterStart +0x1B08, mStageNo +0x1B0C (the game tests it everywhere: 0x226, 0x2BE ...),
  mStartPosIdx +0x1B10, mSceNo +0x1B14.
- sGameMan +0xAC is the game's mode (0, 0x1E, 0x28, 0x32, 0x3C, 0x46, 0x50 are tested by the damage code); not yet
  named.
