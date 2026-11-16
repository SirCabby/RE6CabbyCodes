#pragma once

namespace re6cc {

// Install a last-resort exception filter that records where a fatal fault
// happened, and in which module. Injected mods are the usual suspect for
// crashes at shutdown, so it needs to be possible to tell from the log whether
// the fault is in this DLL or somewhere else entirely. An abort() in the mod's
// C runtime (an assertion, an uncaught C++ exception) is logged too: it ends the
// process through the runtime's own ExitProcess, which the hook below never sees.
void install_crash_logger();

// What the game's own SetUnhandledExceptionFilter import points at (boot.cpp
// re-points BH6.exe's import slot from DllMain, before the game's C runtime
// starts): the game then sets the filter we chain to, and ours stays on top.
void* crash_filter_hook();

// What the game's ExitProcess import points at, for the import `name` that holds
// `real` (nullptr for any other name; FatalAppExitA too, which BH6.exe does not
// import): every ExitProcess is logged - with the game's code it came through,
// when its code is not a quit's 0 - before it goes on to `real`.
void* crash_exit_hook(const char* name, void* real);

// Main thread, about once a second: put our filter back on top if something
// replaced it without going through the game's import - the log names the module
// that had (once, however often it puts itself back), and ours chains to it only if
// it is code. Nothing when SetUnhandledExceptionFilter has been made to do nothing
// (another crash handler keeping the slot): said once.
void reassert_crash_filter();

// "module+0xRVA" for an address, for log lines.
void describe_address(void* addr, char* out, unsigned n);

}  // namespace re6cc
