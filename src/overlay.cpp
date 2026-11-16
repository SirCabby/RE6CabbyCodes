#include "overlay.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#include "adopt.h"
#include "boot.h"
#include "cheats.h"
#include "config.h"
#include "dispatch.h"
#include "game.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "inventory.h"
#include "items.h"
#include "memlog.h"
#include "backends/imgui_impl_win32.h"
#include "log.h"
#include "merc.h"
#include "records.h"
#include "records_text.h"
#include "version.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace re6cc::overlay {
namespace {

HWND g_hwnd = nullptr;
WNDPROC g_orig_wndproc = nullptr;
bool g_context_ready = false;
bool g_visible = false;
bool g_user_hidden = false;  // the toggle key, while the pause is up
bool g_user_shown = false;   // the toggle key, in play: the cheats over the game
bool g_records_hidden = false;  // the toggle key, beside a records screen (until that screen closes)
records::Screen g_records_was = records::Screen::kNone;

// The panel is drawn half as big again as ImGui's own sizes: the game draws at
// the display's resolution, where ImGui's 13-pixel font is small. Every size below
// that is written in pixels is at ImGui's own scale and goes through scaled().
constexpr int kScalePercent = 150;
constexpr float kScale = kScalePercent / 100.0f;
float scaled(float px) { return px * kScale; }

// Kept in the ImGui layout file ([RE6CabbyCodes][Panel]) beside the windows'
// places and sizes: which sections are folded away.
bool g_cheats_open = true;
bool g_case_open[game::kMaxChars] = {true, true, false, false, false, false, false, false};

const char* const kPanelWindow = "Resident Evil 6 - Cabby Codes  v" RE6CC_VERSION "###RE6CC panel";

CRITICAL_SECTION g_imgui_cs;
bool g_imgui_cs_ready = false;

// What the panel is taking this frame, published for the input guard.
volatile LONG g_capture_mouse = 0;
volatile LONG g_capture_keyboard = 0;
volatile LONG g_visible_flag = 0;

const ImVec4 kOrange(1.0f, 0.6f, 0.3f, 1.0f);
const ImVec4 kRed(1.0f, 0.35f, 0.35f, 1.0f);

LRESULT CALLBACK hk_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  const dispatch::Snapshot s = dispatch::snapshot();
  // F-keys above F9 arrive as WM_SYSKEYDOWN; accept both, ignore auto-repeat.
  if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30)) &&
      static_cast<int>(wp) == config::get().toggle_key) {
    ImGuiLock guard;  // wants_draw reads and clears these
    if (s.show_panel || config::get().always_show) {
      g_user_hidden = !g_user_hidden;
      logf("panel %s by the toggle key", g_user_hidden ? "hidden" : "shown");
      return 0;
    }
    if (s.records != records::Screen::kNone) {
      g_records_hidden = !g_records_hidden;
      logf("records window %s by the toggle key", g_records_hidden ? "hidden" : "shown");
      return 0;
    }
    // In play the key opens the cheats over the game; out of a game (the title,
    // the menus) it does nothing - unless the mod cannot see the game, when the
    // panel says why.
    if (s.run.screen == game::Screen::kPlay || !s.ready) {
      g_user_shown = !g_user_shown;
      logf("panel %s over the game by the toggle key", g_user_shown ? "opened" : "closed");
      return 0;
    }
  }
  // ImGui sees every message, visible or not: a button release that arrives
  // after the panel is hidden would otherwise never be delivered, leaving it
  // convinced the button is still held (and the window stuck to the pointer).
  if (g_context_ready) {
    bool swallow = false;
    {
      ImGuiLock guard;
      if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
      if (g_visible) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) swallow = true;
        if (io.WantTextInput && msg >= WM_KEYFIRST && msg <= WM_KEYLAST && wp != VK_ESCAPE) swallow = true;
      }
    }
    if (swallow) return 0;
  }
  // The game's own window procedure runs outside the lock.
  return CallWindowProcW(g_orig_wndproc, hwnd, msg, wp, lp);
}

// Keep the current window inside the display: moved back whenever any part of
// it has ended up outside (a saved position from a bigger window, a panel that
// grew).
void keep_in_reach(const ImVec2& display) {
  const ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
  ImVec2 want = pos;
  if (want.x + size.x > display.x) want.x = display.x - size.x;
  if (want.y + size.y > display.y) want.y = display.y - size.y;
  if (want.x < 0) want.x = 0;
  if (want.y < 0) want.y = 0;
  if (want.x != pos.x || want.y != pos.y) ImGui::SetWindowPos(want);
}

// The help for the item just drawn, on hover (a disabled one too).
void item_help(const char* help) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip()) {
    ImGui::PushTextWrapPos(scaled(380.0f));
    ImGui::TextUnformatted(help);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

// A section heading that folds, remembered in the layout file.
bool section(const char* label, bool* open) {
  ImGui::SetNextItemOpen(*open, ImGuiCond_Always);
  const bool now = ImGui::CollapsingHeader(label);
  if (now != *open) {
    *open = now;
    ImGui::MarkIniSettingsDirty();
  }
  return now;
}

// A section within a section (a list's Add): its heading and what it holds set
// in under the parent's, folding on its own.
template <typename Draw>
void subsection(const char* label, bool* open, Draw&& draw) {
  const float indent = ImGui::GetStyle().IndentSpacing;
  ImGui::Spacing();
  ImGui::Indent(indent);
  if (section(label, open)) draw();
  ImGui::Unindent(indent);
}

void cheat_row(sw::Kind k, const char* label, const char* help) {
  bool on = cheats::enabled(k);
  const bool can = cheats::available(k);
  ImGui::BeginDisabled(!can);
  if (ImGui::Checkbox(label, &on)) cheats::set(k, on);
  ImGui::EndDisabled();
  item_help(can ? help : cheats::unavailable_reason(k));
}

// Edits the panel is typing: kept until Set, then let go so the field follows the game again.
struct Edit {
  int v = 0;
  bool typing = false;
};

// "<label> [   n] Set": Set sends the typed value; until then the field follows the game.
bool int_editor(const char* label, const char* id, Edit& e, int live, int lo, int hi, float width, bool enabled) {
  if (!e.typing) e.v = live;
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine();
  ImGui::BeginDisabled(!enabled);
  ImGui::SetNextItemWidth(scaled(width));
  char field[48];
  std::snprintf(field, sizeof(field), "##%s", id);
  if (ImGui::InputInt(field, &e.v, 0, 0)) e.typing = true;
  if (e.v < lo) e.v = lo;
  if (e.v > hi) e.v = hi;
  ImGui::SameLine();
  char button[48];
  std::snprintf(button, sizeof(button), "Set##%s", id);
  const bool set = ImGui::SmallButton(button);
  ImGui::EndDisabled();
  if (set) e.typing = false;
  return set;
}

Edit g_points;
Edit g_clock;         // the chapter's clock (seconds)
Edit g_counter[2][4];  // the chapter's counters of the characters you play: deaths, kills, shots, hits
Edit g_countdown;     // the countdown on screen (seconds)
bool g_results_open = true;
bool g_merc_open = true;
bool g_add_open[game::kMaxChars] = {};

// A count typed for an item, by character (sPlayer's index), slot number and mode:
// it belongs to the item it was typed for (its id), and is let go when that item
// is no longer there.
constexpr int kSlots = 32;
struct CountEdit {
  int id = 0, v = 0;
  bool typing = false;
};
CountEdit g_count[game::kMaxChars][kSlots][inventory::kModes];

// Ticks on the case items' rows, each with the id it was made on: an item that changed
// under it drops it.
struct CaseTick {
  int id = 0;
  bool on = false;
};
CaseTick g_tick[game::kMaxChars][kSlots];

// The Add list's picker of each character.
struct Picker {
  char query[48] = "";
  int tab = 0;
  int picked = 0;
  int count = 1;
  int preloaded = 0;  // the pick whose files were asked for at this showing
};
Picker g_picker[game::kMaxChars];

void forget_edits() {
  g_points.typing = g_clock.typing = g_countdown.typing = false;
  for (Picker& p : g_picker) p.preloaded = 0;
  for (auto& c : g_counter)
    for (Edit& e : c) e.typing = false;
  for (auto& chara : g_count)
    for (auto& slot : chara)
      for (CountEdit& e : slot) e.typing = false;
}

void hms(float seconds, char* out, size_t n) {
  const int s = seconds < 0 ? 0 : static_cast<int>(seconds);
  std::snprintf(out, n, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
}

// The switches that have sections of their own (counted in their headings).
bool results_switch(int k) { return k == sw::kFreezePlaytime || k == sw::kNoDeathCount || k == sw::kNoMissCount; }
bool merc_switch(int k) { return k == sw::kEndlessCombo || k == sw::kComboTime || k == sw::kAutoKill; }

void draw_cheats(const dispatch::Snapshot& snap, const cheats::Status& st) {
  const bool paused = snap.run.screen == game::Screen::kPause;
  ImGui::SeparatorText("Characters");
  cheat_row(sw::kGodMode, "God mode",
            "No hit takes any health off you: a hit on you does no damage (you still flinch), and the enemies' grabs "
            "and blows take nothing either, nor do they put you down. Your health is held at its maximum too. The "
            "vehicle you ride - Ada's helicopter, Chris's jet, a snowmobile, a bike or a jeep - takes no damage "
            "while you are on it either (with Partner takes no damage, while the computer's partner is), and is "
            "repaired as you switch this on. A death the game scripts without a hit (a fall, a missed QTE) is left "
            "to it. Covers every character this computer plays, a second player on the same screen too.");
  cheat_row(sw::kPartnerGod, "Partner takes no damage",
            "The same for your partner while the computer plays them. (In a campaign the game already spares a "
            "computer partner most damage; this covers the rest, and the modes where it does not.) A partner played "
            "by another person over the network is left to their game.");
  cheat_row(sw::kInfiniteStamina, "Infinite stamina",
            "Your combat gauge (the blocks beside your health) never runs down: melee moves, quick shots and every "
            "other move that costs blocks take none, so none is ever held back for want of them. If the game empties "
            "the gauge itself (as you go down), it is full again as soon as you are back on your feet. Under water "
            "the same gauge is your air - that is Infinite air's. Covers every character this computer plays; a "
            "partner the computer plays is left to the game.");
  cheat_row(sw::kInfiniteAir, "Infinite air",
            "Under water your air never runs out: the air gauge that shows while you swim under water stays full, "
            "so you never drown (the dives where the game counts your breath: Leon's chapter 3 and Ada's chapter 1). "
            "God mode cannot save you from drowning; this can. On land the same gauge is your combat gauge - that is "
            "Infinite stamina's. Covers every character this computer plays; a partner the computer plays never runs "
            "short of air in the game anyway.");
  cheat_row(sw::kOneHitKills, "One hit kills",
            "Any hit by you or your partner that does damage takes all of an enemy's health, so it dies through the "
            "game's own death. An enemy the game keeps alive for now (a boss between phases, one that may not die "
            "yet) keeps its last point, and a boss whose fight goes in stages loses a stage at a hit. The robots "
            "that roll along the floor and blow up (Leon's chapter 4) go off at your first hit. Damage the "
            "enemies do to each other, and to the people you protect, is left as it is.");
  cheat_row(sw::kAutoQte, "Automatic QTEs",
            "The quick time events play themselves: a grab is broken free of, a button mashed, a stick waggled or "
            "turned (a crank winds at full speed), two buttons pressed together, a dodge or a counter taken - as soon "
            "as its prompt comes up, without it being drawn. A press in time waits for its best moment and is taken "
            "then. The scenes the game itself makes automatic for its own option take that path too (the setting in "
            "your options is not changed). Prompts you choose to act on - doors, items, cover, melee moves, helping "
            "your partner - are left to you, and so is a partner played by the computer or online.");
  ImGui::SeparatorText("Weapons");
  cheat_row(sw::kInfiniteAmmo, "Infinite ammo",
            "Every weapon of yours gets the game's own infinite ammo: firing uses no rounds, so there is nothing to "
            "reload, and a thrown grenade stays in your inventory. The helicopter Ada flies keeps all its missiles "
            "too (any it has fired come back as you switch this on). Health tablets are still used up as usual. "
            "Switched off, your guns use rounds again.");
  cheat_row(sw::kSteadyAim, "Steady aim",
            "Your aim holds still. The game sways an aim you hold a little (the laser dot, or the view through a "
            "scope, drifts about where you point it) and kicks it up at every shot. With this on there is neither: "
            "your shots go where the reticle is. Through a scope the view stands still whenever you are not moving "
            "your aim - the rifle's breathing and its kick at a shot don't move it - and follows your mouse or stick "
            "as usual. A shotgun keeps its spread of pellets. (Your partner's aim may steady too.)");
  ImGui::SeparatorText("Skill points");
  cheat_row(sw::kInfiniteSkillPoints, "Infinite skill points",
            "Buying a skill (or its next level) on the skill screen takes no skill points; picking skill points up "
            "still adds to them.");
  if (st.points &&
      int_editor("Skill points:", "points", g_points, st.skill_points, 0, cheats::kMostSkillPoints, 120, paused))
    cheats::edit(cheats::Edit::kSkillPoints, 0, g_points.v);
  if (st.points) item_help("Your profile's skill points, which the skill screen spends. Set changes them at once; the "
                           "game keeps them with your profile the next time it saves.");
  ImGui::SeparatorText("Countdown");
  cheat_row(sw::kFreezeCountdown, "Freeze countdown timer",
            "The countdown on screen stands still where it is, so time never runs out: a stage's countdown in the "
            "campaigns, and the Mercenaries' clock (its time bonuses still add to it). It runs on from there the "
            "moment you switch this off. Offline only.");
  if (st.countdown && st.countdown_most > 0) {
    const int left = static_cast<int>(st.countdown_left);
    if (int_editor("Time left (s)", "countdown", g_countdown, left, 1, static_cast<int>(st.countdown_most), 90, paused))
      cheats::edit(cheats::Edit::kCountdown, 0, g_countdown.v);
    item_help(st.match_clock ? "The seconds left of the match. It runs down from what you set once you leave the "
                               "pause (or stays there while Freeze countdown timer is on)."
                             : "The seconds left of the countdown on screen. It runs down from what you set once you "
                               "leave the pause (or stays there while Freeze countdown timer is on).");
    ImGui::SameLine();
    ImGui::TextDisabled("%d:%02d", left / 60, left % 60);
  }
  if (!paused && (st.points || st.countdown)) ImGui::TextDisabled("Pause the game to change the numbers.");
}

// The Mercenaries' own switches and the match's numbers (shown only in a match).
void draw_mercenaries(const merc::Status& m) {
  cheat_row(sw::kEndlessCombo, "Endless combo",
            "A combo never runs out of time: every kill counts as the next one in it however long since the last, "
            "so its bonus keeps climbing. The game puts what a combo has earned onto your score only once the combo "
            "is over; with this on it goes on as it is earned instead. Switched off, the combo runs out as usual.");
  cheat_row(sw::kComboTime, "Combo Time always on",
            "Combo Time - what the Combo Time hourglass gives: every kill's combo bonus at least 1,000 - runs from "
            "the moment you switch this on and never runs out. Switched off, the time it has left runs down.");
  cheat_row(sw::kAutoKill, "Kill enemies as they appear",
            "Every enemy dies the moment it is in play, wherever it is (those already about when you switch it on "
            "too): the game's own death, and the kill scores as one of your hand-to-hand kills - its points, the "
            "combo, the stage's kills and the time a hand-to-hand kill puts on the clock. Each enemy is hit until it "
            "is dead, whatever its armour or its own damage makes of a hit; one far off may wait until it wakes. One "
            "the game keeps alive by its own rules (Rasklapanje never loses its last point) is hit again and again, "
            "on another part of it each time. While it is on, a body vanishes the moment its kill counts (no death "
            "animation, no dissolve), and the stage sends its next enemy whenever there is room (it sends only while "
            "fewer than its limit are about, bodies counted), without its own wait between enemies (as much as 2 s); "
            "it sends from its first area wherever you stand (on the maps that send by where you are), and each of its "
            "waves also sends what the waves before it left unsent (on Steel Beast the stage stopped at 89 kills "
            "without it). It waits for the clock to start and stops when time is up. It "
            "lasts one match: it switches itself off as the match ends (cleared, time up, retried or quit) - switch it "
            "on again in the pause of the next.");
  if (!m.offline) ImGui::TextColored(kOrange, "Online: these act only in a game of your own (offline).");
  ImGui::SeparatorText("This match");
  const int left = static_cast<int>(m.time_left);
  ImGui::Text("Score %d", m.score);
  if (m.waiting > 0) {
    ImGui::SameLine();
    ImGui::TextDisabled("(+%d as the combo ends)", m.waiting);
  }
  ImGui::Text("Combo %d  (best %d)", m.combo, m.best_combo);
  ImGui::Text("Kills %d of %d", m.kills, m.target);
  ImGui::Text("Time left %d:%02d", left / 60, left % 60);
  if (m.combo_time) {
    const int b = static_cast<int>(m.combo_time_left);
    ImGui::Text("Combo Time %d:%02d left, %d kill%s in it", b / 60, b % 60, m.combo_time_kills,
                m.combo_time_kills == 1 ? "" : "s");
  }
  if (cheats::enabled(sw::kAutoKill) && m.step == 1)
    ImGui::TextDisabled("Killed %d enem%s this match; %d in play now", m.killed, m.killed == 1 ? "y" : "ies",
                        m.killing);
}

void draw_results(const dispatch::Snapshot& snap, const cheats::Status& st) {
  const bool paused = snap.run.screen == game::Screen::kPause;
  cheat_row(sw::kFreezePlaytime, "Freeze play time",
            "The chapter's clock - the time on the results screen, which its rank goes by - stands still.");
  cheat_row(sw::kNoDeathCount, "Deaths don't count", "A death (a Game Over) is not counted in the chapter's results.");
  cheat_row(sw::kNoMissCount, "Misses don't count",
            "A shot that misses is not counted as a shot fired, so the chapter's accuracy stays at 100%.");
  if (!st.results) return;
  char time[32];
  hms(st.clock, time, sizeof(time));
  ImGui::SeparatorText("Chapter");
  if (int_editor("Time (s)", "time", g_clock, static_cast<int>(st.clock), 0, static_cast<int>(st.clock_most), 90, paused))
    cheats::edit(cheats::Edit::kTime, 0, g_clock.v);
  item_help("The chapter's play time in seconds, as the results screen shows it (its rank goes by it). The game keeps "
            "one clock for the chapter, and Set changes it for every character.");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", time);
  static const char* const kWhat[4] = {"Deaths", "Kills", "Shots", "Hits"};
  static const cheats::Edit kEdit[4] = {cheats::Edit::kDeaths, cheats::Edit::kKills, cheats::Edit::kShots,
                                        cheats::Edit::kHits};
  for (int c = 0; c < st.ncounters && c < 2; ++c) {
    const cheats::Counters& n = st.chars[c];
    ImGui::SeparatorText(n.name);
    const int live[4] = {n.deaths, n.kills, n.shots, n.hits};
    for (int k = 0; k < 4; ++k) {
      char id[24], label[24];
      std::snprintf(id, sizeof(id), "c%d%d", c, k);
      std::snprintf(label, sizeof(label), "%-7s", kWhat[k]);
      if (int_editor(label, id, g_counter[c][k], live[k], 0, st.counter_most, 90, paused))
        cheats::edit(kEdit[k], n.index, g_counter[c][k].v);
      if (k % 2 == 0) ImGui::SameLine(0, scaled(24));
    }
    if (n.shots > 0) {
      ImGui::SameLine();
      ImGui::TextDisabled(" accuracy %d%%", n.hits * 100 / n.shots);
    }
  }
  if (!paused) ImGui::TextDisabled("Pause the game to change the numbers.");
}

const char* item_name(int id, char* buf, size_t n) {
  if (const items::Info* i = items::find(id)) return i->name;
  std::snprintf(buf, n, "Item 0x%03X", id);
  return buf;
}

// A row's label for one of an item's counts: its name, and for a weapon that keeps a
// count for each mode, what that mode loads.
const char* count_label(const inventory::Item& it, int m, int modes, char* buf, size_t n) {
  char nb[48];
  const char* name = item_name(it.id, nb, sizeof(nb));
  if (modes < 2) {
    std::snprintf(buf, n, "%s", name);
    return buf;
  }
  const items::Info* i = items::find(it.id);
  if (i && i->modes[m]) std::snprintf(buf, n, "%s  (%s)", name, i->modes[m]);
  else std::snprintf(buf, n, "%s  (%d)", name, m + 1);
  return buf;
}

// "All": ticked when every row is, mixed when some are; a click ticks them all
// (or, all ticked, unticks them).
bool tick_all(const char* label, int on, int of, bool* all) {
  bool v = of > 0 && on == of;
  const bool some = on > 0 && on < of;
  ImGui::BeginDisabled(of == 0);
  if (some) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
  const bool clicked = ImGui::Checkbox(label, &v);
  if (some) ImGui::PopItemFlag();
  ImGui::EndDisabled();
  *all = v;
  return clicked;
}

// A button's label with how many it works on ("Throw away 3"), and its fixed id.
void numbered(char* out, size_t n, const char* text, int count, const char* id) {
  if (count > 1) std::snprintf(out, n, "%s %d###%s", text, count, id);
  else std::snprintf(out, n, "%s###%s", text, id);
}

// One character's inventory: each item with its counts (each mode's, for a weapon that
// keeps more than one) - a field to type a new one, Apply (or Undo) - and, for an item in
// the case, a tick and Throw away (x); above, Apply all for every count typed, and Throw
// away for the ticked. A character played over the network is shown only.
void draw_case(const inventory::View& v, const inventory::Case& c) {
  const bool can = v.paused && c.control != game::Control::kNetwork;
  const bool can_throw = can && v.can_add;
  const int who = c.index & (game::kMaxChars - 1);
  if (!c.nitems) {
    ImGui::TextDisabled("Nothing held.");
    return;
  }
  int typed = 0, on = 0, of = 0;
  for (int i = 0; i < c.nitems; ++i) {
    const inventory::Item& it = c.items[i];
    if (it.slot < 0 || it.slot >= kSlots) continue;
    CaseTick& t = g_tick[who][it.slot];
    if (t.id && t.id != it.id) t = CaseTick{};
    for (int m = 0; m < inventory::kModes; ++m) {
      CountEdit& e = g_count[who][it.slot][m];
      if (e.id != it.id) e = CountEdit{it.id, it.num[m], false};
      typed += e.typing && e.v != it.num[m];
    }
    if (it.in_case && !it.in_hand) {
      ++of;
      on += t.on;
    }
  }
  char label[64], id[32];
  ImGui::BeginDisabled(!can);
  if (v.can_add || of) {
    bool all = false;
    std::snprintf(label, sizeof(label), "All##all%d", who);
    ImGui::BeginDisabled(!can_throw);
    if (tick_all(label, on, of, &all))
      for (int i = 0; i < c.nitems; ++i)
        if (c.items[i].in_case && !c.items[i].in_hand && c.items[i].slot < kSlots)
          g_tick[who][c.items[i].slot] = CaseTick{c.items[i].id, all};
    ImGui::EndDisabled();
    item_help("Tick every item in the case - or, all ticked, untick them. Ticked items can be thrown away together.");
    ImGui::SameLine();
  }
  ImGui::BeginDisabled(!typed);
  std::snprintf(id, sizeof(id), "apply%d", who);
  numbered(label, sizeof(label), "Apply all", typed, id);
  if (ImGui::SmallButton(label))
    for (int i = 0; i < c.nitems; ++i) {
      const inventory::Item& it = c.items[i];
      if (it.slot < 0 || it.slot >= kSlots) continue;
      for (int m = 0; m < inventory::kModes; ++m) {
        CountEdit& e = g_count[who][it.slot][m];
        if (e.typing && e.id == it.id && e.v != it.num[m]) inventory::set_count(c.index, it.slot, it.id, m, e.v);
        e.typing = false;
      }
    }
  ImGui::EndDisabled();
  item_help("Make every count typed in the list below, all at once.");
  ImGui::SameLine();
  ImGui::BeginDisabled(!on || !can_throw);
  std::snprintf(id, sizeof(id), "throw%d", who);
  numbered(label, sizeof(label), "Throw away", on, id);
  if (ImGui::SmallButton(label)) {
    for (int i = 0; i < c.nitems; ++i) {
      const inventory::Item& it = c.items[i];
      if (it.slot < kSlots && g_tick[who][it.slot].on && it.in_case && !it.in_hand)
        inventory::throw_away(c.index, it.slot, it.id);
    }
    for (CaseTick& t : g_tick[who]) t = CaseTick{};
  }
  ImGui::EndDisabled();
  item_help("Throw every ticked item away, as the game's own Discard does.");
  ImGui::EndDisabled();

  if (!ImGui::BeginTable("case", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) return;
  ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(24));
  ImGui::TableSetupColumn("item", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, scaled(110));
  ImGui::TableSetupColumn("do", ImGuiTableColumnFlags_WidthFixed, scaled(110));
  for (int i = 0; i < c.nitems; ++i) {
    const inventory::Item& it = c.items[i];
    if (it.slot < 0 || it.slot >= kSlots) continue;
    int modes = 0;
    for (int m = 0; m < inventory::kModes; ++m)
      if (m == 0 || it.most[m] > 0) modes = m + 1;
    for (int m = 0; m < modes; ++m) {
      CountEdit& e = g_count[who][it.slot][m];
      if (!e.typing) e.v = it.num[m];
      ImGui::PushID(it.slot * 4 + m);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (m == 0 && it.in_case && !it.in_hand) {
        CaseTick& t = g_tick[who][it.slot];
        bool tick = t.on;
        std::snprintf(id, sizeof(id), "##t%d%02d", who, it.slot);
        ImGui::BeginDisabled(!can_throw);
        if (ImGui::Checkbox(id, &tick)) t = CaseTick{it.id, tick};
        ImGui::EndDisabled();
      }
      ImGui::TableNextColumn();
      char nb[96];
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(count_label(it, m, modes, nb, sizeof(nb)));
      if (it.in_hand && m == 0) {
        ImGui::SameLine();
        ImGui::TextColored(kOrange, "(in hand)");
      }
      ImGui::TableNextColumn();
      if (it.most[m] > 0) {
        ImGui::BeginDisabled(!can);
        ImGui::SetNextItemWidth(scaled(50));
        std::snprintf(id, sizeof(id), "##n%d%02d%d", who, it.slot, m);
        if (ImGui::InputInt(id, &e.v, 0, 0)) e.typing = true;
        if (e.v < 0) e.v = 0;
        if (e.v > it.most[m]) e.v = it.most[m];
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("/%d", it.most[m]);
      }
      ImGui::TableNextColumn();
      ImGui::BeginDisabled(!can);
      if (e.typing && e.v != it.num[m]) {
        std::snprintf(id, sizeof(id), "Apply##p%d%02d%d", who, it.slot, m);
        if (ImGui::SmallButton(id)) {
          inventory::set_count(c.index, it.slot, it.id, m, e.v);
          e.typing = false;
        }
        ImGui::SameLine();
        std::snprintf(id, sizeof(id), "Undo##u%d%02d%d", who, it.slot, m);
        if (ImGui::SmallButton(id)) e.typing = false;
        ImGui::SameLine();
      } else if (e.typing) {
        e.typing = false;  // typed back to what it is
      }
      if (m == 0 && it.in_case && !it.in_hand && v.can_add) {
        std::snprintf(id, sizeof(id), "x##x%d%02d", who, it.slot);
        ImGui::BeginDisabled(!can_throw);
        if (ImGui::SmallButton(id)) inventory::throw_away(c.index, it.slot, it.id);
        ImGui::EndDisabled();
        item_help("Throw it away.");
      }
      ImGui::EndDisabled();
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
  if (c.control == game::Control::kNetwork) ImGui::TextDisabled("Played online by another person: shown only.");
}

// The Add list's tabs. No weapons: the Records WEAPONS window puts a character's weapons in
// its saved inventory (the user, 2026-11-14).
enum Tab { kTabAmmo, kTabGrenades, kTabRecovery, kTabCount };
const char* const kTabNames[kTabCount] = {"Ammo", "Grenades", "Recovery"};
int tab_of(const items::Info& i) {
  if (!i.offer) return -1;
  switch (i.kind) {
    case items::kAmmo: return kTabAmmo;
    case items::kGrenade: return kTabGrenades;
    case items::kRecovery: return kTabRecovery;
    default: return -1;
  }
}

// What the Add list offers `c`: what Add offers anyone, the ammo only of a kind one of its own weapons loads.
bool offered_to(const inventory::Case& c, const items::Info& i) {
  return tab_of(i) >= 0 && (i.kind != items::kAmmo || inventory::takes(c, i.id));
}

bool matches_query(const char* name, const char* q) {
  if (!q[0]) return true;
  for (const char* h = name; *h; ++h) {
    const char *a = h, *b = q;
    while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b)))
      ++a, ++b;
    if (!*b) return true;
  }
  return false;
}

// Why `i` cannot be added now, or nullptr when it can.
const char* add_blocked(const inventory::View& v, const items::Info& i) {
  if (inventory::avail(v, i.id) == inventory::kNever)
    return "The game has not loaded what it needs here (its model and sounds), and the mod could not have the "
           "game's loader load it (see the log): it can be added once someone in this stage holds one.";
  return nullptr;
}

void draw_add(const inventory::View& v, const inventory::Case& c) {
  Picker& p = g_picker[c.index & (game::kMaxChars - 1)];
  ImGui::SetNextItemWidth(scaled(220));
  ImGui::InputTextWithHint("##q", "search", p.query, sizeof(p.query));
  if (ImGui::BeginTabBar("tabs")) {
    for (int t = 0; t < kTabCount; ++t)
      if (ImGui::BeginTabItem(kTabNames[t])) {
        p.tab = t;
        ImGui::EndTabItem();
      }
    ImGui::EndTabBar();
  }
  int greyed = 0, listed = 0;
  if (ImGui::BeginChild("list", ImVec2(0, scaled(150)), ImGuiChildFlags_Borders)) {
    for (const items::Info& i : items::kItems) {
      const int t = tab_of(i);
      if (!offered_to(c, i) || (t != p.tab && !p.query[0])) continue;
      if (!matches_query(i.name, p.query)) continue;
      ++listed;
      const char* why = add_blocked(v, i);
      if (why) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      char label[80];
      std::snprintf(label, sizeof(label), "%s##%X", i.name, i.id);
      if (ImGui::Selectable(label, p.picked == i.id)) p.picked = i.id;
      if (why) {
        ImGui::PopStyleColor();
        item_help(why);
        ++greyed;
      }
    }
    if (!listed && p.tab == kTabAmmo && !p.query[0]) ImGui::TextDisabled("No weapon %s holds takes ammo.", c.name);
  }
  ImGui::EndChild();
  if (greyed) ImGui::TextDisabled("Greyed: cannot be added now - hover one for why.");
  const items::Info* pick = items::find(p.picked);
  if (pick && !offered_to(c, *pick)) pick = nullptr;
  const char* why = pick ? add_blocked(v, *pick) : nullptr;
  // What the pick needs that the stage has not loaded, asked for as it is picked: its Add
  // then acts at once.
  if (pick && !why && v.can_add && p.preloaded != pick->id && inventory::avail(v, pick->id) == inventory::kLoadable) {
    inventory::preload(pick->id);
    p.preloaded = pick->id;
  }
  ImGui::BeginDisabled(!pick || !v.can_add || why || c.control == game::Control::kNetwork);
  if (pick) {
    ImGui::SetNextItemWidth(scaled(60));
    ImGui::InputInt("##count", &p.count, 0, 0);
    if (p.count < 1) p.count = 1;
    if (p.count > 999) p.count = 999;
    ImGui::SameLine();
  }
  char label[96];
  std::snprintf(label, sizeof(label), "Add %s###add", pick ? pick->name : "");
  if (ImGui::Button(label) && pick) inventory::add(c.index, pick->id, p.count);
  ImGui::EndDisabled();
  item_help(why ? why
                : "Onto the stacks of it there are first, the rest into free slots of the case - as a pickup goes in. "
                  "As many as there is room for. A grenade's or the spray's model and sounds are loaded first when "
                  "this stage has not loaded them, by the game's own loader.");
  if (v.note[0] && v.note_chara == c.index) {
    ImGui::PushTextWrapPos(0.0f);
    if (v.loading) ImGui::TextDisabled("%s", v.note);
    else ImGui::TextColored(kOrange, "%s", v.note);
    ImGui::PopTextWrapPos();
  }
}

void draw_panel() {
  const dispatch::Snapshot s = dispatch::snapshot();
  const ImGuiIO& io = ImGui::GetIO();
  // Resizable, not auto-fitted (an auto-fitting window whose wrapped text
  // measures itself against its own width shrinks a little every frame once a
  // scrollbar takes some of it), and never bigger than the display.
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - scaled(600), scaled(40)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(scaled(580), scaled(640)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(scaled(360), scaled(160)), io.DisplaySize);
  if (!ImGui::Begin(kPanelWindow, nullptr)) {
    ImGui::End();
    return;
  }
  keep_in_reach(io.DisplaySize);
  if (!s.ready) {
    ImGui::TextColored(kOrange, "Game hooks: %s", boot::done() ? boot::status() : "waiting for the game to start");
    ImGui::End();
    return;
  }
  if (!s.run.in_game) ImGui::TextDisabled("Waiting for a game (start a campaign or a mode)...");
  const cheats::Status st = cheats::status();
  int on = 0, results_on = 0, merc_on = 0;
  for (int k = 0; k < sw::kCount; ++k) {
    if (!cheats::enabled(static_cast<sw::Kind>(k))) continue;
    if (results_switch(k)) ++results_on;
    else if (merc_switch(k)) ++merc_on;
    else ++on;
  }
  char head[64];
  std::snprintf(head, sizeof(head), "Cheats  (%d on)###cheats", on);
  if (section(head, &g_cheats_open)) draw_cheats(s, st);
  const merc::Status ms = merc::status();
  if (s.run.in_game && ms.match) {
    if (merc_on) std::snprintf(head, sizeof(head), "The Mercenaries  (%d on)###mercenaries", merc_on);
    else std::snprintf(head, sizeof(head), "The Mercenaries###mercenaries");
    if (section(head, &g_merc_open)) draw_mercenaries(ms);
  }
  // The chapter's results: in a campaign (when the mode is not known, in any game but a
  // match of the Mercenaries).
  const bool campaign = s.run.mode_known ? s.run.mode == game::kModeCampaign : !ms.match;
  if (s.run.in_game && campaign) {
    if (results_on) std::snprintf(head, sizeof(head), "Results  (%d on)###results", results_on);
    else std::snprintf(head, sizeof(head), "Results###results");
    if (section(head, &g_results_open)) draw_results(s, st);
  }
  if (s.run.in_game) {
    const inventory::View v = inventory::view();
    for (int c = 0; v.known && c < v.ncases; ++c) {
      const inventory::Case& cs = v.cases[c];
      std::snprintf(head, sizeof(head), "%s's inventory  (%d)###case%d", cs.name, cs.nitems, cs.index);
      if (section(head, &g_case_open[cs.index & (game::kMaxChars - 1)])) {
        ImGui::PushID(cs.index);
        draw_case(v, cs);
        if (v.can_add && cs.control != game::Control::kNetwork) {
          char sub[64];
          std::snprintf(sub, sizeof(sub), "Add to %s's inventory###add%d", cs.name, cs.index);
          subsection(sub, &g_add_open[cs.index & (game::kMaxChars - 1)], [&] { draw_add(v, cs); });
        }
        ImGui::PopID();
      }
    }
    if (v.known && !v.paused) ImGui::TextDisabled("Pause the game to change the inventories.");
    else if (v.known && v.online) ImGui::TextDisabled("Online: Add and Throw away are for a game of your own.");
  }
  ImGui::Separator();
  ImGui::TextDisabled("F%d hides this panel", config::get().toggle_key - 0x6F);
  ImGui::End();
}

// --- the records windows: beside the Records screen and the Dog Tag screen (records.h) ----------------------------
const char* const kChaptersWindow = "Resident Evil 6 - Cabby Codes: chapters completed  v" RE6CC_VERSION "###RE6CC chapters";
const char* const kMedalsWindow = "Resident Evil 6 - Cabby Codes: medals  v" RE6CC_VERSION "###RE6CC medals";
const char* const kEmblemsWindow = "Resident Evil 6 - Cabby Codes: emblems  v" RE6CC_VERSION "###RE6CC emblems";
const char* const kTitlesWindow = "Resident Evil 6 - Cabby Codes: titles  v" RE6CC_VERSION "###RE6CC titles";
const char* const kSkillsWindow = "Resident Evil 6 - Cabby Codes: skills  v" RE6CC_VERSION "###RE6CC skills";
const char* const kWeaponsWindow = "Resident Evil 6 - Cabby Codes: weapons  v" RE6CC_VERSION "###RE6CC weapons";
const char* const kEnemiesWindow = "Resident Evil 6 - Cabby Codes: enemies  v" RE6CC_VERSION "###RE6CC enemies";
const char* const kMercWindow = "Resident Evil 6 - Cabby Codes: the Mercenaries  v" RE6CC_VERSION "###RE6CC mercenaries";
const char* const kNoMercyWindow =
    "Resident Evil 6 - Cabby Codes: the Mercenaries No Mercy  v" RE6CC_VERSION "###RE6CC mercenaries";

// The chapter grid's edits waiting for Apply: -1 none, 0 not cleared, 1 + rank cleared at that rank.
int8_t g_chap_edit[records::kChars][records::kDifficulties][records::kChapters];
int g_chap_tab = 0;
int g_chap_all = 1;  // the "set every chapter" choice: S
// A chapter's best time typed (h:mm:ss, as Chapter Select shows it), waiting for Apply: the text as typed; while not
// typing it follows the record.
struct TimeEdit {
  char text[16] = "";
  bool typing = false;
};
TimeEdit g_chap_time[records::kChars][records::kDifficulties][records::kChapters];
char g_chap_all_time[16] = "";  // the "set every chapter" time: empty leaves each chapter's
const char* const kRankItems[] = {"-", "S", "A", "B", "C", "D", "E"};
// The medals' counts typed (waiting for Apply), the tab, what the game's page showed last.
struct MedalEdit {
  int v = 0;
  bool typing = false;
};
MedalEdit g_medal_edit[records::kMedalCats][records::kMaxMedals];
int g_medal_tab = -1, g_medal_game = -1;
int g_medal_all = records::kMedalMost;
char g_medal_query[48] = "";
// The emblems and titles ticked.
bool g_emblem_tick[records::kEmblems] = {};
bool g_emblem_locked_only = false;
bool g_title_tick[records::kMaxTitleRows + 1] = {};  // by counter
bool g_title_open_only = false;
char g_title_query[48] = "";
// A skill's tick as clicked, shown so until the game's state says so (or a moment passes): the click is applied by
// the next tick, the game's frame after this one.
struct SkillPending {
  int8_t on = -1;  // -1 none, else the state asked for (1 unlocked)
  DWORD at = 0;
};
SkillPending g_skill_pending[records_text::kSkillMsgs];
// The weapons window's ticks as clicked, the same way: a WEAPONS page row's (by its row) and a weapon in a
// character's saved inventory (by the character and the weapon's place in its list).
SkillPending g_row_pending[records::kMaxWeaponRows];
SkillPending g_carry_pending[records::kCarriers][records::kMaxCarried];
// The enemies window's ticks as clicked (by the page's row), and its filter.
SkillPending g_enemy_pending[records::kMaxEnemyRows];
bool g_enemy_hidden_only = false;
// The Mercenaries window's ticks as clicked: a stage's (by its bit) and a costume's (by its bit).
SkillPending g_merc_stage_pending[32];
SkillPending g_merc_costume_pending[64];

void forget_records_edits() {
  std::memset(g_chap_edit, -1, sizeof(g_chap_edit));
  for (auto& c : g_chap_time)
    for (auto& d : c)
      for (TimeEdit& t : d) t.typing = false;
  g_chap_all_time[0] = '\0';
  for (auto& c : g_medal_edit)
    for (MedalEdit& e : c) e.typing = false;
  g_medal_game = -1;
  std::memset(g_emblem_tick, 0, sizeof(g_emblem_tick));
  std::memset(g_title_tick, 0, sizeof(g_title_tick));
  for (SkillPending& p : g_skill_pending) p = SkillPending{};
  for (SkillPending& p : g_row_pending) p = SkillPending{};
  for (auto& c : g_carry_pending)
    for (SkillPending& p : c) p = SkillPending{};
  for (SkillPending& p : g_enemy_pending) p = SkillPending{};
  for (SkillPending& p : g_merc_stage_pending) p = SkillPending{};
  for (SkillPending& p : g_merc_costume_pending) p = SkillPending{};
}

int cell_value(const records::Cell& c) { return c.cleared ? 1 + c.rank : 0; }

// A best time in whole seconds as Chapter Select shows it (0x9C0E50): cut to whole seconds (not rounded), at most the
// game's most. 0 is a time ("0:00:00": a clear made with the clock held at 0, as Freeze play time holds it).
int time_seconds(float t, float most) {
  const int m = most > 0 ? static_cast<int>(most) : 0x7FFFFFFF;
  if (!(t > 0)) return 0;  // 0, and a NaN (the game's conversion gives 0 too)
  return t < static_cast<float>(m) ? static_cast<int>(t) : m;
}

// A time typed back into seconds: "h:mm:ss", "m:ss" or seconds - the first part any size, the ones after it
// 0-59 - held at the game's most. False when it is not one.
bool parse_time(const char* text, float most, int* out) {
  int part[3] = {}, n = 0, digits = 0;
  for (const char* q = text;; ++q) {
    if (*q >= '0' && *q <= '9') {
      if (++digits > 9) return false;
      part[n] = part[n] * 10 + (*q - '0');
    } else if (*q == ':' || !*q) {
      if (!digits || (n > 0 && (digits > 2 || part[n] > 59))) return false;
      if (!*q) break;
      if (++n >= 3) return false;
      digits = 0;
    } else if (*q != ' ') {
      return false;
    }
  }
  long long s = 0;
  for (int i = 0; i <= n; ++i) s = s * 60 + part[i];
  const long long m = most > 0 ? static_cast<long long>(most) : s;
  *out = static_cast<int>(s < m ? s : m);
  return true;
}

// Only digits and ':' go into a time field.
int time_chars(ImGuiInputTextCallbackData* d) {
  return (d->EventChar >= '0' && d->EventChar <= '9') || d->EventChar == ':' ? 0 : 1;
}

// What a chapter's record will hold after Apply all: cleared or not (a rank picked, else as it is), and the time its
// field shows when the time is not typed - a cleared one's own, as Chapter Select shows it (0 too: "0:00:00"); one
// Apply all is to clear, what the clear gives it (apply_chapters): the time it holds if above 0, else the game's "no
// time" (its most).
bool will_clear(const records::Cell& cell, int8_t edit) { return edit >= 0 ? edit > 0 : cell.cleared; }
int shown_time(const records::Cell& cell, float most) {
  return time_seconds(cell.cleared || cell.time > 0 ? cell.time : most, most);
}

// A typed time that changes the record (it parses, and is not what the record holds): its seconds, else -1.
int typed_time(const TimeEdit& t, const records::Cell& cell, float most) {
  int s = 0;
  if (!t.typing || most <= 0 || !parse_time(t.text, most, &s)) return -1;
  return s != shown_time(cell, most) ? s : -1;
}

void type_time(TimeEdit& t, int seconds) {
  hms(static_cast<float>(seconds), t.text, sizeof(t.text));
  t.typing = true;
}

// A records window, placed and sized as the panel is on its first showing (the layout file keeps it after).
bool begin_records_window(const char* id, float width = 620) {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - scaled(width + 20), scaled(40)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(scaled(width), scaled(600)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(scaled(380), scaled(200)), io.DisplaySize);
  if (!ImGui::Begin(id, nullptr)) {
    ImGui::End();
    return false;
  }
  keep_in_reach(io.DisplaySize);
  return true;
}

// The room a list leaves for the window's last lines (the note, the toggle key's), and `extra` lines more.
float footer(int extra) {
  return ImGui::GetTextLineHeightWithSpacing() * (3 + extra) + ImGui::GetStyle().ItemSpacing.y * 2 +
         ImGui::GetFrameHeightWithSpacing() * extra;
}

void end_records_window(const records::View& v) {
  ImGui::Separator();
  if (v.note[0]) {
    ImGui::PushTextWrapPos(0.0f);
    if (std::strstr(v.note, "Not saved") || std::strstr(v.note, "cannot")) ImGui::TextColored(kOrange, "%s", v.note);
    else ImGui::TextDisabled("%s", v.note);
    ImGui::PopTextWrapPos();
  }
  ImGui::TextDisabled("F%d hides this window", config::get().toggle_key - 0x6F);
  ImGui::End();
}

void apply_line(int pending, const char* apply_help, bool* apply, bool* undo) {
  char label[48];
  ImGui::BeginDisabled(pending == 0);
  numbered(label, sizeof(label), "Apply all", pending, "apply");
  *apply = ImGui::Button(label);
  item_help(apply_help);
  ImGui::SameLine();
  *undo = ImGui::Button("Undo all");
  item_help("Forget every change typed or picked here that has not been applied.");
  ImGui::EndDisabled();
}

void draw_chapters(const records::View& v) {
  namespace rt = records_text;
  const float most = v.time_most;  // the game's most (its "no time"); 0: not known, no times here
  int pending = 0;
  for (int c = 0; c < records::kChars; ++c)
    for (int d = 0; d < records::kDifficulties; ++d)
      for (int ch = 0; ch < records::kChapters; ++ch) {
        int8_t& e = g_chap_edit[c][d][ch];
        const records::Cell& cell = v.cell[c][d][ch];
        if (e == cell_value(cell)) e = -1;
        pending += e >= 0 || (will_clear(cell, e) && typed_time(g_chap_time[c][d][ch], cell, most) >= 0);
      }
  char most_text[16] = "";
  if (most > 0) hms(static_cast<float>(time_seconds(most, most)), most_text, sizeof(most_text));
  ImGui::Text("Completed %d%%", v.percent);
  item_help("As the Records screen counts it: the records held (every character, difficulty and chapter) out of all "
            "of them.");
  ImGui::SameLine(0, scaled(24));
  bool apply = false, undo = false;
  apply_line(pending,
             "Mark every chapter picked below - cleared at the rank picked (the look of its mark on the grid), or not "
             "cleared - with the best time typed under it, and save your profile. A chapter newly cleared also gets "
             "what clearing it gives: its dog tag template, its achievement (with its emblem), and what a campaign's "
             "completion unlocks once all its chapters are cleared (Ada's campaign once Leon's, Chris' and Jake's "
             "are). A clear taken back leaves those as they are.",
             &apply, &undo);
  auto forget = [] {
    std::memset(g_chap_edit, -1, sizeof(g_chap_edit));
    for (auto& cc : g_chap_time)
      for (auto& dd : cc)
        for (TimeEdit& t : dd) t.typing = false;
  };
  if (undo) forget();
  if (apply) {
    std::vector<records::ChapterSet> sets;
    for (int c = 0; c < records::kChars; ++c)
      for (int d = 0; d < records::kDifficulties; ++d)
        for (int ch = 0; ch < records::kChapters; ++ch) {
          const int8_t e = g_chap_edit[c][d][ch];
          const records::Cell& cell = v.cell[c][d][ch];
          const bool clear = will_clear(cell, e);
          const int time = clear ? typed_time(g_chap_time[c][d][ch], cell, most) : -1;
          if (e < 0 && time < 0) continue;
          records::ChapterSet s;
          s.chara = static_cast<uint8_t>(c);
          s.difficulty = static_cast<uint8_t>(d);
          s.chapter = static_cast<uint8_t>(ch);
          s.cleared = clear;
          s.rank = static_cast<uint8_t>(e > 0 ? e - 1 : cell.rank);
          s.time = time >= 0 ? static_cast<float>(time) : -1.0f;
          sets.push_back(s);
        }
    records::set_chapters(sets.data(), static_cast<int>(sets.size()));
    forget();
  }
  if (ImGui::BeginTabBar("characters")) {
    for (int c = 0; c < records::kChars; ++c) {
      int held = 0;
      for (int d = 0; d < records::kDifficulties; ++d)
        for (int ch = 0; ch < records::kChapters; ++ch) held += v.cell[c][d][ch].cleared;
      char tab[48];
      std::snprintf(tab, sizeof(tab), "%s %d###char%d", rt::kCharacter[c], held, c);
      if (ImGui::BeginTabItem(tab)) {
        g_chap_tab = c;
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
  const int c = g_chap_tab;
  char time_help[320];
  std::snprintf(time_help, sizeof(time_help),
                "Its best time, as Chapter Select shows it (h:mm:ss; m:ss or seconds work too; at most %s). A chapter "
                "cleared here gets %s - the game's own \"no time\" - unless you type one. A real clear replaces the "
                "time when it is faster.",
                most_text, most_text);
  if (ImGui::BeginTable("grid", 2 + records::kChapters, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("");
    for (int ch = 0; ch < records::kChapters; ++ch) {
      char head[24];
      std::snprintf(head, sizeof(head), "%s %d", rt::kChapter, ch + 1);
      ImGui::TableSetupColumn(head);
    }
    ImGui::TableSetupColumn("Row");
    ImGui::TableHeadersRow();
    for (int d = 0; d < records::kDifficulties; ++d) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(rt::kDifficulty[d]);
      for (int ch = 0; ch < records::kChapters; ++ch) {
        ImGui::TableNextColumn();
        int8_t& e = g_chap_edit[c][d][ch];
        const records::Cell& cell = v.cell[c][d][ch];
        int cur = e >= 0 ? e : cell_value(cell);
        char id[24];
        std::snprintf(id, sizeof(id), "##r%d%d%d", c, d, ch);
        ImGui::SetNextItemWidth(scaled(64));
        // The colour is popped by what was pushed: a pick made this frame turns it orange from the next (a pop
        // without its push is ImGui's assert, which ends the game).
        const bool orange = e >= 0;
        if (orange) ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
        if (ImGui::Combo(id, &cur, kRankItems, IM_ARRAYSIZE(kRankItems))) e = static_cast<int8_t>(cur);
        if (orange) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
          ImGui::Text("%s, %s, %s %d: %s", rt::kCharacter[c], rt::kDifficulty[d], rt::kChapter, ch + 1,
                      cell.cleared ? "cleared" : "not cleared");
          if (cell.cleared) {
            char best[16] = "";
            if (most > 0) hms(static_cast<float>(shown_time(cell, most)), best, sizeof(best));
            if (most > 0) ImGui::Text("Rank %s, best time %s", kRankItems[1 + cell.rank], best);
            else ImGui::Text("Rank %s", kRankItems[1 + cell.rank]);
          }
          if (e >= 0) ImGui::TextColored(kOrange, "Apply makes it: %s", e ? kRankItems[e] : "not cleared");
          ImGui::EndTooltip();
        }
        // Its best time, under the rank, while it is (or is to be) cleared.
        if (most > 0 && will_clear(cell, e)) {
          TimeEdit& t = g_chap_time[c][d][ch];
          if (!t.typing) hms(static_cast<float>(shown_time(cell, most)), t.text, sizeof(t.text));
          int typed = 0;
          const bool bad = t.typing && !parse_time(t.text, most, &typed);
          const bool changed = !bad && typed_time(t, cell, most) >= 0;
          std::snprintf(id, sizeof(id), "##time%d%d%d", c, d, ch);
          ImGui::SetNextItemWidth(scaled(64));
          if (bad || changed) ImGui::PushStyleColor(ImGuiCol_Text, bad ? kRed : kOrange);
          if (ImGui::InputText(id, t.text, sizeof(t.text),
                               ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_AutoSelectAll, time_chars))
            t.typing = true;
          // Left with no time in it, or the record's own: it follows the record again.
          if (ImGui::IsItemDeactivated() &&
              (!parse_time(t.text, most, &typed) || typed == shown_time(cell, most)))
            t.typing = false;
          if (bad || changed) ImGui::PopStyleColor();
          item_help(bad ? "Not a time: type it as h:mm:ss (or m:ss, or seconds)." : time_help);
        }
      }
      ImGui::TableNextColumn();
      int row = -1;
      char id[24];
      std::snprintf(id, sizeof(id), "##row%d%d", c, d);
      ImGui::SetNextItemWidth(scaled(54));
      if (ImGui::BeginCombo(id, "...")) {
        for (int k = 0; k < IM_ARRAYSIZE(kRankItems); ++k)
          if (ImGui::Selectable(kRankItems[k])) row = k;
        ImGui::EndCombo();
      }
      item_help("Pick one for every chapter of this row.");
      if (row >= 0)
        for (int ch = 0; ch < records::kChapters; ++ch) g_chap_edit[c][d][ch] = static_cast<int8_t>(row);
    }
    ImGui::EndTable();
  }
  // Every chapter at once: the rank picked, and the time typed beside it (left empty, each chapter keeps its own).
  int all_time = -1;
  const bool all_bad = most > 0 && g_chap_all_time[0] && !parse_time(g_chap_all_time, most, &all_time);
  if (all_bad) all_time = -1;
  auto pick_all = [&](int cc, int dd, int ch) {
    g_chap_edit[cc][dd][ch] = static_cast<int8_t>(g_chap_all);
    if (g_chap_all > 0 && all_time >= 0) type_time(g_chap_time[cc][dd][ch], all_time);
  };
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Every chapter:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(scaled(64));
  ImGui::Combo("##all", &g_chap_all, kRankItems, IM_ARRAYSIZE(kRankItems));
  if (most > 0) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(scaled(72));
    if (all_bad) ImGui::PushStyleColor(ImGuiCol_Text, kRed);
    ImGui::InputTextWithHint("##alltime", "time", g_chap_all_time, sizeof(g_chap_all_time),
                             ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_AutoSelectAll, time_chars);
    if (all_bad) ImGui::PopStyleColor();
    item_help(all_bad ? "Not a time: type it as h:mm:ss (or m:ss, or seconds)."
                      : "A best time for every chapter the buttons mark cleared (h:mm:ss). Left empty, each chapter "
                        "keeps its own time (one cleared here gets the game's \"no time\").");
  }
  ImGui::SameLine();
  char label[64];
  std::snprintf(label, sizeof(label), "%s's###allchar", records_text::kCharacter[c]);
  if (ImGui::SmallButton(label))
    for (int d = 0; d < records::kDifficulties; ++d)
      for (int ch = 0; ch < records::kChapters; ++ch) pick_all(c, d, ch);
  item_help("Pick the choice on the left for every chapter of this character, on every difficulty (then Apply all).");
  ImGui::SameLine();
  if (ImGui::SmallButton("Everyone's"))
    for (int cc = 0; cc < records::kChars; ++cc)
      for (int d = 0; d < records::kDifficulties; ++d)
        for (int ch = 0; ch < records::kChapters; ++ch) pick_all(cc, d, ch);
  item_help("Pick the choice on the left for every chapter of every character on every difficulty (then Apply "
            "all).");
  ImGui::TextDisabled("Orange: picked or typed, waiting for Apply all. The Agent's records are Ada's.");
}

// A category's name as a tab shows it ("THE MERCENARIES" -> "The Mercenaries").
void category_name(int c, char* out, size_t n) {
  const char* s = records_text::kMedalCategory[c];
  size_t i = 0;
  bool start = true;
  for (; s[i] && i + 1 < n; ++i) {
    const char ch = s[i];
    out[i] = start ? ch : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    start = ch == ' ' || ch == '(';
  }
  out[i] = '\0';
}

void draw_medals(const records::View& v) {
  // The tab follows the category the game's page shows, as it changes there.
  bool follow = false;
  if (v.medal_cat != g_medal_game && v.medal_cat >= 0 && v.medal_cat < records::kMedalCats) {
    g_medal_game = v.medal_cat;
    g_medal_tab = v.medal_cat;
    follow = true;
  }
  int pending = 0;
  for (int c = 0; c < records::kMedalCats; ++c)
    for (int i = 0; i < v.medal[c].n; ++i) {
      MedalEdit& e = g_medal_edit[c][i];
      if (!e.typing) e.v = v.medal[c].count[i];
      else if (e.v == v.medal[c].count[i]) e.typing = false;
      pending += e.typing;
    }
  bool apply = false, undo = false;
  apply_line(pending, "Make every count typed below, in every category, and save your profile.", &apply, &undo);
  if (undo)
    for (auto& c : g_medal_edit)
      for (MedalEdit& e : c) e.typing = false;
  if (apply) {
    std::vector<records::MedalSet> sets;
    for (int c = 0; c < records::kMedalCats; ++c)
      for (int i = 0; i < v.medal[c].n; ++i) {
        MedalEdit& e = g_medal_edit[c][i];
        if (!e.typing) continue;
        records::MedalSet s;
        s.cat = static_cast<uint8_t>(c);
        s.medal = v.medal[c].id[i];
        s.count = static_cast<uint8_t>(e.v);
        sets.push_back(s);
        e.typing = false;
      }
    records::set_medals(sets.data(), static_cast<int>(sets.size()));
  }
  ImGui::SetNextItemWidth(scaled(220));
  ImGui::InputTextWithHint("##q", "search", g_medal_query, sizeof(g_medal_query));
  if (ImGui::BeginTabBar("categories")) {
    for (int c = 0; c < records::kMedalCats; ++c) {
      if (!v.medal[c].shown) continue;
      int held = 0;
      for (int i = 0; i < v.medal[c].n; ++i) held += v.medal[c].count[i] > 0;
      char name[48], tab[64];
      category_name(c, name, sizeof(name));
      std::snprintf(tab, sizeof(tab), "%s %d/%d###cat%d", name, held, v.medal[c].n, c);
      if (ImGui::BeginTabItem(tab, nullptr, follow && c == g_medal_tab ? ImGuiTabItemFlags_SetSelected : 0)) {
        if (!follow) g_medal_tab = c;
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
  const int c = g_medal_tab;
  if (c < 0 || c >= records::kMedalCats || !v.medal[c].shown) return;
  std::vector<int> listed;
  if (ImGui::BeginChild("medals", ImVec2(0, -footer(1)), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("list", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("medal", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, scaled(96));
      ImGui::TableSetupColumn("do", ImGuiTableColumnFlags_WidthFixed, scaled(96));
      for (int i = 0; i < v.medal[c].n; ++i) {
        const char* name = records::medal_name(v, c, v.medal[c].id[i]);
        if (!matches_query(name, g_medal_query)) continue;
        listed.push_back(i);
        MedalEdit& e = g_medal_edit[c][i];
        ImGui::PushID(c * 256 + i);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(name[0] ? name : "(no name)");
        ImGui::PopTextWrapPos();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(scaled(48));
        // As the chapters' cells: the first key typed turns it orange from the next frame.
        const bool orange = e.typing;
        if (orange) ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
        if (ImGui::InputInt("##n", &e.v, 0, 0)) e.typing = true;
        if (orange) ImGui::PopStyleColor();
        if (e.v < 0) e.v = 0;
        if (e.v > records::kMedalMost) e.v = records::kMedalMost;
        ImGui::SameLine();
        ImGui::TextDisabled("/%d", records::kMedalMost);
        ImGui::TableNextColumn();
        if (e.typing) {
          if (ImGui::SmallButton("Apply")) {
            records::MedalSet s;
            s.cat = static_cast<uint8_t>(c);
            s.medal = v.medal[c].id[i];
            s.count = static_cast<uint8_t>(e.v);
            records::set_medals(&s, 1);
            e.typing = false;
          }
          ImGui::SameLine();
          if (ImGui::SmallButton("Undo")) e.typing = false;
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Every medal listed:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(scaled(48));
  ImGui::InputInt("##allm", &g_medal_all, 0, 0);
  if (g_medal_all < 0) g_medal_all = 0;
  if (g_medal_all > records::kMedalMost) g_medal_all = records::kMedalMost;
  ImGui::SameLine();
  char label[48];
  numbered(label, sizeof(label), "Type in", static_cast<int>(listed.size()), "allmedals");
  if (ImGui::SmallButton(label))
    for (int i : listed) g_medal_edit[c][i] = MedalEdit{g_medal_all, g_medal_all != v.medal[c].count[i]};
  item_help("Type this count into every medal the list shows (the search narrows it), then Apply all.");
}

void draw_emblems(const records::View& v) {
  namespace rt = records_text;
  int unlocked = 0, ticked = 0;
  for (int e = 0; e < records::kEmblems; ++e) {
    unlocked += v.emblem[e] != 0;
    if (v.emblem[e]) g_emblem_tick[e] = false;
    ticked += g_emblem_tick[e];
  }
  ImGui::Text("Unlocked %d of %d", unlocked, records::kEmblems);
  ImGui::SameLine(0, scaled(24));
  char label[48];
  ImGui::BeginDisabled(!ticked);
  numbered(label, sizeof(label), "Award", ticked, "award");
  if (ImGui::Button(label)) {
    std::vector<uint8_t> ids;
    for (int e = 0; e < records::kEmblems; ++e)
      if (g_emblem_tick[e]) ids.push_back(static_cast<uint8_t>(e));
    records::award_emblems(ids.data(), static_cast<int>(ids.size()));
    std::memset(g_emblem_tick, 0, sizeof(g_emblem_tick));
  }
  ImGui::EndDisabled();
  item_help("Award every ticked emblem: its achievement is given through the game's own call, as earning it does - "
            "the emblem unlocks, and so does the achievement on Steam (Steam does not take one back). Your profile is "
            "saved.");
  ImGui::SameLine();
  if (ImGui::SmallButton("Tick all locked"))
    for (int e = 0; e < records::kEmblems; ++e) g_emblem_tick[e] = !v.emblem[e];
  ImGui::SameLine();
  ImGui::Checkbox("Locked only", &g_emblem_locked_only);
  if (!v.achievements)
    ImGui::TextColored(kOrange, "The game's achievement call was not found: an award sets the emblem only.");
  if (ImGui::BeginChild("emblems", ImVec2(0, -footer(0)), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("list", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(28));
      ImGui::TableSetupColumn("achievement", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed, scaled(118));
      for (int e = 0; e < records::kEmblems && e < rt::kAchievements; ++e) {
        if (g_emblem_locked_only && v.emblem[e]) continue;
        ImGui::PushID(e);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(v.emblem[e] != 0);
        ImGui::Checkbox("##t", &g_emblem_tick[e]);
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        ImGui::Text("%d. %s", e + 1, rt::kAchievement[e].name);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", rt::kAchievement[e].desc);
        ImGui::PopTextWrapPos();
        ImGui::TableNextColumn();
        if (v.emblem[e]) {
          ImGui::TextUnformatted(v.emblem[e] == 1 ? "Unlocked (new)" : "Unlocked");
        } else if (ImGui::SmallButton("Award")) {
          const uint8_t id = static_cast<uint8_t>(e);
          records::award_emblems(&id, 1);
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
}

// A family's level marks: "**--" (held, then not).
void level_marks(const records::TitleRow& t, char* out, size_t n) {
  size_t i = 0;
  for (int k = 0; k < t.nlevels && i + 1 < n; ++k) out[i++] = t.level[k].level <= t.earned ? '*' : '-';
  out[i] = '\0';
}

const records::Level* next_level(const records::TitleRow& t) {
  for (int k = 0; k < t.nlevels; ++k)
    if (t.level[k].level > t.earned) return &t.level[k];
  return nullptr;
}

const records::Level* held_level(const records::TitleRow& t) {
  const records::Level* held = nullptr;
  for (int k = 0; k < t.nlevels; ++k)
    if (t.level[k].level == t.earned) held = &t.level[k];
  return held;
}

const char* title_name(const records::TitleRow& t) {
  // The held title's name (a family without stars names each level), else its first.
  const records::Level* held = held_level(t);
  const records::Level& l = held ? *held : t.level[0];
  return l.msg < records_text::kTitleMsgs ? records_text::kTitleName[l.msg] : "";
}

void draw_titles(const records::View& v) {
  namespace rt = records_text;
  int held = 0, ticked = 0, behind = 0;
  for (int i = 0; i < v.ntitles; ++i) {
    const records::TitleRow& t = v.title[i];
    held += t.earned > 0;
    behind += t.behind;
    if (!next_level(t)) g_title_tick[t.counter] = false;
    ticked += g_title_tick[t.counter];
  }
  ImGui::Text("Held %d of %d", held, v.ntitles);
  ImGui::SameLine(0, scaled(24));
  char label[48];
  ImGui::BeginDisabled(!ticked);
  numbered(label, sizeof(label), "Award to the top", ticked, "award");
  if (ImGui::Button(label)) {
    std::vector<records::TitleSet> sets;
    for (int i = 0; i < v.ntitles; ++i) {
      const records::TitleRow& t = v.title[i];
      if (!g_title_tick[t.counter] || !t.nlevels) continue;
      records::TitleSet s;
      s.counter = t.counter;
      s.level = t.level[t.nlevels - 1].level;
      sets.push_back(s);
    }
    records::award_titles(sets.data(), static_cast<int>(sets.size()));
    std::memset(g_title_tick, 0, sizeof(g_title_tick));
  }
  ImGui::EndDisabled();
  item_help("Award every ticked title up to its family's last level (as the game would once each goal was met: its "
            "count is brought up to what the title needs, so the game shows what earns it), and save your profile. A "
            "level-4 title gives \"Rising Up\", and ten titles held \"Titular Achievement\" - through the game's own "
            "call, on Steam too.");
  ImGui::SameLine();
  ImGui::Checkbox("Not complete only", &g_title_open_only);
  // Titles held with their count short of what they need (awarded before an award brought the count up): the game
  // shows "? ? ?" for what earns them, and its list's page buttons skip a page where no title has a count.
  if (behind) {
    if (ImGui::SmallButton("Bring their counts up###counts")) {
      std::vector<records::TitleSet> sets;
      for (int i = 0; i < v.ntitles; ++i)
        if (v.title[i].behind) {
          records::TitleSet s;
          s.counter = v.title[i].counter;
          s.level = v.title[i].earned;
          sets.push_back(s);
        }
      records::award_titles(sets.data(), static_cast<int>(sets.size()));
    }
    item_help("These titles are held with their count short of what they need (awarded by the mod before it brought "
              "the count up with the title). The game shows what earns a title, and goes to its page of the list, only "
              "once its count has started: this brings each count up to what its held title needs (never down), as "
              "earning it would have, and saves your profile.");
    ImGui::SameLine();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kOrange, "%d held title(s) show \"? ? ?\" in the game for what earns them.", behind);
    ImGui::PopTextWrapPos();
  }
  ImGui::SetNextItemWidth(scaled(220));
  ImGui::InputTextWithHint("##q", "search", g_title_query, sizeof(g_title_query));
  std::vector<int> listed;
  ImGui::SameLine();
  const bool tick_listed = ImGui::SmallButton("Tick all listed");
  if (ImGui::BeginChild("titles", ImVec2(0, -footer(0)), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("list", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(28));
      ImGui::TableSetupColumn("title", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableSetupColumn("next", ImGuiTableColumnFlags_WidthStretch, 1.4f);
      ImGui::TableSetupColumn("do", ImGuiTableColumnFlags_WidthFixed, scaled(96));
      for (int i = 0; i < v.ntitles; ++i) {
        const records::TitleRow& t = v.title[i];
        const records::Level* next = next_level(t);
        if (g_title_open_only && !next) continue;
        const char* name = title_name(t);
        const char* need = next && next->msg < rt::kTitleMsgs ? rt::kTitleNeed[next->msg] : "";
        if (g_title_query[0] && !matches_query(name, g_title_query) && !matches_query(need, g_title_query)) {
          bool any = false;
          for (int k = 0; k < t.nlevels && !any; ++k)
            any = t.level[k].msg < rt::kTitleMsgs && matches_query(rt::kTitleName[t.level[k].msg], g_title_query);
          if (!any) continue;
        }
        if (next && tick_listed) g_title_tick[t.counter] = true;
        ImGui::PushID(t.counter);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (next) ImGui::Checkbox("##t", &g_title_tick[t.counter]);  // a title held at every level: nothing to tick
        ImGui::TableNextColumn();
        char marks[8];
        level_marks(t, marks, sizeof(marks));
        if (t.earned) ImGui::Text("%s", name);
        else ImGui::TextDisabled("%s", name);
        if (t.nlevels > 1 || t.stars) {
          ImGui::SameLine();
          ImGui::TextDisabled("[%s]", marks);
        }
        if (ImGui::BeginItemTooltip()) {
          for (int k = 0; k < t.nlevels; ++k) {
            const records::Level& l = t.level[k];
            const char* ln = l.msg < rt::kTitleMsgs ? rt::kTitleName[l.msg] : "";
            const char* lneed = l.msg < rt::kTitleMsgs ? rt::kTitleNeed[l.msg] : "";
            if (l.level <= t.earned) ImGui::Text("%d. %s - held", l.level, ln);
            else ImGui::TextDisabled("%d. %s", l.level, ln);
            ImGui::TextDisabled("    %s (the game counts %u of %u)", lneed, t.progress, l.goal);
          }
          ImGui::EndTooltip();
        }
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        if (next) {
          ImGui::TextDisabled("%s  (%u/%u)", need, t.progress < next->goal ? t.progress : next->goal, next->goal);
        } else {
          // All held: what earned the last one.
          const records::Level* top = held_level(t);
          const char* got = top && top->msg < rt::kTitleMsgs ? rt::kTitleNeed[top->msg] : "";
          if (got[0]) ImGui::TextDisabled("%s  (all held)", got);
          else ImGui::TextDisabled("All held.");
        }
        ImGui::PopTextWrapPos();
        ImGui::TableNextColumn();
        if (next) {
          if (ImGui::SmallButton(t.nlevels > 1 && next->level < t.level[t.nlevels - 1].level ? "Next" : "Award")) {
            records::TitleSet s;
            s.counter = t.counter;
            s.level = next->level;
            records::award_titles(&s, 1);
          }
          item_help("Award the next level (the one whose need is shown); its count is brought up to that need.");
          if (t.nlevels > 1 && next->level < t.level[t.nlevels - 1].level) {
            ImGui::SameLine();
            if (ImGui::SmallButton("All")) {
              records::TitleSet s;
              s.counter = t.counter;
              s.level = t.level[t.nlevels - 1].level;
              records::award_titles(&s, 1);
            }
            item_help("Award every level up to the family's last.");
          }
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
}

// "12,000".
const char* thousands(uint32_t v, char* out, size_t n) {
  char digits[16];
  const int len = std::snprintf(digits, sizeof(digits), "%u", v);
  size_t o = 0;
  for (int i = 0; i < len && o + 1 < n; ++i) {
    if (i && (len - i) % 3 == 0 && o + 2 < n) out[o++] = ',';
    out[o++] = digits[i];
  }
  out[o] = '\0';
  return out;
}

bool skill_unlocked(const records::Skill& k) { return k.state != 0 || k.met; }
// A tick that can never change: a skill bought, or one the game sells by its own count.
bool skill_fixed(const records::Skill& k) { return k.state == 3 || k.met; }

// The skills window beside the skill screen: each skill its list sells only after a requirement, with what the
// game asks and how far the profile is, a tick that unlocks it (or locks it again), and Unlock all / Lock all.
void draw_skills(const records::View& v) {
  namespace rt = records_text;
  const DWORD now = GetTickCount();
  int unlocked = 0, bought = 0, can_unlock = 0, can_lock = 0;
  for (int i = 0; i < v.nskills; ++i) {
    const records::Skill& k = v.skill[i];
    unlocked += skill_unlocked(k);
    bought += k.state == 3;
    can_unlock += !skill_unlocked(k);
    can_lock += k.state != 0 && !skill_fixed(k);
    SkillPending& p = g_skill_pending[k.id < rt::kSkillMsgs ? k.id : 0];
    if (p.on >= 0 && (static_cast<bool>(p.on) == skill_unlocked(k) || now - p.at > 1500)) p = SkillPending{};
  }
  ImGui::Text("Unlocked %d of %d", unlocked, v.nskills);
  ImGui::SameLine();
  ImGui::TextDisabled("\xC2\xB7");
  ImGui::SameLine();
  ImGui::Text("Bought %d", bought);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  item_help("Tick a skill and it is unlocked: the skill screen sells it, and you buy it there with your skill points "
            "as usual - nothing is bought here, and your skill points stay as they are. Untick it to lock it again "
            "(not one you have bought, nor one whose requirement you have met: the game sells those by its own "
            "count).\n\nThe skill screen's list shows each change at once. Your profile keeps it when you leave the "
            "skill screen, as it keeps your purchases. A skill with levels sells the next level once you own the "
            "one before, as it always does.");
  auto ask_all = [&](bool on) {
    std::vector<records::SkillSet> sets;
    for (int i = 0; i < v.nskills; ++i) {
      const records::Skill& k = v.skill[i];
      if (on ? !skill_unlocked(k) : k.state != 0 && !skill_fixed(k)) {
        sets.push_back(records::SkillSet{k.id, on});
        if (k.id < rt::kSkillMsgs) g_skill_pending[k.id] = SkillPending{static_cast<int8_t>(on ? 1 : 0), now};
      }
    }
    records::set_skills(sets.data(), static_cast<int>(sets.size()));
  };
  char label[48];
  ImGui::BeginDisabled(!can_unlock);
  numbered(label, sizeof(label), "Unlock all", can_unlock, "unlockall");
  if (ImGui::Button(label)) ask_all(true);
  ImGui::EndDisabled();
  item_help("Unlock every skill listed that is locked: the skill screen then sells each one for skill points.");
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_lock);
  numbered(label, sizeof(label), "Lock all", can_lock, "lockall");
  if (ImGui::Button(label)) ask_all(false);
  ImGui::EndDisabled();
  item_help("Lock again every skill listed that is unlocked - not one you have bought, nor one whose requirement you "
            "have met.");
  if (ImGui::BeginChild("skills", ImVec2(0, -footer(0)), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("list", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(28));
      ImGui::TableSetupColumn("skill", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("price", ImGuiTableColumnFlags_WidthFixed, scaled(124));
      ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed, scaled(128));
      for (int i = 0; i < v.nskills; ++i) {
        const records::Skill& k = v.skill[i];
        const int id = k.id < rt::kSkillMsgs ? k.id : 0;
        ImGui::PushID(id);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool on = skill_unlocked(k);
        if (g_skill_pending[id].on >= 0) on = g_skill_pending[id].on != 0;
        ImGui::BeginDisabled(skill_fixed(k));
        if (ImGui::Checkbox("##s", &on)) {
          const records::SkillSet set{k.id, on};
          records::set_skills(&set, 1);
          g_skill_pending[id] = SkillPending{static_cast<int8_t>(on ? 1 : 0), now};
        }
        ImGui::EndDisabled();
        if (k.state == 3) item_help("Bought - it stays unlocked.");
        else if (k.met) item_help("Its requirement is met: the game sells it by its own count.");
        ImGui::TableNextColumn();
        if (skill_unlocked(k)) ImGui::TextUnformatted(rt::kSkillName[id]);
        else ImGui::TextDisabled("%s", rt::kSkillName[id]);
        if (rt::kSkillDesc[id][0]) item_help(rt::kSkillDesc[id]);
        char count[16], goal[16];
        thousands(static_cast<uint32_t>(k.count < k.goal ? (k.count > 0 ? k.count : 0) : k.goal), count, sizeof(count));
        thousands(static_cast<uint32_t>(k.goal > 0 ? k.goal : 0), goal, sizeof(goal));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s  (%s/%s)", rt::kSkillNeed[id], count, goal);
        ImGui::PopTextWrapPos();
        item_help(k.need == records::Need::kCampaigns
                      ? "The campaigns you have completed (every chapter, on any difficulty), as the game counts them."
                      : "Your kills so far, as the game counts them (the Records screen's).");
        ImGui::TableNextColumn();
        char price[24];
        ImGui::Text("%s points", thousands(k.price, price, sizeof(price)));
        item_help("Its price on the skill screen, in skill points.");
        ImGui::TableNextColumn();
        if (k.state == 3) ImGui::TextUnformatted("Bought");
        else if (skill_unlocked(k)) ImGui::TextUnformatted("For sale");
        else ImGui::TextDisabled("Locked");
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
}

// A WEAPONS page row's name, as the page draws it (its own message), or the item's.
const char* weapon_row_name(int entry, uint16_t id) {
  namespace rt = records_text;
  if (entry >= 0 && entry < rt::kWeaponMsgs && rt::kWeaponName[entry][0]) return rt::kWeaponName[entry];
  for (const items::Info& it : items::kItems)
    if (it.id == id) return it.name;
  return "?";
}

// The weapons window beside the Records screen's WEAPONS page. Above, the weapons the campaigns give on the way: a
// row each with its page tick, and a tick for each character whose campaign has it (in that character's saved
// inventory or not); each character's count and All, each weapon's All, and one for every weapon. Below, the page's
// other rows that can show "? ? ?", with their page tick only.
void draw_weapons(const records::View& v) {
  namespace rt = records_text;
  const DWORD now = GetTickCount();
  auto settle = [&](SkillPending& p, bool game) {
    if (p.on >= 0 && (static_cast<bool>(p.on) == game || now - p.at > 1500)) p = SkillPending{};
    return p.on >= 0 ? p.on != 0 : game;
  };
  // The ticks as shown: the game's state, or what was clicked until the game says so.
  int page_of[records::kMaxWeaponRows];
  std::fill(std::begin(page_of), std::end(page_of), -1);
  bool named[records::kMaxWeaponRows] = {};
  int nnamed = 0;
  for (int i = 0; i < v.nweapon_rows; ++i) {
    const records::WeaponRow& r = v.weapon_row[i];
    page_of[r.entry] = i;
    named[i] = settle(g_row_pending[r.entry], r.uses > 0);
    nnamed += named[i];
  }
  bool had[records::kCarriers][records::kMaxCarried] = {};
  int nhad[records::kCarriers] = {}, nmissing = 0;
  for (int c = 0; c < records::kCarriers; ++c)
    for (int k = 0; k < v.carrier[c].n; ++k) {
      had[c][k] = settle(g_carry_pending[c][k], v.carrier[c].weapon[k].had);
      nhad[c] += had[c][k];
      nmissing += v.carrier[c].ready && !had[c][k];
    }
  // The grid's rows: each weapon a campaign gives, in the page's order, with the place in each character's list.
  struct Grid {
    uint16_t id;
    int entry;
    int8_t cell[records::kCarriers];
  };
  std::vector<Grid> grid;
  bool in_grid[records::kMaxWeaponRows] = {};
  for (int c = 0; c < records::kCarriers && v.carried_known; ++c)
    for (int k = 0; k < v.carrier[c].n; ++k) {
      const records::Carried& w = v.carrier[c].weapon[k];
      auto it = std::find_if(grid.begin(), grid.end(), [&](const Grid& g) { return g.id == w.id; });
      if (it == grid.end()) {
        Grid g{w.id, w.entry, {}};
        std::fill(std::begin(g.cell), std::end(g.cell), static_cast<int8_t>(-1));
        grid.push_back(g);
        it = grid.end() - 1;
      }
      it->cell[c] = static_cast<int8_t>(k);
      if (w.entry >= 0 && w.entry < records::kMaxWeaponRows) in_grid[w.entry] = true;
    }
  std::stable_sort(grid.begin(), grid.end(), [](const Grid& a, const Grid& b) {
    return (a.entry < 0 ? 1000 : a.entry) < (b.entry < 0 ? 1000 : b.entry);
  });
  // What a click asks for: a row named (or back to "? ? ?"), a weapon put in (or taken out of) a saved inventory.
  std::vector<records::WeaponSet> rows;
  std::vector<records::CarrySet> carry;
  auto row = [&](int i, bool on) {
    rows.push_back(records::WeaponSet{v.weapon_row[i].entry, on});
    g_row_pending[v.weapon_row[i].entry] = SkillPending{static_cast<int8_t>(on ? 1 : 0), now};
  };
  auto give = [&](int c, int k, bool on) {
    carry.push_back(records::CarrySet{static_cast<uint8_t>(c), v.carrier[c].weapon[k].id, on});
    g_carry_pending[c][k] = SkillPending{static_cast<int8_t>(on ? 1 : 0), now};
  };
  auto page_help = [&](int i) {
    const records::WeaponRow& r = v.weapon_row[i];
    char help[256];
    if (named[i] && r.played)
      std::snprintf(help, sizeof(help), "Used in play (%s %u, %s %u): the game's own count, kept.", rt::kWeaponUses,
                    r.uses, rt::kWeaponKills, r.kills);
    else if (named[i])
      std::snprintf(help, sizeof(help), "Named on the WEAPONS page (%s %u). Untick: back to \"%s\".", rt::kWeaponUses,
                    r.uses, rt::kUnknown);
    else
      std::snprintf(help, sizeof(help), "\"%s\" on the WEAPONS page. Tick: named (its first use, %s 1).", rt::kUnknown,
                    rt::kWeaponUses);
    item_help(help);
  };
  // A row's page tick (a row the page always names: none).
  auto page_tick = [&](int entry) {
    const int i = entry >= 0 && entry < records::kMaxWeaponRows ? page_of[entry] : -1;
    if (i < 0) {
      ImGui::TextDisabled("%s", entry >= 0 ? "always" : "-");
      item_help(entry >= 0 ? "The WEAPONS page always names this one." : "Not a row of the WEAPONS page.");
      return;
    }
    bool on = named[i];
    ImGui::BeginDisabled(named[i] && v.weapon_row[i].played);
    if (ImGui::Checkbox("##page", &on)) row(i, on);
    ImGui::EndDisabled();
    page_help(i);
  };

  ImGui::Text("Named %d of %d", nnamed, v.nweapon_rows);
  if (v.carried_known) {
    ImGui::SameLine();
    ImGui::TextDisabled("\xC2\xB7");
    ImGui::SameLine();
    int have = 0, all = 0;
    for (int c = 0; c < records::kCarriers; ++c) {
      have += nhad[c];
      all += v.carrier[c].n;
    }
    ImGui::Text("Weapons had %d of %d", have, all);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  item_help("The WEAPONS page names a row once the game has counted one use of it, and shows \"? ? ?\" until then "
            "(each character's first weapons, the tablets and the spray it always names). A Page tick gives a row its "
            "first use (Times Used 1, as one shot that missed leaves it); untick takes that use back - not for a row "
            "used in play, whose count is the game's.\n\nOwning a weapon is each character's own saved inventory, "
            "which Continue, New Game and Chapter Select start from: a character's tick puts the weapon in it (a "
            "full magazine), untick takes it out (its loaded ammo with it). A weapon from another campaign is not "
            "offered - the game takes it away at the next area. An AI partner is handed the story's weapons at each "
            "area whatever it has saved.\n\nYour profile is saved after each change.");
  char label[64];
  ImGui::BeginDisabled(!nmissing);
  numbered(label, sizeof(label), "Give every weapon", nmissing, "giveall");
  if (ImGui::Button(label))
    for (int c = 0; c < records::kCarriers; ++c)
      for (int k = 0; k < v.carrier[c].n; ++k)
        if (v.carrier[c].ready && !had[c][k]) give(c, k, true);
  ImGui::EndDisabled();
  item_help("Put every weapon each character's campaign gives in that character's saved inventory.");
  ImGui::SameLine();
  ImGui::BeginDisabled(nnamed == v.nweapon_rows);
  numbered(label, sizeof(label), "Name every row", v.nweapon_rows - nnamed, "nameall");
  if (ImGui::Button(label))
    for (int i = 0; i < v.nweapon_rows; ++i)
      if (!named[i]) row(i, true);
  ImGui::EndDisabled();
  item_help("Give every \"? ? ?\" row of the WEAPONS page its first use, so the page names them all.");
  if (!v.carried_known)
    ImGui::TextColored(kOrange, "The characters' saved inventories were not understood: the WEAPONS page only.");
  if (ImGui::BeginChild("weapons", ImVec2(0, -footer(0)), ImGuiChildFlags_Borders)) {
    if (!grid.empty()) {
      ImGui::SeparatorText("What the campaigns give on the way");
      const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
      if (ImGui::BeginTable("carried", 3 + records::kCarriers, flags)) {
        ImGui::TableSetupColumn("Weapon", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Page");
        for (int c = 0; c < records::kCarriers; ++c) ImGui::TableSetupColumn(rt::kCharacter[c]);
        ImGui::TableSetupColumn("##all");
        ImGui::TableHeadersRow();
        // Each character's weapons had, and All for the rest.
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("Had");
        ImGui::TableNextColumn();
        for (int c = 0; c < records::kCarriers; ++c) {
          ImGui::TableNextColumn();
          const records::Carrier& k = v.carrier[c];
          if (!k.ready) {
            ImGui::TextDisabled("-");
            item_help("Its saved inventory is not set up yet: the game sets it up at a game's start.");
            continue;
          }
          ImGui::Text("%d/%d", nhad[c], k.n);
          std::snprintf(label, sizeof(label), "All###char%d", c);
          ImGui::BeginDisabled(nhad[c] == k.n);
          if (ImGui::SmallButton(label))
            for (int i = 0; i < k.n; ++i)
              if (!had[c][i]) give(c, i, true);
          ImGui::EndDisabled();
          item_help("Put each weapon this character's campaign gives in its saved inventory.");
        }
        ImGui::TableNextColumn();
        for (const Grid& g : grid) {
          ImGui::PushID(g.id);
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(weapon_row_name(g.entry, g.id));
          ImGui::TableNextColumn();
          page_tick(g.entry);
          int todo = 0;
          const int pi = g.entry >= 0 && g.entry < records::kMaxWeaponRows ? page_of[g.entry] : -1;
          todo += pi >= 0 && !named[pi];
          for (int c = 0; c < records::kCarriers; ++c) {
            ImGui::TableNextColumn();
            const int k = g.cell[c];
            if (k < 0) continue;
            todo += v.carrier[c].ready && !had[c][k];
            bool on = had[c][k];
            std::snprintf(label, sizeof(label), "##c%d", c);
            ImGui::BeginDisabled(!v.carrier[c].ready);
            if (ImGui::Checkbox(label, &on)) give(c, k, on);
            ImGui::EndDisabled();
            const records::Carried& w = v.carrier[c].weapon[k];
            char help[160];
            if (!v.carrier[c].ready)
              std::snprintf(help, sizeof(help), "%s's saved inventory is not set up yet.", rt::kCharacter[c]);
            else if (had[c][k] && w.count >= 0)
              std::snprintf(help, sizeof(help), "%s has it (%d loaded). Untick: taken out, its ammo with it.",
                            rt::kCharacter[c], w.count);
            else if (had[c][k])
              std::snprintf(help, sizeof(help), "%s has it. Untick: taken out of the saved inventory.", rt::kCharacter[c]);
            else
              std::snprintf(help, sizeof(help), "Not in %s's saved inventory. Tick: given, a full magazine.",
                            rt::kCharacter[c]);
            item_help(help);
          }
          ImGui::TableNextColumn();
          ImGui::BeginDisabled(!todo);
          if (ImGui::SmallButton("All###row")) {
            if (pi >= 0 && !named[pi]) row(pi, true);
            for (int c = 0; c < records::kCarriers; ++c)
              if (g.cell[c] >= 0 && v.carrier[c].ready && !had[c][g.cell[c]]) give(c, g.cell[c], true);
          }
          ImGui::EndDisabled();
          item_help("Give it to every character whose campaign has it, and name it on the WEAPONS page.");
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
    }
    ImGui::SeparatorText(grid.empty() ? "The WEAPONS page" : "The WEAPONS page's other rows");
    if (ImGui::BeginTable("rows", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(28));
      ImGui::TableSetupColumn("row", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("counts", ImGuiTableColumnFlags_WidthFixed, scaled(240));
      for (int i = 0; i < v.nweapon_rows; ++i) {
        const records::WeaponRow& r = v.weapon_row[i];
        if (in_grid[r.entry]) continue;
        ImGui::PushID(0x1000 + r.entry);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        page_tick(r.entry);
        ImGui::TableNextColumn();
        if (named[i]) ImGui::TextUnformatted(weapon_row_name(r.entry, 0));
        else ImGui::TextDisabled("%s  (%s)", weapon_row_name(r.entry, 0), rt::kUnknown);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s %u, %s %u", rt::kWeaponUses, r.uses, rt::kWeaponKills, r.kills);
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (v.nweapon_always)
      ImGui::TextDisabled("%d rows the page always names (each character's first weapons, the tablets, the spray) "
                          "are not listed.",
                          v.nweapon_always);
  }
  ImGui::EndChild();
  if (!rows.empty()) records::set_weapon_rows(rows.data(), static_cast<int>(rows.size()));
  if (!carry.empty()) records::set_carried(carry.data(), static_cast<int>(carry.size()));
}

// The enemies window beside the Records screen's ENEMIES page: each row the page can draw "? ? ?", with its counts
// and a tick that names it (its first kill: Enemies Defeated 1) - never taken back -, and Reveal all.
void draw_enemies(const records::View& v) {
  namespace rt = records_text;
  const DWORD now = GetTickCount();
  // The ticks as shown: named by the game's own test (a count above 0), or clicked until the game says so.
  bool named[records::kMaxEnemyRows] = {};
  int nnamed = 0;
  for (int i = 0; i < v.nenemy_rows; ++i) {
    const records::EnemyRow& r = v.enemy_row[i];
    const bool game = r.kills > 0 || r.defeated > 0;
    SkillPending& p = g_enemy_pending[r.entry < records::kMaxEnemyRows ? r.entry : 0];
    if (p.on >= 0 && (game || now - p.at > 1500)) p = SkillPending{};
    named[i] = game || p.on > 0;
    nnamed += named[i];
  }
  std::vector<uint8_t> reveal;
  auto ask = [&](int i) {
    const uint8_t e = v.enemy_row[i].entry;
    reveal.push_back(e);
    if (e < records::kMaxEnemyRows) g_enemy_pending[e] = SkillPending{1, now};
  };
  ImGui::Text("Named %d of %d", nnamed, v.nenemy_rows);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  item_help("The ENEMIES page names an enemy once you have defeated one (or one has defeated you), and shows \"? ? ?\" "
            "until then (J'avo and Zombie it always names). Tick a row, or Reveal all, and it is named with one kill "
            "counted (Enemies Defeated 1). A row the page names already keeps its counts, and a row once named stays "
            "named.\n\nYour profile is saved after each change.");
  char label[64];
  ImGui::BeginDisabled(nnamed == v.nenemy_rows);
  numbered(label, sizeof(label), "Reveal all", v.nenemy_rows - nnamed, "revealall");
  if (ImGui::Button(label))
    for (int i = 0; i < v.nenemy_rows; ++i)
      if (!named[i]) ask(i);
  ImGui::EndDisabled();
  item_help("Name every \"? ? ?\" row of the ENEMIES page: each gets one kill (Enemies Defeated 1).");
  ImGui::SameLine();
  ImGui::Checkbox("\"? ? ?\" only###hiddenonly", &g_enemy_hidden_only);
  item_help("List only the rows the ENEMIES page shows as \"? ? ?\".");
  if (ImGui::BeginChild("enemies", ImVec2(0, -footer(0)), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("rows", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(28));
      ImGui::TableSetupColumn("enemy", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("counts", ImGuiTableColumnFlags_WidthFixed, scaled(290));
      for (int i = 0; i < v.nenemy_rows; ++i) {
        const records::EnemyRow& r = v.enemy_row[i];
        if (g_enemy_hidden_only && named[i]) continue;
        ImGui::PushID(r.entry);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool on = named[i];
        ImGui::BeginDisabled(named[i]);
        if (ImGui::Checkbox("##e", &on) && on) ask(i);
        ImGui::EndDisabled();
        char help[160];
        if (r.kills > 0)
          std::snprintf(help, sizeof(help), "Named on the ENEMIES page (%s %d).", rt::kWeaponKills, r.kills);
        else if (r.defeated > 0)
          std::snprintf(help, sizeof(help), "Named on the ENEMIES page: it has defeated you (%s %d).", rt::kEnemyDefeated,
                        r.defeated);
        else if (named[i])
          std::snprintf(help, sizeof(help), "Named (its first kill, %s 1).", rt::kWeaponKills);
        else
          std::snprintf(help, sizeof(help), "\"%s\" on the ENEMIES page. Tick: named (its first kill, %s 1).",
                        rt::kUnknown, rt::kWeaponKills);
        item_help(help);
        ImGui::TableNextColumn();
        const char* name = r.entry < rt::kEnemyMsgs && rt::kEnemyName[r.entry][0] ? rt::kEnemyName[r.entry] : "?";
        if (named[i]) ImGui::TextUnformatted(name);
        else ImGui::TextDisabled("%s  (%s)", name, rt::kUnknown);
        ImGui::TableNextColumn();
        char kills[16], defeated[16];
        thousands(static_cast<uint32_t>(r.kills > 0 ? r.kills : 0), kills, sizeof(kills));
        thousands(static_cast<uint32_t>(r.defeated > 0 ? r.defeated : 0), defeated, sizeof(defeated));
        if (r.shows_defeated) ImGui::TextDisabled("%s %s, %s %s", rt::kWeaponKills, kills, rt::kEnemyDefeated, defeated);
        else ImGui::TextDisabled("%s %s", rt::kWeaponKills, kills);
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (v.nenemy_always)
      ImGui::TextDisabled("%d rows the page always names (J'avo, Zombie) are not listed.", v.nenemy_always);
  }
  ImGui::EndChild();
  if (!reveal.empty()) records::reveal_enemies(reveal.data(), static_cast<int>(reveal.size()));
}

// The Mercenaries window beside the stage select and the character select (the Mercenaries' or No Mercy's): the
// mode's stages, each with a tick that opens it (or closes it again), and the characters with a tick for each
// costume that unlocks it (or locks it again) - one set both modes share. The ones the game always gives: ticked,
// fixed. Open every stage, Unlock every costume.
void draw_merc(const records::View& v) {
  namespace rt = records_text;
  const DWORD now = GetTickCount();
  auto settle = [&](SkillPending& p, bool game) {
    if (p.on >= 0 && (static_cast<bool>(p.on) == game || now - p.at > 1500)) p = SkillPending{};
    return p.on >= 0 ? p.on != 0 : game;
  };
  // The ticks as shown: the game's state, or what was clicked until the game says so.
  bool stage_on[records::kMercStages] = {};
  int nopen = 0, nclosed = 0;
  for (int i = 0; i < v.merc_nstages; ++i) {
    const records::MercStage& s = v.merc_stage[i];
    stage_on[i] = s.always || settle(g_merc_stage_pending[s.bit & 31], s.open);
    nopen += stage_on[i];
    nclosed += !stage_on[i];
  }
  bool costume_on[records::kMercFamilies][records::kMercCostumes] = {};
  int nunlocked = 0, ncostumes = 0, nlocked = 0;
  for (int f = 0; f < v.merc_nfamilies; ++f)
    for (int k = 0; k < v.merc_family[f].n; ++k) {
      const records::MercCostume& c = v.merc_family[f].costume[k];
      costume_on[f][k] = c.always || settle(g_merc_costume_pending[c.bit & 63], c.open);
      nunlocked += costume_on[f][k];
      nlocked += !costume_on[f][k];
      ++ncostumes;
    }
  std::vector<records::MercSet> sets;
  auto stage = [&](const records::MercStage& s, bool on) {
    sets.push_back(records::MercSet{true, s.bit, on});
    g_merc_stage_pending[s.bit & 31] = SkillPending{static_cast<int8_t>(on ? 1 : 0), now};
  };
  auto costume = [&](const records::MercCostume& c, bool on) {
    sets.push_back(records::MercSet{false, c.bit, on});
    g_merc_costume_pending[c.bit & 63] = SkillPending{static_cast<int8_t>(on ? 1 : 0), now};
  };
  auto stage_name = [&](const records::MercStage& s) {
    return s.number >= 1200 ? rt::kMercStage[(s.number - 1200) % 10] : "?";
  };
  auto costume_name = [&](const records::MercCostume& c) {
    return c.bit < rt::kMercCostumeNames ? rt::kMercCostumeName[c.bit] : "?";
  };

  ImGui::Text("Stages open %d of %d", nopen, v.merc_nstages);
  ImGui::SameLine();
  ImGui::TextDisabled("\xC2\xB7");
  ImGui::SameLine();
  ImGui::Text("Costumes unlocked %d of %d", nunlocked, ncostumes);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  item_help("Tick a stage and the stage select lets you pick it at once; untick closes it again (the first three are "
            "always open). The game itself opens each of the others after an A rank or better on the stage above it.\n\n"
            "Tick a costume and the character select offers it, in the Mercenaries and No Mercy alike; untick locks it "
            "again (Leon's, Chris' and Jake's Default and the Agent are always there). The game unlocks the other "
            "characters and each Costume 1 through play; Costumes 2 and 3 it gave only as online event rewards. A "
            "costume unlocked here comes with its Collection file, as the game gives it. Coach, Nick, Ellis and "
            "Rochelle are always there in No Mercy.\n\nYour profile is saved after each change.");
  char label[64];
  ImGui::BeginDisabled(!nclosed);
  numbered(label, sizeof(label), "Open every stage", nclosed, "openall");
  if (ImGui::Button(label))
    for (int i = 0; i < v.merc_nstages; ++i)
      if (!stage_on[i]) stage(v.merc_stage[i], true);
  ImGui::EndDisabled();
  item_help("Open every stage of this mode on the stage select.");
  ImGui::SameLine();
  ImGui::BeginDisabled(!nlocked);
  numbered(label, sizeof(label), "Unlock every costume", nlocked, "unlockall");
  if (ImGui::Button(label))
    for (int f = 0; f < v.merc_nfamilies; ++f)
      for (int k = 0; k < v.merc_family[f].n; ++k)
        if (!costume_on[f][k]) costume(v.merc_family[f].costume[k], true);
  ImGui::EndDisabled();
  item_help("Unlock every character and costume of the character select, with their Collection files.");
  if (ImGui::BeginChild("merc", ImVec2(0, -footer(0)), ImGuiChildFlags_Borders)) {
    ImGui::SeparatorText(v.merc_no_mercy ? "No Mercy's stages" : "The Mercenaries' stages");
    if (ImGui::BeginTable("stages", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(28));
      ImGui::TableSetupColumn("stage", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed, scaled(128));
      for (int i = 0; i < v.merc_nstages; ++i) {
        const records::MercStage& s = v.merc_stage[i];
        ImGui::PushID(0x100 + s.bit);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool on = stage_on[i];
        ImGui::BeginDisabled(s.always);
        if (ImGui::Checkbox("##s", &on)) stage(s, on);
        ImGui::EndDisabled();
        char help[160];
        if (s.always) std::snprintf(help, sizeof(help), "%s is always open.", stage_name(s));
        else if (stage_on[i]) std::snprintf(help, sizeof(help), "%s is open. Untick: closed again.", stage_name(s));
        else std::snprintf(help, sizeof(help), "%s is closed. Tick: open on the stage select.", stage_name(s));
        item_help(help);
        ImGui::TableNextColumn();
        if (stage_on[i]) ImGui::TextUnformatted(stage_name(s));
        else ImGui::TextDisabled("%s", stage_name(s));
        ImGui::TableNextColumn();
        if (s.always) ImGui::TextDisabled("Always open");
        else if (stage_on[i]) ImGui::TextUnformatted("Open");
        else ImGui::TextDisabled("Closed");
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::SeparatorText("Characters and costumes (both modes)");
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("costumes", 2 + records::kMercCostumes, flags)) {
      ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_WidthStretch);
      for (int c = 0; c < records::kMercCostumes; ++c) ImGui::TableSetupColumn(rt::kMercCostume[c]);
      ImGui::TableSetupColumn("##all");
      ImGui::TableHeadersRow();
      for (int f = 0; f < v.merc_nfamilies; ++f) {
        const records::MercFamily& fam = v.merc_family[f];
        ImGui::PushID(0x200 + fam.family);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool any = false;
        int todo = 0;
        for (int k = 0; k < fam.n; ++k) {
          if (costume_on[f][k]) any = true;
          else ++todo;
        }
        const char* name = fam.family < rt::kMercCharacters ? rt::kMercCharacter[fam.family] : "?";
        if (any) ImGui::TextUnformatted(name);
        else ImGui::TextDisabled("%s", name);
        // A tick under each of the character's costumes (the game's own number its column).
        int at[records::kMercCostumes];
        std::fill(std::begin(at), std::end(at), -1);
        for (int k = 0; k < fam.n; ++k)
          if (fam.costume[k].costume < records::kMercCostumes) at[fam.costume[k].costume] = k;
        for (int c = 0; c < records::kMercCostumes; ++c) {
          ImGui::TableNextColumn();
          const int k = at[c];
          if (k < 0) continue;
          const records::MercCostume& mc = fam.costume[k];
          bool on = costume_on[f][k];
          std::snprintf(label, sizeof(label), "##c%d", c);
          ImGui::BeginDisabled(mc.always);
          if (ImGui::Checkbox(label, &on)) costume(mc, on);
          ImGui::EndDisabled();
          char help[160];
          if (mc.always) std::snprintf(help, sizeof(help), "%s is always there.", costume_name(mc));
          else if (costume_on[f][k])
            std::snprintf(help, sizeof(help), "%s is unlocked. Untick: locked again.", costume_name(mc));
          else
            std::snprintf(help, sizeof(help), "%s is locked. Tick: unlocked, with its Collection file.", costume_name(mc));
          item_help(help);
        }
        ImGui::TableNextColumn();
        if (fam.n > 1) {
          ImGui::BeginDisabled(!todo);
          if (ImGui::SmallButton("All###fam"))
            for (int k = 0; k < fam.n; ++k)
              if (!costume_on[f][k]) costume(fam.costume[k], true);
          ImGui::EndDisabled();
          item_help("Unlock each of this character's costumes.");
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
  if (!sets.empty()) records::set_merc(sets.data(), static_cast<int>(sets.size()));
}

void draw_records(records::Screen screen) {
  const records::View v = records::view();
  if (!v.known || v.screen != screen) return;
  switch (screen) {
    case records::Screen::kChapters:
      if (begin_records_window(kChaptersWindow)) {
        draw_chapters(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kMedals:
      if (begin_records_window(kMedalsWindow)) {
        draw_medals(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kEmblems:
      if (begin_records_window(kEmblemsWindow)) {
        draw_emblems(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kTitles:
      if (begin_records_window(kTitlesWindow)) {
        draw_titles(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kSkills:
      if (begin_records_window(kSkillsWindow)) {
        draw_skills(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kWeapons:
      if (begin_records_window(kWeaponsWindow, 900)) {
        draw_weapons(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kEnemies:
      if (begin_records_window(kEnemiesWindow, 720)) {
        draw_enemies(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kMercenaries:
      if (begin_records_window(v.merc_no_mercy ? kNoMercyWindow : kMercWindow, 720)) {
        draw_merc(v);
        end_records_window(v);
      }
      break;
    case records::Screen::kNone: break;
  }
}

// The visibility rule for this frame. What was typed belongs to the showing it
// was made in: it is let go once neither the pause nor the panel over the game is up.
bool g_panel_now = false, g_records_now = false;
bool visible_now(const dispatch::Snapshot& s) {
  const bool at_screen = s.show_panel || config::get().always_show;
  const bool over_play = g_user_shown && (s.run.screen == game::Screen::kPlay || !s.ready);
  if (!at_screen) {
    g_user_hidden = false;  // the next time the pause comes up the panel is back
    if (!over_play) forget_edits();
  }
  if (s.ready && s.run.screen != game::Screen::kPlay) g_user_shown = false;
  // A records window: up with its screen (F7 hides it until that screen closes or changes view).
  if (s.records != g_records_was) {
    g_records_was = s.records;
    g_records_hidden = false;
    forget_records_edits();
  }
  g_panel_now = g_context_ready && ((at_screen && !g_user_hidden) || over_play);
  g_records_now = g_context_ready && s.records != records::Screen::kNone && !g_records_hidden;
  return g_panel_now || g_records_now;
}

}  // namespace

bool ensure_context(HWND hwnd) {
  if (g_context_ready) return true;
  if (!hwnd) return false;
  g_hwnd = hwnd;

  IMGUI_CHECKVERSION();
  ImGui::SetAllocatorFunctions(memlog::heap_alloc, memlog::heap_free, nullptr);  // the mod's heap, counted
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  // Window positions persist beside the DLL, under the mod's own name rather
  // than a stray imgui.ini in the game folder.
  static char ini_path[MAX_PATH] = {};
  std::snprintf(ini_path, sizeof(ini_path), "%sRE6CabbyCodes.imgui.ini", config::dir());
  io.IniFilename = ini_path;
  // ImGui writes the file a moment after a change (wants_draw writes what is
  // still waiting when the panel goes): half a second, not its usual five, so
  // a game closed straight after a click loses nothing.
  io.IniSavingRate = 0.5f;
  // The panel's own lines in that file, registered before the first frame,
  // which is when ImGui reads it.
  ImGuiSettingsHandler panel{};
  panel.TypeName = "RE6CabbyCodes";
  panel.TypeHash = ImHashStr("RE6CabbyCodes");
  panel.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* name) -> void* {
    return std::strcmp(name, "Panel") == 0 ? &g_cheats_open : nullptr;
  };
  panel.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
    int v = 0;
    int i = 0;
    if (std::sscanf(line, "CheatsOpen=%d", &v) == 1) g_cheats_open = v != 0;
    else if (std::sscanf(line, "ResultsOpen=%d", &v) == 1) g_results_open = v != 0;
    else if (std::sscanf(line, "MercenariesOpen=%d", &v) == 1) g_merc_open = v != 0;
    else if (std::sscanf(line, "CaseOpen%d=%d", &i, &v) == 2 && i >= 0 && i < game::kMaxChars) g_case_open[i] = v != 0;
    else if (std::sscanf(line, "AddOpen%d=%d", &i, &v) == 2 && i >= 0 && i < game::kMaxChars) g_add_open[i] = v != 0;
  };
  panel.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* out) {
    out->appendf("[%s][Panel]\nCheatsOpen=%d\nResultsOpen=%d\nMercenariesOpen=%d\n", h->TypeName,
                 g_cheats_open ? 1 : 0, g_results_open ? 1 : 0, g_merc_open ? 1 : 0);
    for (int i = 0; i < game::kMaxChars; ++i)
      out->appendf("CaseOpen%d=%d\nAddOpen%d=%d\n", i, g_case_open[i] ? 1 : 0, i, g_add_open[i] ? 1 : 0);
    out->append("\n");
  };
  ImGui::AddSettingsHandler(&panel);
  // No keyboard navigation: the game's menus keep their keys. Dragging is
  // limited to the title bar so a click on the panel's background cannot pick
  // the window up by accident.
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  // A check the panel fails (a pop without its push, an End too many) is recovered from by ImGui and logged by the
  // mod (imgui_check_failed): no error tooltip over the game, no debug log growing in memory.
  io.ConfigErrorRecovery = true;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigErrorRecoveryEnableDebugLog = false;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.ScaleAllSizes(kScale);  // the padding, spacing and the rest; the font has its own factor
  style.FontScaleMain = kScale;

  if (!ImGui_ImplWin32_Init(hwnd)) {
    logf("ERROR: ImGui Win32 backend init failed");
    ImGui::DestroyContext();
    return false;
  }
  // The window procedure is subclassed through the W entry point whatever the
  // window's kind; CallWindowProcW hands an ANSI window's procedure its messages
  // converted, as Windows does for any subclass.
  g_orig_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hk_wndproc)));
  forget_records_edits();
  g_context_ready = true;
  char cls[64] = "?";
  GetClassNameA(hwnd, cls, sizeof(cls));
  logf("overlay ready (hwnd=%p class '%s'%s, scale %d%%, present thread %lu, window thread %lu)",
       static_cast<void*>(hwnd), cls, IsWindowUnicode(hwnd) ? ", a Unicode window" : "", kScalePercent,
       GetCurrentThreadId(), GetWindowThreadProcessId(hwnd, nullptr));
  return true;
}

HWND window() { return g_context_ready ? g_hwnd : nullptr; }

bool visible() { return g_visible_flag != 0; }
bool requested() { return g_user_shown; }
bool capturing_mouse() { return g_capture_mouse != 0; }
bool capturing_keyboard() { return g_capture_keyboard != 0; }

void lock_imgui() {
  if (g_imgui_cs_ready) EnterCriticalSection(&g_imgui_cs);
}

void unlock_imgui() {
  if (g_imgui_cs_ready) LeaveCriticalSection(&g_imgui_cs);
}

// Called from the present hook with the context lock held.
bool wants_draw() {
  const bool was_visible = g_visible;
  g_visible = visible_now(dispatch::snapshot());
  InterlockedExchange(&g_visible_flag, g_visible ? 1 : 0);
  // ImGui's delay before it writes the layout file (the sections folded, the
  // panel's place) counts down only in frames, and the panel draws none once
  // it is gone - nor does the mod touch ImGui as the game exits. A change made
  // just before the pause closed would wait for the panel's next showing, and be
  // lost if the game ended first: so whatever is waiting is written now.
  if (was_visible && !g_visible && g_context_ready && GImGui->SettingsDirtyTimer > 0.0f && GImGui->IO.IniFilename)
    ImGui::SaveIniSettingsToDisk(GImGui->IO.IniFilename);
  if (!g_visible) {  // nothing is being taken from the game until it draws again
    InterlockedExchange(&g_capture_mouse, 0);
    InterlockedExchange(&g_capture_keyboard, 0);
  }
  if (!g_context_ready) return false;
  ImGuiIO& io = ImGui::GetIO();
  // The game hides the system pointer (and holds the mouse while it plays):
  // the panel draws one of its own.
  io.MouseDrawCursor = g_visible;
  if (g_visible) {
    // If a release was ever missed the panel would follow the pointer for ever;
    // the physical button is the truth. GetAsyncKeyState reads the physical
    // buttons and the messages the logical ones: with the buttons swapped in
    // Windows' mouse settings (left-handed) the panel's left button is the
    // physical right one (RE3CabbyCodes, 2026-10-13).
    static const int kVk[3] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON};
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    for (int b = 0; b < 3; ++b) {
      const int vk = swapped && b < 2 ? kVk[1 - b] : kVk[b];
      if (io.MouseDown[b] && !(GetAsyncKeyState(vk) & 0x8000)) io.AddMouseButtonEvent(b, false);
    }
  } else {
    // Input made while the panel is away is not the panel's: ImGui only queues
    // events and drains them in NewFrame, which does not run while the panel is
    // hidden, so they would be replayed onto it the moment it came back (RE0).
    io.ClearEventsQueue();
    io.ClearInputMouse();
    io.ClearInputKeys();
  }
  return g_visible;
}

void draw_frame() {
  if (!g_visible) return;
  // NewFrame has run, so what ImGui wants this frame is settled; the input guard reads these from the game's
  // thread (the panel's and a records window's alike).
  const ImGuiIO& io = ImGui::GetIO();
  InterlockedExchange(&g_capture_mouse, io.WantCaptureMouse ? 1 : 0);
  InterlockedExchange(&g_capture_keyboard, io.WantTextInput ? 1 : 0);
  if (g_panel_now) draw_panel();
  if (g_records_now) draw_records(g_records_was);
}

void shutdown_imgui() {
  if (!g_context_ready) return;
  ImGui_ImplWin32_Shutdown();
  if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
  g_context_ready = false;
  g_visible = false;
  InterlockedExchange(&g_visible_flag, 0);
  InterlockedExchange(&g_capture_mouse, 0);
  InterlockedExchange(&g_capture_keyboard, 0);
}

void init() {
  adopt::init();
  if (!g_imgui_cs_ready) {  // before the window procedure or the present hook exists
    InitializeCriticalSection(&g_imgui_cs);
    g_imgui_cs_ready = true;
  }
}

void uninstall() {
  // Window procedure first: a message arriving after our image is gone would
  // jump into freed memory.
  if (g_orig_wndproc && g_hwnd && IsWindow(g_hwnd)) {
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig_wndproc));
    g_orig_wndproc = nullptr;
  }
  dx9::uninstall();
  shutdown_imgui();
  logf("overlay hooks removed");
}

}  // namespace re6cc::overlay

namespace re6cc {

// ImGui's checks (IM_ASSERT, src/imconfig_re6cc.h): each one that fails is logged once - a frame's check repeats
// every frame - and the panel goes on (ImGui recovers what it can: a pop too many is not made, an End is supplied).
void imgui_check_failed(const char* what, const char* file, int line) {
  constexpr int kMost = 16;
  static const char* seen_file[kMost];
  static int seen_line[kMost];
  static int seen = 0;  // kMost + 1 once the table is full (and the log has said so)
  for (int i = 0; i < seen && i < kMost; ++i)
    if (seen_line[i] == line && seen_file[i] == file) return;
  if (seen >= kMost) {
    if (seen == kMost) {
      ++seen;
      logf("ERROR: overlay: more of ImGui's checks failed - not logged");
    }
    return;
  }
  seen_file[seen] = file;
  seen_line[seen] = line;
  ++seen;
  const char* leaf = std::strrchr(file, '/');
  if (const char* back = std::strrchr(file, '\\'); back && (!leaf || back > leaf)) leaf = back;
  logf("ERROR: overlay: ImGui's check failed - %s (%s:%d); logged, and the panel goes on", what, leaf ? leaf + 1 : file,
       line);
}

}  // namespace re6cc
