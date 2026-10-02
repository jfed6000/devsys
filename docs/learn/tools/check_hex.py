#!/usr/bin/env python3
"""Check that every annotated hex dump in toolchain.html matches its real file.

Each dump in the page's <script> is a `var <name>Segs = [...]` array of
["HEX", class, label, description] entries. HEX_DUMPS maps each array to the
file it was taken from (relative to docs/learn/examples/out/). Add an entry
here whenever you add a dump to the page. IGNORE lists byte ranges that change
on every build (timestamps) and are skipped in the comparison.

Usage: python3 docs/learn/tools/check_hex.py   (run build_examples.sh first)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, "..", "toolchain.html")
OUT = os.path.join(HERE, "..", "examples", "out")

HEX_DUMPS = {
    "moduleSegs": "hello",
    "rofSegs": "sum.r",
}

# (start, end) byte offsets, end exclusive
IGNORE = {
    "rofSegs": [(7, 12)],  # ROF header h_date: date/time assembled
}


def mask(hexstr, ranges):
    b = bytearray.fromhex(hexstr)
    for start, end in ranges:
        b[start:end] = bytes(end - start)
    return b.hex().upper()


def segments(js, name):
    block = js[js.index("var " + name):]
    block = block[:block.index("];")]
    hexes = re.findall(r'\[\s*"([0-9A-Fa-f ]+)"', block)
    return "".join(h.replace(" ", "") for h in hexes).upper()


def main():
    src = open(PAGE).read()
    ok = True
    for name, fname in HEX_DUMPS.items():
        path = os.path.join(OUT, fname)
        if not os.path.exists(path):
            print(f"{name}: missing {path} (run build_examples.sh)")
            ok = False
            continue
        ranges = IGNORE.get(name, [])
        got = mask(segments(src, name), ranges)
        want = mask(open(path, "rb").read().hex(), ranges)
        if got == want:
            print(f"{name}: MATCH ({len(want) // 2} bytes, {fname})")
        else:
            ok = False
            print(f"{name}: DIFF against {fname}\n  page: {got}\n  file: {want}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
