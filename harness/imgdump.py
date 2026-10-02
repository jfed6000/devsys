#!/usr/bin/env python3
"""imgdump - print a las defs image (--defsimg=FILE, las/defsimg.c).

  imgdump.py FILE          summary: what it needs, files, counters
  imgdump.py FILE --all    also every symbol (S)
  imgdump.py FILE --6809   an image native las wrote (big-endian, 16-bit
                           ints); the default is one from Linux las

Values are in the byte order of the machine that wrote the image: on
Linux a symbol's value and the G counters are 4-byte little-endian ints;
on the 6809 a value is a 4-byte big-endian long and a counter 2 bytes.
"""
import sys


class Reader:
    def __init__(self, data, native):
        self.d, self.i = data, 0
        self.order = "big" if native else "little"
        self.intsize = 2 if native else 4

    def byte(self):
        b = self.d[self.i]
        self.i += 1
        return b

    def num(self, size):
        v = int.from_bytes(self.d[self.i:self.i + size], self.order, signed=True)
        self.i += size
        return v

    def name(self):
        j = self.d.index(0, self.i)
        s = self.d[self.i:j].decode("latin-1")
        self.i = j + 1
        return s

    def sym(self):
        typ, msk = self.byte(), self.byte()
        return typ, msk, self.num(4), self.name()


def main():
    path = sys.argv[1]
    every = "--all" in sys.argv
    r = Reader(open(path, "rb").read(), "--6809" in sys.argv)
    if r.d[:4] != b"LASI":
        sys.exit("not a defs image")
    done, ver, opts = chr(r.d[4]), r.d[5], r.d[6]
    r.i = 7
    print(f"{path}: {'complete' if done == '1' else 'UNFINISHED'}, version {ver}, "
          f"options {opts:#04x}, for 'use {r.name()}'")
    counts = {}
    while r.i < len(r.d):
        tag = chr(r.byte())
        counts[tag] = counts.get(tag, 0) + 1
        if tag == "P":
            t, m, v, n = r.sym()
            print(f"  defined before:  {n:12} typ {t:02x} msk {m:02x} value {v:#x}")
        elif tag == "F":
            use, p = r.name(), r.name()
            st = r.d[r.i:r.i + 9]
            r.i += 9
            print(f"  file  {use:12} {p}  stamp {st.hex()}")
        elif tag == "T":
            kind = chr(r.byte())
            print(f"  {'nam' if kind == 'n' else 'ttl'}   {r.name()!r}")
        elif tag == "G":
            a0, a1, dn, lm, dl, dg, di, nc = (r.num(r.intsize) for _ in range(8))
            bits = r.d[r.i:r.i + (nc + 7) // 8]
            r.i += (nc + 7) // 8
            res = "".join(str(bits[k >> 3] >> (k & 7) & 1) for k in range(nc))
            print(f"  counter {a0:#x} -> {a1:#x}; blocks +{dn} (local={'new' if lm else 'same'});"
                  f" lines +{dl}; globals +{dg}; if count +{di}")
            print(f"  {nc} conditionals: {res}")
        elif tag in "SZ":
            t, m, v, n = r.sym()
            if every or tag == "Z":
                print(f"  {'symbol' if tag == 'S' else 'pass 2'}  {n:20} typ {t:02x} msk {m:02x} value {v:#x}")
        elif tag == "E":
            break
        else:
            sys.exit(f"bad record {tag!r} at {r.i - 1}")
    print("  records: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))


if __name__ == "__main__":
    main()
