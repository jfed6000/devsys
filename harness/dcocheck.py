#!/usr/bin/env python3
"""dcocheck - check that a change to dco68 didn't change its output (D51).

dco68 keeps its records in banked 8K blocks reached by handles
(dcc/COpt/bank.c), and a pointer kept past the next access to another
block reads the wrong bytes, but only where blocks really switch. So the
current dco68 is compared with a reference built from a git revision
(default HEAD) on every input, in three builds:

  plain     dcc/COpt/dco68 (Linux: every block stays where it is)
  bankwin1  -DBANKWIN -DNWIN=1: one window, emulated by copying a block in
            and out, as strict as the 6809 with one window
  bankwin3  -DBANKWIN -DNWIN=3: three windows, as the 6809 build has (D61)

Inputs: dcc68's output for every C source in CoCoC (through the devsys
dcpp and dcc68) and the cross-build's native/obj/*/*.raw.a (run make in
native/ first), each with and without -2. Exit status 0 when every run
is identical to the reference.

  python3 harness/dcocheck.py              # against HEAD's dco68
  python3 harness/dcocheck.py --ref b6a2f26
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
COCOC = os.path.expanduser("~/projects/dcc/CoCoC")
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

WORK = os.path.join(HERE, "out", "dcocheck")


def build(src, out, extra=""):
    """dco68 from the sources in src, built in out (with extra CFLAGS)."""
    shutil.rmtree(out, ignore_errors=True)
    shutil.copytree(src, out, ignore=shutil.ignore_patterns("*.o", "dco68"))
    mk = os.path.join(out, "GNUmakefile")
    text = open(mk).read()
    if extra:
        text = text.replace("CFLAGS\t= -g -mx32 ", f"CFLAGS\t= -g -mx32 {extra} ", 1)
    # the patterns stay where the source tree has them
    text = text.replace("DCCDIR\t= $(CURDIR)", f"DCCDIR\t= {src}", 1)
    open(mk, "w").write(text)
    p = subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=out,
                       env=lwdiff.clean_env(), capture_output=True, text=True)
    if p.returncode:
        sys.exit(f"build in {out} failed:\n{p.stderr[-2000:]}")
    return os.path.join(out, "dco68")


def inputs():
    """dcc68's output for CoCoC's C sources, and the cross-build's."""
    d = os.path.join(WORK, "in")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    env = lwdiff.clean_env()
    files = []
    for n, src in enumerate(sorted(glob.glob(os.path.join(COCOC, "Source", "**", "*.c"),
                                             recursive=True))):
        pre = subprocess.run([os.path.join(DEVSYS, "dcc/CPP/dcpp"),
                              "-I=" + os.path.dirname(src), "-I=" + os.path.join(COCOC, "Defs"),
                              src], cwd=d, env=env, capture_output=True, timeout=60)
        if pre.returncode or not pre.stdout:
            continue
        i = os.path.join(d, "x.i")
        open(i, "wb").write(pre.stdout)
        raw = os.path.join(d, f"c{n}.raw.a")
        c = subprocess.run([os.path.join(DEVSYS, "dcc/CC09/dcc68"), i, "-o=" + raw],
                           cwd=d, env=env, capture_output=True, timeout=60)
        if c.returncode == 0 and os.path.exists(raw):
            files.append(raw)
        elif os.path.exists(raw):
            os.remove(raw)
    files += sorted(glob.glob(os.path.join(DEVSYS, "native", "obj", "*", "*.raw.a")))
    return files


def run(dco, flags, f):
    with open(f, "rb") as fin:
        p = subprocess.run([dco] + flags, stdin=fin, capture_output=True,
                           env=lwdiff.clean_env(), timeout=300)
    return p.returncode, p.stdout, p.stderr


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ref", default="HEAD", help="git revision of the reference dco68")
    args = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)

    refsrc = os.path.join(WORK, "refsrc")
    shutil.rmtree(refsrc, ignore_errors=True)
    os.makedirs(refsrc)
    arch = subprocess.run(["git", "archive", args.ref, "dcc/COpt"], cwd=DEVSYS,
                          capture_output=True)
    if arch.returncode:
        sys.exit(f"git archive {args.ref}: {arch.stderr.decode()}")
    subprocess.run(["tar", "-x", "-C", refsrc], input=arch.stdout, check=True)
    cur = os.path.join(DEVSYS, "dcc", "COpt")
    builds = {"reference": build(os.path.join(refsrc, "dcc", "COpt"),
                                 os.path.join(WORK, "ref")),
              "plain": build(cur, os.path.join(WORK, "plain")),
              "bankwin1": build(cur, os.path.join(WORK, "bw1"), "-DBANKWIN -DNWIN=1"),
              "bankwin3": build(cur, os.path.join(WORK, "bw3"), "-DBANKWIN -DNWIN=3")}

    files = inputs()
    bad = {k: 0 for k in builds if k != "reference"}
    for f in files:
        for flags in (["-2"], []):
            ref = run(builds["reference"], flags, f)
            for k in bad:
                if run(builds[k], flags, f) != ref:
                    bad[k] += 1
                    print(f"  {k}: differs on {os.path.relpath(f, DEVSYS)} {' '.join(flags)}")
    n = 2 * len(files)
    for k, b in bad.items():
        print(f"dcocheck: {k} {n - b} of {n} identical to {args.ref}'s dco68")
    sys.exit(1 if any(bad.values()) else 0)


if __name__ == "__main__":
    main()
