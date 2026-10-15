#!/usr/bin/env python3
"""native - run the devsys tools on an emulated F256 and compare the results.

Phase 7: the cross-built tools (native/bin, from native/GNUmakefile) go onto
a copy of a NitrOS9 Level 2 disk image, together with test inputs and a
`startup` file that runs the tests. The Wildbits MAME boots the image, the
shell runs `startup`, and the outputs are copied back off the image and
compared with what the host tools produce from the same inputs.

Suites (default: all):
  las   native las assembles every .a file of the cross-build (the DCC
        output for las, llink and DCC's own C sources); each ROF must equal
        the host las's, apart from the assembly date
  link  native llink links each tool from the host's .r files; each module
        must equal the host llink's (native/bin)
  cc    native dcc compiles and links a small C program (native/hello.c)
        with the whole native chain (dcpp, dcc68, dco68, las, llink); the
        module must equal the one the host passes build from the same
        source, and it must run. dcc's llink puts it in /dd/CMDS

  self  step 17: every tool built on the F256 by make (dcc, native
        llink); each object must equal the host passes', each module the
        cross-built one (native/bin). --only TOOL for some. Not in the
        default set
  lw    native las assembles NitrOS9 module sources from the Level 2
        build (lwdiff's jobs.json; run harness/lwdiff.py first). Each
        module must equal the host las's (with ,pcr sizing, D38)
        on the same files. Slow: about 40 s of wall time per file
        (--shard to split it over several MAMEs). Not in
        the default set; --only TEXT picks files whose id contains TEXT
  obj   phase 10: native las assembles the 90 object-format sources (the
        libraries' and the .as programs'), merge builds libwildbitsl2,
        libnet and libalib, and native llink --lwlink links the build's 13
        programs. Objects must equal the host las's, programs the host
        llink's from those objects. Not in the default set
  img   phase 9: the 57 commands of level1/cmds (one directory, one
        configuration, as the real build has them) assembled twice by
        native las: without a defs image, then with --defsimg (the first
        records it, the others load it). Every module both times must
        equal the host las's; the emulated clock (date -t) is read
        before and after each round. Not in the default set

  python3 harness/native.py                  # las, link, cc
  python3 harness/native.py lw --only scfg   # one NitrOS9 file
  python3 harness/native.py las --keep       # keep the image for inspection
  python3 harness/native.py --image FILE     # another Jr2 image to start from

MAME (DESIGN.md "Phase 7 test plan"): ~/projects/wild/mame/mame, machine
wbjr2, -bios turbo, headless, never modified. The image is always a copy,
padded upward to 128 MB. A Lua script polls the image (with ToolShed's os9)
for the marker file the last startup line writes, then ends the run.
Work files go to harness/out/native/ (--work DIR for another place).
--shard K/N runs every Nth lw job starting at K, so N runs with their own
--work directories can share the hour-long lw suite:
  python3 harness/native.py lw --shard 0/6 --work harness/out/native0
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
# The C libraries devsys builds (step 18) and its os9defs.a (CoCoC's without
# the unfinished CMOC blocks no RMA-syntax assembler takes)
CLIB = os.path.join(DEVSYS, "clib", "lib")
CLIBDEFS = os.path.join(DEVSYS, "clib", "defs")
NATIVE = os.path.join(DEVSYS, "native")
# The devsys boot disk (boot/wildbits/l2: NitrOS9 main plus pipes and /nil,
# D44); build it with "make" there
IMAGE = os.path.join(DEVSYS, "boot/wildbits/l2/l2_wildbits_devsysjr2.dsk")
WORK = os.path.join(HERE, "out", "native")
PAD = 128 * 1024 * 1024         # 2^27 bytes: a block count MAME accepts
TOOLS = ["las", "llink", "dcc", "dcpp", "dcc68", "dco68", "make"]
LINKABLE = TOOLS
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


def make_image(src, dsk, dirs=("LIB", "TMP", "T")):
    shutil.copyfile(src, dsk)
    if os.path.getsize(dsk) > PAD:
        sys.exit(f"{src} is larger than {PAD} bytes")
    with open(dsk, "r+b") as f:     # pad upward only (never truncate)
        f.truncate(PAD)
    for t in TOOLS:
        os9("del", f"{dsk},CMDS/{t}", check=False)
        put(os.path.join(NATIVE, "bin", t), f"{dsk},CMDS/{t}")
        os9("attr", "-q", "-pe", "-npw", "-pr", "-e", "-w", "-r", f"{dsk},CMDS/{t}")
    for d in dirs:
        os9("makdir", f"{dsk},{d}", check=False)
    # make's default rules (D45): /dd/sys/make.default
    os9("makdir", f"{dsk},SYS", check=False)
    os9("del", f"{dsk},SYS/make.default", check=False)
    put(os.path.join(DEVSYS, "make", "make.default"), f"{dsk},SYS/make.default", text=True)
    # dcc68's messages (step 17): /dd/sys/dcc68.msg
    os9("del", f"{dsk},SYS/dcc68.msg", check=False)
    put(os.path.join(DEVSYS, "dcc", "CC09", "dcc68.msg"), f"{dsk},SYS/dcc68.msg", text=True)
    for f in ("cstart.r", "clib.l", "sys.l"):
        put(os.path.join(CLIB, f), f"{dsk},LIB/{f}")
    for f in ("level1.patterns", "level2.patterns"):
        put(os.path.join(DEVSYS, "dcc/COpt", f), f"{dsk},LIB/{f}", text=True)
    defs = os.path.join(COCOC, "Defs")
    for root, dirs, files in os.walk(defs):
        rel = os.path.relpath(root, defs)
        target = "DEFS" if rel == "." else "DEFS/" + rel
        if rel != ".":
            os9("makdir", f"{dsk},{target}", check=False)
        for f in files:
            src = os.path.join(root, f)
            if rel == "." and os.path.exists(os.path.join(CLIBDEFS, f)):
                src = os.path.join(CLIBDEFS, f)
            put(src, f"{dsk},{target}/{f}", text=True)


# ------------------------------------------------------------------ suites
# Each suite stages its inputs on the image and returns (startup lines,
# check function). Paths on the image stay short: OS-9 names are at most
# 29 characters and the shell's line is limited.

def rof_equal(a, b):
    """Equal ROFs, ignoring the 5-byte assembly date (header offset 7)."""
    return len(a) == len(b) and a[:7] == b[:7] and a[12:] == b[12:]


def only_dtb(a, b, crc=0):
    """a and b differ only in one run of at most 6 bytes: what dtb (the
    assembly date and time, wbinfo) puts in the code. crc: also ignore
    that many bytes at the end (a module's CRC, which the date changes)."""
    if len(a) != len(b):
        return False
    diff = [i for i in range(len(a) - crc) if a[i] != b[i]]
    return bool(diff) and diff[-1] - diff[0] < 6


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

    # lwasm-mode features the corpus lacks (D46): settling, the zero register
    os9("makdir", f"{dsk},T/AS", check=False)
    put(SETTLE, f"{dsk},T/AS/settle.asm", text=True)
    # options in a file: the OS-9 shell splits arguments at commas
    argf = os.path.join(WORK, "settle.args")
    with open(argf, "w") as f:
        f.write(f"--pragma={SETTLE_PRAGMAS}\n-o=settle\n")
    put(argf, f"{dsk},T/AS/args", text=True)
    lines += ["chd /dd/T/AS", "las settle.asm --args=args >lst >>err"]

    def check():
        bad = 0
        out = os.path.join(WORK, "las", "settle")
        ref = out + ".ref"
        os.makedirs(os.path.dirname(out), exist_ok=True)
        subprocess.run([REF, SETTLE, "--pragma=" + SETTLE_PRAGMAS, "-o=" + ref],
                       env=lwdiff.clean_env(), capture_output=True)
        if not (get(f"{dsk},T/AS/settle", out) and os.path.exists(ref)
                and open(out, "rb").read() == open(ref, "rb").read()):
            bad += 1
            print(f"  las settle.asm: differs or missing {(os9_text(f'{dsk},T/AS/err') or '').strip()[:200]}")
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
        print(f"las: {len(files) + 1 - bad} of {len(files) + 1} ROF files and settle.asm "
              f"identical to host las")
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
        # directory (llink opens it with mode "x"), i.e. into /dd/CMDS
        mem = subprocess.run(["make", "-s", "-f", "GNUmakefile", f"printmem-{t}"],
                             cwd=NATIVE, capture_output=True, text=True).stdout.strip()
        lines += [f"chd /dd/T/L{t}",
                  f"llink -o=/dd/T/L{t}/{t} -n={t} {'-M=' + mem + ' ' if mem else ''}"
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
        print(f"link: {len(tools) - bad} of {len(tools)} modules identical to host llink")
        return bad == 0
    return lines, check


def lwdiff_order(tool):
    """The tool's objects in native/GNUmakefile order."""
    out = subprocess.run(["make", "-s", "-f", "GNUmakefile", f"print-{tool}"],
                         cwd=NATIVE, capture_output=True, text=True).stdout.split()
    return out


HELLO = os.path.join(HERE, "native", "hello.c")
SETTLE = os.path.join(HERE, "native", "settle.asm")
SETTLE_PRAGMAS = "pcaspcr,nosymbolcase,condundefzero,undefextern,dollarnotlocal,noforwardrefmax"
# The cc suite's programs: source, and the line each must print. longret
# checks long and double results returned in X (step 14, D43); clibtest
# what step 18 fixed in the C library and in dcc68's constants (D52);
# regwidth that dcc68's register tracking keeps char and int apart (D53);
# unsgn that an unsigned comparison with 0 tests no stale carry (D59)
CC_PROGS = [("hello", HELLO, "hello from dcc on the F256: 45"),
            ("longret", os.path.join(HERE, "native", "longret.c"), "longret: all right"),
            ("clibtest", os.path.join(HERE, "native", "clibtest.c"), "clibtest: all right"),
            ("regwidth", os.path.join(HERE, "native", "regwidth.c"), "regwidth: all right"),
            ("unsgn", os.path.join(HERE, "native", "unsgn.c"), "unsgn: all right")]


def suite_cc(dsk):
    """dcc on the F256 builds each CC_PROGS program and runs it; the host
    passes build the reference."""
    os9("makdir", f"{dsk},T/C", check=False)
    # every redirection needs a new file: the shell's > and >> create,
    # and creating an existing file fails (E$CEF, 218)
    lines = ["chd /dd/T/C"]
    for name, src, _ in CC_PROGS:
        put(src, f"{dsk},T/C/{name}.c", text=True)
        lines += [f"dcc {name}.c -f={name} >{name}.out >>{name}.err",
                  f"{name} >{name}.run >>{name}.rerr"]
    # The devsys disk's pipes and /nil (D44): dcc -w sends dcc68's output
    # to /nil; a shell pipeline needs PipeMan, Piper and Pipe
    lines += ["dcc hello.c -w >w.out >>w.err", "echo w done >w.ok",
              "echo through a pipe ! tee pipe.tee >pipe.out"]

    def check():
        good = 0
        for name, src, want in CC_PROGS:
            ref = host_cc(name, src)
            out = os.path.join(WORK, "cc", name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            ok = get(f"{dsk},CMDS/{name}", out)
            same = ok and open(out, "rb").read() == open(ref, "rb").read()
            printed = (os9_text(f"{dsk},T/C/{name}.run") or "").strip()
            print(f"  {name} printed: " + (printed or "(nothing)"))
            same = same and printed == want
            if not same:
                print(f"  cc {name}: {'differs' if ok else 'no output'}")
                print("  " + (os9_text(f"{dsk},T/C/{name}.err") or "").strip().replace("\n", "\n  "))
            good += same
        print(f"cc: {good} of {len(CC_PROGS)} programs built by native dcc identical to "
              f"the host passes, and printed what they should")
        werr = (os9_text(f"{dsk},T/C/w.err") or "").strip()
        # success: dcc68's line is the last thing dcc -w printed (on a disk
        # without /nil, "can't open /nil" follows it)
        wlast = werr.splitlines()[-1] if werr else ""
        wok = wlast.strip().startswith("dcc68:") and "-o=/nil" in wlast
        piped = (os9_text(f"{dsk},T/C/pipe.tee") or "").strip()
        pok = piped == "through a pipe"
        print(f"cc: dcc -w through /nil {'works' if wok else 'FAILS'}; "
              f"a pipe {'works' if pok else 'FAILS'}" + ("" if wok else f" ({werr[:120]!r})"))
        return good == len(CC_PROGS) and wok and pok
    return lines, check


MAKEDIR = os.path.join(HERE, "native", "make")


def suite_make(dsk):
    """Step 15 (D45): make builds a small C and assembly project with its
    default rules, rebuilds only what changed, and stops at a failing
    step. OS-9 dates are to the minute, so the steps are a minute apart
    (sleep counts ticks, 60 a second)."""
    os9("makdir", f"{dsk},T/M", check=False)
    for f in ("main.c", "util.c", "utilbad.c", "dbl.a", "makefile"):
        put(os.path.join(MAKEDIR, f), f"{dsk},T/M/{f}", text=True)
    lines = ["chd /dd/T/M",
             "make >m1.out >>m1.err", "prog >r1 >>r1.err",
             "sleep 3700", "touch util.c",
             "make >m2.out >>m2.err", "prog >r2 >>r2.err",
             # copy keeps the source's date, older than util.r: touch it
             "sleep 3700", "del util.c", "copy utilbad.c util.c", "touch util.c",
             "make >m3.out >>m3.err", "echo after >m3.ok"]

    def check():
        def text(f):
            return (os9_text(f"{dsk},T/M/{f}") or "").strip()
        m1, m2, m3 = text("m1.out"), text("m2.out"), text("m3.out")
        steps = [
            ("first make builds everything",
             all(w in m1 for w in ("main.c", "util.c", "dbl.a", "-f=prog")) and
             text("r1") == "prog: 5 42"),
            ("after touching util.c, make compiles only util.c and relinks",
             "util.c" in m2 and "main.c" not in m2 and "dbl.a" not in m2 and
             "-f=prog" in m2 and text("r2") == "prog: 5 42"),
            ("a failing compile stops make before the link",
             "util.c" in m3 and "-f=prog" not in m3 and
             "***" in text("m3.err") and text("m3.ok") == "after"),
        ]
        for what, ok in steps:
            print(f"  make: {'ok' if ok else 'FAILED'}: {what}")
        if not all(ok for _, ok in steps):
            for f in ("m1.out", "m1.err", "m2.out", "m2.err", "m3.out", "m3.err", "r1", "r2"):
                print(f"  --- {f}\n  " + text(f).replace("\n", "\n  "))
        good = sum(ok for _, ok in steps)
        print(f"make: {good} of {len(steps)} steps right")
        return good == len(steps)
    return lines, check


def host_cc(name, src):
    """One C program through the host passes, the way dcc runs them natively."""
    d = os.path.join(WORK, "cc-host")
    os.makedirs(d, exist_ok=True)
    env = lwdiff.clean_env()
    dcc = os.path.join(DEVSYS, "dcc")
    lib = CLIB

    def run(argv, **kw):
        p = subprocess.run(argv, cwd=d, env=env, capture_output=True, **kw)
        if p.returncode:
            sys.exit(f"host {argv[0]}: {p.stderr.decode(errors='replace')}")
        return p
    pre = run([os.path.join(dcc, "CPP/dcpp"), "-I=" + os.path.join(COCOC, "Defs"), src])
    open(os.path.join(d, name + ".i"), "wb").write(pre.stdout)
    run([os.path.join(dcc, "CC09/dcc68"), name + ".i", f"-o={name}.raw.a"])
    with open(os.path.join(d, name + ".raw.a"), "rb") as fin:
        opt = run([os.path.join(dcc, "COpt/dco68")], stdin=fin)
    open(os.path.join(d, name + ".a"), "wb").write(opt.stdout)
    run([os.path.join(DEVSYS, "las/build/unix/las"), f"-o={name}.r", name + ".a"])
    run([os.path.join(DEVSYS, "llink/llink"), f"-o={name}", f"-n={name}",
         os.path.join(lib, "cstart.r"), name + ".r", "-l=" + os.path.join(lib, "clib.l")])
    return os.path.join(d, name)


JOBS = os.path.join(HERE, "out", "l2-jr2", "jobs.json")
GEN = os.path.join(HERE, "out", "l2-jr2", "generated")
# The reference for native lwasm-mode output: the host las, ",pcr" sizing
# included (step 13; until then las/build/nosz, DESIGN.md D21)
REF = os.path.join(DEVSYS, "las/build/unix/las")
ONLY = []
SHARD = [0, 1]                  # this run's index, number of runs


def is_text(path):
    data = open(path, "rb").read()
    return b"\0" not in data


def stage_lw(fmt="os9"):
    """lwdiff's jobs of one format (os9: modules, obj: ROF objects) and, for each, the files it reads, placed as
    (source, [(file, place)]) where place is "S/name" (shared: the name
    is unique), "J/src/name" (the job's own) or "J/name" (reached as
    ../name from the source's directory)."""
    import json
    import collections
    jobs = [j for j in json.load(open(JOBS)) if j["format"] == fmt]
    if ONLY:
        jobs = [j for j in jobs if any(o in j["id"] for o in ONLY)]
    jobs = jobs[SHARD[0]::SHARD[1]]

    def find(name, here, incs):
        for d in [here] + incs + [GEN]:
            q = os.path.normpath(os.path.join(d, name))
            if os.path.isfile(q):
                return q
        return None
    uses = {}
    owners = collections.defaultdict(set)
    for j in jobs:
        seen, stack = {}, [(j["source"], None)]
        while stack:
            path, how = stack.pop()
            if path in seen:
                continue
            seen[path] = how
            for line in open(path, encoding="latin-1"):
                t = line.split()
                if line[:1].isspace() and len(t) >= 2 and \
                        t[0].lower() in ("use", "include", "includebin"):
                    q = find(t[1], os.path.dirname(path), j["includes"])
                    if q:
                        stack.append((q, t[1]))
        uses[j["id"]] = seen
        for path in seen:
            owners[os.path.basename(path)].add(path)
    out = []
    for j in jobs:
        files = []
        for path, how in uses[j["id"]].items():
            name = os.path.basename(path)
            if path == j["source"]:
                place = "J/src/" + name
            elif how and "/" in how:
                # use ../defs/x.d: the same layout around the source
                rel = os.path.normpath(os.path.join("src", how))
                if rel.startswith(".."):
                    sys.exit(f"{j['id']}: can't stage {how}")
                place = "J/" + rel
            elif len(owners[name]) == 1:
                place = "S/" + name
            elif how and how.startswith("../"):
                place = "J/" + name
            else:
                place = "J/src/" + name
            files.append((path, place))
        out.append((j, files))
    return out


def lw_args(j, flag=True):
    """The job's las options. Natively they go in a file read with
    --args= (las file_io.c): the shell takes 200 characters a line, and
    cstart would split a --pragma= list at its commas. flag=False leaves
    out --lwasm, so las decides the mode from the source (D40)."""
    a = (["--lwasm"] if flag else []) + [f"-D{d}" for d in j["defines"]]
    if j["pragmas"]:
        a.append("--pragma=" + ",".join(j["pragmas"]))
    return a


def suite_lw(dsk):
    staged = stage_lw()
    os9("makdir", f"{dsk},T/S", check=False)
    done, lines, work = set(), [], []
    for n, (j, files) in enumerate(staged):
        jd = f"T/J{n}"
        os9("makdir", f"{dsk},{jd}", check=False)
        os9("makdir", f"{dsk},{jd}/src", check=False)
        for path, place in files:
            target = place.replace("J/", jd + "/") if place.startswith("J/") else "T/" + place
            if target in done:
                continue
            done.add(target)
            sub = os.path.dirname(target)
            if sub not in (jd, jd + "/src", "T/S"):
                os9("makdir", f"{dsk},{sub}", check=False)
            put(path, f"{dsk},{target}", text=is_text(path))
        src = os.path.basename(j["source"])
        argfile = os.path.join(WORK, "args.tmp")
        with open(argfile, "w") as f:
            # no --lwasm: native las detects the mode (D40); the host
            # reference below keeps the flag, so both must agree
            f.write("\n".join(lw_args(j, flag=False) + ["-i=/dd/T/S", "-o=M"]) + "\n")
        put(argfile, f"{dsk},{jd}/src/A", text=True)
        lines += [f"chd /dd/{jd}/src", f"las {src} --args=A >L >>E"]
        work.append((n, j, files))
    # the shell reads at most 200 characters of a line; the rest becomes
    # another (broken) command
    longest = max((len(l) for l in lines), default=0)
    if longest > 199:
        sys.exit(f"a startup line is {longest} characters (the shell takes 200)")

    def check():
        bad = 0
        hostroot = os.path.join(WORK, "lwhost")
        for n, j, files in work:
            # the same layout on Linux, assembled by the host las
            jd = os.path.join(hostroot, f"J{n}")
            os.makedirs(os.path.join(jd, "src"), exist_ok=True)
            os.makedirs(os.path.join(hostroot, "S"), exist_ok=True)
            for path, place in files:
                t = os.path.join(jd, place[2:]) if place.startswith("J/") \
                    else os.path.join(hostroot, place)
                os.makedirs(os.path.dirname(t), exist_ok=True)
                shutil.copyfile(path, t)
            ref = os.path.join(jd, "ref")
            argv = [REF, os.path.basename(j["source"])] + lw_args(j) + \
                ["-i=" + os.path.join(hostroot, "S"), "-o=" + ref]
            subprocess.run(argv, cwd=os.path.join(jd, "src"), env=lwdiff.clean_env(),
                           capture_output=True)
            out = os.path.join(WORK, "lw", j["id"])
            os.makedirs(os.path.dirname(out), exist_ok=True)
            ok = get(f"{dsk},T/J{n}/src/M", out)
            if ok and os.path.exists(ref) and open(out, "rb").read() == open(ref, "rb").read():
                continue
            bad += 1
            err = (os9_text(f"{dsk},T/J{n}/src/E") or "").strip()
            print(f"  lw {j['id']}: {'differs' if ok else 'no output'} {err[:150]}")
        print(f"lw: {len(work) - bad} of {len(work)} NitrOS9 modules identical to the host las")
        return bad == 0
    return lines, check


IMGDIR = "level1/cmds"


def suite_img(dsk):
    """Phase 9: one directory's commands, without and with a defs image."""
    import json
    staged = [(j, f) for j, f in stage_lw()
              if os.path.dirname(j["source"]).endswith("/" + IMGDIR)]
    # its commonest configuration: one image serves them all
    import collections

    def key(j):
        return json.dumps([j["defines"], j["pragmas"], j["includes"]])
    common = collections.Counter(key(j) for j, _ in staged).most_common(1)[0][0]
    staged = [(j, f) for j, f in staged if key(j) == common]
    placed = {}
    for j, files in staged:
        for path, place in files:
            if placed.setdefault(place, path) != path:
                sys.exit(f"img: {place} would be two files")
    os9("makdir", f"{dsk},T", check=False)
    for d in ("T/S", "T/G", "T/G/src", "T/G/P", "T/G/I"):
        os9("makdir", f"{dsk},{d}", check=False)
    for place, path in placed.items():
        target = "T/G/" + place[2:] if place.startswith("J/") else "T/" + place
        if os.path.dirname(target) not in ("T/S", "T/G", "T/G/src"):
            os9("makdir", f"{dsk},{os.path.dirname(target)}", check=False)
        put(path, f"{dsk},{target}", text=is_text(path))
    argfile = os.path.join(WORK, "args.tmp")
    rounds = ([], [])
    for n, (j, _) in enumerate(staged):
        src = os.path.basename(j["source"])
        for r, (sub, extra) in enumerate((("P", []), ("I", ["--defsimg=/dd/T/G/IMG"]))):
            with open(argfile, "w") as f:
                f.write("\n".join(lw_args(j) + ["-i=/dd/T/S", f"-o=/dd/T/G/{sub}/M{n}"]
                                  + extra) + "\n")
            put(argfile, f"{dsk},T/G/src/{'AB'[r]}{n}", text=True)
            # the shell won't redirect into an existing file (#218)
            rounds[r].append(f"las {src} --args={'AB'[r]}{n} >/dd/T/G/L{sub}{n} >>/dd/T/G/E{sub}{n}")
    lines = ["chd /dd/T/G/src", "date -t >/dd/T/T0"] + rounds[0] + \
            ["date -t >/dd/T/T1"] + rounds[1] + ["date -t >/dd/T/T2"]

    def check():
        import datetime
        hostroot = os.path.join(WORK, "imghost")
        for place, path in placed.items():
            t = os.path.join(hostroot, "G", place[2:]) if place.startswith("J/") \
                else os.path.join(hostroot, place)
            os.makedirs(os.path.dirname(t), exist_ok=True)
            shutil.copyfile(path, t)
        bad = skipped = 0
        for n, (j, _) in enumerate(staged):
            ref = os.path.join(hostroot, f"ref{n}")
            subprocess.run([REF, os.path.basename(j["source"])] + lw_args(j) +
                           ["-i=" + os.path.join(hostroot, "S"), "-o=" + ref],
                           cwd=os.path.join(hostroot, "G", "src"), env=lwdiff.clean_env(),
                           capture_output=True)
            want = open(ref, "rb").read() if os.path.exists(ref) else None
            if want is None:
                skipped += 1        # fails on Linux too (minted: struct)
                continue
            for sub in "PI":
                out = os.path.join(WORK, "img", sub, j["id"])
                os.makedirs(os.path.dirname(out), exist_ok=True)
                ok = get(f"{dsk},T/G/{sub}/M{n}", out)
                if not (ok and want is not None and open(out, "rb").read() == want):
                    bad += 1
                    err = (os9_text(f"{dsk},T/G/E{sub}{n}") or "").strip()
                    print(f"  img {j['id']} ({'with' if sub == 'I' else 'without'} image): "
                          f"{'differs' if ok else 'no output'} {err[:150]}")
        if get(f"{dsk},T/G/IMG", os.path.join(WORK, "img", "IMG")):
            print(f"  image: {os.path.getsize(os.path.join(WORK, 'img', 'IMG'))} bytes")
        times = [(os9_text(f"{dsk},T/T{k}") or "").strip() for k in range(3)]
        print(f"  clock: {times}")
        try:
            ts = [datetime.datetime.strptime(t, "%B %d, %Y  %H:%M:%S") for t in times]
            a, b = (ts[1] - ts[0]).total_seconds(), (ts[2] - ts[1]).total_seconds()
            print(f"  emulated time: {a:.0f} s without the image, {b:.0f} s with it "
                  f"({len(staged)} files)")
        except (ValueError, IndexError):
            pass
        total = 2 * (len(staged) - skipped)
        print(f"img: {total - bad} of {total} modules identical to the host las"
              + (f" ({skipped} sources that fail on Linux too left out)" if skipped else ""))
        return bad == 0
    return lines, check


def link_cmds():
    """The build's lwar and lwlink commands (lwdiff re-reads the recipe's
    dry run), with each object named by its job id."""
    import json
    jobs = json.load(open(JOBS))
    byobj = {os.path.normpath(os.path.join(j["cwd"], j["output"])): j["id"] for j in jobs}
    tmp = os.path.join(WORK, "collect")
    os.makedirs(tmp, exist_ok=True)
    lwdiff.collect(lwdiff.DEFAULT_NITROS9, "l2", "jr2", tmp)
    libs, links = [], []
    for c in lwdiff.LINK_CMDS:
        files = [a for a in c["argv"][1:] if not a.startswith("-")]
        objs = [byobj[os.path.normpath(os.path.join(c["cwd"], f))]
                for f in files if f.endswith(".o")]
        if c["tool"] == "lwar":
            libs.append((os.path.basename(files[0])[3:-2], objs))     # libX.a -> X
        else:
            seq = [("lib", a[2:]) if a.startswith("-l") else ("obj", objs.pop(0))
                   for a in c["argv"][1:]
                   if a.startswith("-l") or (not a.startswith("-") and a.endswith(".o"))]
            name = next(a[2:] for a in c["argv"] if a.startswith("-o"))
            links.append((name, seq))
    return libs, links


def suite_obj(dsk):
    """Phase 10: the object-format sources (lib/, the .as programs)
    assembled natively, the three libraries built from them with merge,
    and the build's 13 programs linked natively with llink --lwlink. The
    reference is the same chain on Linux with the host las and llink."""
    staged = stage_lw("obj")
    libs, links = link_cmds()
    consts = [j["id"] for j, _ in staged if lwdiff.is_constants_object(j)]
    staged.sort(key=lambda jf: jf[0]["id"] not in consts)   # constants first
    for d in ("T/S", "T/O", "T/K"):
        os9("makdir", f"{dsk},{d}", check=False)
    done, lines, work = set(), [], []
    for n, (j, files) in enumerate(staged):
        jd = f"T/J{n}"
        os9("makdir", f"{dsk},{jd}", check=False)
        os9("makdir", f"{dsk},{jd}/src", check=False)
        for path, place in files:
            target = place.replace("J/", jd + "/") if place.startswith("J/") else "T/" + place
            if target in done:
                continue
            done.add(target)
            sub = os.path.dirname(target)
            if sub not in (jd, jd + "/src", "T/S"):
                os9("makdir", f"{dsk},{sub}", check=False)
            put(path, f"{dsk},{target}", text=is_text(path))
        defs = [] if j["id"] in consts else [f"--defs=/dd/T/O/{c}" for c in consts]
        argfile = os.path.join(WORK, "args.tmp")
        with open(argfile, "w") as f:
            f.write("\n".join(lw_args(j) + defs + ["-i=/dd/T/S", f"-o=/dd/T/O/{j['id']}"]) + "\n")
        put(argfile, f"{dsk},{jd}/src/A", text=True)
        lines += [f"chd /dd/{jd}/src", f"las {os.path.basename(j['source'])} --args=A >L >>E"]
        work.append((n, j, files))
    # a library is its members one after another: merge them in groups
    # that fit the shell's line, then merge the groups
    lines.append("chd /dd/T/O")
    for lib, members in libs:
        parts = []
        for i in range(0, len(members), 12):
            parts.append(f"{lib}.{len(parts)}")
            lines.append(f"merge {' '.join(members[i:i + 12])} >{parts[-1]}")
        lines.append(f"merge {' '.join(parts)} >{lib}.l")
    lines.append("chd /dd/T/K")
    for name, seq in links:
        args = " ".join(f"-l=/dd/T/O/{v}.l" if k == "lib" else f"/dd/T/O/{v}" for k, v in seq)
        lines.append(f"llink --lwlink {args} -o=/dd/T/K/{name} -n={name} >>{name}.e")
    longest = max(len(l) for l in lines)
    if longest > 199:
        sys.exit(f"a startup line is {longest} characters (the shell takes 200)")

    def check():
        host = os.path.join(WORK, "objhost")
        hostobj = os.path.join(host, "O")
        os.makedirs(hostobj, exist_ok=True)
        os.makedirs(os.path.join(host, "S"), exist_ok=True)
        bad = 0
        nat = os.path.join(WORK, "obj")
        os.makedirs(nat, exist_ok=True)
        for n, j, files in work:
            jd = os.path.join(host, f"J{n}")
            os.makedirs(os.path.join(jd, "src"), exist_ok=True)
            for path, place in files:
                t = os.path.join(jd, place[2:]) if place.startswith("J/") \
                    else os.path.join(host, place)
                os.makedirs(os.path.dirname(t), exist_ok=True)
                shutil.copyfile(path, t)
            ref = os.path.join(hostobj, j["id"])
            defs = [] if j["id"] in consts else \
                ["--defs=" + os.path.join(hostobj, c) for c in consts]
            subprocess.run([REF, os.path.basename(j["source"])] + lw_args(j) + defs +
                           ["-i=" + os.path.join(host, "S"), "-o=" + ref],
                           cwd=os.path.join(jd, "src"), env=lwdiff.clean_env(),
                           capture_output=True)
            out = os.path.join(nat, j["id"])
            ok = get(f"{dsk},T/O/{j['id']}", out)
            if ok and os.path.exists(ref):
                a, b = open(out, "rb").read(), open(ref, "rb").read()
                if rof_equal(a, b):
                    continue
                if only_dtb(a[12:], b[12:]):
                    print(f"  obj {j['id']}: identical except the assembly date (dtb)")
                    continue
            bad += 1
            err = (os9_text(f"{dsk},T/J{n}/src/E") or "").strip()
            print(f"  obj {j['id']}: {'differs' if ok else 'no output'} {err[:150]}")
        print(f"obj: {len(work) - bad} of {len(work)} objects identical to the host las")
        # libraries: the native merge must equal the native members joined
        badlib = 0
        for lib, members in libs:
            out = os.path.join(nat, lib + ".l")
            ok = get(f"{dsk},T/O/{lib}.l", out)
            want = b"".join(open(os.path.join(nat, m), "rb").read()
                            for m in members if os.path.exists(os.path.join(nat, m)))
            with open(os.path.join(hostobj, lib + ".l"), "wb") as f:
                for m in members:
                    if os.path.exists(os.path.join(hostobj, m)):
                        f.write(open(os.path.join(hostobj, m), "rb").read())
            if not (ok and open(out, "rb").read() == want):
                badlib += 1
                print(f"  lib {lib}: {'differs from its members' if ok else 'no output'}")
        print(f"lib: {len(libs) - badlib} of {len(libs)} libraries built")
        badlink = 0
        hostk = os.path.join(host, "K")
        os.makedirs(hostk, exist_ok=True)
        for name, seq in links:
            ref = os.path.join(hostk, name)
            if os.path.exists(ref):
                os.remove(ref)
            subprocess.run([lwdiff.DEFAULT_LLINK, "--lwlink"] +
                           [("-l=" + os.path.join(hostobj, v + ".l")) if k == "lib"
                            else os.path.join(hostobj, v) for k, v in seq] +
                           ["-o=" + name, "-n=" + name],
                           cwd=hostk, env=lwdiff.clean_env(), capture_output=True)
            out = os.path.join(nat, "K." + name)
            ok = get(f"{dsk},T/K/{name}", out)
            if ok and os.path.exists(ref):
                a, b = open(out, "rb").read(), open(ref, "rb").read()
                if a == b:
                    continue
                if only_dtb(a, b, crc=3):
                    print(f"  link {name}: identical except the assembly date (dtb) and CRC")
                    continue
            badlink += 1
            err = (os9_text(f"{dsk},T/K/{name}.e") or "").strip()
            print(f"  link {name}: {'differs' if ok else 'no output'} {err[:150]}")
        print(f"link: {len(links) - badlink} of {len(links)} programs identical to host llink")
        return bad == 0 and badlib == 0 and badlink == 0
    return lines, check


def native_var(name):
    """A variable of native/GNUmakefile, as a list of words."""
    p = subprocess.run(["make", "-s", "-f", "GNUmakefile", "--eval",
                        f"pv:\n\t@echo $({name})", "pv"], cwd=NATIVE,
                       capture_output=True, text=True)
    return p.stdout.split()


def host_ref(tool, o, defs, cc, srcdir=None):
    """One C file of a devsys tool through the host passes as native dcc
    runs them: the .r, in WORK/ref/TOOL. srcdir: the sources (default
    DEVSYS/TOOL). (Until D51 the F256's dco68 held 300 instructions and
    the host's the whole file, so this ran dco68 -i300.)"""
    d = os.path.join(WORK, "ref", tool)
    os.makedirs(d, exist_ok=True)
    src = os.path.join(srcdir or os.path.join(DEVSYS, tool), o + ".c")
    env = lwdiff.clean_env()
    dcc = os.path.join(DEVSYS, "dcc")
    pre = subprocess.run([os.path.join(dcc, "CPP/dcpp")] + defs +
                         ["-I=" + os.path.dirname(src), "-I=" + os.path.join(COCOC, "Defs"), src],
                         cwd=d, env=env, capture_output=True).stdout
    open(os.path.join(d, o + ".i"), "wb").write(pre)
    subprocess.run([os.path.join(dcc, "CC09/dcc68")] + cc + [o + ".i", f"-o={o}.raw.a"],
                   cwd=d, env=env, capture_output=True)
    with open(os.path.join(d, o + ".raw.a"), "rb") as fin, \
            open(os.path.join(d, o + ".a"), "wb") as fout:
        subprocess.run([os.path.join(dcc, "COpt/dco68"), "-2"], stdin=fin,
                       stdout=fout, env=env)
    subprocess.run([os.path.join(DEVSYS, "las/build/unix/las"), f"-o={o}.r", o + ".a"],
                   cwd=d, env=env, capture_output=True)
    return os.path.join(d, o + ".r")


def self_flags(tool, o):
    """A source's defines and dcc68 flags, as native/GNUmakefile has them."""
    defs = native_var(f"{tool}_DEFS")
    cc = native_var(f"{tool}_CC")
    if o in native_var(f"{tool}_NOCHECK") and "-s" not in cc:
        cc = cc + ["-s"]
    return defs, cc


def host_link(tool, rels, out):
    """The host llink's module from the given objects, linked as
    native/GNUmakefile links TOOL."""
    mem = native_var(f"{tool}_MEM")
    lib = CLIB
    p = subprocess.run([os.path.join(DEVSYS, "llink/llink"), f"-o={out}", f"-n={tool}"]
                       + ([f"-M={mem[0]}"] if mem else [])
                       + [os.path.join(lib, "cstart.r")] + rels
                       + [f"-l={lib}/sys.l", f"-l={lib}/clib.l"],
                       env=lwdiff.clean_env(), capture_output=True)
    return p.returncode == 0 and os.path.exists(out)


def suite_self(dsk):
    """Step 17, self-hosting: every devsys tool (--only TOOL for some) built
    on the F256 by make with the native tools: dcc compiles each source,
    the native llink links the module. (Until step 20 las, llink and dcc
    needed header images, D50: dcc68's symbols are banked now.) Each object must equal the host passes'
    (host_ref), each module the host llink's link of those objects, and
    the cross-built one (native/bin): the F256 builds its own tools
    byte for byte (step 17's goal; D51 made dco68 hold the whole file
    natively, as on Linux)."""
    tools = [t for t in TOOLS if not ONLY or t in ONLY]
    os9("makdir", f"{dsk},T/S", check=False)
    lines = []
    for tool in tools:
        src = native_var(f"{tool}_DIR")[0]
        objs = native_var(f"{tool}_OBJS")
        mem = native_var(f"{tool}_MEM")
        d = f"T/S/{tool}"
        os9("makdir", f"{dsk},{d}", check=False)
        for f in os.listdir(src):
            if f.endswith((".c", ".h")):
                put(os.path.join(src, f), f"{dsk},{d}/{f}", text=True)
        mk = [f"* native.py's self suite: {tool} built on the F256", ""]
        mk += [f"{tool}.mod: {' '.join(o + '.r' for o in objs)}",
               f"    llink /dd/LIB/cstart.r -z=objs -l=/dd/LIB/sys.l -l=/dd/LIB/clib.l "
               f"-o=/dd/{d}/{tool}.mod -n={tool}" + (f" -M={mem[0]}" if mem else "")
               + f" -m >{tool}.map", ""]
        for o in objs:
            defs, cc = self_flags(tool, o)
            mk += [f"{o}.r: {o}.c",
                   f"    dcc -2 {' '.join('-d' + x[2:] for x in defs)} {' '.join(cc)} "
                   f"-r {o}.c", ""]
        mkf = os.path.join(WORK, "self.makefile")
        open(mkf, "w").write("\n".join(mk))
        put(mkf, f"{dsk},{d}/makefile", text=True)
        lst = os.path.join(WORK, "self.objs")
        open(lst, "w").write("".join(o + ".r\n" for o in objs))
        put(lst, f"{dsk},{d}/objs", text=True)
        lines += [f"chd /dd/{d}", "date -t >t0", "make >make.out >>make.err", "date -t >t1"]

    def check():
        import datetime
        ok_all = True
        for tool in tools:
            d = f"T/S/{tool}"
            src = native_var(f"{tool}_DIR")[0]
            objs = native_var(f"{tool}_OBJS")
            err = (os9_text(f"{dsk},{d}/make.err") or "").strip()
            good, refs = 0, []
            for o in objs:
                out = os.path.join(WORK, "self", tool, o + ".r")
                os.makedirs(os.path.dirname(out), exist_ok=True)
                ok = get(f"{dsk},{d}/{o}.r", out)
                defs, cc = self_flags(tool, o)
                ref = host_ref("self/" + tool, o, defs, cc, src)
                refs.append(ref)
                same = ok and rof_equal(open(out, "rb").read(), open(ref, "rb").read())
                if not same:
                    print(f"  {tool} {o}: {'DIFFERS' if ok else 'NO OUTPUT'}")
                good += same
            mod = os.path.join(WORK, "self", tool, tool + ".mod")
            got = get(f"{dsk},{d}/{tool}.mod", mod)
            ref = os.path.join(WORK, "ref", "self", tool, tool + ".mod")
            linked = host_link(tool, refs, ref)
            same = got and linked and open(mod, "rb").read() == open(ref, "rb").read()
            cross = got and open(mod, "rb").read() == open(os.path.join(NATIVE, "bin", tool), "rb").read()
            t = None
            try:
                ts = [datetime.datetime.strptime((os9_text(f"{dsk},{d}/t{i}") or "").strip(),
                                                 "%B %d, %Y  %H:%M:%S") for i in (0, 1)]
                t = (ts[1] - ts[0]).total_seconds()
            except ValueError:
                pass
            print(f"self {tool}: {good} of {len(objs)} objects identical; module "
                  + (f"{os.path.getsize(mod)} bytes, " if got else "")
                  + ("identical to the host link" if same else "DIFFERS from the host link"
                     if got else "NOT BUILT")
                  + (f"; {'equal to' if cross else 'not equal to'} native/bin" if got else "")
                  + (f"; {t:.0f} s emulated" if t is not None else ""))
            if good != len(objs) or not same or not cross:
                print("  --- make.err\n  " + err[-2500:].replace("\n", "\n  "))
                ok_all = False
        return ok_all
    return lines, check


# Step 23: jfed's own F256 programs (~/projects/f256/jfed, outside devsys;
# a program whose source is missing is skipped). C: name, source, dcc
# options; built by native dcc, compared with the host passes.
JFED = os.path.expanduser("~/projects/f256/jfed")
PROG_C = [("te", "te/te/te.c", "-m=2k"),
          ("dev", "dev-cc/dev.c", "-m=2k"),
          ("tepipes", "te-pipes/te.c", "-m=2k"),
          ("forktest", "te-pipes/forktest.c", ""),
          ("as09", "as09/as09_main.c", ""),
          ("optkeyc", "OptKeyTest/optkeytest.c", ""),
          ("keytest", "textedit/keytest.c", ""),
          ("tedm", "textedit/texteditMv2046.c", "-m=2k")]
# Assembly: name, source, defs (a NitrOS9 tree, or None: the source's own
# directory has them); built by native las, compared with the host las
# (and lwasm, by lwdiff's rules, on the host)
N9MAIN = os.path.expanduser("~/projects/wild/main/nitros9")
N9TEXT = os.path.expanduser("~/projects/wild/newtextclutaddr/nitros9")   # SS.MTXTBLK
PROG_ASM = [("testp1", "PipeExample/testp1.asm", N9MAIN),
            ("testp2", "PipeExample/testp2.asm", N9MAIN),
            ("memtext", "memtext/memtexttest.asm", N9TEXT),
            ("optkeya", "OptKeyTest/optkeytest.asm", None)]
PROG_PRAGMAS = "pcaspcr,nosymbolcase,condundefzero,undefextern,dollarnotlocal,noforwardrefmax"


def host_prog_c(name, src, opts):
    """A C program through the host passes as dcc runs them, from a copy
    named NAME.c (dcc68 names the psect after the file)."""
    d = os.path.join(WORK, "progs-host")
    os.makedirs(d, exist_ok=True)
    shutil.copy(src, os.path.join(d, name + ".c"))
    env = lwdiff.clean_env()
    dcc = os.path.join(DEVSYS, "dcc")

    def run(argv, **kw):
        p = subprocess.run(argv, cwd=d, env=env, capture_output=True, **kw)
        if p.returncode:
            sys.exit(f"host {argv[0]} ({name}): {p.stderr.decode(errors='replace')[-500:]}")
        return p
    pre = run([os.path.join(dcc, "CPP/dcpp"), "-I=" + os.path.dirname(src),
               "-I=" + os.path.join(COCOC, "Defs"), name + ".c"])
    open(os.path.join(d, name + ".i"), "wb").write(pre.stdout)
    run([os.path.join(dcc, "CC09/dcc68"), name + ".i", f"-o={name}.raw.a"])
    with open(os.path.join(d, name + ".raw.a"), "rb") as fin:
        opt = run([os.path.join(dcc, "COpt/dco68")], stdin=fin)
    open(os.path.join(d, name + ".a"), "wb").write(opt.stdout)
    run([REF, f"-o={name}.r", name + ".a"])
    mem = [o.replace("-m=", "-M=") for o in opts.split() if o.startswith("-m=")]
    run([os.path.join(DEVSYS, "llink/llink"), f"-o={name}"] + mem +
        [os.path.join(CLIB, "cstart.r"), name + ".r", "-l=" + os.path.join(CLIB, "clib.l")])
    return os.path.join(d, name)


def prog_asm_args(defs, srcdir, root):
    """las options for one assembly program: the include path is the
    source's directory, then the tree's defs and level2/wildbits (where
    defsfile is), each under root (a host or an OS-9 path)."""
    inc = ["-I=" + root("src", srcdir)]
    if defs:
        inc += ["-I=" + root("defs", os.path.join(defs, "defs")),
                "-I=" + root("wb", os.path.join(defs, "level2/wildbits"))]
    return ["--lwasm", "--pragma=" + PROG_PRAGMAS] + inc + ["-Dwildbits=1"]


def suite_progs(dsk):
    """Step 23: jfed's programs built on the F256 (dcc for C, las for
    assembly), each compared with the same build by the host tools.
    Times come from date -t around each build."""
    os9("makdir", f"{dsk},T/P", check=False)
    lines, cprogs, aprogs = ["chd /dd/T/P"], [], []

    def stamp(name, i):
        return f"date -t >{name}.t{i}"
    for name, rel, opts in PROG_C:
        src = os.path.join(JFED, rel)
        if not os.path.exists(src) or (ONLY and not any(o in name for o in ONLY)):
            continue
        put(src, f"{dsk},T/P/{name}.c", text=True)
        lines += [stamp(name, 0), f"dcc {name}.c {opts} -f=/dd/T/P/{name} >{name}.out >>{name}.err",
                  stamp(name, 1)]
        cprogs.append((name, src, opts))
    staged, dirmap = set(), {}     # each directory once, as T/P/D/<n>
    for name, rel, defs in PROG_ASM:
        src = os.path.join(JFED, rel)
        if not os.path.exists(src) or (ONLY and not any(o in name for o in ONLY)):
            continue
        srcdir = os.path.dirname(src)

        def native_root(kind, path):
            return dirmap.setdefault(path, f"/dd/T/P/D/{len(dirmap)}")
        args = prog_asm_args(defs, srcdir, native_root)
        for path, where in dirmap.items():
            if path in staged:
                continue
            os9("makdir", f"{dsk},T/P/D", check=False)
            os9("makdir", f"{dsk},{where[4:]}", check=False)
            names = [f for f in os.listdir(path) if f.endswith(".d") or f == "defsfile"]
            for f in names:
                put(os.path.join(path, f), f"{dsk},{where[4:]}/{f}", text=True)
            staged.add(path)
        put(src, f"{dsk},T/P/{name}.asm", text=True)
        opt = os.path.join(WORK, name + ".opt")
        open(opt, "w").write("\n".join(args + [f"-o=/dd/T/P/{name}"]) + "\n")
        put(opt, f"{dsk},T/P/{name}.opt", text=True)
        lines += [stamp(name, 0), f"las {name}.asm --args={name}.opt >{name}.out >>{name}.err",
                  stamp(name, 1)]
        aprogs.append((name, src, defs))

    def elapsed(name):
        import datetime
        try:
            ts = [datetime.datetime.strptime((os9_text(f"{dsk},T/P/{name}.t{i}") or "").strip(),
                                             "%B %d, %Y  %H:%M:%S") for i in (0, 1)]
            return f"{(ts[1] - ts[0]).total_seconds():.0f} s"
        except ValueError:
            return "? s"

    def check():
        good = 0
        for name, src, opts in cprogs:
            ref = host_prog_c(name, src, opts)
            out = os.path.join(WORK, "progs", name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            ok = get(f"{dsk},T/P/{name}", out)
            same = ok and open(out, "rb").read() == open(ref, "rb").read()
            print(f"  {name}: {os.path.getsize(out) if ok else 0} bytes, "
                  f"{'identical to the host passes' if same else 'DIFFERS' if ok else 'NOT BUILT'}, "
                  f"{elapsed(name)}")
            if not same:
                print("    " + (os9_text(f"{dsk},T/P/{name}.err") or "").strip()[-600:]
                      .replace("\n", "\n    "))
            good += same
        for name, src, defs in aprogs:
            d = os.path.join(WORK, "progs-host")
            os.makedirs(d, exist_ok=True)
            args = prog_asm_args(defs, os.path.dirname(src), lambda kind, path: path)
            ref = os.path.join(d, name)
            p = subprocess.run([REF] + args + [f"-o={ref}", src], cwd=d, env=lwdiff.clean_env(),
                               capture_output=True)
            out = os.path.join(WORK, "progs", name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            ok = get(f"{dsk},T/P/{name}", out)
            same = ok and p.returncode == 0 and open(out, "rb").read() == open(ref, "rb").read()
            print(f"  {name}: {os.path.getsize(out) if ok else 0} bytes, "
                  f"{'identical to the host las' if same else 'DIFFERS' if ok else 'NOT BUILT'}, "
                  f"{elapsed(name)}")
            if not same:
                print("    " + (os9_text(f"{dsk},T/P/{name}.out") or "").strip()[-600:]
                      .replace("\n", "\n    "))
            good += same
        n = len(cprogs) + len(aprogs)
        print(f"progs: {good} of {n} programs built on the F256 identical to the host tools")
        return good == n
    return lines, check


SUITES = {"las": suite_las, "link": suite_link, "cc": suite_cc, "make": suite_make,
          "progs": suite_progs,
          "self": suite_self, "lw": suite_lw,
          "obj": suite_obj, "img": suite_img}
DEFAULT = ["las", "link", "cc", "make"]


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
    ap.add_argument("--seconds", type=int, default=7200,
                    help="emulated seconds before giving up")
    ap.add_argument("--keep", action="store_true", help="keep the test image")
    ap.add_argument("--only", action="append", default=[],
                    help="lw suite: only jobs whose id contains this (repeatable)")
    ap.add_argument("--shard", default="0/1", help="lw suite: K/N, every Nth job from K")
    ap.add_argument("--work", help="work directory (default harness/out/native)")
    ap.add_argument("--no-build", action="store_true",
                    help="don't run make in native/ (parallel shards)")
    args = ap.parse_args()
    ONLY[:] = args.only
    k, n = (int(x) for x in args.shard.split("/"))
    if not 0 <= k < n:
        sys.exit("--shard K/N needs 0 <= K < N")
    SHARD[:] = [k, n]
    if args.work:
        global WORK
        WORK = os.path.abspath(args.work)
    suites = args.suites or DEFAULT
    for s in suites:
        if s not in SUITES:
            sys.exit(f"unknown suite {s}: choose from {', '.join(SUITES)}")

    if not args.no_build and \
            subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=NATIVE).returncode:
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
