#!/usr/bin/env python3
"""fltcheck - Linux dcc68 and 6809 dcc68 make the same float constants.

Step 18 (D52): dcc68 on Linux keeps a float or double constant in the
6809's format and does the compiler's constant work (build a literal,
negate it, convert it to and from int, long, float) as the 6809's
cfloats.a does (dcc/CC09/fp6809.c). This compiles batches of generated
declarations with both and compares what they write: the Linux dcc68
(dcc/CC09/dcc68) against native/bin/dcc68 on harness/os9run.py's model.

  python3 harness/fltcheck.py [--count N] [--batch N] [--seed S] [--keep]

Literals: integers near 2^31, 2^32 and 2^56 (the digit sum's limits;
none overflows it, the edge cases do),
random digit strings with fractions and exponents, their negatives, and
conversions (int and long from double, float from double, double from
int and long). Values stay inside the 6809's range (about 2.9e-39 to
1.7e38): out of it, both wrap or stop the same way, which the edge
cases at the end check one file each. Exit status 0 when all agree.
"""
import argparse
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
HOST = os.path.join(DEVSYS, "dcc/CC09/dcc68")
NATIVE = os.path.join(DEVSYS, "native/bin/dcc68")
MSG = os.path.join(DEVSYS, "dcc/CC09/dcc68.msg")


def digits(rng, n):
    return str(rng.randint(1, 9)) + "".join(str(rng.randint(0, 9)) for _ in range(n - 1))


def literal(rng):
    k = rng.random()
    if k < 0.15:   # integers at the digit sum's edges
        base = rng.choice([2**31, 2**32, 2**56 - 2**31, 2**56 // 10, 2**55, 2**56 - 1])
        v = max(1, base + rng.randint(-1000, 1000))
        if v >= 2**56:
            v = 2**56 - 1
        # no fraction digit: one more would take the digit sum past 2^56
        # (a "constant overflow" on both)
        return f"{v}" + rng.choice([".", "e0", ".e0"])
    if k < 0.3:    # an integer value, written out
        n = rng.randint(1, 14)    # with up to 2 zeros: below 2^56
        return digits(rng, n) + "." + "0" * rng.randint(0, 2)
    # mantissa digits, a point somewhere, an exponent; the value stays
    # within 1e-37..1e37 (16 digits: the digit sum stays below 2^56)
    n = rng.randint(1, 16)
    s = digits(rng, n)
    point = rng.randint(0, n)
    lead = rng.randint(0, 3)
    if point == 0:
        s = "0." + "0" * lead + s
        mag = -lead - 1
    else:
        s = s[:point] + "." + s[point:]
        mag = point - 1
    lo, hi = -37 - mag, 37 - mag
    if rng.random() < 0.7 and lo <= hi:
        e = rng.randint(max(lo, -39), min(hi, 39))
        s += rng.choice("eE") + (str(e) if e >= 0 else "-" + str(-e))
    return s


def batch(rng, count):
    lines, k = [], 0
    for i in range(count):
        lit = literal(rng)
        r = rng.random()
        neg = "-" if rng.random() < 0.3 else ""
        if r < 0.7:
            lines.append(f"double d{i} = {neg}{lit};")
        elif r < 0.8:
            lines.append(f"float f{i} = {neg}{lit};")
        elif r < 0.87:
            # int and long from double: keep inside a long
            v = f"{rng.randint(0, 2**31 - 2)}.{digits(rng, 3)}"
            lines.append(f"long l{i} = {neg}{v};")
        elif r < 0.93:
            v = f"{rng.randint(0, 32766)}.{digits(rng, 2)}"
            lines.append(f"int n{i} = {neg}{v};")
        elif r < 0.95:
            lines.append(f"double i{i} = {neg}{rng.randint(0, 65535)};")
        elif r < 0.97:
            # a decimal constant 32768..65535 is a long (it was the int
            # -1..-32768 on the 6809)
            lines.append(f"long q{i} = {neg}{rng.randint(0, 99999)};")
        else:
            lines.append(f"double m{i} = {neg}{rng.randint(0, 2**31 - 1)}L;")
    return "\n".join(lines) + "\n"


EDGES = [
    "double a = 0.0;", "double a = 0.000;", "double a = -0.0;",
    "double a = 1.7014118346046923e+38;", "double a = 1.7e38;",
    "double a = 2.9387358770557188e-39;", "double a = 3e38;", "double a = 1e-39;",
    "double a = 72057594037927935.0;", "double a = 72057594037927936.0;",
    "double a = 4294967295.0;", "double a = 2147483648.0;",
    "long a = -2147483648.0;", "long a = 2147483648.0;", "long a = -0.5;",
    "int a = -32768.7;", "float a = 1.1;", "double a = .5;",
    "double a = 1.00000000000000000000000000000000000000000000000000000000000001;",
    "long a = 40000;", "double a = 65535;", "long a = 32768;", "int a = 32767;",
    "unsigned a = 65535;", "long a = -40000;",
]


def run_native(root, name, text):
    with open(os.path.join(root, name + ".i"), "wb") as f:
        f.write(text.replace("\n", "\r").encode())
    for x in os.listdir(root):
        if x.startswith("cstr"):
            os.remove(os.path.join(root, x))
    p = subprocess.run([sys.executable, os.path.join(HERE, "os9run.py"), "--root", root,
                        NATIVE, name + ".i", f"-o={name}.a"], capture_output=True)
    out = os.path.join(root, name + ".a")
    body = open(out, "rb").read().replace(b"\r", b"\n") if os.path.exists(out) else b""
    if os.path.exists(out):
        os.remove(out)
    return p.returncode, body


def run_host(root, name, text):
    src = os.path.join(root, name + ".c")
    with open(src, "w") as f:
        f.write(text)
    out = os.path.join(root, name + ".h.a")
    p = subprocess.run([HOST, src, f"-o={out}"], capture_output=True)
    body = open(out, "rb").read() if os.path.exists(out) else b""
    if os.path.exists(out):
        os.remove(out)
    return p.returncode, body


def code(body):
    """The data lines: no titles (the Linux build adds file and line)."""
    return [l for l in body.split(b"\n") if not l.startswith(b" ttl")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=4000)
    ap.add_argument("--batch", type=int, default=200)
    ap.add_argument("--seed", type=int, default=18)
    ap.add_argument("--keep", action="store_true")
    o = ap.parse_args()
    env_root = tempfile.mkdtemp(prefix="fltcheck.")
    os.makedirs(os.path.join(env_root, "SYS"))
    with open(MSG, "rb") as f, open(os.path.join(env_root, "SYS", "dcc68.msg"), "wb") as g:
        g.write(f.read().replace(b"\n", b"\r"))
    os.environ.pop("LD_PRELOAD", None)
    rng = random.Random(o.seed)
    bad = 0
    jobs = [(f"b{k}", batch(rng, o.batch)) for k in range(o.count // o.batch)]
    jobs += [(f"e{k}", t + "\n") for k, t in enumerate(EDGES)]
    for name, text in jobs:
        print(name, end=" ", flush=True)
        nrc, nbody = run_native(env_root, name, text)
        hrc, hbody = run_host(env_root, name, text)
        same = nrc == hrc and code(nbody) == code(hbody)
        if name.startswith("b") and nrc:
            print(f"{name}: native dcc68 exit {nrc}: the batch is not a valid test")
        if not same:
            bad += 1
            print(f"{name}: differs (exit native {nrc}, Linux {hrc})")
            nl, hl = code(nbody), code(hbody)
            src = text.split("\n")
            shown = 0
            for i in range(max(len(nl), len(hl))):
                a = nl[i] if i < len(nl) else b""
                b = hl[i] if i < len(hl) else b""
                if a != b and shown < 6:
                    print(f"    native {a.decode()!r}\n    Linux  {b.decode()!r}")
                    shown += 1
            if name.startswith("e"):
                print(f"    source: {text.strip()}")
        elif name.startswith("e"):
            print(f"{name}: same (exit {nrc}): {text.strip()}")
    n = len(jobs)
    print(f"{n - bad} of {n} files the same ({o.count} generated declarations, "
          f"{len(EDGES)} edge cases)")
    if not o.keep:
        subprocess.run(["rm", "-rf", env_root])
    else:
        print("kept", env_root)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
