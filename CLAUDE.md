# devsys — native NitrOS9 development system for the F256

Goal: build programs on the Foenix F256 itself (6809, NitrOS9 **Level 2
only**) from the same sources lwasm assembles on Linux. Plan: fork the C
version of RMA as the single assembler (named `las`, in `las/`), use DCC as the C compiler, and use ROF
+ rlink as the only native object format.

## Repository

`devsys` is one git repo (branch `main`, no remote) holding `las/`, `docs/`,
and our forks of rlink (`rlink/`) and DCC (`dcc/`). No upstream remotes: this is a clean
break, nothing is contributed back. The user will set up the project's own
remote later; don't add one or push.

## Key files

| File | What it is |
|---|---|
| `las/` | The assembler: our fork of RMA (from upstream commit `e82adeb`, no upstream history kept). Build: `cd las/build/unix && make`. |
| `rlink/` | Our fork of rlink (D13): long names, `--lwlink` mode (lwlink's module layout, library choice and module order, `lwsel.c`). Build: `cd rlink && make -f GNUmakefile`. |
| `dcc/` | Our fork of DCC (D18): `CPP` (dcpp), `CC09` (dcc68), `COpt` (dco68), `DCC` (the `dcc` driver, runs `las`). Build: `make -f GNUmakefile` in each. `python3 harness/dccdiff.py [--names8]` compiles every CoCoC C source with it and with CoCoC's binaries and compares; `--names8` ignores differences that are only longer names. |
| `las/sizing.c` | lwasm's `,pcr` sizing (D16): a trial pass 1 in a child process decides sizes (Linux `fork()` + pipe; OS-9 `F$Fork` of las itself + a shared block, D38). If a lwasm-mode file grows by a byte or two, `python3 harness/sizediff.py [id]` lists the instructions whose length differs from lwasm's. |
| `harness/lwdiff.py` | Compares `las` with lwasm over the real Level 2 build. Run `python3 harness/lwdiff.py` after every change to `las` or `rlink`; `--only NAME --verbose` for one file. Also replays the build's lwar/lwlink commands and compares linked programs (`--rlink`, `--lwlink` options). Object-format files get the constants library as `--defs`. Report in `harness/reports/`. `harness/rofdump.py file.r` prints a ROF file. |
| `corpus/` | NitrOS9 source the harness assembles: clones of the user's forks (`jfed6000/nitros9`, `jfed6000/nitros9-languages`, branch `devsys`), pinned by `harness/corpus.lock`. Not version-controlled. Set up or restore with `harness/corpus.sh`; `harness/corpus.sh update` moves to the newest `devsys` commits. Don't edit files in it. |
| `harness/regress.py` | Checks that a change to `las` did **not** change behaviour: compares the working tree with `HEAD` (or `--ref REV`) on ~740 inputs. Must pass for refactors (phases 1, 2, 8, 9). `--skip lwdiff` when a change is meant to alter NitrOS9 results (then check the RMA/DCC differences one by one, as DESIGN.md §8 describes for phase 3). |
| `las/build/stats`, `las/build/asan` | Extra builds of the same source: `stats` (`-DSYMSTATS`) prints symbol table memory, used by `harness/symstats.py`; `asan` checks memory errors: `ASAN_OPTIONS=detect_leaks=0 python3 harness/regress.py --current las/build/asan/las`. Run the asan check after any change to how memory is allocated. For rlink, build a sanitizer copy in scratch (`make -f GNUmakefile CFLAGS="-g -m32 -Wno-implicit-int -fsanitize=address" LFLAGS="-m32 -fsanitize=address"`) and pass it to lwdiff with `--rlink`. |
| `native/GNUmakefile` | Phase 7 cross-build: compiles las, rlink and DCC for the 6809 with the host DCC passes, `las` and `rlink` into `native/bin/` (`make -f GNUmakefile`; `obj/TOOL.map` + `obj/TOOL/*.lst` give every address a name). DESIGN.md D20, D25. |
| `harness/native.py` | Runs the cross-built tools on the F256 in MAME (suites `las`, `link`, `cc`; about 1 minute) and compares with the host tools. Suite `lw` assembles the 200 NitrOS9 module sources natively (`--only NAME`); suite `obj` assembles the 90 object sources, builds the libraries and links the 13 `.as` programs (phase 10); suite `img` assembles level1/cmds without and with a defs image (phase 9). Run long suites in parallel: `--shard K/N --work DIR --no-build` (run `make -f GNUmakefile` in `native/` first; 8 shards of `lw` take about 22 minutes). Native las options that don't fit the shell's 200-character line go in a file: `las src --args=FILE`. After any change to las, rlink or DCC, run **every** suite, not only the one you think is affected (DESIGN.md §8). `--keep` keeps the test image. |
| `harness/card.py` | Real hardware (K2, SD card): `build` makes `harness/out/devsys_k2.dsk` (tools, C libs/headers, `/dd/DEVSYS/selftest`); the owner `dd`s it to the card and types `/dd/DEVSYS/selftest`; `check FILE` compares the card read back with the Linux tools; `mame` rehearses the same card on the Jr2 image. |
| `harness/os9run.py` | Runs one OS-9 program on a host model (6809 from `harness/cpu6809.py` + system calls). For debugging native failures in seconds: `--trace`, `--stats`, `--map native/obj/las.map --break e_report` (`--break NAME+HEX` for an address inside a function). Not the proof; MAME is. |
| `las/symtab.c`, `las/bank.c` | Phase 8: symbols are handles (`SYMH`), read and changed only through `symtab.c`'s accessors; storage is 8K banked blocks (`bank.c`). In `symtab.c`, never use two entries in one statement (a pointer is good only until another block is mapped). After any change there: `python3 harness/regress.py --current las/build/bankwin/las` (emulates the 6809's single window, DESIGN.md D27). |
| `las/defsimg.c` | Phase 9: precompiled defs, `las --defsimg=FILE` (DESIGN.md D35). After any change to las: `python3 harness/imgcheck.py` (290 jobs with and without an image, per configuration; `--share one` stress test; `--variants` must-fall-back cases; `--las` for the asan/bankwin builds). `harness/imgdump.py FILE [--6809]` prints an image. Native las must stay ≤ 48,384 bytes, its data in one 8K block (D37) |
| `las/build/nosz` | Host las built with `-DNOSIZING` (no `,pcr` sizing). Was the reference for native lwasm-mode output until step 13 (D21); native las now sizes too (D38: `F$Fork`s itself, sizes back in a shared RAM block), so the harnesses compare with `las/build/unix`. |
| `docs/DESIGN.md` | Decisions, measurements, roadmap (§9), open questions (§10). The project's record. |
| `docs/learn/toolchain.html` | Teaching primer for the project owner. |
| `docs/learn/GUIDE.md` | Rules for maintaining the primer, and its topic backlog. |
| `docs/learn/examples/` | Sources for the primer's examples; `out/` is generated. |
| `docs/learn/tools/` | `build_examples.sh` regenerates examples; `check_hex.py` verifies the page's hex dumps; `hash_data.py` recomputes chapter 9's hashing figures. |

Related trees (reference sources; don't modify without asking):
`~/projects/dcc/rma` (RMA C source), `~/projects/dcc/mw-rlink`,
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
