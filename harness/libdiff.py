#!/usr/bin/env python3
"""libdiff - compare two ROF libraries (or objects) member by member.

Step 18: devsys builds the C libraries itself (clib/), where CoCoC's were
built by tools that kept 8 characters of a name. This says, per member
(psect), whether the two sides hold the same thing: header values,
definitions, code and initialised data, references and fixups.

  python3 harness/libdiff.py A.l B.l [--names8] [--verbose]

  --names8   cut every name to 8 characters on both sides first, and drop
             definitions that are only an 8-character alias of another (as
             CoCoC's intercept_a defines both intercept and intercep): then
             "same" means the code is the same and only names grew
  --verbose  print what differs in each differing member

Members are matched by psect name; the order of the library is compared
too (llink takes members in order). A member's references are compared as
sets of (name, offset, flags), so their order in the file doesn't matter.
Exit status 0 when every member is the same.
"""
import argparse
import sys

SYNC = b"\x62\xcd\x23\x87"


def word(d, i):
    return (d[i] << 8) | d[i + 1], i + 2


def name(d, i):
    j = d.index(0, i)
    return d[i:j].decode("latin-1"), j + 1


def members(path):
    """[(psect name, {fields})] in file order."""
    d = open(path, "rb").read()
    out = []
    i = 0
    while i < len(d):
        if d[i:i + 4] != SYNC:
            raise SystemExit(f"{path}: not a ROF at offset {i}")
        h = d[i + 4:i + 28]
        ver = d[i + 13]
        code, _ = word(d, i + 22)
        idat, _ = word(d, i + 18)
        idpd, _ = word(d, i + 20)
        i += 28
        mname, i = name(d, i)
        m = {"header": h[0:3] + h[8:9] + h[10:], "version": ver}   # not the date or ROF version
        count, i = word(d, i)
        defs = []
        for _ in range(count):
            s, i = name(d, i)
            f = d[i]
            off, i = word(d, i + 1)
            defs.append((s, f, off))
        m["defs"] = defs
        m["body"] = d[i:i + code + idpd + idat]
        i += code + idpd + idat
        count, i = word(d, i)
        refs = []
        for _ in range(count):
            s, i = name(d, i)
            nref, i = word(d, i)
            for _ in range(nref):
                f = d[i]
                off, i = word(d, i + 1)
                refs.append((s, f, off))
        m["refs"] = refs
        count, i = word(d, i)
        fix = []
        for _ in range(count):
            fix.append(d[i:i + 3])
            i += 3
        m["fixups"] = sorted(fix)
        count, i = word(d, i) if ver > 0 else (0, i)
        for _ in range(count):
            s, i = name(d, i)
            size, i = word(d, i)
            nref, i = word(d, i)
            i += 3 * nref
        out.append((mname, m))
    return out


def norm(m, names8):
    cut = (lambda s: s[:8]) if names8 else (lambda s: s)
    defs = {(cut(s), f, off) for s, f, off in m["defs"]}
    return {"header": m["header"], "defs": defs, "body": m["body"],
            "refs": {(cut(s), f, off) for s, f, off in m["refs"]},
            "fixups": m["fixups"]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--names8", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    o = ap.parse_args()
    cut = (lambda s: s[:8]) if o.names8 else (lambda s: s)
    A = [(cut(n), m) for n, m in members(o.a)]
    B = [(cut(n), m) for n, m in members(o.b)]
    da, db = dict(A), dict(B)
    same = differ = 0
    for n, ma in A:
        if n not in db:
            print(f"only in A: {n}")
            continue
        x, y = norm(ma, o.names8), norm(db[n], o.names8)
        what = [k for k in x if x[k] != y[k]]
        if not what:
            same += 1
            continue
        differ += 1
        print(f"differs:   {n}: {', '.join(what)}")
        if o.verbose:
            for k in what:
                if k in ("defs", "refs"):
                    print(f"    A only: {sorted(x[k] - y[k])}")
                    print(f"    B only: {sorted(y[k] - x[k])}")
                elif k == "body":
                    print(f"    sizes {len(x[k])} / {len(y[k])}")
                else:
                    print(f"    A {x[k]!r}\n    B {y[k]!r}")
    for n, _ in B:
        if n not in da:
            print(f"only in B: {n}")
    order_a = [n for n, _ in A if n in db]
    order_b = [n for n, _ in B if n in da]
    if order_a != order_b:
        print("member order differs")
    print(f"{same} same, {differ} differ, {len(A) - same - differ} only in A, "
          f"{len([n for n, _ in B if n not in da])} only in B")
    return 0 if same == len(A) == len(B) and order_a == order_b else 1


if __name__ == "__main__":
    sys.exit(main())
