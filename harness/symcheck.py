#!/usr/bin/env python3
"""symcheck - check that a change to dcc68 didn't change its output (D57).

dcc68 keeps its symbols and lists in banked 8K blocks reached by handles
(dcc/CC09/symdb.c on bank.c), and a pointer kept past the next access to
another record reads the wrong bytes, but only where blocks really
switch. So the current dcc68 is compared with a reference built from a
git revision (default HEAD) on every input, in three builds:

  plain     dcc/CC09/dcc68 (Linux: every block stays where it is)
  bankwin   dcc/CC09-bankwin: one window, emulated by copying a block in
            and out, as strict as the 6809
  bank16    dcc/CC09-bank16: the same with 2-byte fields, so the blocks
            fill and switch as they do on the 6809

Each run must give the reference's exit status, messages and assembly.
Inputs: every C source in CoCoC (through the devsys dcpp), the
cross-build's native/obj/*/*.i (run make in native/ first), clib's
clib/obj/*/*.i, harness/native/*.c, and any --add FILE.c. The bankwin
and bank16 builds print the blocks they used and their window switches.

  python3 harness/symcheck.py                     # against HEAD's dcc68
  python3 harness/symcheck.py --ref 7dba96f --add ~/projects/f256/jfed/te/te/te.c
"""
import argparse
import concurrent.futures as cf
import glob
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
COCOC = os.path.expanduser("~/projects/dcc/CoCoC")
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

WORK = os.path.join(HERE, "out", "symcheck")
BUILDS = {"plain": "dcc/CC09/dcc68", "bankwin": "dcc/CC09-bankwin/dcc68",
          "bank16": "dcc/CC09-bank16/dcc68"}


def make(d):
    p = subprocess.run(["make", "-s", "-f", "GNUmakefile"], cwd=d,
                       env=lwdiff.clean_env(), capture_output=True, text=True)
    if p.returncode:
        sys.exit(f"build in {d} failed:\n{p.stderr[-2000:]}")


def reference(ref):
    """dcc68 built from dcc/CC09 at git revision ref."""
    d = os.path.join(WORK, "ref")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    tar = subprocess.run(["git", "archive", ref, "dcc/CC09"], cwd=DEVSYS,
                         capture_output=True, check=True).stdout
    tarfile.open(fileobj=io.BytesIO(tar)).extractall(d, filter="data")
    make(os.path.join(d, "dcc/CC09"))
    return os.path.join(d, "dcc/CC09/dcc68")


def preprocess(src, d, n):
    pre = subprocess.run([os.path.join(DEVSYS, "dcc/CPP/dcpp"),
                          "-I=" + os.path.dirname(src), "-I=" + os.path.join(COCOC, "Defs"),
                          src], cwd=d, env=lwdiff.clean_env(), capture_output=True, timeout=60)
    if pre.returncode or not pre.stdout:
        return None
    i = os.path.join(d, f"c{n}.i")
    open(i, "wb").write(pre.stdout)
    return i


def inputs(extra):
    d = os.path.join(WORK, "in")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    srcs = sorted(glob.glob(os.path.join(COCOC, "Source", "**", "*.c"), recursive=True))
    srcs += sorted(glob.glob(os.path.join(HERE, "native", "*.c"))) + extra
    files = [i for n, s in enumerate(srcs) if (i := preprocess(s, d, n))]
    seen = set()
    for f in files + sorted(glob.glob(os.path.join(DEVSYS, "native", "obj", "*", "*.i"))) \
            + sorted(glob.glob(os.path.join(DEVSYS, "clib", "obj", "*", "*.i"))):
        h = hashlib.md5(open(f, "rb").read()).hexdigest()
        if h not in seen:
            seen.add(h)
            yield f


def run(binary, f, tag):
    w = os.path.join(WORK, "run", tag, hashlib.md5(f.encode()).hexdigest())
    os.makedirs(w, exist_ok=True)
    p = subprocess.run([binary, f, "-o=out.a"], cwd=w, env=lwdiff.clean_env(),
                       capture_output=True, timeout=120)
    out = os.path.join(w, "out.a")
    asm = open(out, "rb").read() if os.path.exists(out) else b""
    shutil.rmtree(w)
    lines = (p.stdout + p.stderr).splitlines(True)
    stats = [x for x in lines if x.startswith(b"bankstats:")]
    msgs = b"".join(x for x in lines if not x.startswith(b"bankstats:"))
    return (p.returncode, msgs, asm), stats


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ref", default="HEAD", help="git revision of the reference")
    ap.add_argument("--add", action="append", default=[], help="another C source")
    args = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    for b in BUILDS.values():
        make(os.path.join(DEVSYS, os.path.dirname(b)))
    ref = reference(args.ref)
    files = list(inputs([os.path.abspath(os.path.expanduser(a)) for a in args.add]))
    refout = {}
    with cf.ThreadPoolExecutor(os.cpu_count()) as ex:
        for f, (r, _) in zip(files, ex.map(lambda f: run(ref, f, "ref"), files)):
            refout[f] = r
    ok = True
    for name, b in BUILDS.items():
        binary = os.path.join(DEVSYS, b)
        bad, stats = [], []
        with cf.ThreadPoolExecutor(os.cpu_count()) as ex:
            for f, (r, s) in zip(files, ex.map(lambda f: run(binary, f, name), files)):
                stats += s
                if r != refout[f]:
                    bad.append(f)
        line = f"symcheck: {name} {len(files) - len(bad)} of {len(files)} identical to {args.ref}'s dcc68"
        if stats:
            blocks = [int(x.split()[2]) for x in stats]
            switches = [int(x.split()[6]) for x in stats]
            line += (f" (blocks: at most {max(blocks)}, {sum(b > 1 for b in blocks)} files over"
                     f" one; window switches: at most {max(switches)})")
        print(line)
        for f in bad[:10]:
            print("  differs:", f)
        ok = ok and not bad
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
