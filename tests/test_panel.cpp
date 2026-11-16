// Offline test of the panel (src/overlay.cpp): drawn headless with ImGui over a made-up game - paused in a
// campaign with Leon and Helena, the profile's skill points known - and driven by clicks, as a player drives it.
// ImGui's test-engine hooks report each widget's label and place, so a click lands on the widget it names.
// overlay.cpp is built into this file (its insides are reached by name); everything it asks of the rest of the
// mod is made up below. Build:
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_ENABLE_TEST_ENGINE
//       -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD -Icontrib/imgui -Isrc -static tests/test_panel.cpp
//       contrib/imgui/imgui.cpp contrib/imgui/imgui_draw.cpp contrib/imgui/imgui_tables.cpp
//       contrib/imgui/imgui_widgets.cpp contrib/imgui/backends/imgui_impl_win32.cpp
//       -luser32 -lgdi32 -ldwmapi -o tests/build/test_panel.exe
//   wine tests/build/test_panel.exe 'Z:\some\scratch\dir' [shots]
//
// With "shots" each step's panel is drawn to a picture (.ppm) in the scratch dir.
//
// Each frame runs the panel's own rule for what it keeps (visible_now, as the present hook does) before it draws.
//
// - the pause: every cheat (Infinite stamina and Infinite air among them; each click flipping its switch, and back),
//   the skill points' editor, no Characters section (the character list has no section of its own) - and no ImGui
//   assert;
// - Set beside the skill points asks for the points shown, or those typed;
// - each character's inventory: a count typed, Apply (for its slot and id), Undo, Apply all, a count held at its
//   most, a character played online shown only; a weapon's modes named by what they load (the Wing Shooter's by the
//   game's own names for them, One Gun and Two Guns), an unknown item by its number; Throw away (x, and the ticked
//   together) for the case's items only; Add from the list's tabs or a search - no weapon, the ammo only of a kind
//   the character's own weapons load (none for one holding no gun), a stale pick of another not added - a grenade
//   the stage has not loaded preloaded as it is picked and added, one the game's loader cannot load greyed;
// - Automatic QTEs, Steady aim, Freeze countdown timer (its Time left when a countdown is up); the Results in a
//   campaign (its switches, the clock's and a counter's Set); the Mercenaries' section in a match only;
// - a switch the game lacks (its hook not found) is drawn greyed, and a click does not flip it;
// - in play (the panel opened over the game with F7) the editor is disabled: nothing is asked;
// - out of a game (the title, the menus) nothing is drawn, and F7 does not open it there;
// - beside the game's Records screen (its chapter grid, its medals) and Dog Tag screen (the emblem and title lists):
//   each one's window, never the cheat panel; F7 hides it until the screen changes; the chapters' picks (one, a
//   whole character's, everyone's) asked for by Apply all with only the real changes; a cleared chapter's best time
//   under its rank (none under one not cleared), one typed as a player types it asked for with its rank kept, one
//   that is no time not asked for and let go as the field is left, a time for every chapter of a character with
//   its rank; a rank alone leaving the time as it is; a medal's count (Apply, Type
//   in for every medal listed, Apply all); the categories the game shows, on its own; an emblem's Award, every
//   locked one ticked and awarded; a title's Next and All; "Bring their counts up" for titles held short;
// - beside the game's skill screen: the skills window, never the cheat panel; a locked skill's tick asks to unlock it,
//   an unlocked one's to lock it again, a bought one's and one whose requirement is met fixed; Unlock all and Lock
//   all ask for just the skills they can change; F7 hides it.
// - beside the Records screen's WEAPONS page: the weapons window - a tick only where a character's campaign has the
//   weapon, a character not set up and a row used in play fixed; a weapon given and taken out, a row named and taken
//   back; each character's and each weapon's All, Give every weapon, Name every row; F7 hides it.
// - beside the Records screen's ENEMIES page: the enemies window - a "? ? ?" row's tick reveals it, a named row's
//   (by its kills or its Times Defeated) fixed; Reveal all asks for just the "? ? ?" rows; the "? ? ?" filter; F7
//   hides it.
// - beside the Mercenaries' stage select and character select: the Mercenaries window (No Mercy's) - a stage's tick
//   opens it or closes it again, a costume's unlocks it or locks it again, the ones always there fixed; a
//   character's All, Open every stage and Unlock every costume ask for just the ones they change; F7 hides it.
#include "../src/overlay.cpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// --- what overlay.cpp asks of the rest of the mod, made up ---------------------------------
namespace re6cc {
void logf(const char*, ...) {}
namespace overlay::dx9 {
void uninstall() {}
}  // namespace overlay::dx9
namespace adopt {
void init() {}
}  // namespace adopt
namespace boot {
bool done() { return true; }
const char* status() { return "ready"; }
}  // namespace boot
namespace config {
Settings g_settings;
const Settings& get() { return g_settings; }
const char* dir() { return ".\\"; }
}  // namespace config
namespace dispatch {
Snapshot g_snap;
Snapshot snapshot() { return g_snap; }
}  // namespace dispatch
namespace cheats {
bool g_on[sw::kCount];
bool g_ok[sw::kCount];
struct Asked {
  Edit what;
  int chara;
  double value;
};
std::vector<Asked> g_asked;
Status g_status;
bool enabled(sw::Kind k) { return g_on[k]; }
void set(sw::Kind k, bool on) { g_on[k] = on; }
bool available(sw::Kind k) { return g_ok[k]; }
const char* unavailable_reason(sw::Kind) { return "not found"; }
Status status() { return g_status; }
void edit(Edit what, int chara, double value) { g_asked.push_back(Asked{what, chara, value}); }
}  // namespace cheats
namespace inventory {
View g_view;
struct Asked {
  int chara, slot, id, mode, count;
};
std::vector<Asked> g_asked, g_thrown, g_added;
View view() { return g_view; }
void set_count(int chara, int slot, int id, int mode, int count) {
  g_asked.push_back(Asked{chara, slot, id, mode, count});
}
void throw_away(int chara, int slot, int id) { g_thrown.push_back(Asked{chara, slot, id, 0, 0}); }
void add(int chara, int id, int count) { g_added.push_back(Asked{chara, -1, id, 0, count}); }
std::vector<int> g_preloaded;
void preload(int id) { g_preloaded.push_back(id); }
Avail avail(const View& v, int id) {
  for (int k = 0; k < items::kCount; ++k)
    if (items::kItems[k].id == id) return static_cast<Avail>(v.avail[k]);
  return (id >> 8) == 1 ? kNever : kHere;
}
}  // namespace inventory
namespace memlog {
void* heap_alloc(size_t n, void*) { return std::malloc(n); }
void heap_free(void* p, void*) { std::free(p); }
}  // namespace memlog
namespace merc {
Status g_status;
Status status() { return g_status; }
}  // namespace merc
namespace records {
View g_view;
std::vector<ChapterSet> g_chapters;
std::vector<MedalSet> g_medals;
std::vector<uint8_t> g_emblems;
std::vector<TitleSet> g_titles;
std::vector<SkillSet> g_skills;
std::vector<WeaponSet> g_weapon_rows;
std::vector<CarrySet> g_carried;
std::vector<uint8_t> g_enemies;
std::vector<MercSet> g_merc;
View view() { return g_view; }
void set_merc(const MercSet* s, int n) { g_merc.insert(g_merc.end(), s, s + n); }
Screen screen() { return g_view.screen; }
void set_weapon_rows(const WeaponSet* s, int n) { g_weapon_rows.insert(g_weapon_rows.end(), s, s + n); }
void set_carried(const CarrySet* s, int n) { g_carried.insert(g_carried.end(), s, s + n); }
void reveal_enemies(const uint8_t* e, int n) { g_enemies.insert(g_enemies.end(), e, e + n); }
void set_chapters(const ChapterSet* s, int n) { g_chapters.insert(g_chapters.end(), s, s + n); }
void set_medals(const MedalSet* s, int n) { g_medals.insert(g_medals.end(), s, s + n); }
void award_emblems(const uint8_t* e, int n) { g_emblems.insert(g_emblems.end(), e, e + n); }
void award_titles(const TitleSet* s, int n) { g_titles.insert(g_titles.end(), s, s + n); }
void set_skills(const SkillSet* s, int n) { g_skills.insert(g_skills.end(), s, s + n); }
const char* medal_name(const View& v, int cat, int medal) {
  const int msg = v.medal[cat].base + medal - records_text::kMedalFirst;
  return msg >= 0 && msg < records_text::kMedalMsgs ? records_text::kMedal[msg] : "";
}
}  // namespace records
}  // namespace re6cc

// --- ImGui's test-engine hooks: what was drawn where ---------------------------------------
struct Drawn {
  ImGuiID id = 0;
  ImRect bb;
  std::string label;
  ImGuiItemStatusFlags flags = 0;  // a checkbox's ImGuiItemStatusFlags_Checked among them
  bool placed = false, named = false;
};
std::vector<Drawn> g_drawn;

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& bb, const ImGuiLastItemData*) {
  for (auto it = g_drawn.rbegin(); it != g_drawn.rend(); ++it)
    if (it->id == id && !it->placed) {
      it->bb = bb;
      it->placed = true;
      return;
    }
  g_drawn.push_back(Drawn{id, bb, "", 0, true, false});
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags flags) {
  for (auto it = g_drawn.rbegin(); it != g_drawn.rend(); ++it)
    if (it->id == id && !it->named) {
      it->label = label ? label : "";
      it->flags = flags;
      it->named = true;
      return;
    }
  g_drawn.push_back(Drawn{id, ImRect(), label ? label : "", flags, false, true});
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

namespace {

using namespace re6cc;
int g_fail = 0;
std::string g_shots;
int g_shot = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

// --- the made-up game ----------------------------------------------------------------------
void make_game(bool in_game, bool paused) {
  dispatch::Snapshot& s = dispatch::g_snap;
  s = dispatch::Snapshot{};
  s.ready = true;
  s.run.in_game = in_game;
  s.run.paused = paused;
  s.run.screen = !in_game ? game::Screen::kNone : paused ? game::Screen::kPause : game::Screen::kPlay;
  s.show_panel = in_game && paused;
  if (in_game) {
    s.run.nchars = 2;
    s.run.chars[0] = game::Character{0x1000, 0, game::Control::kLocal, "Leon", 1050, 1200};
    s.run.chars[1] = game::Character{0x2000, 1, game::Control::kAi, "Helena", 1200, 1200};
  }
  s.run.mode_known = true;
  s.run.mode = game::kModeCampaign;
  // Leon: the Wing Shooter in hand (a count for each of its modes: one gun 13 of 18, two 20 of 36), the grenade
  // launcher keeping a count for each of its three modes, 9mm rounds in the case and an item the table does not
  // know (shown by number); Helena: 9mm rounds, no gun. Leon also has the crossbow to pick up later; the launcher
  // and the crossbow are loaded, the grenades are not (the game's loader loads one first), and the loader could not
  // load the Remote Bomb.
  inventory::View& v = inventory::g_view;
  v = inventory::View{};
  v.known = in_game;
  v.paused = paused;
  v.can_add = in_game && paused;
  if (in_game) {
    v.ncases = 2;
    inventory::Case& l = v.cases[0];
    l.index = 0;
    l.name = "Leon";
    l.nitems = 4;
    l.items[0] = inventory::Item{0, 0x103, 0, {13, 20, 0}, {18, 36, 0}, true, false};
    l.items[1] = inventory::Item{3, 0x112, 0, {4, 2, 0}, {6, 6, 6}, false, false};
    l.items[2] = inventory::Item{15, 0x201, 0, {30, 0, 0}, {150, 0, 0}, false, true};
    l.items[3] = inventory::Item{16, 0x7A1, 0, {1, 0, 0}, {1, 0, 0}, false, true};
    l.nheld = 4;
    l.held[0] = 0x103;
    l.held[1] = 0x112;
    l.held[2] = 0x201;
    l.held[3] = 0x7A1;
    inventory::Case& h = v.cases[1];
    h.index = 1;
    h.name = "Helena";
    h.control = game::Control::kAi;
    h.nitems = 1;
    h.items[0] = inventory::Item{15, 0x201, 0, {30, 0, 0}, {150, 0, 0}, false, true};
    v.loader = true;
    for (int k = 0; k < items::kCount; ++k) {
      const int id = items::kItems[k].id;
      v.avail[k] = (id >> 8) != 1 || id == 0x103 || id == 0x112 || id == 0x117 ? inventory::kHere
                   : id == 0x116                                                ? inventory::kNever
                                                                                : inventory::kLoadable;
    }
  }
  inventory::g_preloaded.clear();
  inventory::g_asked.clear();
  inventory::g_thrown.clear();
  inventory::g_added.clear();
  merc::g_status = merc::Status{};
  cheats::g_status = cheats::Status{};
  cheats::g_status.results = in_game;
  cheats::g_status.clock = 3723;
  cheats::g_status.clock_most = 71999;
  cheats::g_status.counter_most = 50000;
  cheats::g_status.ncounters = 1;
  cheats::g_status.chars[0] = cheats::Counters{0, "Leon", 10, 5, 7, 1};
  cheats::g_status.points = true;
  cheats::g_status.skill_points = 123456;
  for (bool& ok : cheats::g_ok) ok = true;
}

void frame() {
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.DisplaySize = ImVec2(1920, 1080);
  g_drawn.clear();
  overlay::g_visible = overlay::visible_now(dispatch::g_snap);  // what the panel keeps, as wants_draw() decides it
  ImGui::NewFrame();
  // The panel places itself at its first showing; the test's window is made tall enough to hold every section
  // (a widget out of sight is not reported, and could not be clicked).
  ImGui::SetWindowPos(overlay::kPanelWindow, ImVec2(960, 20), ImGuiCond_Always);
  ImGui::SetWindowSize(overlay::kPanelWindow, ImVec2(940, 1050), ImGuiCond_Always);
  // The Mercenaries window too (its stages and costumes in one list).
  ImGui::SetWindowPos("###RE6CC mercenaries", ImVec2(820, 10), ImGuiCond_Always);
  ImGui::SetWindowSize("###RE6CC mercenaries", ImVec2(1090, 1065), ImGuiCond_Always);
  overlay::draw_frame();
  ImGui::Render();
  for (ImTextureData* t : ImGui::GetPlatformIO().Textures)
    if (t->Status == ImTextureStatus_WantCreate || t->Status == ImTextureStatus_WantUpdates) {
      t->SetTexID(static_cast<ImTextureID>(1));
      t->SetStatus(ImTextureStatus_OK);
    }
}

void frames(int n) {
  for (int i = 0; i < n; ++i) frame();
}

const Drawn* find(const char* label, int nth = 0) {
  for (const Drawn& d : g_drawn)
    if (d.label == label && nth-- == 0) return &d;
  return nullptr;
}

bool click_at(const Drawn* d) {
  if (!d) return false;
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 at((d->bb.Min.x + d->bb.Max.x) * 0.5f, (d->bb.Min.y + d->bb.Max.y) * 0.5f);
  io.AddMousePosEvent(at.x, at.y);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frames(2);
  return true;
}
bool click(const char* label, int nth = 0) { return click_at(find(label, nth)); }

// A widget known by its ID (a combo reports no label to the test engine): the one placed with it.
const Drawn* find_id(ImGuiID id) {
  for (const Drawn& d : g_drawn)
    if (id && d.id == id && d.placed) return &d;
  return nullptr;
}

bool any_containing(const char* text) {
  for (const Drawn& d : g_drawn)
    if (d.label.find(text) != std::string::npos) return true;
  return false;
}

// --- pictures (shots): ImGui's triangles filled in, the font's alpha from its atlas -----------
void picture(const char* name) {
  if (g_shots.empty()) return;
  const ImDrawData* dd = ImGui::GetDrawData();
  const int W = 1920, H = 1080;
  std::vector<float> px(static_cast<size_t>(W) * H * 3, 0.06f);
  for (const ImDrawList* dl : dd->CmdLists)
    for (const ImDrawCmd& c : dl->CmdBuffer) {
      if (c.UserCallback) continue;
      const ImTextureData* tex = c.TexRef._TexData;
      const int cx0 = std::max(0, static_cast<int>(c.ClipRect.x)), cy0 = std::max(0, static_cast<int>(c.ClipRect.y));
      const int cx1 = std::min(W, static_cast<int>(c.ClipRect.z)), cy1 = std::min(H, static_cast<int>(c.ClipRect.w));
      for (unsigned i = 0; i + 2 < c.ElemCount; i += 3) {
        const ImDrawVert* v[3];
        for (int k = 0; k < 3; ++k) v[k] = &dl->VtxBuffer[c.VtxOffset + dl->IdxBuffer[c.IdxOffset + i + k]];
        const float x0 = v[0]->pos.x, y0 = v[0]->pos.y, x1 = v[1]->pos.x, y1 = v[1]->pos.y;
        const float x2 = v[2]->pos.x, y2 = v[2]->pos.y;
        const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
        if (std::fabs(area) < 1e-6f) continue;
        const int bx0 = std::max(cx0, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
        const int by0 = std::max(cy0, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
        const int bx1 = std::min(cx1, static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
        const int by1 = std::min(cy1, static_cast<int>(std::ceil(std::max({y0, y1, y2}))));
        for (int y = by0; y < by1; ++y)
          for (int x = bx0; x < bx1; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            const float w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) / area;
            const float w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
            float col[4];
            for (int k = 0; k < 4; ++k)
              col[k] = (((v[0]->col >> (8 * k)) & 0xFF) * w0 + ((v[1]->col >> (8 * k)) & 0xFF) * w1 +
                        ((v[2]->col >> (8 * k)) & 0xFF) * w2) /
                       255.0f;
            float ta = 1.0f, tc[3] = {1, 1, 1};
            if (tex && tex->Pixels) {
              const float u = v[0]->uv.x * w0 + v[1]->uv.x * w1 + v[2]->uv.x * w2;
              const float t = v[0]->uv.y * w0 + v[1]->uv.y * w1 + v[2]->uv.y * w2;
              const int tx = std::clamp(static_cast<int>(u * tex->Width), 0, tex->Width - 1);
              const int ty = std::clamp(static_cast<int>(t * tex->Height), 0, tex->Height - 1);
              const unsigned char* p = tex->Pixels + (static_cast<size_t>(ty) * tex->Width + tx) * tex->BytesPerPixel;
              if (tex->Format == ImTextureFormat_Alpha8) {
                ta = p[0] / 255.0f;
              } else {
                for (int k = 0; k < 3; ++k) tc[k] = p[k] / 255.0f;
                ta = p[3] / 255.0f;
              }
            }
            const float a = col[3] * ta;
            float* o = &px[(static_cast<size_t>(y) * W + x) * 3];
            for (int k = 0; k < 3; ++k) o[k] = o[k] * (1.0f - a) + col[k] * tc[k] * a;
          }
      }
    }
  char path[600];
  std::snprintf(path, sizeof(path), "%s\\%02d-%s.ppm", g_shots.c_str(), ++g_shot, name);
  if (FILE* f = std::fopen(path, "wb")) {
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (const float c : px) std::fputc(static_cast<int>(std::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f), f);
    std::fclose(f);
  }
}

// --- the checks ------------------------------------------------------------------------------
void check_pause() {
  make_game(true, true);
  frames(3);
  picture("pause");
  check(find("God mode") && find("Partner takes no damage") && find("Infinite stamina") && find("Infinite air") &&
            find("One hit kills") && find("Infinite ammo") && find("Infinite skill points"),
        "pause: every cheat");
  static const struct {
    const char* label;
    sw::Kind k;
  } kRows[] = {{"God mode", sw::kGodMode},
               {"Partner takes no damage", sw::kPartnerGod},
               {"Infinite stamina", sw::kInfiniteStamina},
               {"Infinite air", sw::kInfiniteAir},
               {"One hit kills", sw::kOneHitKills},
               {"Infinite ammo", sw::kInfiniteAmmo},
               {"Infinite skill points", sw::kInfiniteSkillPoints}};
  for (const auto& r : kRows) {
    const bool was = cheats::g_on[r.k];
    click(r.label);
    const bool flipped = cheats::g_on[r.k] != was;
    click(r.label);
    char what[96];
    std::snprintf(what, sizeof(what), "pause: %s - a click flips its switch, a second puts it back", r.label);
    check(flipped && cheats::g_on[r.k] == was, what);
  }
  check(any_containing("###cheats") && !any_containing("###characters"),
        "pause: the Cheats section, no Characters section");
  check(find("Set##points") != nullptr, "pause: the skill points' editor");
  cheats::g_asked.clear();
  click("Set##points");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kSkillPoints &&
            cheats::g_asked[0].value == 123456,
        "skill points: Set asks for the points shown");
  overlay::g_points = {500000, true};
  frames(1);
  cheats::g_asked.clear();
  click("Set##points");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].value == 500000, "skill points: Set asks for those typed");
  frames(1);
  check(!overlay::g_points.typing, "skill points: the field follows the game again after Set");
}

// A count typed into a character's inventory: Apply asks for it, by the slot and the id it was typed for; Undo
// lets it go; Apply all asks for every count typed; a character played online is shown only.
void check_inventory() {
  make_game(true, true);
  // The sections above folded (as a player would), so the inventories are in sight.
  overlay::g_cheats_open = overlay::g_results_open = false;
  frames(2);
  picture("inventory");
  check(find("Leon's inventory  (4)###case0") && find("Helena's inventory  (1)###case1") && find("##n0000") &&
            find("##n0001") && find("##n0030") && find("##n0031") && find("##n1150"),
        "inventory: each character's section, a field for each count (each mode's for the Wing Shooter and the "
        "launcher)");
  {
    char buf[96];
    const inventory::Item& ws = inventory::g_view.cases[0].items[0];
    const inventory::Item& gl = inventory::g_view.cases[0].items[1];
    const inventory::Item& odd = inventory::g_view.cases[0].items[3];
    check(!std::strcmp(overlay::count_label(gl, 1, 3, buf, sizeof(buf)), "Grenade Launcher  (40mm Acid Rounds)"),
          "inventory: a weapon's count of each mode named by what it loads (the game's own names)");
    char buf2[96];
    check(!std::strcmp(overlay::count_label(ws, 0, 2, buf, sizeof(buf)), "Wing Shooter  (One Gun)") &&
              !std::strcmp(overlay::count_label(ws, 1, 2, buf2, sizeof(buf2)), "Wing Shooter  (Two Guns)"),
          "inventory: the Wing Shooter's two counts named by the game's own names for its modes (one gun, two)");
    check(!std::strcmp(overlay::count_label(odd, 0, 1, buf, sizeof(buf)), "Item 0x7A1"),
          "inventory: an item the table does not name shown by number");
  }
  check(!find("Apply##p0000"), "inventory: nothing typed, no Apply");
  overlay::g_count[0][0][0] = overlay::CountEdit{0x103, 9, true};
  frames(1);
  click("Apply##p0000");
  check(inventory::g_asked.size() == 1 && inventory::g_asked[0].chara == 0 && inventory::g_asked[0].slot == 0 &&
            inventory::g_asked[0].id == 0x103 && inventory::g_asked[0].mode == 0 && inventory::g_asked[0].count == 9,
        "inventory: Apply asks for the count typed, for that slot and id");
  frames(1);
  check(!overlay::g_count[0][0][0].typing && overlay::g_count[0][0][0].v == 13,
        "inventory: after Apply the field follows the game again");
  overlay::g_count[0][3][1] = overlay::CountEdit{0x112, 99, true};
  frames(2);
  check(overlay::g_count[0][3][1].v == 6, "inventory: a count typed past the item's most is held at it");
  inventory::g_asked.clear();
  click("Undo##u0031");
  check(inventory::g_asked.empty() && !overlay::g_count[0][3][1].typing, "inventory: Undo lets the typed count go");
  overlay::g_count[0][0][0] = overlay::CountEdit{0x103, 1, true};
  overlay::g_count[0][3][0] = overlay::CountEdit{0x112, 5, true};
  frames(1);
  click("Apply all 2###apply0");
  check(inventory::g_asked.size() == 2 && inventory::g_asked[0].slot == 0 && inventory::g_asked[0].count == 1 &&
            inventory::g_asked[1].slot == 3 && inventory::g_asked[1].mode == 0 && inventory::g_asked[1].count == 5,
        "inventory: Apply all asks for every count typed");
  // Throw away: a case item's x; the ticked ones together; never the item in hand.
  check(find("x##x016") && find("x##x015") && !find("x##x000") && !find("x##x003"),
        "inventory: Throw away (x) for the case's items only - not a weapon, not the one in hand");
  click("x##x016");
  check(inventory::g_thrown.size() == 1 && inventory::g_thrown[0].slot == 16 && inventory::g_thrown[0].id == 0x7A1,
        "inventory: x asks for that item thrown away (its slot and id)");
  inventory::g_thrown.clear();
  click("##t015");
  click("Throw away###throw0");
  check(inventory::g_thrown.size() == 1 && inventory::g_thrown[0].slot == 15 && inventory::g_thrown[0].id == 0x201,
        "inventory: a ticked item thrown away by Throw away");
  // Add: the list (by tabs, a search), the count, the button.
  overlay::g_add_open[0] = true;
  overlay::g_picker[0] = overlay::Picker{};
  frames(2);
  click("Ammo");
  picture("add");
  check(find("9mm Ammo##201") && find("40mm Explosive Rounds##208") && find("40mm Acid Rounds##209") &&
            find("40mm Nitrogen Rounds##20A") && !find("12-Gauge Shells##202") && !find("Arrows (Normal)##20C") &&
            !find("Shotgun##104"),
        "Add: the Ammo tab lists what Leon's weapons load - 9mm (the Wing Shooter), the launcher's three rounds - "
        "not 12-gauge shells (no shotgun), not the arrows (the crossbow is to be picked up, not held)");
  check(!find("Weapons") && find("Grenades") && find("Recovery"), "Add: no Weapons tab (the grenades', the "
                                                                   "recovery items' tabs)");
  click("9mm Ammo##201");
  overlay::g_picker[0].count = 25;
  frames(1);
  click("Add 9mm Ammo###add");
  check(inventory::g_added.size() == 1 && inventory::g_added[0].chara == 0 && inventory::g_added[0].id == 0x201 &&
            inventory::g_added[0].count == 25,
        "Add: 25 9mm Ammo asked for, for Leon");
  inventory::g_added.clear();
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "acid");
  frames(2);
  check(find("40mm Acid Rounds##209") && !find("9mm Ammo##201"), "Add: a search lists what matches, in any tab");
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "grenade");
  frames(2);
  check(find("Hand Grenade##113") && find("Flash Grenade##115") && !find("Grenade Launcher##112"),
        "Add: a search lists no weapon (\"grenade\": the grenades, not the Grenade Launcher)");
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "shells");
  frames(2);
  check(!find("12-Gauge Shells##202") && !find("10-Gauge Shells##20E"),
        "Add: ... nor ammo no weapon of Leon's loads (\"shells\": none)");
  overlay::g_picker[0] = overlay::Picker{};
  overlay::g_picker[0].picked = 0x202;  // picked before (as if a shotgun had been held then)
  frames(1);
  click("Add ###add");
  check(inventory::g_added.empty(), "Add: a pick of ammo no weapon of Leon's loads is not added");
  overlay::g_picker[0] = overlay::Picker{};
  frames(1);
  click("Grenades");
  click("Hand Grenade##113");  // nobody in play holds one: the stage has not loaded it
  frames(2);
  check(inventory::g_preloaded.size() == 1 && inventory::g_preloaded[0] == 0x113,
        "Add: a grenade the stage has not loaded is loaded as it is picked (the game's loader, asked once)");
  overlay::g_picker[0].count = 3;
  frames(1);
  click("Add Hand Grenade###add");
  check(inventory::g_added.size() == 1 && inventory::g_added[0].id == 0x113 && inventory::g_added[0].count == 3,
        "Add: ... and its Add asked for, 3 (not greyed)");
  inventory::g_added.clear();
  inventory::g_view.loading = true;
  inventory::g_view.note_chara = 0;
  std::snprintf(inventory::g_view.note, sizeof(inventory::g_view.note), "Loading the Hand Grenade...");
  frames(2);
  picture("add-loading");
  inventory::g_view.loading = false;
  inventory::g_view.note[0] = '\0';
  click("Remote Bomb##116");  // the loader could not load it
  frames(2);
  click("Add Remote Bomb###add");
  check(inventory::g_added.empty() && inventory::g_preloaded.size() == 1,
        "Add: a grenade the game's loader cannot load is greyed: not asked for, not preloaded");
  overlay::g_add_open[0] = false;
  overlay::g_picker[0] = overlay::Picker{};
  // Helena holds no gun: her Add's Ammo tab lists nothing; her grenades are there.
  overlay::g_add_open[1] = true;
  frames(2);
  click("Ammo");
  check(!any_containing("Ammo##2") && !any_containing("Rounds##2") && !any_containing("Shells##2") &&
            !any_containing("Arrows") && !any_containing("Rocket##2"),
        "Add: Helena (no gun) is offered no ammo");
  click("Grenades");
  check(find("Hand Grenade##113") != nullptr, "Add: ... and her grenades are there");
  overlay::g_add_open[1] = false;
  overlay::g_picker[1] = overlay::Picker{};
  // Helena played online: her fields are drawn, nothing she is typed is asked for.
  inventory::g_view.cases[1].control = game::Control::kNetwork;
  overlay::g_count[1][15][0] = overlay::CountEdit{0x201, 5, true};
  frames(1);
  inventory::g_asked.clear();
  click("Apply##p1150");
  check(inventory::g_asked.empty() && any_containing("##n1150"), "inventory: a character played online is shown only");
  overlay::forget_edits();
  overlay::g_cheats_open = overlay::g_results_open = true;
}

// The cheats added with the rest: Automatic QTEs, Steady aim, Freeze countdown timer (and its editor when a
// countdown is up); the Results in a campaign; the Mercenaries' section in a match only (no Results there).
void check_sections() {
  make_game(true, true);
  frames(2);
  static const struct {
    const char* label;
    sw::Kind k;
  } kRows[] = {{"Automatic QTEs", sw::kAutoQte},
               {"Steady aim", sw::kSteadyAim},
               {"Freeze countdown timer", sw::kFreezeCountdown},
               {"Freeze play time", sw::kFreezePlaytime},
               {"Deaths don't count", sw::kNoDeathCount},
               {"Misses don't count", sw::kNoMissCount}};
  for (const auto& r : kRows) {
    const bool was = cheats::g_on[r.k];
    click(r.label);
    const bool flipped = cheats::g_on[r.k] != was;
    click(r.label);
    char what[96];
    std::snprintf(what, sizeof(what), "%s - a click flips its switch, a second puts it back", r.label);
    check(flipped && cheats::g_on[r.k] == was, what);
  }
  check(any_containing("###results") && !any_containing("###mercenaries") && !find("Endless combo"),
        "a campaign: the Results, no Mercenaries section");
  check(!find("Set##countdown"), "no countdown on screen: no Time left editor");
  cheats::g_asked.clear();
  click("Set##time");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kTime && cheats::g_asked[0].value == 3723,
        "Results: Set beside the time asks for the chapter's clock shown");
  cheats::g_asked.clear();
  overlay::g_counter[0][0] = overlay::Edit{0, true};
  frames(1);
  click("Set##c00");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kDeaths && cheats::g_asked[0].chara == 0 &&
            cheats::g_asked[0].value == 0,
        "Results: Leon's deaths set to 0 (typed), for his index");
  cheats::g_status.countdown = true;
  cheats::g_status.countdown_left = 95;
  cheats::g_status.countdown_most = 5999.99f;
  frames(2);
  cheats::g_asked.clear();
  click("Set##countdown");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kCountdown &&
            cheats::g_asked[0].value == 95,
        "a countdown on screen: Time left's Set asks for the seconds shown");
  // A match of the Mercenaries.
  dispatch::g_snap.run.mode = game::kModeMercenaries;
  merc::g_status.match = true;
  merc::g_status.offline = true;
  merc::g_status.step = 1;
  merc::g_status.score = 123456;
  merc::g_status.combo = 12;
  merc::g_status.target = 150;
  merc::g_status.time_left = 100;
  frames(2);
  picture("mercenaries");
  check(any_containing("###mercenaries") && !any_containing("###results"),
        "a match: the Mercenaries' section, no Results");
  static const struct {
    const char* label;
    sw::Kind k;
  } kMerc[] = {{"Endless combo", sw::kEndlessCombo},
               {"Combo Time always on", sw::kComboTime},
               {"Kill enemies as they appear", sw::kAutoKill}};
  for (const auto& r : kMerc) {
    const bool was = cheats::g_on[r.k];
    click(r.label);
    const bool flipped = cheats::g_on[r.k] != was;
    click(r.label);
    char what[96];
    std::snprintf(what, sizeof(what), "the Mercenaries: %s - a click flips its switch, a second puts it back", r.label);
    check(flipped && cheats::g_on[r.k] == was, what);
  }
  cheats::g_status.countdown = false;
  overlay::forget_edits();
}

void check_greyed() {
  make_game(true, true);
  cheats::g_ok[sw::kInfiniteSkillPoints] = false;
  frames(2);
  const bool was = cheats::g_on[sw::kInfiniteSkillPoints];
  click("Infinite skill points");
  check(cheats::g_on[sw::kInfiniteSkillPoints] == was, "a switch the game lacks: drawn, a click does not flip it");
  cheats::g_ok[sw::kInfiniteSkillPoints] = true;
}

void check_play() {
  // F7 in play: the panel over the game.
  make_game(true, false);
  overlay::g_user_shown = true;
  frames(2);
  picture("play");
  check(find("God mode") != nullptr, "play (F7): the panel over the game");
  cheats::g_asked.clear();
  click("Set##points");
  check(cheats::g_asked.empty(), "play: the skill points' Set does nothing (disabled)");
  overlay::g_count[0][0][0] = overlay::CountEdit{0x103, 2, true};
  frames(1);
  inventory::g_asked.clear();
  click("Apply##p0000");
  check(inventory::g_asked.empty(), "play: an inventory count's Apply does nothing (disabled)");
  click("x##x016");
  check(inventory::g_thrown.empty(), "play: Throw away does nothing (disabled)");
  overlay::forget_edits();
  overlay::g_user_shown = false;
  frames(1);
  check(!find("God mode"), "play: F7 again, the panel goes");
}

void check_menus() {
  make_game(false, false);
  overlay::g_user_shown = true;  // F7 at the title
  frames(2);
  check(!find("God mode") && !overlay::g_user_shown, "out of a game: nothing drawn, F7 does not open the panel");
}

// The records windows beside the game's Records and Dog Tag screens (the menus: no game).
void records_screen(records::Screen s) {
  make_game(false, false);
  dispatch::g_snap.records = s;
  records::g_view.screen = s;
}

void press_toggle() {
  overlay::hk_wndproc(nullptr, WM_KEYDOWN, static_cast<WPARAM>(config::get().toggle_key), 0);
}

// What the tick makes of the chapter changes asked for (records.cpp's apply_chapters), in the view the window reads
// next: as the game then holds them.
void apply_chapters_to(records::View& v) {
  for (const records::ChapterSet& s : records::g_chapters) {
    records::Cell& c = v.cell[s.chara][s.difficulty][s.chapter];
    if (!s.cleared) {
      c.cleared = false;
      continue;
    }
    if (!c.cleared && !(c.time > 0)) c.time = v.time_most;  // a first clear: the game's "no time"
    c.cleared = true;
    c.rank = s.rank;
    if (s.time >= 0) c.time = s.time < v.time_most ? s.time : v.time_most;
  }
  records::g_chapters.clear();
}

// Every chapter field of a character's tab (the one shown) reads `text`, none typed.
bool chapter_times_read(int chara, const char* text) {
  for (int d = 0; d < records::kDifficulties; ++d)
    for (int ch = 0; ch < records::kChapters; ++ch) {
      const overlay::TimeEdit& t = overlay::g_chap_time[chara][d][ch];
      if (t.typing || std::strcmp(t.text, text)) return false;
    }
  return true;
}

void check_records() {
  records::View& v = records::g_view;
  v = records::View{};
  v.known = true;
  v.achievements = v.can_save = true;
  // Chapters: Leon's Normal chapter 1 cleared at B in 1:23:45, Helena's Amateur chapter 2 at S (the game's "no
  // time"); a best time's most the chapter clock's.
  v.cell[0][1][0] = records::Cell{true, 2, 5025.2f};
  v.cell[1][0][1] = records::Cell{true, 0, 71999.0f};
  v.percent = 1;
  v.time_most = 71999.0f;
  records_screen(records::Screen::kChapters);
  frames(2);
  picture("records_chapters");
  check(find("Everyone's") && find("Leon's###allchar") && !find("God mode"),
        "the Records chapter grid: its window, never the cheat panel");
  check(any_containing("Leon 1###char0") && any_containing("Helena 1###char1"), "a tab for each character, with "
                                                                               "the records held");
  check(find("##time010") && !find("##time000") && !find("##time011") &&
            !std::strcmp(overlay::g_chap_time[0][1][0].text, "1:23:45"),
        "a cleared chapter's best time under its rank (Leon, Normal, chapter 1: 1:23:45); none under one not "
        "cleared");
  // A rank picked as a player picks it: the cell's combo opened, a rank clicked. The pick turns the cell orange from
  // the next frame; the frame it is made in pops no colour it did not push (ImGui asserts on that, and in the game
  // its assert ends the process - 2026-11-06, the medals' field).
  {
    ImGuiWindow* w = ImGui::FindWindowByName(overlay::kChaptersWindow);
    const ImGuiID combo = w ? ImHashStr("##r010", 0, ImHashStr("grid", 0, w->ID)) : 0;  // Leon, Normal, chapter 1
    const bool found = click_at(find_id(combo));
    click("A");
    frames(1);
    check(found && overlay::g_chap_edit[0][1][0] == 2 && find("Apply all###apply"),
          "a rank picked in a cell's combo (Leon, Normal, chapter 1: A, from B), as a player picks it - no ImGui "
          "assert");
  }
  overlay::g_chap_edit[0][1][0] = 1;  // Leon, Normal, chapter 1: S
  overlay::g_chap_edit[0][1][1] = 0;  // ... chapter 2: not cleared (it is not: nothing to do)
  overlay::g_chap_edit[0][4][4] = 6;  // No Hope, chapter 5: E
  frames(1);
  records::g_chapters.clear();
  click("Apply all 2###apply");
  check(records::g_chapters.size() == 2 && records::g_chapters[0].chara == 0 && records::g_chapters[0].difficulty == 1 &&
            records::g_chapters[0].chapter == 0 && records::g_chapters[0].cleared && records::g_chapters[0].rank == 0 &&
            records::g_chapters[1].difficulty == 4 && records::g_chapters[1].chapter == 4 &&
            records::g_chapters[1].rank == 5,
        "Apply all: the two real changes asked for (S for Normal 1, E for No Hope 5)");
  check(records::g_chapters[0].time < 0 && records::g_chapters[1].time < 0,
        "... a rank alone leaves the best time as it is (a first clear gets the game's \"no time\")");
  check(overlay::g_chap_edit[0][1][0] < 0, "... and let go");
  // A best time typed as a player types it: the field clicked (its text all selected), the time typed over it.
  click("##time010");
  for (const char* k = "45:00"; *k; ++k) ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(*k));
  frames(2);
  check(overlay::g_chap_time[0][1][0].typing && !std::strcmp(overlay::g_chap_time[0][1][0].text, "45:00") &&
            find("Apply all###apply"),
        "a best time typed into its field (45:00, from 1:23:45), as a player types it - no ImGui assert");
  records::g_chapters.clear();
  click("Apply all###apply");
  check(records::g_chapters.size() == 1 && records::g_chapters[0].chara == 0 && records::g_chapters[0].difficulty == 1 &&
            records::g_chapters[0].chapter == 0 && records::g_chapters[0].cleared && records::g_chapters[0].rank == 2 &&
            records::g_chapters[0].time == 2700.0f && !overlay::g_chap_time[0][1][0].typing,
        "Apply all: its time asked for (2700 s), its rank kept (B); the field let go");
  // One that is no time: nothing to apply; leaving the field (Enter) lets it go.
  click("##time010");
  for (const char* k = "1:75"; *k; ++k) ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(*k));
  frames(2);
  check(overlay::g_chap_time[0][1][0].typing && !std::strcmp(overlay::g_chap_time[0][1][0].text, "1:75") &&
            overlay::typed_time(overlay::g_chap_time[0][1][0], v.cell[0][1][0], v.time_most) < 0,
        "a time that is none (1:75): typed, nothing to apply");
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
  frame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
  frames(2);
  records::g_chapters.clear();
  click("Apply all###apply");
  check(!overlay::g_chap_time[0][1][0].typing && !std::strcmp(overlay::g_chap_time[0][1][0].text, "1:23:45") &&
            records::g_chapters.empty(),
        "... let go as the field is left (the record's time again); Apply all asks nothing");
  overlay::g_chap_all = 0;
  click("Everyone's");
  records::g_chapters.clear();
  frames(1);
  click("Apply all 2###apply");
  check(records::g_chapters.size() == 2 && !records::g_chapters[0].cleared && !records::g_chapters[1].cleared,
        "Everyone's \"-\": the two clears taken back");
  // A time for every chapter of a character, with its rank: Leon's 25 at S in 30:00.
  overlay::g_chap_all = 1;
  click("##alltime");
  for (const char* k = "30:00"; *k; ++k) ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(*k));
  frames(2);
  click("Leon's###allchar");
  records::g_chapters.clear();
  frames(1);
  click("Apply all 25###apply");
  bool all_timed = records::g_chapters.size() == 25;
  for (const records::ChapterSet& c : records::g_chapters)
    all_timed = all_timed && c.chara == 0 && c.cleared && c.rank == 0 && c.time == 1800.0f;
  check(all_timed, "Leon's with a time beside the rank: every chapter of his at S in 30:00 (1800 s)");
  // The tick applies them; the fields show the times applied (2026-11-14: after Apply the window showed the old
  // times while the game had the new).
  apply_chapters_to(v);
  frames(2);
  check(chapter_times_read(0, "0:30:00") && overlay::typed_time(overlay::g_chap_time[0][1][0], v.cell[0][1][0],
                                                                 v.time_most) < 0,
        "... applied: each of Leon's fields shows the time it now holds (0:30:00), nothing waiting");
  // The play-test's case: 0:00:00 for everyone. A record cleared in 0 seconds (the clock held at 0, as Freeze play
  // time holds it) is shown so, as Chapter Select shows it - not as the game's "no time" (19:59:59).
  click("##alltime");
  for (const char* k = "0:00:00"; *k; ++k) ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(*k));
  frames(2);
  click("Everyone's");
  records::g_chapters.clear();
  frames(1);
  click("Apply all 175###apply");
  bool zeroed = records::g_chapters.size() == 175;
  for (const records::ChapterSet& c : records::g_chapters) zeroed = zeroed && c.cleared && c.time == 0.0f;
  apply_chapters_to(v);
  frames(2);
  check(zeroed && chapter_times_read(0, "0:00:00") && overlay::shown_time(v.cell[6][4][4], v.time_most) == 0,
        "Everyone's in 0:00:00, applied: the fields show 0:00:00 (Leon's; Ada's No Hope 5, a first clear, too) - not "
        "19:59:59");
  // A real clear's time with a fraction: cut to whole seconds, as Chapter Select cuts it (not rounded).
  v.cell[0][1][0].time = 1234.7f;
  frames(1);
  check(!std::strcmp(overlay::g_chap_time[0][1][0].text, "0:20:34"),
        "a time of 1234.7 s shows 0:20:34, as Chapter Select shows it (cut, not rounded to 0:20:35)");
  overlay::forget_records_edits();
  // F7 hides it until the screen changes.
  press_toggle();
  frames(1);
  check(!find("Everyone's") && overlay::g_records_hidden, "F7: the window hidden");
  press_toggle();
  frames(1);
  check(find("Everyone's") != nullptr, "F7 again: back");
  press_toggle();
  frames(1);

  // Medals: the campaigns' and Survivors' (team battle) categories shown, the game showing the second.
  records_screen(records::Screen::kMedals);
  v.screen = records::Screen::kMedals;
  v.medal_cat = 5;
  v.medal[0].shown = v.medal[5].shown = true;
  v.medal[0].base = 3000;
  v.medal[0].n = 2;
  v.medal[0].id[0] = 6;
  v.medal[0].id[1] = 26;
  v.medal[0].count[0] = 3;
  v.medal[5].base = 3450;
  v.medal[5].n = 1;
  v.medal[5].id[0] = 0;
  frames(2);
  picture("records_medals");
  check(!overlay::g_records_hidden && any_containing("###cat0") && any_containing("###cat5") &&
            !any_containing("###cat1") && overlay::g_medal_tab == 5,
        "the medals window (a screen change shows it again): the categories shown, on the game's own");
  click("Campaigns 1/2###cat0");
  check(overlay::g_medal_tab == 0 && find("##n", 0) != nullptr, "the campaigns' tab: its medals with their counts");
  // A count typed as a player types it: the field clicked, a digit typed. The frame the first key lands in pops no
  // colour it did not push (the game ended there, 2026-11-06: ImGui's assert); Undo lets it go.
  click("##n", 0);
  ImGui::GetIO().AddInputCharacter('5');
  frames(2);
  check(overlay::g_medal_edit[0][0].typing && overlay::g_medal_edit[0][0].v == 5 && find("Apply") && find("Undo"),
        "a medal's count typed into its field (5, from 3), as a player types it - no ImGui assert; its Apply and "
        "Undo");
  click("Undo");
  frames(1);
  check(!overlay::g_medal_edit[0][0].typing && overlay::g_medal_edit[0][0].v == 3,
        "Undo: the count typed let go, the field follows the game (3)");
  overlay::g_medal_edit[0][1] = overlay::MedalEdit{42, true};
  frames(1);
  records::g_medals.clear();
  click("Apply");
  check(records::g_medals.size() == 1 && records::g_medals[0].cat == 0 && records::g_medals[0].medal == 26 &&
            records::g_medals[0].count == 42,
        "a medal's count typed and applied (its own number in the category)");
  overlay::g_medal_all = 7;
  click("Type in 2###allmedals");
  records::g_medals.clear();
  frames(1);
  click("Apply all 2###apply");
  check(records::g_medals.size() == 2 && records::g_medals[0].count == 7 && records::g_medals[1].count == 7,
        "every medal listed typed in at once, then Apply all");

  // Emblems: the first two unlocked.
  records_screen(records::Screen::kEmblems);
  v.screen = records::Screen::kEmblems;
  v.emblem[0] = 3;
  v.emblem[1] = 1;
  frames(2);
  picture("records_emblems");
  check(find("Award") && find("Tick all locked") && find("##t", 49) && !find("##t", 50),
        "the emblems window: each achievement with a tick (the unlocked ones' greyed), Award for a locked one");
  records::g_emblems.clear();
  click("Award");
  check(records::g_emblems.size() == 1 && records::g_emblems[0] == 2, "Award: the first locked emblem (3)");
  click("Tick all locked");
  records::g_emblems.clear();
  click("Award 48###award");
  check(records::g_emblems.size() == 48 && records::g_emblems.front() == 2 && records::g_emblems.back() == 49,
        "Tick all locked, then Award: the 48 locked ones");

  // Titles: Global Player (counter 4) at level 1 of 4, the Default title held.
  records_screen(records::Screen::kTitles);
  v.screen = records::Screen::kTitles;
  v.ntitles = 2;
  v.title[0] = records::TitleRow{};
  v.title[0].counter = 1;
  v.title[0].earned = 1;
  v.title[0].nlevels = 1;
  v.title[0].level[0] = records::Level{1, 0, 1};
  v.title[1] = records::TitleRow{};
  v.title[1].counter = 4;
  v.title[1].earned = 1;
  v.title[1].stars = true;
  v.title[1].progress = 23;
  v.title[1].nlevels = 4;
  const uint32_t goals[4] = {10, 50, 100, 300};
  for (int k = 0; k < 4; ++k) v.title[1].level[k] = records::Level{goals[k], static_cast<uint16_t>(11 + k), static_cast<uint8_t>(k + 1)};
  frames(2);
  picture("records_titles");
  check(find("Next") && find("All") && !find("Next", 1), "the titles window: Next and All for Global Player only (the "
                                                          "Default title held)");
  check(find("##t") && !find("##t", 1), "a tick box for Global Player only: none for a title held at every level");
  records::g_titles.clear();
  click("Next");
  click("All");
  check(records::g_titles.size() == 2 && records::g_titles[0].counter == 4 && records::g_titles[0].level == 2 &&
            records::g_titles[1].level == 4,
        "Next: level 2; All: level 4");
  check(!find("Bring their counts up###counts"), "no title held with its count short: no line for it");
  // A title held with its count short of its goal (the mod's awards before 2026-11-06 left them so: "? ? ?" in the
  // game for what earns it): the line for them, asking for each one's held level again (its count brought up only).
  v.title[1].earned = 2;
  v.title[1].behind = true;
  frames(2);
  picture("records_titles_behind");
  records::g_titles.clear();
  click("Bring their counts up###counts");
  check(records::g_titles.size() == 1 && records::g_titles[0].counter == 4 && records::g_titles[0].level == 2,
        "a title held with its count short: \"Bring their counts up\" asks for its held level (2) again");
  records_screen(records::Screen::kNone);
  frames(2);
  check(!find("Next") && !find("God mode"), "the screen closed: nothing drawn");
}

// The skills window beside the game's skill screen: four skills - J'avo Killer locked (12 of 30), Zombie Hunter's
// requirement met (sold by the game), Handgun Master unlocked, Infinite Handgun bought.
void check_skills() {
  records::View& v = records::g_view;
  v = records::View{};
  v.known = true;
  v.nskills = 4;
  v.skill[0] = records::Skill{16, 0, records::Need::kKills, false, 12, 30, 3000};
  v.skill[1] = records::Skill{18, 0, records::Need::kKills, true, 30, 30, 3000};
  v.skill[2] = records::Skill{21, 2, records::Need::kWeaponKills, false, 40, 1500, 37000};
  v.skill[3] = records::Skill{29, 3, records::Need::kCampaigns, false, 2, 4, 79000};
  records_screen(records::Screen::kSkills);
  frames(2);
  picture("records_skills");
  check(find("Unlock all###unlockall") && find("Lock all###lockall") && !find("God mode"),
        "the skill screen: the skills window (Unlock all, Lock all), never the cheat panel");
  check(find("##s", 3) && !find("##s", 4), "a tick for each skill listed");
  records::g_skills.clear();
  click("##s", 0);
  check(records::g_skills.size() == 1 && records::g_skills[0].id == 16 && records::g_skills[0].unlocked,
        "a locked skill's tick (J'avo Killer): unlock it");
  records::g_skills.clear();
  click("##s", 2);
  check(records::g_skills.size() == 1 && records::g_skills[0].id == 21 && !records::g_skills[0].unlocked,
        "an unlocked one's (Handgun Master): lock it again");
  records::g_skills.clear();
  click("##s", 1);
  click("##s", 3);
  check(records::g_skills.empty(), "the ticks of one whose requirement is met and of one bought: fixed, nothing asked");
  overlay::forget_records_edits();
  click("Unlock all###unlockall");
  check(records::g_skills.size() == 1 && records::g_skills[0].id == 16 && records::g_skills[0].unlocked,
        "Unlock all: just the locked one");
  records::g_skills.clear();
  click("Lock all###lockall");
  check(records::g_skills.size() == 1 && records::g_skills[0].id == 21 && !records::g_skills[0].unlocked,
        "Lock all: just the unlocked one not bought nor sold by the game");
  // Two locked: Unlock all counts them.
  v.skill[2].state = 0;
  overlay::forget_records_edits();
  frames(2);
  check(find("Unlock all 2###unlockall") && find("Lock all###lockall"), "Unlock all with the count it changes (2)");
  records::g_skills.clear();
  press_toggle();
  frames(2);
  check(!find("Unlock all 2###unlockall") && overlay::g_records_hidden, "F7: the window hidden");
  press_toggle();
  frames(2);
  check(find("Unlock all 2###unlockall") && records::g_skills.empty(), "F7 again: back, nothing asked");
  records_screen(records::Screen::kNone);
  frames(2);
  check(!find("Unlock all 2###unlockall") && !find("God mode"), "the skill screen closed: nothing drawn");
}

// The weapons window beside the Records screen's WEAPONS page: Leon with his Shotgun (8 loaded) and Semi-Auto Sniper
// Rifle, not his Assault Rifle RN or Lightning Hawk; Helena with none of the four; Jake without his Shotgun; the Agent
// not set up (her Assault Shotgun's tick fixed). The page: the Shotgun used in play (5 uses), the Semi-Auto SR and the
// Incendiary Grenade one use each, the Assault Shotgun, Lightning Hawk, Assault Rifle RN and Physical Attack -
// Counter "? ? ?".
void check_weapons() {
  records::View& v = records::g_view;
  v = records::View{};
  v.known = true;
  v.achievements = v.can_save = true;
  const struct {
    uint8_t entry;
    uint32_t uses, kills;
  } rows[] = {{3, 5, 2}, {4, 0, 0}, {6, 0, 0}, {9, 1, 0}, {16, 0, 0}, {19, 1, 0}, {29, 0, 0}};
  for (const auto& r : rows) {
    records::WeaponRow& w = v.weapon_row[v.nweapon_rows++];
    w.entry = r.entry;
    w.uses = r.uses;
    w.kills = r.kills;
    w.played = r.uses > 1 || r.kills > 0;
  }
  v.nweapon_always = 15;
  v.carried_known = true;
  auto weapon = [](uint16_t id, int8_t entry, int16_t count) { return records::Carried{id, entry, count != -2, count}; };
  const uint16_t leons[4] = {0x104, 0x10A, 0x111, 0x107};
  const int8_t leon_rows[4] = {3, 9, 16, 6};
  const int16_t leon_has[4] = {8, -1, -2, -2};
  for (int c : {0, 1}) {
    v.carrier[c].ready = true;
    v.carrier[c].n = 4;
    for (int k = 0; k < 4; ++k) v.carrier[c].weapon[k] = weapon(leons[k], leon_rows[k], c == 0 ? leon_has[k] : -2);
  }
  v.carrier[4].ready = true;
  v.carrier[4].n = 1;
  v.carrier[4].weapon[0] = weapon(0x104, 3, -2);
  v.carrier[7].n = 1;
  v.carrier[7].weapon[0] = weapon(0x105, 4, -2);
  records_screen(records::Screen::kWeapons);
  frames(2);
  picture("records_weapons");
  check(find("Give every weapon 7###giveall") && find("Name every row 4###nameall") && !find("God mode"),
        "the WEAPONS page: the weapons window (Give every weapon 7, Name every row 4), never the cheat panel");
  check(find("##c0", 3) && !find("##c0", 4) && find("##c1", 3) && find("##c4", 0) && !find("##c4", 1) &&
            find("##c7", 0) && !find("##c2", 0) && find("##page", 6) && !find("##page", 7),
        "a tick where a character's campaign has the weapon (Leon's and Helena's four, Jake's Shotgun, the Agent's "
        "Assault Shotgun), none where it has not; a page tick for each of the 7 rows");
  check(find("All###char0") && find("All###char1") && find("All###char4") && !find("All###char7") &&
            find("All###row", 4) && !find("All###row", 5),
        "All for each character set up (not the Agent), and for each of the five weapons");
  records::g_carried.clear();
  click("##c1", 0);
  check(records::g_carried.size() == 1 && records::g_carried[0].chara == 1 && records::g_carried[0].id == 0x104 &&
            records::g_carried[0].had,
        "Helena's Shotgun (not had): a click gives it");
  records::g_carried.clear();
  click("##c0", 0);
  check(records::g_carried.size() == 1 && records::g_carried[0].chara == 0 && records::g_carried[0].id == 0x104 &&
            !records::g_carried[0].had,
        "Leon's Shotgun (had): a click takes it out");
  records::g_carried.clear();
  records::g_weapon_rows.clear();
  click("##c7", 0);
  click("##page", 0);
  check(records::g_carried.empty() && records::g_weapon_rows.empty(),
        "fixed, nothing asked: the Agent's tick (her saved inventory not set up), the Shotgun's page tick (used in "
        "play)");
  click("##page", 2);
  click("##page", 3);
  check(records::g_weapon_rows.size() == 2 && records::g_weapon_rows[0].entry == 6 && records::g_weapon_rows[0].named &&
            records::g_weapon_rows[1].entry == 9 && !records::g_weapon_rows[1].named,
        "page ticks: the Lightning Hawk (\"? ? ?\") named, the Semi-Auto SR's one use taken back");
  overlay::forget_records_edits();
  frames(1);
  records::g_carried.clear();
  click("All###char1");
  bool helenas = records::g_carried.size() == 4;
  for (const records::CarrySet& s : records::g_carried) helenas = helenas && s.chara == 1 && s.had;
  check(helenas, "Helena's All: her four given");
  overlay::forget_records_edits();
  frames(1);
  records::g_carried.clear();
  records::g_weapon_rows.clear();
  click("All###row", 0);
  check(records::g_carried.size() == 2 && records::g_carried[0].chara == 1 && records::g_carried[1].chara == 4 &&
            records::g_carried[0].id == 0x104 && records::g_carried[1].id == 0x104 && records::g_weapon_rows.empty(),
        "the Shotgun's All: Helena's and Jake's (Leon has it, the page names it)");
  overlay::forget_records_edits();
  frames(1);
  records::g_carried.clear();
  click("Give every weapon 7###giveall");
  bool ready_only = records::g_carried.size() == 7;
  for (const records::CarrySet& s : records::g_carried) ready_only = ready_only && s.had && s.chara != 7;
  check(ready_only, "Give every weapon: the 7 not had, of the characters set up (not the Agent's)");
  overlay::forget_records_edits();
  frames(1);
  records::g_weapon_rows.clear();
  click("Name every row 4###nameall");
  check(records::g_weapon_rows.size() == 4 && records::g_weapon_rows[0].entry == 4 &&
            records::g_weapon_rows[3].entry == 29,
        "Name every row: the 4 \"? ? ?\" (the Assault Shotgun to Physical Attack - Counter)");
  overlay::forget_records_edits();
  records::g_carried.clear();
  records::g_weapon_rows.clear();
  press_toggle();
  frames(2);
  check(!find("Give every weapon 7###giveall") && overlay::g_records_hidden, "F7: the window hidden");
  press_toggle();
  frames(2);
  check(find("Give every weapon 7###giveall") && records::g_carried.empty() && records::g_weapon_rows.empty(),
        "F7 again: back, nothing asked");
  records_screen(records::Screen::kNone);
  frames(2);
  check(!find("Give every weapon 7###giveall") && !find("God mode"), "the WEAPONS page closed: nothing drawn");
}

// The enemies window beside the Records screen's ENEMIES page: five rows - Neo-Umbrella J'avo with kills, Glava-
// Begunats "? ? ?", Ustanak named by its Times Defeated alone, the Crow "? ? ?" (the page shows no Times Defeated for
// it), The Witch "? ? ?".
void check_enemies() {
  records::View& v = records::g_view;
  v = records::View{};
  v.known = true;
  v.achievements = v.can_save = true;
  const struct {
    uint8_t entry;
    int32_t kills, defeated;
    bool shows;
  } rows[] = {{1, 239, 0, true}, {13, 0, 0, true}, {39, 0, 2, true}, {80, 0, 0, false}, {85, 0, 0, true}};
  for (const auto& r : rows) {
    records::EnemyRow& e = v.enemy_row[v.nenemy_rows++];
    e.entry = r.entry;
    e.kills = r.kills;
    e.defeated = r.defeated;
    e.shows_defeated = r.shows;
  }
  v.nenemy_always = 2;
  records_screen(records::Screen::kEnemies);
  frames(2);
  picture("records_enemies");
  check(find("Reveal all 3###revealall") && find("\"? ? ?\" only###hiddenonly") && !find("God mode"),
        "the ENEMIES page: the enemies window (Reveal all 3, the \"? ? ?\" filter), never the cheat panel");
  check(find("##e", 4) && !find("##e", 5), "a tick for each of the 5 rows");
  records::g_enemies.clear();
  click("##e", 0);
  click("##e", 2);
  check(records::g_enemies.empty(),
        "the ticks of a named row - by its kills (Neo-Umbrella J'avo), by its Times Defeated alone (Ustanak): fixed, "
        "nothing asked");
  click("##e", 1);
  check(records::g_enemies.size() == 1 && records::g_enemies[0] == 13, "a \"? ? ?\" row's tick (Glava-Begunats): reveal it");
  frames(1);
  check(find("Reveal all 2###revealall"), "... shown named until the game says so: Reveal all 2");
  records::g_enemies.clear();
  click("##e", 1);
  check(records::g_enemies.empty(), "... and its tick then fixed (a reveal is not taken back)");
  overlay::forget_records_edits();
  frames(1);
  click("Reveal all 3###revealall");
  check(records::g_enemies.size() == 3 && records::g_enemies[0] == 13 && records::g_enemies[1] == 80 &&
            records::g_enemies[2] == 85,
        "Reveal all: the 3 \"? ? ?\" rows (Glava-Begunats, the Crow, The Witch)");
  overlay::forget_records_edits();
  frames(1);
  records::g_enemies.clear();
  click("\"? ? ?\" only###hiddenonly");
  frames(1);
  check(overlay::g_enemy_hidden_only && find("##e", 2) && !find("##e", 3),
        "\"? ? ?\" only: the 3 \"? ? ?\" rows listed");
  click("##e", 2);
  check(records::g_enemies.size() == 1 && records::g_enemies[0] == 85, "... their ticks as before (The Witch)");
  click("\"? ? ?\" only###hiddenonly");
  overlay::forget_records_edits();
  frames(1);
  check(!overlay::g_enemy_hidden_only && find("##e", 4), "the filter off: all 5 again");
  records::g_enemies.clear();
  press_toggle();
  frames(2);
  check(!find("Reveal all 3###revealall") && overlay::g_records_hidden, "F7: the window hidden");
  press_toggle();
  frames(2);
  check(find("Reveal all 3###revealall") && records::g_enemies.empty(), "F7 again: back, nothing asked");
  records_screen(records::Screen::kNone);
  frames(2);
  check(!find("Reveal all 3###revealall") && !find("God mode"), "the ENEMIES page closed: nothing drawn");
}


// The Mercenaries window beside the stage select (and the character select): the Mercenaries' ten stages (the first
// three always open, the next three open), and the profile's nine characters as the user's save held them on
// 2026-11-15 - each one's Default and Costume 1 (Carla, the Agent), Leon's, Chris', Jake's Default and the Agent
// always there.
void check_merc() {
  records::View& v = records::g_view;
  v = records::View{};
  v.known = true;
  v.achievements = v.can_save = true;
  const uint16_t number[10] = {1202, 1206, 1203, 1200, 1204, 1205, 1201, 1209, 1207, 1208};
  auto stages = [&](bool no_mercy) {
    v.merc_no_mercy = no_mercy;
    v.merc_nstages = 10;
    for (int i = 0; i < 10; ++i) {
      records::MercStage& s = v.merc_stage[i];
      s.bit = static_cast<uint8_t>(i + (no_mercy ? 10 : 0));
      s.number = static_cast<uint16_t>(number[i] + (no_mercy ? 10 : 0));
      s.always = i < 3;
      s.open = i < 6;
    }
  };
  stages(false);
  const uint64_t held = 0x33333333ull;
  v.merc_nfamilies = 9;
  for (int f = 0; f < 9; ++f) {
    records::MercFamily& fm = v.merc_family[f];
    fm.family = static_cast<uint8_t>(f);
    fm.n = f < 7 ? 4 : 1;
    for (int k = 0; k < fm.n; ++k) {
      records::MercCostume& c = fm.costume[k];
      c.bit = static_cast<uint8_t>(f < 7 ? 4 * f + k : 21 + f);
      c.costume = static_cast<uint8_t>(f < 7 ? k : 0);
      c.always = c.bit == 0 || c.bit == 8 || c.bit == 16 || c.bit == 29;
      c.open = c.always || ((held >> c.bit) & 1);
    }
  }
  records::g_merc.clear();
  records_screen(records::Screen::kMercenaries);
  frames(2);
  picture("records_merc");
  check(find("Open every stage 4###openall") && find("Unlock every costume 14###unlockall") && !find("God mode"),
        "the stage select: the Mercenaries window (Open every stage 4, Unlock every costume 14), never the cheat panel");
  check(find("##s", 9) && !find("##s", 10) && find("##c0", 8) && !find("##c0", 9) && find("##c3", 6) && !find("##c3", 7),
        "a tick for each of the 10 stages; a Default tick for the 9 characters, the costumes' for the 7 with four");
  click("##s", 0);
  click("##c0", 0);
  click("##c0", 8);
  check(records::g_merc.empty(), "the ticks of a stage always open (Urban Chaos), of Leon's Default and the Agent: "
                                 "fixed, nothing asked");
  click("##s", 6);
  check(records::g_merc.size() == 1 && records::g_merc[0].stage && records::g_merc[0].bit == 6 && records::g_merc[0].open,
        "a closed stage's tick (Requiem for War, bit 6): open it");
  click("##s", 3);
  check(records::g_merc.size() == 2 && records::g_merc[1].stage && records::g_merc[1].bit == 3 && !records::g_merc[1].open,
        "an open one's (Rail Yard, bit 3): close it again");
  click("##c2", 0);
  check(records::g_merc.size() == 3 && !records::g_merc[2].stage && records::g_merc[2].bit == 2 && records::g_merc[2].open,
        "Leon's Costume 2 (bit 2, locked): unlock it");
  click("##c1", 1);
  check(records::g_merc.size() == 4 && !records::g_merc[3].stage && records::g_merc[3].bit == 5 && !records::g_merc[3].open,
        "Helena's Costume 1 (bit 5, unlocked): lock it again");
  frames(1);
  check(find("Open every stage 4###openall") && find("Unlock every costume 14###unlockall"),
        "... each shown as clicked until the game says so (Requiem open, Rail Yard closed: 4; Leon's Costume 2 in, "
        "Helena's Costume 1 out: 14)");
  overlay::forget_records_edits();
  frames(1);
  records::g_merc.clear();
  click("All###fam", 0);
  check(records::g_merc.size() == 2 && records::g_merc[0].bit == 2 && records::g_merc[1].bit == 3 &&
            records::g_merc[0].open && records::g_merc[1].open,
        "Leon's All: his Costumes 2 and 3 (the two locked)");
  overlay::forget_records_edits();
  frames(1);
  records::g_merc.clear();
  click("Unlock every costume 14###unlockall");
  bool every = records::g_merc.size() == 14;
  for (size_t i = 0; i < records::g_merc.size() && every; ++i) {
    const records::MercSet& m = records::g_merc[i];
    every = !m.stage && m.open && ((m.bit & 3) == 2 || (m.bit & 3) == 3) && m.bit < 28;
  }
  check(every, "Unlock every costume: the 14 locked (each character's Costumes 2 and 3)");
  overlay::forget_records_edits();
  frames(1);
  records::g_merc.clear();
  click("Open every stage 4###openall");
  check(records::g_merc.size() == 4 && records::g_merc[0].bit == 6 && records::g_merc[3].bit == 9 &&
            records::g_merc[0].stage && records::g_merc[3].open,
        "Open every stage: the 4 closed (Requiem for War ... Creature Workshop)");
  overlay::forget_records_edits();
  // No Mercy (its character select): its own stages' bits.
  stages(true);
  records::g_merc.clear();
  frames(2);
  click("##s", 7);
  check(records::g_merc.size() == 1 && records::g_merc[0].stage && records::g_merc[0].bit == 17 && records::g_merc[0].open,
        "No Mercy: its stages' ticks ask for its own bits (its Liquid Fire, bit 17)");
  overlay::forget_records_edits();
  records::g_merc.clear();
  press_toggle();
  frames(2);
  check(!find("Open every stage 4###openall") && overlay::g_records_hidden, "F7: the window hidden");
  press_toggle();
  frames(2);
  check(find("Open every stage 4###openall") && records::g_merc.empty(), "F7 again: back, nothing asked");
  records_screen(records::Screen::kNone);
  frames(2);
  check(!find("Open every stage 4###openall") && !find("God mode"), "the select screen closed: nothing drawn");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: test_panel.exe <scratch dir> [shots]\n");
    return 2;
  }
  if (argc > 2 && std::strcmp(argv[2], "shots") == 0) g_shots = argv[1];
  ImGui::CreateContext();
  GImGui->TestEngineHookItems = true;
  overlay::g_context_ready = true;
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(overlay::kScale);
  style.FontScaleMain = overlay::kScale;
  check_pause();
  check_inventory();
  check_sections();
  check_greyed();
  check_play();
  check_menus();
  check_records();
  check_skills();
  check_weapons();
  check_enemies();
  check_merc();
  ImGui::DestroyContext();
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
