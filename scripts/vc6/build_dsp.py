#!/usr/bin/env python3
"""build_dsp.py - build a Visual C++ 6 project (.dsp) under Wine, as its Release config says.

For reading the original RC2: RC2's rc2.dll and client.dll were built with VC6's compiler
12.00.8168 (their Rich headers), so the same SDK source built with the same compiler and
flags gives byte-identical stock functions, which Ghidra's Function ID then names in the
originals. Nothing built here ships.

Reads the .dsp's "Win32 Release" ADD CPP and ADD LINK32 lines and its SOURCE= list (skipping
files excluded from that config), compiles each .c/.cpp with cl.exe and links with link.exe,
keeping the .map (always) and asking for a .pdb, not incrementally (the Release default). /YX, /FD and /Fr (precompiled headers,
dependency and browse files) are dropped: they don't change code.

Usage:
  scripts/vc6/build_dsp.py <dir containing the .dsp> <name>.dsp <out dir>
Needs VC6 at $VC6 (default $VC6_HOME/VC98, with VC6_HOME defaulting to ~/vc6; see
scripts/vc6/README.md) and Wine; uses its own prefix ($WINEPREFIX, default $VC6_HOME/wineprefix).
"""
import os, re, shlex, subprocess, sys

VC6_HOME = os.environ.get("VC6_HOME", os.path.expanduser("~/vc6"))
VC6 = os.environ.get("VC6", os.path.join(VC6_HOME, "VC98"))
PREFIX = os.environ.get("WINEPREFIX", os.path.join(VC6_HOME, "wineprefix"))
DROP = {"/YX", "/FD", "/Fr", "/FR", "/c", "/nologo"}


def winpath(p):
    return "Z:" + os.path.abspath(p).replace("/", "\\")


def parse(dsp_path):
    text = open(dsp_path, encoding="latin-1").read().replace("\r\n", "\n")
    cfg, cpp, link, sources = None, None, None, []
    cur_src, excluded, src_cpp = None, {}, {}
    for line in text.split("\n"):
        m = re.match(r'!(?:IF|ELSEIF)\s+"\$\(CFG\)" == "(.*)"', line)
        if m:
            cfg = m.group(1)
            continue
        if line.startswith("!ENDIF"):
            cfg = None
            continue
        rel = cfg is not None and cfg.endswith("Win32 Release")
        if line.startswith("SOURCE="):
            cur_src = line[7:].strip().strip('"')
            sources.append(cur_src)
            continue
        if cur_src is None:
            if rel and line.startswith("# ADD CPP "):
                cpp = line[len("# ADD CPP "):]
            elif rel and line.startswith("# ADD LINK32 "):
                link = line[len("# ADD LINK32 "):]
        else:
            # inside a config block it applies to that config, outside one to all of them
            if (rel or cfg is None) and "PROP Exclude_From_Build 1" in line:
                excluded[cur_src] = True
            if rel and line.startswith("# ADD CPP "):
                src_cpp[cur_src] = line[len("# ADD CPP "):]
    srcs = [s for s in sources if s.lower().endswith((".c", ".cpp")) and not excluded.get(s)]
    return cpp, link, srcs, src_cpp


def wine(args, cwd):
    env = dict(os.environ, WINEPREFIX=PREFIX, WINEDEBUG="-all",
               INCLUDE=winpath(os.path.join(VC6, "INCLUDE")),
               LIB=winpath(os.path.join(VC6, "LIB")))
    env["PATH"] = os.environ["PATH"]
    r = subprocess.run(["wine", os.path.join(VC6, "BIN", args[0])] + args[1:], cwd=cwd, env=env,
                       capture_output=True, text=True, errors="replace")
    return r.returncode, r.stdout + r.stderr


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    proj_dir, dsp, out = sys.argv[1], sys.argv[2], os.path.abspath(sys.argv[3])
    os.makedirs(out, exist_ok=True)
    cpp, link, srcs, src_cpp = parse(os.path.join(proj_dir, dsp))
    base = [f for f in shlex.split(cpp.replace("\\", "/"), posix=True) if f not in DROP]
    base = [f.replace("/", "\\") if not f.startswith("/") else f for f in base]
    objs, failed = [], []
    for s in srcs:
        flags = base
        if s in src_cpp:  # a per-file ADD CPP adds to the project's flags
            flags = base + [f for f in shlex.split(src_cpp[s].replace("\\", "/")) if f not in DROP]
        src = s.replace("\\", "/")
        obj = os.path.join(out, re.sub(r"\W", "_", os.path.splitext(src)[0]) + ".obj")
        code, log = wine(["CL.EXE", "/nologo", "/c"] + flags +
                         ["/Fo" + winpath(obj), "/Fd" + winpath(os.path.join(out, "vc60.pdb")), src], proj_dir)
        if code != 0:
            failed.append(s)
            print("FAIL", s, "\n", log[-1500:])
        else:
            objs.append(obj)
    print("%d compiled, %d failed" % (len(objs), len(failed)))
    if failed:
        sys.exit(1)
    lf = [f for f in shlex.split(link.replace("\\", "/")) if not f.startswith("/out:") and f != "/nologo"]
    lf = [("/def:" + winpath(os.path.join(proj_dir, f[5:]))) if f.startswith("/def:") else
          (winpath(os.path.join(proj_dir, f)) if f.endswith(".lib") and "/" in f else f) for f in lf]
    name = os.path.splitext(dsp)[0]
    dll = os.path.join(out, name + ".dll")
    # /INCREMENTAL:NO is VC6's Release default (the IDE adds it; the .dsp line doesn't say it).
    # Without it /debug links incrementally: every call and vtable slot goes through a jump thunk
    # and functions get int3 padding, which the originals don't have.
    code, log = wine(["LINK.EXE", "/nologo", "/incremental:no"] + lf + ["/map:" + winpath(os.path.join(out, name + ".map")),
                     "/pdb:" + winpath(os.path.join(out, name + ".pdb")), "/out:" + winpath(dll)] +
                     [winpath(o) for o in objs], proj_dir)
    print(log[-3000:])
    if code != 0:
        sys.exit(1)
    print("->", dll)


if __name__ == "__main__":
    main()
