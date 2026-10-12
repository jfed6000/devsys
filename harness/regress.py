#!/usr/bin/env python3
"""regress - check that a change to las didn't change its behaviour.

Builds a reference las from a git revision (default: HEAD, the last commit)
and the current las from the working tree, runs both on the same inputs, and
compares everything they produce:

  - the object file (ROF), ignoring the assembly timestamp
  - the listing with symbol table (-l -s), ignoring the banner's date/time
  - stderr and the exit code

Inputs:
  rma    hand-written RMA-syntax sources (.a files containing psect) from
         corpus/ and ~/projects/dcc/CoCoC
  dcc    C sources from ~/projects/dcc/CoCoC/Source compiled to RMA assembly
         by devsys's DCC (dcc/: dcpp, dcc68, dco68); files that don't compile are
         skipped
  lwdiff the NitrOS9 Level 2 build commands from lwdiff.py (mostly errors
         today, but error output is behaviour too)

Use it for refactors that must not change behaviour (roadmap phases 1, 2, 8,
9): any difference is a failure. For intentional behaviour changes, use
lwdiff.py instead.

Usage:
    python3 harness/regress.py              # compare working tree with HEAD
    python3 harness/regress.py --ref 9b3c026
    python3 harness/regress.py --show 3     # print the first 3 differences
    ASAN_OPTIONS=detect_leaks=0 python3 harness/regress.py --current las/build/asan/las
                                            # memory-check build (las/build/asan)
"""
import argparse
import concurrent.futures
import glob
import os
import re
import shutil
import subprocess
import sys
import tarfile
import io

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
HOME = os.path.expanduser("~")
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

COCOC = os.path.join(HOME, "projects/dcc/CoCoC")
DCC = os.path.join(os.path.dirname(HERE), "dcc")    # devsys's fork (phase 6)
WORK = os.path.join(HERE, "out", "regress")
TIMEOUT = 30


def sh(argv, **kw):
    return subprocess.run(argv, env=lwdiff.clean_env(), capture_output=True, **kw)


# ---------------------------------------------------------------- builds

def build_reference(rev):
    """Build las from a git revision. Cached per commit."""
    commit = sh(["git", "-C", DEVSYS, "rev-parse", rev], text=True).stdout.strip()
    if not commit:
        sys.exit(f"unknown revision {rev}")
    root = os.path.join(WORK, "ref-" + commit[:12])
    binary = os.path.join(root, "las/build/unix/las")
    if not os.path.exists(binary):
        shutil.rmtree(root, ignore_errors=True)
        os.makedirs(root)
        tar = sh(["git", "-C", DEVSYS, "archive", commit, "las"]).stdout
        tarfile.open(fileobj=io.BytesIO(tar)).extractall(root)
        p = sh(["make", "-C", os.path.join(root, "las/build/unix")], text=True)
        if p.returncode or not os.path.exists(binary):
            sys.exit("reference build failed:\n" + p.stdout + p.stderr)
    return binary, commit


def build_current():
    p = sh(["make", "-C", os.path.join(DEVSYS, "las/build/unix")], text=True)
    binary = os.path.join(DEVSYS, "las/build/unix/las")
    if p.returncode or not os.path.exists(binary):
        sys.exit("las build failed:\n" + p.stdout + p.stderr)
    warnings = [l for l in (p.stdout + p.stderr).splitlines() if "warning" in l.lower()]
    return binary, warnings


# ---------------------------------------------------------------- inputs

def rma_inputs():
    """Hand-written RMA sources: .a files that contain a psect."""
    roots = [os.path.join(DEVSYS, "corpus"), COCOC]
    found = []
    for root in roots:
        for path in sorted(glob.glob(os.path.join(root, "**", "*.a"), recursive=True)):
            try:
                text = open(path, "rb").read().decode("latin-1")
            except OSError:
                continue
            if re.search(r"^\s*psect\b", text, re.M | re.I):
                found.append(path)
    jobs = []
    for path in found:
        d = os.path.dirname(path)
        jobs.append({"id": "rma/" + os.path.relpath(path, HOME), "cwd": d,
                     "args": [path, f"-i={d}", f"-i={os.path.join(COCOC, 'Defs')}"]})
    return jobs


def dcc_inputs():
    """C sources compiled to RMA assembly with the host DCC."""
    outdir = os.path.join(WORK, "dcc-devsys")   # not "dcc": CoCoC DCC's cache
    os.makedirs(outdir, exist_ok=True)
    defs = os.path.join(COCOC, "Defs")
    sources = sorted(glob.glob(os.path.join(COCOC, "Source", "**", "*.c"), recursive=True))

    def compile_one(src):
        rel = os.path.relpath(src, COCOC).replace("/", "__")[:-2]
        out = os.path.join(outdir, rel + ".a")
        if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(src):
            return out
        d = os.path.dirname(src)
        try:
            pre = sh([os.path.join(DCC, "CPP/dcpp"), f"-I={d}", f"-I={defs}", src],
                     cwd=outdir, timeout=TIMEOUT)
            if pre.returncode or not pre.stdout:
                return None
            ipath = os.path.join(outdir, rel + ".i")
            raw = os.path.join(outdir, rel + ".raw.a")
            with open(ipath, "wb") as f:
                f.write(pre.stdout)
            # dcc68 writes temporary files (cstr.*) to the current directory
            c = sh([os.path.join(DCC, "CC09/dcc68"), ipath, f"-o={raw}"], cwd=outdir, timeout=TIMEOUT)
            if c.returncode or not os.path.exists(raw):
                return None
            with open(raw, "rb") as fin, open(out, "wb") as fout:
                o = subprocess.run([os.path.join(DCC, "COpt/dco68")], stdin=fin, stdout=fout,
                                   stderr=subprocess.DEVNULL, env=lwdiff.clean_env(), timeout=TIMEOUT)
            if o.returncode:
                os.remove(out)
                return None
            return out
        except subprocess.TimeoutExpired:
            return None

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        outs = [o for o in ex.map(compile_one, sources) if o]
    return [{"id": "dcc/" + os.path.basename(o), "cwd": outdir, "args": [o]} for o in outs], len(sources)


def lwdiff_inputs():
    """The NitrOS9 build commands, translated to las arguments as lwdiff does."""
    nitros9 = lwdiff.DEFAULT_NITROS9
    workdir = os.path.join(WORK, "lwdiff")
    os.makedirs(workdir, exist_ok=True)
    collected = lwdiff.collect(nitros9, "l2", "jr2", workdir)
    gen = lwdiff.make_generated(workdir)   # after collect: runs its generator steps
    jobs = []
    for job in collected:
        args = [job["source"], "--lwasm"] + [f"-D{d}" for d in job["defines"]]
        if job["pragmas"]:
            args.append("--pragma=" + ",".join(job["pragmas"]))
        incs = [os.path.dirname(job["source"])] + job["includes"] + [gen]
        args += [f"-i={d}" for d in incs]
        jobs.append({"id": "lwdiff/" + job["id"], "cwd": job["cwd"], "args": args})
    return jobs


# ---------------------------------------------------------------- compare

BANNER = re.compile(r"^(.*?)\s+\d\d/\d\d/\d{4}\s+\d\d:\d\d\s")


def normalise_listing(text):
    """Blank out the date and time in page headers; the program name stays."""
    out = []
    for line in text.splitlines():
        m = BANNER.match(line)
        out.append(BANNER.sub(r"\1 <date> ", line) if m else line)
    return "\n".join(out)


def mask_rof(data):
    if len(data) >= 12 and data[:4] == b"\x62\xcd\x23\x87":
        data = bytearray(data)
        data[7:12] = bytes(5)  # h_date
        return bytes(data)
    return data


def run(binary, job, tag):
    out = os.path.join(WORK, "objs", tag, job["id"].replace("/", "__") + ".r")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    if os.path.exists(out):
        os.remove(out)
    argv = [binary] + job["args"] + ["-l", "-s", f"-o={out}"]
    # Each run gets its own empty directory: las writes its macro work file
    # (rma.tmp) to the current directory, so parallel runs in one directory
    # would overwrite each other's, and we don't want it in source trees.
    # Sources and use files are found through absolute paths and -i.
    rundir = os.path.join(WORK, "run", tag, job["id"].replace("/", "__"))
    shutil.rmtree(rundir, ignore_errors=True)
    os.makedirs(rundir)
    try:
        p = sh(argv, cwd=rundir, timeout=TIMEOUT)
        rc, so, se = p.returncode, p.stdout.decode("latin-1"), p.stderr.decode("latin-1")
    except subprocess.TimeoutExpired:
        rc, so, se = "timeout", "", ""
    obj = open(out, "rb").read() if os.path.exists(out) else None
    return {"rc": rc, "listing": normalise_listing(so), "stderr": se,
            "obj": mask_rof(obj) if obj is not None else None, "argv": argv}


def compare(ref, cur):
    diffs = []
    if ref["rc"] != cur["rc"]:
        diffs.append(f"exit code {ref['rc']} -> {cur['rc']}")
    if ref["obj"] != cur["obj"]:
        if ref["obj"] is None or cur["obj"] is None:
            diffs.append("object file: one side produced none")
        else:
            a, b = ref["obj"], cur["obj"]
            k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            diffs.append(f"object file differs at byte {k} ({len(a)} vs {len(b)} bytes)")
    if ref["listing"] != cur["listing"]:
        la, lb = ref["listing"].splitlines(), cur["listing"].splitlines()
        k = next((i for i in range(min(len(la), len(lb))) if la[i] != lb[i]), min(len(la), len(lb)))
        diffs.append(f"listing differs at line {k + 1}:\n      ref: {la[k] if k < len(la) else '<end>'}"
                     f"\n      cur: {lb[k] if k < len(lb) else '<end>'}")
    if ref["stderr"] != cur["stderr"]:
        diffs.append(f"stderr differs: {ref['stderr'].strip()[:80]!r} -> {cur['stderr'].strip()[:80]!r}")
    return diffs


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default="HEAD", help="git revision to compare against (default HEAD)")
    ap.add_argument("--show", type=int, default=10, help="how many differences to print")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--skip", action="append", default=[], choices=["rma", "dcc", "lwdiff"],
                    help="leave out an input group, e.g. --skip lwdiff when a change is meant "
                         "to alter NitrOS9 results (compare those with lwdiff.py instead)")
    ap.add_argument("--current", help="test this las binary instead of building las/build/unix "
                    "(e.g. las/build/asan/las, with ASAN_OPTIONS=detect_leaks=0)")
    args = ap.parse_args()

    ref_bin, commit = build_reference(args.ref)
    if args.current:
        cur_bin, warnings = os.path.abspath(args.current), []
    else:
        cur_bin, warnings = build_current()
    for w in warnings:
        print("build warning:", w, file=sys.stderr)

    rma = [] if "rma" in args.skip else rma_inputs()
    dcc, csources = ([], 0) if "dcc" in args.skip else dcc_inputs()
    lwd = [] if "lwdiff" in args.skip else lwdiff_inputs()
    jobs = rma + dcc + lwd

    def one(job):
        return job, compare(run(ref_bin, job, "ref"), run(cur_bin, job, "cur"))

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        results = list(ex.map(one, jobs))

    # Coverage: how many inputs the reference assembled cleanly. A test where
    # both sides fail the same way early proves little.
    clean = {"rma": 0, "dcc": 0, "lwdiff": 0}
    for job, _ in results:
        r = os.path.join(WORK, "objs", "ref", job["id"].replace("/", "__") + ".r")
        kind = job["id"].split("/")[0]
        if os.path.exists(r):
            clean[kind] += 1

    failed = [(j, d) for j, d in results if d]
    print(f"reference: las at {commit[:8]} ({args.ref})")
    print(f"inputs: {len(rma)} RMA sources, {len(dcc)} DCC-compiled C files "
          f"(of {csources}), {len(lwd)} NitrOS9 build commands")
    print(f"assembled to an object file by the reference: rma {clean['rma']}, "
          f"dcc {clean['dcc']}, lwdiff {clean['lwdiff']}")
    for job, diffs in failed[:args.show]:
        print(f"\nDIFFERENT: {job['id']}")
        for d in diffs:
            print("   ", d)
    print(f"\n{'PASS' if not failed else 'FAIL'}: {len(jobs) - len(failed)} of {len(jobs)} identical")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
