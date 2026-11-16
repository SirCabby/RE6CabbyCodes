// Offline test of the crash filter's chain (src/crash.cpp, built in), 2026-11-10 - found reading a crash report of
// RE4OGCabbyCodes', whose filter, chaining and reassert this one shares. The filter is called as
// UnhandledExceptionFilter calls the top-level one, with a made-up access violation. Build (the mod's own flags; no
// object of the mod linked: image::text, memlog::on_crash and logf are stood in for below):
//
//   i686-w64-mingw32-g++ -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type -DWIN32_LEAN_AND_MEAN -DNOMINMAX
//       -Isrc -static tests/test_crash.cpp -o tests/build/test_crash.exe
//   wine tests/build/test_crash.exe [-v]
//
// - installed, the game's filter set through its import (the hook): ours stays on top;
// - a crash: one dump - its kind, what it touched, every register (ebx, esi, edi too), the top of the stack raw
//   (six rows), the code addresses up it - the memory log's crash line, and it is passed on to the game's filter,
//   named;
// - the same exception given to ours again: a line, not a second dump (nor a second memory log), still passed on;
// - a filter set over ours that chains to the one it replaced (ours), which the reassert then chains to: a crash is
//   stopped as it comes back to ours with the same exception (before, ours went round until the stack ran out); a
//   filter that keeps putting itself back is logged once;
// - a "previous filter" that is no code: never chained to;
// - two threads: one inside the chain, the other crashing meanwhile - each its own dump, each stopped on its own
//   thread;
// - a new exception raised inside the filter ours passed one on to (a packer's handler may): passed on as well,
//   with its answer back; one raised every time: stopped three deep;
// - SetUnhandledExceptionFilter made to do nothing (`ret 4` over its first bytes, another crash handler keeping the
//   slot): said once, nothing taken for a filter, no line each second; neither the game's call through the hook
//   nor install claimed to put ours on top; install still takes abort (SIGABRT).
#include "../src/crash.cpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::vector<std::string> g_lines;
SRWLOCK g_lines_lock = SRWLOCK_INIT;  // two threads log at once
bool g_verbose = false;
int g_memlog_calls = 0;
}  // namespace

namespace re6cc {
void logf(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  AcquireSRWLockExclusive(&g_lines_lock);
  g_lines.emplace_back(buf);
  if (g_verbose) std::printf("      log: %s\n", buf);
  ReleaseSRWLockExclusive(&g_lines_lock);
}
namespace image {
mem::Range text() { return {}; }
}  // namespace image
namespace memlog {
void on_crash() { ++g_memlog_calls; }
}  // namespace memlog
}  // namespace re6cc

namespace {
using namespace re6cc;
using Filter = LPTOP_LEVEL_EXCEPTION_FILTER;

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

size_t mark() {
  AcquireSRWLockShared(&g_lines_lock);
  const size_t n = g_lines.size();
  ReleaseSRWLockShared(&g_lines_lock);
  return n;
}

size_t count(const char* needle, size_t from) {
  size_t n = 0;
  AcquireSRWLockShared(&g_lines_lock);
  for (size_t i = from; i < g_lines.size(); ++i) n += g_lines[i].find(needle) != std::string::npos;
  ReleaseSRWLockShared(&g_lines_lock);
  return n;
}

// "test_crash.exe+0xRVA" for an address, as the log writes it.
std::string where(void* a) {
  char out[MAX_PATH + 32];
  describe_address(a, out, sizeof(out));
  return out;
}

// The filter on top now, the slot left as it was.
Filter top() {
  const Filter t = SetUnhandledExceptionFilter(nullptr);
  SetUnhandledExceptionFilter(t);
  return t;
}

// A made-up access violation: reading address 0 at `at`, with a stack of its own.
struct Fake {
  EXCEPTION_RECORD rec;
  CONTEXT ctx;
  EXCEPTION_POINTERS ptrs;
  uint32_t stack[160];
};
void make(Fake* f, void* at, uint32_t edi) {
  std::memset(f, 0, sizeof(*f));
  f->rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  f->rec.ExceptionAddress = at;
  f->rec.NumberParameters = 2;
  for (int i = 0; i < 160; ++i) f->stack[i] = 0x01000000u + i;
  f->stack[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&make));  // a return address in this program
  f->ctx.ContextFlags = CONTEXT_FULL;
  f->ctx.Eip = static_cast<DWORD>(reinterpret_cast<uintptr_t>(at));
  f->ctx.Esp = static_cast<DWORD>(reinterpret_cast<uintptr_t>(f->stack));
  f->ctx.Eax = 0x10;
  f->ctx.Ecx = 4;
  f->ctx.Esi = 0;
  f->ctx.Edi = edi;
  f->ptrs.ExceptionRecord = &f->rec;
  f->ptrs.ContextRecord = &f->ctx;
}

// The game's own filter, set through its import (the hook).
int g_game_calls = 0;
LONG WINAPI game_filter(EXCEPTION_POINTERS*) {
  ++g_game_calls;
  return EXCEPTION_CONTINUE_SEARCH;
}

// A filter set over ours with the real API, that passes every exception on to the one it replaced (as many crash
// loggers do).
Filter g_x_prev = nullptr;
int g_x_calls = 0;
LONG WINAPI x_filter(EXCEPTION_POINTERS* p) {
  ++g_x_calls;
  return g_x_prev ? g_x_prev(p) : EXCEPTION_CONTINUE_SEARCH;
}

// For the two threads: the first, inside it, waits for the second's crash.
HANDLE g_inside_x = nullptr, g_second_done = nullptr;
DWORD g_waiting_thread = 0;
LONG WINAPI x_waiting_filter(EXCEPTION_POINTERS* p) {
  ++g_x_calls;
  if (GetCurrentThreadId() == g_waiting_thread) {
    SetEvent(g_inside_x);
    WaitForSingleObject(g_second_done, 5000);
  }
  return g_x_prev ? g_x_prev(p) : EXCEPTION_CONTINUE_SEARCH;
}

// A filter that raises a new exception of its own while it handles one - a packer's handler may, on purpose - which
// reaches the top-level filter (ours), and handles that one itself (goes on); the first it lets go.
Filter g_top = nullptr;
Fake g_nested;
int g_y_calls = 0;
LONG g_y_nested_r = -9;
LONG WINAPI y_filter(EXCEPTION_POINTERS* p) {
  if (++g_y_calls == 1) {
    g_y_nested_r = g_top(&g_nested.ptrs);
    return EXCEPTION_CONTINUE_SEARCH;
  }
  return p == &g_nested.ptrs ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

// A filter that raises a new exception every time it runs (one that crashes in itself, again and again).
Fake g_runaway[8];
int g_z_calls = 0;
LONG WINAPI z_filter(EXCEPTION_POINTERS*) {
  const int k = g_z_calls++;
  return k < 8 ? g_top(&g_runaway[k].ptrs) : EXCEPTION_CONTINUE_SEARCH;
}

struct Fire {
  Filter f;
  Fake* fake;
  LONG r;
};
DWORD WINAPI fire_thread(void* arg) {
  auto* a = static_cast<Fire*>(arg);
  a->r = a->f(&a->fake->ptrs);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace re6cc;
  g_verbose = argc > 1 && std::string(argv[1]) == "-v";

  // Installed; the game's filter set through the hook.
  install_crash_logger();
  const Filter ours = top();
  check(ours == &on_exception, "ours on top after install");
  const auto hook = reinterpret_cast<SetFilterFn>(crash_filter_hook());
  hook(&game_filter);
  check(top() == ours, "... and after the game sets its filter through the hook");

  // A crash.
  Fake f1;
  make(&f1, reinterpret_cast<void*>(&top), 0x0C5D77C);
  size_t m = mark();
  LONG r = ours(&f1.ptrs);
  check(r == EXCEPTION_CONTINUE_SEARCH && count("CRASH: ACCESS_VIOLATION", m) == 1 &&
            count("reading address 00000000", m) == 1,
        "a crash: one dump, what it touched, the search goes on");
  check(count("esi=00000000 edi=00C5D77C", m) == 1, "... every register, esi and edi among them");
  check(count("esp+000: 01000000 01000001 01000002", m) == 1 && count("esp+0A0:", m) == 1,
        "... the top of the stack, raw (six rows)");
  check(count(("stack[003] " + where(reinterpret_cast<void*>(&make))).c_str(), m) == 1,
        "... and the code addresses up it, by module");
  check(g_memlog_calls == 1, "... the memory log's crash line");
  check(count(("passed on to the filter at " + where(reinterpret_cast<void*>(&game_filter))).c_str(), m) == 1 &&
            g_game_calls == 1,
        "... passed on to the game's filter, named; it ran once");

  m = mark();
  ours(&f1.ptrs);
  check(count("CRASH: the same ACCESS_VIOLATION", m) == 1 && count("eip=", m) == 0 && g_memlog_calls == 1 &&
            g_game_calls == 2,
        "the same exception again: a line, not a second dump nor memory log; still passed on");

  // A filter set over ours that chains to the one it replaced (ours): the reassert puts ours back over it and chains
  // to it - a crash must not go round between the two.
  g_x_prev = SetUnhandledExceptionFilter(&x_filter);
  check(g_x_prev == ours, "a filter set over ours saved ours as the one to chain to");
  m = mark();
  reassert_crash_filter();
  check(top() == ours &&
            count((", " + where(reinterpret_cast<void*>(&x_filter)) + ") - ours is back on top, chaining to it").c_str(),
                  m) == 1,
        "... the reassert puts ours back on top, chaining to it (named)");
  Fake f2;
  make(&f2, reinterpret_cast<void*>(&mark), 0x12345678);
  m = mark();
  r = ours(&f2.ptrs);
  check(g_x_calls == 1 && count("back in our filter with the same exception", m) == 1,
        "... a crash then: the other filter ran once, the way back into ours stopped and logged - no loop");
  check(count("CRASH: ACCESS_VIOLATION", m) == 1 && r == EXCEPTION_CONTINUE_SEARCH, "... one dump, the search goes on");
  m = mark();
  SetUnhandledExceptionFilter(&x_filter);
  reassert_crash_filter();
  check(count("replaced", m) == 0, "a filter that puts itself back again is not logged again");

  // A "previous filter" that is no code.
  SetUnhandledExceptionFilter(reinterpret_cast<Filter>(static_cast<uintptr_t>(0x3EA)));
  m = mark();
  reassert_crash_filter();
  check(top() == ours && count("which is no code", m) == 1, "a previous filter that is no code: ours back on top, said so");
  Fake f3;
  make(&f3, reinterpret_cast<void*>(&count), 1);
  const int x_before = g_x_calls;
  ours(&f3.ptrs);  // would jump to 0x3EA if it were chained to
  check(g_x_calls == x_before + 1, "... not chained to: still the last filter that is code");

  // Two threads: one inside the chain, the other crashing meanwhile.
  g_x_prev = SetUnhandledExceptionFilter(&x_waiting_filter);
  reassert_crash_filter();
  g_inside_x = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  g_second_done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  Fake fa, fb;
  make(&fa, reinterpret_cast<void*>(&fire_thread), 0xAAAA);
  make(&fb, reinterpret_cast<void*>(&x_waiting_filter), 0xBBBB);
  Fire a{ours, &fa, -9}, b{ours, &fb, -9};
  m = mark();
  HANDLE ta = CreateThread(nullptr, 0, fire_thread, &a, CREATE_SUSPENDED, &g_waiting_thread);
  ResumeThread(ta);
  check(WaitForSingleObject(g_inside_x, 5000) == WAIT_OBJECT_0, "two threads: the first is inside the chain");
  HANDLE tb = CreateThread(nullptr, 0, fire_thread, &b, 0, nullptr);
  WaitForSingleObject(tb, 5000);
  check(count("edi=0000BBBB", m) == 1, "... the second's crash meanwhile gets its own dump");
  SetEvent(g_second_done);
  WaitForSingleObject(ta, 5000);
  check(count("edi=0000AAAA", m) == 1 && count("back in our filter", m) == 2 && a.r == EXCEPTION_CONTINUE_SEARCH &&
            b.r == EXCEPTION_CONTINUE_SEARCH,
        "... and the first's; each stopped on its own thread, both searches go on");
  CloseHandle(ta);
  CloseHandle(tb);

  // A new exception raised inside the filter ours passed one on to: passed on too.
  g_top = ours;
  SetUnhandledExceptionFilter(&y_filter);
  reassert_crash_filter();
  Fake f4;
  make(&f4, reinterpret_cast<void*>(&y_filter), 0x4444);
  make(&g_nested, reinterpret_cast<void*>(&z_filter), 0x5555);
  m = mark();
  r = ours(&f4.ptrs);
  check(g_y_calls == 2 && g_y_nested_r == EXCEPTION_CONTINUE_EXECUTION,
        "a new exception raised in the filter ours passed one on to: passed on as well, its answer back (handled)");
  check(count("another exception, raised while", m) == 1 && count("edi=00004444", m) == 1 &&
            count("edi=00005555", m) == 1 && count("handled it and the program went on", m) == 1 &&
            r == EXCEPTION_CONTINUE_SEARCH,
        "... said so, each with its dump; the nested one went on, the first one's search goes on");

  // A filter that raises a new exception every time it runs: stopped three deep.
  SetUnhandledExceptionFilter(&z_filter);
  reassert_crash_filter();
  for (int k = 0; k < 8; ++k) make(&g_runaway[k], reinterpret_cast<char*>(&z_filter) + 1 + k, 0x7000 + k);
  Fake f5;
  make(&f5, reinterpret_cast<void*>(&y_filter), 0x6666);
  m = mark();
  r = ours(&f5.ptrs);
  check(g_z_calls == 3 && count("others on this thread - not passed on", m) == 1 && r == EXCEPTION_CONTINUE_SEARCH,
        "a filter that raises a new exception every time it runs: stopped three deep, said so, the search goes on");

  // SetUnhandledExceptionFilter made to do nothing, as another crash handler keeping the slot does (re4_tweaks' with a
  // CrashDumps folder): a `ret 4` over its first bytes.
  auto* api = reinterpret_cast<uint8_t*>(&SetUnhandledExceptionFilter);
  uint8_t saved[3];
  std::memcpy(saved, api, 3);
  DWORD old = 0;
  VirtualProtect(api, 3, PAGE_EXECUTE_READWRITE, &old);
  const uint8_t ret4[3] = {0xC2, 0x04, 0x00};
  std::memcpy(api, ret4, 3);
  FlushInstructionCache(GetCurrentProcess(), api, 3);
  m = mark();
  for (int k = 0; k < 5; ++k) reassert_crash_filter();  // five seconds of ticks
  check(count("made to do nothing", m) == 1 && count("replaced", m) == 0,
        "the API made to do nothing: said once; no garbage taken for a filter, no line a second");
  hook(&game_filter);
  check(count("it did not go in", m) == 1 && count("ours stays on top", m) == 0,
        "... the game's call is not claimed to leave ours on top");
  std::signal(SIGABRT, SIG_DFL);
  install_crash_logger();
  const auto abort_handler = std::signal(SIGABRT, SIG_DFL);
  check(count("installed without its top-level filter", m) == 1 && abort_handler == &on_abort,
        "... install stands aside, says so, and still takes abort");
  std::memcpy(api, saved, 3);
  VirtualProtect(api, 3, old, &old);
  FlushInstructionCache(GetCurrentProcess(), api, 3);
  check(top() == ours, "... the API back as it was: ours is still on top");

  std::printf(g_fail ? "%d FAILED\n" : "all passed (0 failed)\n", g_fail);
  return g_fail ? 1 : 0;
}
