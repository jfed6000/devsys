#!/usr/bin/env python3
"""imgcheck - does a defs image (las --defsimg, phase 9) change anything?

For every lwdiff job (the NitrOS9 Level 2 build's 290 las commands) it runs
las three times: without an image, then with --defsimg (the first run
records the image, the second loads it). All three must give the same
output file, the same messages and exit status, and the same symbol table
(-s; the listing's lines differ, since a loaded image lists no defs lines).

  --share one     every job uses one image file, in job order: a stress
                  test of the checks (most loads find another configuration
                  or other files and must fall back to reading the defs)
  --share config  (default) one image per configuration, as a user would:
                  same includes, -D names and values, pragmas
  --las PATH      the las to test (default las/build/unix/las)
  --only NAME     jobs whose id contains NAME
  --variants      instead: cases built around echo.asm that must not load
                  the image (changed -D, a touched defs file, a source that
                  changes a defs symbol...) or must load it

Prints how each job used the image (las's LASIMG_TRACE) and any difference.
"""
import argparse
import collections
import concurrent.futures
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

JOBS = os.path.join(HERE, "out", "l2-jr2", "jobs.json")
GEN = os.path.join(HERE, "out", "l2-jr2", "generated")
WORK = os.path.join(HERE, "out", "imgcheck")


def argv_for(job, las, out, img=None):
    incs = [os.path.dirname(job["source"])] + job["includes"] + [GEN]
    a = ([las, job["source"], "--lwasm"] + [f"-D{d}" for d in job["defines"]]
         + ([f"--pragma={','.join(job['pragmas'])}"] if job["pragmas"] else [])
         + [f"-i={d}" for d in incs] + [f"-o={out}", "-s"])
    if img:
        a.append(f"--defsimg={img}")
    return a


def symtab(text):
    i = text.find("\nSymbol Table\n")
    return text[i:] if i >= 0 else ""


def run(job, las, out, img, rundir):
    env = dict(lwdiff.clean_env(), LASIMG_TRACE="1")
    if os.path.exists(out):
        os.remove(out)
    p = subprocess.run(argv_for(job, las, out, img), cwd=rundir, env=env,
                       capture_output=True, timeout=600)
    err = p.stderr.decode("latin-1")
    trace = [l[9:] for l in err.splitlines() if l.startswith("defsimg: ")]
    err = "\n".join(l for l in err.splitlines() if not l.startswith("defsimg: "))
    data = open(out, "rb").read() if os.path.exists(out) else None
    return {"rc": p.returncode, "err": err, "data": data,
            "sym": symtab(p.stdout.decode("latin-1")), "trace": trace}


def titles(text):
    """The last page header's title line (nam - ttl)."""
    lines = text.splitlines()
    t = [lines[i + 1] for i, l in enumerate(lines[:-1]) if " Page " in l and l.startswith("las ")]
    return t[-1] if t else ""


def variants(las):
    """Each case: what to do before the run, the run's options and source,
    and how the image must be used. Every run is compared with the same
    source and options assembled without an image."""
    import time
    d = os.path.join(WORK, "var")
    inc = os.path.join(d, "inc")
    os.makedirs(inc)
    n9 = os.path.join(DEVSYS, "corpus", "nitros9")
    shutil.copy(os.path.join(n9, "level1/wildbits/defsfile"), inc)
    for f in ("os9.d", "scf.d", "rbf.d", "wildbits.d"):
        shutil.copy(os.path.join(n9, "defs", f), inc)
    echo = open(os.path.join(n9, "level1/cmds/echo.asm"), encoding="latin-1").read()
    before, use, after = echo.partition("                    use       defsfile\n")
    assert use
    # a page header just after the defs shows the titles they left
    after = "                    pag\n" + after
    echo = before + use + after
    nonam = "".join(l for l in echo.splitlines(True) if l.split()[:1] != ["nam"])
    base = ["--lwasm", "--pragma=pcaspcr,nosymbolcase,condundefzero,undefextern,"
            "dollarnotlocal,noforwardrefmax", "-Dwildbits=1", f"-i={inc}", "-l", "-s"]
    img = os.path.join(d, "img")

    def touch(path):
        t = os.path.getmtime(path) + 120
        os.utime(path, (t, t))

    cases = [
        # (what, before, extra options, source, expected trace)
        ("record", None, [], echo, "missing or out of date; recorded"),
        ("load", None, [], echo, "loaded; pass 2 loaded"),
        ("-DH6309: the defs created H6309", None, ["-DH6309=1"], echo, "other configuration"),
        ("record with -DH6309=1", lambda: os.remove(img), ["-DH6309=1"], echo,
         "missing or out of date; recorded"),
        ("load with -DH6309=1", None, ["-DH6309=1"], echo, "loaded; pass 2 loaded"),
        ("-DH6309=0: a value the defs read", None, ["-DH6309=0"], echo, "other configuration"),
        ("no H6309", None, [], echo, "other configuration"),
        ("record again", lambda: os.remove(img), [], echo, "missing or out of date; recorded"),
        ("os9.d touched", lambda: touch(os.path.join(inc, "os9.d")), [], echo,
         "missing or out of date; recorded"),
        ("after the touch", None, [], echo, "loaded; pass 2 loaded"),
        ("source changes a defs symbol after the use", None, [],
         before + use + "OS9.D set 2\n" + after, "loaded; pass 2 read"),
        ("source defines a defs name before the use", None, [],
         before + "MappedIOStart equ $FE00\n" + use + after, "other configuration"),
        ("no nam before the use: the defs' nam is the title", None, [], nonam,
         "loaded; pass 2 loaded"),
        ("--defs: no image", None, [f"--defs={os.path.join(d, 'none.r')}"], echo, ""),
    ]
    open(os.path.join(d, "none.r"), "wb").close()
    bad = 0
    env = dict(lwdiff.clean_env(), LASIMG_TRACE="1")
    for i, (what, pre, extra, src, want) in enumerate(cases):
        if pre:
            pre()
        sp = os.path.join(d, f"v{i}.asm")
        open(sp, "w", encoding="latin-1").write(src)
        res = []
        for with_img in (False, True):
            out = os.path.join(d, f"v{i}{'i' if with_img else 'p'}")
            argv = [las, sp] + base + extra + [f"-o={out}"] + ([f"--defsimg={img}"] if with_img else [])
            p = subprocess.run(argv, cwd=d, env=env, capture_output=True)
            text = p.stdout.decode("latin-1")
            err = p.stderr.decode("latin-1").splitlines()
            res.append({"rc": p.returncode, "data": open(out, "rb").read() if os.path.exists(out) else None,
                        "sym": symtab(text), "title": titles(text),
                        "err": [l for l in err if not l.startswith("defsimg: ")],
                        "trace": "; ".join(l[9:] for l in err if l.startswith("defsimg: "))})
        diff = [k for k in ("rc", "data", "sym", "title", "err") if res[0][k] != res[1][k]]
        ok = not diff and res[1]["trace"] == want
        bad += not ok
        print(f"  {'ok ' if ok else 'BAD'} {what}: {res[1]['trace'] or '(no image)'}"
              + (f"  (wanted: {want})" if res[1]["trace"] != want else "")
              + (f"  differs: {', '.join(diff)}" if diff else ""))
    print(f"variants: {len(cases) - bad} of {len(cases)} as expected")
    return 1 if bad else 0


def config_key(job):
    k = json.dumps([[os.path.dirname(job["source"])] + job["includes"],
                    sorted(job["defines"]), job["pragmas"]])
    return hashlib.sha1(k.encode()).hexdigest()[:12]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--las", default=os.path.join(DEVSYS, "las/build/unix/las"))
    ap.add_argument("--share", choices=("one", "config"), default="config")
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("-j", type=int, default=os.cpu_count())
    ap.add_argument("--variants", action="store_true")
    a = ap.parse_args()
    a.las = os.path.abspath(a.las)
    if a.variants:
        shutil.rmtree(WORK, ignore_errors=True)
        return variants(a.las)

    jobs = json.load(open(JOBS))
    if a.only:
        jobs = [j for j in jobs if any(o in j["id"] for o in a.only)]
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(os.path.join(WORK, "img"))

    def one(job):
        d = os.path.join(WORK, "run", job["id"])
        os.makedirs(d, exist_ok=True)
        img = os.path.join(WORK, "img", "shared" if a.share == "one" else config_key(job))
        plain = run(job, a.las, os.path.join(d, "plain"), None, d)
        rec = run(job, a.las, os.path.join(d, "rec"), img, d)
        load = run(job, a.las, os.path.join(d, "load"), img, d)
        bad = []
        for name, r in (("record", rec), ("load", load)):
            for field in ("rc", "err", "data", "sym"):
                if r[field] != plain[field]:
                    bad.append(f"{name}: {field} differs")
        return job, rec["trace"], load["trace"], bad

    if a.share == "one":
        results = [one(j) for j in jobs]            # in order: one file
    else:
        # jobs of one configuration share a file: run each group in order
        groups = collections.defaultdict(list)
        for j in jobs:
            groups[config_key(j)].append(j)
        with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
            parts = ex.map(lambda g: [one(j) for j in g], groups.values())
            results = [r for p in parts for r in p]

    outcome = collections.Counter()
    ids = collections.defaultdict(list)
    nbad = 0
    for job, rt, lt, bad in results:
        k = "; ".join(rt) + "  |  " + "; ".join(lt)
        outcome[k] += 1
        ids[k].append(job["id"])
        if bad:
            nbad += 1
            print(f"  {job['id']}: {', '.join(bad)}")
    print(f"imgcheck ({a.share}): {len(results) - nbad} of {len(results)} jobs identical "
          f"with and without the image")
    print("how the image was used (record run | load run):")
    for k, v in outcome.most_common():
        print(f"  {v:4}  {k.strip(' |') or '(no top-level use)'}"
              + (f"  ({', '.join(ids[k])})" if v <= 8 else ""))
    return 1 if nbad else 0


if __name__ == "__main__":
    sys.exit(main())
