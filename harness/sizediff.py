#!/usr/bin/env python3
"""sizediff - find instructions las and lwasm encode at different lengths.

For each DIFF file in the last lwdiff run (or the files named), assembles
with lwasm and las to listings, lines the listings up by source text, and
reports every instruction whose encoding has a different length, with the
addressing form and whether its label operand is defined before or after
the instruction (a forward reference).

Usage: python3 harness/sizediff.py [id ...] [--summary] [--work DIR]
(--work: the lwdiff work directory, as given to lwdiff.py --work)
"""
import collections
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

WORK = os.path.join(HERE, "out", "l2-jr2")
S = os.path.join(HERE, "out", "sizediff")


def listings(job):
    os.makedirs(S, exist_ok=True)
    lst = os.path.join(S, job["id"] + ".lw.lst")
    argv = [lwdiff.DEFAULT_LWASM] + [a for a in job["argv"][1:] if not a.startswith("-o")
                                     and not a.startswith("--output")]
    argv += ["-I" + os.path.join(WORK, "generated"), "--list=" + lst, "--output=/dev/null"]
    subprocess.run(argv, cwd=job["cwd"], capture_output=True, env=lwdiff.clean_env())
    log = os.path.join(WORK, "las", job["id"] + ".log")
    cmd = open(log).readline()[2:].strip()
    cmd = re.sub(r"-o=\S+", "-o=/dev/null -l", cmd)
    las = subprocess.run(cmd, shell=True, capture_output=True, text=True,
                         env=lwdiff.clean_env(), cwd=S).stdout
    # Both listings print the source line with its own spacing at a fixed
    # place: lwasm after "(file):NNNNN ", las from column 22.
    lw = []
    for l in open(lst, errors="replace"):
        m = re.match(r"^([0-9A-F]{4}) ([0-9A-F]+)\s+\(.*?\):\d+ (.*)$", l.rstrip("\n"))
        if m:
            lw.append((int(m.group(1), 16), m.group(2).lower(), m.group(3)))
    la = []
    for l in las.splitlines():
        m = re.match(r"^\d{5} ([0-9a-f]{4})[ =]([0-9a-f]+)", l)
        if m and len(l) > 22 and l[21] == " ":
            la.append((int(m.group(1), 16), m.group(2), l[22:]))
    return lw, la


def fields(text):
    t = text.split()
    if not t:
        return None, None, None
    if text[:1].isspace():
        return None, t[0].lower(), (t[1] if len(t) > 1 else "")
    return t[0], (t[1].lower() if len(t) > 1 else None), (t[2] if len(t) > 2 else "")


def classify(op, operand):
    o = operand.lower()
    if o.startswith("#"):
        return "immediate"
    if ",pcr" in o or ",pc" in o:
        return ("forced-8 " if o.startswith("<") else "forced-16 " if o.startswith(">") else "") + "pcr"
    if "," in o:
        return ("forced-8 " if o.startswith("<") else "forced-16 " if o.startswith(">") else "") + "indexed"
    if op and op.startswith("lb"):
        return "long branch"
    if op and op.startswith("b") and op not in ("bita", "bitb", "bitd", "bitmd"):
        return "branch"
    return ("forced-8 " if o.startswith("<") else "forced-16 " if o.startswith(">") else "") + "direct/extended"


def analyse(job):
    lw, la = listings(job)
    # where each label is defined (las listing addresses), to tell forward refs
    labels = {}
    for i, (addr, _, text) in enumerate(la):
        lab, _, _ = fields(text)
        if lab:
            labels.setdefault(lab.rstrip(":").lower(), i)
    out = []
    i = 0
    for k, (addr, b, text) in enumerate(la):
        lab, op, operand = fields(text)
        if not op:
            continue
        key = " ".join(text.split()[:3]).lower()
        while i < len(lw) and " ".join(lw[i][2].split()[:3]).lower() != key:
            i += 1
        if i >= len(lw):
            break
        lwlen = len(lw[i][1]) // 2
        laslen = len(b) // 2
        # listings may show only part of long data; compare instructions only
        if (lwlen != laslen and laslen <= 4 and lwlen <= 5 and op and
                op not in ("fcc", "fcs", "fcb", "fdb", "fcn", "fill", "mod", "emod", "rmb")):
            sym = re.findall(r"[A-Za-z_@.$][A-Za-z0-9_@.$?]*", operand.lstrip("<>#[").split(",")[0])
            fwd = None
            for s_ in sym:
                d = labels.get(s_.lower())
                if d is not None:
                    fwd = d > k
            out.append({"op": op, "operand": operand, "form": classify(op, operand),
                        "lwasm": lwlen, "las": laslen,
                        "ref": "forward" if fwd else "backward" if fwd is False else "other",
                        "lw_bytes": lw[i][1], "las_bytes": b})
        i += 1
    return out


def main():
    global WORK
    args = sys.argv[1:]
    if "--work" in args:
        k = args.index("--work")
        WORK = os.path.abspath(args[k + 1])
        del args[k:k + 2]
    ids = [a for a in args if not a.startswith("--")]
    jobs = {j["id"]: j for j in json.load(open(os.path.join(WORK, "jobs.json")))}
    if not ids:
        ids = [r["id"] for r in json.load(open(os.path.join(WORK, "results.json")))
               if r["status"] == "DIFF" or (r["format"] == "obj" and r["status"] == "ASSEMBLED")]
    total = collections.Counter()
    for jid in ids:
        rows = analyse(jobs[jid])
        if not rows:
            continue
        if "--summary" not in sys.argv:
            print(f"== {jid}")
            for r in rows[:8]:
                print(f"   {r['op']:6s} {r['operand'][:28]:28s} {r['form']:22s} {r['ref']:8s} "
                      f"lwasm {r['lwasm']} {r['lw_bytes']:10s} las {r['las']} {r['las_bytes']}")
        for r in rows:
            total[(r["form"], r["ref"], r["lwasm"] - r["las"])] += 1
    print("\nAll differing instructions: (form, label, lwasm bytes minus las bytes) -> count")
    for k, v in total.most_common():
        print(f"  {v:4d}  {k}")


if __name__ == "__main__":
    main()
