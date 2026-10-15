#!/usr/bin/env python3
"""topslot - does a native tool's module fill the top memory slot? (D61)

las, llink, dcc68 and dco68 map RAM blocks with F$MapBlk, which gives
the highest free slot of the process's 64K. NitrOS9 maps a module into
the top slots, and the last 768 bytes ($FD00-$FFFF) are the system's,
so a module of n blocks starts at $10000 - n * 8K and holds at most
n * 8K - 768 bytes. If it doesn't reach $E000, the top slot stays free,
a window lands on the system page at $FE00, and the machine hangs at the
next interrupt (D51, docs/nitros9-fixes.md #5): that is a module of
15,617 to 16,384 bytes, or 7,425 to 8,192, and so on.

dco68 checks for itself, more strictly, that its code (etext) reaches
$E000 (topslot.c); a module whose map has `topslot` gets that check too.

    topslot.py MODULE[:MAP] ...     non-zero exit if any fails
"""

import re
import sys

BLK, SYSPAGE = 8192, 768


def check(module, mapfile):
    data = open(module, "rb").read()
    size = (data[2] << 8) | data[3]
    n = (size + SYSPAGE + BLK - 1) // BLK
    base = 0x10000 - n * BLK
    problems = []
    if base + size <= 0xE000:
        problems.append(f"ends at ${base + size:04X}, below $E000: "
                        f"make it at most {(n - 1) * BLK - SYSPAGE:,} bytes or at least "
                        f"{(n - 1) * BLK + 1:,}")
    note = ""
    if mapfile:
        text = open(mapfile).read()
        if re.search(r"^\s+topslot\s+code", text, re.M):
            etext = int(re.search(r"^\s+etext\s+code\s+([0-9a-f]+)", text, re.M).group(1), 16)
            spare = base + etext - 0xE000
            note = f", etext ${base + etext:04X} ({spare:+,} over $E000)"
            if spare < 0:
                problems.append(f"etext ${base + etext:04X} is below $E000: "
                                f"add {-spare:,} zeros or more to topslot.c")
    room = n * BLK - SYSPAGE - size
    print(f"topslot: {module}: {size:,} bytes in {n} blocks at ${base:04X}, "
          f"{room:,} to spare{note}" + ("" if not problems else " -- " + "; ".join(problems)))
    return not problems


def main():
    ok = True
    for arg in sys.argv[1:]:
        module, _, mapfile = arg.partition(":")
        ok &= check(module, mapfile or None)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
