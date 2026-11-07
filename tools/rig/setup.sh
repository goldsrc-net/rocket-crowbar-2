#!/usr/bin/env bash
# setup.sh - set up the side-by-side test: the original RC2 beta 1.1 and our rebuild on two
# servers, and the client folder to join them with. Idempotent: re-running refreshes our
# libraries and leaves everything else alone. Undo: tools/rig/README.md.
#
#   tools/rig/setup.sh     needs RC2's content (RC2_CONTENT, default /media/Data3/Projects/mods/rc2),
#                          build-i386/dlls/hl.so and build-i386/cl_dll/client.so (scripts/build.sh i386 all),
#                          the Windows HLDS (HLDS_WIN) and the 32-bit Linux rig (RIG)
#
# The original is Windows-only, so it runs in the Windows HLDS under Wine, in a prefix of its
# own (mod-ts's test-win-server.sh kills the shared prefix's wineserver on every run).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CONTENT="${RC2_CONTENT:-/media/Data3/Projects/mods/rc2}"
HLDS_WIN="${HLDS_WIN:-/media/Data3/Projects/goldsrc.net/hlds-win}" # runs the original, port 27031
RIG="${RIG:-$HOME/Containers/hlds/hlds-linux32-test}"              # the 32-bit ReHLDS install
RIG2="${RIG2:-$HOME/Containers/hlds/hlds-linux32-rc2}"             # runs ours, port 27032
CLIENT="${CLIENT:-$HOME/.local/share/Steam/steamapps/common/Half-Life}"
OURS="$ROOT/build-i386/dlls/hl.so"
OURS_CL="$ROOT/build-i386/cl_dll/client.so"
ORIG="$ROOT/reference/rc2.dll"

for f in "$CONTENT/liblist.gam" "$OURS" "$OURS_CL" "$HLDS_WIN/hlds.exe" "$RIG/hlds_linux" "$CLIENT/hl_linux"; do
	[ -e "$f" ] || { echo "missing: $f" >&2; exit 1; }
done

# Replace a library only when it changed, through a new file renamed into place: a running
# server keeps its mapping of the old one (cp over a mapped .so truncates it under the server).
put_lib() { # <src> <dst>
	cmp -s "$1" "$2" 2>/dev/null && return 0
	cp "$1" "$2.new" && mv -f "$2.new" "$2"
}

# A server gamedir: links to every top-level entry of the content except dlls/, cl_dlls/,
# liblist.gam and the client's own state, then a real dlls/ and liblist.gam.
server_gamedir() { # <dir> <liblist gamedll line>
	local d="$1" e b
	if [ ! -d "$d" ]; then
		mkdir -p "$d/dlls"
		for e in "$CONTENT"/*; do
			b="$(basename "$e")"
			case "$b" in dlls | cl_dlls | liblist.gam | SAVE | custom.hpk | voice_ban.dt) continue ;; esac
			ln -s "$e" "$d/$b"
		done
	fi
	printf 'game "Rocket Crowbar 2"\nversion "1"\nsvonly "0"\ncldll "1"\nmpentity "info_player_deathmatch"\n%s\n' "$2" > "$d/liblist.gam"
}

# server 1: the original, Windows HLDS under Wine, its own prefix
server_gamedir "$HLDS_WIN/rc2" 'gamedll "dlls\rc2.dll"'
put_lib "$ORIG" "$HLDS_WIN/rc2/dlls/rc2.dll"
PREFIX="$HLDS_WIN/wineprefix-rc2"
[ -d "$PREFIX" ] || WINEPREFIX="$PREFIX" WINEDEBUG=-all wineboot -i > /dev/null 2>&1

# server 2: ours, in a folder of links to the Linux rig with its own rc2/
if [ ! -d "$RIG2" ]; then
	mkdir "$RIG2"
	for e in "$RIG"/*; do
		b="$(basename "$e")"
		case "$b" in rc2 | rockcrowbar | rcours) continue ;; esac
		ln -s "$e" "$RIG2/$b"
	done
fi
server_gamedir "$RIG2/rc2" 'gamedll_linux "dlls/hl.so"'
put_lib "$OURS" "$RIG2/rc2/dlls/hl.so"

# client: a copy of the content (the client writes its config and saves into it) with our
# client.so; the Linux client can't load RC2's Windows client.dll, for either server
if [ ! -d "$CLIENT/rc2" ]; then
	mkdir "$CLIENT/rc2"
	rsync -a --exclude 'dlls/' --exclude 'cl_dlls/' --exclude 'SAVE/' "$CONTENT/" "$CLIENT/rc2/"
	printf 'game "Rocket Crowbar 2"\nversion "1"\nsvonly "0"\ncldll "1"\nmpentity "info_player_deathmatch"\ngamedll "dlls\\rc2.dll"\ngamedll_linux "dlls/hl.so"\n' > "$CLIENT/rc2/liblist.gam"
	mkdir -p "$CLIENT/rc2/cl_dlls"
fi
put_lib "$OURS_CL" "$CLIENT/rc2/cl_dlls/client.so"

# The Steam accounts logged in on this machine are super admins on ours (adminprim.cfg, read from
# the server's working directory, which is $RIG2). The engine reports STEAM_0 or STEAM_1 for the
# same account, so both are listed. Rewritten each run; ADMINS="STEAM_0:1:2 ..." adds others.
LOGINS="$HOME/.local/share/Steam/config/loginusers.vdf"
{
	echo "# written by tools/rig/setup.sh: <steamid> <level>, 2 = super admin"
	if [ -f "$LOGINS" ]; then
		grep -oE '"7656[0-9]{13}"' "$LOGINS" | tr -d '"' | while read -r id64; do
			a=$((id64 - 76561197960265728))
			echo "STEAM_0:$((a & 1)):$((a >> 1)) 2"
			echo "STEAM_1:$((a & 1)):$((a >> 1)) 2"
		done
	fi
	for id in ${ADMINS:-}; do echo "$id 2"; done
} > "$RIG2/adminprim.cfg"

echo "original: $(md5sum "$HLDS_WIN/rc2/dlls/rc2.dll" | cut -c1-12)  ours: $(md5sum "$RIG2/rc2/dlls/hl.so" | cut -c1-12)  client: $(md5sum "$CLIENT/rc2/cl_dlls/client.so" | cut -c1-12)"
echo "run: tools/rig/run.sh original | ours"
