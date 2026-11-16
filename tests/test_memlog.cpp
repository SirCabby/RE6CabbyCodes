// Offline test of the memory log (src/memlog.cpp, the MemoryLog diagnostic): its reading of a mapping list - a
// made-up 32-bit one with every kind and gap known, a 64-bit one - and of the live one under Wine, the Windows view,
// the mod's heap count (operator new and ImGui's allocator), the ini key, and the lines and mapping dumps its thread
// writes at start, at a load and at a crash. And the frame log (src/framelog.cpp, the FrameLog diagnostic, 2026-11-13):
// its ini key, made-up frames through it - its line a second and a long frame's own line. memlog.cpp, framelog.cpp,
// config.cpp and log.cpp are built into this file (their insides are reached by name). Build:
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static tests/test_memlog.cpp
//       -o tests/build/test_memlog.exe
//   wine tests/build/test_memlog.exe 'Z:\some\scratch\dir\'     (the dir empty, its trailing backslash kept)
#include "../src/memlog.cpp"
#include "../src/framelog.cpp"
#include "../src/config.cpp"
#include "../src/log.cpp"

#include <string>
#include <vector>

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

std::string read_file(const std::string& path) {
  std::string s;
  if (FILE* f = std::fopen(path.c_str(), "rb")) {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
  }
  return s;
}

void write_file(const std::string& path, const char* text) {
  if (FILE* f = std::fopen(path.c_str(), "w")) {
    std::fputs(text, f);
    std::fclose(f);
  }
}

// The file holding `what`, within a few seconds (the memory log's thread writes it).
bool soon(const std::string& path, const char* what) {
  for (int i = 0; i < 100; ++i) {
    if (read_file(path).find(what) != std::string::npos) return true;
    Sleep(50);
  }
  return false;
}

// Lines of a mapping list through the parser, as read_maps hands them over.
re6cc::memlog::Maps parse(const char* text) {
  re6cc::memlog::Maps m;
  std::string all(text);
  size_t at = 0;
  while (at < all.size()) {
    size_t nl = all.find('\n', at);
    if (nl == std::string::npos) nl = all.size();
    re6cc::memlog::add(&m, all.substr(at, nl - at).c_str());
    at = nl + 1;
  }
  re6cc::memlog::finish(&m);
  return m;
}

// A device whose only method asked is GetAvailableTextureMem (slot 4): 1536 MB.
UINT __stdcall fake_available(void*) { return 1536u * 1024 * 1024; }
void* g_fake_vtable[8] = {nullptr, nullptr, nullptr, nullptr, reinterpret_cast<void*>(&fake_available)};
struct {
  void** vtable;
} g_fake_device = {g_fake_vtable};
}  // namespace

int main(int argc, char** argv) {
  using namespace re6cc;
  using namespace re6cc::memlog;
  const std::string dir = argc > 1 ? argv[1] : ".\\";
  const std::string log = dir + "RE6CabbyCodes.log", ini = dir + "RE6CabbyCodes.ini";
  const std::string maps = dir + "RE6CabbyCodes.maps.txt", maps_prev = dir + "RE6CabbyCodes.maps.prev.txt";

  // --- a made-up 32-bit list: every kind, its sizes and its gaps known ---
  std::snprintf(g_game, sizeof(g_game), "/Resident Evil 6/");
  {
    const Maps m = parse(
        "00010000-00110000 ---p 00000000 00:00 0 \n"                                           // reserved 1 MB
        "00110000-00510000 rw-p 00000000 00:00 0\n"                                            // anon 4 MB
        "00600000-01600000 r-xp 00000000 08:01 11   /x/steamapps/common/Resident Evil 6/BH6.exe\n"  // game 16 MB
        "10000000-20000000 rw-s 00000000 00:05 12   /dev/nvidiactl\n"                          // gpu 256 MB
        "20000000-28000000 r-xp 00000000 08:01 13   /usr/lib32/libnvidia-gpucomp.so.615.78.08\n"  // nvidia 128 MB
        "28000000-28400000 r-xp 00000000 08:01 14   /p/Proton/files/lib/wine/i386-unix/ntdll.so\n"  // wine 4 MB
        "28400000-28800000 r--p 00000000 08:01 15   /p/pfx/drive_c/windows/syswow64/d3d9.dll\n"  // windows dlls 4 MB
        "28800000-28c00000 r-xp 00000000 08:01 16   /h/.local/share/Steam/ubuntu12_32/gameoverlayrenderer.so\n"
        "28c00000-29000000 rw-s 00000000 00:01 17   /memfd:wine-shm (deleted)\n"              // shared 4 MB
        "29000000-29400000 r--p 00000000 08:01 18   /usr/lib/locale/locale-archive\n"          // files 4 MB
        "f0000000-f0400000 rw-p 00000000 00:00 0    [heap]\n"                                   // [heap] 4 MB
        "ffefe000-ffffe000 rw-p 00000000 00:00 0    [stack]");                                  // [stack] 1 MB, no newline
    const uint64_t M = 1024 * 1024;
    check(m.ok && !m.wide && m.lines == 12, "a 32-bit list: 12 mappings read, the last without its line end");
    check(m.kind[kReserved] == 1 * M && m.kind[kAnon] == 4 * M, "a 32-bit list: Wine's reserve (no access) apart from anon");
    check(m.kind[kGame] == 16 * M && m.kind[kGpu] == 256 * M && m.kind[kNvidia] == 128 * M,
          "a 32-bit list: the game's files, the GPU's mappings, the NVIDIA libraries");
    check(m.kind[kWine] == 4 * M && m.kind[kWinDlls] == 4 * M && m.kind[kSteam] == 4 * M && m.kind[kShared] == 4 * M &&
              m.kind[kFiles] == 4 * M && m.kind[kHeap] == 4 * M && m.kind[kStack] == 1 * M,
          "a 32-bit list: Wine's, the prefix's DLLs, Steam's, shared, other files, [heap], [stack]");
    check(m.mapped == 430 * M, "a 32-bit list: 430 MB mapped in all");
    // The gaps: 0x510000-0x600000, 0x1600000-0x10000000, 0x29400000-0xF0000000 (the largest),
    // 0xF0400000-0xFFEFE000; nothing above the stack, at the top.
    check(m.unmapped == 0xE51EE000ull && m.gap == 0xC6C00000ull && m.mapped + m.unmapped == kTop32 - kLow,
          "a 32-bit list: 3665 MB not mapped, its largest gap 3180 MB, all of the 4 GB accounted for");
  }
  // --- a 64-bit process (Proton's new WoW64): the low 4 GB is the program's, the rest counted apart ---
  {
    const Maps m = parse(
        "00010000-00110000 rw-p 00000000 00:00 0\n"
        "fff00000-100100000 rw-p 00000000 00:00 0\n"
        "7f0000000000-7f0000100000 r-xp 00000000 08:01 1   /usr/lib/libnvidia-glcore.so.615.78.08\n");
    // Not mapped: 0x110000-0xFFF00000 only (the second mapping reaches 4 GB, the top of a 64-bit process's low part).
    check(m.ok && m.wide && m.lines == 3 && m.high == 0x200000ull && m.kind[kNvidia] == 0 && m.mapped == 0x200000ull &&
              m.unmapped == 0xFFDF0000ull && m.gap == 0xFFDF0000ull,
          "a 64-bit list: the part above 4 GB apart (a mapping across it split), the low 4 GB's gaps");
  }

  // --- the live list and the Windows view ---
  static char buf[64 * 1024];
  {
    find_list(buf, sizeof(buf));
    std::printf("      (this process's list: %s)\n", g_list[0] ? g_list : "not found");
    check(g_list[0] && std::strstr(g_list, "\\proc\\self\\") == nullptr,
          "this process's mapping list found by its pid (/proc/self is Wine's server, not this process)");
    const Maps m = read_maps(buf, sizeof(buf), nullptr);
    uint64_t sum = 0;
    for (uint64_t k : m.kind) sum += k;
    std::printf("      (this process: %s, %d mappings, %u MB under 4 GB, %u MB not mapped, largest gap %u MB)\n",
                m.wide ? "64-bit" : "32-bit", m.lines, mb(m.mapped), mb(m.unmapped), mb(m.gap));
    check(m.ok && m.lines > 20 && sum == m.mapped && m.kind[kAnon] > 0,
          "this process's own list read (Wine's unix path), every mapping in a kind");
    const Win w = read_windows();
    check(w.image > 0 && w.priv > 0 && w.free > 0 && w.largest > 0 && w.largest <= w.free,
          "the Windows view: committed images and private memory, free room and its largest block");
  }

  // --- the mod's heap: operator new and ImGui's allocator, counted on and off ---
  {
    const LONG before = g_heap_bytes, blocks = g_heap_blocks;
    auto* v = new std::vector<char>(1 << 20);
    const LONG during = g_heap_bytes;
    delete v;
    check(during - before >= (1 << 20) && g_heap_bytes == before && g_heap_blocks == blocks,
          "the mod's heap: a 1 MB vector counted, and gone once deleted");
    void* p = heap_alloc(1000, nullptr);
    const LONG with = g_heap_bytes;
    heap_free(p, nullptr);
    check(p && with - before >= 1000 && g_heap_bytes == before, "the mod's heap: ImGui's allocator counted the same way");
  }

  // --- the ini: the key written with the rest, read, held to its bounds ---
  log_init(dir.c_str());
  DeleteFileA(ini.c_str());
  config::load(dir.c_str());
  const std::string fresh = read_file(ini);
  check(fresh.find("MemoryLog = 0") != std::string::npos && fresh.find("#   MemoryLog   = 0") != std::string::npos &&
            config::get().memory_log == 0,
        "the ini: a first start writes MemoryLog = 0 (off), documented");
  check(fresh.find("\nFrameLog = 0") != std::string::npos && fresh.find("#   FrameLog    = 0") != std::string::npos &&
            !config::get().frame_log && !framelog::on(),
        "the ini: a first start writes FrameLog = 0 (off), documented");
  write_file(ini, "FrameLog = 1 ; a line a second\n");
  config::load(dir.c_str());
  check(config::get().frame_log && framelog::on() && read_file(log).find("FrameLog=1") != std::string::npos,
        "the ini: FrameLog = 1 read (and logged with the config)");
  write_file(ini, "MemoryLog = 2\n");
  config::load(dir.c_str());
  check(config::get().memory_log == 5, "the ini: MemoryLog = 2 is taken as 5 s, the least");
  write_file(ini, "MemoryLog = 30 ; every half a minute\n");
  config::load(dir.c_str());
  check(config::get().memory_log == 30 && read_file(log).find("MemoryLog=30") != std::string::npos,
        "the ini: MemoryLog = 30 read (and logged with the config)");

  // --- the frame log: made-up frames (microseconds) through it ---
  {
    using namespace re6cc::framelog;
    const long long t = 5000000;
    // The screen's text in a buffer of the caller's, as dispatch's tick passes it: gone (here overwritten) once
    // tick() returns - the long frame's line, written later, must still say the screen (2026-11-13: it printed the
    // dead buffer's bytes).
    char where[48];
    std::snprintf(where, sizeof(where), "%s", "in a game, play");
    tick(t, where);  // the line's second starts here
    std::memset(where, '#', sizeof(where) - 1);
    where[sizeof(where) - 1] = '\0';
    // Frame 1, 16 ms: the game's frame 9.05 ms with a Present inside it (the mod's part 0.05 ms, the device's 4 ms).
    begin(t);
    game_begin();
    present(50, 4000);
    game_end();
    end(30, 9050, 10);
    // Frame 2, 164 ms: the game's frame 150 ms, the Present after it (the mod's part 0.04 ms, the device's 6 ms).
    begin(t + 16000);
    game_begin();
    game_end();
    present(40, 6000);
    end(20, 150000, 0);
    begin(t + 180000);  // frame 2 ends: over 100 ms, a line of its own
    end(25, 8000, 5);
    end_scene(t + 1000);
    end_scene(t + 21000);
    end_scene(t + 171000);  // the draw thread's longest gap: 150 ms
    made(kShader, 1200);
    made(kShader, 1200);
    made(kShader, 1200);
    made(kTexture, 500);
    const std::string hitch = read_file(log);
    check(hitch.find("frame: a 164.0 ms frame (in a game, play) - the game's frame 150.0 ms, Present 6.0, the mod 0.060, "
                     "the rest 7.9") != std::string::npos,
          "the frame log: a 164 ms frame gets its own line, split - the game's frame, Present, the mod, the rest");
    tick(t + 999000, "in a game, play");
    check(read_file(log).find("frame: in a game, play |") == std::string::npos, "the frame log: no line before a second");
    tick(t + 1000000, "in a game, play");
    const std::string line = read_file(log);
    check(line.find("frame: in a game, play | 2 frames in 1.00 s, 2 presents - average 90.0 ms, the longest 164.0 "
                    "(1 over 33 ms, 1 over 100)") != std::string::npos,
          "the frame log's line a second: 2 frames (16 and 164 ms), their average and the longest, those over 33 and 100 ms");
    check(line.find("| the game's frame 77.5, the longest 150.0 | Present 5.0, the longest 6.0 |") != std::string::npos,
          "the frame log's line: the game's frame (a Present inside it taken out) and Present");
    check(line.find("| the draw thread 3 scenes, the longest gap 150.0 |") != std::string::npos,
          "the frame log's line: the draw thread's scenes and its longest gap");
    check(line.find("| the mod 0.075, the longest 0.090 | made 1 textures (0.5 ms), 3 shaders (3.6 ms)") != std::string::npos,
          "the frame log's line: the mod's own time a frame (its tick, after-frame and part of Present), what was made");
    tick(t + 2000000, "in a game, play");
    const std::string next = read_file(log);
    const size_t second = next.rfind("frame: in a game, play |");
    check(second != std::string::npos && next.find("| 1 frames in", second) == std::string::npos &&
              next.find("| 0 frames in 1.00 s, 0 presents", second) != std::string::npos &&
              next.find("made ", second) == std::string::npos,
          "the frame log: the counts start again each second (none made, no frame closed since)");
  }

  // --- the thread: a line at start; the run before's mappings kept ---
  write_file(maps, "the run before\n");
  start();
  check(read_file(maps_prev).find("the run before") == 0, "start: the run before's mappings kept as RE6CabbyCodes.maps.prev.txt");
  check(soon(log, "mem: at start: ") && soon(log, "MB mapped under 4 GB") && soon(log, "| windows: committed") &&
            soon(log, "texture memory left not asked yet") && soon(log, "the mod's heap: "),
        "start: a memory line - the mappings, the Windows view, Direct3D's (not asked yet), the mod's heap");
  // --- the present hook asks the device, a load pokes the thread ---
  on_present(reinterpret_cast<IDirect3DDevice9*>(&g_fake_device));
  check(g_tex_mb == 1536, "the present hook: the device's texture memory taken (1536 MB)");
  poke(true);
  check(soon(log, "mem: a game started: ") && soon(log, "texture memory left 1536 MB"),
        "a game started: a line at once, with the texture memory");
  check(soon(maps, "the first game's start ===") && read_file(maps).find(" r-xp ") != std::string::npos,
        "a game started: the first game's mappings into RE6CabbyCodes.maps.txt");
  poke(false);
  check(soon(log, "mem: a game ended: "), "a game ended: a line at once");
  // --- the crash ---
  on_crash();
  const std::string after = read_file(log);
  check(after.find("mem: at the crash: ") != std::string::npos &&
            after.find("the mappings at the crash written to") != std::string::npos,
        "the crash: a last memory line");
  const std::string dumped = read_file(maps);
  const size_t crash_at = dumped.find(" - the crash ===");
  check(crash_at != std::string::npos && dumped.find(" r-xp ", crash_at) != std::string::npos,
        "the crash: the mappings at the crash under their own heading");

  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  std::fflush(stdout);
  ExitProcess(g_fail ? 1 : 0);  // the memory log's thread runs on: the process ends here
}
