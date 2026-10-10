#!/usr/bin/env python3
"""card - put the native toolchain on a real F256's SD card, and check it.

  card.py build [--image FILE] [--out FILE]
      A copy of the K2 boot image (default l2_wildbitsk2.dsk) with the six
      tools in /dd/CMDS, CoCoC's C libraries in /dd/LIB, its headers in
      /dd/DEFS, /dd/TMP, and /dd/DEVSYS: test sources and the procedure
      file `selftest`. Write it to the card with dd (check the device with
      lsblk first). On the F256 type:   /dd/DEVSYS/selftest
      It writes everything to /dd/DEVSYS/R (run it once per card write:
      OS-9's shell won't redirect into a file that exists).
  card.py check FILE
      FILE: the card read back (dd with an explicit byte count). Compares
      what the F256 made with the Linux tools: hello.c's module and what it
      printed, each NitrOS9 module without and with a defs image (against
      the host las, ,pcr sizing included), and prints the F256's timings.
  card.py mame [--image FILE]
      The same card for the Jr2 (MAME has no K2), run in the Wildbits MAME
      with selftest as the startup, then checked: proves the card before
      it goes on the machine.

What selftest does, timed with date -t between steps (mfree before and
after, so check can report free RAM and blocks not given back; mmap last):
  1. dcc builds hello.c (dcpp, dcc68, dco68, las, llink) and runs it
  2. las assembles CMDS of level1/cmds without a defs image
  3. the same with --defsimg (the first records the image, the rest load it)
"""
import argparse
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
L2 = os.path.expanduser("~/projects/wild/netrek/nitros9/recipes/wildbits/l2")
K2 = os.path.join(L2, "l2_wildbitsk2.dsk")
JR2 = os.path.join(L2, "l2_wildbitsjr2.dsk")
WORK = os.path.join(HERE, "out", "card")
SRCDIR = "level1/cmds"
CMDS = ["echo", "date", "attr", "copy", "dir", "list", "free", "dump"]
D = "DEVSYS"


def jobs():
    """The modules selftest assembles, staged as the img suite does: one
    directory, one configuration, files placed once."""
    native.ONLY[:] = []
    native.SHARD[:] = [0, 1]
    staged = [(j, f) for j, f in native.stage_lw()
              if os.path.dirname(j["source"]).endswith("/" + SRCDIR) and j["id"] in CMDS]
    keys = {json.dumps([j["defines"], j["pragmas"], j["includes"]]) for j, _ in staged}
    if len(staged) != len(CMDS) or len(keys) != 1:
        sys.exit(f"card: wanted {CMDS} in {SRCDIR} with one configuration")
    staged.sort(key=lambda jf: CMDS.index(jf[0]["id"]))
    placed = {}
    for j, files in staged:
        for path, place in files:
            if placed.setdefault(place, path) != path:
                sys.exit(f"card: {place} would be two files")
    return staged, placed


def target(place):
    """Where a staged file goes: G is the commands' tree, S the shared files."""
    return f"{D}/G/" + place[2:] if place.startswith("J/") else f"{D}/" + place


def build(image, out):
    native.WORK = WORK
    os.makedirs(WORK, exist_ok=True)
    native.make_image(image, out)
    os9 = native.os9
    for d in (D, f"{D}/S", f"{D}/G", f"{D}/G/src", f"{D}/R", f"{D}/C"):
        os9("makdir", f"{out},{d}", check=False)
    native.put(native.HELLO, f"{out},{D}/C/hello.c", text=True)
    staged, placed = jobs()
    for place, path in placed.items():
        t = target(place)
        if os.path.dirname(t) not in (f"{D}/S", f"{D}/G", f"{D}/G/src"):
            os9("makdir", f"{out},{os.path.dirname(t)}", check=False)
        native.put(path, f"{out},{t}", text=native.is_text(path))

    r = f"/dd/{D}/R"
    lines = ["-x", "* devsys selftest: results in /dd/DEVSYS/R", f"mfree >{r}/M0",
             f"date -t >{r}/T0",
             f"chd /dd/{D}/C", f"dcc hello.c -f={r}/hello >{r}/CO >>{r}/CE",
             f"{r}/hello >{r}/HO >>{r}/HE", f"date -t >{r}/T1", f"chd /dd/{D}/G/src"]
    argfile = os.path.join(WORK, "args.tmp")
    for rnd, (sub, extra) in enumerate((("P", []), ("I", [f"--defsimg=/dd/{D}/IMG"]))):
        for n, (j, _) in enumerate(staged):
            with open(argfile, "w") as f:
                f.write("\n".join(native.lw_args(j) + [f"-i=/dd/{D}/S", f"-o={r}/{sub}{n}"]
                                  + extra) + "\n")
            native.put(argfile, f"{out},{D}/G/src/{sub}{n}", text=True)
            lines.append(f"las {os.path.basename(j['source'])} --args={sub}{n}"
                         f" >{r}/L{sub}{n} >>{r}/E{sub}{n}")
        lines.append(f"date -t >{r}/T{rnd + 2}")
    # free RAM after: less than before means a tool kept blocks (F$DelRAM)
    lines += [f"mfree >{r}/M1", f"mmap >{r}/MM", f"echo done >{r}/DONE"]
    if max(len(l) for l in lines) > 199:
        sys.exit("card: a selftest line is longer than the shell's 200 characters")
    st = os.path.join(WORK, "selftest")
    with open(st, "w") as f:
        f.write("\n".join(lines) + "\n")
    native.put(st, f"{out},{D}/selftest", text=True)
    os9("attr", "-q", "-e", "-pe", "-r", "-pr", f"{out},{D}/selftest")
    print(f"card: {out} ({os.path.getsize(out)} bytes), selftest of {len(lines)} lines")


def check(dsk):
    """Compare what selftest left on the image with the Linux tools."""
    native.WORK = WORK
    os.makedirs(WORK, exist_ok=True)
    get, text = native.get, native.os9_text
    r = f"{dsk},{D}/R"
    bad = 0
    if not get(f"{r}/DONE", os.path.join(WORK, "DONE")):
        print("card: selftest didn't finish (no DONE)")
        bad += 1

    # 1. C
    ref = native.host_cc("hello", native.HELLO)
    out = os.path.join(WORK, "res", "hello")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    same = get(f"{r}/hello", out) and open(out, "rb").read() == open(ref, "rb").read()
    printed = (text(f"{r}/HO") or "").strip()
    ok = same and printed == "hello from dcc on the F256: 45"
    bad += not ok
    print(f"  C: hello {'identical to the host passes' if same else 'DIFFERS or missing'};"
          f" printed {printed!r}")
    if not ok:
        print("    " + ((text(f"{r}/CE") or "") + (text(f"{r}/HE") or "")).strip()[:300])

    # 2, 3. modules, against the host las on the same layout
    staged, placed = jobs()
    host = os.path.join(WORK, "host")
    shutil.rmtree(host, ignore_errors=True)
    for place, path in placed.items():
        t = os.path.join(host, target(place).split("/", 1)[1])
        os.makedirs(os.path.dirname(t), exist_ok=True)
        shutil.copyfile(path, t)
    for n, (j, _) in enumerate(staged):
        refp = os.path.join(host, f"ref{n}")
        subprocess.run([native.REF, os.path.basename(j["source"])] + native.lw_args(j) +
                       ["-i=" + os.path.join(host, "S"), "-o=" + refp],
                       cwd=os.path.join(host, "G", "src"), env=lwdiff.clean_env(),
                       capture_output=True)
        want = open(refp, "rb").read()
        res = []
        for sub in "PI":
            o = os.path.join(WORK, "res", f"{sub}{n}")
            ok = get(f"{r}/{sub}{n}", o) and open(o, "rb").read() == want
            bad += not ok
            res.append("ok" if ok else "DIFFERS: " + (text(f"{r}/E{sub}{n}") or "").strip()[:120])
        print(f"  {j['id']:8} without image: {res[0]:4}  with image: {res[1]}")
    if not get(f"{dsk},{D}/IMG", os.path.join(WORK, "res", "IMG")):
        print("  no defs image was written")
        bad += 1

    times = [(text(f"{r}/T{k}") or "").strip() for k in range(4)]
    try:
        t = [datetime.datetime.strptime(x, "%B %d, %Y  %H:%M:%S") for x in times]
        print(f"  F256 time: hello {(t[1] - t[0]).seconds} s; {len(staged)} modules "
              f"{(t[2] - t[1]).seconds} s without an image, {(t[3] - t[2]).seconds} s with it")
    except (ValueError, IndexError):
        print(f"  clock: {times}")
    # 4. free RAM (mfree's "Total:" line) before and after; mmap for the record
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
        get(f"{r}/{k}", os.path.join(WORK, "res", k))
    print(f"card: {'all good' if not bad else f'{bad} problem(s)'}")
    return 0 if not bad else 1


def mame(image):
    native.WORK = WORK
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    if subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=native.NATIVE).returncode:
        sys.exit("native build failed")
    dsk = os.path.join(WORK, "test.dsk")
    build(image, dsk)
    native.os9("makdir", f"{dsk},T", check=False)
    st = os.path.join(WORK, "startup")
    with open(st, "w") as f:
        f.write(f"-x\n/dd/{D}/selftest\necho done >/dd/T/{native.MARKER}\n")
    native.put(st, f"{dsk},startup", text=True)
    if not native.run_mame(dsk, 7200):
        return 1
    return check(dsk)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmd", choices=("build", "check", "mame"))
    ap.add_argument("file", nargs="?", help="check: the card image read back")
    ap.add_argument("--image", help="boot image to start from")
    ap.add_argument("--out", default=os.path.join(HERE, "out", "devsys_k2.dsk"))
    a = ap.parse_args()
    if a.cmd == "build":
        if subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=native.NATIVE).returncode:
            sys.exit("native build failed")
        build(a.image or K2, os.path.abspath(a.out))
        return 0
    if a.cmd == "check":
        if not a.file:
            sys.exit("check: which image?")
        return check(os.path.abspath(a.file))
    return mame(a.image or JR2)


if __name__ == "__main__":
    sys.exit(main())
