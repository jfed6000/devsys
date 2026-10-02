#!/usr/bin/env python3
"""Recompute the hashing figures used in toolchain.html chapter 9.

Reads the Level 2 F256 defs (os9.d, scf.d, rbf.d, wildbits.d, the files
level2/wildbits/defsfile uses) from corpus/nitros9/defs, takes every label in
the order the files define it, and models las's symbol table: character-sum
hash into 64 buckets, an unbalanced binary tree per bucket (las/symtab.c).

This is a model, not a run of las: it includes labels that conditional
assembly might skip, and compares names exactly as written.

Usage: python3 docs/learn/tools/hash_data.py [--json]
"""
import json
import os
import re
import sys
import zlib
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
DEFS = os.path.join(HERE, "..", "..", "..", "corpus", "nitros9", "defs")
FILES = ["os9.d", "scf.d", "rbf.d", "wildbits.d"]
BUCKETS = 64
EXAMPLE_BUCKET = 25
SEARCH_EXAMPLE = "PE.FilNm"


def names():
    out, seen = [], set()
    for f in FILES:
        for line in open(os.path.join(DEFS, f), encoding="latin-1"):
            m = re.match(r"^([A-Za-z_.@$][A-Za-z0-9_.@$]*)", line)
            if m and m.group(1) not in seen:
                seen.add(m.group(1))
                out.append(m.group(1))
    return out


def charsum(n):
    return sum(map(ord, n))


def build(ns, nb, h):
    trees = {}
    for n in ns:
        k = h(n) % nb
        if k not in trees:
            trees[k] = {"name": n, "l": None, "r": None}
            continue
        t = trees[k]
        while True:
            side = "l" if n < t["name"] else "r"
            if t[side] is None:
                t[side] = {"name": n, "l": None, "r": None}
                break
            t = t[side]
    return trees


def cost(trees, total):
    depths = []

    def walk(t, d):
        if t:
            depths.append(d)
            walk(t["l"], d + 1)
            walk(t["r"], d + 1)
    for t in trees.values():
        walk(t, 1)
    return sum(depths) / total, max(depths)


def main():
    ns = names()
    sizes = [0] * BUCKETS
    for n in ns:
        sizes[charsum(n) & (BUCKETS - 1)] += 1
    sums = defaultdict(list)
    for n in ns:
        sums[charsum(n)].append(n)
    shared = [v for v in sums.values() if len(v) > 1]

    hashed = build(ns, BUCKETS, charsum)
    avg, worst = cost(hashed, len(ns))
    one_avg, one_worst = cost(build(ns, 1, lambda n: 0), len(ns))
    crc_avg, crc_worst = cost(build(ns, BUCKETS, lambda n: zlib.crc32(n.encode())), len(ns))

    # The example bucket's tree, with depth and in-order position for drawing.
    nodes = []

    def walk(t, d, parent):
        if t:
            walk(t["l"], d + 1, t["name"])
            nodes.append({"name": t["name"], "depth": d, "parent": parent})
            walk(t["r"], d + 1, t["name"])
    walk(hashed[EXAMPLE_BUCKET], 1, None)
    path, t = [], hashed[EXAMPLE_BUCKET]
    while t:
        path.append(t["name"])
        if t["name"] == SEARCH_EXAMPLE:
            break
        t = t["l"] if SEARCH_EXAMPLE < t["name"] else t["r"]

    data = {
        "names": len(ns),
        "bucket_sizes": sizes,
        "smallest": min(sizes), "largest": max(sizes),
        "mean": round(len(ns) / BUCKETS, 1),
        "empty_buckets": sizes.count(0),
        "avg_compares": round(avg, 1), "worst_compares": worst,
        "one_tree_avg": round(one_avg, 1), "one_tree_worst": one_worst,
        "crc_avg": round(crc_avg, 1), "crc_worst": crc_worst,
        "linear_avg": round((len(ns) + 1) / 2),
        "names_sharing_a_sum": sum(len(v) for v in shared),
        "sum_groups": len(shared),
        "examples": {n: {"sum": charsum(n), "bucket": charsum(n) & 63}
                     for n in ["F$Exit", "MMU_SLOT_0", "MMU_SLOT_1", "MMU_SLOT_7",
                               "F$Link", "SS.DRAM", "E$Sect", "WIZ_MR"]},
        "example_bucket": EXAMPLE_BUCKET,
        "example_bucket_insert_order": [n for n in ns if charsum(n) & 63 == EXAMPLE_BUCKET],
        "example_tree_inorder": nodes,
        "search_example": SEARCH_EXAMPLE,
        "search_path": path,
    }
    if "--json" in sys.argv:
        print(json.dumps(data, indent=1))
    else:
        for k, v in data.items():
            if k not in ("bucket_sizes", "example_tree_inorder"):
                print(f"{k}: {v}")
        print("bucket_sizes:", data["bucket_sizes"])


if __name__ == "__main__":
    main()
