#!/usr/bin/env python3
"""kreiderdiff - NitrOS9's lib/kreiderclib against devsys's C library.

NitrOS9 main carries lib/kreiderclib: 130 .as files made by Os9disasm from
an old Kreider clib.l, built by no makefile. devsys's clib/ is the same
library from its sources (D52). This assembles each kreiderclib file with
the host las (NitrOS9's object-file options: lwasm mode, its pragmas with
`export`, as rules.mak's ASM) and compares it with our clib.l and sys.l
(docs/nitros9-fixes.md #6, DESIGN.md D68).

  python3 harness/kreiderdiff.py [--nitros9 DIR] [--verbose]

Reports, per file:
  identical  code and data the same bytes as our member defining its name
  broken     the disassembly lost something: a reference to a label it no
             longer defines (Lxxxx, usually a string after the code), or an
             entry point turned into a constant (`sleep equ *-2`)
  differs    the rest, with both sizes (older sources or compiler, fixes)
then the labels more than one file defines (with `export` a library of
them can't link: "symbol already defined") and the entry points only ours
has. Exit status 0 always: it describes, it doesn't judge.
"""
import argparse
import collections
import glob
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from libdiff import members  # noqa: E402

DEVSYS = os.path.dirname(HERE)
LAS = os.path.join(DEVSYS, "las", "build", "unix", "las")
OURS = [os.path.join(DEVSYS, "clib", "lib", f) for f in ("clib.l", "sys.l")]
PRAGMAS = "--pragma=pcaspcr,condundefzero,undefextern,dollarnotlocal,noforwardrefmax,export"
DISASM = re.compile(r"^[LBGDX][0-9A-Fa-f]{4}$")     # Os9disasm's made-up labels
CONST = 6                                           # ROF definition flag: constant


def entries(m):
    """A member's code/data names (not constants, not Os9disasm's)."""
    return sorted(s for s, f, _ in m["defs"] if f != CONST and not DISASM.match(s)
                  and s != "__lwsec")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nitros9", default=os.path.expanduser("~/projects/wild/main/nitros9"))
    ap.add_argument("--verbose", action="store_true")
    o = ap.parse_args()
    src = sorted(glob.glob(os.path.join(o.nitros9, "lib", "kreiderclib", "*.as")))
    if not src:
        sys.exit(f"kreiderdiff: no lib/kreiderclib/*.as in {o.nitros9}")

    ours, ournames = {}, set()
    for lib in OURS:
        for n, m in members(lib):
            for s in entries(m):
                ours.setdefault(s, (n, m))
                ournames.add(s)

    env = {k: v for k, v in os.environ.items() if k != "LD_PRELOAD"}
    same, broken, differ, unmatched = [], [], [], []
    defined = collections.Counter()
    theirs = set()
    with tempfile.TemporaryDirectory() as tmp:
        for f in src:
            base = os.path.basename(f)[:-3]
            out = os.path.join(tmp, base + ".r")
            p = subprocess.run([LAS, f, "--lwasm", PRAGMAS, f"-o={out}"],
                               capture_output=True, text=True, env=env)
            if p.returncode != 0:
                broken.append((base, "las failed: " + p.stderr.strip()[:100]))
                continue
            for n, m in members(out):
                for s, fl, _ in m["defs"]:
                    if fl != CONST:
                        defined[s] += 1
                names = entries(m)
                theirs.update(names)
                lost = sorted({s for s, _, _ in m["refs"] if DISASM.match(s)})
                asconst = [s for s, fl, _ in m["defs"] if fl == CONST and s.islower()
                           and not s.startswith("__")]
                if lost or asconst:
                    why = []
                    if lost:
                        why.append("refers to labels it doesn't define: " + " ".join(lost))
                    if asconst:
                        why.append("entry point is a constant: " + " ".join(asconst))
                    broken.append((base, "; ".join(why)))
                    continue
                match = [s for s in names if s in ours]
                if not match:
                    unmatched.append(base)
                    continue
                on, om = ours[match[0]]
                if m["body"] == om["body"]:
                    same.append(base)
                else:
                    differ.append((base, on, len(m["body"]), len(om["body"])))

    rest = sorted(set(os.listdir(os.path.dirname(src[0]))) - {os.path.basename(f) for f in src})
    print(f"kreiderclib: {len(src)} .as files (not assembled: {' '.join(rest)})")
    print(f"  identical to ours: {len(same)}")
    if o.verbose:
        print("    " + " ".join(same))
    print(f"  broken by the disassembly: {len(broken)}")
    for b, why in broken:
        print(f"    {b}: {why}")
    print(f"  differ: {len(differ)} (theirs / ours, bytes of code and data)")
    for b, on, a, c in differ:
        print(f"    {b} ~ {on}: {a} / {c}")
    if unmatched:
        print(f"  no member of ours defines their names: {' '.join(unmatched)}")
    dup = sorted((s for s, k in defined.items() if k > 1), key=lambda s: -defined[s])
    print(f"  labels defined by more than one file: {len(dup)}"
          + (f" (e.g. {', '.join(f'{s} x{defined[s]}' for s in dup[:4])})" if dup else ""))
    only = sorted(ournames - theirs)
    print(f"  entry points only ours has: {len(only)} (some are only other names for the same"
          " code: theirs calls signal() _sigint, index() strchr)")
    if o.verbose:
        print("    " + " ".join(only))
    return 0


if __name__ == "__main__":
    sys.exit(main())
