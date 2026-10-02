#!/usr/bin/env python3
"""dccdiff - compare two DCC builds on every C source in CoCoC.

Runs each ~/projects/dcc/CoCoC/Source/**/*.c through dcpp, dcc68 and dco68
of a reference build (default: CoCoC's own binaries) and of a new build
(default: devsys/dcc), and compares the assembly each produces.

  python3 harness/dccdiff.py                 # summary
  python3 harness/dccdiff.py --show N        # also print N differing files
  python3 harness/dccdiff.py --ref DIR --new DIR
  python3 harness/dccdiff.py --names8        # long-name check (phase 6)

Each DIR holds CPP/dcpp, CC09/dcc68 and COpt/dco68. Exit status is 0 only
if every file that compiles with the reference gives identical output.

--names8 counts a file as identical if, token by token, the only
differences are names longer than 8 characters in the new output where the
reference has their first 8: with a DCC that keeps long names, anything
still listed differs in some other way.
"""
import argparse
import concurrent.futures
import difflib
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

HOME = os.path.expanduser("~")
COCOC = os.path.join(HOME, "projects/dcc/CoCoC")
TIMEOUT = 30


def compile_c(dcc, src, work):
    """dcpp | dcc68 | dco68 for one file; the assembly as bytes, or None."""
    os.makedirs(work, exist_ok=True)
    d = os.path.dirname(src)
    env = lwdiff.clean_env()
    try:
        pre = subprocess.run([os.path.join(dcc, "CPP/dcpp"), f"-I={d}",
                              "-I=" + os.path.join(COCOC, "Defs"), src],
                             cwd=work, env=env, capture_output=True, timeout=TIMEOUT)
        if pre.returncode or not pre.stdout:
            return None
        ipath, raw = os.path.join(work, "x.i"), os.path.join(work, "x.raw.a")
        with open(ipath, "wb") as f:
            f.write(pre.stdout)
        if os.path.exists(raw):
            os.remove(raw)
        c = subprocess.run([os.path.join(dcc, "CC09/dcc68"), ipath, f"-o={raw}"],
                           cwd=work, env=env, capture_output=True, timeout=TIMEOUT)
        if c.returncode or not os.path.exists(raw):
            return None
        with open(raw, "rb") as fin:
            o = subprocess.run([os.path.join(dcc, "COpt/dco68")], stdin=fin, env=env,
                               capture_output=True, timeout=TIMEOUT)
        return None if o.returncode else o.stdout
    except subprocess.TimeoutExpired:
        return None


def names_only(a, b):
    """Do a and b differ only where b has a name longer than 8 characters
    and a has its first 8? Compared token by token."""
    ta = re.split(rb"([A-Za-z_][A-Za-z0-9_]*)", a)
    tb = re.split(rb"([A-Za-z_][A-Za-z0-9_]*)", b)
    return len(ta) == len(tb) and all(
        x == y or (len(y) > 8 and y[:8] == x) for x, y in zip(ta, tb))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ref", default=os.path.join(COCOC, "Source/Compiler"))
    ap.add_argument("--new", default=os.path.join(os.path.dirname(HERE), "dcc"))
    ap.add_argument("--show", type=int, default=0, help="print diffs of N files")
    ap.add_argument("--names8", action="store_true",
                    help="ignore differences that are only longer names")
    args = ap.parse_args()

    sources = sorted(glob.glob(os.path.join(COCOC, "Source", "**", "*.c"), recursive=True))
    base = os.path.join(HERE, "out", "dccdiff")

    def one(i_src):
        i, src = i_src
        w = os.path.join(base, str(i))
        a = compile_c(args.ref, src, w + "r")
        b = compile_c(args.new, src, w + "n")
        if a is not None and b is not None and args.names8 and names_only(a, b):
            b = a           # differs only by names the reference cut short
        return src, a, b

    same, diff, refonly, newonly, neither = [], [], [], [], 0
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for src, a, b in ex.map(one, enumerate(sources)):
            rel = os.path.relpath(src, COCOC)
            if a is None and b is None:
                neither += 1
            elif b is None:
                refonly.append(rel)
            elif a is None:
                newonly.append(rel)
            elif a == b:
                same.append(rel)
            else:
                diff.append((rel, a, b))

    print(f"{len(sources)} C sources: identical {len(same)}  different {len(diff)}  "
          f"only reference compiles {len(refonly)}  only new compiles {len(newonly)}  "
          f"neither {neither}")
    for rel in refonly:
        print("  new build fails:", rel)
    for rel in newonly:
        print("  reference fails:", rel)
    for rel, a, b in diff[:args.show]:
        print(f"--- {rel}")
        sys.stdout.writelines(list(difflib.unified_diff(
            a.decode("latin-1").splitlines(True), b.decode("latin-1").splitlines(True),
            "reference", "new", n=1))[:40])
    sys.exit(0 if not diff and not refonly else 1)


if __name__ == "__main__":
    main()
