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

- `/media/Data3/Projects/goldsrc.net/hlds-win/rc2/`: links to RC2's content
  (`/media/Data3/Projects/mods/rc2`) and a real `dlls/rc2.dll`, in the Windows HLDS mod-ts uses.
  The original runs there under Wine with its own prefix, `hlds-win/wineprefix-rc2`: mod-ts's
  `test-win-server.sh` kills the shared prefix's wineserver on each run.
- `~/Containers/hlds/hlds-linux32-rc2/`: links to every entry of the 32-bit ReHLDS rig
  (`hlds-linux32-test`), plus a real `rc2/` of content links with our `hl.so`.
- `~/.local/share/Steam/steamapps/common/Half-Life/rc2/`: the client's mod folder, a copy of the
  content with RC2's own `config.cfg` (60 binds) and our `client.so`. The Linux client can't
  load RC2's Windows `client.dll`, so it plays both servers with ours; until the client is
  reconstructed that is Half-Life's client with RC2's content.
- Servers run with `+sv_cheats 1` and `+sv_lan 1`.

## Undo

Stop both servers, then move these four away (nothing else is touched):

```
/media/Data3/Projects/goldsrc.net/hlds-win/rc2
/media/Data3/Projects/goldsrc.net/hlds-win/wineprefix-rc2
~/Containers/hlds/hlds-linux32-rc2
~/.local/share/Steam/steamapps/common/Half-Life/rc2
```
