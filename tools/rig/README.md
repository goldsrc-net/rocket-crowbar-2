# tools/rig - running the original and the rebuild side by side

Two dedicated servers on this machine, both with gamedir `rc2`, so one client folder joins
either:

```
original RC2 beta 1.1 (reference/rc2.dll)   tools/rig/run.sh original   port 27031   Windows HLDS, Wine
our rebuild (build-i386/dlls/hl.so)         tools/rig/run.sh ours       port 27032   Linux ReHLDS

steam -applaunch 70 -game rc2 +connect 127.0.0.1:<port>   (or "connect" in the console)
```

Set up or refresh with `tools/rig/setup.sh` after `scripts/build.sh i386 all`. It copies our
latest `hl.so` and `client.so` every time and creates the rest only if missing. The default map
is `rc_sandcrab`; `run.sh <which> <map>` takes another.

## What it sets up

The paths below are the defaults; `RC2_CONTENT` (RC2's content: the beta 1.1 installer's `rc2`
folder, default `~/Downloads/rc2`), `HLDS_WIN`, `RIG`, `RIG2` and `CLIENT` override them, for
`setup.sh`, `run.sh` and `smoke.py` alike.

- `~/Containers/hlds/hlds-win/rc2/` (`HLDS_WIN`): links to RC2's content and a real
  `dlls/rc2.dll`, in a Windows HLDS install (the one mod-ts uses, fetched with steamcmd). The
  original runs there under Wine with its own prefix, `hlds-win/wineprefix-rc2`: mod-ts's
  `test-win-server.sh` kills the shared prefix's wineserver on each run.
- `~/Containers/hlds/hlds-linux32-rc2/`: links to every entry of the 32-bit ReHLDS rig
  (`hlds-linux32-test`), plus a real `rc2/` of content links with our `hl.so`.
- `~/.local/share/Steam/steamapps/common/Half-Life/rc2/`: the client's mod folder, a copy of the
  content with RC2's own `config.cfg` (60 binds) and our `client.so`. The Linux client can't
  load RC2's Windows `client.dll`, so it plays both servers with ours.
- `~/Containers/hlds/hlds-linux32-rc2/adminprim.cfg`: the Steam accounts logged in on this machine
  (`~/.local/share/Steam/config/loginusers.vdf`) as super admins (level 2) on ours, in both the
  `STEAM_0` and `STEAM_1` form; `ADMINS="STEAM_0:1:2 ..."` adds others. Rewritten on every run.
- Servers run with `+sv_cheats 1`. The original keeps `+sv_lan 1` (its admin file takes WON ids,
  which no engine issues now). Ours runs `+sv_lan 0 -nomaster`: players carry their Steam IDs, so
  the admin file works, and it still doesn't list itself on Valve's master servers. In that mode
  it doesn't answer server browser queries, so it won't show in the LAN tab: join with `connect`.

## Smoke test

`tools/rig/smoke.py original|ours <log>` boots one server headless on its own port (27041,
27042, clear of run.sh's), reads back RC2's cvars, changes map through rc_sandcrab, rc_race and
rc_oneking over rcon, quits, and saves the console plus the rcon replies. `--clean` drops what
differs between the Windows engine and ReHLDS rather than between the games; compare sorted,
since the two engines send a command's output to different places (the docstring has the
details and the expected differences). As of 2026-11-07 the only game difference is the
`sk_plr_hornet_dmg1` warning (`reference/DEVIATIONS.md`).

```sh
tools/rig/smoke.py original /tmp/o.log; tools/rig/smoke.py ours /tmp/u.log
diff <(tools/rig/smoke.py --clean /tmp/o.log | sort -u) <(tools/rig/smoke.py --clean /tmp/u.log | sort -u)
```

`forkliftfun` (one of the extra maps) loads on neither: its `alex2.wad` isn't in the content.

## Undo

Stop both servers, then move these four away (nothing else is touched):

```
~/Containers/hlds/hlds-win/rc2
~/Containers/hlds/hlds-win/wineprefix-rc2
~/Containers/hlds/hlds-linux32-rc2
~/.local/share/Steam/steamapps/common/Half-Life/rc2
```
