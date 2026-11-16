// A stand-in for another tool's d3d9.dll in the game's folder (ReShade, DXVK on Windows), for tests/test_boot.cpp:
// its Direct3DCreate9 marks that it was called (an environment variable the test reads) and passes the call on to
// the system's d3d9.dll, as a wrapper does. Build:
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -shared -static -Wl,--kill-at tests/fake_d3d9.cpp -o tests/build/fake_d3d9.dll
#include <windows.h>

#include <cstdio>

extern "C" __declspec(dllexport) void* WINAPI Direct3DCreate9(UINT sdk) {
  SetEnvironmentVariableA("RE6CC_FAKE_D3D9", "called");
  char path[MAX_PATH] = {};
  const UINT n = GetSystemDirectoryA(path, MAX_PATH);
  std::snprintf(path + n, sizeof(path) - n, "\\d3d9.dll");
  HMODULE real = LoadLibraryA(path);
  using Fn = void*(WINAPI*)(UINT);
  const auto fn = real ? reinterpret_cast<Fn>(GetProcAddress(real, "Direct3DCreate9")) : nullptr;
  return fn ? fn(sdk) : nullptr;
}

extern "C" __declspec(dllexport) int WINAPI D3DPERF_GetStatus() { return 0; }
extern "C" __declspec(dllexport) void WINAPI D3DPERF_SetOptions(DWORD) {}
