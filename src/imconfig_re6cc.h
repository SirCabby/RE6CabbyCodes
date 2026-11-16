#pragma once

// The mod's ImGui config (IMGUI_USER_CONFIG, set in the Makefile for the mod and ImGui alike). An ImGui check that
// fails is written to the log, and the panel goes on: ImGui's own assert is the C runtime's, which ends the game
// with no line in the log about why (a colour popped that was never pushed, at a medal's count typed, 2026-11-06).
// The tests build ImGui with its own asserts, so a failed check still stops them.
namespace re6cc {
void imgui_check_failed(const char* what, const char* file, int line);  // overlay.cpp
}  // namespace re6cc

#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : ::re6cc::imgui_check_failed(#_EXPR, __FILE__, __LINE__))
