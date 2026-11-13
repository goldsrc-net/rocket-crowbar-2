#!/usr/bin/env bash
# boot-package.sh win|linux <package gamedir> <content dir> <map> — boot a scripts/package.sh (or CI)
# package's server on a real engine and wait for it to answer A2S_INFO, the way a server list asks.
# Answers "does the packaged library load and run a map under its shipped name".
#
#   win    the Windows HLDS under Wine (HLDS_WIN, default /media/Data3/Projects/goldsrc.net/hlds-win,
#          the one mod-ts's scripts/test-win-server.sh fetches with steamcmd), loading the Win32 dll
#   linux  the 32-bit Linux rig (RIG, default ~/Containers/hlds/hlds-linux32-test), loading the i386 .so
#
#   <package gamedir>  e.g. dist/reRC2-<rev>/rc2
#   <content dir>      the original's own rc2/ folder; linked, not copied, except dlls/
#   LIBLIST=<file>     the liblist.gam to run with, when the package has none (the original's)
#   SECS               the upper limit, default 90
#
# Makes <engine dir>/rc2-boot (refusing a folder it did not make) and removes it after; the console
# goes to boot-<engine>.log beside the package folder.
# Exit 0 when the server answered.
set -euo pipefail
ENGINE="${1:?usage: boot-package.sh win|linux <package gamedir> <content dir> <map>}"
PKGG="$(realpath "${2:?}")"; CONTENT="$(realpath "${3:?}")"; MAP="${4:?}"
PORT=27045; SECS="${SECS:-90}"
case "$ENGINE" in
  win)   H="${HLDS_WIN:-/media/Data3/Projects/goldsrc.net/hlds-win}" ;;
  linux) H="${RIG:-$HOME/Containers/hlds/hlds-linux32-test}" ;;
  *) echo "engine: win or linux" >&2; exit 2 ;;
esac
NAME=rc2-boot
LOG="$(dirname "$PKGG")/boot-$ENGINE.log"
G="$H/$NAME"
if [ -e "$G" ] && [ ! -f "$G/.boot-package" ]; then echo "$G exists and was not made by this script" >&2; exit 2; fi
rm -rf "${G:?}"
mkdir -p "$G/dlls"; touch "$G/.boot-package"
for e in "$CONTENT"/*; do
  b="$(basename "$e")"
  # custom.hpk too: the engine rewrites it (and deletes a bad one), which a link would pass on to
  # the original content.
  case "$b" in dlls|cl_dlls|logs|SAVE|liblist.gam|custom.hpk) continue ;; esac
  ln -s "$e" "$G/$b"
done
cp -r "$PKGG"/dlls/. "$G/dlls/"
if [ -n "${LIBLIST:-}" ]; then cp "$LIBLIST" "$G/liblist.gam"
elif [ -f "$PKGG/liblist.gam" ]; then cp "$PKGG/liblist.gam" "$G/liblist.gam"
else cp "$CONTENT/liblist.gam" "$G/liblist.gam"; fi
grep -E '^gamedll' "$G/liblist.gam"

cd "$H"
if [ "$ENGINE" = win ]; then
  export WINEPREFIX="${WINEPREFIX:-$H/wineprefix}" WINEDEBUG="${WINEDEBUG:--all}"
  # Wine's helpers can trip a libdbus assertion that aborts the process (mod-ts test-win-server.sh)
  export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
  CMD=(wine hlds.exe -console)
else
  export LD_LIBRARY_PATH=".:${LD_LIBRARY_PATH:-}"
  CMD=(./hlds_linux -nobreakpad)
fi
timeout "$SECS" "${CMD[@]}" -game "$NAME" +maxplayers 8 +sv_lan 1 -port $PORT +map "$MAP" > "$LOG" 2>&1 &
PID=$!
ok=1
for _ in $(seq 1 $((SECS / 2))); do
  if python3 - "$PORT" <<'EOF'
import socket, sys
port = int(sys.argv[1]); q = b'\xff\xff\xff\xffTSource Engine Query\x00'
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(1.5)
try:
    s.sendto(q, ('127.0.0.1', port)); d, _ = s.recvfrom(4096)
    if d[4:5] == b'A':  # a challenge: send it back
        s.sendto(q + d[5:9], ('127.0.0.1', port)); d, _ = s.recvfrom(4096)
except OSError:
    sys.exit(1)
print('A2S_INFO (name, map, gamedir, game):', [p.decode('latin1') for p in d[6:].split(b'\x00')[:4]])
EOF
  then ok=0; break; fi
  kill -0 $PID 2>/dev/null || break
  sleep 2
done
fuser -k $PORT/udp >/dev/null 2>&1 || true
wait $PID 2>/dev/null || true
[ $ok = 0 ] || { echo "no answer; console: $LOG" >&2; tail -20 "$LOG" >&2; }
rm -rf "${G:?}"
exit $ok
