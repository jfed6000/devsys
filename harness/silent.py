#!/usr/bin/env python3
"""silent - the failures that reported success (roadmap step 14, D43).

Each check failed before its fix and must pass after. Host tools, plus
harness/os9run.py's model for what only happens on OS-9; MAME is the
proof for the native chain (native.py cc builds and runs longret.c).

  1. longret  a long or double returned in X survives the return: dco68
              turned "leas 2,s / puls u,pc" into "puls x,u,pc" and lost
              it. harness/native/longret.c, built with the devsys passes,
              run in the model: prints "longret: all right", status 0.
  2. errexit  las's fatal errors exit non-zero: errexit() exited with
              errno, 0 for "bad -D value".
  3. dcpp     dcpp out of memory (16 pages, in the model) writes its
              fatal record, and dcc68 then reports it and exits non-zero,
              so dcc stops. No fix was needed; this keeps it so.
  4. macros   two las runs in one directory don't share a macro file
              ("rma.tmp"): four at once, three times, each must equal a
              run on its own.
  5. regwidth dcc68's register tracking doesn't take a char for an int
              at the same place, or an int for a char (D53): it stored D
              after "ldb" where "ldd" was needed. harness/native/
              regwidth.c, built and run like longret: "regwidth: all
              right", status 0.
  6. unsgn    an unsigned comparison with 0 doesn't test a stale carry
              (D59): dco68 dropped "cmpd #0" after "ldd" before bhi/bls.
              harness/native/unsgn.c, built and run like longret.
  7. cstr     dcc68 removes its strings file ("cstr.XXXXXX") when a fatal
              error ends the compile (D61): errexit() left it in the
              current directory. Linux dcc68, and native dcc68 in the
              model, on a string followed by a fatal error record.
  8. undef    las reports a symbol never defined in a module (step 23):
              a module is written raw without its references, and las
              dropped them, leaving 0 and exit status 0, where lwasm says
              "Undefined symbol". In code, fdb/fcb and an indexed operand.

  python3 harness/silent.py [--keep]
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lwdiff  # noqa: E402

DEVSYS = os.path.dirname(HERE)
COCOC = os.path.expanduser("~/projects/dcc/CoCoC")
LIB = os.path.join(DEVSYS, "clib", "lib")   # step 18
LAS = os.path.join(DEVSYS, "las/build/unix/las")
LLINK = os.path.join(DEVSYS, "llink/llink")
DCC = os.path.join(DEVSYS, "dcc")
NATIVE = os.path.join(DEVSYS, "native")
WORK = os.path.join(HERE, "out", "silent")


def run(argv, cwd, **kw):
    return subprocess.run(argv, cwd=cwd, env=lwdiff.clean_env(), capture_output=True, **kw)


def os9run(root, cwd, module, args, out=None):
    argv = [sys.executable, os.path.join(HERE, "os9run.py"), "--root", root, "--cwd", cwd]
    if out:
        argv += ["--stdout", out, "--stderr", out + ".err"]
    return run(argv + [module] + args, root)


def cr_copy(src, dst):
    """A text file as an OS-9 disk holds it: CR line ends."""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    data = open(src, "rb").read().replace(b"\r\n", b"\n").replace(b"\n", b"\r")
    open(dst, "wb").write(data)


def build_and_run(name):
    """harness/native/NAME.c built with the devsys passes and run in the
    model: it must print "NAME: all right" and exit 0."""
    d = os.path.join(WORK, name)
    os.makedirs(os.path.join(d, "CMDS"))
    src = os.path.join(HERE, "native", name + ".c")
    p = run([os.path.join(DCC, "CPP/dcpp"), "-I=" + os.path.join(COCOC, "Defs"), src], d)
    open(os.path.join(d, "x.i"), "wb").write(p.stdout)
    run([os.path.join(DCC, "CC09/dcc68"), "x.i", "-o=x.raw.a"], d)
    with open(os.path.join(d, "x.raw.a"), "rb") as fin:
        opt = subprocess.run([os.path.join(DCC, "COpt/dco68")], stdin=fin,
                             capture_output=True, env=lwdiff.clean_env())
    open(os.path.join(d, "x.a"), "wb").write(opt.stdout)
    run([LAS, "x.a", "-o=x.r"], d)
    run([LLINK, os.path.join(LIB, "cstart.r"), "x.r", "-l=" + os.path.join(LIB, "clib.l"),
         "-o=CMDS/" + name, "-n=" + name], d)
    out = os.path.join(d, "printed")
    r = os9run(d, ".", os.path.join(d, "CMDS", name), [], out)
    printed = open(out, "rb").read().decode("latin-1").replace("\r", "\n").strip() \
        if os.path.exists(out) else ""
    ok = r.returncode == 0 and printed == name + ": all right"
    return ok, f"status {r.returncode}, printed {printed!r}"


def check_longret():
    return build_and_run("longret")


def check_regwidth():
    return build_and_run("regwidth")


def check_unsgn():
    return build_and_run("unsgn")


def check_errexit():
    d = os.path.join(WORK, "errexit")
    os.makedirs(d)
    open(os.path.join(d, "x.a"), "w").write(" psect x,0,0,0,0,0\n rts\n endsect\n")
    good = run([LAS, "x.a", "-o=x.r"], d).returncode
    bad = run([LAS, "x.a", "-Dfoo=zz", "-o=x.r"], d).returncode
    return good == 0 and bad != 0, f"good source {good}, bad -D value {bad}"


def check_cstr():
    d = os.path.join(WORK, "cstr")
    src = 'main() { puts("hello"); }\n#1\nbad line\n2\n0\nfatal\nf() {}\n'
    left = {}
    for where in ("linux", "native"):
        w = os.path.join(d, where)
        os.makedirs(w)
        open(os.path.join(w, "x.i"), "w").write(src if where == "linux" else src.replace("\n", "\r"))
        if where == "linux":
            p = run([os.path.join(DCC, "CC09/dcc68"), "x.i", "-o=x.a"], w)
        else:
            p = os9run(d, where, os.path.join(NATIVE, "bin", "dcc68"), ["x.i", "-o=x.a"],
                       os.path.join(d, "c68"))
        left[where] = (p.returncode, [f for f in os.listdir(w) if f.startswith("cstr.")])
    ok = all(rc != 0 and not files for rc, files in left.values())
    return ok, ", ".join(f"{k}: status {rc}, {len(f)} strings file(s) left"
                         for k, (rc, f) in left.items())


def check_undef():
    d = os.path.join(WORK, "undef")
    os.makedirs(d)
    open(os.path.join(d, "u.asm"), "w").write(
        " mod eom,name,$11,$81,start,size\n org 0\nsize equ .\nname fcs /u/\n"
        "start ldb #NOSUCH\n fdb NOB\n lda NOC,x\n rts\n emod\neom equ *\n")
    p = run([LAS, "--lwasm", "-o=u.mod", "u.asm"], d)
    out = p.stdout.decode(errors="replace")
    named = [n for n in ("NOSUCH", "NOB", "NOC") if f"undefined symbol {n}" in out]
    return p.returncode != 0 and len(named) == 3, f"status {p.returncode}, {len(named)} of 3 named"


def check_dcpp():
    d = os.path.join(WORK, "dcpp")
    os.makedirs(os.path.join(d, "CMDS"))
    objs = run(["make", "-s", "-f", "GNUmakefile", "print-dcpp"], NATIVE).stdout.decode().split()
    have = [os.path.join(NATIVE, "obj", "dcpp", o + ".r") for o in objs]
    if not objs or not all(os.path.exists(f) for f in have) or \
            not os.path.exists(os.path.join(NATIVE, "bin", "dcc68")):
        return False, "build native/ first (make -f GNUmakefile there)"
    run([LLINK, "-o=CMDS/dcpp16", "-n=dcpp16", "-M=16", os.path.join(LIB, "cstart.r")] + have +
        ["-l=" + os.path.join(LIB, "sys.l"), "-l=" + os.path.join(LIB, "clib.l")], d)
    shutil.copy(os.path.join(NATIVE, "bin", "dcc68"), os.path.join(d, "CMDS"))
    for f in glob.glob(os.path.join(COCOC, "Defs", "**", "*"), recursive=True):
        if os.path.isfile(f):
            cr_copy(f, os.path.join(d, "DEFS", os.path.relpath(f, os.path.join(COCOC, "Defs"))))
    cc09 = os.path.join(COCOC, "Source/Compiler/CC09")
    for f in ("lex.c", "cj.h"):
        cr_copy(os.path.join(cc09, f), os.path.join(d, "src", f))
    out = os.path.join(d, "src", "lex.i")
    p1 = os9run(d, "src", os.path.join(d, "CMDS/dcpp16"), ["lex.c"], out)
    record = b"out of memory" in open(out, "rb").read() if os.path.exists(out) else False
    p2 = os9run(d, "src", os.path.join(d, "CMDS/dcc68"), ["lex.i", "-o=lex.a"],
                os.path.join(d, "c68"))
    ok = record and p2.returncode != 0
    return ok, (f"dcpp status {p1.returncode} (by design), error record "
                f"{'written' if record else 'MISSING'}; dcc68 status {p2.returncode}")


def check_macros():
    d = os.path.join(WORK, "macros")
    os.makedirs(os.path.join(d, "solo"))
    for k in range(4):
        lines = [f" psect m{k},0,0,0,0,0"]
        for i in range(60):
            lines += [f"mac{i} macro", f" ldd #{i}+{k}*100+\\1", " std \\2,x", " endm"]
        for r in range(40):
            lines += [f" mac{i} {r},{i}" for i in range(60)]
        lines.append(" endsect")
        open(os.path.join(d, f"m{k}.a"), "w").write("\n".join(lines) + "\n")
        run([LAS, f"../m{k}.a", f"-o=m{k}.r"], os.path.join(d, "solo"))
    bad = 0
    for _ in range(3):
        procs = [subprocess.Popen([LAS, f"m{k}.a", f"-o=m{k}.r"], cwd=d, env=lwdiff.clean_env(),
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                 for k in range(4)]
        for p in procs:
            p.wait()
        for k in range(4):
            a = open(os.path.join(d, f"m{k}.r"), "rb").read()
            if a != open(os.path.join(d, "solo", f"m{k}.r"), "rb").read():
                bad += 1
    left = [f for f in os.listdir(d) if f.endswith(".tmp")]
    return bad == 0 and not left, f"{bad} of 12 concurrent runs differ, {len(left)} work files left"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--keep", action="store_true", help=f"keep {WORK}")
    args = ap.parse_args()
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    results = []
    for name, check in (("longret", check_longret), ("errexit", check_errexit),
                        ("dcpp", check_dcpp), ("macros", check_macros),
                        ("regwidth", check_regwidth), ("unsgn", check_unsgn),
                        ("cstr", check_cstr), ("undef", check_undef)):
        ok, detail = check()
        results.append(ok)
        print(f"{'pass' if ok else 'FAIL'}  {name}: {detail}")
    if not args.keep:
        shutil.rmtree(WORK, ignore_errors=True)
    print(f"silent: {sum(results)} of {len(results)} checks pass")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
