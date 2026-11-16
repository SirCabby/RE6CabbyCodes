#include "memlog.h"

#include <windows.h>
#include <d3d9.h>
#include <malloc.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <new>

#include "config.h"
#include "log.h"

// --- the mod's own heap ----------------------------------------------------------------------
// Every operator new and delete of the mod's code (the C++ library is linked into the DLL, so
// its containers' allocations come here too) and ImGui's allocator: each block's size as the C
// runtime keeps it, added on and taken off. Plain zero-initialised words, so the count holds
// from the first static initialiser on. Nothing else changes: malloc and free underneath.
namespace {

volatile LONG g_heap_bytes = 0;
volatile LONG g_heap_blocks = 0;

void* counted_alloc(std::size_t n) {
  void* p = std::malloc(n ? n : 1);
  if (p) {
    InterlockedExchangeAdd(&g_heap_bytes, static_cast<LONG>(_msize(p)));
    InterlockedIncrement(&g_heap_blocks);
  }
  return p;
}

void counted_free(void* p) {
  if (!p) return;
  InterlockedExchangeAdd(&g_heap_bytes, -static_cast<LONG>(_msize(p)));
  InterlockedDecrement(&g_heap_blocks);
  std::free(p);
}

}  // namespace

void* operator new(std::size_t n) {
  if (void* p = counted_alloc(n)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
  if (void* p = counted_alloc(n)) return p;
  throw std::bad_alloc();
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return counted_alloc(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return counted_alloc(n); }
void operator delete(void* p) noexcept { counted_free(p); }
void operator delete[](void* p) noexcept { counted_free(p); }
void operator delete(void* p, std::size_t) noexcept { counted_free(p); }
void operator delete[](void* p, std::size_t) noexcept { counted_free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { counted_free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { counted_free(p); }

namespace re6cc::memlog {
namespace {

constexpr uint64_t kMB = 1024 * 1024;
constexpr uint64_t k4G = 0x100000000ull;
// A 32-bit process's space on a 64-bit kernel: from the lowest address mmap hands out
// (vm.mmap_min_addr) to the top the kernel gives it (its compat TASK_SIZE).
constexpr uint64_t kLow = 0x10000, kTop32 = 0xFFFFE000ull;

// What a mapping is, from its path (and, with none, its access).
enum Kind { kAnon, kGpu, kNvidia, kSteam, kWine, kWinDlls, kGame, kFiles, kShared, kHeap, kStack, kReserved, kKinds };

struct Maps {
  bool ok = false;
  bool wide = false;           // a 64-bit process (Proton's new WoW64): the program's 4 GB is the low part
  uint64_t kind[kKinds] = {};  // under 4 GB
  uint64_t mapped = 0;         // under 4 GB, all of it
  uint64_t unmapped = 0;       // under the top, not mapped at all
  uint64_t gap = 0;            // the largest such gap
  uint64_t high = 0;           // above 4 GB (a 64-bit process's Linux side)
  int lines = 0;
  uint64_t next = kLow;  // while reading: where the mappings so far end
};

// The Windows view (VirtualQuery): committed by type, reserved, free.
struct Win {
  uint64_t priv = 0, mapped = 0, image = 0;
  uint64_t reserved = 0, free = 0, largest = 0;
};

const char kStarted[] = "a game started";
const char kEnded[] = "a game ended";
constexpr unsigned kMarks[] = {512, 256, 128, 64};  // MB of room: the mappings written as it falls under each

bool g_on = false;
DWORD g_interval_ms = 0;
HANDLE g_poke = nullptr;
void* volatile g_why = nullptr;   // kStarted or kEnded, for the line the poke asks for
char g_game[MAX_PATH] = {};       // "/<the game's folder>/", as a path in the mapping list has it
char g_list[64] = {};             // this process's mapping list: "\\?\unix\proc\<pid>\maps" (found once)
char g_maps_path[MAX_PATH] = {};  // RE6CabbyCodes.maps.txt
volatile LONG g_tex_mb = -1;      // what GetAvailableTextureMem said last, in MB
DWORD g_tex_at = 0;

Kind classify(const char* path, const char* perms) {
  if (!*path || !std::strncmp(path, "[anon:", 6)) return std::strncmp(perms, "---", 3) ? kAnon : kReserved;
  if (!std::strncmp(path, "/dev/nvidia", 11) || !std::strncmp(path, "/dev/dri/", 9)) return kGpu;
  if (std::strstr(path, "nvidia")) return kNvidia;
  if (std::strstr(path, "/ubuntu12_32/") || std::strstr(path, "/ubuntu12_64/") || std::strstr(path, "steamoverlay") ||
      std::strstr(path, "gameoverlay"))
    return kSteam;
  if (g_game[0] && std::strstr(path, g_game)) return kGame;
  if (std::strstr(path, "/drive_c/windows/")) return kWinDlls;
  if (std::strstr(path, "/lib/wine/") || std::strstr(path, "/bin/wine")) return kWine;
  if (!std::strncmp(path, "/memfd:", 7) || !std::strncmp(path, "/SYSV", 5) || !std::strncmp(path, "/dev/shm/", 9) ||
      std::strstr(path, " (deleted)"))
    return kShared;
  if (!std::strcmp(path, "[heap]")) return kHeap;
  if (!std::strncmp(path, "[stack", 6)) return kStack;
  return kFiles;
}

// One line of the mapping list: "start-end perms offset dev inode   path" (the path may be
// missing, or hold spaces).
void add(Maps* m, const char* line) {
  char* e = nullptr;
  const uint64_t start = std::strtoull(line, &e, 16);
  if (!e || *e != '-') return;
  const uint64_t end = std::strtoull(e + 1, &e, 16);
  if (end <= start) return;
  const char* p = e;
  char perms[5] = "----";
  for (int field = 0; field < 4; ++field) {  // perms, offset, dev, inode
    while (*p == ' ' || *p == '\t') ++p;
    const char* from = p;
    while (*p && *p != ' ' && *p != '\t') ++p;
    if (field == 0) std::memcpy(perms, from, std::min<size_t>(4, static_cast<size_t>(p - from)));
  }
  while (*p == ' ' || *p == '\t') ++p;
  ++m->lines;
  if (end > k4G) {
    m->wide = true;
    m->high += end - std::max(start, k4G);
    if (start >= k4G) return;
  }
  const uint64_t to = std::min(end, k4G);
  if (start > m->next) {
    m->unmapped += start - m->next;
    m->gap = std::max(m->gap, start - m->next);
  }
  m->next = std::max(m->next, to);
  m->mapped += to - start;
  m->kind[classify(p, perms)] += to - start;
}

// After the last line: the gap between it and the top.
void finish(Maps* m) {
  const uint64_t top = m->wide ? k4G : kTop32;
  if (top > m->next) {
    m->unmapped += top - m->next;
    m->gap = std::max(m->gap, top - m->next);
  }
  m->ok = m->lines > 0;
}

// A Linux file read in pieces, each whole line handed to `line`; with `copy`, its text written
// there too. `buf` is the caller's: the thread's and the crash's are not the same one.
template <class Line>
bool each_line(const char* path, char* buf, size_t cap, HANDLE copy, Line&& line) {
  HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  size_t have = 0;
  for (;;) {
    DWORD got = 0;
    const bool more = ReadFile(f, buf + have, static_cast<DWORD>(cap - 1 - have), &got, nullptr) && got > 0;
    if (more && copy) {
      DWORD wrote = 0;
      WriteFile(copy, buf + have, got, &wrote, nullptr);
    }
    have += got;
    buf[have] = '\0';
    char* p = buf;
    for (char* nl; (nl = std::strchr(p, '\n')) != nullptr; p = nl + 1) {
      *nl = '\0';
      line(static_cast<const char*>(p));
    }
    if (!more) {
      if (*p) line(static_cast<const char*>(p));
      break;
    }
    have = std::strlen(p);
    std::memmove(buf, p, have);
    if (have >= cap - 1) have = 0;  // a line as long as the buffer (no mapping's is): dropped
  }
  CloseHandle(f);
  return true;
}

const char* leaf_of(const char* path) {
  const char* l = path;
  for (const char* p = path; *p; ++p)
    if (*p == '\\' || *p == '/') l = p + 1;
  return l;
}

// A mapping line's path: what follows its five fields.
const char* path_of(const char* line) {
  const char* p = line;
  for (int field = 0; field < 5; ++field) {
    while (*p == ' ' || *p == '\t') ++p;
    while (*p && *p != ' ' && *p != '\t') ++p;
  }
  while (*p == ' ' || *p == '\t') ++p;
  return p;
}

// This process's mapping list. Not /proc/self: Wine's server opens the files a program asks
// for, so "self" is the server. The process whose command line starts with this program's file
// name and whose list has this program's image where this program has it: /proc/self first (a
// Wine that opens files itself), then each process in /proc.
void find_list(char* buf, size_t cap) {
  char exe[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  const char* leaf = leaf_of(exe);
  char at[24];
  std::snprintf(at, sizeof(at), "%08lx-", static_cast<unsigned long>(reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr))));
  const size_t at_len = std::strlen(at);
  auto ours = [&](const char* list) {
    bool found = false;
    each_line(list, buf, cap, nullptr, [&](const char* l) {
      if (!found && !std::strncmp(l, at, at_len)) found = !_stricmp(leaf_of(path_of(l)), leaf);
    });
    return found;
  };
  for (const char* root : {"\\\\?\\unix", "Z:"}) {
    char path[64];
    std::snprintf(path, sizeof(path), "%s\\proc\\self\\maps", root);
    if (ours(path)) {
      std::snprintf(g_list, sizeof(g_list), "%s", path);
      return;
    }
    std::snprintf(path, sizeof(path), "%s\\proc\\*", root);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    do {
      const char* pid = fd.cFileName;
      if (!*pid || std::strspn(pid, "0123456789") != std::strlen(pid)) continue;
      char cmd[512] = {};
      std::snprintf(path, sizeof(path), "%s\\proc\\%s\\cmdline", root, pid);
      HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
      if (f == INVALID_HANDLE_VALUE) continue;
      DWORD got = 0;
      ReadFile(f, cmd, sizeof(cmd) - 1, &got, nullptr);
      CloseHandle(f);
      cmd[got < sizeof(cmd) ? got : sizeof(cmd) - 1] = '\0';
      if (_stricmp(leaf_of(cmd), leaf)) continue;  // argv[0]: the program as Wine names it
      std::snprintf(path, sizeof(path), "%s\\proc\\%s\\maps", root, pid);
      if (ours(path)) {
        std::snprintf(g_list, sizeof(g_list), "%s", path);
        break;
      }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (g_list[0]) return;
  }
}

// The mapping list, summed; with `copy`, its text written there too.
Maps read_maps(char* buf, size_t cap, HANDLE copy) {
  Maps m;
  if (!g_list[0] || !each_line(g_list, buf, cap, copy, [&](const char* l) { add(&m, l); })) return m;
  finish(&m);
  return m;
}

Win read_windows() {
  Win w;
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  uintptr_t p = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
  const uintptr_t top = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
  MEMORY_BASIC_INFORMATION mbi;
  while (p < top && VirtualQuery(reinterpret_cast<void*>(p), &mbi, sizeof(mbi)) == sizeof(mbi)) {
    const uint64_t n = mbi.RegionSize;
    if (mbi.State == MEM_FREE) {
      w.free += n;
      w.largest = std::max(w.largest, n);
    } else if (mbi.State == MEM_RESERVE) {
      w.reserved += n;
    } else if (mbi.Type == MEM_IMAGE) {
      w.image += n;
    } else if (mbi.Type == MEM_MAPPED) {
      w.mapped += n;
    } else {
      w.priv += n;
    }
    const uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (next <= p) break;  // the top of the space, wrapped
    p = next;
  }
  return w;
}

// The room left: the Linux side's (a 32-bit process: what is not mapped at all) or the
// Windows side's, whichever is less.
uint64_t room(const Maps& m, const Win& w) { return m.ok && !m.wide ? std::min(m.unmapped, w.free) : w.free; }

unsigned mb(uint64_t v) { return static_cast<unsigned>(v / kMB); }

void log_line(const char* why, const Maps& m, const Win& w) {
  char maps[640] = "no mapping list (not under Wine)";
  if (m.ok) {
    char high[48] = "";
    if (m.wide) std::snprintf(high, sizeof(high), ", %u MB above 4 GB", mb(m.high));
    std::snprintf(maps, sizeof(maps),
                  "%s process: %u MB mapped under 4 GB (%u of it reserved for Windows), %u MB not mapped (largest "
                  "gap %u)%s - anon %u, gpu %u, nvidia libs %u, steam %u, wine %u, windows dlls %u, game %u, files %u, "
                  "shared %u, [heap] %u, [stack] %u",
                  m.wide ? "64-bit" : "32-bit", mb(m.mapped), mb(m.kind[kReserved]), mb(m.unmapped), mb(m.gap), high,
                  mb(m.kind[kAnon]), mb(m.kind[kGpu]), mb(m.kind[kNvidia]), mb(m.kind[kSteam]), mb(m.kind[kWine]),
                  mb(m.kind[kWinDlls]), mb(m.kind[kGame]), mb(m.kind[kFiles]), mb(m.kind[kShared]), mb(m.kind[kHeap]),
                  mb(m.kind[kStack]));
  }
  char tex[32] = "not asked yet";
  const LONG t = g_tex_mb;
  if (t >= 0) std::snprintf(tex, sizeof(tex), "%ld MB", static_cast<long>(t));
  logf("mem: %s%s%s | windows: committed %u MB (private %u, mapped %u, image %u), reserved %u, free %u (largest %u) "
       "| d3d: texture memory left %s | the mod's heap: %ld KB in %ld blocks",
       why ? why : "", why ? ": " : "", maps, mb(w.priv + w.mapped + w.image), mb(w.priv), mb(w.mapped), mb(w.image),
       mb(w.reserved), mb(w.free), mb(w.largest), tex, static_cast<long>(g_heap_bytes / 1024),
       static_cast<long>(g_heap_blocks));
}

// The whole mapping list, appended to RE6CabbyCodes.maps.txt under a heading.
void dump(const char* why, char* buf, size_t cap) {
  HANDLE out = CreateFileA(g_maps_path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
  if (out == INVALID_HANDLE_VALUE) return;
  SYSTEMTIME t;
  GetLocalTime(&t);
  char head[200];
  const int n = std::snprintf(head, sizeof(head), "\n=== %02u:%02u:%02u.%03u - %s ===\n", t.wHour, t.wMinute,
                              t.wSecond, t.wMilliseconds, why);
  DWORD wrote = 0;
  if (n > 0) WriteFile(out, head, static_cast<DWORD>(std::min<int>(n, sizeof(head) - 1)), &wrote, nullptr);
  read_maps(buf, cap, out);
  CloseHandle(out);
}

DWORD WINAPI thread(LPVOID) {
  static char buf[64 * 1024];
  size_t mark = 0;  // kMarks passed so far
  bool first_game = true;
  const char* why = "at start";
  find_list(buf, sizeof(buf));
  if (g_list[0])
    logf("memlog: this process's mappings: %s", g_list);
  else
    logf("memlog: this process's mapping list not found (not under Wine?) - the Windows view only");
  for (;;) {
    const Maps m = read_maps(buf, sizeof(buf), nullptr);
    const Win w = read_windows();
    log_line(why, m, w);
    if (why == kStarted && first_game) {
      first_game = false;
      dump("the first game's start", buf, sizeof(buf));
    }
    const uint64_t r = room(m, w);
    size_t passed = mark;
    while (passed < std::size(kMarks) && r < kMarks[passed] * kMB) ++passed;
    if (passed != mark) {
      mark = passed;
      char what[96];
      std::snprintf(what, sizeof(what), "the room left (%u MB) is under %u MB", mb(r), kMarks[mark - 1]);
      logf("mem: %s - the mappings written to RE6CabbyCodes.maps.txt", what);
      dump(what, buf, sizeof(buf));
    }
    why = WaitForSingleObject(g_poke, g_interval_ms) == WAIT_OBJECT_0
              ? static_cast<const char*>(InterlockedExchangePointer(&g_why, nullptr))
              : nullptr;
  }
}

}  // namespace

void start() {
  const int seconds = config::get().memory_log;
  if (seconds <= 0 || g_on) return;
  // The game's folder as the paths in the mapping list have it: "/Resident Evil 6/".
  char dir[MAX_PATH];
  std::snprintf(dir, sizeof(dir), "%s", config::dir());
  size_t n = std::strlen(dir);
  while (n && (dir[n - 1] == '\\' || dir[n - 1] == '/')) dir[--n] = '\0';
  const char* leaf = std::strrchr(dir, '\\');
  leaf = leaf ? leaf + 1 : dir;
  if (*leaf && !std::strchr(leaf, ':')) std::snprintf(g_game, sizeof(g_game), "/%s/", leaf);
  // The run before's mappings are kept, as its log is: a crash is followed by a start.
  std::snprintf(g_maps_path, sizeof(g_maps_path), "%sRE6CabbyCodes.maps.txt", config::dir());
  char prev[MAX_PATH];
  std::snprintf(prev, sizeof(prev), "%sRE6CabbyCodes.maps.prev.txt", config::dir());
  MoveFileExA(g_maps_path, prev, MOVEFILE_REPLACE_EXISTING);
  g_interval_ms = static_cast<DWORD>(seconds) * 1000;
  g_poke = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  g_on = g_poke != nullptr;
  HANDLE t = g_on ? CreateThread(nullptr, 0, thread, nullptr, 0, nullptr) : nullptr;
  if (!t) {
    g_on = false;
    logf("memlog: could not start (error %lu)", GetLastError());
    return;
  }
  CloseHandle(t);
  logf("memlog: a memory line every %d s and at each load (MemoryLog); the mappings into %s at the first game, as "
       "the room falls under 512/256/128/64 MB and at a crash",
       seconds, g_maps_path);
}

void poke(bool game_started) {
  if (!g_on) return;
  InterlockedExchangePointer(&g_why, const_cast<char*>(game_started ? kStarted : kEnded));
  SetEvent(g_poke);
}

void on_present(IDirect3DDevice9* device) {
  if (!g_on || !device) return;
  const DWORD now = GetTickCount();
  if (g_tex_at && now - g_tex_at < 5000) return;
  g_tex_at = now | 1;
  InterlockedExchange(&g_tex_mb, static_cast<LONG>(device->GetAvailableTextureMem() / kMB));
}

void on_crash() {
  if (!g_on) return;
  static char buf[64 * 1024];  // not the thread's: it may be reading at this moment
  const Maps m = read_maps(buf, sizeof(buf), nullptr);
  log_line("at the crash", m, read_windows());
  dump("the crash", buf, sizeof(buf));
  logf("mem: the mappings at the crash written to %s", g_maps_path);
}

void* heap_alloc(size_t size, void*) { return counted_alloc(size); }
void heap_free(void* p, void*) { counted_free(p); }

}  // namespace re6cc::memlog
