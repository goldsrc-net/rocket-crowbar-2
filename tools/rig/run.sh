#!/usr/bin/env bash
# run.sh original|ours [map] - run one of the two test servers in the foreground (Ctrl-C stops it).
#
#   original  Windows HLDS under Wine (HLDS_WIN), port 27031, reference/rc2.dll
#   ours      ~/Containers/hlds/hlds-linux32-rc2,  port 27032, build-i386/dlls/hl.so
#
# Join: steam -applaunch 70 -game rc2 +connect 127.0.0.1:<port>
# -nobreakpad (Linux): the engine's crash handler otherwise uploads minidumps to Valve.
set -euo pipefail
MAP="${2:-rc_sandcrab}"
case "${1:-}" in
	original)
		HLDS_WIN="${HLDS_WIN:-/media/Data3/Projects/goldsrc.net/hlds-win}"
		cd "$HLDS_WIN"
		export WINEPREFIX="$HLDS_WIN/wineprefix-rc2" WINEDEBUG=-all
		# Wine's helpers can trip a libdbus assertion that aborts the process (mod-ts test-win-server.sh)
		export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
		exec wine hlds.exe -console -game rc2 +developer 1 +sv_cheats 1 +maxplayers 8 +sv_lan 1 \
			+deathmatch 1 -port 27031 +map "$MAP" ;;
	ours)
		cd "${RIG2:-$HOME/Containers/hlds/hlds-linux32-rc2}"
		export LD_LIBRARY_PATH=".:${LD_LIBRARY_PATH:-}"
		exec ./hlds_linux -nobreakpad -game rc2 +developer 1 +sv_cheats 1 +maxplayers 8 +sv_lan 1 \
			-port 27032 +map "$MAP" ;;
	*) sed -n '2,8p' "$0"; exit 2 ;;
esac
