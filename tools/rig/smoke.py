#!/usr/bin/env python3
"""smoke.py - boot the original RC2 or our rebuild headless, drive its console, save the log.

Both run as tools/rig/run.sh runs them (the original under Wine in the Windows HLDS, ours in the
32-bit ReHLDS rig), on ports clear of run.sh's 27031/27032, so this works while those run:

  tools/rig/smoke.py original out-orig.log
  tools/rig/smoke.py ours out-ours.log
  diff <(tools/rig/smoke.py --clean out-orig.log) <(tools/rig/smoke.py --clean out-ours.log)

The script: start the server (developer 1, LAN, deathmatch), wait, then send each console command in
SCRIPT after its delay over rcon (the Windows server under Wine reads no stdin), end with "quit" and
wait for the exit. It changes map through three of RC2's own maps and asks for RC2's cvars, so the
defaults, the map loads and the console output of the two can be compared. After mod-rockcrowbar's
tools/rig/smoke.py. (forkliftfun, one of the extra maps, can't load: its alex2.wad isn't in the
content.)

The log is the server's console, then each rcon command with its reply (which is where the cvar
values and status go). The console: the Linux server's stdout; the Windows one's qconsole.log
(-condebug; under Wine its console doesn't reach stdout).

--clean drops what differs between the two engines and not the games (engine banners, timings, the
.nod and dedicated-config lines, the Windows engine's extra config echo, challenges, addresses, and
the lines one engine routes to rcon and the other to its console), so what is left to differ is the
game library's. It keeps the order; the engines route a command's output differently (the Windows
one back over rcon, ReHLDS to its console), so compare sorted too:
  diff <(smoke.py --clean a.log | sort -u) <(smoke.py --clean b.log | sort -u)
A missing .nod gives ".NOD File will be updated" on the Windows engine and "*Error opening .NOD
file" on ours (halflife-updated's file-time compare); both then build the graph. Known and expected (reference/DEVIATIONS.md): ours prints
"GetSkillCVar Got a zero for sk_plr_hornet_dmg1" on every map; the original's rcon gives no reply
for a cvar query, so the cvar values show on ours only (reference/cvars-rc2.tsv has the original's).
"""
import argparse, os, re, socket, subprocess, sys, time

RCON_PW = "smoke"

RIG2 = os.environ.get("RIG2", os.path.expanduser("~/Containers/hlds/hlds-linux32-rc2"))
HLDS_WIN = os.environ.get("HLDS_WIN", "/media/Data3/Projects/goldsrc.net/hlds-win")

# RC2's own cvars (reference/cvars-rc2.tsv)
CVARS = ["mp_teamannoy", "mp_riding", "mp_random", "mp_scoremode", "mp_allowtransform",
         "sv_allowgive", "sv_noexit", "sv_chglevel", "sv_pcstuff", "sv_tempadmins", "rccfgfile"]

# (seconds after the previous command, command)
SCRIPT = ([(15, "status")] + [(1, c) for c in CVARS] +
          [(2, "changelevel rc_race"), (15, "status"),
           (2, "changelevel rc_oneking"), (15, "status"), (2, "quit")])


def rcon(port, command):
    """GoldSrc rcon: ask for a challenge, then send the command with it. The reply text, or None."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(3)
    try:
        s.sendto(b"\xff\xff\xff\xffchallenge rcon\n", ("127.0.0.1", port))
        reply = s.recv(1400)
        ch = re.search(rb"challenge rcon (\d+)", reply).group(1).decode()
        s.sendto(b"\xff\xff\xff\xff" + ('rcon %s "%s" %s' % (ch, RCON_PW, command)).encode(), ("127.0.0.1", port))
        text = []
        try:
            while True:
                d = s.recv(1400)
                text.append(d[5:].decode(errors="replace").rstrip("\x00"))  # ff ff ff ff 'l'
                s.settimeout(0.5)
        except socket.timeout:
            pass
        return "".join(text)
    except (socket.timeout, OSError, AttributeError):
        return None
    finally:
        s.close()


ENGINE_NOISE = (r"^Protocol version|^Exe version|^Exe build|^Server IP address|^WARNING: radv|minidump"
                r"|ELFCLASS32|fixme:|err:|\.nod\b|\.NOD [Ff]ile|^Executing dedicated server config"
                r"|^Sys_InitializeGameDLL|^execing (config|server|skill)\.cfg|^\"[a-z_]+\" changed to"
                r"|^couldn't exec|VAC secure|^Adding:|^Dll loaded|^Texture load|failed to locate sequence"
                r"|^ +[0-9]+ (users|fakeclients)|^hostname|^version|^tcp/ip|^\s*$"
                r"|^Console initialized|^STEAM Auth Server|^rcon \d+ \"|v_dark\b"
                # what the Windows engine sends back over rcon and ReHLDS prints on its console (and
                # the reverse for a cvar's value): compared in the rcon section only
                r"|^GAME SKILL LEVEL|^\"[a-z_0-9]+\" is \"")


def clean(path):
    """A log with what differs between the engines (not the games) removed, for diffing."""
    out = []
    for line in open(path, errors="replace"):
        line = line.replace("\r", "").rstrip("\n")
        line = re.sub(r"^\d\d/\d\d \d\d:\d\d:\d\d ", "", line)
        line = re.sub(r"(\d+\.){3}\d+:\d+", "ADDR", line)
        line = re.sub(r'^Cvar_Set: variable "([^"]+)"', r"Cvar_Set: variable \1", line)
        if re.search(ENGINE_NOISE, line):
            continue
        out.append(line)
    print("\n".join(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("which", nargs="?", choices=["original", "ours"])
    ap.add_argument("log", nargs="?")
    ap.add_argument("--map", default="rc_sandcrab")
    ap.add_argument("--clean", metavar="LOG")
    a = ap.parse_args()
    if a.clean:
        return clean(a.clean)
    if not a.which or not a.log:
        ap.error("which and log are required")
    # developer 2: the Windows engine prints at_error messages only from 2, ReHLDS from 1
    common = ["-game", "rc2", "+rcon_password", RCON_PW, "+developer", "2", "+sv_cheats", "1",
              "+maxplayers", "8", "+sv_lan", "1", "+deathmatch", "1"]
    if a.which == "original":
        env = dict(os.environ, WINEPREFIX=os.path.join(HLDS_WIN, "wineprefix-rc2"), WINEDEBUG="-all",
                   DBUS_SYSTEM_BUS_ADDRESS="unix:path=/nonexistent",
                   DBUS_SESSION_BUS_ADDRESS="unix:path=/nonexistent")
        port, cwd = 27041, HLDS_WIN
        cmd = ["wine", "hlds.exe", "-console", "-condebug"] + common + ["-port", str(port), "+map", a.map]
        console = os.path.join(HLDS_WIN, "qconsole.log")
        if os.path.exists(console):
            os.remove(console)
    else:
        env = dict(os.environ, LD_LIBRARY_PATH=".:" + os.environ.get("LD_LIBRARY_PATH", ""))
        port, cwd = 27042, RIG2
        cmd = ["./hlds_linux", "-nobreakpad"] + common + ["-port", str(port), "+map", a.map]
        console = a.log + ".stdout"
    out = open(a.log + ".stdout", "w")
    p = subprocess.Popen(cmd, cwd=cwd, env=env, stdin=subprocess.DEVNULL, stdout=out,
                         stderr=subprocess.STDOUT, text=True)
    replies, lost = [], []
    for delay, c in SCRIPT:
        time.sleep(delay)
        if p.poll() is not None:
            break
        r = rcon(port, c)
        if r is None:
            lost.append(c)
        replies.append("] %s\n%s" % (c, r or ""))
    try:
        rc = p.wait(timeout=30)
    except subprocess.TimeoutExpired:
        p.kill()
        rc = "killed (no exit after quit)"
    out.close()
    with open(a.log, "w") as f:
        f.write(open(console, errors="replace").read() if os.path.exists(console) else "")
        f.write("\n=== rcon\n" + "\n".join(replies) + "\n")
    print(f"{a.which}: exit {rc}" + (f"; no rcon answer for {lost}" if lost else ""))


if __name__ == "__main__":
    sys.exit(main())
