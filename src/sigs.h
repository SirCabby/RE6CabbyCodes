#pragma once

// Every byte signature the mod finds the game's code by: SIG(name, "pattern", matches).
// `matches` is how many times the pattern occurs in Steam build 10112070's code - each is
// unique. tools/sig.py --check runs them all over the exe laid out as in memory
// (tools/map_image.py: ghidra_proj/BH6.mapped.exe), and fails a name defined twice (the
// mod finds a name's first entry only). Each operand the mod reads out of a match is named
// where it is used (game.cpp, cheats.cpp), and cross-checked there.
#define RE6CC_SIGS(SIG)                                                                                              \
  /* sBioMain's frame (its vtable slot 6) switching the pause: the requested state +2, the paused byte +10 (and  */ \
  /* written again +31 as the game pauses), sUnit's own pointer +25.                                              */ \
  SIG(kSigMainPause,                                                                                                \
      "8A 85 ?? ?? ?? ?? 33 DB 38 85 ?? ?? ?? ?? 74 ?? 3A C3 74 ?? 0F 57 C0 8B 85 ?? ?? ?? ?? C6 85 ?? ?? ?? ?? 01 " \
      "F3 0F 11 85 ?? ?? ?? ?? C6 80 ?? ?? ?? ?? 01",                                                                \
      1)                                                                                                             \
  /* uCharacter's subtract-health (its vtable slot 44; thiscall (amount), ret 4) - every character's, and the    */ \
  /* one an enemy's hit comes to: health +8 +19 +33 +42 (16-bit), its most +26.                                  */ \
  SIG(kSigSubLife,                                                                                                  \
      "66 8B 44 24 04 66 29 81 ?? ?? ?? ?? 79 ?? 33 D2 66 89 91 ?? ?? ?? ?? 0F B7 81 ?? ?? ?? ?? 66 39 81 ?? ?? ?? " \
      "?? 7E ?? 66 89 81 ?? ?? ?? ?? C2 04 00",                                                                      \
      1)                                                                                                             \
  /* sPlayer's hand-over of a character (to this machine, the AI or the network): its characters' array +19     */ \
  /* (8 of them, +12), and the character's own word for who drives it +38, tested against the AI's 1 +56 (the AI */ \
  /* taking one over gives it all its health).                                                                   */ \
  SIG(kSigPlayerControl,                                                                                            \
      "57 8B 7C 24 08 83 FF FF 7E ?? 83 FF 08 7D ?? 56 8B 74 B9 ?? 85 F6 74 ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? " \
      "89 86 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 83 BE ?? ?? ?? ?? 01",                                  \
      1)                                                                                                             \
  /* A player's hit (uPlPlayer's vtable slot 117; thiscall (hit), ret 4), where it reads the hit: its            */ \
  /* attributes +2, who dealt it +15, its damage +48 (a word).                                                    */ \
  SIG(kSigPlayerHit,                                                                                                \
      "8B 4B 10 8B D1 C1 EA 1A F6 C2 01 74 ?? 8B 43 28 85 C0 74 ?? 3B C6 75 ?? C1 E9 05 F6 C1 01 74 ?? 51 8B CE " \
      "F3 0F 11 04 24 E8 ?? ?? ?? ?? 0F B7 5B 04",                                                                   \
      1)                                                                                                             \
  /* A player's take-damage (uPlayer's vtable slot 121; thiscall, 6 arguments, ret 0x18): the AI's partner in    */ \
  /* the story is let off at +105 (who drives it, read at +107, must be this machine).                           */ \
  SIG(kSigPlayerApply,                                                                                              \
      "A1 ?? ?? ?? ?? 83 EC 24 83 B8 ?? ?? ?? ?? 46 56 8B F1 75 ?? 8B 16 8B 42 10 FF D0 50 8D 4C 24 0C E8 ?? ?? ?? " \
      "?? 8B 4C 24 24 81 E1 00 FF FF 00 81 F9 00 07 00 00 75 ?? 8B 16 8B 42 10 8B CE FF D0 50 8D 4C 24 0C E8 ?? ?? " \
      "?? ?? 80 7C 24 24 02 75 ?? 8B 0D ?? ?? ?? ?? 83 B9 ?? ?? ?? ?? 03 75 ?? 32 C0 5E 83 C4 24 C2 18 00 8B 86 ?? " \
      "?? ?? ?? 8B 15 ?? ?? ?? ?? 85 C0 0F 94 C1",                                                                   \
      1)                                                                                                             \
  /* An enemy's take-damage (uEnemy's vtable slot 128; thiscall (hit, amount), ret 8): the amount clamped to a   */ \
  /* word and handed to subtract-health through the vtable (its place +70).                                      */ \
  SIG(kSigEnemyApply,                                                                                               \
      "83 EC 20 56 57 8B 7C 24 2C 8B F1 85 FF 0F 84 ?? ?? ?? ?? 8B 06 8B 90 ?? ?? ?? ?? 57 FF D2 8B 44 24 30 3D FF " \
      "7F 00 00 7E ?? B8 FF 7F 00 00 EB ?? 3D 01 80 FF FF 7D ?? B8 01 80 FF FF EB ?? 0F B7 C0 8B 16 50 8B 82 ?? ?? " \
      "?? ?? 8B CE FF D0",                                                                                           \
      1)                                                                                                             \
  /* The same take-damage, the health gone: the enemy keeps a point when its slot 58 (its place +4) says no, or a */ \
  /* flag of its own is set - +18 (a byte) or mIsNoDead +27 (a byte; the reflected property) - or the hit says   */ \
  /* so; otherwise it is marked dead (+48).                                                                       */ \
  SIG(kSigEnemyKeepAlive,                                                                                           \
      "8B 16 8B 82 E8 00 00 00 8B CE FF D0 84 C0 74 ?? 80 BE ?? ?? ?? ?? 00 75 ?? 80 BE ?? ?? ?? ?? 00 75 ?? 57 8B " \
      "CE E8 ?? ?? ?? ?? 84 C0 75 ?? C6 86 ?? ?? ?? ?? 01",                                                          \
      1)                                                                                                             \
  /* uEnemy's damage worked out (its vtable slot 126; thiscall (hit), ret 4 - the hit the game's damage handler's */ \
  /* own copy): with no parameter set of its own (+6) the hit's damage (+17), as it is.                          */ \
  SIG(kSigEnemyWork,                                                                                                \
      "53 56 8B F1 8B 8E ?? ?? ?? ?? 57 8B 7C 24 10 8B 5F ?? 85 C9 75 ?? 5F 5E 8B C3 5B C2 04 00",                   \
      1)                                                                                                             \
  /* An enemy's part damage (uEnemy's; thiscall (part, amount), ret 8): the parts a hit's damage is worked into  */ \
  /* besides the health (Deborah's cores, a J'avo's limbs ...) - 32 (the last +11), each its health and most     */ \
  /* from +17 (8 bytes a part), a bit each in the broken mask +28 (a part at 0 is broken, and stays so).         */ \
  SIG(kSigEnemyPart,                                                                                                \
      "56 8B F1 8B 4C 24 08 32 C0 83 F9 1F 77 ?? 8B 94 CE ?? ?? ?? ?? 03 54 24 0C 53 8B 9E ?? ?? ?? ?? 57 BF 01 00 " \
      "00 00 D3 E7 85 DF",                                                                                           \
      1)                                                                                                             \
  /* Deborah's (uEm4500's) damage worked out (slot 126), its end: the health it hands on is none while bit 1 of   */ \
  /* her flags word (+2; `shr 1`, then `test bl` - bl is 1 from her start) is set - the mine cart's fight, where  */ \
  /* only her three cores take a hit.                                                                             */ \
  SIG(kSigEnemyNoLife,                                                                                              \
      "8B 96 ?? ?? ?? ?? F3 0F 2C 05 ?? ?? ?? ?? 5F D1 EA 5E 84 D3 5B 75 ?? F3 0F 2C C0 C2 04 00",                   \
      1)                                                                                                             \
  /* Brzak's (uEm4200's) damage worked out (slot 126), whole: uEnemy's own run on the hit (+5: its stagger), then  */ \
  /* 1 handed on (+11) whatever the hit - its life counts hits, and the stage's script ends it.                   */ \
  SIG(kSigEnemyHitCount,                                                                                            \
      "8B 44 24 04 50 E8 ?? ?? ?? ?? B8 01 00 00 00 C2 04 00",                                                       \
      1)                                                                                                             \
  /* Rasklapanje's (uEm4700Base's) subtract-health (slot 44), whole: the characters' own run (+8), then health    */ \
  /* (+16, +40; its most +26) put back to 1 when it fell below - damage never ends it.                           */ \
  SIG(kSigEnemyFloor,                                                                                               \
      "8B 44 24 04 56 50 8B F1 E8 ?? ?? ?? ?? 66 83 BE ?? ?? ?? ?? 01 7D ?? 0F B7 86 ?? ?? ?? ?? B9 01 00 00 00 8B " \
      "D1 66 89 8E ?? ?? ?? ?? 66 3B D0 7E",                                                                         \
      1)                                                                                                             \
  /* uEnemy's setup making its brain: mpIntel (+11, +29; the reflected property) filled when empty.              */ \
  SIG(kSigEnemyIntel,                                                                                               \
      "56 57 8B F1 E8 ?? ?? ?? ?? 83 BE ?? ?? ?? ?? 00 75 ?? 56 E8 ?? ?? ?? ?? 83 C4 04 89 86 ?? ?? ?? ?? 8B 06 8B " \
      "90 CC 02 00 00 8B CE FF D2",                                                                                  \
      1)                                                                                                             \
  /* The pending hits' reaction (0x9425F0's): none while the keep-alive flag (+15) is set; then, when slot 58     */ \
  /* (+26) says it may die and the unit's state (+40) is 2 or its health (+47) is gone, the brain's death       */ \
  /* reaction (its vtable offset +62), else its damage reaction (+76).                                           */ \
  SIG(kSigEnemyReaction,                                                                                            \
      "80 BE ?? ?? ?? ?? 00 0F 84 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00 75 ?? 8B 06 8B 90 E8 00 00 00 8B CE FF D2 84 C0 " \
      "74 ?? 80 7E ?? 02 74 ?? 66 83 BE ?? ?? ?? ?? 00 7F ?? 8B 4C 24 ?? 8B 01 8B 90 ?? ?? ?? ?? EB ?? 8B 4C 24 ?? " \
      "8B 01 8B 90 ?? ?? ?? ??",                                                                                     \
      1)                                                                                                             \
  /* Haos' (uEm8000's) damage worked out (slot 126), +9 in: its mode (+18; its placement's mSetType - 2 on the   */ \
  /* pipes, stage 903, whose end is the stage's script) switched on.                                              */ \
  SIG(kSigEnemyHaosMode,                                                                                            \
      "8B 06 8B 90 F4 00 00 00 57 8B 7D 04 6A 00 FF D2 8B 8E ?? ?? ?? ?? BB 01 00 00 00 2B CB 0F 84 ?? ?? ?? ?? 2B " \
      "CB 0F 84 ?? ?? ?? ?? 2B CB 74 ?? 0F 57 C0 F3 0F 2A C7 F3 0F 58 86",                                           \
      1)                                                                                                             \
  /* The attack choppers' (uEm9900's) damage worked out (slot 126), its end: while the profile's (+8; cGameInfo  */ \
  /* +11 bytes each) stage (+18) is +22 and its scene (+31) below +35, the health handed on is held to health    */ \
  /* (+44) - 1: the chopper the stage's script shoots down keeps a point until then.                             */ \
  SIG(kSigEnemyStageHold,                                                                                           \
      "8B 0D ?? ?? ?? ?? 8B 51 ?? 69 D2 ?? ?? ?? ?? 81 BC 11 ?? ?? ?? ?? ?? ?? ?? ?? 75 ?? 81 BC 11 ?? ?? ?? ?? ?? " \
      "?? ?? ?? 73 ?? 66 8B 8D ?? ?? ?? ?? 66 49 0F B7 C9 0F BF C9 3B C1 7E 02 8B C1",                               \
      1)                                                                                                             \
  /* The room bombs' (uSmRoomBomb's: the floor robots of Leon's chapter 4, s0512) hit check while they roll       */ \
  /* (their Move action's): a hit already pending (bit +11 of the flags +5) counts; else a hit with damage (the   */ \
  /* bomb's hit record +22) from a character of the players' side (the class test called +57) not played over the */ \
  /* network (+71: who drives it, 2) - and when the game's instant test (called +85: the hit's attributes) says   */ \
  /* it blows up at once, +96 empties its points (a word at the characters' health).                              */ \
  SIG(kSigBombHit,                                                                                                   \
      "56 8B F1 8B 86 ?? ?? ?? ?? C1 E8 02 A8 01 74 04 B0 01 5E C3 83 BE ?? ?? ?? ?? 00 75 04 32 C0 5E C3 57 8B "    \
      "BE ?? ?? ?? ?? 85 FF 74 ?? 8B 17 8B 42 10 8B CF FF D0 8B 40 1C 50 E8 ?? ?? ?? ?? 83 C4 04 84 C0 74 ?? 83 "    \
      "BF ?? ?? ?? ?? 02 75 ?? 5F 32 C0 5E C3 8B CE E8 ?? ?? ?? ?? 84 C0 74 ?? 8B CE E8 ?? ?? ?? ??",                \
      1)                                                                                                             \
  /* ... and the same in a room bomb's warning (its Warning action's frame, the second it beeps before it goes    */ \
  /* off by the player): the hit record +20, its dealer +29, the class test +50, the network +64, the instant     */ \
  /* test called +73 - its points emptied (+84) or one taken (+91).                                               */ \
  SIG(kSigBombWarnHit,                                                                                               \
      "53 56 57 8B F1 8B 4E 18 6A 02 E8 ?? ?? ?? ?? 8B 5E 18 83 BB ?? ?? ?? ?? 00 74 ?? 8B BB ?? ?? ?? ?? 85 FF "    \
      "74 ?? 8B 07 8B 50 10 8B CF FF D2 8B 40 1C 50 E8 ?? ?? ?? ?? 83 C4 04 84 C0 74 ?? 83 BF ?? ?? ?? ?? 02 74 "    \
      "?? 8B CB E8 ?? ?? ?? ?? 8B CB 84 C0 74 ?? E8 ?? ?? ?? ?? EB ?? E8 ?? ?? ?? ??",                               \
      1)                                                                                                             \
  /* A player's shot using up rounds (thiscall (slot, count), ret 8): the inventory +15 (items of +32 bytes,     */ \
  /* +35 of them, each found by its slot number, a byte +24 into it, and taken only while in use, a byte +46),  */ \
  /* and the game's own "infinite ammo" - a bit (+62) of a word of the player's (+56) - letting the shot through */ \
  /* without using any.                                                                                          */ \
  SIG(kSigAmmoUse,                                                                                                  \
      "51 56 57 8B F9 8B 4C 24 10 89 7C 24 08 8D B7 ?? ?? ?? ?? 33 C0 0F B6 56 ?? 3B CA 74 ?? 40 83 C6 ?? 83 F8 ?? " \
      "72 ?? 5F 5E 59 C2 08 00 80 7E ?? 00 0F 84 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? C1 E8 ?? A8 01 0F 85",                \
      1)                                                                                                             \
  /* A player's reload, its start: the slot in hand +6 (a dword of the player's), looked for in the inventory     */ \
  /* (+17, items of +34 bytes by their slot byte +26, +37 of them); a character not driven here (+50) is filled   */ \
  /* from nothing.                                                                                                */ \
  SIG(kSigInHand,                                                                                                   \
      "51 55 8B E9 8B 8D ?? ?? ?? ?? 57 89 6C 24 08 8D BD ?? ?? ?? ?? 33 C0 0F B6 57 ?? 3B CA 74 ?? 40 83 C7 ?? 83 " \
      "F8 ?? 72 ?? 5F 32 C0 5D 59 C2 04 00 83 BD ?? ?? ?? ?? 00",                                                    \
      1)                                                                                                             \
  /* cItem's set-count (thiscall (count, mode), ret 8): the mode in use when none is given +11 (a byte), the     */ \
  /* item's id +22 (a word; ids from 0x103 to 0x117 go through a table of which keep a mode), the counts +59     */ \
  /* (three words, one a mode).                                                                                 */ \
  SIG(kSigItemSetNum,                                                                                               \
      "8B 44 24 08 85 C0 79 04 0F B6 41 ?? 83 F8 03 72 02 33 C0 0F B7 51 ?? 81 C2 FD FE FF FF 83 FA 14 77 ?? 0F B6 " \
      "92 ?? ?? ?? ?? FF 24 95 ?? ?? ?? ?? 33 C0 66 8B 54 24 04 66 89 54 41 ?? C2 08 00",                            \
      1)                                                                                                             \
  /* cItem's get-count and get-most (thiscall (mode), ret 4), the same shape: each reads its words at +54 - the   */ \
  /* counts' (kSigItemSetNum's) and the mosts' (three words on).                                                */ \
  SIG(kSigItemGet,                                                                                                  \
      "8B 44 24 04 85 C0 79 04 0F B6 41 ?? 83 F8 03 72 02 33 C0 0F B7 51 ?? 81 C2 FD FE FF FF 83 FA 14 77 ?? 0F B6 " \
      "92 ?? ?? ?? ?? FF 24 95 ?? ?? ?? ?? 33 C0 0F BF 44 41 ?? C2 04 00",                                            \
      2)                                                                                                             \
  /* A player's reload (thiscall (count), ret 4) filling the gun from nothing under the same bit (+8, the word   */ \
  /* +2) - so a gun that uses no rounds never needs one either.                                                  */ \
  SIG(kSigAmmoReload,                                                                                               \
      "8B 95 ?? ?? ?? ?? C1 EA ?? 8B CF F6 C2 01 74 ?? 6A FF 6A FF E8 ?? ?? ?? ?? 50 8B CF E8 ?? ?? ?? ?? 5F B0 01 " \
      "5D 59 C2 04 00",                                                                                              \
      1)                                                                                                             \
  /* A hit on a player paying for itself from the health tablets the player had set aside to take (in the      */ \
  /* player's hit, slot 117): the tablets' item id +1, the slot of the inventory holding them found (called +7),*/ \
  /* then taken through the shot's own use of an item (called +21, kSigAmmoUse) - the game's infinite-ammo bit  */ \
  /* spares them too, which Infinite ammo must not.                                                             */ \
  SIG(kSigTabletUse, "68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 83 F8 FF 74 ?? 57 50 8B CE E8", 1)                        \
  /* The skill screen's purchase of a skill: the price taken off its copy of the points (+21, `sub [esi+..],ecx`, */ \
  /* 6 bytes), which then goes to sInfoMan's set-skill-points (called +59; kSkillPointsSet below).              */ \
  SIG(kSigSkillBuy,                                                                                                 \
      "8B 86 ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 03 C0 8B 8C C6 ?? ?? ?? ?? 29 8E ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D BC C6 " \
      "?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 E8",                                      \
      1)                                                                                                             \
  /* ... and of a skill's next level: the price off the same copy (+9, `sub [esi+..],eax`, 6 bytes), then       */ \
  /* set-skill-points (called +40).                                                                              */ \
  SIG(kSigSkillLevel,                                                                                               \
      "8B 8E ?? ?? ?? ?? 8B 47 08 29 86 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 8B 0D ?? " \
      "?? ?? ?? 52 E8",                                                                                              \
      1)                                                                                                             \
  /* The chapter's numbers (cGameInfo's, a block of each character's): a counter added to (thiscall (character,  */ \
  /* counter, n), ret 0xC): 8 characters +6, 63 counters +15, a character's words +20, character 0's counter 0   */ \
  /* +26, the most a counter holds +38 (and +45).                                                                */ \
  SIG(kSigChapterCounterAdd,                                                                                        \
      "8B 44 24 04 83 F8 ?? 73 ?? 8B 54 24 08 83 FA ?? 73 ?? 6B C0 ?? 03 C2 8D 8C 41 ?? ?? ?? ?? 0F B7 01 03 44 "   \
      "24 0C 3D ?? ?? ?? ?? 76 ?? B8 ?? ?? ?? ?? 66 89 01 C2 0C 00",                                                \
      1)                                                                                                            \
  /* sInfoMan's frame adding the frame's time to every character's chapter clock: the profile in use +2,         */ \
  /* cGameInfo's size +11, the profiles' cGameInfo +25 (from sInfoMan's start), the add-clock called +29         */ \
  /* (RE6CC_CLOCK_ADD), 8 characters +37.                                                                        */ \
  SIG(kSigChapterClockLoop,                                                                                         \
      "8B 56 ?? F3 0F 10 44 24 ?? 69 D2 ?? ?? ?? ?? 51 F3 0F 11 04 24 57 8D 8C 32 ?? ?? ?? ?? E8 ?? ?? ?? ?? 47 "   \
      "83 FF ?? 72",                                                                                                \
      1)                                                                                                            \
  /* cGameInfo's restore of the chapter's numbers from the copy a save takes: its size +7, the copy +13, the     */ \
  /* live ones +17.                                                                                              */ \
  SIG(kSigChapterRestore,                                                                                           \
      "56 8B F1 57 33 FF 68 ?? ?? ?? ?? 8D 46 ?? 50 8D 8E ?? ?? ?? ?? 51",                                          \
      1)                                                                                                            \
  /* The Game Over counting a death for each character in play: sGameMan +2, sInfoMan +20, the count +24 (a      */ \
  /* `push 1`), the counter +27, the locked add called +29.                                                      */ \
  SIG(kSigDeathCounted,                                                                                             \
      "8B 0D ?? ?? ?? ?? 85 A9 ?? ?? ?? ?? 74 ?? 85 EB 74 ?? 8B 0D ?? ?? ?? ?? 6A 01 6A ?? 56 E8",                  \
      1)                                                                                                            \
  /* The results grading a character, the counters it reads: shots +1, hits +61, kills +117, deaths +141 (the    */ \
  /* get-counter called +14).                                                                                    */ \
  SIG(kSigChapterGrade,                                                                                             \
      "6A ?? 8D 8C 30 ?? ?? ?? ?? 57 89 54 24 1C E8 ?? ?? ?? ?? 89 44 24 34 DB 44 24 34 85 C0 79 06 D8 05 ?? ?? "   \
      "?? ?? D9 54 24 34 D9 EE D9 C9 DF F1 DD D8 76 ?? 8B 46 ?? 69 C0 ?? ?? ?? ?? 6A ?? 8D 8C 30 ?? ?? ?? ?? 57 "   \
      "E8 ?? ?? ?? ?? 89 44 24 10 DB 44 24 10 85 C0 79 06 D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 74 24 34 D9 5C "   \
      "24 34 8B 4E ?? 69 C9 ?? ?? ?? ?? 6A ?? 8D 8C 31 ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 8B 56 ?? 69 D2 ?? ?? ?? ?? "   \
      "6A ?? 8D 8C 32 ?? ?? ?? ?? 57 8B D8 E8 ?? ?? ?? ?? 8B E8 8B 46 ?? 69 C0 ?? ?? ?? ?? 8D 8C 30 ?? ?? ?? ?? "   \
      "57 E8",                                                                                                      \
      1)                                                                                                            \
  /* sActionButton's managers asking whether a prompt was answered (each frame, each of a player's two units):   */ \
  /* the unit in use +6, the manager's player +12 and pad +15, the check called +20 (kSigQteCheck), the unit's   */ \
  /* request +31.                                                                                                */ \
  SIG(kSigQteCall,                                                                                                  \
      "8B 74 BC 14 80 7E ?? 00 74 ?? 8B 51 ?? 83 C1 ?? 52 51 8B CE E8 ?? ?? ?? ?? 84 C0 74 ?? 8B 76 ?? 85 F6 74 "   \
      "?? 8B 0D ?? ?? ?? ?? 8B 51 ?? 33 C0 85 D2 74 ?? 8B 49 ?? 39 31 74",                                          \
      1)                                                                                                            \
  /* ... the check (thiscall (pad, player) on the unit, ret 8): the unit's request +5, drawn +8, the request's   */ \
  /* answered-this-frame +27.                                                                                    */ \
  SIG(kSigQteCheck,                                                                                                 \
      "56 8B F1 8B 46 ?? C6 46 ?? 00 C6 86 ?? ?? ?? ?? 00 85 C0 0F 84 ?? ?? ?? ?? 80 78 ?? 01 0F 84 ?? ?? ?? ?? "   \
      "80 78 ?? 01 75 ?? E8 ?? ?? ?? ?? 8B 46 ?? 8B 4E ?? 57 50 E8",                                                \
      1)                                                                                                            \
  /* A unit taking a request: its request +13, the request's kind +35 copied to the unit's +42, the request's    */ \
  /* flags +101 (the bit of a dodge made as a press +102).                                                       */ \
  SIG(kSigQteUnitSet,                                                                                               \
      "53 56 8B F1 57 8B 7C 24 10 33 DB 89 7E ?? 66 C7 46 ?? 01 01 66 89 9E ?? ?? ?? ?? 88 5F ?? 8B 4E ?? 8B 41 "   \
      "?? 8B 56 ?? 52 89 46 ?? 89 5E ?? E8 ?? ?? ?? ?? 8B 04 85 ?? ?? ?? ?? 8B 4E ?? 0F 57 C0 51 8B 4E ?? 89 46 "   \
      "?? 89 5E ?? F3 0F 11 46 ?? F3 0F 11 46 ?? E8 ?? ?? ?? ?? 0F B6 90 ?? ?? ?? ?? 89 56 ?? F6 47 ?? 04 74",      \
      1)                                                                                                            \
  /* A press in time: the 100% +4 (a float's place), the request +13, the progress +37 (in %), the               */ \
  /* input-blocked test called +63, the request's failed players +104, the good window's start +116, the         */ \
  /* request's good byte +209.                                                                                   */ \
  SIG(kSigQteTimed,                                                                                                 \
      "F3 0F 10 0D ?? ?? ?? ?? 56 8B F1 8B 46 ?? 8B 0D ?? ?? ?? ?? 0F 28 C1 F3 0F 5E 40 ?? F3 0F 59 41 ?? F3 0F "   \
      "58 46 ?? 0F 2F C1 F3 0F 11 46 ?? 72 05 F3 0F 11 4E ?? 8B 56 ?? 8B 0D ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 84 C0 "   \
      "74 0D C6 86 ?? ?? ?? ?? 01 32 C0 5E C2 0C 00 8B 46 ?? 57 8B 7C 24 10 BA 01 00 00 00 8B CF D3 E2 85 50 ?? "   \
      "0F 85 ?? ?? ?? ?? D9 40 ?? D9 5E ?? D9 80 ?? ?? ?? ?? D9 5E ?? 83 78 ?? 13 75 ?? 0F 57 C0 F3 0F 11 46 ?? "   \
      "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 46 ?? 8B 44 24 0C 57 50 8B CE E8 ?? ?? ?? ?? 3C 01 0F 85 ?? ?? ?? ?? 8B "   \
      "4E ?? D9 46 ?? D9 59 ?? 8B 46 ?? 8B 48 ?? 83 F9 0C 75 ?? F3 0F 10 46 ?? 0F 2F 46 ?? 72 ?? 6A 2D C6 40 ?? "   \
      "01",                                                                                                         \
      1)                                                                                                            \
  /* The check's switch on the unit's kind (+2), 0 to 0x19 (+5).                                                 */ \
  SIG(kSigQteKinds,                                                                                                 \
      "8B 46 ?? 83 F8 19 0F 87 ?? ?? ?? ?? 0F B6 88 ?? ?? ?? ?? FF 24 8D ?? ?? ?? ?? B3 01 E9",                     \
      1)                                                                                                            \
  /* sPlayer's input-blocked test (thiscall (player)): two bytes, a bit for each player, at +38.                 */ \
  SIG(kSigInputBlocked,                                                                                             \
      "56 8B 74 24 08 57 8B F9 33 C0 8D 9B 00 00 00 00 83 FE 08 73 ?? 83 F8 02 7D ?? BA 01 00 00 00 8B CE D3 E2 "   \
      "84 94 38 ?? ?? ?? ?? 0F 95 C2 84 D2 75",                                                                     \
      1)                                                                                                            \
  /* The scenes that take their Amateur (automatic) path when the profile's mExOptAutoActBtn is on: sInfoMan     */ \
  /* +1, the profile in use +7, a profile's size +10, the option +17 (from sInfoMan's start), `cmp [..],0` +21.  */ \
  SIG(kSigAutoActBtn,                                                                                               \
      "A1 ?? ?? ?? ?? 8B ?? 20 69 ?? D8 FD 00 00 83 BC ?? BC 70 00 00 00",                                          \
      16)                                                                                                           \
  /* ... the same against a register that holds 0 there (its ModRM +15).                                         */ \
  SIG(kSigAutoActBtnReg,                                                                                            \
      "A1 ?? ?? ?? ?? 8B ?? 20 69 ?? D8 FD 00 00 39 ?? ?? BC 70 00 00",                                             \
      3)                                                                                                            \
  /* The aim's sway (thiscall (player, a, b) on the weapon, ret 0xC; two angles out): its early way out for no   */ \
  /* player (+17, a `jne` past it) writes 0 and 0.                                                               */ \
  SIG(kSigAimSway,                                                                                                  \
      "83 EC 34 56 8B 74 24 3C 57 8B F9 89 7C 24 14 85 F6 75 ?? 0F 57 C0 8B 44 24 44 8B 4C 24 48 5F F3 0F 11 00 "   \
      "F3 0F 11 01 5E 83 C4 34 C2 0C 00",                                                                           \
      1)                                                                                                            \
  /* A shot's kick added to the player's aim (uPlIntel's pad handling): the player's flags +2 (the word with     */ \
  /* the infinite-ammo bit), the kick +10, the shot's bit +16, the `je` past it for a frame with no shot +32.    */ \
  SIG(kSigAimKick,                                                                                                  \
      "8B 8E ?? ?? ?? ?? F3 0F 10 8E ?? ?? ?? ?? C1 E9 ?? F3 0F 11 44 24 ?? F3 0F 11 4C 24 ?? F6 C1 01 0F 84 ?? "   \
      "?? ?? ?? 8B 54 24 ?? 52 E8 ?? ?? ?? ?? 8B 1D ?? ?? ?? ?? 83 C4 04 8B CE 8B F8 E8 ?? ?? ?? ?? 0F B6 C0 50 "   \
      "8B CB E8 ?? ?? ?? ?? 8B D8 B9 ?? ?? ?? ?? D9 44 DF ??",                                                      \
      1)                                                                                                            \
  /* A camera mode's callback (cdecl (camera, out)): its view worked out (+15, fastcall (camera) -> al), copied  */ \
  /* out - the fov +26, the eye +39 +45 +51, the point it looks at +62 +68 +74 (a byte each). Three modes: the   */ \
  /* scope's is the one whose call is the scope camera (kSigScopeCamera, the one that calls the aim sway).       */ \
  SIG(kSigScopeView,                                                                                                \
      "56 8B 74 24 08 85 F6 75 04 32 C0 5E C3 8B CE E8 ?? ?? ?? ?? 84 C0 74 F1 D9 46 ?? 8B 44 24 10 0F 57 C0 "      \
      "D9 58 34 D9 46 ?? D9 58 10 D9 46 ?? D9 58 14 D9 46 ?? F3 0F 11 40 1C D9 58 18 D9 46 ?? D9 58 20 D9 46 ?? "   \
      "D9 58 24 D9 46 ??",                                                                                          \
      3)                                                                                                            \
  /* The scope camera (fastcall (camera) -> al: the view down the gun's scope joint, 100): its player +55 and   */ \
  /* again +113, the zoom it reads off the player +159 (the aim's last number, kSigAimState).                   */ \
  SIG(kSigScopeCamera,                                                                                              \
      "55 8B EC 83 E4 F0 81 EC ?? ?? ?? ?? 53 56 8B F1 8B 86 ?? ?? ?? ?? 57 85 C0 74 ?? 8B 40 10 83 E0 07 83 F8 "   \
      "02 74 ?? 83 F8 01 74 ?? C7 86 ?? ?? ?? ?? 00 00 00 00 8B 8E ?? ?? ?? ?? 85 C9 75 ?? 32 C0 5F 5E 5B 8B E5 "   \
      "5D C3 8B 01 8B 50 10 FF D2 50 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 84 24 ?? ?? ?? ?? 25 00 00 00 FF 3D "   \
      "00 00 00 01 75 ?? 8B 9E ?? ?? ?? ?? 85 DB 74 ?? 6A 00 8B CB E8 ?? ?? ?? ?? 8B C8 89 8C 24 ?? ?? ?? ?? 85 "   \
      "C9 74 ?? 8B 93 ?? ?? ?? ?? C1 EA 02 F6 C2 01 74 ?? D9 83 ?? ?? ?? ?? D9 9E",                                 \
      1)                                                                                                            \
  /* The player's laser and line (fastcall (player)): its call of the gun's line (+100, fastcall (player); the   */ \
  /* function that calls the aim sway as a scope's view is up).                                                  */ \
  SIG(kSigGunLineCall,                                                                                              \
      "55 8B EC 83 E4 F0 81 EC ?? ?? ?? ?? 53 56 8B F1 8B 8E ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 57 85 C9 0F 84 ?? ?? "   \
      "?? ?? 8B 11 50 8B 82 ?? ?? ?? ?? FF D0 8B F8 85 FF 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 89 44 24 ?? 85 C0 "   \
      "0F 84 ?? ?? ?? ?? 8B 8E ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 8B CE C7 86 ?? ?? ?? ?? 00 00 00 00 E8 ?? ?? ?? ?? "   \
      "8B CE E8 ?? ?? ?? ?? 3C 01",                                                                                 \
      1)                                                                                                            \
  /* The gun's line's end: where it comes from +4 (+18 +26 its y and z) and the point it goes to +66 (+42 +58    */ \
  /* its y and z; read back +116, copied for the next frame's blend +122).                                       */ \
  SIG(kSigGunLineEnd,                                                                                               \
      "F3 0F 11 A6 ?? ?? ?? ?? F3 0F 10 64 24 ?? F3 0F 11 A6 ?? ?? ?? ?? F3 0F 11 BE ?? ?? ?? ?? F3 0F 11 96 ?? "   \
      "?? ?? ?? F3 0F 11 8E ?? ?? ?? ?? F3 0F 11 96 ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? "   \
      "D9 86 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 86 ?? ?? ?? ?? F3 0F 11 96 ?? "   \
      "?? ?? ?? D9 9E ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 86 "   \
      "?? ?? ?? ?? F3 0F 11 96 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 5F 5E 5B 8B E5 5D C3",                                 \
      1)                                                                                                            \
  /* A weapon's shot (on the weapon; its owner in esi): the gun's line it fires down - from +40 (+48 +56), to   */ \
  /* +64 (+72).                                                                                                  */ \
  SIG(kSigShotLine,                                                                                                 \
      "8B 17 8B 82 ?? ?? ?? ?? 8B CF FF D0 50 57 E8 ?? ?? ?? ?? 83 C4 04 50 E8 ?? ?? ?? ?? F3 0F 10 86 ?? ?? ?? "   \
      "?? F3 0F 10 AE ?? ?? ?? ?? F3 0F 10 B6 ?? ?? ?? ?? F3 0F 10 BE ?? ?? ?? ?? F3 0F 10 9E ?? ?? ?? ?? F3 0F "   \
      "10 8E ?? ?? ?? ?? F3 0F 10 96 ?? ?? ?? ?? 83 C4 08",                                                         \
      1)                                                                                                            \
  /* The aim's numbers copied from the player's brain (+2) to the player each frame: the turn +28, the pitch +54 */ \
  /* (-1 up .. 1 down; from its +48), its copy +80, the waist's twist +106 (degrees; from its +100), the zoom    */ \
  /* +122.                                                                                                       */ \
  SIG(kSigAimState,                                                                                                 \
      "8B 81 ?? ?? ?? ?? 85 C0 75 04 33 D2 EB 06 8D 90 ?? ?? ?? ?? D9 82 ?? ?? ?? ?? D9 99 ?? ?? ?? ?? 85 C0 "      \
      "75 04 33 D2 EB 06 8D 90 ?? ?? ?? ?? D9 82 ?? ?? ?? ?? D9 99 ?? ?? ?? ?? 85 C0 75 04 33 D2 EB 06 8D 90 ?? "   \
      "?? ?? ?? D9 82 ?? ?? ?? ?? D9 99 ?? ?? ?? ?? 85 C0 75 04 33 D2 EB 06 8D 90 ?? ?? ?? ?? D9 82 ?? ?? ?? ?? "   \
      "D9 99 ?? ?? ?? ?? 85 C0 75 0D D9 80 ?? ?? ?? ?? D9 99 ?? ?? ?? ?? C3",                                       \
      1)                                                                                                            \
  /* uPlayer's health add (its own; thiscall (amount, a word), ret 4): health +11 +22 +36 +45, its most +29; a  */ \
  /* "down" bit cleared above 1 (+52).                                                                           */ \
  SIG(kSigPlayerHealthAdd,                                                                                          \
      "66 8B 44 24 04 56 8B F1 66 01 86 ?? ?? ?? ?? 79 ?? 33 C9 66 89 8E ?? ?? ?? ?? 0F B7 86 ?? ?? ?? ?? 66 39 86 " \
      "?? ?? ?? ?? 7E ?? 66 89 86 ?? ?? ?? ?? 66 83 BE ?? ?? ?? ?? 01",                                               \
      1)                                                                                                            \
  /* uPlayer's reset (a Continue, a stage's start; thiscall ()): its brain +5, the flags' reset (+45: the whole  */ \
  /* flags words written), the slot in hand +69.                                                                 */ \
  SIG(kSigPlayerReset,                                                                                              \
      "56 8B F1 8B 8E ?? ?? ?? ?? 33 C0 89 46 38 3B C8 74 ?? 8B 41 10 83 E0 07 83 F8 02 74 ?? 83 F8 01 75 ?? 8B 01 " \
      "8B 50 7C FF D2 6A 01 8B CE E8 ?? ?? ?? ?? 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 89 86 ?? ?? ?? " \
      "?? 8B CE 5E E9",                                                                                             \
      1)                                                                                                            \
  /* uPlayer's add to the combat gauge - the air under water (thiscall (amount, a float), ret 4): the pause      */ \
  /* after a spend +10, the gauge +27 +41.                                                                       */ \
  SIG(kSigAirAdd,                                                                                                   \
      "0F 57 DB 56 8B F1 F3 0F 10 86 ?? ?? ?? ?? 0F 2F C3 0F 87 ?? ?? ?? ?? F3 0F 10 86 ?? ?? ?? ?? F3 0F 58 44 24 " \
      "08 F3 0F 10 8E ?? ?? ?? ??",                                                                                 \
      1)                                                                                                            \
  /* A ride's subtract-health (uSmRide's vtable slot 44; thiscall (amount, a word), ret 4 at +277): scaled by its */ \
  /* first rider's (+13, read again +72) level when that is a player, then taken off its health (+234; put at 0   */ \
  /* +245; kept to its most +252: +259, +268) - the characters' health words.                                     */ \
  SIG(kSigRideSubLife,                                                                                              \
      "83 EC 24 0F BF 44 24 28 56 8B F1 8B 8E ?? ?? ?? ?? 0F 57 C0 F3 0F 2A C0 57 F3 0F 11 44 24 30 85 C9 74 ?? 8B " \
      "11 8B 42 10 FF D0 50 8D 4C 24 10 E8 ?? ?? ?? ?? 8B 4C 24 28 81 E1 00 00 00 FF 81 F9 00 00 00 01 75 ?? 8B 8E " \
      "?? ?? ?? ??",                                                                                                \
      1)                                                                                                            \
  /* A character getting on a ride (thiscall (character, how), ret 8): the first seat if free (+27, set +36),     */ \
  /* else the second (+44, set +57).                                                                              */ \
  SIG(kSigRideOn,                                                                                                   \
      "55 8B EC 83 E4 F0 83 EC 14 53 56 8B 75 08 57 8B D9 85 F6 0F 84 ?? ?? ?? ?? 83 BB ?? ?? ?? ?? 00 75 08 89 B3 " \
      "?? ?? ?? ?? EB 13 83 BB ?? ?? ?? ?? 00 0F 85 ?? ?? ?? ?? 89 B3 ?? ?? ?? ??",                                  \
      1)                                                                                                            \
  /* The players' aircraft's missile action's start (cSmPlHarrierAct_Missile's vtable slot 6; thiscall ()): the */ \
  /* seat (the action's +0x10 is 1 or not) its copy of the aircraft's (the action's +0x18) count (+29) tested - */ \
  /* none left, a dry launch - else one taken off both copies (`add [ecx+n], edx` with edx -1: +53, its field  */ \
  /* +55; +62, +64), and the seat's wait set (+84).                                                              */ \
  SIG(kSigMissileLaunch,                                                                                            \
      "56 8B F1 E8 ?? ?? ?? ?? 8B 46 10 48 74 04 33 C0 EB 05 B8 01 00 00 00 8B 4E 18 83 BC 81 ?? ?? ?? ?? 00 76 ?? " \
      "C7 84 81 ?? ?? ?? ?? 01 00 00 00 8B 4E 18 83 CA FF 01 91 ?? ?? ?? ?? 8B 4E 18 01 91 ?? ?? ?? ?? 8B 56 18 F3 " \
      "0F 10 05 ?? ?? ?? ?? F3 0F 11 84 82 ?? ?? ?? ??",                                                            \
      1)                                                                                                            \
  /* ... the helicopter's setup (its constructor; its vtable stored before): each seat's count (+8, +29) given  */ \
  /* the load from a global (+2, +21: 6), each seat's wait zeroed (+16, +37).                                    */ \
  SIG(kSigMissileLoad,                                                                                              \
      "8B 15 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B CE 89 86 ?? ?? ?? ?? F3 0F 11 " \
      "86 ?? ?? ?? ?? E8",                                                                                          \
      1)                                                                                                            \
  /* uPlPlayer's spend of combat gauge blocks (its vtable slot 123, the one after the add; thiscall (blocks),    */ \
  /* ret 4) - what a move with a cost pays: none spent under the skill pushed +31 (Training), nor with no blocks */ \
  /* held (the slot +50, the one after it); else 100 a block off the gauge (+95, stored +111; put at 0 +123     */ \
  /* when it went below), kept to its most (+131).                                                               */ \
  SIG(kSigGaugeSpend,                                                                                               \
      "56 57 8B 7C 24 0C 8B F1 85 FF 0F 84 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 0F B6 C0 6A ?? 50 E8 ?? ?? " \
      "?? ?? 84 C0 0F 85 ?? ?? ?? ?? 8B 06 8B 90 ?? ?? ?? ?? 8B CE FF D2 85 C0 0F 8E ?? ?? ?? ?? 89 7C 24 0C DB 44 " \
      "24 0C 85 FF 79 06 D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 0F 57 C9 D8 AE ?? ?? ?? ?? D9 54 24 0C F3 0F 2C 44 24 " \
      "0C D9 9E ?? ?? ?? ?? 85 C0 79 08 F3 0F 11 8E ?? ?? ?? ?? F3 0F 10 86 ?? ?? ?? ??",                           \
      1)                                                                                                            \
  /* A shot counted (sInfoMan's counter add, +30: thiscall (character, counter, amount)): the counter +26 (the  */ \
  /* shots'), after the shooter's record (the call +0).                                                          */ \
  SIG(kSigShotCount,                                                                                                \
      "E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 80 78 03 00 74 ?? 8B 54 24 ?? 6A 01 6A ?? 52 8B CE E8 ?? ?? ?? ??",    \
      1)                                                                                                            \
  /* A shot's hit counted (+35), once a shot (its flag +21, set +42): the counter +31 (the hits').               */ \
  SIG(kSigHitCount,                                                                                                 \
      "E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 80 78 03 00 74 ?? 80 BB ?? ?? ?? ?? 00 75 ?? 6A 01 6A ?? 57 8B CE E8 " \
      "?? ?? ?? ?? C6 83 ?? ?? ?? ?? 01",                                                                           \
      1)                                                                                                            \
  /* Another hit counted (+15: a blow's), through sInfoMan's global (+6): the counter +13.                       */ \
  SIG(kSigHitCountBlow,                                                                                             \
      "0F B6 46 05 8B 0D ?? ?? ?? ?? 6A 01 6A ?? 50 E8 ?? ?? ?? ?? 5F 5E C2 08 00",                                 \
      1)                                                                                                            \
  /* sInfoMan's counter adds (its lock, then the profile in use's (+43, a profile's size +46) cGameInfo (+55)   */ \
  /* add, +59): the chapter's counters' and another array's.                                                     */ \
  SIG(kSigInfoCount,                                                                                                \
      "56 8B F1 80 7E 1C 00 75 ?? 80 3D ?? ?? ?? ?? 00 74 ?? 8D 46 04 50 FF 15 ?? ?? ?? ?? 8B 4C 24 10 8B 54 24 0C " \
      "8B 44 24 08 51 8B 4E ?? 69 C9 ?? ?? ?? ?? 52 50 8D 8C 31 ?? ?? ?? ?? E8 ?? ?? ?? ??",                        \
      2)                                                                                                            \
  /* The Mercenaries' kill award (+15, thiscall (player, amount) on sGameMan), after the combo marked running   */ \
  /* (+10).                                                                                                      */ \
  SIG(kSigComboAwardCall,                                                                                           \
      "8B 44 24 ?? 50 57 8B CD C6 86 ?? ?? ?? ?? 01 E8 ?? ?? ?? ?? 80 7D 1C 00",                                    \
      1)                                                                                                            \
  /* ... its end: the award added to what the combo has earned (+6, stored +25), at most the score's cap (+12   */ \
  /* +19).                                                                                                       */ \
  SIG(kSigComboAwardEnd,                                                                                            \
      "8B 5C 24 10 03 BB ?? ?? ?? ?? 81 FF ?? ?? ?? ?? 76 05 BF ?? ?? ?? ?? 89 BB ?? ?? ?? ?? 5F 5E 5B 5D 59 C2 08 " \
      "00",                                                                                                         \
      1)                                                                                                            \
  /* sUnit's line frame: a unit starting (state 1) made running (2) and started - its vtable's slot +28 (a      */ \
  /* byte offset) called once.                                                                                   */ \
  SIG(kSigUnitStart,                                                                                                \
      "8B C8 83 E1 07 F3 0F 11 46 20 83 F9 01 75 ?? 8B 16 83 E0 FA 83 C8 02 89 46 10 8B 42 ?? 8B CE FF D0",          \
      1)                                                                                                            \
  /* ... a unit going (state 4): taken off its line, then its vtable's first slot (its destructor) called with 1. */ \
  SIG(kSigUnitDestroy,                                                                                              \
      "83 F9 04 75 ?? 83 E0 F8 56 8B CF 89 46 10 E8 ?? ?? ?? ?? 8B 16 8B 02 6A 01 8B CE FF D0",                     \
      1)                                                                                                            \
  /* uModel's joint matrix (its vtable slot 20; thiscall (joint), ret 4): below 0 the model's world matrix +10   */ \
  /* (4x4 floats: the axes, the position), else the joint by the table +19 in the joints +41.                    */ \
  SIG(kSigJointMatrix,                                                                                              \
      "8B 44 24 04 85 C0 79 09 8D 81 ?? ?? ?? ?? C2 04 00 8B 91 ?? ?? ?? ?? 25 FF 00 00 00 0F B6 04 02 3D FF 00 "   \
      "00 00 74 E1 8B 89 ?? ?? ?? ?? 8D 04 80 C1 E0 05 8D 44 01 10 C2 04 00",                                       \
      1)                                                                                                            \
  /* The drowning (cdecl (player); uPlPlayer's frame, its slot 112, runs it in the stages with a dive: 250 and   */ \
  /* 1003), whole: when its slot 58 says this machine runs the character (+7: the network's no), it is under     */ \
  /* water (the test called +23: a bit of a flags word) and its air - the combat gauge, a float - is gone (+38), */ \
  /* with bit 1 of +46 clear, sPlayer's (+58) set-health (+65) puts it at 0 and its slot 46 (+72) plays the      */ \
  /* drowning (+87: action 0x226).                                                                               */ \
  SIG(kSigDrown,                                                                                                    \
      "56 8B 74 24 08 8B 06 8B 90 E8 00 00 00 8B CE FF D2 84 C0 74 ?? 8B CE E8 ?? ?? ?? ?? 84 C0 74 ?? 0F 57 C0 "   \
      "0F 2F 86 ?? ?? ?? ?? 72 ?? 8B 86 ?? ?? ?? ?? D1 E8 A8 01 75 ?? 8B 0D ?? ?? ?? ?? 6A 00 56 E8 ?? ?? ?? ?? "   \
      "8B 16 8B 82 B8 00 00 00 6A 00 6A FF 6A 00 6A 00 68 ?? ?? ?? ?? 8B CE FF D0 5E C3",                           \
      1)                                                                                                            \
  /* The air gauge on screen (uIdOxygen's frame, its slot 8): its player (+2), under water (the same test,      */ \
  /* called +12) with health (+34, a word) above 0, the gauge shown is the air (+88) over its most (+96).        */ \
  SIG(kSigAirGauge,                                                                                                 \
      "8B 8E ?? ?? ?? ?? 88 86 ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 66 83 B8 ?? ?? " \
      "?? ?? 00 0F 8E ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00 0F 84 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? 83 EC 0C F3 0F 11 " \
      "44 24 08 0F 57 C0 F3 0F 11 44 24 04 F3 0F 10 80 ?? ?? ?? ?? F3 0F 5E 80 ?? ?? ?? ??",                        \
      1)                                                                                                            \
  /* The modes' match clock (sGameMan's; thiscall (time), ret 4), its start: the host's number +5, this          */ \
  /* machine's +11, the match's end state +23.                                                                   */ \
  SIG(kSigMercClockHead,                                                                                            \
      "56 8B F1 8B 86 ?? ?? ?? ?? 3B 86 ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 83 E8 00 57 74 ?? 48 0F "   \
      "84 ?? ?? ?? ?? 5F 5E C2 04 00 F3 0F 10 86 ?? ?? ?? ?? F3 0F 10 4C 24 0C F3 0F 58 C1",                        \
      1)                                                                                                            \
  /* ... its run-down: the mode +2, the seconds left +10 (and +80), the `subss` +17, the end state +53, online   */ \
  /* +63, the HUD's copy +98.                                                                                    */ \
  SIG(kSigMercClock,                                                                                                \
      "8B BE ?? ?? ?? ?? F3 0F 10 86 ?? ?? ?? ?? 0F 57 D2 F3 0F 5C C1 83 FF 46 75 ?? 8B CE E8 ?? ?? ?? ?? D9 5C "   \
      "24 0C F3 0F 10 54 24 0C 0F 2F D0 72 ?? 0F 28 C2 C7 86 ?? ?? ?? ?? 01 00 00 00 80 BE ?? ?? ?? ?? 00 F3 0F "   \
      "58 8E ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? F3 0F 11 8E ?? ?? ?? ?? 75 ?? F3 0F 11 86 ?? ?? ?? ??",            \
      1)                                                                                                            \
  /* The match's frame (sGameMan's): its step +6 (1 while it plays), the clock called +88 (kSigMercClockHead).   */ \
  SIG(kSigMercFrame,                                                                                                \
      "51 55 8B E9 8B 85 ?? ?? ?? ?? 83 F8 04 74 ?? 8B 0D ?? ?? ?? ?? F3 0F 10 81 ?? ?? ?? ?? F3 0F 11 44 24 04 "   \
      "85 C0 75 ?? C7 85 ?? ?? ?? ?? 01 00 00 00 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 32 C0 5D 59 C3 8B 81 ?? ?? "   \
      "?? ?? C1 E8 1E A8 01 75 ?? 51 8B CD F3 0F 11 04 24 E8 ?? ?? ?? ?? 8B CD E8 ?? ?? ?? ?? 83 BD ?? ?? ?? ?? "   \
      "03 0F 57 C9 74 ?? 80 BD ?? ?? ?? ?? 00 74 ?? F3 0F 10 85 ?? ?? ?? ?? F3 0F 5C 44 24 04 0F 2F C8 72 ?? 0F "   \
      "28 C1 F3 0F 11 85 ?? ?? ?? ??",                                                                              \
      1)                                                                                                            \
  /* A time bonus added to the match clock: the most the clock holds +4 (a float's place: 99:59.99), the clock   */ \
  /* +19.                                                                                                        */ \
  SIG(kSigMercBonus,                                                                                                \
      "F3 0F 10 0D ?? ?? ?? ?? 0F 57 C0 F3 0F 2A C0 F3 0F 58 86 ?? ?? ?? ?? 0F 2F C1 F3 0F 11 86 ?? ?? ?? ?? 76 "   \
      "?? F3 0F 11 8E ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00",                                                            \
      1)                                                                                                            \
  /* A stage's scenario timer, read (thiscall (n) on the stage; a float back): 32 of them +6, the timers +12.    */ \
  SIG(kSigStageGet,                                                                                                 \
      "8B 44 24 04 83 F8 20 73 0A D9 84 81 ?? ?? ?? ?? C2 04 00 D9 EE C2 04 00",                                    \
      1)                                                                                                            \
  /* The stage's frame running its timers: the timers + 4 +20, each one running (a bit of the word +29) counted  */ \
  /* down (a bit of +42) or up.                                                                                  */ \
  SIG(kSigStageTimers,                                                                                              \
      "A1 ?? ?? ?? ?? F3 0F 10 80 ?? ?? ?? ?? B8 01 00 00 00 8D 8E ?? ?? ?? ?? 8D 50 03 85 86 ?? ?? ?? ?? 74 ?? "   \
      "F3 0F 10 49 FC 85 86 ?? ?? ?? ?? 74 06 F3 0F 5C C8 EB 04 F3 0F 58 C8 F3 0F 11 49 FC D1 C0",                  \
      1)                                                                                                            \
  /* The scenario's CountDown order showing a timer on the stage's uIdStageTimer: its place in the stage +44,    */ \
  /* its shown byte +81, the stage timer read +126 (a call of kSigStageGet's).                                   */ \
  SIG(kSigCountDown,                                                                                                \
      "83 EC 08 56 8B 74 24 10 8B D1 85 F6 75 ?? B8 01 00 00 00 5E 83 C4 08 C2 08 00 A1 ?? ?? ?? ?? 8B 88 ?? ?? "   \
      "?? ?? 85 C9 74 ?? 57 8B B9 ?? ?? ?? ?? 85 FF 75 ?? 8D 47 01 5F 5E 83 C4 08 C2 08 00 0F B6 46 04 83 E8 00 "   \
      "74 ?? 48 0F 85 ?? ?? ?? ?? 88 87 ?? ?? ?? ?? 5F 5E 83 C4 08 C2 08 00 0F 57 C0 C6 87 ?? ?? ?? ?? 01 0F B6 "   \
      "46 06 83 E8 00 F3 0F 11 44 24 14 74 ?? 48 75 ?? 0F B6 56 07 52 E8 ?? ?? ?? ?? D9 5C 24 14 EB ??",            \
      1)                                                                                                            \
  /* The SmallTimer order (a gauge over the players) reading its timer: the stage timer read +39 (a call of      */ \
  /* kSigStageGet's).                                                                                            */ \
  SIG(kSigSmallTimerRead,                                                                                           \
      "0F B6 46 04 83 E8 00 F3 0F 10 56 08 0F 28 C8 F3 0F 11 4C 24 18 F3 0F 11 54 24 1C 74 ?? 48 75 ?? 0F B6 46 "   \
      "05 50 8B CB E8 ?? ?? ?? ?? 0F 57 C0",                                                                        \
      1)                                                                                                            \
  /* The CountDown order reading a timer of its own FSM's in place (its timer kind 0; the FSM is the order's     */ \
  /* `this`, in edx): the timer's number +3 (the order's +7), the FSM's timers +9 - 19 bytes the mod rewrites     */ \
  /* into a call of its own read. kSigCountDown +137, right after the stage timer's read.                         */ \
  SIG(kSigCountDownOwn,                                                                                             \
      "0F B6 46 07 F3 0F 10 84 82 ?? ?? ?? ?? F3 0F 11 44 24 14 0F B6 46 05 83 E8 00 0F 84",                        \
      1)                                                                                                            \
  /* An FSM's own timers run each frame (cFSMOrder's; thiscall (the frame's time)): the timers +45, how many +59, */ \
  /* each one running (a bit of the word +66) counted down (a bit of +78) or up (the add +84).                    */ \
  SIG(kSigOwnTimers,                                                                                                \
      "83 EC 08 A1 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 53 8B D9 89 44 24 08 85 C0 0F 84 ?? ?? ?? ?? F3 0F 10 54 24 10 "    \
      "55 56 57 BD 01 00 00 00 8D BB ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? C7 44 24 10 20 00 00 00 90 85 AB ?? ?? ?? ?? "    \
      "74 ?? F3 0F 10 07 85 AB ?? ?? ?? ?? 75 ?? F3 0F 58 C2 F3 0F 11 07",                                          \
      1)                                                                                                            \
  /* The inventory's set-slot (thiscall (slot or -1, id, count, mode, unit) on the player, ret 0x14; the slot    */ \
  /* back): the inventory +34. A pickup's item goes in through it.                                               */ \
  SIG(kSigItemSetSlot,                                                                                              \
      "51 53 55 8B 6C 24 14 C1 FD 08 56 57 81 E5 FF 00 00 00 8B F9 89 7C 24 10 83 FD 01 75 ?? 33 D2 90 8D 8F ?? "   \
      "?? ?? ?? 33 C0 0F B6 59 05",                                                                                 \
      1)                                                                                                            \
  /* The inventory's top-up of the stacks it has of an item (thiscall (id, count), ret 8; what did not fit       */ \
  /* back): the first slot it fills +27, the inventory +33.                                                      */ \
  SIG(kSigItemTopUp,                                                                                                \
      "83 EC 08 8B 44 24 0C 53 55 56 57 50 8B F1 E8 ?? ?? ?? ?? 8B 6C 24 24 83 C4 04 B9 ?? 00 00 00 81 C6 ?? ?? "   \
      "?? ??",                                                                                                      \
      1)                                                                                                            \
  /* The inventory's refresh of its summary slots (the grenades' and the spray's totals; thiscall on the         */ \
  /* player): the first summary slot +24.                                                                        */ \
  SIG(kSigItemRefresh,                                                                                              \
      "51 53 55 56 33 C0 57 8B E9 89 44 24 10 8D 49 00 0F B7 98 ?? ?? ?? ?? B9 ?? 00 00 00",                        \
      1)                                                                                                            \
  /* cItem's clear (thiscall): its unit +6 let go, its fields zeroed - the game's own discard of an item.        */ \
  SIG(kSigItemClear,                                                                                                \
      "53 56 8B F1 8B 46 18 33 DB 3B C3 74 14 81 60 10 FF DF FF FF 8B 4E 18 8B 01 8B 50 3C FF D2 89 5E 18",         \
      1)                                                                                                            \
  /* The game's loader, sLoadMan, asked for a class's archive (thiscall (kind, number, variant, flag), ret       */ \
  /* 0x10): the class looked up by its key, (kind << 16 | number) << 8 | variant (the lookup's call +64), its    */ \
  /* archive "arc\DX9\<class name>" added to the list the loader adds to unless a list holds it already, and     */ \
  /* what the class needs with it asked for too (a launcher's rounds, the crossbow's arrows, a grenade's thrown  */ \
  /* form).                                                                                                      */ \
  SIG(kSigLoadRequest,                                                                                              \
      "83 EC 34 55 8B 6C 24 3C 56 57 33 FF 8B F1 89 7C 24 0C 83 FD 03 75 ?? 81 7C 24 48 ?? ?? 00 00 75 ?? C7 "      \
      "44 24 48 ?? ?? 00 00 0F B7 4C 24 48 0F B6 54 24 4C 8B C5 C1 E0 10 0B C1 C1 E0 08 0B C2 50 E8",               \
      1)                                                                                                            \
  /* ... its commit of the list it adds to (thiscall, no arguments; how many entries wait to load, back): which  */ \
  /* list +7 (a dword of sLoadMan's), the lists' counts +29 and items +50 (MtArrays 5 * 4 bytes apart), an       */ \
  /* entry's state +59 - 0 let go with its archive (+81), 1 or 4 asked for.                                      */ \
  SIG(kSigLoadCommit,                                                                                               \
      "51 55 57 8B F9 8B 87 ?? ?? ?? ?? 8D 04 80 33 D2 33 C9 8D 04 87 33 ED 89 4C 24 08 39 90 ?? ?? ?? ?? 0F "      \
      "8E ?? ?? ?? ?? 53 56 8D 5A 01 8D 64 24 00 8B 80 ?? ?? ?? ?? 8B 34 A8 8B 46 ?? 2B C2 74 ?? 48 74 ?? 83 "      \
      "E8 03 75 ?? 41 89 4C 24 10 EB ?? 8B 4E ?? 3B CA 74 ?? E8",                                                   \
      1)                                                                                                            \
  /* ... and the commit's end, the list's job set to load what waits: which list +6, a job's size +12, the jobs  */ \
  /* +16, a job asked for: its word +28 made 1.                                                                  */ \
  SIG(kSigLoadJob,                                                                                                  \
      "3B CA 7E ?? 8B 87 ?? ?? ?? ?? 6B C0 ?? 8D 84 38 ?? ?? ?? ?? 89 50 40 89 50 44 C7 40 ?? 01 00 00 00 89 "      \
      "50 4C 89 50 50 5F 8B C1 5D 59 C3",                                                                           \
      1)                                                                                                            \
  /* sLoadMan's frame (its vtable slot 6; the game's frame calls it in play, not while the game is paused): its  */ \
  /* state +8 (a byte, 0 idle), the jobs +50 (+66 of them, +63 bytes each), a job asked for: bit 0 of its word   */ \
  /* +56.                                                                                                        */ \
  SIG(kSigLoadFrame,                                                                                                \
      "83 EC 08 56 8B F1 8B 46 ?? 25 FF 00 00 00 83 F8 04 0F 87 ?? ?? ?? ?? 53 55 57 FF 24 85 ?? ?? ?? ?? 33 "      \
      "DB C6 44 24 13 00 89 9E ?? ?? ?? ?? 33 ED 8D BE ?? ?? ?? ?? F6 47 ?? 01 75 ?? 45 83 C7 ?? 83 FD ?? 72",      \
      1)                                                                                                            \
  /* The loader's add of an archive to the list it adds to (thiscall (name, class, flag)): nothing when one of   */ \
  /* the lists (+39 of them) holds that path already - that entry is kept; else an entry of +44 bytes, its path  */ \
  /* +71 (an MtString, its characters +81 on) begun with the folder +93 ("arc\DX9\").                            */ \
  SIG(kSigLoadAdd,                                                                                                  \
      "53 55 8B 6C 24 0C 8B D9 85 ED 0F 84 ?? ?? ?? ?? 56 33 F6 55 56 8B CB E8 ?? ?? ?? ?? 84 C0 0F 85 ?? ?? "      \
      "?? ?? 46 83 FE ?? 72 ?? 57 6A ?? E8 ?? ?? ?? ?? 8B F8 83 C4 04 85 FF 0F 84 ?? ?? ?? ?? C7 07 ?? ?? ?? "      \
      "?? C7 47 ?? 00 00 00 00 8B 47 ?? 8D 48 ?? 85 C0 75 ?? B9 ?? ?? ?? ?? 81 F9 ?? ?? ?? ??",                     \
      1)                                                                                                            \
  /* A list's job on an entry being loaded: its archive +2 (and +24), the archive's flags +9 - failed (the bit   */ \
  /* +12), asked for again from its path +31 (an MtString's characters +46 on), or loaded (bit 0 of +86: the     */ \
  /* entry's state +92 made 3).                                                                                  */ \
  SIG(kSigLoadPoll,                                                                                                 \
      "8B 4E ?? 85 C9 74 ?? 8B 41 ?? C1 E8 ?? A8 01 74 ?? E8 ?? ?? ?? ?? C7 46 ?? 00 00 00 00 8B 46 ?? 8B 0D "      \
      "?? ?? ?? ?? 85 C0 74 ?? 8B 11 83 C0 ?? 6A 02 50 8B 42 ?? 68 ?? ?? ?? ?? FF D0 EB ?? 8B 11 6A 02 B8 ?? "      \
      "?? ?? ?? 50 8B 42 ?? 68 ?? ?? ?? ?? FF D0 EB ?? F6 41 ?? 01 74 ?? C7 46 ?? 03 00 00 00",                     \
      1)                                                                                                            \
  /* The loader's choice of the list it adds to (thiscall (list), ret 4): which list +6 (again +34), every       */ \
  /* entry's state (+53) made 0 - one asked for again is kept, the others a commit lets go. The counts +18, the  */ \
  /* items +44.                                                                                                  */ \
  SIG(kSigLoadChoose,                                                                                               \
      "8B 54 24 04 89 91 ?? ?? ?? ?? 33 C0 8D 14 92 39 84 91 ?? ?? ?? ?? 7E ?? EB ?? 8D 9B 00 00 00 00 8B 91 "      \
      "?? ?? ?? ?? 8D 14 92 8B 94 91 ?? ?? ?? ?? 8B 14 82 C7 42 ?? 00 00 00 00",                                    \
      1)                                                                                                            \
  /* A stage's load of the characters' archives: the list it chooses +14 (the call +15, kSigLoadChoose's),       */ \
  /* sLoadMan +9 (again +39), then the players' own asked for (the request +51) ...                              */ \
  SIG(kSigCharsLoad,                                                                                                \
      "83 EC 14 89 4C 24 10 8B 0D ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A FF E8 ?? ?? ?? ?? 85 "      \
      "C0 74 ?? 8B 0D ?? ?? ?? ?? 6A 00 6A 00 6A ?? 6A ?? E8",                                                      \
      1)                                                                                                            \
  /* ... and each character's items as the last stage left them: a category-1 item (a weapon, a grenade, the     */ \
  /* spray) asked for by the kind +36 (the request +37; sLoadMan +26).                                           */ \
  SIG(kSigCharsItems,                                                                                               \
      "0F BF 07 25 FF 0F 00 00 8B D0 81 E2 00 FF 00 00 81 FA 00 01 00 00 75 ?? 8B 0D ?? ?? ?? ?? 6A 00 6A 00 "      \
      "50 6A ?? E8",                                                                                                \
      1)                                                                                                            \
  /* Set-slot making a category-1 item's unit (sItem's make: the class by its key): the kind +29 (the call +30). */ \
  SIG(kSigItemMake,                                                                                                 \
      "85 F6 75 ?? 83 FD 01 0F 85 ?? ?? ?? ?? 8B 44 24 1C 8B 0D ?? ?? ?? ?? 6A 00 6A 00 50 6A ?? E8",               \
      1)                                                                                                            \
  /* The Mercenaries' combo clock (sGameMan's; thiscall (block, time)): a score block's size +11, its combo      */ \
  /* running +21, the mode +34, the combo's clock +43, its `subss` +47.                                          */ \
  SIG(kSigMercCombo,                                                                                                \
      "55 8B 6C 24 08 56 57 8B FD 69 FF ?? ?? ?? ?? 8B F1 03 FE 80 BF ?? ?? ?? ?? 00 0F 84 ?? ?? ?? ?? 83 BE ?? "   \
      "?? ?? ?? 28 F3 0F 10 87 ?? ?? ?? ?? F3 0F 5C 44 24 14",                                                      \
      1)                                                                                                            \
  /* ... a combo's end, its earnings banked: the mode +10 (and +76), a block's size +25, the combo +32 (+70),    */ \
  /* the best +39 (+50), the score +56 (+115), the earnings +62 (+121), No Mercy's mode +80 and its cap +87,     */ \
  /* the cap +102, the combo running +127.                                                                       */ \
  SIG(kSigMercComboEnd,                                                                                             \
      "56 8B 74 24 08 57 8B F9 83 BF ?? ?? ?? ?? 28 75 06 56 E8 ?? ?? ?? ?? 69 F6 ?? ?? ?? ?? 8B 8C 3E ?? ?? ?? "   \
      "?? 3B 8C 3E ?? ?? ?? ?? 8D 04 3E 7E 06 89 88 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 03 88 ?? ?? ?? ?? 33 D2 89 90 "   \
      "?? ?? ?? ?? 83 BF ?? ?? ?? ?? 50 5F 5E 75 0F 81 F9 ?? ?? ?? ?? 76 14 B9 ?? ?? ?? ?? EB 0D 81 F9 ?? ?? ?? "   \
      "?? 76 05 B9 ?? ?? ?? ?? 89 88 ?? ?? ?? ?? 89 90 ?? ?? ?? ?? 88 90 ?? ?? ?? ?? C2 04 00",                     \
      1)                                                                                                            \
  /* ... Combo Time's clock: the mode +2 (the Mercenaries' +8, No Mercy's +13), a block's size +76, Combo Time   */ \
  /* running +84, its clock +95, its `subss` +99, its kills +156.                                                */ \
  SIG(kSigMercComboTime,                                                                                            \
      "8B 81 ?? ?? ?? ?? 83 F8 1E 74 ?? 83 F8 50 74 ?? 83 F8 28 0F 85 ?? ?? ?? ?? 8B 54 24 04 83 FA 01 0F 8F ?? "   \
      "?? ?? ?? EB ?? 8B 54 24 04 85 D2 0F 85 ?? ?? ?? ?? 83 FA 08 72 02 33 D2 83 F8 1E 74 05 83 F8 50 75 02 33 "   \
      "D2 56 8B F2 69 F6 ?? ?? ?? ?? 03 F1 80 BE ?? ?? ?? ?? 00 74 ?? F3 0F 10 86 ?? ?? ?? ?? F3 0F 5C 44 24 0C "   \
      "0F 57 C9 0F 2F C8 72 ?? 0F 28 C1 F3 0F 11 44 24 08 83 F8 28 74 ?? 8B 81 ?? ?? ?? ?? 3B 81 ?? ?? ?? ?? 0F "   \
      "94 C0 84 C0 74 ?? 52 C6 86 ?? ?? ?? ?? 00 C7 86 ?? ?? ?? ?? 00 00 00 00",                                    \
      1)                                                                                                            \
  /* ... the Combo Time hourglass broken: the mode +5, the match's step +19, how long Combo Time runs +88 (a     */ \
  /* float's place).                                                                                             */ \
  SIG(kSigMercComboTimePickup,                                                                                      \
      "56 8B F1 8B 86 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 83 BE ?? ?? ?? ?? 03 0F 84 ?? ?? ?? ?? 83 F8 28 74 ?? "   \
      "8B 8E ?? ?? ?? ?? 3B 8E ?? ?? ?? ?? 0F 94 C1 84 C9 0F 84 ?? ?? ?? ?? 8B 4C 24 08 57 8B F9 83 F9 08 72 02 "   \
      "33 FF 83 F8 1E 74 05 83 F8 50 75 02 33 FF F3 0F 10 05 ?? ?? ?? ?? 6A 40",                                    \
      1)                                                                                                            \
  /* ... the stage cleared at its kills: the mode +2 (the Mercenaries' +13, No Mercy's +40), the stage's kills   */ \
  /* +19 (+45), the targets +25 and +51 (an int's place each), the end state +34 (+59).                          */ \
  SIG(kSigMercTarget,                                                                                               \
      "8B 81 ?? ?? ?? ?? BA 01 00 00 00 83 F8 1E 75 ?? 56 8B B1 ?? ?? ?? ?? 3B 35 ?? ?? ?? ?? 5E 7C ?? 89 91 ?? "   \
      "?? ?? ?? 83 F8 50 75 ?? 8B 81 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 7C ?? 89 91 ?? ?? ?? ?? C3",                     \
      1)                                                                                                            \
  /* An enemy's brain taking its hit (the enemy's own hit record): the record valid +49, the record +57.         */ \
  SIG(kSigHitCheck,                                                                                                 \
      "83 EC 10 53 55 56 8B 74 24 20 57 33 ED 6A 01 89 4C 24 18 33 DB 55 8B CE 33 FF 88 5C 24 1B 88 5C 24 1A 89 "   \
      "6C 24 24 E8 ?? ?? ?? ?? 84 C0 74 ?? 38 9E ?? ?? ?? ?? 74 ?? 8D BE ?? ?? ?? ?? 3B FD 74 ?? C6 44 24 13 01 "   \
      "8D 86 ?? ?? ?? ?? 89 44 24 18 83 FD 05",                                                                     \
      1)                                                                                                            \
  /* A hit from the network written into an enemy's hit record (a DamageInfo): valid +14, the damage +25, the    */ \
  /* attribute words +66 (a bit each).                                                                           */ \
  SIG(kSigHitRebuild,                                                                                               \
      "8B 44 24 04 85 C0 0F 84 ?? ?? ?? ?? C6 40 ?? 01 8B 91 ?? ?? ?? ?? 56 89 50 ?? 57 BE 01 00 00 00 BA 02 00 "   \
      "00 00 EB 09 8D A4 24 00 00 00 00 8B FF 8D 7A FE C1 EF 05 85 B4 B9 ?? ?? ?? ?? 74 04 09 74 B8 ??",            \
      1)                                                                                                            \
  /* ... the rest of it: the attribute bits +10, the position +28 +39 +53 (w +50), the direction +62 +71 +85 (w  */ \
  /* +82), two words +94 +104, the attack +113, who dealt it +122, two bytes +131 +140.                          */ \
  SIG(kSigHitRebuildPos,                                                                                            \
      "83 C2 06 8D 7A FE D1 C6 83 FF ?? 0F 82 ?? ?? ?? ?? D9 81 ?? ?? ?? ?? 0F 57 C0 D9 58 ?? 5F D9 81 ?? ?? ?? "   \
      "?? 5E D9 58 ?? D9 81 ?? ?? ?? ?? F3 0F 11 40 ?? D9 58 ?? D9 81 ?? ?? ?? ?? D9 58 ?? D9 81 ?? ?? ?? ?? D9 "   \
      "58 ?? D9 81 ?? ?? ?? ?? F3 0F 11 40 ?? D9 58 ?? 8B 91 ?? ?? ?? ?? 89 50 ?? 0F B7 91 ?? ?? ?? ?? 89 50 ?? "   \
      "8B 91 ?? ?? ?? ?? 89 50 ?? 8B 91 ?? ?? ?? ?? 89 50 ?? 8A 91 ?? ?? ?? ?? 88 50 ?? 8A 89 ?? ?? ?? ?? 88 48 "   \
      "?? C2 04 00",                                                                                                \
      1)                                                                                                            \
  /* An enemy's damage handler reporting its death once: its state +2, the health +9, died +22 (and +81), the    */ \
  /* kill report called +93.                                                                                     */ \
  SIG(kSigKillReport,                                                                                               \
      "80 7E 38 02 74 ?? 66 83 BE ?? ?? ?? ?? 00 0F 8F ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00 0F 85 ?? ?? ?? ?? 8A 96 "   \
      "?? ?? ?? ?? C0 EA 03 F6 C2 01 74 ?? A1 ?? ?? ?? ?? 83 B8 ?? ?? ?? ?? 28 75 ?? 8B 16 8B 82 ?? ?? ?? ?? 8B "   \
      "CE FF D0 8B 4C 24 1C 51 57 C6 86 ?? ?? ?? ?? 01 8B 0D ?? ?? ?? ?? 56 E8 ?? ?? ?? ??",                        \
      1)                                                                                                            \
  /* ... the kill report itself: the enemy's flags +30 (bit 31: not reported), reported +47.                     */ \
  SIG(kSigKillOnce,                                                                                                 \
      "85 F6 0F 84 ?? ?? ?? ?? 8B 06 8B 90 ?? ?? ?? ?? 8B CE FF D2 84 C0 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? C1 "   \
      "E8 1F A8 01 0F 85 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00 0F 85",                                                   \
      1)                                                                                                            \
  /* --- the Mercenaries' flow (merc.cpp): how a stage sends its enemies ---                                      */ \
  /* EnemyAdd's stock built (its state function's first step; thiscall (stock, list, param), ret 0xC: the call  */ \
  /* +11), then the param's mRemainEmNum (+18) read.                                                              */ \
  SIG(kSigEnemyAddStock, "53 8D 45 ?? 50 8D 75 ?? 56 8B CF E8 ?? ?? ?? ?? 8B 7B ?? 83 FF 20", 1)                   \
  /* ... and its wait, in the same function: fewer in play than the param's mMinEmNum (+2), then the work's     */ \
  /* timer (+13) above 0 (the constant's address +17) has the frame's time taken off it (the order's slot +27;  */ \
  /* the timer +34, +37) - else the jbe +21 to the send, which reads mAddEmNum.                                  */ \
  SIG(kSigEnemyAddWait,                                                                                             \
      "3B 43 ?? 0F 83 ?? ?? ?? ?? F3 0F 10 45 ?? 0F 2F 05 ?? ?? ?? ?? 76 ?? 8B 17 8B 42 ?? 8B CF FF D0 D8 6D ?? "   \
      "D9 5D ??",                                                                                                   \
      1)                                                                                                            \
  /* The enemies' brains' end step (uEmIntel's; each family's brain ends its own with it): its own work called,  */ \
  /* then an enemy (the brain's +10) whose byte +13 is set - the last its erase sets - has the brain's mRno    */  \
  /* (+22) and its own (+29) made 3: the unit ends.                                                              */ \
  SIG(kSigBrainEnd,                                                                                                 \
      "56 8B F1 E8 ?? ?? ?? ?? 8B 46 ?? 80 B8 ?? ?? ?? ?? 01 75 ?? C7 46 ?? 03 00 00 00 C7 40 ?? 03 00 00 00 5E C3", \
      1)                                                                                                            \
  /* The AreaHit order's area test (its kind 0: a player in an area task): the task got (+2), then asked whether */ \
  /* a player is inside it (the call +17; fastcall (task)).                                                       */ \
  SIG(kSigAreaHitInside, "8B CD E8 ?? ?? ?? ?? 3B C6 0F 84 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 88 44 24 ?? E9", 1)     \
  /* A player's number (its index byte +2; 8 players +8).                                                        */ \
  SIG(kSigPlayerIndex,                                                                                              \
      "8A 89 ?? ?? ?? ?? 80 F9 08 1A C0 22 C1 C3",                                                                  \
      1)                                                                                                            \
  /* --- the records windows (records.cpp) ---                                                                    */ \
  /* The Records screen's (uIdRecord00) constructor filling its chapter grid from the profile in use: the grid  */ \
  /* +2 (a byte each, character x difficulty x chapter), sInfoMan's global +25, the profile in use +31, a        */ \
  /* profile's size +38, its cSystemInfo +47 (from sInfoMan's start); "not cleared" +50 (and +97); the cleared   */ \
  /* get called +54 (rel +55), the rank get +88 (rel +89), the rank's most +101; chapters +115, difficulties +126,*/ \
  /* characters +144; the completed percent's function +157 (rel +158), stored +164.                             */ \
  SIG(kSigRecordGridFill,                                                                                           \
      "8D 86 ?? ?? ?? ?? 8B FF 33 ED 89 44 24 14 33 FF EB 06 8D 9B 00 00 00 00 A1 ?? ?? ?? ?? 8B 48 ?? 8B 54 24 10 "\
      "69 C9 ?? ?? ?? ?? 57 55 8D 4C 01 ?? 52 BB ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 ?? A1 ?? ?? ?? ?? 8B 48 ?? 8B "\
      "54 24 10 69 C9 ?? ?? ?? ?? 57 55 8D 4C 01 ?? 52 E8 ?? ?? ?? ?? 8B D8 83 FB ?? 72 05 BB ?? ?? ?? ?? 8B 44 24 "\
      "14 88 1C 38 47 83 FF ?? 7C A2 83 44 24 14 ?? 45 83 FD ?? 7C 8D 8B 4C 24 10 8B 44 24 14 41 89 4C 24 10 83 F9 "\
      "?? 0F 8C ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 86 ?? ?? ?? ??",                                     \
      1)                                                                                                            \
  /* The Records screen's frame (its vtable slot 8), its pages: the page +23 (a dword), the last page +26, the   */ \
  /* jump table +32; page 0's (PROFILES) call +38 (rel +39: the case at +36), page 3's (MEDALS) +65 (rel +66:    */ \
  /* the case at +63).                                                                                           */ \
  SIG(kSigRecordPages,                                                                                              \
      "8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 46 ?? 83 F8 04 77 ?? FF 24 85 ?? ?? ?? ?? " \
      "8B CE E8 ?? ?? ?? ?? EB ?? 8B CE E8 ?? ?? ?? ?? EB ?? 8B CE E8 ?? ?? ?? ?? EB ?? 8B CE E8 ?? ?? ?? ?? EB ?? " \
      "8B CE E8 ?? ?? ?? ??",                                                                                       \
      1)                                                                                                            \
  /* The Records screen's input: the page +15 (a byte's place) tested for the medal page +16; the medal category */ \
  /* +60 (and +84), stepped through those the screen shows (a mask, +76).                                       */ \
  SIG(kSigRecordInput,                                                                                              \
      "55 8B EC 83 E4 F0 83 EC 44 53 56 8B F1 83 7E ?? ?? 57 0F 85 ?? ?? ?? ?? 68 FE 00 00 00 E8 ?? ?? ?? ?? 85 C0 " \
      "74 ?? 80 B8 ?? ?? ?? ?? 00 74 ?? A1 ?? ?? ?? ?? F6 40 ?? 01 74 ?? 8B 8E ?? ?? ?? ?? 49 78 ?? BA 01 00 00 00 " \
      "D3 E2 85 96 ?? ?? ?? ?? 74 ?? 89 8E ?? ?? ?? ??",                                                             \
      1)                                                                                                            \
  /* The Records screen's medal page, a medal's count: the category +2 (and +48), sInfoMan's global +64, the     */ \
  /* profile's medal get (thiscall (category, medal)) called +100 (rel +101).                                    */ \
  SIG(kSigMedalPage,                                                                                                \
      "8B 86 ?? ?? ?? ?? 0F 57 C9 8B D0 8B 84 86 ?? ?? ?? ?? 69 D2 ?? ?? ?? ?? 8D 4F ?? 03 C2 03 C1 0F B6 9C 30 ?? "\
      "?? ?? ?? 0F 57 C0 F3 0F 2A C1 8B 8E ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 53 51 8B 0D ?? ?? ?? ?? F3 0F 11 4C "\
      "24 18 F3 0F 11 44 24 1C F3 0F 11 4C 24 20 F3 0F 11 4C 24 24 C7 44 24 34 ?? ?? ?? ?? E8 ?? ?? ?? ??",          \
      1)                                                                                                            \
  /* ... and a medal's name: the category +2, the name's message (thiscall (category, medal)) called +14 (rel   */ \
  /* +15).                                                                                                       */ \
  SIG(kSigMedalName,                                                                                                \
      "8B 96 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53 52 E8 ?? ?? ?? ?? 6A FF 68 E0 00 00 00 8D 4C 24 34",                  \
      1)                                                                                                            \
  /* The Dog Tag screen (uIdCustomize10) opening a list from its menu: the menu's choice +2, the template list's */ \
  /* step +9, the title list's +24 and the emblem list's +30 (the screen's step: a byte, +18 +23 +29).           */ \
  SIG(kSigDogTagLists,                                                                                              \
      "8B 46 ?? 2B C5 89 6E ?? B1 ?? 74 0F 2B C5 74 05 88 4E ?? EB 0A C6 46 ?? ?? EB 04 C6 46 ?? ??",               \
      1)                                                                                                            \
  /* ... its start filling the title list, a row (0x14 bytes) for each title counter: the rows +2, how many      */ \
  /* +12, the counters' unused table +58 (and +67); the profile's title state get called +119 (rel +120), the   */ \
  /* title table's find (counter, level) +151 (rel +152) and its record +163 (rel +164); a record's float flag   */ \
  /* +187, the counter's get (rel +200) or float get (rel +207); the record's goal +221, its stars flag +228.    */ \
  SIG(kSigDogTagTitles,                                                                                             \
      "8D BE ?? ?? ?? ?? 89 44 24 0C 39 86 ?? ?? ?? ?? 0F 8E ?? ?? ?? ?? B9 01 00 00 00 81 E9 ?? ?? ?? ?? 89 4C 24 " \
      "14 EB 10 EB 0A 8D A4 24 00 00 00 00 8D 49 00 8B 4C 24 14 8D 8C 08 ?? ?? ?? ?? 89 4F 04 80 B8 ?? ?? ?? ?? 02 "\
      "75 19 66 0F EF C0 66 0F D6 07 66 0F D6 47 08 C7 47 10 00 00 00 00 E9 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 51 ?? " \
      "69 D2 ?? ?? ?? ?? 8D 4C 0A ?? 50 E8 ?? ?? ?? ?? 89 07 83 E0 7F 89 44 24 18 76 04 FF 44 24 10 8B 47 04 8B 0D " \
      "?? ?? ?? ?? 6A 01 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 51 ?? 69 D2 ?? " \
      "?? ?? ?? 8B D8 80 7B ?? 00 8B 47 04 50 8D 4C 0A ?? 75 07 E8 ?? ?? ?? ?? EB 0A E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? " \
      "89 47 0C 8B 43 ?? 89 47 10 0F B6 4B ?? 8B 5C 24 18",                                                         \
      1)                                                                                                            \
  /* ... its end: a row's size +3, the rows' count +10; ten titles or more +24 give achievement +34 (the game's  */ \
  /* achievement call +35, rel +36).                                                                             */ \
  SIG(kSigDogTagTitlesEnd,                                                                                          \
      "40 83 C7 ?? 89 44 24 0C 3B 86 ?? ?? ?? ?? 0F 8C ?? ?? ?? ?? 83 7C 24 10 ?? 7C ?? 8B 0D ?? ?? ?? ?? 6A ?? E8 " \
      "?? ?? ?? ??",                                                                                                \
      1)                                                                                                            \
  /* ... right after it (+40 from that one), the title list's pages (15 rows each, the division by 15 in the      */ \
  /* pattern): the rows' count +8, the page shown +33, the last page +58 (+156), the pages its buttons go to +67  */ \
  /* (set to 1, then a bit OR'd +142 for each page with a used row - the unused table +109 - whose progress, the  */ \
  /* rows +83 + a row's +0xC, is above 0); a row's size +125, rows a page +128 (+163), a page's bytes +166.       */ \
  SIG(kSigDogTagTitlePages,                                                                                         \
      "8B 8E ?? ?? ?? ?? 8B 9E ?? ?? ?? ?? B8 89 88 88 88 F7 E9 03 D1 C1 FA 03 8B C2 C1 E8 1F 03 C2 89 86 ?? ?? ?? " \
      "?? B8 89 88 88 88 F7 EB 03 D3 C1 FA 03 8B C2 C1 E8 1F 03 C2 89 86 ?? ?? ?? ?? 40 33 C9 C7 86 ?? ?? ?? ?? 01 " \
      "00 00 00 85 C0 7E ?? 33 D2 8D BE ?? ?? ?? ?? 89 54 24 10 89 7C 24 0C 33 C0 8D 9B 00 00 00 00 3B D3 7D ?? 80 " \
      "BA ?? ?? ?? ?? 02 74 ?? 83 3F 00 7F ?? 40 42 83 C7 ?? 83 F8 ?? 7C ?? EB ?? BA 01 00 00 00 D3 E2 09 96 ?? ?? " \
      "?? ?? 8B 54 24 10 8B 7C 24 0C 8B 86 ?? ?? ?? ?? 41 83 C2 ?? 81 C7 ?? ?? ?? ?? 40",                            \
      1)                                                                                                            \
  /* ... and the emblem list, a row (0x14 bytes) for each emblem: the rows +2, how many +12, the profile's       */ \
  /* emblem get called +49 (rel +50), then Steam's word on its achievement +67 (rel +68).                        */ \
  SIG(kSigDogTagEmblems,                                                                                            \
      "8D 9E ?? ?? ?? ?? 89 5C 24 10 39 BE ?? ?? ?? ?? 0F 8E ?? ?? ?? ?? EB 06 8D 9B 00 00 00 00 A1 ?? ?? ?? ?? 8B " \
      "48 ?? 69 C9 ?? ?? ?? ?? 8D 4C 01 ?? 57 E8 ?? ?? ?? ?? 89 03 85 C0 75 ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? " \
      "84 C0",                                                                                                      \
      1)                                                                                                            \
  /* sInfoMan's achievement (thiscall (achievement), ret 4): skipped under sBioMain's two no-achievement bits   */ \
  /* (sBioMain's global +1, the word +10 and +23); sInfoMan's lock - taken when its byte +32 or the global byte  */ \
  /* +38 is set, at +47 (EnterCriticalSection through +51); the emblems +62, the profile in use +67, its size    */ \
  /* +70, its cSystemInfo +77; the emblem get called +79 (rel +80), the emblem set +93 (rel +94); Steam's        */ \
  /* achievements +100 (global) and its unlock called +105 (rel +106).                                           */ \
  SIG(kSigAchievement,                                                                                              \
      "A1 ?? ?? ?? ?? 56 8B F1 8B 88 ?? ?? ?? ?? D1 E9 F6 C1 01 75 ?? F6 80 ?? ?? ?? ?? 01 75 ?? 80 7E ?? 00 75 09 "\
      "80 3D ?? ?? ?? ?? 00 74 0A 8D 56 ?? 52 FF 15 ?? ?? ?? ?? 57 8B 7C 24 0C 83 FF ?? 73 ?? 8B 46 ?? 69 C0 ?? ?? " \
      "?? ?? 8D 4C 30 ?? 57 E8 ?? ?? ?? ?? 85 C0 75 ?? 6A 01 57 8B CE E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? " \
      "?? ??",                                                                                                      \
      1)                                                                                                            \
  /* The menus' save of the profile as they close (uIdCBase's, thiscall, a step of the screen's +5): step 2     */ \
  /* waits for sInfoMan (global +22) +28 to be 0, then its finish (rel +40, (0)); step 1 waits for sStorage      */ \
  /* (global +55) +61 not 1, then sInfoMan's two afters (rel +83 (0, 1), rel +96 (2)); step 0: the profile in    */ \
  /* use (+117, its size +120, its cSystemInfo +127) changed (rel +129)? - sInfoMan's commit (rel +151), the     */ \
  /* profile's cGameInfo (+157, +160, +167) copied (rel +172), and unless sStorage is busy (rel +183) its save   */ \
  /* asked for (rel +199).                                                                                       */ \
  SIG(kSigProfileSave,                                                                                              \
      "56 8B F1 8B 86 ?? ?? ?? ?? 83 E8 00 74 ?? 48 74 ?? 48 75 ?? 8B 0D ?? ?? ?? ?? 39 81 ?? ?? ?? ?? 0F 85 ?? ?? " \
      "?? ?? 50 E8 ?? ?? ?? ?? FF 86 ?? ?? ?? ?? 32 C0 5E C3 A1 ?? ?? ?? ?? 83 B8 ?? ?? ?? ?? 01 0F 84 ?? ?? ?? ?? " \
      "8B 0D ?? ?? ?? ?? 6A 01 6A 00 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A 02 E8 ?? ?? ?? ?? FF 86 ?? ?? ?? ?? 32 C0 " \
      "5E C3 A1 ?? ?? ?? ?? 8B 48 ?? 69 C9 ?? ?? ?? ?? 8D 4C 01 ?? E8 ?? ?? ?? ?? 84 C0 75 04 B0 01 5E C3 8B 0D ?? " \
      "?? ?? ?? 57 8B F9 E8 ?? ?? ?? ?? 8B 57 ?? 69 D2 ?? ?? ?? ?? 8D 8C 3A ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? " \
      "?? ?? E8 ?? ?? ?? ?? 5F 84 C0 75 C9 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? FF 86 ?? ?? ?? ?? 32 C0 5E C3",          \
      1)                                                                                                            \
  /* A chapter's clear, its rewards for a story's chapter (stories +10): the chapter's achievement (story * 5 + */ \
  /* chapter + 1, the call rel +28), its dog tag template (the same number: the template get rel +52, set rel   */ \
  /* +70); Jake's chapter 3 (the chapter in ebp: 2) the files +111 and +118, Chris' chapter 1 +132 and +139     */ \
  /* (the files' set rel +113).                                                                                  */ \
  SIG(kSigClearAward,                                                                                               \
      "8B 7C 24 24 8B 5C 24 20 83 FF ?? 73 ?? 8B 0D ?? ?? ?? ?? 8D 14 BB 8D 74 17 01 56 E8 ?? ?? ?? ?? A1 ?? ?? ?? " \
      "?? 8B 48 ?? 69 C9 ?? ?? ?? ?? 8D 4C 01 ?? 56 E8 ?? ?? ?? ?? 85 C0 75 ?? 8B 0D ?? ?? ?? ?? 6A 01 56 E8 ?? ?? " \
      "?? ?? 8B 54 24 10 8B 0D ?? ?? ?? ?? 6A ?? 52 E8 ?? ?? ?? ?? 8B C7 48 74 ?? 48 75 ?? 3B DD 75 ?? 8B 0D ?? ?? " \
      "?? ?? 6A ?? E8 ?? ?? ?? ?? 6A ?? EB ?? 85 DB 75 ?? 8B 0D ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 6A ?? 8B 0D ?? ?? " \
      "?? ?? E8 ?? ?? ?? ??",                                                                                       \
      1)                                                                                                            \
  /* ... then which stories are complete (the test rel +15, stories +30), and any one gives the unlock flag     */ \
  /* (sInfoMan +59, bit +63).                                                                                    */ \
  SIG(kSigClearCampaigns,                                                                                           \
      "33 DB 33 FF 8D 73 01 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 84 C0 74 02 0B DE 47 D1 C6 83 FF ?? 7E E6 8B 0D ?? " \
      "?? ?? ?? 85 DB 0F 84 ?? ?? ?? ?? 8B 41 ?? 69 C0 ?? ?? ?? ?? F6 84 08 ?? ?? ?? ?? ??",                         \
      1)                                                                                                            \
  /* ... Chris' story (its bit +2) gives a stage bit (sInfoMan +21: bit 1) ...                                  */ \
  SIG(kSigClearChris,                                                                                               \
      "F6 C3 ?? 0F 84 ?? ?? ?? ?? 8B 41 ?? 69 C0 ?? ?? ?? ?? 8B 94 08 ?? ?? ?? ?? D1 EA F6 C2 01 75",                \
      1)                                                                                                            \
  /* ... Jake's (+2) another (+21, its bit +27, set +74 +78) ...                                                 */ \
  SIG(kSigClearJake,                                                                                                \
      "F6 C3 ?? 0F 84 ?? ?? ?? ?? 8B 51 ?? 69 D2 ?? ?? ?? ?? 8B 84 0A ?? ?? ?? ?? C1 E8 ?? A8 01 75 ?? 80 79 1C 00 " \
      "8D 79 1C 8B F1 75 09 80 3D ?? ?? ?? ?? 00 74 0A 83 C1 04 51 FF 15 ?? ?? ?? ?? 8B 4E ?? 69 C9 ?? ?? ?? ?? 83 " \
      "8C 31 ?? ?? ?? ?? ??",                                                                                       \
      1)                                                                                                            \
  /* ... Leon's, Chris' and Jake's (+4 +6) the unlock flag's bit +25 (Ada's campaign; sInfoMan +21) ...         */ \
  SIG(kSigClearThree,                                                                                               \
      "8B C3 83 E0 ?? 3C ?? 75 ?? 8B 51 ?? 69 D2 ?? ?? ?? ?? F6 84 0A ?? ?? ?? ?? ??",                               \
      1)                                                                                                            \
  /* ... Ada's (+2) a character bit (sInfoMan +21, the bit +26) ...                                              */ \
  SIG(kSigClearAda,                                                                                                 \
      "F6 C3 ?? 0F 84 ?? ?? ?? ?? 8B 41 ?? 69 C0 ?? ?? ?? ?? 8B 84 08 ?? ?? ?? ?? 25 ?? ?? ?? ?? 33 D2 0B C2 75",    \
      1)                                                                                                            \
  /* ... and every chapter of every story on a difficulty (or a harder one: the test rel +4) gives achievement   */ \
  /* +14 + the difficulty, for the first +36 (the call rel +23).                                                 */ \
  SIG(kSigClearDifficulty,                                                                                          \
      "33 F6 56 E8 ?? ?? ?? ?? 84 C0 74 0F 8D 4E ?? 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 46 83 FE " \
      "?? 72 DB",                                                                                                   \
      1)                                                                                                            \
  /* The game's title check, a title earned: its level (+0 reads it) of the top one +14 or past gives the        */ \
  /* achievement +83 (also +96 +111).                                                                            */ \
  SIG(kSigTitleCheck,                                                                                               \
      "0F B6 7B FE 3B FD 0F 86 ?? ?? ?? ?? 83 FF ?? 72 ?? A1 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? D1 E9 F6 C1 01 75 ?? F6 " \
      "80 ?? ?? ?? ?? 01 75 ?? 80 7E 1C 00 75 09 80 3D ?? ?? ?? ?? 00 74 0A 8D 56 04 52 FF 15 ?? ?? ?? ?? 8B 46 ?? " \
      "69 C0 ?? ?? ?? ?? 8D 4C 30 ?? 6A ?? E8 ?? ?? ?? ?? 85 C0 75 0B 6A 01 6A ?? 8B CE E8 ?? ?? ?? ?? 8B 0D ?? ?? " \
      "?? ?? 6A ?? E8 ?? ?? ?? ??",                                                                                 \
      1)                                                                                                            \
  /* ... and its end: the titles held (a state get rel +2, of +14 counters) written to the counter +56 (the     */ \
  /* counter set rel +62).                                                                                       */ \
  SIG(kSigTitleCount,                                                                                               \
      "57 E8 ?? ?? ?? ?? 85 C0 74 01 45 47 81 FF ?? ?? ?? ?? 7C DF 80 7E 1C 00 75 09 80 3D ?? ?? ?? ?? 00 74 0A 8D " \
      "4E 04 51 FF 15 ?? ?? ?? ?? 8B 56 ?? 69 D2 ?? ?? ?? ?? 55 6A ?? 8D 4C 32 ?? E8 ?? ?? ?? ??",                   \
      1)                                                                                                            \
  /* --- the skills window (records.cpp) ---                                                                      */ \
  /* The skill screen's (uIdSkillSet10) list, filled from the skill table and the profile in use (its start and   */ \
  /* each purchase run it; thiscall, the whole function): the rows' count +0x62 (and +0xD4, +0xDA, +0xFD; at most */ \
  /* +0xDE), the table's count called +0x70 (rel +0x71) and its record +0x83 (rel +0x84); a record's id +0xA2    */ \
  /* (the place every other place is counted from: its mask +0xB2, its skill +0xC3, its requirement +0x116, its   */ \
  /* level +0x13F, its price +0x145, its top level +0x15A; the record's size +0x18F); the screen's kind of list    */ \
  /* +0xA5 (a record is listed when its mask has bit `kind`); sInfoMan's global +0xEA, the profile in use +0xF0,   */ \
  /* a profile's size +0xF7, its cSystemInfo +0x106; the rows +0x10B, a row's id +0 (no displacement), its state  */ \
  /* +0x168, requirement +0x16E, "can be bought" +0x17D, price +0x183; the profile's skill state get called       */ \
  /* +0x10F (rel +0x110) and the game's "can be bought" test +0x176 (rel +0x177); bought (3) +0x11C.              */ \
  SIG(kSigSkillListFill,                                                                                            \
      "83 EC 0C 53 55 56 57 8B F9 33 DB 39 9F ?? ?? ?? ?? 7E ?? 8D AF ?? ?? ?? ?? 89 6C 24 14 8D 49 00 33 F6 39 B7 " \
      "?? ?? ?? ?? 7E ?? 8D 9B 00 00 00 00 8B 0D ?? ?? ?? ?? 56 53 E8 ?? ?? ?? ?? 89 45 00 46 83 C5 04 3B B7 ?? ?? " \
      "?? ?? 7C ?? 8B 6C 24 14 43 83 C5 0C 89 6C 24 14 3B 9F ?? ?? ?? ?? 7C ?? C7 87 ?? ?? ?? ?? 00 00 00 00 8B 0D " \
      "?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B F0 6A 00 89 74 24 1C E8 ?? ?? ?? ?? C7 44 24 10 FF FF FF FF " \
      "C7 44 24 14 00 00 00 00 85 F6 0F 8E ?? ?? ?? ?? 8D 68 ?? 8B 8F ?? ?? ?? ?? B8 01 00 00 00 D3 E0 84 45 ?? 0F " \
      "84 ?? ?? ?? ?? 83 7C 24 10 00 7D ?? 0F B6 4D ?? 89 4C 24 10 0F B6 45 ?? 39 44 24 10 74 ?? FF 87 ?? ?? ?? ?? " \
      "83 BF ?? ?? ?? ?? ?? 89 44 24 10 0F 8D ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 48 ?? 0F B6 5D ?? 69 C9 ?? ?? ?? ?? 8B " \
      "97 ?? ?? ?? ?? 03 D2 8D 4C 01 ?? 53 8D B4 D7 ?? ?? ?? ?? E8 ?? ?? ?? ?? 80 7D ?? ?? 75 ?? 83 F8 ?? 73 ?? A1 " \
      "?? ?? ?? ?? 8B 50 ?? 69 D2 ?? ?? ?? ?? 4B 8D 4C 02 ?? 53 E8 ?? ?? ?? ?? 83 F8 ?? 75 ?? 8A 45 ?? 88 46 ?? 8B " \
      "4D ?? 89 4E ?? EB ?? 0F B6 55 ?? 88 16 8A 4D ?? 88 4E ?? 0F B6 55 ?? 8B 4C 24 14 88 56 ?? C6 46 ?? 00 88 46 " \
      "?? 8A 45 ?? 88 46 ?? 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 88 46 ?? 8B 55 ?? 89 56 ?? C6 46 ?? 00 8B 44 24 14 " \
      "40 83 C5 ?? 89 44 24 14 3B 44 24 18 0F 8C ?? ?? ?? ??",                                                      \
      1)                                                                                                            \
  /* ... and its choice of a skill for a set: the profile the screen fills its list from and buys for (its user,  */ \
  /* a dword +10) made the one in use (sInfoMan's global +16, its push called +22, rel +23) before the row under  */ \
  /* the cursor is read (the rows +44).                                                                           */ \
  SIG(kSigSkillScreenUser,                                                                                          \
      "83 EC 0C 53 55 56 8B F1 8B 86 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 57 50 E8 ?? ?? ?? ?? 8B 8E ?? ?? ?? ?? 8B 9E ?? " \
      "?? ?? ?? 03 C9 8D BC CE ?? ?? ?? ??",                                                                        \
      1)                                                                                                            \
  /* --- the weapons window (records.cpp) ---                                                                     */ \
  /* The Records screen's WEAPONS page (the frame's page 1: kSigRecordPages' case at +45, rel +48), a row: the     */ \
  /* first row shown +2 (a dword), sInfoMan's global +12 (and +35); the weapon table's record of the row's entry   */ \
  /* called +17 (rel +18: RE6CC_WEAPON_ROW) and its "always named" byte +32; the profile in use +41, a profile's   */ \
  /* size +49, its cSystemInfo +56; Times Used called +58 (rel +59: RE6CC_WEAPON_GET) - the row is named when the  */ \
  /* byte is set or Times Used is above 0, else drawn "? ? ?".                                                    */ \
  SIG(kSigRecordWeaponRows,                                                                                         \
      "8B BE ?? ?? ?? ?? 03 7C 24 ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 80 78 ?? 00 A1 ?? " \
      "?? ?? ?? 8B 48 ?? 0F 95 44 24 ?? 69 C9 ?? ?? ?? ?? 8D 4C 01 ?? 57 E8 ?? ?? ?? ?? 89 44 24 ?? 85 C0 7E ?? C6 " \
      "44 24 ?? 01",                                                                                                \
      1)                                                                                                            \
  /* ... the row under the cursor's "Enemies Defeated" (mes_system 0x8CB): sInfoMan's global +14, the profile in   */ \
  /* use +20, the row +23, a profile's size +29, its cSystemInfo +36; the count called +38 (rel +39).             */ \
  SIG(kSigRecordWeaponKills,                                                                                        \
      "68 CB 08 00 00 8B CE E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 51 ?? 8B 86 ?? ?? ?? ?? 69 D2 ?? ?? ?? ?? 8D 4C 0A " \
      "?? 50 E8 ?? ?? ?? ??",                                                                                       \
      1)                                                                                                            \
  /* The page row a weapon's use counts on (sInfoMan's, thiscall (item, mode), ret 8; none: -1): how many records  */ \
  /* (a global, +4), the table's mode bytes +24 (a record's item the word -2 before them: +35), a record's size    */ \
  /* +56, its row the signed byte +73 from its mode. A record matches the item, and the mode or any (below 0).     */ \
  SIG(kSigWeaponEntry,                                                                                              \
      "53 56 8B 35 ?? ?? ?? ?? 33 D2 57 85 F6 7E ?? 8B 7C 24 14 8B 5C 24 10 B8 ?? ?? ?? ?? 8D 64 24 00 0F B7 48 FE " \
      "3B D9 75 ?? 8A 08 84 C9 78 ?? 0F BE C9 3B F9 74 ?? 42 83 C0 ?? 3B D6 7C ?? 5F 5E 83 C8 FF 5B C2 08 00 0F BE " \
      "40 ??",                                                                                                      \
      1)                                                                                                            \
  /* The New Game's reset of a campaign's two saved inventories (sInfoMan's, thiscall: "Start a new game without  */ \
  /* the weapons you've obtained?" answered Yes): the profile in use +2 (and +117), cGameInfo's size +5 (+120) and */ \
  /* the first one's place (sInfoMan's) +12; the character's carried record called +17 (rel +18:                  */ \
  /* RE6CC_CARRIED_GET); its default record called +68 (rel +69; RE6CC_DATA_GET) - the default's first byte into  */ \
  /* the record's slot in hand (+77), its slots from +83 into the record's from +80, +85 dwords; the character's  */ \
  /* weapon table called +102 (rel +103), the stage's place in the story +133 (rel +134), the table's count +155   */ \
  /* (rel +156) - the tables sGameMan's (RE6CC_DATA_GET).                                                          */ \
  SIG(kSigCarriedReset,                                                                                             \
      "8B 45 ?? 69 C0 ?? ?? ?? ?? 8D 8C 28 ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? 8B F8 8A C3 24 01 B9 ?? ?? ?? ?? 88 1F C6 " \
      "47 01 00 C7 47 02 ?? ?? ?? ?? C6 47 06 00 C6 47 08 FF 88 47 09 66 89 4F 0A 8B 0D ?? ?? ?? ?? 53 E8 ?? ?? ?? " \
      "?? 8A 10 88 57 ?? 83 C7 ?? 8D 70 ?? B9 ?? ?? ?? ?? 89 7C 24 ?? F3 A5 8B 0D ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? 8B " \
      "0D ?? ?? ?? ?? 8B F0 8B 45 ?? 69 C0 ?? ?? ?? ?? 8B 84 28 ?? ?? ?? ?? 53 50 E8 ?? ?? ?? ?? 8B F8 89 7C 24 ?? " \
      "85 FF 78 ?? 8B 0D ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? 85 C0",                                                      \
      1)                                                                                                            \
  /* ... and its weapons due by the stage: a table entry's stage its first word (below 0: from the start), its     */ \
  /* place in the story called +23 (rel +24), the entry's item +43; each of the record's weapon slots (+65 bytes   */ \
  /* each, +68 of them) holding the item kept (its count +58 at +59: -2) made had (`or edx, -1`, stored at the     */ \
  /* count +79); the entries +82 bytes each.                                                                       */ \
  SIG(kSigCarriedDue,                                                                                               \
      "8B 7C 24 ?? 0F B7 06 66 85 C0 78 ?? 0F BF C8 53 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 78 ?? 3B F8 7C ?? " \
      "8B 44 24 ?? 0F B7 56 ?? 33 C9 8B FF 0F BF 38 3B FA 75 ?? 66 83 78 ?? ?? 74 ?? 41 83 C0 ?? 83 F9 ?? 72 ?? EB " \
      "?? 83 CA FF 66 89 50 ?? 83 C6 ??",                                                                           \
      1)                                                                                                            \
  /* --- the enemies window (records.cpp) ---                                                                      */ \
  /* The Records screen's ENEMIES page (the frame's page 2: kSigRecordPages' case at +54, rel +57), a row: the     */ \
  /* first row shown +2 (a dword), sInfoMan's global +12 (and +35, +66); the enemy table's record of the row's     */ \
  /* entry called +17 (rel +18: RE6CC_WEAPON_ROW, the WEAPONS page's lookup over this table) and its "always       */ \
  /* named" byte +32; the profile in use +41 (and +72), a profile's size +49 (+75), its cSystemInfo +56 (+82);     */ \
  /* Enemies Defeated called +58 (rel +59), Times Defeated +84 (rel +85; both RE6CC_KILL_GET) - the row is named   */ \
  /* when the byte is set or either count is above 0, else drawn "? ? ?".                                          */ \
  SIG(kSigRecordEnemyRows,                                                                                          \
      "8B BE ?? ?? ?? ?? 03 7C 24 ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 80 78 ?? 00 A1 ?? " \
      "?? ?? ?? 8B 48 ?? 0F 95 44 24 ?? 69 C9 ?? ?? ?? ?? 8D 4C 01 ?? 57 E8 ?? ?? ?? ?? 8B D8 A1 ?? ?? ?? ?? 8B 50 " \
      "?? 69 D2 ?? ?? ?? ?? 8D 4C 02 ?? 57 E8 ?? ?? ?? ?? 85 DB 7F ?? 85 C0 7E ?? C6 44 24 ?? 01",                  \
      1)                                                                                                            \
  /* ... the row under the cursor's "Times Defeated" (mes_system 0x8D0), drawn only while its record's byte +2 is  */ \
  /* 0 (the animals', the Chrysalids' and Oko's rows have it set: they defeat nobody).                             */ \
  SIG(kSigRecordEnemyDefeats,                                                                                       \
      "80 7F ?? 00 75 ?? 6A 01 6A FF 68 E0 00 00 00 8D 4C 24 ?? 51 8D 54 24 ?? 52 68 ?? ?? 00 00 68 ?? ?? 00 00 68 " \
      "D0 08 00 00",                                                                                                \
      1)                                                                                                            \
  /* --- the Mercenaries' unlocks window (records.cpp) ---                                                        */ \
  /* The stages open (sInfoMan's, thiscall, a dword of bits - a stage the stage select's row: the Mercenaries'    */ \
  /* first, No Mercy's from a later bit), its head: the profile in use +2, a profile's size +5, the stages' bits  */ \
  /* (sInfoMan's) +12; then a chain the mod walks to its `ret`: `mov edx, ecx; shr edx, n; test dl, 1; je` and   */ \
  /* the bit set (`mov eax, imm`, `or eax, imm`) - the profile's own bit n - with the bits always open `mov eax,  */ \
  /* imm` (+21) first and `or eax, imm` between the tests.                                                       */ \
  SIG(kSigMercStageGetter,                                                                                          \
      "8B 51 ?? 69 D2 ?? ?? ?? ?? 8B 8C 0A ?? ?? ?? ?? 8B D1 C1 EA ?? B8 ?? ?? ?? ?? F6 C2 01 74 ?? B8 ?? ?? ?? ?? " \
      "8B D1 C1 EA",                                                                                                \
      1)                                                                                                            \
  /* The characters available (sInfoMan's, thiscall, 64 bits - a bit a character's costume): the profile in use */ \
  /* +5, a profile's size +8, the bits +16 (low) and +24 (high, sInfoMan's); always available +32 (or'ed in) and */ \
  /* +144 (`or ecx`); the loop over the bits +128 (its end); then sGameMan (its global +132): its mode +150 equal */ \
  /* to +154 (No Mercy) and its flag +161 (a byte) with bit +165 - the extras +170 (low) and +176 (high, `or edx, */ \
  /* imm8`) set, else cleared (+185 and +191 their complements).                                                  */ \
  SIG(kSigMercCharGetter,                                                                                           \
      "83 EC 08 8B 41 ?? 69 C0 ?? ?? ?? ?? 53 8B 9C 08 ?? ?? ?? ?? 55 8B AC 08 ?? ?? ?? ?? 8B C3 56 0D ?? ?? ?? ?? " \
      "57 89 44 24 10 89 6C 24 14 33 FF 90 B8 01 00 00 00 33 D2 8B CF E8 ?? ?? ?? ?? 8B C8 8B F2 23 CB 23 F5 0B CE " \
      "74 08 09 44 24 10 09 54 24 14 8D 4F 01 B8 01 00 00 00 33 D2 E8 ?? ?? ?? ?? 8B C8 8B F2 23 CB 23 F5 0B CE 8B " \
      "4C 24 10 74 0A 0B C8 09 54 24 14 89 4C 24 10 83 C7 02 83 FF ?? 72 AD A1 ?? ?? ?? ?? 8B 54 24 14 5F 5E 81 C9 " \
      "?? ?? ?? ?? 83 B8 ?? ?? ?? ?? ?? 5D 5B 75 ?? F6 80 ?? ?? ?? ?? ?? 74 ?? 81 C9 ?? ?? ?? ?? 83 CA ?? 8B C1 83 " \
      "C4 08 C3 81 E1 ?? ?? ?? ?? 83 E2 ?? 8B C1 83 C4 08 C3",                                                      \
      1)                                                                                                            \
  /* A stage's number (sGameMan's, thiscall (stage), ret 4; below 0 the one chosen, +10; past the last the first): */ \
  /* how many +16 (a global), the table +31 (a word at the start of each 10-byte record: `lea eax, [eax+eax*4]`). */ \
  SIG(kSigMercStageTable,                                                                                           \
      "8B 44 24 04 85 C0 79 06 8B 81 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 72 02 33 C0 8D 04 80 0F B7 04 45 ?? ?? ?? ?? C2 " \
      "04 00",                                                                                                      \
      1)                                                                                                            \
  /* The stage select's frame (uIdStartUp31's slot 8), its head: the stages open read each frame - in No Mercy   */ \
  /* (sGameMan's mode +23 equal to +9) the getter (called +29, rel +30) shifted right +36, else as it is (+39,   */ \
  /* rel +40) - into the screen's dword +52; its rows +55 (a dword) are +56.                                      */ \
  SIG(kSigMercStageFrame,                                                                                           \
      "51 A1 ?? ?? ?? ?? 56 57 BF ?? ?? ?? ?? 8B F1 8B 0D ?? ?? ?? ?? 39 B8 ?? ?? ?? ?? 75 ?? E8 ?? ?? ?? ?? C1 E8 " \
      "?? EB ?? E8 ?? ?? ?? ?? 53 55 6A FF 8B CE 89 46 ?? C7 46 ?? ?? 00 00 00",                                    \
      1)                                                                                                            \
  /* The stage select's side panel (its start's; the Mercenaries'): the characters available (called +0, rel     */ \
  /* +1), the pages (+10, a page's count byte before it; +170 bytes a page, to +183), the panel's ids +16 (+167  */ \
  /* bytes a page), counts +22 (a dword a page) and masks (the counts' +49: an entry's bit set when one of its   */ \
  /* characters is available); the character table's count +83 (a global) and its select ids +94 (+130 bytes a  */ \
  /* record) - its bit the record's place (`shl eax, cl`, the low dword).                                         */ \
  SIG(kSigMercStagePanel,                                                                                           \
      "E8 ?? ?? ?? ?? 89 44 24 20 B8 ?? ?? ?? ?? 8D 8E ?? ?? ?? ?? 8D 9E ?? ?? ?? ?? 89 44 24 1C 89 4C 24 18 89 5C " \
      "24 14 90 0F B6 50 FF 33 FF 89 13 89 7B ?? 3B D7 7E ?? 8B 4C 24 18 89 4C 24 10 EB ?? 8D A4 24 00 00 00 00 0F " \
      "B6 2C 38 8B 54 24 10 89 2A 8B 35 ?? ?? ?? ?? 33 C9 85 F6 74 ?? BA ?? ?? ?? ?? EB ?? 8D 49 00 0F B6 02 3B E8 " \
      "75 ?? 8B 5C 24 14 B8 01 00 00 00 D3 E0 85 44 24 20 75 ?? 41 83 C2 ?? 3B CE 72 ?? EB ?? BA 01 00 00 00 8B CF " \
      "D3 E2 09 53 ?? 83 44 24 10 04 8B 44 24 1C 47 3B 3B 7C ?? 83 44 24 18 ?? 83 C0 ?? 83 C3 04 89 5C 24 14 89 44 " \
      "24 1C 3D ?? ?? ?? ??",                                                                                       \
      1)                                                                                                            \
  /* ... No Mercy's: the characters available +0 (rel +1), the ids +11, the pages +20 (to +205; +192 a page), the */ \
  /* counts +26 and masks (+52), the table's count +84 and select ids +99 (+144 a record) - a 64-bit test.       */ \
  SIG(kSigMercStagePanelNoMercy,                                                                                    \
      "E8 ?? ?? ?? ?? 89 54 24 28 8D 96 ?? ?? ?? ?? 89 44 24 24 B8 ?? ?? ?? ?? 81 C6 ?? ?? ?? ?? 89 44 24 1C 89 54 " \
      "24 18 89 74 24 14 0F B6 48 FF 33 ED 89 0E 89 6E ?? 3B CD 0F 8E ?? ?? ?? ?? 8B 54 24 18 89 54 24 10 8D 49 00 " \
      "0F B6 04 28 8B 4C 24 10 89 01 8B 1D ?? ?? ?? ?? 33 F6 89 44 24 20 85 DB 74 ?? BF ?? ?? ?? ?? 90 0F B6 17 3B " \
      "C2 75 ?? B8 01 00 00 00 33 D2 8B CE E8 ?? ?? ?? ?? 23 44 24 24 23 54 24 28 0B C2 75 ?? 8B 44 24 20 46 83 C7 " \
      "?? 3B F3 72 ?? EB ?? 8B 44 24 14 BA 01 00 00 00 8B CD D3 E2 09 50 ?? 8B 74 24 14 83 44 24 10 04 8B 44 24 1C " \
      "45 3B 2E 7C ?? 83 44 24 18 ?? 83 C0 ?? 83 C6 04 89 74 24 14 89 44 24 1C 3D ?? ?? ?? ??",                    \
      1)                                                                                                            \
  /* The character select's start (uIdStartUp32's slot 5), its lists: the player +8, sGameMan's (global +2) each  */ \
  /* player's character chosen +12; the rows +22 (the characters shown, a dword each; +19 bytes cleared), the    */ \
  /* cursor +34, how many rows +40; each character's costume cursor +53, its costumes +68 (+297: the cursors'),   */ \
  /* their lists +86 (+80 bytes: 4 dwords a character), +113 a dword cleared; the costumes' bits +129 (+323 a     */ \
  /* character, a signed byte each: below 0 none), how many +170 (a byte a character, at most +266 listed), the  */ \
  /* characters +338; the screen's copy of the characters available +214 (low) and +220 (high).                  */ \
  SIG(kSigMercCharFill,                                                                                             \
      "8B 0D ?? ?? ?? ?? 8B 46 ?? 8B 94 81 ?? ?? ?? ?? 33 ED 6A ?? 8D BE ?? ?? ?? ?? 55 57 89 54 24 38 89 AE ?? ?? " \
      "?? ?? 89 AE ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 8D 9E ?? ?? ?? ?? 55 53 E8 ?? ?? ?? ?? 6A ?? 8D 86 ?? ?? ?? ?? " \
      "55 50 E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8E ?? ?? ?? ?? 55 51 E8 ?? ?? ?? ?? 33 C0 BA ?? ?? ?? ?? 83 C4 30 8B " \
      "CB 2B D6 89 AE ?? ?? ?? ?? 89 6C 24 10 89 44 24 14 C7 44 24 1C ?? ?? ?? ?? 89 7C 24 20 89 4C 24 18 89 54 24 " \
      "24 EB ?? 8D A4 24 00 00 00 00 8B 54 24 24 33 DB C7 01 00 00 00 00 33 ED 38 98 ?? ?? ?? ?? 76 ?? 03 D1 89 54 " \
      "24 28 8D 64 24 00 8B 54 24 1C 0F BE 3C 1A 85 FF 78 ?? B8 01 00 00 00 33 D2 8B CF E8 ?? ?? ?? ?? 23 86 ?? ?? " \
      "?? ?? 23 96 ?? ?? ?? ?? 0B C2 74 ?? 8B 44 24 28 03 C5 89 9C 86 ?? ?? ?? ?? 39 7C 24 2C 75 ?? 8B 4C 24 10 8B " \
      "54 24 18 89 8E ?? ?? ?? ?? 89 2A 45 83 FD ?? 7D ?? 8B 44 24 14 0F B6 88 ?? ?? ?? ?? 43 3B D9 7C ?? EB ?? 8B " \
      "44 24 14 8B 4C 24 18 89 69 ?? 85 ED 74 ?? 8B 54 24 20 FF 44 24 10 89 02 83 C2 04 89 54 24 20 83 44 24 1C ?? " \
      "40 83 C1 04 89 44 24 14 89 4C 24 18 83 F8 ?? 0F 82 ?? ?? ?? ?? 8B 54 24 10 6A 00 89 96 ?? ?? ?? ??",         \
      1)                                                                                                            \
  /* The menus' notices of what an online event gave: its costumes - a table of (bit, file) pairs +6 (to the     */ \
  /* tail's end: RE6CC_MERC_EVENTS_TAIL) each a costume's bit and the Collection file that comes with it.         */ \
  SIG(kSigMercCostumeEvents,                                                                                        \
      "8B 40 04 33 D2 BB ?? ?? ?? ?? 89 44 24 2C 89 54 24 1C C7 44 24 10 01 00 00 00 89 5C 24 24 8B 74 24 10 85 74 " \
      "24 2C 0F 84 ?? ?? ?? ??",                                                                                    \
      1)                                                                                                            \
  /* The Mercenaries' match end, an A rank or better (the rank +2 at most): Costume 1 of the character played     */ \
  /* (its bit, sGameMan's +8) - seven calls (rel +32, +42 ... +92; each RE6CC_COSTUME_AWARD8/32), then Carla's   */ \
  /* (+100).                                                                                                      */ \
  SIG(kSigMercCostumeAwards,                                                                                        \
      "83 FB ?? 77 ?? 8B 44 2E ?? 83 F8 ?? 77 ?? 0F B6 90 ?? ?? ?? ?? FF 24 95 ?? ?? ?? ?? 57 8B CE E8 ?? ?? ?? ?? " \
      "EB ?? 57 8B CE E8 ?? ?? ?? ?? EB ?? 57 8B CE E8 ?? ?? ?? ?? EB ?? 57 8B CE E8 ?? ?? ?? ?? EB ?? 57 8B CE E8 " \
      "?? ?? ?? ?? EB ?? 57 8B CE E8 ?? ?? ?? ?? EB ?? 57 8B CE E8 ?? ?? ?? ?? 57 8B CE E8 ?? ?? ?? ??",            \
      1)

// The chapter clock's add (thiscall (character, time), ret 8), matched only where kSigChapterClockLoop calls it (a
// second accumulator has its shape): a character's block +18 (and +60), the clock +27 (stored +69), its most +41 (a
// float's place), the `ret 8` +73.
#define RE6CC_CLOCK_ADD                                                                                              \
  "8B 44 24 04 83 F8 ?? 72 ?? 0F 57 C0 EB ?? 8B D0 69 D2 ?? ?? ?? ?? F3 0F 10 84 0A ?? ?? ?? ?? F3 0F 58 44 24 08 " \
  "F3 0F 10 0D ?? ?? ?? ?? 0F 2F C1 76 ?? 0F 28 C1 83 F8 ?? 73 ?? 69 C0 ?? ?? ?? ?? F3 0F 11 84 08 ?? ?? ?? ?? C2 " \
  "08 00"

// uCoord's registration of its fields (its vtable slot 3: MT Framework's createProperty), its first: the position,
// mPos (+27: its place in a unit, +34: its name's place).
#define RE6CC_COORD_PROPS "55 8B EC 83 E4 F8 83 EC 2C 53 56 57 8B 7D 08 57 8B D9 E8 ?? ?? ?? ?? 33 F6 8D 43 ?? 8B CF C7 44 24 10 ?? ?? ?? ??"
// --- the Mercenaries' flow's functions, matched only where its signatures call them (merc.cpp) ---------------------
// EnemyAdd's stock builder (thiscall (stock, list, param), ret 0xC): the param's mStockType +32 (2: by groups, +40),
// then each group of its mStockGroup (+60, a bit a group) whose set entries are stocked.
#define RE6CC_STOCK_BUILD                                                                                            \
  "53 8B 5C 24 08 55 56 57 8B CB E8 ?? ?? ?? ?? 8B 6C 24 18 8B CD E8 ?? ?? ?? ?? 8B 54 24 1C 8B 42 ?? 83 E8 00 74 ?? " \
  "83 E8 02 0F 85 ?? ?? ?? ?? 33 DB B8 01 00 00 00 8B CB D3 E0 85 42 ??"
// An area task's player test (fastcall (task)): none without its shape, then the players inside it against its mask.
#define RE6CC_AREA_INSIDE "83 79 08 00 75 03 32 C0 C3 8B 41 04 8B 50 0C"

// sInfoMan's set-skill-points (thiscall (points), ret 4), matched only where kSigSkillBuy and
// kSigSkillLevel call it (sInfoMan has some forty setters of this shape, one a field): the
// profile in use +30, a profile's size +37, the points +44 (cSystemInfo's mSkillPoint, counted
// from the start of sInfoMan).
#define RE6CC_SKILL_POINTS_SET                                                                                       \
  "56 8B F1 80 7E 1C 00 75 ?? 80 3D ?? ?? ?? ?? 00 74 ?? 8D 46 04 50 FF 15 ?? ?? ?? ?? 8B 4E ?? 8B 54 24 08 69 C9 " \
  "?? ?? ?? ?? 89 94 31 ?? ?? ?? ??"

// --- the records windows' functions, matched only where the signatures above call them (records.cpp) ---------------
// cSystemInfo's chapter record get (thiscall (character, difficulty, chapter), ret 0xC): the characters +7, the
// difficulties +16, the chapters +25; character +30 read as +34 (the Agent as Ada); a record's index base +49 (a
// record 12 bytes: its index (character * difficulties + difficulty) * chapters + chapter).
#define RE6CC_CLEAR_GET                                                                                              \
  "8B 54 24 04 56 83 FA ?? 73 ?? 8B 44 24 0C 83 F8 ?? 73 ?? 8B 74 24 10 83 FE ?? 73 ?? 83 FA ?? 75 05 BA ?? ?? ?? " \
  "?? 8D 04 90 03 C2 8D 14 86 8D 84 10 ?? ?? ?? ?? 8D 04 40 8B 04 81 5E C2 0C 00"

// ... and its rank get (the same shape): the rank +54 (a record's own field).
#define RE6CC_CLEAR_RANK                                                                                             \
  "8B 54 24 04 56 83 FA ?? 73 ?? 8B 44 24 0C 83 F8 ?? 73 ?? 8B 74 24 10 83 FE ?? 73 ?? 83 FA ?? 75 05 BA ?? ?? ?? " \
  "?? 8D 04 90 03 C2 8D 14 86 03 D0 8D 04 52 8B 84 81 ?? ?? ?? ?? 5E C2 0C 00"

// The Records screen's PROFILES page (thiscall): its view +10 (a dword), the last view +16, the jump table +22; the
// views' tail jumps from +26, 8 bytes apart (rel +30 + 8 * view).
#define RE6CC_RECORD_PROFILES                                                                                        \
  "56 8B F1 E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 83 F8 04 77 ?? FF 24 85 ?? ?? ?? ?? 8B CE 5E E9 ?? ?? ?? ?? 8B CE 5E " \
  "E9 ?? ?? ?? ?? 8B CE 5E E9 ?? ?? ?? ?? 8B CE 5E E9 ?? ?? ?? ?? 8B CE 5E E9 ?? ?? ?? ??"

// The Records chapter grid's draw (the PROFILES view that shows it): its first cell's number in the draw's count
// (+4, RE6CC_RECORD_GRID_FIRST) and a cell's look read from the screen's bytes less it (+4, RE6CC_RECORD_GRID_READ).
#define RE6CC_RECORD_GRID_FIRST "C7 44 24 18 ?? 00 00 00 8D 5F ?? 53 8D 47 ?? 50 8B CE E8"
#define RE6CC_RECORD_GRID_READ "0F B6 94 3E ?? ?? ?? ??"

// cSystemInfo's medal get, the profile's medal category get (thiscall (category, medal), ret 8): the last category
// +8, the jump table +18. Each case: a category of its own (the profile's thiscall (medal) get, rel +23 from the
// case's start: RE6CC_MEDAL_CASE) or one of two sharing a get (rel +22 from its start, the list +16: RE6CC_MEDAL_CASE2).
#define RE6CC_MEDAL_SWITCH "8B 54 24 04 33 C0 83 FA ?? 0F 87 ?? ?? ?? ?? FF 24 95 ?? ?? ?? ??"
#define RE6CC_MEDAL_CASE "8B 41 ?? 8B 54 24 08 69 C0 ?? ?? ?? ?? 8D 4C 08 ?? 52 E8 ?? ?? ?? ?? C2 08 00"
#define RE6CC_MEDAL_CASE2 "8B 41 ?? 69 C0 ?? ?? ?? ?? 8D 4C 08 ?? C7 44 24 04 ?? 00 00 00 E9 ?? ?? ?? ??"
// A category's own get (thiscall (medal), ret 4): its medals +5 (a dword) or +6 (a byte: RE6CC_MEDAL_GET8); the
// unused table +18 / +16 (2: not a medal), the counts +29 / +27 (a byte each, from cSystemInfo's start).
#define RE6CC_MEDAL_GET32 "8B 44 24 04 3D ?? ?? ?? ?? 72 05 33 C0 C2 04 00 80 B8 ?? ?? ?? ?? 02 74 F2 0F B6 84 08 ?? ?? ?? ?? C2 04 00"
#define RE6CC_MEDAL_GET8 "8B 44 24 04 83 F8 ?? 72 05 33 C0 C2 04 00 80 B8 ?? ?? ?? ?? 02 74 F2 0F B6 84 08 ?? ?? ?? ?? C2 04 00"
// The get two categories share (thiscall (list, medal), ret 8): the lists +6, the medals +15, the first list's
// unused table +24, the second's +38, a list's size +52 (a byte), the counts +59.
#define RE6CC_MEDAL_GET2                                                                                             \
  "8B 44 24 04 83 F8 ?? 73 16 8B 54 24 08 83 FA ?? 73 0D 85 C0 75 0E 80 BA ?? ?? ?? ?? 02 75 13 33 C0 C2 08 00 80 " \
  "BA ?? ?? ?? ?? 02 75 05 33 C0 C2 08 00 6B C0 ?? 03 C2 0F B6 84 08 ?? ?? ?? ?? C2 08 00"

// The medal page's name (thiscall (category, medal), ret 8): the categories from 1 +5 (cases: the last less one),
// the jump table +13; each case `mov eax, base; add eax, [esp+8]; ret 8` (12 bytes apart from +17); the default
// (category 0) at +101's base (+102).
#define RE6CC_MEDAL_NAMES "8B 44 24 04 48 83 F8 ?? 77 ?? FF 24 85 ?? ?? ?? ??"

// cSystemInfo's byte gets (thiscall (index), ret 4): the dog tag templates and the emblems - how many +6, the
// bytes +18 (from cSystemInfo's start); the title states - how many +5 (a dword), the bytes +20.
#define RE6CC_BYTE_GET "8B 44 24 04 83 F8 ?? 72 05 33 C0 C2 04 00 0F B6 84 08 ?? ?? ?? ?? C2 04 00"
#define RE6CC_BYTE_GET32 "8B 44 24 04 3D ?? ?? ?? ?? 72 05 33 C0 C2 04 00 0F B6 84 08 ?? ?? ?? ?? C2 04 00"
// The title counters (counted from 1): how many +6, the counters +20 (a dword each; RE6CC_COUNTER_GETF the same
// read as floats).
#define RE6CC_COUNTER_GET "8B 44 24 04 48 3D ?? ?? ?? ?? 72 05 33 C0 C2 04 00 8B 84 81 ?? ?? ?? ?? C2 04 00"
#define RE6CC_COUNTER_GETF "8B 44 24 04 48 3D ?? ?? ?? ?? 72 05 D9 EE C2 04 00 D9 84 81 ?? ?? ?? ?? C2 04 00"
// The title table's record (thiscall (title), ret 4): how many (a global, +6), the table +20 (12 bytes a title).
#define RE6CC_TITLE_DEF "8B 44 24 04 3B 05 ?? ?? ?? ?? 72 02 33 C0 8D 04 40 8D 04 85 ?? ?? ?? ?? C2 04 00"
// ... and its find (counter, level): the same count +2, the table's levels +24 (a title's +4; its counter the
// word before).
#define RE6CC_TITLE_FIND                                                                                             \
  "8B 15 ?? ?? ?? ?? 53 56 33 C0 57 85 D2 74 ?? 8B 74 24 14 8B 7C 24 10 B9 ?? ?? ?? ?? 8D 64 24 00 0F B7 59 FE 3B " \
  "FB 75 07 0F B6 19 3B F3 74 ?? 40 83 C1 0C 3B C2 72 E9"
// sInfoMan's set of a "new" file (thiscall (file), ret 4), its call of cSystemInfo's (thiscall (file)): how many +6,
// the bytes +13 (and +24).
#define RE6CC_FILE_SET "8B 44 24 04 83 F8 ?? 73 ?? 0F B6 94 08 ?? ?? ?? ?? 85 D2 75 ?? C6 84 08 ?? ?? ?? ?? 01 C2 04 00"

// --- the skills window's functions, matched only where kSigSkillListFill and the functions below call them ---------
// The skill table's count (`mov eax, [count]; ret`: the global +1) and a record (thiscall (i), ret 4; i past the
// count reads record 0): the count +6, the table +20, a record 20 bytes (`lea eax, [eax+eax*4]`, then `* 4`).
#define RE6CC_SKILL_COUNT "A1 ?? ?? ?? ?? C3"
#define RE6CC_SKILL_RECORD "8B 44 24 04 3B 05 ?? ?? ?? ?? 72 02 33 C0 8D 04 80 8D 04 85 ?? ?? ?? ?? C2 04 00"
// sInfoMan's "can be bought" test of a skill record (thiscall (record), ret 4) - what the list's rows hold: the
// record called +15 (rel +16) on sGameMan; any state but 0 says yes (the profile in use +0x16, its size +0x19, its
// cSystemInfo +0x26; the record's skill +0x22; the state get rel +0x29). Else the requirement (+0x3B, a signed byte)
// less one, through a jump table (+0x48; the last case +0x42, past it yes): 1 the skill before it (+0x52) bought
// (+0x66; the get rel +0x60); 2 the kills of an enemy kind (the record's word +0x76; the count rel +0x7B) at least
// its goal (the word +0x82); 3 the kills with a kind of weapon (+0x8A; rel +0x8F) at least its goal (+0x96); 4 the
// campaigns complete (each of +0xB2 stories, the story test rel +0xA6 to its chapter +0xA1) at least +0xB7.
#define RE6CC_SKILL_AVAILABLE                                                                                        \
  "8B 44 24 04 56 57 8B F9 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 4F ?? 69 C9 ?? ?? ?? ?? 8B F0 0F B6 46 ?? 8D 4C " \
  "39 ?? 50 E8 ?? ?? ?? ?? 85 C0 74 ?? 5F B0 01 5E C2 04 00 0F BE 46 ?? 53 48 32 DB 83 F8 ?? 77 ?? FF 24 85 ?? ?? " \
  "?? ?? 8B 57 ?? 0F B6 46 ?? 69 D2 ?? ?? ?? ?? 48 8D 4C 3A ?? 50 E8 ?? ?? ?? ?? 83 F8 ?? 75 ?? B3 01 8A C3 5B 5F " \
  "5E C2 04 00 0F B7 46 ?? 50 8B CF E8 ?? ?? ?? ?? 0F B7 4E ?? 3B C1 EB ?? 0F B7 56 ?? 52 8B CF E8 ?? ?? ?? ?? 0F " \
  "B7 4E ?? 3B C1 EB ?? 55 33 ED 33 F6 6A ?? 56 8B CF E8 ?? ?? ?? ?? 84 C0 74 ?? 45 46 83 FE ?? 7C ?? 83 FD ?? 5D " \
  "7C ?? B3 01 8A C3 5B 5F 5E C2 04 00"
// The kills of an enemy kind (sInfoMan's, thiscall (kind), ret 4): two kinds counted with others - the kind +0x09
// (its kinds +0x135 and +0x160), the kind +0x14 (+0x48, +0x73, +0x9E, +0xCB, +0xF8, +0x123; the last also +0x12E) -
// any other alone (`push kind; push 3; push 0`: the kill table's find with its two keys +0x1C and +0x1E, called
// rel +0x20); each found one's count (rel +0x3C, a jmp; the gets rel +0x68 +0x95 +0xC2 +0xEF +0x11C +0x155 +0x182,
// the finds +0x51 +0x7E +0xAB +0xD8 +0x105 +0x13E +0x16B).
#define RE6CC_SKILL_KILLS                                                                                            \
  "8B 44 24 04 56 57 8B F1 3D ?? ?? ?? ?? 0F 84 21 01 00 00 3D ?? ?? ?? ?? 74 2D 50 6A 03 6A 00 E8 ?? ?? ?? ?? 85 " \
  "C0 78 18 8B 4E 20 69 C9 D8 FD 00 00 5F 8D 4C 31 38 5E 89 44 24 04 E9 ?? ?? ?? ?? 5F 33 C0 5E C2 04 00 68 ?? ?? " \
  "?? ?? 6A 03 6A 00 E8 ?? ?? ?? ?? 85 C0 78 17 8B 56 20 69 D2 D8 FD 00 00 8D 4C 32 38 50 E8 ?? ?? ?? ?? 8B F8 EB " \
  "02 33 FF 68 ?? ?? ?? ?? 6A 03 6A 00 8B CE E8 ?? ?? ?? ?? 85 C0 78 15 8B 4E 20 69 C9 D8 FD 00 00 8D 4C 31 38 50 " \
  "E8 ?? ?? ?? ?? EB 02 33 C0 68 ?? ?? ?? ?? 6A 03 6A 00 8B CE 03 F8 E8 ?? ?? ?? ?? 85 C0 78 15 8B 56 20 69 D2 D8 " \
  "FD 00 00 8D 4C 32 38 50 E8 ?? ?? ?? ?? EB 02 33 C0 68 ?? ?? ?? ?? 6A 03 6A 00 8B CE 03 F8 E8 ?? ?? ?? ?? 85 C0 " \
  "78 15 8B 4E 20 69 C9 D8 FD 00 00 8D 4C 31 38 50 E8 ?? ?? ?? ?? EB 02 33 C0 68 ?? ?? ?? ?? 6A 03 6A 00 8B CE 03 " \
  "F8 E8 ?? ?? ?? ?? 85 C0 78 1C 8B 56 20 69 D2 D8 FD 00 00 8D 4C 32 38 50 E8 ?? ?? ?? ?? 03 F8 68 ?? ?? ?? ?? EB " \
  "3B 33 C0 03 F8 68 ?? ?? ?? ?? EB 30 68 ?? ?? ?? ?? 6A 03 6A 00 E8 ?? ?? ?? ?? 85 C0 78 17 8B 56 20 69 D2 D8 FD " \
  "00 00 8D 4C 32 38 50 E8 ?? ?? ?? ?? 8B F8 EB 02 33 FF 68 ?? ?? ?? ?? 6A 03 6A 00 8B CE E8 ?? ?? ?? ?? 85 C0 78 " \
  "1A 8B 4E 20 69 C9 D8 FD 00 00 8D 4C 31 38 50 E8 ?? ?? ?? ?? 03 C7 5F 5E C2 04 00 33 C0 8B C7 5F 5E C2 04 00"
// The kill table's find (key1, key2, kind) -> its index (a signed byte), or -1: how many (a global, +2); the first
// record's key2 +0x1D (records 8 bytes apart, +0x3B), key1 at it +0x24, the kind (a word) +0x33, the index +0x4D.
#define RE6CC_KILL_FIND                                                                                              \
  "8B 15 ?? ?? ?? ?? 53 55 56 33 C9 57 85 D2 7E ?? 8B 74 24 1C 8B 7C 24 18 8B 5C 24 14 B8 ?? ?? ?? ?? 0F B6 68 ?? " \
  "3B DD 75 ?? 0F B6 28 3B FD 75 ?? 0F B7 68 ?? 3B F5 74 ?? 41 83 C0 ?? 3B CA 7C ?? 5F 5E 5D 83 C8 FF 5B C2 0C 00 " \
  "0F BE 40 ??"
// cSystemInfo's kill count of an enemy entry (thiscall (index), ret 4; past the last, entry 0): how many +6, the
// counts +14 (8 bytes apart). The ENEMIES page's Times Defeated (the dword after each count) has its shape too.
#define RE6CC_KILL_GET "8B 44 24 04 83 F8 ?? 72 02 33 C0 8B 84 C1 ?? ?? ?? ?? C2 04 00"
// The kills with a kind of weapon (sInfoMan's, thiscall (weapon), ret 4): the kinds from 0x101 (its negation +8) to
// it + the last +0x10 (past them, the default rel +0x13) through a byte table (+0x1A) into a jump table (+0x21); a
// case sums the kills with each of its weapons (RE6CC_WEAPON_FIND), the default those with the weapon asked alone
// (RE6CC_WEAPON_DEFAULT, inline).
#define RE6CC_SKILL_WEAPON_KILLS "56 57 8B 7C 24 0C 8D 87 ?? ?? ?? ?? 8B F1 83 F8 ?? 0F 87 ?? ?? ?? ?? 0F B6 80 ?? ?? ?? ?? FF 24 85 ?? ?? ?? ??"
// The kills with one weapon (sInfoMan's, thiscall (weapon), ret 4): how many weapon records (a global +4), the table
// +0x14 (records +0x23 bytes apart, the weapon a word at +0x1B, the record's first byte the index, signed), its count
// (a jmp, rel +0x48).
#define RE6CC_WEAPON_FIND                                                                                            \
  "53 56 8B 35 ?? ?? ?? ?? 33 C0 57 85 F6 7E ?? 8B 7C 24 10 BA ?? ?? ?? ?? 0F B7 5A ?? 3B FB 74 ?? 40 83 C2 ?? 3B " \
  "C6 7C ?? 5F 5E 33 C0 5B C2 04 00 0F BE 02 8B 51 20 69 D2 D8 FD 00 00 5F 5E 8D 4C 0A 38 5B 89 44 24 04 E9 ?? ?? " \
  "?? ??"
// ... the same inline, the weapon kills' default: the count +2, the table +0xE, the weapon +0x15, the size +0x1D, the
// count's jmp rel +0x42.
#define RE6CC_WEAPON_DEFAULT                                                                                         \
  "8B 15 ?? ?? ?? ?? 33 C0 53 85 D2 7E ?? B9 ?? ?? ?? ?? 0F B7 59 ?? 3B FB 74 ?? 40 83 C1 ?? 3B C2 7C ?? 5B 5F 33 " \
  "C0 5E C2 04 00 0F BE 01 8B 4E 20 69 C9 D8 FD 00 00 5B 5F 8D 4C 31 38 5E 89 44 24 04 E9 ?? ?? ?? ??"
// cSystemInfo's kills with a weapon entry (thiscall (index), ret 4; past the last, entry 0): how many +6, a record's
// size +13, the kills +17 (the record's first dword).
#define RE6CC_WEAPON_GET "8B 44 24 04 83 F8 ?? 72 02 33 C0 6B C0 ?? 8B 84 08 ?? ?? ?? ?? C2 04 00"
// sInfoMan's push of the profile in use (thiscall (user), ret 4; a user past 15 is no push; an unknown one is the
// main pad's): the one in use kept (+0x46) and the user made it (+0x50).
#define RE6CC_PROFILE_PUSH                                                                                           \
  "56 57 8B 7C 24 0C 8B F1 83 FF ?? 77 ?? A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53 8B 98 ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? " \
  "84 C0 74 ?? 8B 0D ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 84 C0 75 02 8B FB 8B 46 ?? 5B 83 F8 ?? 7D ?? 8B 4E ?? 89 4C 86 " \
  "?? FF 46 ?? 89 7E ?? 5F 5E C2 04 00"
// --- the weapons window's functions, matched only where its signatures call them (records.cpp) ---------------------
// The WEAPONS page's named row, its name: the message its entry + the base +8 (`lea edx, [edi + base]`, from
// mes_file) - searched in the page's own draw (the ENEMIES page draws its names with the same code).
#define RE6CC_WEAPON_NAME "51 68 ?? 00 00 00 8D 97 ?? ?? ?? ?? 52 8B CE E8 ?? ?? ?? ?? 3B BE ?? ?? ?? ??"
// The WEAPONS page's record of a row (sInfoMan's, thiscall (entry), ret 4; none: 0): how many (a global, +2), the
// table +19, a record's size +33 (its entry the record's first byte, signed: `movsx edi, byte [eax]`). The ENEMIES
// page's, over the enemy table, has its shape too.
#define RE6CC_WEAPON_ROW                                                                                             \
  "8B 15 ?? ?? ?? ?? 56 33 C9 57 85 D2 7E ?? 8B 74 24 0C B8 ?? ?? ?? ?? 0F BE 38 3B F7 74 ?? 41 83 C0 ?? 3B CA 7C " \
  "?? 33 C0 5F 5E C2 04 00"
// cGameInfo's carried record of a character (thiscall (character), ret 4; past the last, character 0): how many +6,
// a record's size +13, the records +17 (`lea`: the record's place).
#define RE6CC_CARRIED_GET "8B 44 24 04 83 F8 ?? 72 02 33 C0 6B C0 ?? 8D 84 08 ?? ?? ?? ?? C2 04 00"
// A table of the game's data by character (sGameMan's, thiscall (character), ret 4; past the last, the first's):
// how many +6, the table +14 (a dword a character: a record's place, or a count).
#define RE6CC_DATA_GET "8B 44 24 04 83 F8 ?? 72 02 33 C0 8B 04 85 ?? ?? ?? ?? C2 04 00"
// --- the Mercenaries' unlocks window's functions, matched only where the signatures above call them -----------------
// The online event list's costumes, the tail of their loop (searched after kSigMercCostumeEvents): the entry's file
// +0 (`mov ebp, [ebx+4]`) given through cSystemInfo's file set (called +38, rel +39: RE6CC_FILE_SET), an entry +82
// bytes, the table's end +95.
#define RE6CC_MERC_EVENTS_TAIL                                                                                       \
  "8B 6B 04 75 ?? 80 3D ?? ?? ?? ?? 00 74 ?? 8D 56 04 52 FF 15 ?? ?? ?? ?? 8B 46 ?? 69 C0 ?? ?? ?? ?? 55 8D 4C 30 " \
  "?? E8 ?? ?? ?? ?? 80 7E 1C 00 75 ?? 80 3D ?? ?? ?? ?? 00 74 ?? 83 C6 04 56 FF 15 ?? ?? ?? ?? 8B 6C 24 14 8B 54 " \
  "24 20 8B 4C 24 10 83 C3 ?? D1 C1 89 5C 24 24 89 4C 24 10 81 FB ?? ?? ?? ?? 0F 8C"
// A costume given at a match's end (sGameMan's, thiscall (player), ret 4): sInfoMan's global +5, the profile in use
// +11, a profile's size +14, the characters' bits (sInfoMan's) +21 tested with its own bit (+27 a byte, or +26 a
// dword: RE6CC_COSTUME_AWARD32), that bit's number pushed +36 (+38) for the set (called +37, rel +38; +39, rel +40);
// then its Collection file (RE6CC_COSTUME_FILE, searched after it: the file +1, cSystemInfo's file set called +6, rel
// +7).
#define RE6CC_COSTUME_AWARD8 \
  "56 8B F1 8B 0D ?? ?? ?? ?? 8B 41 ?? 69 C0 ?? ?? ?? ?? 8B 84 08 ?? ?? ?? ?? 83 E0 ?? 33 D2 0B C2 57 75 ?? 6A ?? E8"
#define RE6CC_COSTUME_AWARD32 \
  "56 8B F1 8B 0D ?? ?? ?? ?? 8B 41 ?? 69 C0 ?? ?? ?? ?? 8B 84 08 ?? ?? ?? ?? 25 ?? ?? ?? ?? 33 D2 0B C2 57 75 ?? 6A ?? E8"
#define RE6CC_COSTUME_FILE "6A ?? 8D 4C 38 ?? E8 ?? ?? ?? ??"
// A uIdCBase screen's frame dispatching on its step (searched in the skill screen's frame): the step +2 (a byte of
// it), the last step +10, the byte table +20 into the jump table +27. The closing step's case starts with the
// menus' profile save (`mov ecx, esi; call` - kSigProfileSave).
#define RE6CC_SCREEN_STEPS "8B 46 ?? 25 FF 00 00 00 83 F8 ?? 0F 87 ?? ?? ?? ?? 0F B6 90 ?? ?? ?? ?? FF 24 95 ?? ?? ?? ??"
