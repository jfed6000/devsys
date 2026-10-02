#!/usr/bin/env python3
"""lwdiff - compare las against lwasm over a real NitrOS9 build.

1. collect: dry-run (`make -n -B`) a NitrOS9 recipe and capture every lwasm
   command it would run, with the directory it runs in.
2. run: replay each command with lwasm (the reference) and with las, using
   the same source, include paths and defines.
3. report: classify each file and write a Markdown report plus a line in
   history.csv, so progress can be tracked from run to run.

Usage:
    python3 harness/lwdiff.py                       # l2 recipe, jr2
    python3 harness/lwdiff.py --platform k2
    python3 harness/lwdiff.py --only scfg --verbose  # one file, show output

Outputs go to harness/out/<recipe>-<platform>/ (not version-controlled).
The report goes to harness/reports/<recipe>-<platform>.md.
"""
import argparse
import concurrent.futures
import csv
import datetime
import json
import os
import re
import shlex
import subprocess
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
DEVSYS = os.path.dirname(HERE)
HOME = os.path.expanduser("~")

DEFAULT_NITROS9 = os.path.join(DEVSYS, "corpus/nitros9")  # pinned by harness/corpus.lock
DEFAULT_LWASM = os.path.join(HOME, "projects/coco-shelf/bin/lwasm.orig")
DEFAULT_LAS = os.path.join(DEVSYS, "las/build/unix/las")

TIMEOUT = 30  # seconds per assembly


def clean_env():
    env = dict(os.environ)
    env.pop("LD_PRELOAD", None)  # stops the libgtk3-nocsd noise on stderr
    return env


def corpus_state(nitros9):
    """Commit and cleanliness of nitros9 and its sibling nitros9-languages."""
    state = {}
    for name, path in (("nitros9", nitros9),
                       ("nitros9-languages", os.path.join(os.path.dirname(nitros9), "nitros9-languages"))):
        try:
            head = subprocess.run(["git", "-C", path, "rev-parse", "HEAD"],
                                  capture_output=True, text=True, check=True).stdout.strip()
            dirty = subprocess.run(["git", "-C", path, "status", "--porcelain", "--untracked-files=no"],
                                   capture_output=True, text=True).stdout.strip() != ""
        except (subprocess.CalledProcessError, FileNotFoundError):
            head, dirty = "unknown", False
        state[name] = {"path": path, "commit": head, "dirty": dirty}
    return state


def check_lock(state):
    """Warn when the corpus isn't at the commits in corpus.lock."""
    lock = os.path.join(HERE, "corpus.lock")
    if not os.path.exists(lock):
        return
    for line in open(lock):
        f = line.split()
        if len(f) == 4 and not line.startswith("#") and f[0] in state:
            st = state[f[0]]
            if st["commit"] != f[3]:
                print(f"warning: {f[0]} is at {st['commit'][:8]}, lock says {f[3][:8]} "
                      f"(run harness/corpus.sh)", file=sys.stderr)
            if st["dirty"]:
                print(f"warning: {f[0]} has uncommitted changes", file=sys.stderr)


def short(st):
    return st["commit"][:8] + ("+dirty" if st["dirty"] else "")


# ---------------------------------------------------------------- collect

def collect(nitros9, recipe, platform, workdir):
    """Dry-run the recipe and return a list of lwasm jobs."""
    recipe_dir = os.path.join(nitros9, "recipes/wildbits", recipe)
    sysdir = os.path.join(workdir, "sysdir")
    os.makedirs(sysdir, exist_ok=True)
    env = clean_env()
    env["NITROS9DIR"] = nitros9
    # SYS_DIR override: the real build creates .sys in the recipe directory;
    # a dry run can't, so point it at our work directory instead.
    cmd = ["make", "-n", "-B", "-k", f"PLATFORM={platform}", f"SYS_DIR={sysdir}"]
    proc = subprocess.run(cmd, cwd=recipe_dir, env=env, capture_output=True, text=True)
    with open(os.path.join(workdir, "dryrun.txt"), "w") as f:
        f.write(proc.stdout)

    jobs = []
    seen = set()
    dirs = [recipe_dir]
    for line in proc.stdout.splitlines():
        m = re.match(r"make\[\d+\]: (Entering|Leaving) directory '(.*)'", line)
        if m:
            if m.group(1) == "Entering":
                dirs.append(m.group(2))
            elif len(dirs) > 1:
                dirs.pop()
            continue
        # A line can hold several commands joined by ; or &&
        for part in re.split(r"\s*(?:&&|;)\s*", line):
            try:
                argv = shlex.split(part)
            except ValueError:
                continue
            if not argv or os.path.basename(argv[0]) != "lwasm":
                continue
            job = parse_lwasm(argv, dirs[-1])
            if job is None:
                continue
            key = (job["cwd"], tuple(job["argv"]))
            if key in seen:
                continue
            seen.add(key)
            jobs.append(job)

    # Give every job a unique, filesystem-safe id based on its output name.
    counts = Counter()
    for job in jobs:
        base = os.path.basename(job["output"]) or "out"
        counts[base] += 1
        job["id"] = base if counts[base] == 1 else f"{base}~{counts[base]}"
    return jobs


def parse_lwasm(argv, cwd):
    """Pull out what we need from one lwasm command line."""
    job = {"cwd": cwd, "argv": argv, "format": "os9", "includes": [],
           "defines": [], "pragmas": [], "sources": [], "output": None,
           "other": []}
    i = 1
    while i < len(argv):
        a = argv[i]
        nxt = argv[i + 1] if i + 1 < len(argv) else None
        if a.startswith("--format="):
            job["format"] = a.split("=", 1)[1]
        elif a.startswith("--includedir="):
            job["includes"].append(a.split("=", 1)[1])
        elif a == "-I" and nxt:
            job["includes"].append(nxt); i += 1
        elif a.startswith("-I"):
            job["includes"].append(a[2:])
        elif a == "-D" and nxt:
            job["defines"].append(nxt); i += 1
        elif a.startswith("-D"):
            job["defines"].append(a[2:])
        elif a.startswith("--pragma="):
            job["pragmas"] += a.split("=", 1)[1].split(",")
        elif a == "-o" and nxt:
            job["output"] = nxt; i += 1
        elif a.startswith("-o"):
            job["output"] = a[2:]
        elif a.startswith("--output="):
            job["output"] = a.split("=", 1)[1]
        elif a.startswith("-"):
            job["other"].append(a)
        else:
            job["sources"].append(a)
        i += 1
    if not job["sources"] or not job["output"]:
        return None
    job["source"] = os.path.normpath(os.path.join(cwd, job["sources"][0]))
    job["includes"] = [os.path.normpath(os.path.join(cwd, d)) for d in job["includes"]]
    return job


# ---------------------------------------------------------------- run

def make_generated(workdir):
    """Create files the real build generates before assembling.

    The dry run prints these commands but doesn't run them. They go in our
    work directory, which both assemblers get as the last include path.
    """
    gen = os.path.join(workdir, "generated")
    os.makedirs(gen, exist_ok=True)
    # wildbits.mak writes buildinfo (date, commit, branch) for init.asm
    with open(os.path.join(gen, "buildinfo"), "w") as f:
        f.write(" fcc !lwdiff harness build!\n")
    return gen


def run_reference(job, lwasm, refdir, gen):
    """Assemble with lwasm exactly as the build would, output to refdir."""
    out = os.path.join(refdir, job["id"])
    argv = [lwasm]
    skip = False
    for i, a in enumerate(job["argv"][1:], 1):
        if skip:
            skip = False
            continue
        if a == "-o":
            skip = True
            continue
        if a.startswith("-o") or a.startswith("--output=") or a.startswith("--list") \
                or a.startswith("--map") or a == "--symbols":
            continue
        argv.append(a)
    argv.append(f"-I{gen}")
    argv.append(f"--output={out}")
    p = subprocess.run(argv, cwd=job["cwd"], env=clean_env(),
                       capture_output=True, text=True, timeout=TIMEOUT)
    ok = p.returncode == 0 and os.path.exists(out)
    return ok, out, (p.stdout + p.stderr).strip()


def define_lines(defines):
    """lwasm -DNAME[=VAL] becomes 'NAME set VAL' (VAL defaults to 1)."""
    lines = []
    for d in defines:
        name, _, val = d.partition("=")
        lines.append(f"{name} set {val or '1'}")
    return lines


def run_las(job, las, lasdir, gen):
    """Assemble the same source with las.

    las has no -D option yet, so defines go into a small prelude file that
    is assembled first (RMA assembles several input files as one). lwasm
    also looks for `use` files next to the source, so the source's own
    directory goes first in the include list.
    """
    out = os.path.join(lasdir, job["id"])
    prelude = None
    sources = [job["source"]]
    if job["defines"]:
        prelude = os.path.join(lasdir, job["id"] + ".defines.a")
        with open(prelude, "w") as f:
            f.write("\n".join(define_lines(job["defines"])) + "\n")
        sources.insert(0, prelude)
    incs = [os.path.dirname(job["source"])] + job["includes"] + [gen]
    argv = [las] + sources + [f"-i={d}" for d in incs] + [f"-o={out}"]
    if os.path.exists(out):
        os.remove(out)
    try:
        p = subprocess.run(argv, cwd=job["cwd"], env=clean_env(),
                           capture_output=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return {"rc": None, "stdout": "", "stderr": "timeout", "out": out, "argv": argv}
    return {"rc": p.returncode, "out": out, "argv": argv,
            "stdout": p.stdout.decode("latin-1"), "stderr": p.stderr.decode("latin-1")}


ERR_RE = re.compile(r"^\*\*\* error - (.*?) \*\*\*")
LIST_RE = re.compile(r"^\d{5} (?:[0-9a-fA-F]{4})?[ =]?\S*\s+(\S*)\s+(\S+)?")


def parse_las_errors(stdout):
    """Return a list of (message, mnemonic, source line) from las output."""
    errors = []
    lines = stdout.splitlines()
    for i, line in enumerate(lines):
        m = ERR_RE.match(line)
        if not m:
            continue
        msg = m.group(1)
        src = lines[i + 1] if i + 1 < len(lines) else ""
        mnem = ""
        # Listing line: line number, address, bytes, label, mnemonic, operand.
        fields = src[22:].split() if len(src) > 22 else []
        if src[22:23] not in (" ", "") and fields:
            fields = fields[1:]  # drop the label
        if fields:
            mnem = fields[0].lower()
        errors.append((msg, mnem, src.strip()))
    return errors


def classify(job, ref_ok, ref_out, ref_msg, lasr):
    r = {"id": job["id"], "source": job["source"], "format": job["format"],
         "defines": job["defines"]}
    if not ref_ok:
        r["status"] = "REF_FAIL"
        r["detail"] = ref_msg.splitlines()[0] if ref_msg else "lwasm failed"
        return r
    rc = lasr["rc"]
    errors = parse_las_errors(lasr["stdout"])
    r["errors"] = errors
    if rc is None:
        r["status"] = "TIMEOUT"
        r["detail"] = "las took longer than %ds" % TIMEOUT
    elif rc < 0:
        r["status"] = "CRASH"
        r["detail"] = "las killed by signal %d" % -rc
    elif rc not in (0, 1):
        r["status"] = "FATAL"
        msg = lasr["stderr"].strip().replace("\r", "\\r")  # show CRs from CRLF files
        r["detail"] = (msg.splitlines() or ["exit %d" % rc])[-1]
    elif errors or rc == 1:
        r["status"] = "ERRORS"
        r["detail"] = "%d errors; first: %s" % (len(errors), errors[0][0] if errors else "?")
    elif job["format"] != "os9":
        r["status"] = "ASSEMBLED"
        r["detail"] = "object format: compare after linking (phase 5)"
    else:
        a = open(ref_out, "rb").read()
        b = open(lasr["out"], "rb").read() if os.path.exists(lasr["out"]) else b""
        if a == b:
            r["status"] = "MATCH"
            r["detail"] = "%d bytes" % len(a)
        else:
            first = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]),
                         min(len(a), len(b)))
            r["status"] = "DIFF"
            r["detail"] = "lwasm %d bytes, las %d bytes, first difference at $%04X" % (
                len(a), len(b), first)
    return r


# ---------------------------------------------------------------- report

STATUS_ORDER = ["MATCH", "ASSEMBLED", "DIFF", "ERRORS", "FATAL", "CRASH", "TIMEOUT", "REF_FAIL"]
STATUS_MEANING = {
    "MATCH": "las output is byte-identical to lwasm",
    "ASSEMBLED": "object-format file assembled without errors; compared after linking (phase 5)",
    "DIFF": "assembled without errors, but the bytes differ",
    "ERRORS": "las reported assembly errors",
    "FATAL": "las stopped early (e.g. couldn't open a file)",
    "CRASH": "las crashed",
    "TIMEOUT": "las didn't finish in time",
    "REF_FAIL": "lwasm itself failed, so there's nothing to compare against",
}


def blocker_key(msg, mnem):
    if msg in ("bad mnemonic", "illegal external reference", "bad operator") and mnem:
        return f"{msg}: {mnem}"
    return msg


def write_report(results, jobs, args, workdir, report_path, corpus):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    counts = Counter(r["status"] for r in results)
    total = len(results)

    # Blockers: the first error las reports in each file. Later errors are
    # mostly knock-on effects (after `mod` fails, every instruction is
    # rejected), so the first error is what needs fixing.
    first = defaultdict(list)
    for r in results:
        errs = r.get("errors") or []
        if errs:
            msg, mnem, line = errs[0]
            first[blocker_key(msg, mnem)].append((r["id"], line))
    fatal = Counter(r["detail"] for r in results if r["status"] == "FATAL")

    # lwasm options las doesn't have, across all jobs.
    pragmas = Counter(p for j in jobs for p in j["pragmas"])
    other = Counter(o for j in jobs for o in j["other"])
    formats = Counter(j["format"] for j in jobs)

    L = []
    L.append(f"# lwdiff report: {args.recipe} / {args.platform}\n")
    L.append(f"Run {now}. {total} assemblies from a dry run of "
             f"`recipes/wildbits/{args.recipe}` (PLATFORM={args.platform}).\n")
    L.append("Corpus: " + ", ".join(f"`{n}` {short(st)}" for n, st in corpus.items()) + ".\n")
    L.append("Generated by `harness/lwdiff.py`. Don't edit by hand.\n")
    L.append("## Summary\n")
    L.append("| Status | Files | Meaning |\n|---|---|---|")
    for s in STATUS_ORDER:
        if counts[s]:
            L.append(f"| {s} | {counts[s]} | {STATUS_MEANING[s]} |")
    L.append(f"| **Total** | **{total}** | |\n")
    L.append("Formats in this build: " + ", ".join(f"`{k}` {v}" for k, v in formats.most_common()) + ".\n")

    if fatal:
        L.append("## Fatal errors\n")
        L.append("| Message | Files |\n|---|---|")
        for msg, n in fatal.most_common():
            L.append(f"| `{msg}` | {n} |")
        L.append("")

    if first:
        L.append("## Blockers\n")
        L.append("The first error las reports in each file, grouped. Later errors in a "
                 "file are usually knock-on effects of the first, so fix these first.\n")
        L.append("| First error | Files | Example file | Example line |\n|---|---|---|---|")
        for k in sorted(first, key=lambda k: (-len(first[k]), k)):
            fid, line = first[k][0]
            line = line.replace("|", "\\|")
            L.append(f"| `{k}` | {len(first[k])} | `{fid}` | `{line[:90]}` |")
        L.append("")

    L.append("## lwasm options the build uses\n")
    L.append("Pragmas (las has none of these yet): " +
             ", ".join(f"`{k}`" for k in pragmas) + ".\n")
    if other:
        L.append("Other options: " + ", ".join(f"`{k}` ({v})" for k, v in other.most_common()) + ".\n")
    L.append("The harness works around the missing `-D` option by assembling a prelude "
             "file of `NAME set VALUE` lines before the source.\n")

    L.append("## Files\n")
    L.append("| File | Source | Status | Detail |\n|---|---|---|---|")
    order = {s: i for i, s in enumerate(STATUS_ORDER)}
    for r in sorted(results, key=lambda r: (order[r["status"]], r["id"])):
        src = os.path.relpath(r["source"], args.nitros9) if r["source"].startswith(args.nitros9) else r["source"]
        L.append(f"| `{r['id']}` | `{src}` | {r['status']} | {r['detail']} |")
    L.append("")

    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w") as f:
        f.write("\n".join(L))

    hist = os.path.join(HERE, "reports", "history.csv")
    new = not os.path.exists(hist)
    with open(hist, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["date", "recipe", "platform", "nitros9", "nitros9-languages", "total"] + STATUS_ORDER)
        w.writerow([now, args.recipe, args.platform, short(corpus["nitros9"]),
                    short(corpus["nitros9-languages"]), total] + [counts[s] for s in STATUS_ORDER])
    return counts


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--recipe", default="l2")
    ap.add_argument("--platform", default="jr2", help="jr, jr2 or k2")
    ap.add_argument("--nitros9", default=os.environ.get("NITROS9DIR", DEFAULT_NITROS9))
    ap.add_argument("--lwasm", default=DEFAULT_LWASM)
    ap.add_argument("--las", default=DEFAULT_LAS)
    ap.add_argument("--only", help="only files whose id or source contains this text")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--verbose", action="store_true", help="print las output for each file")
    ap.add_argument("--no-report", action="store_true", help="don't write the report or history")
    args = ap.parse_args()
    args.nitros9 = os.path.abspath(args.nitros9)

    if not os.path.exists(args.las):
        sys.exit(f"las not found at {args.las}; build it with: cd las/build/unix && make")

    workdir = os.path.join(HERE, "out", f"{args.recipe}-{args.platform}")
    refdir, lasdir = os.path.join(workdir, "ref"), os.path.join(workdir, "las")
    for d in (refdir, lasdir):
        os.makedirs(d, exist_ok=True)

    corpus = corpus_state(args.nitros9)
    check_lock(corpus)
    gen = make_generated(workdir)
    jobs = collect(args.nitros9, args.recipe, args.platform, workdir)
    with open(os.path.join(workdir, "jobs.json"), "w") as f:
        json.dump(jobs, f, indent=1)
    if args.only:
        jobs = [j for j in jobs if args.only in j["id"] or args.only in j["source"]]
    print(f"{len(jobs)} assemblies to compare", file=sys.stderr)

    def one(job):
        ref_ok, ref_out, ref_msg = run_reference(job, args.lwasm, refdir, gen)
        lasr = run_las(job, args.las, lasdir, gen)
        with open(os.path.join(lasdir, job["id"] + ".log"), "w") as f:
            f.write("$ " + " ".join(shlex.quote(a) for a in lasr["argv"]) + "\n")
            f.write(lasr["stdout"] + "\n--- stderr ---\n" + lasr["stderr"])
        r = classify(job, ref_ok, ref_out, ref_msg, lasr)
        if args.verbose:
            print(f"\n=== {job['id']}: {r['status']} - {r['detail']}\n{lasr['stdout']}{lasr['stderr']}")
        return r

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        results = list(ex.map(one, jobs))

    with open(os.path.join(workdir, "results.json"), "w") as f:
        json.dump(results, f, indent=1)

    counts = Counter(r["status"] for r in results)
    print("  ".join(f"{s} {counts[s]}" for s in STATUS_ORDER if counts[s]), file=sys.stderr)
    if not args.no_report and not args.only:
        path = os.path.join(HERE, "reports", f"{args.recipe}-{args.platform}.md")
        write_report(results, jobs, args, workdir, path, corpus)
        print(f"report: {os.path.relpath(path, DEVSYS)}", file=sys.stderr)


if __name__ == "__main__":
    main()
