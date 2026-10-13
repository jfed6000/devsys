#!/usr/bin/env python3
"""card - the devsys install disk for a real F256 (SD card), and its check.

  card.py build [--image FILE] [--out FILE] [--platform k2|jr2]
      A copy of the boot image (default boot/wildbits/l2/l2_wildbits_devsysk2.dsk,
      D44) with devsys installed (D47):
        /dd/CMDS        las, llink, dcc, dcpp, dcc68, dcc68w, dco68, make
        /dd/LIB         devsys's C libraries (clib/, step 18); NitrOS9
                        main's alib.l, net.l and wildbitsl2.l (assembled
                        by the host las)
        /dd/DEFS        CoCoC's C headers, devsys's os9defs.a
        /dd/DEFS/NITROS9  NitrOS9 main's defs/ and its recipe's defsfile
        /dd/SYS         make.default; dcc68.msg (dcc68's messages);
                        n9mod.opt and n9obj.opt (las options for NitrOS9
                        sources); help entries in helpmsg
        /dd/DEVSYS      README, EX (examples), G (selftest's sources), selftest
      Write it to the card with dd (check the device with lsblk first).
      On the F256 type:   /dd/DEVSYS/selftest
      It writes everything to /dd/DEVSYS/R (run it once per card write:
      OS-9's shell won't redirect into a file that exists).
  card.py check FILE [--platform k2|jr2]
      FILE: the card read back (dd with an explicit byte count). Compares
      what the F256 made with the Linux tools and prints its timings.
  card.py mame [--image FILE]
      The same disk for the Jr2 (MAME has no K2), run in the Wildbits MAME
      with selftest as the startup, then checked: proves the card before
      it goes on the machine.

What selftest does, from the installed files only, timed with date -t
between steps (mfree before and after, so check can report free RAM and
blocks not given back; mmap last):
  1. dcc builds hello.c and runs it; dcc -w checks it
  2. las assembles 8 commands of NitrOS9 main's level1/cmds without a defs
     image, then with n9mod.opt (records /dd/SYS/n9mod.img, then loads it)
  3. las assembles EX/hello.asm with n9mod.opt (struct, mode detection),
     twice to the same file, and runs it
  4. las assembles main's grep.as with n9obj.opt, llink links it with
     main's libraries, and it runs
  5. make builds EX/M (C and RMA-style assembly) and the program runs
"""
import argparse
import collections
import datetime
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402
import native  # noqa: E402

DEVSYS = native.DEVSYS
L2 = os.path.join(DEVSYS, "boot", "wildbits", "l2")     # D44: make PLATFORM=k2 / jr2 there
K2 = os.path.join(L2, "l2_wildbits_devsysk2.dsk")
JR2 = os.path.join(L2, "l2_wildbits_devsysjr2.dsk")
N9 = os.environ.get("N9", os.path.join(native.HOME, "projects/wild/main/nitros9"))  # as L2's makefile
INSTALL = os.path.join(DEVSYS, "install")
WORK = os.path.join(HERE, "out", "card")
SRCDIR = "level1/cmds"
CMDS = ["echo", "date", "attr", "copy", "dir", "list", "free", "dump"]
D = "DEVSYS"
NDEFS = "DEFS/NITROS9"
OBJPROG = "grep"
HEY = "hello from las on the F256"
MKDEMO = "mkdemo: 5 42"


# ------------------------------------------------------------ NitrOS9 main

def n9(platform):
    """NitrOS9 main's l2 build for platform, read from its recipe (lwdiff's
    dry run): its jobs, the libraries assembled by the host las and joined
    as lwar would, and the options its modules and objects are built with."""
    work = os.path.join(WORK, f"n9-{platform}")
    os.makedirs(work, exist_ok=True)
    jobs = lwdiff.collect(N9, "l2", platform, work)
    gen = lwdiff.make_generated(work)
    byout = {os.path.normpath(os.path.join(j["cwd"], j["output"])): j for j in jobs}
    libs, links = [], {}
    for c in lwdiff.LINK_CMDS:
        files = [a for a in c["argv"][1:] if not a.startswith("-")]
        if c["tool"] == "lwar":
            libs.append((os.path.basename(files[0])[3:-2],         # libX.a -> X
                         [byout[os.path.normpath(os.path.join(c["cwd"], f))] for f in files[1:]]))
        else:
            name = next(a[2:] for a in c["argv"] if a.startswith("-o"))
            links[name] = c["argv"]
    consts = [n for n, m in libs if all(lwdiff.is_constants_object(j) for j in m)]
    if len(consts) != 1:
        sys.exit(f"card: wanted one constants library, found {consts}")
    libs.sort(key=lambda nm: nm[0] not in consts)            # constants first
    lasdir, libdir = os.path.join(work, "las"), os.path.join(work, "lib")
    os.makedirs(lasdir, exist_ok=True)
    os.makedirs(libdir, exist_ok=True)
    cdefs, built = [], []
    for name, members in libs:
        for j in members:
            r = lwdiff.run_las(j, native.REF, lasdir, gen, cdefs)
            if r["rc"] != 0:
                sys.exit(f"card: las failed on {j['source']}: {r['stderr'][:300]}")
        path = os.path.join(libdir, name + ".l")
        with open(path, "wb") as f:
            for j in members:
                f.write(open(os.path.join(lasdir, j["id"]), "rb").read())
        built.append((name, path))
        if name in consts:
            cdefs = [path]

    def config(fmt):
        c = collections.Counter(json.dumps([j["defines"], j["pragmas"]])
                                for j in jobs if j["format"] == fmt)
        defines, pragmas = json.loads(c.most_common(1)[0][0])
        return [f"-D{d}" for d in defines] + [f"--pragma={','.join(pragmas)}"]
    return {"jobs": jobs, "gen": gen, "libs": built, "members": libs, "consts": consts[0],
            "links": links,
            "mod": config("os9"), "obj": config("obj"), "work": work}


def opts(n, kind, defs, libdir, sysdir):
    """An options file's lines (las --args=): NitrOS9 main's settings for a
    module (kind "mod") or object ("obj") source, the defs directory, the
    constants library for objects, and the defs image. No --lwasm: las
    picks the mode from the source (D40)."""
    a = n[kind] + [f"-i={defs}"]
    if kind == "obj":
        a.append(f"--defs={libdir}/{n['consts']}.l")
    return a + [f"--defsimg={sysdir}/n9{kind}.img"]


def defs_files():
    """main's defs/ (the files git knows) and the l2 recipe's defsfile."""
    p = subprocess.run(["git", "-C", N9, "ls-files", "defs"], capture_output=True, text=True,
                       check=True)
    files = [os.path.join(N9, f) for f in p.stdout.split() if "/" not in f[5:]]
    return files + [os.path.join(N9, "recipes/wildbits/l2/defsfile")]


def cmd_jobs(n):
    """The 8 commands selftest assembles, from main's level1/cmds."""
    want = {j["id"]: j for j in n["jobs"] if j["format"] == "os9"
            and os.path.dirname(j["source"]).endswith("/" + SRCDIR) and j["id"] in CMDS}
    if sorted(want) != sorted(CMDS):
        sys.exit(f"card: wanted {CMDS} in {SRCDIR}, found {sorted(want)}")
    return [want[c] for c in CMDS]


def obj_job(n):
    return next(j for j in n["jobs"] if j["id"] == OBJPROG + ".o")


def helpmsg(old):
    """The image's helpmsg with devsys's entries (install/help/*.hp) added,
    replacing any of the same name."""
    ours = {}
    for f in sorted(os.listdir(os.path.join(INSTALL, "help"))):
        text = open(os.path.join(INSTALL, "help", f)).read()
        ours[text.split("\n", 1)[0].strip().upper()] = text.rstrip("\n") + "\n"
    out, keep = [], True
    for line in old.replace("\r", "\n").splitlines(keepends=True):
        if line.startswith("@"):
            keep = line.strip().upper() not in ours
        if keep:
            out.append(line)
    return "".join(out).rstrip("\n") + "\n" + "".join(ours[k] for k in sorted(ours))


# ------------------------------------------------------------ the disk

def host_layout(n, root):
    """The install's files as a Linux tree (root/DEFS/NITROS9, root/LIB,
    root/DEVSYS/...), for the host reference runs."""
    shutil.rmtree(root, ignore_errors=True)
    for d in (NDEFS, "LIB", f"{D}/G", f"{D}/EX"):
        os.makedirs(os.path.join(root, d))
    for f in defs_files():
        shutil.copyfile(f, os.path.join(root, NDEFS, os.path.basename(f)))
    for name, path in n["libs"]:
        shutil.copyfile(path, os.path.join(root, "LIB", name + ".l"))
    for j in cmd_jobs(n):
        shutil.copyfile(j["source"], os.path.join(root, D, "G", os.path.basename(j["source"])))
    shutil.copyfile(os.path.join(INSTALL, "examples", "hello.asm"),
                    os.path.join(root, D, "EX", "hello.asm"))
    shutil.copyfile(obj_job(n)["source"], os.path.join(root, D, "EX", OBJPROG + ".as"))


def build(image, out, platform):
    native.WORK = WORK
    os.makedirs(WORK, exist_ok=True)
    n = n9(platform)
    native.make_image(image, out, dirs=("LIB", "TMP"))
    os9, put = native.os9, native.put
    # NitrOS9 main: defs, libraries, options files
    os9("makdir", f"{out},{NDEFS}", check=False)
    for f in defs_files():
        put(f, f"{out},{NDEFS}/{os.path.basename(f)}", text=True)
    for name, path in n["libs"]:
        put(path, f"{out},LIB/{name}.l")
    for kind in ("mod", "obj"):
        p = os.path.join(WORK, f"n9{kind}.opt")
        with open(p, "w") as f:
            f.write("\n".join(opts(n, kind, f"/dd/{NDEFS}", "/dd/LIB", "/dd/SYS")) + "\n")
        put(p, f"{out},SYS/n9{kind}.opt", text=True)
    # help: devsys's entries in /dd/SYS/helpmsg
    hp = os.path.join(WORK, "helpmsg")
    with open(hp, "w") as f:
        f.write(helpmsg(native.os9_text(f"{out},SYS/helpmsg") or ""))
    os9("del", f"{out},SYS/helpmsg", check=False)
    put(hp, f"{out},SYS/helpmsg", text=True)

    # /dd/DEVSYS: README, examples, selftest's sources
    for d in (D, f"{D}/EX", f"{D}/EX/M", f"{D}/G", f"{D}/R"):
        os9("makdir", f"{out},{d}", check=False)
    put(os.path.join(INSTALL, "README"), f"{out},{D}/README", text=True)
    put(os.path.join(INSTALL, "examples", "hello.asm"), f"{out},{D}/EX/hello.asm", text=True)
    put(native.HELLO, f"{out},{D}/EX/hello.c", text=True)
    put(obj_job(n)["source"], f"{out},{D}/EX/{OBJPROG}.as", text=True)
    for f in ("makefile", "main.c"):
        put(os.path.join(INSTALL, "examples", "make", f), f"{out},{D}/EX/M/{f}", text=True)
    for f in ("util.c", "dbl.a"):
        put(os.path.join(native.MAKEDIR, f), f"{out},{D}/EX/M/{f}", text=True)
    cmds = cmd_jobs(n)
    for j in cmds:
        put(j["source"], f"{out},{D}/G/{os.path.basename(j['source'])}", text=True)

    r, ex = f"/dd/{D}/R", f"/dd/{D}/EX"
    mod, obj = "--args=/dd/SYS/n9mod.opt", "--args=/dd/SYS/n9obj.opt"
    libs = " ".join(f"-l=/dd/LIB/{name}.l" for name in lib_order(n))
    lines = ["-x", "* devsys selftest: results in /dd/DEVSYS/R", f"mfree >{r}/M0",
             f"date -t >{r}/T0",
             # 1. C
             f"chd {ex}", f"dcc hello.c -f={r}/hello >{r}/CO >>{r}/CE",
             f"{r}/hello >{r}/HO >>{r}/HE", f"dcc hello.c -w >{r}/WO >>{r}/WE",
             f"date -t >{r}/T1", f"chd /dd/{D}/G"]
    # 2. NitrOS9 commands, without an image (an options file without
    # --defsimg), then with n9mod.opt
    noimg = os.path.join(WORK, "P")
    with open(noimg, "w") as f:
        f.write("\n".join(opts(n, "mod", f"/dd/{NDEFS}", "/dd/LIB", "/dd/SYS")[:-1]) + "\n")
    put(noimg, f"{out},{D}/G/P", text=True)
    for k, j in enumerate(cmds):
        src = os.path.basename(j["source"])
        lines.append(f"las {src} --args=P -o={r}/P{k} >{r}/LP{k} >>{r}/EP{k}")
    lines.append(f"date -t >{r}/T2")
    for k, j in enumerate(cmds):
        src = os.path.basename(j["source"])
        lines.append(f"las {src} {mod} -o={r}/I{k} >{r}/LI{k} >>{r}/EI{k}")
    lines.append(f"date -t >{r}/T3")
    # 3. a module of our own, assembled twice to the same file (las must
    # replace it), then run
    lines += [f"chd {ex}",
              f"las hello.asm {mod} -o={r}/hey >{r}/AL >>{r}/AE",
              f"las hello.asm {mod} -o={r}/hey >{r}/AL2 >>{r}/AE2",
              f"attr {r}/hey e pe", f"{r}/hey >{r}/AO >>{r}/AOE",
              # 4. an object program with main's libraries
              f"las {OBJPROG}.as {obj} -o={r}/{OBJPROG}.o >{r}/GL >>{r}/GE",
              f"llink {r}/{OBJPROG}.o {libs} -o={r}/{OBJPROG} >{r}/KL >>{r}/KE",
              f'{r}/{OBJPROG} -n "selftest" /dd/{D}/README >{r}/GO >>{r}/GOE',
              f"date -t >{r}/T4",
              # 5. make
              f"chd {ex}/M", f"make >{r}/MK >>{r}/MKE", f"mkdemo >{r}/MO >>{r}/MOE",
              f"date -t >{r}/T5", f"help las >{r}/HL"]
    # free RAM after: less than before means a tool kept blocks (F$DelRAM)
    lines += [f"mfree >{r}/M1", f"mmap >{r}/MM", f"echo done >{r}/DONE"]
    if max(len(l) for l in lines) > 199:
        sys.exit("card: a selftest line is longer than the shell's 200 characters")
    st = os.path.join(WORK, "selftest")
    with open(st, "w") as f:
        f.write("\n".join(lines) + "\n")
    put(st, f"{out},{D}/selftest", text=True)
    os9("attr", "-q", "-e", "-pe", "-r", "-pr", f"{out},{D}/selftest")
    print(f"card: {out} ({os.path.getsize(out)} bytes, {platform}), "
          f"selftest of {len(lines)} lines")
    return n


def lib_order(n):
    """The libraries in the order main's links name them (-lX)."""
    argv = n["links"][OBJPROG]
    return [a[2:] for a in argv if a.startswith("-l")]


# ------------------------------------------------------------ check

def check(dsk, platform, n=None):
    """Compare what selftest left on the image with the Linux tools."""
    native.WORK = WORK
    os.makedirs(WORK, exist_ok=True)
    n = n or n9(platform)
    get, text = native.get, native.os9_text
    r = f"{dsk},{D}/R"
    res = os.path.join(WORK, "res")
    shutil.rmtree(res, ignore_errors=True)
    os.makedirs(res)
    bad = 0

    def fetch(name):
        p = os.path.join(res, name)
        return open(p, "rb").read() if get(f"{r}/{name}", p) else None

    def err(*names):
        return " ".join((text(f"{r}/{x}") or "").strip() for x in names).strip()[:300]

    if fetch("DONE") is None:
        print("card: selftest didn't finish (no DONE)")
        bad += 1

    # 1. C
    ref = native.host_cc("hello", native.HELLO)
    same = fetch("hello") == open(ref, "rb").read()
    printed = (text(f"{r}/HO") or "").strip()
    ok = same and printed == "hello from dcc on the F256: 45"
    bad += not ok
    print(f"  C: hello {'identical to the host passes' if same else 'DIFFERS or missing'};"
          f" printed {printed!r}")
    if not ok:
        print("    " + err("CE", "HE"))
    werr = (text(f"{r}/WE") or "").strip()
    wlast = werr.splitlines()[-1] if werr else ""
    wok = wlast.strip().startswith("dcc68:") and "-o=/nil" in wlast
    bad += not wok
    print(f"  C: dcc -w {'checks through /nil' if wok else 'FAILS: ' + werr[:200]}")

    # the host las on the same files and options (Linux paths)
    host = os.path.join(WORK, "host")
    host_layout(n, host)
    hdefs, hlib = os.path.join(host, NDEFS), os.path.join(host, "LIB")

    def host_las(src, kind, outname, cwd):
        o = os.path.join(host, outname)
        a = opts(n, kind, hdefs, hlib, host)[:-1]        # no image: it only changes speed
        subprocess.run([native.REF, src] + a + ["-o=" + o], cwd=cwd,
                       env=lwdiff.clean_env(), capture_output=True)
        return open(o, "rb").read() if os.path.exists(o) else b"(host las failed)"

    # 2. NitrOS9 commands: the host las, and lwasm as main's build runs it
    lwref = os.path.join(n["work"], "lwasm")
    os.makedirs(lwref, exist_ok=True)
    for k, j in enumerate(cmd_jobs(n)):
        want = host_las(os.path.basename(j["source"]), "mod", f"cmd{k}", os.path.join(host, D, "G"))
        lw_ok, lw_out, _ = lwdiff.run_reference(j, lwdiff.DEFAULT_LWASM, lwref, n["gen"])
        build_same = lw_ok and open(lw_out, "rb").read() == want
        cols = []
        for sub in "PI":
            got = fetch(f"{sub}{k}")
            ok = got == want
            bad += not ok
            cols.append("ok" if ok else "DIFFERS: " + err(f"E{sub}{k}")[:120])
        bad += not build_same
        print(f"  {j['id']:6} without image: {cols[0]:4}  with n9mod.opt: {cols[1]:4}"
              f"  host = main's lwasm build: {'yes' if build_same else 'NO'}")
    img = os.path.join(res, "n9mod.img")
    if get(f"{dsk},SYS/n9mod.img", img):
        print(f"  defs image /dd/SYS/n9mod.img: {os.path.getsize(img)} bytes")
    else:
        print("  no defs image was written")
        bad += 1

    # 3. hello.asm
    exdir = os.path.join(host, D, "EX")
    want = host_las("hello.asm", "mod", "hey", exdir)
    got = fetch("hey")
    said = (text(f"{r}/AO") or "").strip()
    ok = got == want and said == HEY and not err("AE", "AE2")
    bad += not ok
    print(f"  hello.asm: {'identical to the host las' if got == want else 'DIFFERS or missing'}"
          f" (assembled twice); printed {said!r}" + ("" if ok else f" {err('AE', 'AE2', 'AOE')}"))

    # 4. grep: las, then llink with main's libraries, against the host chain
    want = host_las(OBJPROG + ".as", "obj", OBJPROG + ".o", exdir)
    got = fetch(OBJPROG + ".o")
    ook = got is not None and (native.rof_equal(got, want) or native.only_dtb(got[12:], want[12:]))
    hk = os.path.join(host, "K")
    os.makedirs(hk, exist_ok=True)
    subprocess.run([lwdiff.DEFAULT_LLINK, os.path.join(host, OBJPROG + ".o")] +
                   [f"-l={os.path.join(hlib, l)}.l" for l in lib_order(n)] +
                   [f"-o={OBJPROG}"], cwd=hk, env=lwdiff.clean_env(), capture_output=True)
    want = open(os.path.join(hk, OBJPROG), "rb").read() if os.path.exists(
        os.path.join(hk, OBJPROG)) else b""
    got = fetch(OBJPROG)
    kok = got is not None and (got == want or native.only_dtb(got, want, crc=3))
    lw_link = lwdiff_link(n, want)
    found = (text(f"{r}/GO") or "").strip("\n").splitlines()
    readme = open(os.path.join(INSTALL, "README")).read().splitlines()
    # grep -n prints its line count (5 BCD digits) and ":" before each line;
    # compare the lines it found, and show the number: MAME printed
    # "0040::", the model "00044:" (inca/daa, D47), the K2 will tell
    wanted = [l.rstrip() for l in readme if "selftest" in l]
    nums = [re.match(r"^[0-9:;<=>?]*:", l) for l in found]
    gok = [l[m.end() if m else 0:].rstrip() for l, m in zip(found, nums)] == wanted
    ok = ook and kok and gok and lw_link
    bad += not ok
    print(f"  {OBJPROG}.as: object {'ok' if ook else 'DIFFERS'}; linked "
          f"{'identical to host llink' if kok else 'DIFFERS'}"
          f" (host = lwlink's build: {'yes' if lw_link else 'NO'});"
          f" found {len(found)} line(s), {'right' if gok else 'WRONG'}"
          f" (numbered {[m.group(0) for m in nums if m]})")
    if not ok:
        print("    " + err("GE", "KE", "GOE") + f" {found[:3]}")

    # 5. make
    said = (text(f"{r}/MO") or "").strip()
    mk = (text(f"{r}/MK") or "")
    ok = said == MKDEMO and all(w in mk for w in ("main.c", "util.c", "dbl.a"))
    bad += not ok
    print(f"  make: {'built' if ok else 'FAILED'}; mkdemo printed {said!r}")
    if not ok:
        print("    " + err("MK", "MKE", "MOE"))

    hl = text(f"{r}/HL") or ""
    ok = "devsys assembler" in hl
    bad += not ok
    print(f"  help las: {'devsys entry' if ok else 'MISSING: ' + hl[:100]!r}")

    times = [(text(f"{r}/T{k}") or "").strip() for k in range(6)]
    try:
        t = [datetime.datetime.strptime(x, "%B %d, %Y  %H:%M:%S") for x in times]
        s = [(t[k + 1] - t[k]).seconds for k in range(5)]
        print(f"  F256 time: C {s[0]} s; {len(CMDS)} commands {s[1]} s without an image, "
              f"{s[2]} s with it; hello.asm, {OBJPROG} {s[3]} s; make {s[4]} s")
    except (ValueError, IndexError):
        print(f"  clock: {times}")
    # free RAM (mfree's "Total:" line) before and after; mmap for the record
    totals = []
    for k in (0, 1):
        m = re.search(r"Total:\s*([0-9A-Fa-f]+)\s+(\S+)", text(f"{r}/M{k}") or "")
        totals.append((int(m.group(1), 16), m.group(2)) if m else None)   # blocks in hex
    if None in totals:
        print("  free RAM: no mfree output")
        bad += 1
    else:
        print(f"  free RAM: {totals[0][0]} blocks ({totals[0][1]}) before, "
              f"{totals[1][0]} ({totals[1][1]}) after")
        if totals[1][0] < totals[0][0]:
            print(f"  {totals[0][0] - totals[1][0]} block(s) not given back")
            bad += 1
    for k in ("M0", "M1", "MM"):
        fetch(k)
    print(f"card: {'all good' if not bad else f'{bad} problem(s)'}")
    return 0 if not bad else 1


def lwdiff_link(n, ours):
    """True if lwlink links main's program as the host llink did (ours):
    the real build, with lwasm's objects and lwar's libraries."""
    work = os.path.join(n["work"], "lw")
    os.makedirs(os.path.join(work, ".lib"), exist_ok=True)
    os.makedirs(os.path.join(work, ".obj"), exist_ok=True)
    jobs = {j["id"]: j for j in n["jobs"]}
    for name, members in n["members"]:
        objs = []
        for j in members:
            ok, out, _ = lwdiff.run_reference(j, lwdiff.DEFAULT_LWASM, os.path.join(work, ".obj"),
                                              n["gen"])
            objs.append(out)
        lib = os.path.join(work, ".lib", f"lib{name}.a")
        if os.path.exists(lib):
            os.remove(lib)
        subprocess.run([lwdiff.DEFAULT_LWAR, "-c", lib] + objs, capture_output=True)
    ok, obj, _ = lwdiff.run_reference(jobs[OBJPROG + ".o"], lwdiff.DEFAULT_LWASM,
                                      os.path.join(work, ".obj"), n["gen"])
    out = os.path.join(work, OBJPROG)
    if os.path.exists(out):
        os.remove(out)
    subprocess.run([lwdiff.DEFAULT_LWLINK, "--format=os9", "-L", os.path.join(work, ".lib")] +
                   [f"-l{l}" for l in lib_order(n)] + [obj, f"-o{OBJPROG}"],
                   cwd=work, capture_output=True, env=lwdiff.clean_env())   # -o names it
    return os.path.exists(out) and open(out, "rb").read() == ours


def mame(image):
    native.WORK = WORK
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    if subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=native.NATIVE).returncode:
        sys.exit("native build failed")
    dsk = os.path.join(WORK, "test.dsk")
    n = build(image, dsk, "jr2")
    native.os9("makdir", f"{dsk},T", check=False)
    st = os.path.join(WORK, "startup")
    with open(st, "w") as f:
        f.write(f"-x\n/dd/{D}/selftest\necho done >/dd/T/{native.MARKER}\n")
    native.put(st, f"{dsk},startup", text=True)
    if not native.run_mame(dsk, 7200):
        return 1
    return check(dsk, "jr2", n)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmd", choices=("build", "check", "mame"))
    ap.add_argument("file", nargs="?", help="check: the card image read back")
    ap.add_argument("--image", help="boot image to start from")
    ap.add_argument("--platform", choices=("k2", "jr2"), default="k2",
                    help="build, check: the machine (sets main's -Dk2 or -Djr2)")
    ap.add_argument("--out", default=os.path.join(HERE, "out", "devsys_k2.dsk"))
    a = ap.parse_args()
    if a.cmd == "build":
        if subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=native.NATIVE).returncode:
            sys.exit("native build failed")
        build(a.image or (K2 if a.platform == "k2" else JR2), os.path.abspath(a.out), a.platform)
        return 0
    if a.cmd == "check":
        if not a.file:
            sys.exit("check: which image?")
        return check(os.path.abspath(a.file), a.platform)
    return mame(a.image or JR2)


if __name__ == "__main__":
    sys.exit(main())
