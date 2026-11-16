# RE6CabbyCodes - a 32-bit proxy-DLL mod for Resident Evil 6 on Steam (app 221040, BH6.exe).
# Cross-compiled on Linux with mingw-w64 (no MSVC / no Windows needed).
#
# The mod ships as steam_api.dll, in front of the game's own (renamed
# steam_api_orig.dll at install): every export is a jump thunk into the
# original. Of the DLLs BH6.exe imports it is the one that ships with the game
# and is no common mod's - d3d9.dll (ReShade, DXVK), dinput8.dll, version.dll and
# winmm.dll (ASI loaders) and xinput1_3.dll (controller wrappers) stay free - and
# it is not a Wine builtin, so Proton loads ours with no launch options.

CXX      := i686-w64-mingw32-g++

# The project name (used for the distributable), the DLL name (fixed by the
# game, which imports steam_api.dll) and the game executable are deliberately
# separate things.
PROJECT  := RE6CabbyCodes
NAME     := steam_api
ORIG     := steam_api_orig
EXE      := BH6.exe

# VERSION holds the single source of truth (the user's). With no VERSION file
# the build reports 0.0.0.
VERSION  := $(shell cat VERSION 2>/dev/null || echo 0.0.0)
BUILD    := build
TARGET   := $(BUILD)/$(NAME).dll
DEF      := steam_api.def

# Per-machine config (game install path), kept out of git. Copy
# config.mk.example to config.mk and set GAME_DIR there. The path contains
# spaces, so it is only ever used quoted inside shell recipes below.
-include config.mk

ifneq (,$(filter version rev,$(MAKECMDGOALS)))
REV := $(strip $(filter-out version rev,$(MAKECMDGOALS)))
$(eval $(REV):;@:)
endif

SRCS := $(wildcard src/*.cpp)

# Dear ImGui (vendored, MIT) supplies the overlay panel: the core plus the
# Win32 and DX9 backends - BH6.exe draws through Direct3D 9.
IMGUI_DIR  := contrib/imgui
IMGUI_SRCS := $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp \
              $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_dx9.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_win32.cpp

OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS)) \
        $(patsubst $(IMGUI_DIR)/%.cpp,$(BUILD)/imgui/%.o,$(IMGUI_SRCS))

# -MMD -MP: emit .d header-dependency files so editing a header recompiles every
# .cpp that includes it.
# IMGUI_USER_CONFIG: an ImGui check that fails is logged, not the game's end (src/imconfig_re6cc.h).
CXXFLAGS := -m32 -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD \
            -I$(IMGUI_DIR) -iquote src -DIMGUI_USER_CONFIG='"imconfig_re6cc.h"' \
            -ffunction-sections -fdata-sections \
            -MMD -MP

# -static*: fold the C/C++ runtime in so the DLL has no external mingw runtime
# deps. Exports come from the .def, so no --exclude-all-symbols here.
LDFLAGS  := -m32 -shared \
            -static -static-libgcc -static-libstdc++ \
            -Wl,--gc-sections

# Only DLLs every Windows (and Wine) has are imported. d3d9 and dinput8 are the
# game's: the mod hooks the game's own import slots and only ever calls the
# objects the game made with them.
LDLIBS   := -luser32 -lshell32 -lole32 -lgdi32 -ldwmapi -lwinmm

.PHONY: all clean install uninstall dist package rev version
all: $(TARGET)

$(TARGET): $(OBJS) $(DEF) | $(BUILD)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(DEF) $(LDLIBS)
	@echo ">> Built $@"

MOD_OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

# Rebuild the mod's own objects when the version changes (the vendored ImGui
# objects do not use it, so they are left alone). No VERSION file: nothing to track.
$(MOD_OBJS): $(wildcard VERSION)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -DRE6CC_VERSION='"$(VERSION)"' -c $< -o $@

# -Wno-array-bounds: ImGui indexes its mouse-button arrays after checking the button; with its checks logged rather
# than ending the game (imconfig_re6cc.h), GCC sees a way past the check (the buttons are the backend's own 0-4).
$(BUILD)/imgui/%.o: $(IMGUI_DIR)/%.cpp | $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Wno-array-bounds -c $< -o $@

-include $(OBJS:.o=.d)

$(BUILD):
	mkdir -p $(BUILD) $(BUILD)/imgui/backends

# `make version` prints the version; `make version X.Y.Z` sets it (`rev` is an
# older spelling of the same thing). Make has no argument syntax, so the new
# version arrives as a second goal; the block near the top defines a do-nothing
# target for it so make does not fail trying to build "1.2.3".
version rev:
	@v='$(REV)'; \
	if [ -z "$$v" ] && [ "$@" = version ]; then echo "$(VERSION)"; exit 0; fi; \
	if ! echo "$$v" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
	  echo "usage: make $@ X.Y.Z        (for example: make $@ 1.2.0)"; \
	  echo "current version: $(VERSION)"; \
	  exit 1; \
	fi; \
	printf '%s\n' "$$v" > VERSION; \
	echo ">> version $(VERSION) -> $$v"; \
	echo ">> run 'make package' to build $(PROJECT)_v$$v.zip"

clean:
	rm -rf $(BUILD) $(DIST)

# --- Deploy -----------------------------------------------------------------
# The game's own steam_api.dll is moved aside to steam_api_orig.dll first (once:
# an original already kept stays as it is), and the mod passes the game's calls
# on to it. Never the other way round: if steam_api_orig.dll is missing and the
# live DLL is ours, there is nothing to preserve. Steam's "Verify integrity of
# game files" puts the game's own steam_api.dll back over the mod's (and leaves
# steam_api_orig.dll alone): run make install again then.
#
# Atomic install: write to a temp name in the SAME directory then rename(2) over
# the target, so a running game keeps its old intact mapping.
install: $(TARGET)
	@set -e; \
	if [ -z "$(GAME_DIR)" ]; then \
	  echo "ERROR: GAME_DIR is not set."; \
	  echo "  Copy config.mk.example to config.mk and set GAME_DIR,"; \
	  echo "  or run: make install GAME_DIR=\"/path/to/Resident Evil 6\""; \
	  exit 1; \
	fi; \
	if [ ! -f "$(GAME_DIR)/$(EXE)" ]; then \
	  echo "ERROR: no '$(EXE)' in '$(GAME_DIR)' - is that really the game's folder?"; exit 1; \
	fi; \
	if pgrep -x $(EXE) >/dev/null 2>&1 || pgrep -f '[/\\]$(EXE)( |$$)' >/dev/null 2>&1; then \
	  echo "ERROR: $(EXE) is running - close the game first."; exit 1; \
	fi; \
	orig="$(GAME_DIR)/$(ORIG).dll"; \
	live="$(GAME_DIR)/$(NAME).dll"; \
	if [ -f "$$orig" ]; then \
	  echo ">> the game's own $(NAME).dll already kept at $$orig"; \
	else \
	  stock=$$(ls "$(GAME_DIR)" | grep -ix '$(NAME)\.dll' | head -1); \
	  if [ -z "$$stock" ]; then echo "ERROR: no $(NAME).dll in '$(GAME_DIR)'."; exit 1; fi; \
	  if grep -aq "$(PROJECT)" "$(GAME_DIR)/$$stock" 2>/dev/null; then \
	    echo "ERROR: $(GAME_DIR)/$$stock is already the mod, but $$orig is missing."; \
	    echo "       Refusing to keep the mod as the game's original."; \
	    echo "       Restore the real $(NAME).dll (Steam: Verify integrity of game files),"; \
	    echo "       then re-run make install."; \
	    exit 1; \
	  fi; \
	  mv -f "$(GAME_DIR)/$$stock" "$$orig"; \
	  echo ">> kept the game's own $$stock as $(ORIG).dll"; \
	fi; \
	t="$$live.re6cc-new.$$$$"; cp -f $(TARGET) "$$t"; mv -f "$$t" "$$live"; \
	echo ">> installed (atomic) $$live"; \
	for stray in $$(ls "$(GAME_DIR)" | grep -ix '$(NAME)\.dll' | grep -vx '$(NAME).dll'); do \
	  if grep -aq "$(PROJECT)" "$(GAME_DIR)/$$stray" 2>/dev/null; then \
	    rm -f "$(GAME_DIR)/$$stray"; \
	    echo ">> removed stale copy $$stray - it shadowed this install"; \
	  else \
	    echo ">> WARNING: $$stray exists and is not ours - it may shadow the mod"; \
	  fi; \
	done; \
	echo ">> NOTE: Steam 'Verify integrity of game files' puts the game's own $(NAME).dll back;"; \
	echo ">>       just re-run 'make install' if that happens."

# Put the game's own DLL back and remove the mod entirely.
uninstall:
	@set -e; \
	if [ -z "$(GAME_DIR)" ]; then echo "ERROR: GAME_DIR is not set."; exit 1; fi; \
	orig="$(GAME_DIR)/$(ORIG).dll"; \
	if [ ! -f "$$orig" ]; then echo "Nothing to do: $$orig not present."; exit 0; fi; \
	for stray in $$(ls "$(GAME_DIR)" | grep -ix '$(NAME)\.dll'); do \
	  if grep -aq "$(PROJECT)" "$(GAME_DIR)/$$stray" 2>/dev/null; then \
	    rm -f "$(GAME_DIR)/$$stray"; \
	  fi; \
	done; \
	if ls "$(GAME_DIR)" | grep -ixq '$(NAME)\.dll'; then \
	  echo ">> the game's own $(NAME).dll is in place already (Steam put it back) - $(ORIG).dll left for you to delete"; \
	else \
	  mv -f "$$orig" "$(GAME_DIR)/$(NAME).dll"; \
	  echo ">> put the game's own $(NAME).dll back"; \
	fi; \
	echo ">> the mod is removed"

# --- Distributable ----------------------------------------------------------
# `make dist` assembles dist/RE6CabbyCodes/ - the mod DLL plus the docs and an
# INSTALL.txt. It deliberately does NOT contain steam_api_orig.dll: that is the
# game's own file, which the player renames themselves (or `make install` does).
DIST    ?= dist
PAYLOAD  = $(DIST)/$(PROJECT)

dist: $(TARGET)
	@rm -rf "$(PAYLOAD)" && mkdir -p "$(PAYLOAD)"
	@cp $(TARGET) "$(PAYLOAD)/$(NAME).dll"
	@cp README.md LICENSE "$(PAYLOAD)/"
	@printf '%s\n' \
	  "$(PROJECT) $(VERSION) - pause-menu cheat panel for Resident Evil 6" \
	  "" \
	  "  For the current Steam version of Resident Evil 6 (build 10112070)." \
	  "" \
	  "INSTALL" \
	  "  In the game's folder (the one containing '$(EXE)'), rename the game's own" \
	  "  $(NAME).dll to $(ORIG).dll, then copy this mod's $(NAME).dll in beside it." \
	  "  The mod passes the game's Steam calls on to $(ORIG).dll." \
	  "" \
	  "  That is all, on Windows and on Linux / Steam Deck (Proton): no launch" \
	  "  options. ReShade, DXVK and ASI loaders (d3d9.dll, dinput8.dll, ...) work" \
	  "  beside it." \
	  "" \
	  "  Steam's 'Verify integrity of game files' puts the game's own $(NAME).dll" \
	  "  back over the mod's: copy the mod's in again then ($(ORIG).dll stays)." \
	  "" \
	  "UNINSTALL" \
	  "  Delete the mod's $(NAME).dll and rename $(ORIG).dll back to $(NAME).dll." \
	  "" \
	  "USE" \
	  "  Pause the game while playing and a panel appears beside the pause screen." \
	  "  F7 hides/shows it. Settings and the log live in $(PROJECT).ini / .log" \
	  "  beside the DLL." \
	  > "$(PAYLOAD)/INSTALL.txt"
	@echo ">> assembled $(PAYLOAD)"

# <project>_v<version>.zip, e.g. RE6CabbyCodes_v1.0.0.zip
PKG ?= $(DIST)/$(PROJECT)_v$(VERSION).zip
package: dist
	@command -v zip >/dev/null || { echo "ERROR: zip required"; exit 1; }
	@rm -f "$(PKG)"
	@cd "$(PAYLOAD)" && zip -r -q "$(abspath $(PKG))" .
	@echo ">> packaged -> $(PKG) ($$(du -h "$(PKG)" | cut -f1))"
