#pragma once

// The cheat switches: X(enum name, ini key, the ini's comment). The panel flips
// them, the tick applies them (cheats.cpp), and every change is written back to
// RE6CabbyCodes.ini (config.cpp), so they come back on the next start. A key
// that is no longer one goes in sw::kRetired: an ini that still has it is
// rewritten without it (config.cpp).
#define RE6CC_SWITCHES(X)                                                                                        \
  X(kGodMode, "GodMode", "you take no damage, and your health is held at its maximum")                           \
  X(kPartnerGod, "PartnerGodMode", "your AI partner takes no damage either")                                      \
  X(kInfiniteStamina, "InfiniteStamina",                                                                          \
    "your combat gauge never runs down: melee moves and quick shots use none of it (under water it is your air: " \
    "InfiniteAir's)")                                                                                             \
  X(kInfiniteAir, "InfiniteAir", "under water your air never runs out: its gauge stays full, so you never drown") \
  X(kOneHitKills, "OneHitKills",                                                                                  \
    "any hit of yours that does damage kills an enemy, through the game's own death")                             \
  X(kInfiniteAmmo, "InfiniteAmmo",                                                                                \
    "the game's own infinite ammo for every weapon: no rounds used, no reloads (health tablets still used up); "  \
    "the helicopter Ada flies keeps its missiles")                                                                \
  X(kSteadyAim, "SteadyAim", "your aim holds still: no sway while you aim, no kick when you fire")               \
  X(kAutoQte, "AutoQTE",                                                                                          \
    "the quick time events play themselves: their prompts answered (a press in time at its best moment), and "    \
    "the scenes the game makes automatic for its own option take that path")                                      \
  X(kInfiniteSkillPoints, "InfiniteSkillPoints", "buying a skill (or its next level) takes no skill points")      \
  X(kFreezeCountdown, "FreezeCountdown",                                                                          \
    "the countdown on screen stands still: a stage's, and the Mercenaries' clock (its time bonuses still add)")   \
  X(kFreezePlaytime, "FreezePlaytime", "the chapter's clock stands still (its rank goes by it)")                  \
  X(kNoDeathCount, "NoDeathCount", "the results: a death is not counted")                                         \
  X(kNoMissCount, "NoMissCount", "the results: a shot that misses is not counted (the accuracy stays at 100%)")   \
  X(kEndlessCombo, "EndlessCombo",                                                                                \
    "the Mercenaries: a combo never runs out, and its bonus goes onto your score as it is earned")                \
  X(kComboTime, "ComboTimeAlwaysOn",                                                                              \
    "the Mercenaries: Combo Time runs from the moment it is switched on and never runs out")                       \
  X(kAutoKill, "KillEnemiesAsTheyAppear",                                                                         \
    "the Mercenaries: every enemy dies as it comes, the kill scored as yours; it lasts one match")

namespace re6cc::sw {

enum Kind {
#define RE6CC_ENUM(name, key, help) name,
  RE6CC_SWITCHES(RE6CC_ENUM)
#undef RE6CC_ENUM
      kCount
};

// Switches the mod no longer has (none yet; "" matches no key).
constexpr const char* kRetired[] = {""};

}  // namespace re6cc::sw
