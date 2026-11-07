#!/usr/bin/env bash
# re.sh - Ghidra front-end for RC2's original rc2.dll and client.dll.
#
# One analyzed project at recovered/ghidra/rc2 (gitignored) holds both libraries. setup imports
# them fresh, runs Ghidra's analysis (PE exports, MSVC demangling, the bundled Function ID
# signatures, which name the static C runtime) and then ApplyNames.java, which applies
# reference/names-<lib>.tsv from scripts/vc6/match.py.
#
#   scripts/ghidra/re.sh setup                     fresh import + analysis + names
#   scripts/ghidra/re.sh export <lib> [filter]     decompile to recovered/decomp/<lib>/<Class>.c
#   scripts/ghidra/re.sh at <lib> 0x<addr> ...     decompile by address (image base 0x10000000)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA_HOME:-$HOME/Tools/ghidra}/support/analyzeHeadless"
SCRIPTS="$ROOT/scripts/ghidra"
PROJDIR="$ROOT/recovered/ghidra"
PROJ=rc2
OUT="$ROOT/recovered/decomp"

cmd="${1:-}"; shift || true
case "$cmd" in
  setup)
    mkdir -p "$PROJDIR"
    rm -rf "${PROJDIR:?}/$PROJ.gpr" "${PROJDIR:?}/$PROJ.rep"
    for lib in rc2 client; do
      "$GHIDRA" "$PROJDIR" "$PROJ" -import "$ROOT/reference/$lib.dll" -scriptPath "$SCRIPTS" \
        -preScript DisableNoReturn.java -postScript ApplyNames.java "$ROOT/reference" 2>&1 \
        | grep -E "ApplyNames|ERROR" || true
    done
    ;;
  export)
    lib="${1:?usage: re.sh export <rc2|client> [filter]}"; shift
    mkdir -p "$OUT/$lib"
    "$GHIDRA" "$PROJDIR" "$PROJ" -process "$lib.dll" -noanalysis -readOnly \
      -scriptPath "$SCRIPTS" -postScript DecompExport.java "$OUT/$lib" "${1:-}" 2>&1 \
      | grep -E "^(INFO|ERROR|WARN).*(DecompExport|wrote|loaded|error)" || true
    ls "$OUT/$lib" | wc -l | sed "s/^/files in recovered\/decomp\/$lib: /"
    ;;
  at)
    lib="${1:?usage: re.sh at <rc2|client> 0x<addr> ...}"; shift
    "$GHIDRA" "$PROJDIR" "$PROJ" -process "$lib.dll" -noanalysis -readOnly \
      -scriptPath "$SCRIPTS" -postScript DecompAt.java "$@" 2>&1 \
      | sed -n '/DecompAt.java (HeadlessAnalyzer)/,/REPORT/p' | sed '1d;$d'
    ;;
  *)
    sed -n '2,13p' "$0"; exit 2 ;;
esac
