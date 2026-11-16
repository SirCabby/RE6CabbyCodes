// Offline smoke test of the mod's DLL as the game loads it (build/steam_api.dll, the real one): a stand-in for
// BH6.exe that imports what the game imports from steam_api.dll, d3d9.dll, kernel32, user32 and dinput8 is run
// under Wine beside a copy of the mod as steam_api.dll, the game's own Steam DLL as steam_api_orig.dll, and another
// tool's d3d9.dll (tests/fake_d3d9.cpp, standing in for ReShade's) - so the mod's DllMain runs as the program loads,
// before this program's own code, as it does in the game. Build, then run in an empty scratch dir:
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -shared -static -Wl,--kill-at tests/fake_d3d9.cpp -o tests/build/fake_d3d9.dll
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_boot.cpp build/steam_api.dll
//       -ld3d9 -ldinput8 -ldxguid -luser32 -o tests/build/test_boot.exe
//   cp tests/build/test_boot.exe build/steam_api.dll <scratch>/
//   cp "$GAME_DIR/steam_api.dll" <scratch>/steam_api_orig.dll; cp tests/build/fake_d3d9.dll <scratch>/d3d9.dll
//   cd <scratch> && WINEDLLOVERRIDES=d3d9=n,b wine test_boot.exe
//
// - the mod's DllMain ran first: this program's import slots for SetUnhandledExceptionFilter, ExitProcess,
//   DirectInput8Create, GetAsyncKeyState and Direct3DCreate9 point into the mod (its import hooks, from DllMain);
// - the Steam API is passed on: the mod's SteamAPI_IsSteamRunning is a jump into the game's own (steam_api_orig.dll);
// - the hooks pass the calls on: GetAsyncKeyState answers, and the program's own exception filter is chained under
//   the mod's, which stays on top;
// - Direct3DCreate9 goes through the mod to the d3d9.dll beside it - another tool's, standing in for ReShade - and
//   on to the system's; the mod's boot runs at it (its log says this program is not the game: no discovery);
// - an abort in the C runtime this program shares with the mod (ucrtbase) is logged: the SIGABRT handler is the
//   mod's, and a raised SIGABRT leaves its line in the log (abort() would end the process after it);
// - the log and the ini are written beside the DLL.
#include <windows.h>
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" bool __cdecl SteamAPI_IsSteamRunning();

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

// Which module an address lies in.
std::string owner(const void* p) {
  HMODULE m = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          static_cast<LPCSTR>(p), &m) || !m)
    return "?";
  char path[MAX_PATH] = "?";
  GetModuleFileNameA(m, path, MAX_PATH);
  const char* leaf = std::strrchr(path, '\\');
  return leaf ? leaf + 1 : path;
}

// This program's import slot for dll!name: what it holds now.
void* slot(const char* dll, const char* name) {
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  for (auto* imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp) {
    if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), dll)) continue;
    auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + imp->OriginalFirstThunk);
    auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++slots)
      if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal) &&
          !std::strcmp(reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name, name))
        return reinterpret_cast<void*>(slots->u1.Function);
  }
  return nullptr;
}

std::string read_file(const char* path) {
  std::string s;
  if (FILE* f = std::fopen(path, "rb")) {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
  }
  return s;
}

LONG WINAPI mine(EXCEPTION_POINTERS*) { return EXCEPTION_CONTINUE_SEARCH; }
}  // namespace

int main() {
  // The imports the mod watches (each used, so the linker keeps it).
  volatile bool never = false;
  if (never) {
    DirectInput8Create(nullptr, 0, IID_IDirectInput8A, nullptr, nullptr);
    ExitProcess(0);  // last: nothing after it is kept
  }
  check(owner(slot("kernel32.dll", "SetUnhandledExceptionFilter")) == "steam_api.dll",
        "SetUnhandledExceptionFilter's slot points into the mod (from its DllMain)");
  check(owner(slot("kernel32.dll", "ExitProcess")) == "steam_api.dll", "ExitProcess's slot points into the mod");
  check(owner(slot("dinput8.dll", "DirectInput8Create")) == "steam_api.dll",
        "DirectInput8Create's slot points into the mod");
  check(owner(slot("user32.dll", "GetAsyncKeyState")) == "steam_api.dll", "GetAsyncKeyState's slot points into the mod");
  check(owner(slot("d3d9.dll", "Direct3DCreate9")) == "steam_api.dll", "Direct3DCreate9's slot points into the mod");

  // The Steam API: the mod's export is `jmp [table]`, the table entry the original's function.
  {
    const auto* thunk = reinterpret_cast<const unsigned char*>(slot("steam_api.dll", "SteamAPI_IsSteamRunning"));
    HMODULE orig = GetModuleHandleA("steam_api_orig.dll");
    void* real = orig ? reinterpret_cast<void*>(GetProcAddress(orig, "SteamAPI_IsSteamRunning")) : nullptr;
    void* to = nullptr;
    if (thunk && thunk[0] == 0xFF && thunk[1] == 0x25) to = **reinterpret_cast<void** const*>(thunk + 2);
    check(owner(thunk) == "steam_api.dll" && real && to == real,
          "the mod's SteamAPI_IsSteamRunning jumps into the game's own (steam_api_orig.dll)");
    SteamAPI_IsSteamRunning();  // answered by the original (no Steam here)
    check(true, "SteamAPI_IsSteamRunning answers through the mod");
  }

  GetAsyncKeyState(VK_SHIFT);  // through the guard to user32's
  check(true, "GetAsyncKeyState answers through the guard");
  // This program sets its filter through its import (the mod's hook); kernel32's own, asked straight, still has
  // the mod's on top (put back as it was after).
  SetUnhandledExceptionFilter(&mine);
  using SetFilter = LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI*)(LPTOP_LEVEL_EXCEPTION_FILTER);
  const auto real = reinterpret_cast<SetFilter>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetUnhandledExceptionFilter"));
  const LPTOP_LEVEL_EXCEPTION_FILTER top = real(nullptr);
  real(top);
  check(top && top != &mine && owner(reinterpret_cast<void*>(top)) == "steam_api.dll",
        "SetUnhandledExceptionFilter: the mod's filter stays on top (this program's chained under it)");

  // Direct3DCreate9: through the mod to the other tool's d3d9.dll beside it, and on to the system's. Without a
  // display Wine's d3d9 may have no device to offer: the call reaching it is what counts (the mod's log says which
  // way it came back).
  IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (d3d) d3d->Release();
  char marker[32] = {};
  GetEnvironmentVariableA("RE6CC_FAKE_D3D9", marker, sizeof(marker));
  check(!std::strcmp(marker, "called"), "Direct3DCreate9 reached the other tool's d3d9.dll (ReShade's place) through the mod");

  // abort() in the C runtime: the mod's handler logs where it came from. raise() alone calls it and comes back.
  {
    const auto prev = std::signal(SIGABRT, SIG_DFL);
    std::signal(SIGABRT, prev);
    check(prev != SIG_DFL && prev != SIG_IGN && prev != SIG_ERR && owner(reinterpret_cast<void*>(prev)) == "steam_api.dll",
          "abort's handler (the C runtime's SIGABRT, shared with this program) is the mod's");
    std::raise(SIGABRT);
  }

  char dir[MAX_PATH] = {};
  GetModuleFileNameA(GetModuleHandleA("steam_api.dll"), dir, MAX_PATH);
  if (char* s = std::strrchr(dir, '\\')) s[1] = '\0';
  const std::string log = read_file((std::string(dir) + "RE6CabbyCodes.log").c_str());
  const std::string ini = read_file((std::string(dir) + "RE6CabbyCodes.ini").c_str());
  check(log.find("loaded (steam_api.dll proxy)") != std::string::npos &&
            log.find("forwarding 55 Steam API functions") != std::string::npos,
        "the log: loaded, forwarding the Steam API to the game's own DLL");
  check(log.find("now comes to the mod") != std::string::npos, "the log: the import slots re-pointed at DllMain");
  check(log.find("Direct3DCreate9 import hooked") != std::string::npos && log.find("it led to d3d9.dll") != std::string::npos,
        "the log: Direct3DCreate9 hooked, leading to the d3d9.dll beside it");
  check(log.find("not the Steam version") != std::string::npos, "the log: this program is not the game (no discovery)");
  check(log.find("the game ends: abort() in the mod's C runtime") != std::string::npos,
        "the log: an abort in the C runtime said, with where it came from");
  check(log.find(d3d ? "dx9: the game called Direct3DCreate9" : "dx9: the real Direct3DCreate9 (SDK") != std::string::npos,
        d3d ? "the log: the D3D object came back (its CreateDevice hooked)"
            : "the log: no D3D object without a display (logged)");
  check(ini.find("GodMode = 0") != std::string::npos && ini.find("InfiniteSkillPoints = 0") != std::string::npos &&
            ini.find("MemoryLog = 0") != std::string::npos && ini.find("\nFrameLog = 0") != std::string::npos,
        "the ini written with every switch, and the memory log and the frame log off");
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  std::fflush(stdout);
  return g_fail ? 1 : 0;
}
