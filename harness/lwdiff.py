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
DEFAULT_LWAR = os.path.join(HOME, "projects/coco-shelf/bin/lwar")
DEFAULT_LWLINK = os.path.join(HOME, "projects/coco-shelf/bin/lwlink")
DEFAULT_LLINK = os.path.join(DEVSYS, "llink/llink")

# lwar/lwlink commands seen by the last collect(), in build order
LINK_CMDS = []
GEN_CMDS = []                   # recipe steps that write an include file (collect)

TIMEOUT = 300  # seconds per assembly (lwasm takes 65 s for main's diag)


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
    del LINK_CMDS[:]
    del GEN_CMDS[:]
    for line in proc.stdout.splitlines():
        m = re.match(r"make\[\d+\]: (Entering|Leaving) directory '(.*)'", line)
        if m:
            if m.group(1) == "Entering":
                dirs.append(m.group(2))
            elif len(dirs) > 1:
                dirs.pop()
            continue
        # A step that writes an include file (diag's diagfont.inc,
        # diagpal.inc: sed over a source, "> .obj/NAME.inc")
        m = re.match(r"(.*\S)\s*>\s*(\S+\.inc)$", line)
        if m and not line.startswith(("lwasm", "lwar", "lwlink")):
            GEN_CMDS.append({"cwd": dirs[-1], "cmd": m.group(1), "name": os.path.basename(m.group(2))})
            continue
        # A line can hold several commands joined by ; or &&
        for part in re.split(r"\s*(?:&&|;)\s*", line):
            try:
                argv = shlex.split(part)
            except ValueError:
                continue
            if argv and os.path.basename(argv[0]) in ("lwar", "lwlink"):
                LINK_CMDS.append({"tool": os.path.basename(argv[0]),
                                  "cwd": dirs[-1], "argv": argv})
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
    # include files the recipe generates, written here instead of the
    # recipe's $(OBJDIR), so nothing is written into the NitrOS9 tree
    for g in GEN_CMDS:
        with open(os.path.join(gen, g["name"]), "w") as f:
            subprocess.run(g["cmd"], shell=True, cwd=g["cwd"], stdout=f, env=clean_env())
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


def is_constants_object(job):
    """An object-format source that only defines constants (a _constant
    section), like lib/wildbitsl2.as: the defs a linked program gets its
    system call numbers etc. from."""
    if job["format"] != "obj":
        return False
    secs = set()
    for line in open(job["source"], encoding="latin-1"):
        t = line.split(";")[0].split()
        if len(t) >= 2 and line[:1].isspace() and t[0].lower() == "section":
            secs.add(t[1].lower())
    return bool(secs) and secs <= {"_constant", "_constants"}


def run_las(job, las, lasdir, gen, defs=()):
    """Assemble the same source with las, with the same -D definitions.

    lwasm also looks for `use` files next to the source, so the source's
    own directory goes first in the include list.
    """
    out = os.path.join(lasdir, job["id"])
    incs = [os.path.dirname(job["source"])] + job["includes"] + [gen]
    argv = ([las, job["source"], "--lwasm"] + [f"-D{d}" for d in job["defines"]]
            + [f"--defs={d}" for d in defs]
            + ([f"--pragma={','.join(job['pragmas'])}"] if job["pragmas"] else [])
            + [f"-i={d}" for d in incs] + [f"-o={out}"])
    if os.path.exists(out):
        os.remove(out)
    # Run in an empty directory of its own: las writes its macro work file
    # (rma.tmp) to the current directory, so parallel runs sharing one would
    # collide, and it shouldn't write into the corpus. lwasm never searches
    # the current directory for use files anyway (it searches the including
    # file's directory, then -I paths), so this is closer to lwasm too.
    rundir = os.path.join(lasdir, "run", job["id"])
    os.makedirs(rundir, exist_ok=True)
    try:
        p = subprocess.run(argv, cwd=rundir, env=clean_env(),
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
    # A failed sizing trial still assembles, with long ,pcr offsets: it
    # shows only as a size difference, so say it (D46)
    if "sizing trial failed" in (lasr["stderr"] or ""):
        r["sizing_failed"] = True
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


# ---------------------------------------------------------------- link

LINK_ORDER = ["LINK_MATCH", "LINK_DIFF", "LINK_FAIL", "LINK_REF_FAIL"]
LINK_MEANING = {
    "LINK_MATCH": "llink module (lwlink layout, detected) is byte-identical to lwlink's",
    "LINK_DIFF": "both linked, but the modules differ",
    "LINK_FAIL": "llink failed, or an object it needs didn't assemble",
    "LINK_REF_FAIL": "lwlink itself failed",
}


def run_links(jobs, results, args, refdir, lasdir):
    """Replay the build's lwar and lwlink commands on both sides.

    Reference: lwar and lwlink on lwasm's objects. las: a ROF library is
    its members' .r files one after another, and llink links
    with the same libraries in the same order.
    """
    byobj = {os.path.normpath(os.path.join(j["cwd"], j["output"])): j["id"] for j in jobs}
    ok = {r["id"] for r in results if r["status"] == "ASSEMBLED"}
    for d in ("_lib", "_link"):     # "link" is also a NitrOS9 command name
        os.makedirs(os.path.join(refdir, d), exist_ok=True)
        os.makedirs(os.path.join(lasdir, d), exist_ok=True)
    libs_ok = {}
    out = []
    for cmd in LINK_CMDS:
        argv, cwd = cmd["argv"], cmd["cwd"]
        if cmd["tool"] == "lwar":
            files = [a for a in argv[1:] if not a.startswith("-")]
            lib = os.path.basename(files[0])                    # libX.a
            members = [byobj.get(os.path.normpath(os.path.join(cwd, f))) for f in files[1:]]
            subprocess.run([args.lwar, "-c", os.path.join(refdir, "_lib", lib)]
                           + [os.path.join(refdir, m) for m in members if m],
                           cwd=refdir, env=clean_env(), capture_output=True)
            with open(os.path.join(lasdir, "_lib", lib[:-2] + ".l"), "wb") as f:
                for m in members:
                    if m and os.path.exists(os.path.join(lasdir, m)):
                        f.write(open(os.path.join(lasdir, m), "rb").read())
            libs_ok[lib[:-2]] = [m for m in members if m not in ok]
            continue
        # lwlink
        # seq keeps libraries and objects in command-line order: lwlink
        # lays out by it (a library before the program puts its code first)
        libs, inputs, seq, name = [], [], [], None
        for a in argv[1:]:
            if a.startswith("-l"):
                libs.append("lib" + a[2:])
                seq.append(("lib", "lib" + a[2:]))
            elif a.startswith("-o"):
                name = a[2:]
            elif not a.startswith("-") and a.endswith(".o"):
                inputs.append(byobj.get(os.path.normpath(os.path.join(cwd, a))))
                seq.append(("obj", inputs[-1]))
        r = {"id": name, "inputs": inputs, "libs": libs}
        missing = [i for i in inputs if i not in ok]
        refout = os.path.join(refdir, "_link", name)
        lasout = os.path.join(lasdir, "_link", name)
        for f in (refout, lasout):
            if os.path.exists(f):
                os.remove(f)
        p = subprocess.run([args.lwlink, "--format=os9", "-L", os.path.join(refdir, "_lib")]
                           + [("-l" + v[3:]) if k == "lib" else os.path.join(refdir, v)
                              for k, v in seq if v] + ["-o" + name],
                           cwd=os.path.join(refdir, "_link"), env=clean_env(),
                           capture_output=True, text=True)
        if not os.path.exists(refout):
            r["status"], r["detail"] = "LINK_REF_FAIL", (p.stdout + p.stderr).strip()[:120]
            out.append(r)
            continue
        if missing or None in inputs:
            r["status"], r["detail"] = "LINK_FAIL", "objects didn't assemble: " + ", ".join(
                str(m) for m in missing)
            out.append(r)
            continue
        # no --lwlink: llink must pick lwlink's layout from the mainline
        # (D42); native.py obj keeps the flag, so both are tested
        p = subprocess.run([args.llink]
                           + [("-l=" + os.path.join(lasdir, "_lib", v + ".l")) if k == "lib"
                              else os.path.join(lasdir, v) for k, v in seq]
                           + ["-o=" + name, "-n=" + name],
                           cwd=os.path.join(lasdir, "_link"), env=clean_env(),
                           capture_output=True, text=True)
        if not os.path.exists(lasout):
            r["status"] = "LINK_FAIL"
            r["detail"] = "llink: " + ((p.stdout + p.stderr).strip().splitlines() or ["failed"])[0][:110]
            bad = [m for l in libs for m in libs_ok.get(l, [])]
            if bad:
                r["detail"] += f" (library members that didn't assemble: {len(bad)})"
        else:
            a, b = open(refout, "rb").read(), open(lasout, "rb").read()
            if a == b:
                r["status"], r["detail"] = "LINK_MATCH", f"{len(a)} bytes"
            else:
                k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
                r["status"] = "LINK_DIFF"
                r["detail"] = f"lwlink {len(a)} bytes, llink {len(b)} bytes, first difference at ${k:04X}"
        out.append(r)
    return out


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


def write_report(results, jobs, args, workdir, report_path, corpus, links=()):
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

    if links:
        lc = Counter(r["status"] for r in links)
        L.append("## Linked programs\n")
        L.append("Object-format files are compared after linking: the build's lwar and "
                 "lwlink commands are replayed with lwasm's objects, and the same links are "
                 "made from las's ROF objects with llink (which detects lwlink's layout).\n")
        L.append("| Status | Programs | Meaning |\n|---|---|---|")
        for s in LINK_ORDER:
            if lc[s]:
                L.append(f"| {s} | {lc[s]} | {LINK_MEANING[s]} |")
        L.append("")
        L.append("| Program | Status | Detail |\n|---|---|---|")
        for r in sorted(links, key=lambda r: (LINK_ORDER.index(r["status"]), r["id"])):
            L.append(f"| `{r['id']}` | {r['status']} | {r['detail']} |")
        L.append("")

    L.append("## lwasm options the build uses\n")
    L.append("Pragmas (las has none of these yet): " +
             ", ".join(f"`{k}`" for k in pragmas) + ".\n")
    if other:
        L.append("Other options: " + ", ".join(f"`{k}` ({v})" for k, v in other.most_common()) + ".\n")

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
    header = (["date", "recipe", "platform", "nitros9", "nitros9-languages", "total"]
              + STATUS_ORDER + ["LINKED", "LINK_MATCH"])
    rows = list(csv.reader(open(hist))) if os.path.exists(hist) else [header]
    if rows[0] != header:           # older history: add the new columns
        rows = [header] + [r + [""] * (len(header) - len(r)) for r in rows[1:]]
    lc = Counter(r["status"] for r in links)
    rows.append([now, args.recipe, args.platform, short(corpus["nitros9"]),
                 short(corpus["nitros9-languages"]), total] + [counts[s] for s in STATUS_ORDER]
                + [len(links), lc["LINK_MATCH"]])
    with open(hist, "w", newline="") as f:
        csv.writer(f).writerows(rows)
    return counts


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--recipe", default="l2")
    ap.add_argument("--platform", default="jr2", help="jr, jr2 or k2")
    ap.add_argument("--nitros9", default=os.environ.get("NITROS9DIR", DEFAULT_NITROS9))
    ap.add_argument("--lwasm", default=DEFAULT_LWASM)
    ap.add_argument("--las", default=DEFAULT_LAS)
    ap.add_argument("--llink", default=DEFAULT_LLINK)
    ap.add_argument("--lwar", default=DEFAULT_LWAR)
    ap.add_argument("--lwlink", default=DEFAULT_LWLINK)
    ap.add_argument("--only", help="only files whose id or source contains this text")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--verbose", action="store_true", help="print las output for each file")
    ap.add_argument("--no-report", action="store_true", help="don't write the report or history")
    ap.add_argument("--work", help="work directory (default harness/out/RECIPE-PLATFORM, which "
                    "native.py reads; another tree, e.g. --nitros9 for NitrOS9 main, needs its own)")
    args = ap.parse_args()
    args.nitros9 = os.path.abspath(args.nitros9)
    args.las = os.path.abspath(args.las)       # las runs in its own scratch directory
    args.llink = os.path.abspath(args.llink)
    args.lwasm = os.path.abspath(args.lwasm)

    if not os.path.exists(args.las):
        sys.exit(f"las not found at {args.las}; build it with: cd las/build/unix && make")

    workdir = os.path.abspath(args.work) if args.work else \
        os.path.join(HERE, "out", f"{args.recipe}-{args.platform}")
    os.makedirs(workdir, exist_ok=True)
    refdir, lasdir = os.path.join(workdir, "ref"), os.path.join(workdir, "las")
    for d in (refdir, lasdir):
        os.makedirs(d, exist_ok=True)

    corpus = corpus_state(args.nitros9)
    check_lock(corpus)
    jobs = collect(args.nitros9, args.recipe, args.platform, workdir)
    gen = make_generated(workdir)          # after collect: runs its generator steps
    with open(os.path.join(workdir, "jobs.json"), "w") as f:
        json.dump(jobs, f, indent=1)
    if args.only:
        jobs = [j for j in jobs if args.only in j["id"] or args.only in j["source"]]
    print(f"{len(jobs)} assemblies to compare", file=sys.stderr)

    # Constants objects first: every other object-format file gets their
    # ROF as --defs, so expressions on linker-only constants can be
    # evaluated (las preload.c), as lwlink would at link time.
    defs_jobs = [j for j in jobs if is_constants_object(j)]
    defs_files = [os.path.join(lasdir, j["id"]) for j in defs_jobs]

    def one(job):
        ref_ok, ref_out, ref_msg = run_reference(job, args.lwasm, refdir, gen)
        use = defs_files if (job["format"] == "obj" and job not in defs_jobs) else ()
        lasr = run_las(job, args.las, lasdir, gen, use)
        with open(os.path.join(lasdir, job["id"] + ".log"), "w") as f:
            f.write("$ " + " ".join(shlex.quote(a) for a in lasr["argv"]) + "\n")
            f.write(lasr["stdout"] + "\n--- stderr ---\n" + lasr["stderr"])
        r = classify(job, ref_ok, ref_out, ref_msg, lasr)
        if args.verbose:
            print(f"\n=== {job['id']}: {r['status']} - {r['detail']}\n{lasr['stdout']}{lasr['stderr']}")
        return r

    first = {j["id"]: one(j) for j in defs_jobs}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        rest = list(ex.map(one, [j for j in jobs if j["id"] not in first]))
    byid = {r["id"]: r for r in rest}
    byid.update(first)
    results = [byid[j["id"]] for j in jobs]

    with open(os.path.join(workdir, "results.json"), "w") as f:
        json.dump(results, f, indent=1)

    links = [] if args.only else run_links(jobs, results, args, refdir, lasdir)
    with open(os.path.join(workdir, "links.json"), "w") as f:
        json.dump(links, f, indent=1)

    counts = Counter(r["status"] for r in results)
    print("  ".join(f"{s} {counts[s]}" for s in STATUS_ORDER if counts[s]), file=sys.stderr)
    szf = [r["id"] for r in results if r.get("sizing_failed")]
    if szf:
        print(f"sizing trial failed (long offsets) in {len(szf)}: {' '.join(szf[:10])}",
              file=sys.stderr)
    if links:
        lc = Counter(r["status"] for r in links)
        print("links: " + "  ".join(f"{s} {lc[s]}" for s in LINK_ORDER if lc[s]), file=sys.stderr)
    if not args.no_report and not args.only:
        path = os.path.join(HERE, "reports", f"{args.recipe}-{args.platform}.md")
        write_report(results, jobs, args, workdir, path, corpus, links)
        print(f"report: {os.path.relpath(path, DEVSYS)}", file=sys.stderr)


if __name__ == "__main__":
    main()
