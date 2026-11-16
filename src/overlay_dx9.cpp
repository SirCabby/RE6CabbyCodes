// D3D9 backend. BH6.exe imports Direct3DCreate9 statically: that import slot is
// re-pointed from DllMain (boot.cpp) at hk_create9, which calls on to whatever the
// slot held - the system's d3d9.dll, or another tool's in front of it (ReShade's,
// DXVK's on Windows) - and from the returned IDirect3D9 we hook CreateDevice
// (slot 16). On the device: Present (17), CreateAdditionalSwapChain (13),
// EndScene (42) and Reset (16); on its swap chains: Present (3). The panel is
// drawn into the back buffer of whichever present path the game uses (the log
// says which), on the thread that presents.
//
// Under Wine those hooks go into the D3D9 classes' vtables. On Windows they go
// into vtables of the game's device's and swap chains' own (adopt.cpp: the
// Steam overlay hooks what the classes' vtables point to), and the adopted
// device also answers GetSwapChain(0) itself, with the adopted implicit swap
// chain: D3D9's GetSwapChain is where the overlay hooks every swap chain it
// hands out, by whatever that swap chain's vtable holds then - the mod's
// Present, in the copy. A wrapper another tool hands the game (ReShade's device)
// is what gets the hooks then: its Present calls the real device's, so both draw.
//
// The game's first Direct3DCreate9 is also the mod's first moment with the
// game's class records built (its static initializers run after DllMain):
// boot::started() runs then, before the real call.

#include <windows.h>
#include <d3d9.h>

#include "overlay.h"

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"
#include "adopt.h"
#include "boot.h"
#include "config.h"
#include "crash.h"
#include "framelog.h"
#include "log.h"
#include "mem.h"
#include "memlog.h"

namespace re6cc::overlay::dx9 {
namespace {

using Create9Fn = IDirect3D9*(WINAPI*)(UINT);
using CreateDeviceFn = HRESULT(__stdcall*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*,
                                           IDirect3DDevice9**);
using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using SwapPresentFn = HRESULT(__stdcall*)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
using CreateSwapFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*, IDirect3DSwapChain9**);
using GetSwapFn = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, IDirect3DSwapChain9**);
using EndSceneFn = HRESULT(__stdcall*)(IDirect3DDevice9*);
using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

// The device calls that make something: hooked only for the FrameLog diagnostic, which counts and times them.
using CreateTextureFn = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                            IDirect3DTexture9**, HANDLE*);
using CreateCubeFn = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                         IDirect3DCubeTexture9**, HANDLE*);
using CreateVertexBufferFn = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL,
                                                 IDirect3DVertexBuffer9**, HANDLE*);
using CreateIndexBufferFn = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                IDirect3DIndexBuffer9**, HANDLE*);
using CreateDeclFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**);
using CreateVertexShaderFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**);
using CreatePixelShaderFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**);

Create9Fn g_real_create9 = nullptr;
CreateDeviceFn g_orig_create_device = nullptr;
PresentFn g_orig_present = nullptr;
SwapPresentFn g_orig_swap_present = nullptr;
CreateSwapFn g_orig_create_swap = nullptr;
GetSwapFn g_orig_get_swap = nullptr;  // adopted devices only
EndSceneFn g_orig_end_scene = nullptr;
ResetFn g_orig_reset = nullptr;
uintptr_t g_d3d_vt = 0;
uintptr_t g_device_vt = 0;  // the class hooks' (Wine)
uintptr_t g_swap_vt = 0;

constexpr int kCreateSwap = 13, kGetSwap = 14, kReset = 16, kPresent = 17, kEndScene = 42, kSwapPresent = 3;
constexpr int kCreateDevice = 16;  // IDirect3D9's
// IDirect3DDevice9's calls that make something (FrameLog): textures, cube textures, vertex and index buffers,
// vertex declarations, vertex and pixel shaders.
constexpr int kCreateTexture = 23, kCreateCube = 25, kCreateVertexBuffer = 26, kCreateIndexBuffer = 27,
              kCreateDecl = 86, kCreateVertexShader = 91, kCreatePixelShader = 106;
CreateTextureFn g_orig_create_texture = nullptr;
CreateCubeFn g_orig_create_cube = nullptr;
CreateVertexBufferFn g_orig_create_vb = nullptr;
CreateIndexBufferFn g_orig_create_ib = nullptr;
CreateDeclFn g_orig_create_decl = nullptr;
CreateVertexShaderFn g_orig_create_vs = nullptr;
CreatePixelShaderFn g_orig_create_ps = nullptr;
// IDirect3DDevice9's slots (IDirect3DDevice9Ex has 134), IDirect3DSwapChain9's
// (IDirect3DSwapChain9Ex has 13) and IDirect3D9's (IDirect3D9Ex has 22); an
// adopted object's copy has as many as can be read, up to the second number.
constexpr int kDeviceSlots = 119, kDeviceCopy = 192, kSwapSlots = 10, kSwapCopy = 32, kD3DSlots = 17, kD3DCopy = 32;
// The adopted device and its implicit swap chain (Windows): what GetSwapChain(0)
// answers with. The swap chain lives as long as the device (it survives Reset);
// no reference is held, which would keep the device alive.
IDirect3DDevice9* g_game_device = nullptr;
IDirect3DSwapChain9* g_game_chain = nullptr;
IDirect3DDevice9* g_device = nullptr;
IDirect3DSwapChain9* g_swap = nullptr;  // the one the game presents through (not ours to release)
HWND g_hwnd = nullptr;
bool g_renderer_ready = false;
// The device's own Present may hand over to its swap chain's through the
// vtable; the panel must not be drawn twice in one frame.
bool g_in_device_present = false;

// Per-second call counts (dispatch.cpp's tick reads them once a second).
Stats g_stats;
volatile LONG g_presents = 0, g_swap_presents = 0, g_end_scenes = 0;
volatile LONG g_first_present = 0, g_first_swap_present = 0, g_first_end_scene = 0;
int g_rate_logs = 0;

bool ensure_renderer(IDirect3DDevice9* device) {
  if (g_renderer_ready) return true;
  if (!g_hwnd) {
    D3DDEVICE_CREATION_PARAMETERS cp{};
    if (SUCCEEDED(device->GetCreationParameters(&cp))) g_hwnd = cp.hFocusWindow;
  }
  if (!ensure_context(g_hwnd)) return false;
  if (!ImGui_ImplDX9_Init(device)) {
    logf("ERROR: dx9: ImGui DX9 backend init failed");
    return false;
  }
  g_renderer_ready = true;
  return true;
}

// The panel, into `back` (the back buffer of the swap chain being presented),
// in a scene of its own. The state block puts the device state back; it does
// not cover render targets, so the old one is restored first - and the viewport
// with the state block after it.
void draw_into(IDirect3DDevice9* device, IDirect3DSurface9* back) {
  IDirect3DStateBlock9* state = nullptr;
  device->CreateStateBlock(D3DSBT_ALL, &state);
  IDirect3DSurface9* old_rt = nullptr;
  device->GetRenderTarget(0, &old_rt);
  device->SetRenderTarget(0, back);
  if (SUCCEEDED(device->BeginScene())) {
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw_frame();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    device->EndScene();
  }
  if (old_rt) {
    device->SetRenderTarget(0, old_rt);
    old_rt->Release();
  }
  if (state) {
    state->Apply();
    state->Release();
  }
}

// Every present path: the overlay drawn over the frame about to be shown.
void panel_frame(IDirect3DDevice9* device, IDirect3DSurface9* back);
bool before_present(IDirect3DDevice9* device, IDirect3DSurface9* back) {
  panel_frame(device, back);
  return true;
}

void panel_frame(IDirect3DDevice9* device, IDirect3DSurface9* back) {
  // The window procedure feeds this same context.
  ImGuiLock guard;
  if (ensure_renderer(device) && wants_draw()) draw_into(device, back);
}

void note_first(volatile LONG* flag, const char* what, HWND wnd, const RECT* dst) {
  if (InterlockedExchange(flag, 1)) return;
  if (dst)
    logf("dx9: first %s on thread %lu (hwnd=%p, dst %ld,%ld-%ld,%ld)", what, GetCurrentThreadId(), static_cast<void*>(wnd),
         dst->left, dst->top, dst->right, dst->bottom);
  else
    logf("dx9: first %s on thread %lu (hwnd=%p)", what, GetCurrentThreadId(), static_cast<void*>(wnd));
}

// A present that comes back in while one is on its way on (adopt::calling_on:
// another tool's hook calling the object's vtable again) is the same frame:
// the panel is drawn the first time through.
HRESULT __stdcall hk_present(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND wnd,
                             const RGNDATA* dirty) {
  InterlockedIncrement(&g_presents);
  g_stats.present_thread = GetCurrentThreadId();
  note_first(&g_first_present, "IDirect3DDevice9::Present", wnd, dst);
  const bool timed = framelog::on();  // a diagnostic (FrameLog): the mod's part, then the device's Present
  const long long t0 = timed ? framelog::now_us() : 0;
  memlog::on_present(device);  // a diagnostic (MemoryLog): the texture memory left, every 5 s
  IDirect3DSurface9* back = nullptr;
  bool show = true;
  if (!adopt::calling_on() && SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) && back) {
    show = before_present(device, back);
    back->Release();
  }
  if (!show) return D3D_OK;
  const auto orig = reinterpret_cast<PresentFn>(adopt::original(device, kPresent, reinterpret_cast<void*>(g_orig_present)));
  g_in_device_present = true;
  const long long t1 = timed ? framelog::now_us() : 0;
  HRESULT hr;
  {
    adopt::CallingOn on;
    hr = orig(device, src, dst, wnd, dirty);
  }
  g_in_device_present = false;
  if (timed) framelog::present(t1 - t0, framelog::now_us() - t1);
  return hr;
}

HRESULT __stdcall hk_swap_present(IDirect3DSwapChain9* sc, const RECT* src, const RECT* dst, HWND wnd,
                                  const RGNDATA* dirty, DWORD flags) {
  InterlockedIncrement(&g_swap_presents);
  g_stats.present_thread = GetCurrentThreadId();
  note_first(&g_first_swap_present, "IDirect3DSwapChain9::Present", wnd, dst);
  // A diagnostic (FrameLog): a swap chain's own Present, unless it is the device's handing over (timed there).
  const bool timed = framelog::on() && !g_in_device_present && !adopt::calling_on();
  const long long t0 = timed ? framelog::now_us() : 0;
  if (!g_in_device_present && !adopt::calling_on()) {
    g_swap = sc;
    bool show = true;
    IDirect3DDevice9* device = nullptr;
    if (SUCCEEDED(sc->GetDevice(&device)) && device) {
      IDirect3DSurface9* back = nullptr;
      if (SUCCEEDED(sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &back)) && back) {
        show = before_present(device, back);
        back->Release();
      }
      device->Release();
    }
    if (!show) return D3D_OK;
  }
  const auto orig =
      reinterpret_cast<SwapPresentFn>(adopt::original(sc, kSwapPresent, reinterpret_cast<void*>(g_orig_swap_present)));
  const long long t1 = timed ? framelog::now_us() : 0;
  HRESULT hr;
  {
    adopt::CallingOn on;
    hr = orig(sc, src, dst, wnd, dirty, flags);
  }
  if (timed) framelog::present(t1 - t0, framelog::now_us() - t1);
  return hr;
}

HRESULT __stdcall hk_end_scene(IDirect3DDevice9* device) {
  InterlockedIncrement(&g_end_scenes);
  note_first(&g_first_end_scene, "IDirect3DDevice9::EndScene", nullptr, nullptr);
  if (framelog::on()) framelog::end_scene(framelog::now_us());  // a diagnostic (FrameLog): the draw thread's gaps
  const auto orig = reinterpret_cast<EndSceneFn>(adopt::original(device, kEndScene, reinterpret_cast<void*>(g_orig_end_scene)));
  adopt::CallingOn on;
  return orig(device);
}

// FrameLog's: each call that makes something, counted and timed, then the device's own.
template <typename Fn, typename... Args>
HRESULT count_made(framelog::Kind kind, IDirect3DDevice9* device, int slot, Fn fallback, Args... args) {
  const auto orig = reinterpret_cast<Fn>(adopt::original(device, slot, reinterpret_cast<void*>(fallback)));
  const long long t = framelog::now_us();
  HRESULT hr;
  {
    adopt::CallingOn on;
    hr = orig(device, args...);
  }
  framelog::made(kind, framelog::now_us() - t);
  return hr;
}

HRESULT __stdcall hk_create_texture(IDirect3DDevice9* d, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt,
                                    D3DPOOL pool, IDirect3DTexture9** out, HANDLE* shared) {
  return count_made(framelog::kTexture, d, kCreateTexture, g_orig_create_texture, w, h, levels, usage, fmt, pool, out, shared);
}
HRESULT __stdcall hk_create_cube(IDirect3DDevice9* d, UINT edge, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                 IDirect3DCubeTexture9** out, HANDLE* shared) {
  return count_made(framelog::kTexture, d, kCreateCube, g_orig_create_cube, edge, levels, usage, fmt, pool, out, shared);
}
HRESULT __stdcall hk_create_vb(IDirect3DDevice9* d, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool,
                               IDirect3DVertexBuffer9** out, HANDLE* shared) {
  return count_made(framelog::kBuffer, d, kCreateVertexBuffer, g_orig_create_vb, length, usage, fvf, pool, out, shared);
}
HRESULT __stdcall hk_create_ib(IDirect3DDevice9* d, UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                               IDirect3DIndexBuffer9** out, HANDLE* shared) {
  return count_made(framelog::kBuffer, d, kCreateIndexBuffer, g_orig_create_ib, length, usage, fmt, pool, out, shared);
}
HRESULT __stdcall hk_create_decl(IDirect3DDevice9* d, const D3DVERTEXELEMENT9* elements, IDirect3DVertexDeclaration9** out) {
  return count_made(framelog::kDeclaration, d, kCreateDecl, g_orig_create_decl, elements, out);
}
HRESULT __stdcall hk_create_vs(IDirect3DDevice9* d, const DWORD* code, IDirect3DVertexShader9** out) {
  return count_made(framelog::kShader, d, kCreateVertexShader, g_orig_create_vs, code, out);
}
HRESULT __stdcall hk_create_ps(IDirect3DDevice9* d, const DWORD* code, IDirect3DPixelShader9** out) {
  return count_made(framelog::kShader, d, kCreatePixelShader, g_orig_create_ps, code, out);
}

// The creation hooks (FrameLog), in the order of their slots.
const adopt::Hook kMadeHooks[] = {{kCreateTexture, reinterpret_cast<void*>(&hk_create_texture)},
                                  {kCreateCube, reinterpret_cast<void*>(&hk_create_cube)},
                                  {kCreateVertexBuffer, reinterpret_cast<void*>(&hk_create_vb)},
                                  {kCreateIndexBuffer, reinterpret_cast<void*>(&hk_create_ib)},
                                  {kCreateDecl, reinterpret_cast<void*>(&hk_create_decl)},
                                  {kCreateVertexShader, reinterpret_cast<void*>(&hk_create_vs)},
                                  {kCreatePixelShader, reinterpret_cast<void*>(&hk_create_ps)}};
constexpr int kMadeHookCount = sizeof(kMadeHooks) / sizeof(kMadeHooks[0]);
void** const kMadeOrig[kMadeHookCount] = {
    reinterpret_cast<void**>(&g_orig_create_texture), reinterpret_cast<void**>(&g_orig_create_cube),
    reinterpret_cast<void**>(&g_orig_create_vb),      reinterpret_cast<void**>(&g_orig_create_ib),
    reinterpret_cast<void**>(&g_orig_create_decl),    reinterpret_cast<void**>(&g_orig_create_vs),
    reinterpret_cast<void**>(&g_orig_create_ps)};

// Wine: the creation hooks into the device class's vtable (FrameLog only).
void hook_made(uintptr_t vt) {
  if (g_orig_create_texture) return;
  for (int i = 0; i < kMadeHookCount; ++i) *kMadeOrig[i] = mem::hook_vtable(vt, kMadeHooks[i].slot, kMadeHooks[i].fn);
  logf("dx9: FrameLog - the device's calls that make textures, buffers, vertex declarations and shaders hooked "
       "(counted and timed)");
}

// Every swap chain of the device shares one class, so the first one's vtable
// covers the rest; a different vtable is logged rather than hooked. On Windows
// each gets a copy of its vtable instead (adopt.cpp).
void hook_swap_chain(IDirect3DSwapChain9* sc, const char* which) {
  const auto vt = mem::read<uintptr_t>(reinterpret_cast<uintptr_t>(sc));
  if (adopt::enabled()) {
    static const adopt::Hook hooks[] = {{kSwapPresent, reinterpret_cast<void*>(&hk_swap_present)}};
    // What the hook falls back on: the class's Present (its vtable is left alone).
    if (!g_orig_swap_present) g_orig_swap_present = mem::read<SwapPresentFn>(vt + kSwapPresent * sizeof(void*));
    const int copied = adopt::take(sc, kSwapSlots, kSwapCopy, hooks, 1);
    char where[MAX_PATH + 32];
    re6cc::describe_address(reinterpret_cast<void*>(vt), where, sizeof(where));
    if (copied)
      logf("dx9: %s swap chain %p adopted - a vtable of its own with Present (%d slots copied; the class's at %s)", which,
           static_cast<void*>(sc), copied, where);
    else
      logf("ERROR: dx9: %s swap chain %p could not be adopted (its vtable at %s) - no panel on it", which,
           static_cast<void*>(sc), where);
    return;
  }
  if (!g_orig_swap_present) {
    g_orig_swap_present =
        reinterpret_cast<SwapPresentFn>(mem::hook_vtable(vt, 3, reinterpret_cast<void*>(&hk_swap_present)));
    g_swap_vt = vt;
    logf("dx9: %s swap chain %p: Present hooked", which, static_cast<void*>(sc));
  } else if (vt != g_swap_vt) {
    logf("dx9: %s swap chain %p has another vtable %p - not hooked", which, static_cast<void*>(sc),
         reinterpret_cast<void*>(vt));
  }
}

HRESULT __stdcall hk_create_swap(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp, IDirect3DSwapChain9** out) {
  const auto orig =
      reinterpret_cast<CreateSwapFn>(adopt::original(device, kCreateSwap, reinterpret_cast<void*>(g_orig_create_swap)));
  const HRESULT hr = orig(device, pp, out);
  if (SUCCEEDED(hr) && out && *out) {
    logf("dx9: the game created an additional swap chain (%ux%u, hwnd=%p)", pp ? pp->BackBufferWidth : 0u,
         pp ? pp->BackBufferHeight : 0u, pp ? static_cast<void*>(pp->hDeviceWindow) : nullptr);
    hook_swap_chain(*out, "additional");
  }
  return hr;
}

// Adopted devices only (Windows): GetSwapChain(0) of the game's device is
// answered here with the adopted implicit swap chain - one more reference, all
// D3D9's does - so that the Steam overlay's GetSwapChain hook never reads the
// copy (see the top of this file).
HRESULT __stdcall hk_get_swap(IDirect3DDevice9* device, UINT i, IDirect3DSwapChain9** out) {
  if (out && i == 0 && device == g_game_device && g_game_chain) {
    g_game_chain->AddRef();
    *out = g_game_chain;
    return D3D_OK;
  }
  const auto orig = reinterpret_cast<GetSwapFn>(adopt::original(device, kGetSwap, reinterpret_cast<void*>(g_orig_get_swap)));
  return orig(device, i, out);
}

// A reset is rare: the switch to fullscreen, a change of display options. A
// display mode that does not hold makes the game reset over and over, ~20 a
// second, until it crashes - with or without the mod: its fullscreen under
// gamescope's --force-windows-fullscreen at a resolution other than gamescope's
// (-w/-h). The log says so once, then counts, instead of a line a reset.
DWORD g_reset_at = 0;  // the last reset's tick
int g_reset_run = 0;   // resets in a row, each within 5 s of the one before

void log_reset(const D3DPRESENT_PARAMETERS* pp, HRESULT hr) {
  const DWORD now = GetTickCount();
  g_reset_run = (g_reset_run && now - g_reset_at < 5000) ? g_reset_run + 1 : 1;
  g_reset_at = now;
  const char* mode = pp->Windowed ? "windowed" : "fullscreen";
  const auto code = static_cast<unsigned long>(hr);
  if (g_reset_run <= 5)
    logf("dx9: Reset to %ux%u %s (hr=0x%08lX)", pp->BackBufferWidth, pp->BackBufferHeight, mode, code);
  else if (g_reset_run == 6)
    logf("dx9: the game keeps resetting the display (6 resets in a row, the last to %ux%u %s): its display mode does "
         "not hold - under gamescope's --force-windows-fullscreen, give the game gamescope's resolution (-w/-h) or "
         "play it windowed; every 100th reset is logged from here",
         pp->BackBufferWidth, pp->BackBufferHeight, mode);
  else if (g_reset_run % 100 == 0)
    logf("dx9: %d resets in a row (the last to %ux%u %s, hr=0x%08lX)", g_reset_run, pp->BackBufferWidth,
         pp->BackBufferHeight, mode, code);
}

HRESULT __stdcall hk_reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp) {
  // The real Reset stays outside the lock: it can block on the GPU, and the
  // window procedure must not wait that out.
  if (g_renderer_ready) {
    ImGuiLock guard;
    ImGui_ImplDX9_InvalidateDeviceObjects();
  }
  const auto orig = reinterpret_cast<ResetFn>(adopt::original(device, kReset, reinterpret_cast<void*>(g_orig_reset)));
  HRESULT hr;
  {
    adopt::CallingOn on;
    hr = orig(device, pp);
  }
  if (pp) {
    g_stats.backbuffer_w = pp->BackBufferWidth;
    g_stats.backbuffer_h = pp->BackBufferHeight;
    g_stats.windowed = pp->Windowed != FALSE;
    log_reset(pp, hr);
  }
  if (g_renderer_ready) {
    ImGuiLock guard;
    ImGui_ImplDX9_CreateDeviceObjects();
  }
  return hr;
}

// Windows: the game's device gets a vtable of its own, then its implicit swap
// chain - taken through the class's GetSwapChain, the one call of it the
// overlay sees for this device - and that swap chain is what GetSwapChain(0)
// answers with from then on (hk_get_swap).
HRESULT adopt_device(IDirect3DDevice9* device, HWND hwnd, const D3DPRESENT_PARAMETERS* pp, DWORD flags, HRESULT hr) {
  if (!adopt::game_window(hwnd)) {
    adopt::leave_alone("dx9: a device", "CreateDevice", hwnd);
    return hr;
  }
  const auto vt = mem::read<uintptr_t>(reinterpret_cast<uintptr_t>(device));
  // What the hooks fall back on: the class's functions (its vtable is left alone).
  if (!g_orig_present) {
    g_orig_present = mem::read<PresentFn>(vt + kPresent * sizeof(void*));
    g_orig_create_swap = mem::read<CreateSwapFn>(vt + kCreateSwap * sizeof(void*));
    g_orig_get_swap = mem::read<GetSwapFn>(vt + kGetSwap * sizeof(void*));
    g_orig_end_scene = mem::read<EndSceneFn>(vt + kEndScene * sizeof(void*));
    g_orig_reset = mem::read<ResetFn>(vt + kReset * sizeof(void*));
    for (int i = 0; i < kMadeHookCount; ++i) *kMadeOrig[i] = mem::read<void*>(vt + kMadeHooks[i].slot * sizeof(void*));
  }
  // The creation calls (FrameLog) are in the copy only while that diagnostic is on.
  static adopt::Hook hooks[5 + kMadeHookCount] = {{kPresent, reinterpret_cast<void*>(&hk_present)},
                                                  {kCreateSwap, reinterpret_cast<void*>(&hk_create_swap)},
                                                  {kGetSwap, reinterpret_cast<void*>(&hk_get_swap)},
                                                  {kEndScene, reinterpret_cast<void*>(&hk_end_scene)},
                                                  {kReset, reinterpret_cast<void*>(&hk_reset)}};
  int nhooks = 5;
  if (framelog::on())
    for (int i = 0; i < kMadeHookCount; ++i) hooks[nhooks++] = kMadeHooks[i];
  const int copied = adopt::take(device, kDeviceSlots, kDeviceCopy, hooks, nhooks);
  char where[MAX_PATH + 32], win[128];
  re6cc::describe_address(reinterpret_cast<void*>(vt), where, sizeof(where));
  adopt::describe_window(hwnd, win, sizeof(win));
  if (!copied) {
    logf("ERROR: dx9: the game's device %p (%s) could not be adopted (its vtable at %s) - no panel",
         static_cast<void*>(device), win, where);
    return hr;
  }
  g_device = device;
  g_hwnd = hwnd;
  if (pp) {
    g_stats.backbuffer_w = pp->BackBufferWidth;
    g_stats.backbuffer_h = pp->BackBufferHeight;
    g_stats.windowed = pp->Windowed != FALSE;
  }
  RECT rc{};
  if (hwnd) GetClientRect(hwnd, &rc);
  logf("dx9: device %p created on thread %lu (%s, client %ldx%ld, backbuffer %ux%u %s, fmt %u, flags 0x%lX) adopted - "
       "a vtable of its own with Present/CreateAdditionalSwapChain/GetSwapChain/EndScene/Reset (%d slots copied; the "
       "class's at %s)",
       static_cast<void*>(device), GetCurrentThreadId(), win, rc.right - rc.left, rc.bottom - rc.top,
       pp ? pp->BackBufferWidth : 0u, pp ? pp->BackBufferHeight : 0u, pp && pp->Windowed ? "windowed" : "fullscreen",
       pp ? static_cast<unsigned>(pp->BackBufferFormat) : 0u, static_cast<unsigned long>(flags), copied, where);
  g_game_device = device;
  g_game_chain = nullptr;
  IDirect3DSwapChain9* sc = nullptr;
  const auto get = reinterpret_cast<GetSwapFn>(adopt::original(device, kGetSwap, reinterpret_cast<void*>(g_orig_get_swap)));
  if (SUCCEEDED(get(device, 0, &sc)) && sc) {
    hook_swap_chain(sc, "implicit");
    g_game_chain = sc;
    sc->Release();
  } else {
    logf("ERROR: dx9: the game's device has no implicit swap chain - no panel on its presents");
  }
  return hr;
}

HRESULT __stdcall hk_create_device(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                                   D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out) {
  const auto orig = reinterpret_cast<CreateDeviceFn>(
      adopt::original(self, kCreateDevice, reinterpret_cast<void*>(g_orig_create_device)));
  HRESULT hr;
  {
    adopt::CallingOn on;
    hr = orig(self, adapter, type, focus, flags, pp, out);
  }
  if (FAILED(hr) || !out || !*out) {
    static volatile LONG failures = 0;
    if (InterlockedIncrement(&failures) <= 8)
      logf("dx9: CreateDevice failed (hr=0x%08lX; adapter %u, type %d, hwnd=%p, flags 0x%lX, backbuffer %ux%u %s, fmt %u)",
           static_cast<unsigned long>(hr), adapter, static_cast<int>(type), static_cast<void*>(focus),
           static_cast<unsigned long>(flags), pp ? pp->BackBufferWidth : 0u, pp ? pp->BackBufferHeight : 0u,
           pp && pp->Windowed ? "windowed" : "fullscreen", pp ? static_cast<unsigned>(pp->BackBufferFormat) : 0u);
    return hr;
  }
  if (adopt::enabled()) return adopt_device(*out, focus ? focus : pp ? pp->hDeviceWindow : nullptr, pp, flags, hr);
  g_device = *out;
  if (focus) g_hwnd = focus;
  else if (pp && pp->hDeviceWindow) g_hwnd = pp->hDeviceWindow;
  if (pp) {
    g_stats.backbuffer_w = pp->BackBufferWidth;
    g_stats.backbuffer_h = pp->BackBufferHeight;
    g_stats.windowed = pp->Windowed != FALSE;
  }
  char cls[64] = "?";
  if (g_hwnd) GetClassNameA(g_hwnd, cls, sizeof(cls));
  RECT rc{};
  if (g_hwnd) GetClientRect(g_hwnd, &rc);
  const auto vt = mem::read<uintptr_t>(reinterpret_cast<uintptr_t>(g_device));
  if (!g_orig_present) {
    g_orig_present = reinterpret_cast<PresentFn>(mem::hook_vtable(vt, 17, reinterpret_cast<void*>(&hk_present)));
    g_orig_create_swap =
        reinterpret_cast<CreateSwapFn>(mem::hook_vtable(vt, 13, reinterpret_cast<void*>(&hk_create_swap)));
    g_orig_end_scene = reinterpret_cast<EndSceneFn>(mem::hook_vtable(vt, 42, reinterpret_cast<void*>(&hk_end_scene)));
    g_orig_reset = reinterpret_cast<ResetFn>(mem::hook_vtable(vt, 16, reinterpret_cast<void*>(&hk_reset)));
    g_device_vt = vt;
    if (framelog::on()) hook_made(vt);
    char where[MAX_PATH + 32];
    re6cc::describe_address(reinterpret_cast<void*>(g_orig_present), where, sizeof(where));
    logf("dx9: device %p created on thread %lu (hwnd=%p class '%s' client %ldx%ld, backbuffer %ux%u %s, fmt %u, "
         "flags 0x%lX); hooked Present/CreateAdditionalSwapChain/EndScene/Reset (Present was %s)",
         static_cast<void*>(g_device), GetCurrentThreadId(), static_cast<void*>(g_hwnd), cls, rc.right - rc.left,
         rc.bottom - rc.top, pp ? pp->BackBufferWidth : 0u, pp ? pp->BackBufferHeight : 0u,
         pp && pp->Windowed ? "windowed" : "fullscreen", pp ? static_cast<unsigned>(pp->BackBufferFormat) : 0u,
         static_cast<unsigned long>(flags), where);
  } else if (vt != g_device_vt) {
    logf("dx9: another device %p with a different vtable - not hooked", static_cast<void*>(g_device));
  }
  IDirect3DSwapChain9* sc = nullptr;
  if (SUCCEEDED(g_device->GetSwapChain(0, &sc)) && sc) {
    hook_swap_chain(sc, "implicit");
    sc->Release();
  }
  return hr;
}

// Windows: every IDirect3D9 the game makes gets a vtable of its own with
// CreateDevice in it. A compatibility shim (apphelp.dll - RE3-1999CabbyCodes
// met one for ResidentEvil3.exe, 2026-10-08) can write its own CreateDevice
// into d3d9.dll's class vtable on every Direct3DCreate9, over whatever was put
// there before; a private copy keeps the hook whatever happens to the class.
// The copy's CreateDevice calls on through the class's slot as it is at the
// time (a shim's, then d3d9.dll's).
IDirect3D9* adopt_d3d(IDirect3D9* d3d) {
  const auto vt = mem::read<uintptr_t>(reinterpret_cast<uintptr_t>(d3d));
  void* now = mem::read<void*>(vt + kCreateDevice * sizeof(void*));
  // What the hook falls back on: the class's CreateDevice (its vtable is left alone).
  if (!g_orig_create_device) g_orig_create_device = reinterpret_cast<CreateDeviceFn>(now);
  static const adopt::Hook hooks[] = {{kCreateDevice, reinterpret_cast<void*>(&hk_create_device)}};
  const int copied = adopt::take(d3d, kD3DSlots, kD3DCopy, hooks, 1);
  static int logged = 0;
  if (logged < 4) {
    ++logged;
    char where[MAX_PATH + 32], cd[MAX_PATH + 32];
    re6cc::describe_address(reinterpret_cast<void*>(vt), where, sizeof(where));
    re6cc::describe_address(now, cd, sizeof(cd));
    if (copied)
      logf("dx9: the game called Direct3DCreate9 (thread %lu): IDirect3D9 %p adopted - a vtable of its own with "
           "CreateDevice (%d slots copied; the class's at %s, its CreateDevice %s)",
           GetCurrentThreadId(), static_cast<void*>(d3d), copied, where, cd);
    else
      logf("ERROR: dx9: the game's IDirect3D9 %p could not be adopted (its vtable at %s) - no device will be seen",
           static_cast<void*>(d3d), where);
  }
  return d3d;
}

uintptr_t g_create9_slot = 0;  // BH6.exe's import slot for Direct3DCreate9, re-pointed at hk_create9

IDirect3D9* WINAPI hk_create9(UINT sdk) {
  static volatile LONG first = 0;
  if (!InterlockedExchange(&first, 1)) boot::started();
  IDirect3D9* d3d = g_real_create9(sdk);
  if (!d3d) {
    static volatile LONG logged = 0;
    if (InterlockedIncrement(&logged) <= 4)
      logf("dx9: the real Direct3DCreate9 (SDK %u) returned null - the game has no Direct3D 9 (no panel)", sdk);
    return d3d;
  }
  if (adopt::enabled()) return adopt_d3d(d3d);
  if (!g_orig_create_device) {
    const auto vt = mem::read<uintptr_t>(reinterpret_cast<uintptr_t>(d3d));
    g_orig_create_device =
        reinterpret_cast<CreateDeviceFn>(mem::hook_vtable(vt, 16, reinterpret_cast<void*>(&hk_create_device)));
    g_d3d_vt = vt;
    logf("dx9: the game called Direct3DCreate9 (thread %lu); hooked IDirect3D9::CreateDevice", GetCurrentThreadId());
  }
  return d3d;
}

}  // namespace

bool install_import(HMODULE exe) {
  const uintptr_t slot = mem::iat_slot(exe, "d3d9.dll", "Direct3DCreate9");
  void* prev = slot ? mem::read<void*>(slot) : nullptr;
  // Bound already (the loader binds a program's imports before any DLL's DllMain):
  // a slot still holding its name's place would be bound over the hook.
  if (!prev || !mem::in_module(prev) || !mem::write<uintptr_t>(slot, reinterpret_cast<uintptr_t>(&hk_create9))) {
    logf("ERROR: dx9: BH6.exe's Direct3DCreate9 import could not be hooked (%s) - no panel",
         !slot ? "not in its import table" : !prev || !mem::in_module(prev) ? "not bound yet" : "not writable");
    return false;
  }
  g_real_create9 = reinterpret_cast<Create9Fn>(prev);
  g_create9_slot = slot;
  // Where it leads: the system's d3d9.dll, or a tool's in front of it.
  char where[MAX_PATH + 32];
  re6cc::describe_address(prev, where, sizeof(where));
  logf("dx9: BH6.exe's Direct3DCreate9 import hooked (exe+0x%X; it led to %s)",
       static_cast<unsigned>(slot - reinterpret_cast<uintptr_t>(exe)), where);
  return true;
}

Stats stats() { return g_stats; }

// Main thread, once a second: the counts of the last second, logged for the
// first ten seconds with a device (and whenever they change, with Trace = 1).
void log_rates() {
  const auto p = static_cast<unsigned>(InterlockedExchange(&g_presents, 0));
  const auto sp = static_cast<unsigned>(InterlockedExchange(&g_swap_presents, 0));
  const auto es = static_cast<unsigned>(InterlockedExchange(&g_end_scenes, 0));
  const bool changed = p != g_stats.present_per_s || sp != g_stats.swap_present_per_s || es != g_stats.end_scene_per_s;
  g_stats.present_per_s = p;
  g_stats.swap_present_per_s = sp;
  g_stats.end_scene_per_s = es;
  if (!g_device) return;
  if (g_rate_logs < 10 || (config::get().trace && changed)) {
    ++g_rate_logs;
    logf("dx9: last second: %u device Present, %u swap-chain Present, %u EndScene", p, sp, es);
  }
}

void uninstall() {
  if (g_device_vt && mem::readable(reinterpret_cast<void*>(g_device_vt), 64 * sizeof(void*))) {
    if (g_orig_present) mem::write<void*>(g_device_vt + 17 * sizeof(void*), reinterpret_cast<void*>(g_orig_present));
    if (g_orig_create_swap)
      mem::write<void*>(g_device_vt + 13 * sizeof(void*), reinterpret_cast<void*>(g_orig_create_swap));
    if (g_orig_end_scene)
      mem::write<void*>(g_device_vt + 42 * sizeof(void*), reinterpret_cast<void*>(g_orig_end_scene));
    if (g_orig_reset) mem::write<void*>(g_device_vt + 16 * sizeof(void*), reinterpret_cast<void*>(g_orig_reset));
    for (int i = 0; i < kMadeHookCount; ++i)  // FrameLog's, each while it still holds the mod's function
      if (*kMadeOrig[i] && mem::read<void*>(g_device_vt + kMadeHooks[i].slot * sizeof(void*)) == kMadeHooks[i].fn)
        mem::write<void*>(g_device_vt + kMadeHooks[i].slot * sizeof(void*), *kMadeOrig[i]);
  }
  if (g_swap_vt && g_orig_swap_present && mem::readable(reinterpret_cast<void*>(g_swap_vt), 4 * sizeof(void*)))
    mem::write<void*>(g_swap_vt + 3 * sizeof(void*), reinterpret_cast<void*>(g_orig_swap_present));
  if (g_d3d_vt && mem::readable(reinterpret_cast<void*>(g_d3d_vt), 17 * sizeof(void*)) && g_orig_create_device)
    mem::write<void*>(g_d3d_vt + 16 * sizeof(void*), reinterpret_cast<void*>(g_orig_create_device));
  if (g_create9_slot && g_real_create9 &&
      mem::read<uintptr_t>(g_create9_slot) == reinterpret_cast<uintptr_t>(&hk_create9))
    mem::write<uintptr_t>(g_create9_slot, reinterpret_cast<uintptr_t>(g_real_create9));
  g_orig_present = nullptr;
  g_orig_swap_present = nullptr;
  g_orig_create_swap = nullptr;
  g_orig_end_scene = nullptr;
  g_orig_reset = nullptr;
  g_orig_create_device = nullptr;
  for (int i = 0; i < kMadeHookCount; ++i) *kMadeOrig[i] = nullptr;
  g_swap = nullptr;
  if (g_renderer_ready) {
    ImGui_ImplDX9_Shutdown();
    g_renderer_ready = false;
  }
}

}  // namespace re6cc::overlay::dx9

