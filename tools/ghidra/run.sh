#!/bin/bash
# tools/ghidra/run.sh <xref|dec|funcs> [addresses...] - a headless query against
# the analysed exe (ghidra_proj/RE6, imported from BH6.exe - see CLAUDE.md
# "Tooling notes"; "x:ADDR"/"d:ADDR" mix the two in one run). The answer is
# printed and also left in <project dir>/report.txt.
#
# GHIDRA_PROJ_DIR picks another copy of the project (a copy per concurrent user:
# Ghidra locks a project while a run has it open); MAXMEM caps the JVM (8G).
cd "$(dirname "$0")/../.." || exit 1
PROJ_DIR="${GHIDRA_PROJ_DIR:-$PWD/ghidra_proj}"
OUT="$PROJ_DIR/report.txt"
MAXMEM="${MAXMEM:-8G}" /opt/ghidra/support/analyzeHeadless "$PROJ_DIR" RE6 -process BH6.exe -noanalysis \
  -scriptPath tools/ghidra -postScript Report.java "$OUT" "$@" > "$PROJ_DIR/report.log" 2>&1
cat "$OUT"
