# devsys — native NitrOS9 development system for the F256

Goal: build programs on the Foenix F256 itself (6809, NitrOS9 **Level 2
only**) from the same sources lwasm assembles on Linux. Plan: fork the C
version of RMA as the single assembler (named `las`, in `las/`), use DCC as the C compiler, and use ROF
+ our rlink fork (`llink`) as the only native object format.

## Repository

`devsys` is one git repo (branch `main`, no remote) holding `las/`, `docs/`,
and our forks of rlink (`llink/`, renamed `llink`, D42) and DCC (`dcc/`). No upstream remotes: this is a clean
break, nothing is contributed back. The user will set up the project's own
remote later; don't add one or push.

## Key files

| File | What it is |
|---|---|
| `las/` | The assembler: our fork of RMA (from upstream commit `e82adeb`, no upstream history kept). Build: `cd las/build/unix && make`. |
| `llink/` | Our fork of rlink (D13), renamed `llink` (D42) so it never clashes with the original `rlink` on PATH or in `/dd/CMDS`: long names, lwlink's module layout, library choice and module order (`lwsel.c`), chosen from the mainline (`__lwsec`) or forced with `--lwlink`/`--mw`. Internal file names kept (`rlink.h`, `rlink_main.c`). Build: `cd llink && make -f GNUmakefile`. The psect being linked is kept in 8K blocks (`bigbuf.c`, D60: F$AllRAM and a window on OS-9). |
| `dcc/` | Our fork of DCC (D18): `CPP` (dcpp), `CC09` (dcc68), `COpt` (dco68), `DCC` (the `dcc` driver, runs `las`). Build: `make -f GNUmakefile` in each. dcc68's symbols and lists are banked (D57, `CC09/symdb.c` on `bank.c`, las's storage): read and write fields with `SG_F(s)`/`SP_F(s,v)`; `SYMP(s)` maps a record, good only until the next access to another symbol or cell; on the 6809 the handle goes in direct-page `symh`. After any change to dcc68: `python3 harness/symcheck.py` (against HEAD's dcc68: plain, `CC09-bankwin` one-window and `CC09-bank16` 2-byte-field builds on ~350 inputs; `--add FILE.c` for more, e.g. jfed's `te.c`). Header images (D50) were removed in D57. dcc68's messages are codes (`CC09/msgs.h`) read from `/dd/SYS/dcc68.msg` (`CC09/dcc68.msg` on Linux, made from `msgs.h`): dcc68 is 471 bytes under 48,384 (D61: no `rewind`, so no `fseek`). `python3 harness/dccdiff.py [--names8]` compiles every CoCoC C source with it and with CoCoC's binaries and compares; `--names8` ignores differences that are only longer names. |
| `clib/` | The C libraries (step 18, D52): our fork of CoCoC's KLibc and SysLib, built by devsys's DCC and las (`cd clib && make -f GNUmakefile` → `lib/cstart.r`, `clib.l`, `clibt.l`, `sys.l`; native/GNUmakefile runs it). Whole names, so D19's aliases are no longer needed; `defs/os9defs.a` is CoCoC's without its unfinished CMOC blocks (installed on the test and card disks). `python3 harness/libdiff.py A.l B.l [--names8]` compares two libraries member by member (CoCoC's `Lib/` is the reference: see D52 for each difference). |
| `make/` | Our fork of CoCoC's `make` (Tim Koonce's, D45): built for the 6809 by `native/GNUmakefile` (tool `make`); `make.default` holds the devsys rules (`dcc`, `las`), installed as `/dd/SYS/make.default`. Tested by `native.py make`. |
| `las/sizing.c` | lwasm's `,pcr` sizing (D16): pass 1 is the trial (every `,pcr` long), `sz_finish` decides, and settling passes correct the values before pass 2 (D55; until then a child process, D38). Host las prints the settling passes with `LASSZ_TRACE=1`. If a lwasm-mode file grows by a byte or two, `python3 harness/sizediff.py [id]` lists the instructions whose length differs from lwasm's. |
| `harness/lwdiff.py` | Compares `las` with lwasm over the real Level 2 build. Run `python3 harness/lwdiff.py` after every change to `las` or `llink`; `--only NAME --verbose` for one file. `--nitros9 DIR --work DIR` checks another NitrOS9 tree (e.g. `~/projects/wild/main/nitros9`, work dir `harness/out/main-k2`) without touching the corpus results; `sizediff.py` takes the same `--work`. A file whose sizing trial failed is named at the end ("sizing trial failed"). Also replays the build's lwar/lwlink commands and compares linked programs (`--llink`, `--lwlink` options; its links rely on llink's layout detection). Object-format files get the constants library as `--defs`. Report in `harness/reports/`. `harness/rofdump.py file.r` prints a ROF file. |
| `corpus/` | NitrOS9 source the harness assembles: clones of the user's forks (`jfed6000/nitros9`, `jfed6000/nitros9-languages`, branch `devsys`), pinned by `harness/corpus.lock`. Not version-controlled. Set up or restore with `harness/corpus.sh`; `harness/corpus.sh update` moves to the newest `devsys` commits. Don't edit files in it. |
| `dcc/COpt/bank.c` | dco68 keeps the whole file (D51): instructions, labels, reference cells and operand texts are banked records reached by 16-bit handles (`INS`/`LAB`/`CHN`/`ARG`, reads `I_SUCC`…, in `op.h`); one rule: a mapped pointer is good only until the next access to another record. Handles count 8-byte units (62 blocks, D57); operand and label texts are as long as their text (`talloc`/`tfreerec`, change an operand with `setarg`), an instruction's mnemonic is an index into `mnemtab` (`MNEMOF(i)`, a string that stays valid; `mnemix` interns one) (D58). After any change to `dcc/COpt`: `python3 harness/dcocheck.py` (against HEAD's dco68: plain, one-window and four-window builds on ~560 inputs). Native dco68 is three blocks (D61): its code must reach `$E000`, or `F$MapBlk` maps a window over the system page, so `topslot.c` (linked last) pads it with zeros and checks at start; 17,692 bytes, about 6K to grow (take zeros away when it needs more; `harness/topslot.py`, run by `native/GNUmakefile`, says how many). Three windows: the module, dco68's data (two slots) and the windows fill the 64K. |
| `dcc/CC09/fp6809.c` | Linux dcc68 keeps float/double constants in the 6809's format and builds them with a model of `cfloats.a` (D52), so Linux and the F256 write the same bytes. After any change to it, or to dcc68's constant handling (`lex.c` `number`/`normaliz`, `scale.c`): `python3 harness/fltcheck.py` (native dcc68 on os9run against Linux dcc68; 4,000 generated declarations and edge cases, about 5 minutes). |
| `harness/silent.py` | Step 14's checks (D43, 2 s): a long/double returned in X survives dco68 (`harness/native/longret.c`, also built and run natively by `native.py cc`); las's fatal errors exit non-zero; dcpp out of memory stops `dcc`; concurrent las runs don't share a macro file; dcc68's register reuse keeps char and int apart (`harness/native/regwidth.c`, D53, also run natively by `native.py cc`). Check 6: an unsigned comparison with 0 tests no stale carry (`harness/native/unsgn.c`, D59: dco68's `cmp #0` rules, `dcc/COpt/level1.patterns`). Check 7: dcc68 removes its `cstr.XXXXXX` strings file after a fatal error (D61; Linux and native in the model). Run after changing DCC or las. devsys's `dco68` reads its patterns from `dcc/COpt/level?.patterns` (CoCoC's from `~/.dcc`). |
| `harness/regress.py` | Checks that a change to `las` did **not** change behaviour: compares the working tree with `HEAD` (or `--ref REV`) on ~740 inputs. Must pass for refactors (phases 1, 2, 8, 9). `--skip lwdiff` when a change is meant to alter NitrOS9 results (then check the RMA/DCC differences one by one, as DESIGN.md §8 describes for phase 3). |
| `las/build/stats`, `las/build/asan` | Extra builds of the same source: `stats` (`-DSYMSTATS`) prints symbol table memory, used by `harness/symstats.py`; `asan` checks memory errors: `ASAN_OPTIONS=detect_leaks=0 python3 harness/regress.py --current las/build/asan/las`. Run the asan check after any change to how memory is allocated. For llink, build a sanitizer copy in scratch (`make -f GNUmakefile CFLAGS="-g -m32 -Wno-implicit-int -fsanitize=address" LFLAGS="-m32 -fsanitize=address"`) and pass it to lwdiff with `--llink`. |
| `boot/wildbits/l2/` | The F256 boot disks the harnesses use (D44): `make PLATFORM=k2` and `make PLATFORM=jr2` there build `l2_wildbits_devsys{k2,jr2}.dsk` from NitrOS9 `main` (`~/projects/wild/main/nitros9`; `make N9=DIR` for another tree): that tree's own l2 recipe plus PipeMan/Piper/Pipe and `/nil`. Rebuild after `main` changes, **clean**: `rm -rf .mods .obj .lib` there first (`main`'s makefiles don't track the defs a source includes, and `lwar -c $@ $?` keeps only a library's changed member; `docs/nitros9-fixes.md` #2, #4). The stand-in `rules.mak`, `libs.mak`, `wildbits.mak` let NitrOS9's relative includes find the real ones. |
| `native/GNUmakefile` | Phase 7 cross-build: compiles las, llink and DCC for the 6809 with the host DCC passes, `las` and `llink` into `native/bin/` (`make -f GNUmakefile`; `obj/TOOL.map` + `obj/TOOL/*.lst` give every address a name). DESIGN.md D20, D25. After linking a tool that maps windows (las, llink, dcc68, dco68) it runs `harness/topslot.py`, which fails the build if the module leaves the top memory slot free (D51, D61) and prints each one's room. |
| `harness/native.py` | Runs the cross-built tools on the F256 in MAME (suites `las`, `link`, `cc`, `make`; about 1.5 minutes) and compares with the host tools. Suite `lw` assembles the 200 NitrOS9 module sources natively (`--only NAME`); suite `obj` assembles the 90 object sources, builds the libraries and links the 13 `.as` programs (phase 10); suite `self` (step 17, D51) builds every tool on the F256 with make and native llink and compares each module with `native/bin` (`--only TOOL`); suite `img` assembles level1/cmds without and with a defs image (phase 9). Run long suites in parallel: `--shard K/N --work DIR --no-build` (run `make -f GNUmakefile` in `native/` first; 8 shards of `lw` take about 22 minutes). Native las options that don't fit the shell's 200-character line go in a file: `las src --args=FILE`. After any change to las, llink or DCC, run **every** suite, not only the one you think is affected (DESIGN.md §8). `--keep` keeps the test image. |
| `harness/card.py` | The install disk and real hardware (K2, SD card; D47): `build` makes `harness/out/devsys_k2.dsk` from the devsys boot image: tools, C libs/headers, NitrOS9 `main`'s defs (`/dd/DEFS/NITROS9`) and libraries (`alib.l`, `net.l`, `wildbitsl2.l` in `/dd/LIB`, assembled by the host las), las options files `/dd/SYS/n9mod.opt`/`n9obj.opt`, help entries (`install/help/*.hp` into `helpmsg`), `/dd/DEVSYS` (README, EX examples, selftest). The disk's own files live in `install/`. The owner `dd`s it to the card and types `/dd/DEVSYS/selftest`; `check FILE` compares the card read back with the Linux tools (and with lwasm/lwlink's `main` build); `mame` rehearses the same disk on the Jr2 image (`-Djr2`). `N9=DIR` for another NitrOS9 tree. `build --add FILE` also puts a file of your own in `/dd/DEVSYS` (step 20: `--add ~/projects/f256/jfed/te/te/te.c`). |
| `harness/os9run.py` | Runs one OS-9 program on a host model (6809 from `harness/cpu6809.py` + system calls). For debugging native failures in seconds: `--trace`, `--stats`, `--map native/obj/las.map --break e_report` (`--break NAME+HEX` for an address inside a function); `--profile` (instructions per function, per process, children included) and `--mark NAME` (instruction count each time NAME is entered: splits a run into passes, e.g. `--mark sz_pass`). Not the proof; MAME is. |
| `las/symtab.c`, `las/bank.c` | Phase 8: symbols are handles (`SYMH`), read and changed only through `symtab.c`'s accessors; storage is 8K banked blocks (`bank.c`). In `symtab.c`, never use two entries in one statement (a pointer is good only until another block is mapped). After any change there: `python3 harness/regress.py --current las/build/bankwin/las` (emulates the 6809's single window, DESIGN.md D27). |
| `las/defsimg.c` | Phase 9: precompiled defs, `las --defsimg=FILE` (DESIGN.md D35). After any change to las: `python3 harness/imgcheck.py` (290 jobs with and without an image, per configuration; `--share one` stress test; `--variants` must-fall-back cases; `--las` for the asan/bankwin builds). `harness/imgdump.py FILE [--6809]` prints an image. Native las must stay ≤ 48,384 bytes, its data in one 8K block (D37) |
| `las/build/nosz` | Host las built with `-DNOSIZING` (no `,pcr` sizing). Was the reference for native lwasm-mode output until step 13 (D21); native las now sizes too (D55: the same in-process trial as Linux), so the harnesses compare with `las/build/unix`. |
| `docs/nitros9-fixes.md` | Problems found in NitrOS9 `main`, to fix later on a branch and submit upstream as PRs. `~/projects/wild/main/nitros9` is a pristine copy of upstream `main`: never edit it; add an entry here instead. |
| `docs/DESIGN.md` | Decisions, measurements, roadmap (§9), open questions (§10). The project's record. |
| `docs/learn/toolchain.html` | Teaching primer for the project owner. |
| `docs/learn/GUIDE.md` | Rules for maintaining the primer, and its topic backlog. |
| `docs/learn/examples/` | Sources for the primer's examples; `out/` is generated. |
| `docs/learn/tools/` | `build_examples.sh` regenerates examples; `check_hex.py` verifies the page's hex dumps; `hash_data.py` recomputes chapter 9's hashing figures. |

Related trees (reference sources; don't modify without asking):
`~/projects/dcc/rma` (RMA C source), `~/projects/dcc/mw-rlink` (the original rlink; `~/projects/bin/rlink` is its binary),
`~/projects/dcc/CoCoC` (DCC), and the user's own NitrOS9 working trees in
`~/projects/wild/Sept2026/`. Use `corpus/` for NitrOS9 source, not those.

## Every session

**At the start:** read `docs/DESIGN.md` §9 (roadmap) and §10 (open questions)
to see where things stand.

**Before finishing work:**

1. Update `docs/DESIGN.md`: record new decisions with their reasons, tick or
   answer open questions, add new ones, and update the roadmap status column.
2. Update the primer per `docs/learn/GUIDE.md`. If you explained a concept
   to the user in chat, or a roadmap phase finished, the primer probably
   needs a new or extended section. If you changed a hex dump, run
   `docs/learn/tools/build_examples.sh` and
   `python3 docs/learn/tools/check_hex.py`.
3. Add a line to the changelog in `docs/learn/GUIDE.md` if the primer
   changed.

The user wants to understand the work, not just see results. Explain the why,
using examples from their own trees.

## Environment notes

- `LD_PRELOAD` prints a harmless `libgtk3-nocsd` error from every program.
  `unset LD_PRELOAD` before capturing tool output into files.
- The `lwasm` on PATH is a wrapper script that adds relative
  `--map`/`--list` options. For scripted runs use
  `~/projects/coco-shelf/bin/lwasm.orig` with `--output=FILE` and `-I DIR`.
- NitrOS9 lwasm flags are in `corpus/nitros9/rules.mak` (`AS` and `ASM`). The
  Level 2 F256 defs come from `-I nitros9/defs -I nitros9/level2/wildbits -Dwildbits=1`.
- After a fresh checkout of `devsys`, run `harness/corpus.sh` before the harness.
- Native testing (phase 7 on) uses the Wildbits MAME: see DESIGN.md "Phase 7
  test plan" and `~/projects/wild/tempest/docs/port-guide.md` §13. Never modify
  MAME, and mount a copy of a disk image, never the original.
- `las` has two modes: RMA behaviour and lwasm behaviour (`--lwasm`;
  DESIGN.md D12, §4). With neither `--lwasm` nor `--rma`, las picks the mode
  from the source (D40: `psect` → RMA, `section`/`mod` or nothing → lwasm).
  NitrOS9 sources also need the build's `--pragma=` list; the harnesses pass
  it with `--lwasm` (except `native.py lw`, which tests the detection). A change for lwasm compatibility
  normally goes behind `LwasmMode`, so `regress.py --skip lwdiff` (RMA/DCC
  inputs) must stay 450/450.
- Host builds: `las` at `las/build/unix/las`; the unmodified RMA at
  `~/projects/dcc/rma/build/unix/rma` is the reference for "no behaviour change" checks; DCC passes at
  `dcc/{CPP/dcpp,CC09/dcc68,COpt/dco68}` (CoCoC's originals in
  `~/projects/dcc/CoCoC/Source/Compiler/` are the reference for `dccdiff.py`).
