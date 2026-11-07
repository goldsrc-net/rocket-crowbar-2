#!/usr/bin/env bash
# run.sh original|ours [map] - run one of the two test servers in the foreground (Ctrl-C stops it).
#
#   original  Windows HLDS under Wine (HLDS_WIN, default ~/Containers/hlds/hlds-win), port 27031, reference/rc2.dll
#   ours      ~/Containers/hlds/hlds-linux32-rc2 (RIG2), port 27032, build-i386/dlls/hl.so
#
# Join: steam -applaunch 70 -game rc2 +connect 127.0.0.1:<port>
# -nobreakpad (Linux): the engine's crash handler otherwise uploads minidumps to Valve.
set -euo pipefail
MAP="${2:-rc_sandcrab}"

# A server just stopped can hold its port for a few seconds; started too soon, the engine fails to
# bind it and segfaults on the way out (exit 139). Wait for it.
wait_port() {
	for _ in $(seq 30); do
		ss -uln 2>/dev/null | grep -q ":$1 " || return 0
		sleep 0.5
	done
	echo "port $1 still in use" >&2
	exit 1
}
case "${1:-}" in
	original)
		HLDS_WIN="${HLDS_WIN:-$HOME/Containers/hlds/hlds-win}"
		wait_port 27031
		cd "$HLDS_WIN"
		export WINEPREFIX="$HLDS_WIN/wineprefix-rc2" WINEDEBUG=-all
		# Wine's helpers can trip a libdbus assertion that aborts the process (mod-ts test-win-server.sh)
		export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
		exec wine hlds.exe -console -game rc2 +developer 1 +sv_cheats 1 +maxplayers 8 +sv_lan 1 \
			+deathmatch 1 -port 27031 +map "$MAP" ;;
	ours)
		wait_port 27032
		cd "${RIG2:-$HOME/Containers/hlds/hlds-linux32-rc2}"
		export LD_LIBRARY_PATH=".:${LD_LIBRARY_PATH:-}"
		# sv_lan 0 so players carry their Steam IDs (admin levels, tools/rig/setup.sh's adminprim.cfg);
		# -nomaster keeps it off Valve's master server list all the same
		exec ./hlds_linux -nobreakpad -nomaster -game rc2 +developer 1 +sv_cheats 1 +maxplayers 8 +sv_lan 0 \
			-port 27032 +map "$MAP" ;;
	*) sed -n '2,8p' "$0"; exit 2 ;;
esac
