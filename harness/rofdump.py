#!/usr/bin/env python3
"""rofdump - print a ROF (Microware relocatable object) file readably.

A ROF file (or a library, which is ROF files one after another) holds, per
module: a header, the global symbols it defines, its code and initialised
data, the external symbols it references (with where), and its local
references (fixups). Format as in rlink's rof.h.

Usage: python3 harness/rofdump.py file.r [--code]
"""
import sys

SYNC = b"\x62\xcd\x23\x87"


def word(d, i):
    return (d[i] << 8) | d[i + 1], i + 2


def name(d, i):
    j = d.index(0, i)
    return d[i:j].decode("latin-1"), j + 1


def flags_def(f):
    kind = {0: "data", 1: "idata", 2: "dp-data", 3: "dp-idata", 4: "code",
            5: "set", 6: "const", 7: "common"}.get(f & 7, f"?{f:02x}")
    return kind


def flags_ref(f):
    parts = []
    parts.append("code" if f & 0x20 else ("dp" if f & 0x10 else "data"))
    if f & 0x08:
        parts.append("8-bit")
    if f & 0x40:
        parts.append("negate")
    if f & 0x80:
        parts.append("pc-relative")
    return " ".join(parts)


def dump(path, show_code=False):
    d = open(path, "rb").read()
    i = 0
    n = 0
    while i < len(d):
        if d[i:i + 4] != SYNC:
            print(f"  (stray bytes at {i}: {d[i:i+8].hex()})")
            break
        n += 1
        tylan, attrev = d[i + 4], d[i + 5]
        valid = d[i + 6]
        ed, ver = d[i + 12], d[i + 13]
        udat, _ = word(d, i + 14)
        udpd, _ = word(d, i + 16)
        idat, _ = word(d, i + 18)
        idpd, _ = word(d, i + 20)
        code, _ = word(d, i + 22)
        stack, _ = word(d, i + 24)
        entry, _ = word(d, i + 26)
        i += 28
        mname, i = name(d, i)
        print(f"psect {mname!r}: type/lang ${tylan:02X} attr/rev ${attrev:02X} edition {ed} "
              f"stack {stack} entry ${entry:04X} {'ERRORS' if valid else ''}")
        print(f"  code {code}, data {udat}, dp-data {udpd}, idata {idat}, dp-idata {idpd}")
        count, i = word(d, i)
        print(f"  {count} global(s)")
        for _ in range(count):
            s, i = name(d, i)
            f = d[i]
            off, i = word(d, i + 1)
            print(f"    {s:24s} {flags_def(f):8s} ${off:04X}")
        codebytes = d[i:i + code + idpd + idat]
        if show_code:
            for k in range(0, len(codebytes), 16):
                print(f"    {k:04X}: {codebytes[k:k+16].hex(' ')}")
        i += code + idpd + idat
        count, i = word(d, i)
        print(f"  {count} external symbol(s) referenced")
        for _ in range(count):
            s, i = name(d, i)
            nref, i = word(d, i)
            refs = []
            for _ in range(nref):
                f = d[i]
                off, i = word(d, i + 1)
                refs.append(f"${off:04X} ({flags_ref(f)})")
            print(f"    {s:24s} " + ", ".join(refs))
        count, i = word(d, i)
        print(f"  {count} local fixup(s)")
        for _ in range(count):
            f = d[i]
            off, i = word(d, i + 1)
            print(f"    at ${off:04X}  {flags_ref(f)} -> {flags_def(f)}")
        count, i = word(d, i)   # common blocks
        for _ in range(count):
            s, i = name(d, i)
            size, i = word(d, i)
            nref, i = word(d, i)
            i += 3 * nref
            print(f"  common {s} size {size}, {nref} refs")
    return n


if __name__ == "__main__":
    for p in sys.argv[1:]:
        if not p.startswith("--"):
            dump(p, "--code" in sys.argv)
