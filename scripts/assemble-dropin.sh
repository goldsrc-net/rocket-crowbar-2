#!/usr/bin/env bash
# assemble-dropin.sh — lay out the reRC2 patch for a stock Rocket Crowbar 2 beta 1.1 install from
# binaries that are already built. Rocket Crowbar 2 beta 1.1 is the base; the patch replaces only the
# libraries, under the original's own file names, and its liblist.gam gets the line a Linux server
# needs. Used by scripts/package.sh (local builds) and by CI's package job (its artifacts).
#
# Usage: scripts/assemble-dropin.sh [--debug] [--amd64 <server.so> <client.so>]
#            [--arm64 <server.so> <client.so>] [--win64 <server.dll> <client.dll>] <rev> <i386-server.so> <i386-client.so> <win32-server.dll> <win32-client.dll> <outdir>
# Output: <outdir>/reRC2-<rev>/{rc2/..., README.txt, MANIFEST.tsv} and <outdir>/reRC2-<rev>.zip
#
#   Release (the default): give it Release builds. The Linux libraries are stripped to their dynamic
#   exports; the script refuses the result if any export changed. The Windows dlls already export
#   only their entry points.
#   --debug: give it Debug builds. Nothing is stripped, each dll's PDB (beside the dll it was built
#   with) is included under the name the dll records, and the output is reRC2-<rev>-debug. Not for
#   release.
#
#   rc2/dlls/rc2.so rc2_i386.so rc2_i686.so         the server, i386 (the names the engines
#                                                   take from liblist.gam; README.txt says which)
#   rc2/dlls/rc2.dll                                the server, Win32 (liblist's gamedll)
#   rc2/cl_dlls/client.so, client.dll               the client; every engine loads it by this name
#   rc2/dlls/rc2_amd64.so rc2_arm64.so rc2_amd64.dll  64-bit servers (--amd64/--arm64/--win64)
#   rc2/cl_dlls/client_amd64.so client_arm64.so       64-bit clients, for Xash3D, which loads
#     client_amd64.dll                                  client_<arch> on a 64-bit system
#   rc2/liblist.gam                                 the original's, plus a gamedll_linux line
set -euo pipefail

usage() { sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

# What this mod's install looks like.
PKG_NAME=reRC2
TITLE='Rocket Crowbar 2 beta 1.1'
GAMEDIR=rc2
HAS_CLIENT=1
SERVER_SO=(rc2.so rc2_i386.so rc2_i686.so)   # the first is stripped, the rest are copies of it
SERVER_DLL=rc2.dll
SERVER_AMD64=rc2_amd64.so; SERVER_ARM64=rc2_arm64.so; SERVER_WIN64=rc2_amd64.dll
LIBLIST='game "Rocket Crowbar 2"
version "1"
size "1000"
svonly "0"
cldll "1"
startmap "c0a0"
trainmap "t0a0"
mpentity "info_player_deathmatch"
gamedll "dlls\rc2.dll"
gamedll_linux "dlls/rc2_i386.so"
'
NAMES_NOTE='RC2 shipped for Windows only, so its liblist.gam names no Linux server. The patch'\''s liblist.gam is
the original'\''s with one line added, gamedll_linux "dlls/rc2_i386.so". The server is installed as
rc2.so, rc2_i386.so and rc2_i686.so because the engines pick different names from that line: Steam'\''s
current Linux engine loads rc2.so (it cuts the name at the first "_"), older engines load
rc2_i386.so, and Metamod-R prefers rc2_i686.so. Windows loads rc2.dll. The client is always
cl_dlls/client.dll or client.so; no engine lets a mod rename it, and players need it too.'
RESTART_CLIENT=' (and the game, for players)'
X64_EXTRA=' The cl_dlls/client_amd64 and client_arm64 files are the 64-bit clients, for Xash3D,
which loads the client under that name on a 64-bit system; they link VGUI statically (openvgui),
so Xash hosts it inside the client.'
NEW_FILES='dlls/rc2.so, dlls/rc2_i386.so, dlls/rc2_i686.so, cl_dlls/client.so and the 64-bit files'   # what the original install never had

DEBUG=0
X64_SO=""; X64_CL=""; A64_SO=""; A64_CL=""; W64_DLL=""; W64_CL=""
while [ $# -gt 0 ]; do
  case "$1" in
    --debug) DEBUG=1; shift ;;
    --amd64) X64_SO="$2"; X64_CL="$3"; shift 3 ;;
    --arm64) A64_SO="$2"; A64_CL="$3"; shift 3 ;;
    --win64) W64_DLL="$2"; W64_CL="$3"; shift 3 ;;
    --*) usage ;;
    *) break ;;
  esac
done
if [ "$HAS_CLIENT" = 1 ]; then
  [ $# -eq 6 ] || usage
  REV="$1"; SO="$2"; CLSO="$3"; DLL="$4"; CLDLL="$5"; OUT="$6"
else
  [ $# -eq 4 ] || usage
  REV="$1"; SO="$2"; CLSO=""; DLL="$3"; CLDLL=""; OUT="$4"
fi

for f in "$SO" "$DLL" $CLSO $CLDLL $X64_SO $X64_CL $A64_SO $A64_CL $W64_DLL $W64_CL; do
  [ -f "$f" ] || { echo "ERROR: missing $f" >&2; exit 1; }
done

NAME="$PKG_NAME-$REV"; [ $DEBUG = 1 ] && NAME="$PKG_NAME-$REV-debug"
PKG="$OUT/$NAME"
if [ -e "$PKG" ] || [ -e "$PKG.zip" ]; then
  echo "ERROR: $PKG exists; remove it or package another commit" >&2
  exit 1
fi
G="$PKG/$GAMEDIR"
mkdir -p "$G/dlls"
[ "$HAS_CLIENT" = 1 ] && mkdir -p "$G/cl_dlls"

# GNU binutils for x86 (the i386 and amd64 builds), llvm's for anything GNU's cannot read (aarch64 on an
# x86 host). Checked per file so the x86 output stays what GNU strip makes.
tool() { # <gnu> <llvm> <file>: GNU's for an x86 ELF, else the first llvm-* found
  case "$(readelf -h "$3" | awk -F: '/Machine:/{print $2}')" in
    *80386*|*X86-64*) echo "$1"; return ;;
  esac
  for t in "$2" "$2"-21 "$2"-20 "$2"-19 "$2"-18 "$2"-17; do
    command -v "$t" >/dev/null 2>&1 && { echo "$t"; return; }
  done
  echo "ERROR: no $2 to read $3" >&2; exit 1
}
exports() { "$(tool nm llvm-nm "$1")" -D --defined-only "$1" | awk '{print $NF}' | sort; }
put_so() { # <in> <out>: Release strips (refusing a changed export list), Debug copies
  if [ $DEBUG = 1 ]; then cp "$1" "$2"; return; fi
  local st; st="$(tool strip llvm-strip "$1")"
  "$st" --strip-unneeded -o "$2" "$1"
  if ! diff <(exports "$1") <(exports "$2") >/dev/null; then
    echo "ERROR: stripping $1 changed its dynamic exports" >&2; exit 1
  fi
}

FILES=()
SRC=()   # "<path in the gamedir>\t<source>" for the manifest
add() { FILES+=("$1"); SRC+=("$1	$(basename "$2") @$REV"); }

put_so "$SO" "$G/dlls/${SERVER_SO[0]}"
for name in "${SERVER_SO[@]}"; do
  [ "$name" = "${SERVER_SO[0]}" ] || cp "$G/dlls/${SERVER_SO[0]}" "$G/dlls/$name"
  add "dlls/$name" "$SO"
done
cp "$DLL" "$G/dlls/$SERVER_DLL"; add "dlls/$SERVER_DLL" "$DLL"
if [ "$HAS_CLIENT" = 1 ]; then
  put_so "$CLSO" "$G/cl_dlls/client.so"; add cl_dlls/client.so "$CLSO"
  cp "$CLDLL" "$G/cl_dlls/client.dll"; add cl_dlls/client.dll "$CLDLL"
fi
if [ -n "$X64_SO" ]; then put_so "$X64_SO" "$G/dlls/$SERVER_AMD64"; add "dlls/$SERVER_AMD64" "$X64_SO"; fi
if [ -n "$A64_SO" ]; then put_so "$A64_SO" "$G/dlls/$SERVER_ARM64"; add "dlls/$SERVER_ARM64" "$A64_SO"; fi
if [ -n "$W64_DLL" ]; then cp "$W64_DLL" "$G/dlls/$SERVER_WIN64"; add "dlls/$SERVER_WIN64" "$W64_DLL"; fi
if [ -n "$X64_CL" ]; then put_so "$X64_CL" "$G/cl_dlls/client_amd64.so"; add cl_dlls/client_amd64.so "$X64_CL"; fi
if [ -n "$A64_CL" ]; then put_so "$A64_CL" "$G/cl_dlls/client_arm64.so"; add cl_dlls/client_arm64.so "$A64_CL"; fi
if [ -n "$W64_CL" ]; then cp "$W64_CL" "$G/cl_dlls/client_amd64.dll"; add cl_dlls/client_amd64.dll "$W64_CL"; fi
if [ -n "$LIBLIST" ]; then
  printf '%s' "$LIBLIST" > "$G/liblist.gam"
  FILES+=(liblist.gam); SRC+=("liblist.gam	scripts/assemble-dropin.sh @$REV")
fi

if [ $DEBUG = 1 ]; then
  # Each PDB beside the dll it belongs to, under the file name that dll records; the 64-bit dll's PDB
  # has the same name as the 32-bit one's, so it gets its own folder.
  for spec in "$DLL:dlls" ${CLDLL:+"$CLDLL:cl_dlls"} ${W64_DLL:+"$W64_DLL:dlls/amd64-pdb"} ${W64_CL:+"$W64_CL:cl_dlls/amd64-pdb"}; do
    dll="${spec%%:*}"; dir="${spec##*:}"; pdb="${dll%.dll}.pdb"
    [ -f "$pdb" ] || { echo "ERROR: --debug needs $pdb beside $dll" >&2; exit 1; }
    mkdir -p "$G/$dir"
    cp "$pdb" "$G/$dir/$(basename "$pdb")"
    SRC+=("$dir/$(basename "$pdb")	$(basename "$pdb") @$REV")
  done
fi

sha() { sha256sum "$1" | cut -d' ' -f1; }
{
  printf 'path\tsource\tsha256\n'
  for row in "${SRC[@]}"; do
    p="${row%%	*}"
    printf '%s/%s\t%s\n' "$GAMEDIR" "$row" "$(sha "$G/$p")"
  done
} > "$PKG/MANIFEST.tsv"

if [ $DEBUG = 1 ]; then
  KIND="Debug build (not stripped, with PDBs; for debugging, not for release)"
else
  KIND="Release build"
fi
# README.txt: each prose paragraph reflowed to 100 columns (fmt), the file list as it is.
para() { printf '%s\n' "$*" | fmt -w 100; }
{
  printf '%s %s: a patch for %s\n%s\n\n' "$PKG_NAME" "$REV" "$TITLE" "$KIND"
  para "$TITLE is the base. This replaces only these files:"
  echo
  for f in "${FILES[@]}"; do printf '  %s/%s\n' "$GAMEDIR" "$f"; done
  echo
  para "To install, copy the $GAMEDIR folder over your Half-Life/$GAMEDIR folder and restart the" \
    "server$RESTART_CLIENT. To be able to go back, first copy the listed files your install" \
    "already has somewhere else; putting them back and deleting the new ones undoes the patch." \
    "The new ones are $NEW_FILES."
  echo
  para "$NAMES_NOTE"
  if [ -n "$X64_SO$A64_SO$W64_DLL" ]; then
    echo
    para "The dlls/*_amd64 and *_arm64 files are the 64-bit servers. Xash3D on a 64-bit system builds" \
      "these names from liblist.gam by itself. A 64-bit ReHLDS (x86-64 or ARM) loads" \
      "liblist.gam's gamedll_linux exactly as written, so on one, set it to dlls/$SERVER_AMD64" \
      "or dlls/$SERVER_ARM64. Steam's Half-Life is 32-bit and never loads them.$X64_EXTRA"
  fi
  echo
  para "MANIFEST.tsv lists every file with its sha256."
} > "$PKG/README.txt"

( cd "$PKG" && zip -qr "../$NAME.zip" "$GAMEDIR" README.txt MANIFEST.tsv )
echo ">>> $PKG"
echo ">>> $OUT/$NAME.zip"
column -t -s $'\t' "$PKG/MANIFEST.tsv"
