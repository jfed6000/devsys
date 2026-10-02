#!/usr/bin/env python3
"""native - run the devsys tools on an emulated F256 and compare the results.

Phase 7: the cross-built tools (native/bin, from native/GNUmakefile) go onto
a copy of a NitrOS9 Level 2 disk image, together with test inputs and a
`startup` file that runs the tests. The Wildbits MAME boots the image, the
shell runs `startup`, and the outputs are copied back off the image and
compared with what the host tools produce from the same inputs.

Suites (default: all):
  las   native las assembles every .a file of the cross-build (the DCC
        output for las, rlink and DCC's own C sources); each ROF must equal
        the host las's, apart from the assembly date
  link  native rlink links each tool from the host's .r files; each module
        must equal the host rlink's (native/bin). las and dcc68 are left
        out: linking them needs more memory than rlink has in 64K (phase 8)
  cc    native dcc compiles and links a small C program (native/hello.c)
        with the whole native chain (dcpp, dcc68, dco68, las, rlink); the
        module must equal the one the host passes build from the same
        source, and it must run. dcc's rlink puts it in /dd/CMDS

  python3 harness/native.py                  # all suites
  python3 harness/native.py las --keep       # keep the image for inspection
  python3 harness/native.py --image FILE     # another Jr2 image to start from

MAME (DESIGN.md "Phase 7 test plan"): ~/projects/wild/mame/mame, machine
wbjr2, -bios turbo, headless, never modified. The image is always a copy,
padded upward to 128 MB. A Lua script polls the image (with ToolShed's os9)
for the marker file the last startup line writes, then ends the run.
Work files go to harness/out/native/.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
HOME = os.path.expanduser("~")
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

MAME = os.path.join(HOME, "projects/wild/mame/mame")
OS9 = os.path.join(HOME, "projects/coco-shelf/bin/os9")
COCOC = os.path.join(HOME, "projects/dcc/CoCoC")
NATIVE = os.path.join(DEVSYS, "native")
IMAGE = os.path.join(HOME, "projects/wild/netrek/nitros9/recipes/wildbits/l2/l2_wildbitsjr2.dsk")
WORK = os.path.join(HERE, "out", "native")
PAD = 128 * 1024 * 1024         # 2^27 bytes: a block count MAME accepts
TOOLS = ["las", "rlink", "dcc", "dcpp", "dcc68", "dco68"]
LINKABLE = ["rlink", "dcc", "dcpp", "dco68"]     # fit native rlink's memory
MARKER = "MARKER"


def os9(*args, check=True):
    p = subprocess.run([OS9] + list(args), env=lwdiff.clean_env(), capture_output=True)
    if check and p.returncode:
        sys.exit(f"os9 {' '.join(args)}: {p.stderr.decode(errors='replace')}")
    return p.stdout.decode("latin-1")


def put(src, dst, text=False):
    """Copy a host file onto the image. Text files get CR line ends (-l)."""
    os9("copy", "-r", *(["-l"] if text else []), src, dst)


def get(src, dst):
    if os.path.exists(dst):
        os.remove(dst)
    p = subprocess.run([OS9, "copy", src, dst], env=lwdiff.clean_env(), capture_output=True)
    return p.returncode == 0 and os.path.exists(dst)


def os9_text(path):
    """A text file from the image, with LF line ends."""
    tmp = os.path.join(WORK, "text.tmp")
    if not get(path, tmp):
        return None
    with open(tmp, "rb") as f:
        return f.read().replace(b"\r", b"\n").decode("latin-1")


def make_image(src, dsk):
    shutil.copyfile(src, dsk)
    if os.path.getsize(dsk) > PAD:
        sys.exit(f"{src} is larger than {PAD} bytes")
    with open(dsk, "r+b") as f:     # pad upward only (never truncate)
        f.truncate(PAD)
    for t in TOOLS:
        os9("del", f"{dsk},CMDS/{t}", check=False)
        put(os.path.join(NATIVE, "bin", t), f"{dsk},CMDS/{t}")
        os9("attr", "-q", "-pe", "-npw", "-pr", "-e", "-w", "-r", f"{dsk},CMDS/{t}")
    for d in ("LIB", "TMP", "T"):
        os9("makdir", f"{dsk},{d}", check=False)
    for f in ("cstart.r", "clib.l", "sys.l"):
        put(os.path.join(COCOC, "Lib", f), f"{dsk},LIB/{f}")
    for f in ("level1.patterns", "level2.patterns"):
        put(os.path.join(DEVSYS, "dcc/COpt", f), f"{dsk},LIB/{f}", text=True)
    defs = os.path.join(COCOC, "Defs")
    for root, dirs, files in os.walk(defs):
        rel = os.path.relpath(root, defs)
        target = "DEFS" if rel == "." else "DEFS/" + rel
        if rel != ".":
            os9("makdir", f"{dsk},{target}", check=False)
        for f in files:
            put(os.path.join(root, f), f"{dsk},{target}/{f}", text=True)


# ------------------------------------------------------------------ suites
# Each suite stages its inputs on the image and returns (startup lines,
# check function). Paths on the image stay short: OS-9 names are at most
# 29 characters and the shell's line is limited.

def rof_equal(a, b):
    """Equal ROFs, ignoring the 5-byte assembly date (header offset 7)."""
    return len(a) == len(b) and a[:7] == b[:7] and a[12:] == b[12:]


def suite_las(dsk):
    lines, files = [], []
    for t in TOOLS:
        objs = sorted(a for a in glob.glob(os.path.join(NATIVE, "obj", t, "*.a"))
                      if not a.endswith(".raw.a"))
        if not objs:
            continue
        os9("makdir", f"{dsk},T/A{t}", check=False)
        lines.append(f"chd /dd/T/A{t}")
        for a in objs:
            b = os.path.splitext(os.path.basename(a))[0]
            put(a, f"{dsk},T/A{t}/{b}.a", text=True)
            lines.append(f"las -o={b}.r {b}.a >{b}.lst >>{b}.err")
            files.append((t, b))

    def check():
        bad = 0
        for t, b in files:
            out = os.path.join(WORK, "las", t, b + ".r")
            os.makedirs(os.path.dirname(out), exist_ok=True)
            ok = get(f"{dsk},T/A{t}/{b}.r", out)
            ref = open(os.path.join(NATIVE, "obj", t, b + ".r"), "rb").read()
            if ok and rof_equal(open(out, "rb").read(), ref):
                continue
            bad += 1
            err = os9_text(f"{dsk},T/A{t}/{b}.err") or ""
            print(f"  las {t}/{b}: {'differs' if ok else 'no output'} {err.strip()[:200]}")
        print(f"las: {len(files) - bad} of {len(files)} ROF files identical to host las")
        return bad == 0
    return lines, check


def suite_link(dsk):
    lines, tools = [], []
    for t in LINKABLE:
        rs = sorted(glob.glob(os.path.join(NATIVE, "obj", t, "*.r")))
        ref = os.path.join(NATIVE, "bin", t)
        if not rs or not os.path.exists(ref):
            continue
        # link in the makefile's order, which decides the module layout
        order = lwdiff_order(t)
        os9("makdir", f"{dsk},T/L{t}", check=False)
        names = os.path.join(WORK, f"{t}.names")
        with open(names, "w") as f:
            f.write("/dd/lib/cstart.r\n" + "".join(f"{o}.r\n" for o in order))
        put(names, f"{dsk},T/L{t}/names", text=True)
        for o in order:
            put(os.path.join(NATIVE, "obj", t, o + ".r"), f"{dsk},T/L{t}/{o}.r")
        # A full -o path: on OS-9 a bare name goes to the execution
        # directory (rlink opens it with mode "x"), i.e. into /dd/CMDS
        mem = subprocess.run(["make", "-s", "-f", "GNUmakefile", f"printmem-{t}"],
                             cwd=NATIVE, capture_output=True, text=True).stdout.strip()
        lines += [f"chd /dd/T/L{t}",
                  f"rlink -o=/dd/T/L{t}/{t} -n={t} {'-M=' + mem + ' ' if mem else ''}"
                  f"-m -z=names -l=/dd/lib/sys.l -l=/dd/lib/clib.l >map >>err"]
        tools.append(t)

    def check():
        bad = 0
        for t in tools:
            out = os.path.join(WORK, "link", t)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            ok = get(f"{dsk},T/L{t}/{t}", out)
            ref = open(os.path.join(NATIVE, "bin", t), "rb").read()
            if ok and open(out, "rb").read() == ref:
                continue
            bad += 1
            err = os9_text(f"{dsk},T/L{t}/err") or ""
            print(f"  link {t}: {'differs' if ok else 'no output'} {err.strip()[:200]}")
        print(f"link: {len(tools) - bad} of {len(tools)} modules identical to host rlink")
        return bad == 0
    return lines, check


def lwdiff_order(tool):
    """The tool's objects in native/GNUmakefile order."""
    out = subprocess.run(["make", "-s", "-f", "GNUmakefile", f"print-{tool}"],
                         cwd=NATIVE, capture_output=True, text=True).stdout.split()
    return out


HELLO = os.path.join(HERE, "native", "hello.c")


def suite_cc(dsk):
    """dcc on the F256 builds hello.c; the host passes build the reference."""
    os9("makdir", f"{dsk},T/C", check=False)
    put(HELLO, f"{dsk},T/C/hello.c", text=True)
    # every redirection needs a new file: the shell's > and >> create,
    # and creating an existing file fails (E$CEF, 218)
    lines = ["chd /dd/T/C", "dcc hello.c -f=hello >out >>err", "hello >run >>runerr"]

    def check():
        ref = host_hello()
        out = os.path.join(WORK, "cc", "hello")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        ok = get(f"{dsk},CMDS/hello", out)
        same = ok and open(out, "rb").read() == open(ref, "rb").read()
        printed = (os9_text(f"{dsk},T/C/run") or "").strip()
        print("  hello printed: " + (printed or "(nothing)"))
        same = same and printed == "hello from dcc on the F256: 45"
        if not same:
            print(f"  cc hello: {'differs' if ok else 'no output'}")
            print("  " + (os9_text(f"{dsk},T/C/err") or "").strip().replace("\n", "\n  "))
        print(f"cc: native dcc output {'identical to' if same else 'NOT identical to'} the host passes")
        return same
    return lines, check


def host_hello():
    """hello.c through the host passes, the way dcc runs them natively."""
    d = os.path.join(WORK, "cc-host")
    os.makedirs(d, exist_ok=True)
    env = lwdiff.clean_env()
    dcc = os.path.join(DEVSYS, "dcc")
    lib = os.path.join(COCOC, "Lib")

    def run(argv, **kw):
        p = subprocess.run(argv, cwd=d, env=env, capture_output=True, **kw)
        if p.returncode:
            sys.exit(f"host {argv[0]}: {p.stderr.decode(errors='replace')}")
        return p
    pre = run([os.path.join(dcc, "CPP/dcpp"), "-I=" + os.path.join(COCOC, "Defs"), HELLO])
    open(os.path.join(d, "hello.i"), "wb").write(pre.stdout)
    run([os.path.join(dcc, "CC09/dcc68"), "hello.i", "-o=hello.raw.a"])
    with open(os.path.join(d, "hello.raw.a"), "rb") as fin:
        opt = run([os.path.join(dcc, "COpt/dco68")], stdin=fin)
    open(os.path.join(d, "hello.a"), "wb").write(opt.stdout)
    run([os.path.join(DEVSYS, "las/build/unix/las"), "-o=hello.r", "hello.a"])
    run([os.path.join(DEVSYS, "rlink/rlink"), "-o=hello", "-n=hello",
         os.path.join(lib, "cstart.r"), "hello.r", "-l=" + os.path.join(lib, "clib.l")])
    return os.path.join(d, "hello")


SUITES = {"las": suite_las, "link": suite_link, "cc": suite_cc}


# ------------------------------------------------------------------ MAME

LUA = """
frames = 0
done = false
sub = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if frames %% 600 == 0 and not done then
    local p = io.popen('%(os9)s dir "%(dsk)s,T" 2>/dev/null')
    local s = p:read("*a")
    p:close()
    if s:find("%(marker)s") then
      done = true
      manager.machine.video:snapshot()
      print("native.py: marker found at frame " .. frames)
      manager.machine:exit()
    end
  end
end)
"""


def run_mame(dsk, seconds):
    lua = os.path.join(WORK, "drive.lua")
    with open(lua, "w") as f:
        f.write(LUA % {"os9": OS9, "dsk": dsk, "marker": MARKER})
    snap = os.path.join(WORK, "snap")
    shutil.rmtree(snap, ignore_errors=True)
    argv = [MAME, "wbjr2", "-bios", "turbo", "-hard", dsk, "-skip_gameinfo",
            "-video", "none", "-sound", "none", "-nothrottle",
            "-autoboot_script", lua, "-snapshot_directory", snap,
            "-seconds_to_run", str(seconds)]
    t0 = time.time()
    p = subprocess.run(argv, cwd=WORK, env=lwdiff.clean_env(), capture_output=True)
    log = p.stdout.decode("latin-1") + p.stderr.decode("latin-1")
    open(os.path.join(WORK, "mame.log"), "w").write(log)
    found = "marker found" in log
    print(f"MAME: {time.time() - t0:.0f} s wall, "
          f"{'finished' if found else 'NO MARKER (timed out or crashed)'}")
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("suites", nargs="*", help="las, link, cc (default: all)")
    ap.add_argument("--image", default=IMAGE, help="Jr2 disk image to copy")
    ap.add_argument("--seconds", type=int, default=3600,
                    help="emulated seconds before giving up")
    ap.add_argument("--keep", action="store_true", help="keep the test image")
    args = ap.parse_args()
    suites = args.suites or list(SUITES)
    for s in suites:
        if s not in SUITES:
            sys.exit(f"unknown suite {s}: choose from {', '.join(SUITES)}")

    if subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=NATIVE).returncode:
        sys.exit("native build failed")
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    dsk = os.path.join(WORK, "test.dsk")
    make_image(args.image, dsk)

    lines, checks = ["-x"], []        # -x: keep going after an error
    for s in suites:
        l, c = SUITES[s](dsk)
        lines += l
        checks.append(c)
    lines.append(f"echo done >/dd/T/{MARKER}")
    startup = os.path.join(WORK, "startup")
    with open(startup, "w") as f:
        f.write("\n".join(lines) + "\n")
    put(startup, f"{dsk},startup", text=True)

    finished = run_mame(dsk, args.seconds)
    results = [c() for c in checks]
    if not args.keep:
        os.remove(dsk)
    sys.exit(0 if finished and all(results) else 1)


if __name__ == "__main__":
    main()
