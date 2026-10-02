# F256 NitrOS9 Native Development System — Design

Status: draft, 2026-10-02
Target: Foenix F256, 6809, **NitrOS9 Level 2 only**
Companion: `docs/learn/toolchain.html` (background and concepts)

---

## 1. Goal

Build programs on the F256 itself, under NitrOS9 Level 2, from the **same source
files** that the Linux cross-build assembles with lwasm. A file must assemble
the same way on the F256 and on Linux, ideally to identical bytes.

Out of scope for now: Level 1, other CoCo platforms, ANSI C.

## 2. Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | **Fork the C version of RMA** (`~/projects/dcc/rma`) as the one native assembler | Both C compilers emit RMA syntax, so RMA is needed anyway. It's C, so it builds on Linux for testing. Already has macros, `rept`, ROF output. Its source compiles under both K&R and ANSI C. |
| D2 | Do **not** extend `asm.asm` | 4,069 lines of disassembled 6809, no macros, no ROF output, names fixed at 8 bytes in the symbol entry layout. |
| D3 | Do **not** port lwasm or mamou | lwasm is C99 and too big for a 64K process. Mamou is ANSI C (DCC can't compile it), has no ROF output, and is marked deprecated. |
| D4 | **DCC** as the C compiler, built with `-2` | Real C source (`CoCoC/Source/Compiler/CC09`), builds on Linux, better code than Microware C. The Microware C compiler only exists as a disassembly. |
| D5 | **ROF + rlink** is the only object/library format on the F256 | LWOBJ/lwar never need to exist natively. Libraries used by `.as` programs get rebuilt as ROF `.l`. |
| D6 | Keep all three **source** styles | So the same files keep building with lwasm on Linux. |
| D7 | Symbol table: **compact entries + banked blocks + precompiled defs**, behind one interface | See §6. Memory measurements show long names don't fit otherwise. |
| D8 | The fork is named **`las`** and lives in `devsys/las`, inside the single `devsys` git repo. Clean break from upstream: forked from `gitlab.com/debmint/rma` commit `e82adeb`, no upstream history or remotes kept; we won't contribute back. The project will get its own remote later (owner to set up) | It will behave differently from RMA (lwasm syntax, module output, long names), and must coexist with Microware `rma`/`c.asm` on the F256. DCC calls the assembler by name: change `ASSEMBLER` in `CoCoC/Source/Compiler/DCC/dcc.h:24` to `"las"`. |
| D9 | A module is assembled as a **psect at address 0**, in `las/module.c` | In a psect, RMA's code bytes hold offsets from the psect start, which for a module starting at 0 are the final addresses, so RMA's tested instruction code is reused and relocation records are simply not written. Module labels and `*` are plain numbers (no relocation flag), as in lwasm, so any expression lwasm accepts works. |
| D10 | lwasm's location counters: outside a module one counter (`*` = `.`; `org` sets it, `rmb` advances it); inside `mod`…`emod` two (`*` code, `.` data, both from 0; `rmb`/`org` work on `.`). `rmb` never writes bytes. Output is every emitted byte in order. | Measured from lwasm 4.25's behaviour, not its source (the source suggested a quirk the binary doesn't have). |
| D11 | `-D NAME[=value]` (upper-case D; `-d` stays RMA's page depth) defines a `set` symbol before assembly | The harness needed it: RMA assembles each input file as a separate unit, so a "defines" prelude file wrote its own (empty) ROF trailer into module output. |
| D12 | **lwasm mode** (`--lwasm`) holds every lwasm-specific behaviour; default las stays RMA | NitrOS9 needs lwasm's semantics, but RMA's own sources (and the regression guard) depend on RMA's. Key choice inside it: `ifp1` is true in both passes (as lwasm, which doesn't support it), and **conditionals are decided in pass 1 and replayed in pass 2** (`cond_log`, 1 bit each). lwasm decides conditionals once, when it first reads a line, but resolves values later: an include guard (`ifne OS9.D-1 / OS9.D set 1`) must stay true in pass 2, yet a forward reference to a `set` symbol gets its final value. Resetting `set` symbols before pass 2 got the first right and the second wrong; replay gets both. |
| D13 | **rlink forked** into `devsys/rlink` from the owner's mw-rlink working tree (commit 0c6de25 = the CoCo-native source, plus the owner's uncommitted Linux port) | HEAD includes `/dd/defs/rof.h` and CoCo types, so it can't build on Linux; the working tree is the port and matches the owner's working binary. Validated by linking DCC-compiled `tr` against CoCoC's real `cstart.r`/`clib.l`: a valid module with the shipped binary's structure |
| D14 | rlink **`--lwlink`** mode for modules from lwasm-style sources: header, code from `$0D`, name, edition byte only if `__os9` gave one, CRC; no C initialised-data tables; lwlink's header defaults (type 1, lang 1, attr 8, rev 0, stack 0 plus `__os9` stack); library members chosen as lwlink does (`lwsel.c`: repeat until nothing new is needed, lay out in library order); duplicate exports allowed | Microware's layout (name first, cstart tables, name again) is for C programs and can never equal lwlink's |
| D15 | las **`--defs=FILE.r`** preloads constants a ROF library exports. They stay external references (lwasm's sizes, rlink fills them in) except where ROF can't express the reference or the value is needed while assembling | lwasm's object format hands the linker whole expressions (`(Prgrm+Objct)*256`, `^Carry`, `type equ Prgrm`); ROF only "add this symbol". A first step toward phase 9's precompiled defs |
| D16 | **`,pcr` sizes chosen by replaying lwasm's rule** (`las/sizing.c`): a trial pass 1 in a forked child assembles with every `,pcr` long and records each site, each line lwasm would leave unsettled, and each code label's section; at its end the child replays lwasm's resolve1/resolve2 (settle when the span is known and fits ±127, or when the worst case is within ±100, a `lea` line has no worst case; when stuck, force the first unsettled line long) and pipes one byte per site to the parent, which then assembles for real | Matching lwasm needs its exact rule, not just "shortest that fits": its ±100 margin and forced order decide real cases. A clean child avoids resetting pass-1 state (symbol definitions, equate refs, macros, the condition log), which phase 4's `set_reset` showed to be fragile; pass 2's phase check still guards the result |
| D17 | rlink `--lwlink` **lays modules out the way lwlink does**: by section rank (code, .text, data, .data), then command-line position with libraries at their place, then library order; las exports the rank as the constant `__lwsec`; the mainline may be anywhere and the entry point is `__start` | NitrOS9 links `-lwildbitsl2 -lnet -lalib prog.o`, so lwlink puts a `net` member (section `code`) before the program and an `alib` member (`.text`) after it |
| D18 | **DCC forked** into `devsys/dcc` from the owner's CoCoC working tree (68fdbf9 plus uncommitted edits, kept as `dcc/uncommitted-at-fork.patch`): identifiers significant to **31** characters; symbol entries keep their old 8-character room and grow only for longer names; the optimizer takes 32-character labels (31 plus a spare byte); the driver runs `las` | 31 is C89's significance for internal names, well inside las's and rlink's 63. Compact growth keeps memory unchanged for short names on the 6809, as in phase 2 |
| D19 | rlink matches a name from a **ROF version 0** module (Microware's tools) on **8 characters**: a long reference (`localtime`) to an 8-character definition (`localtim` in `clib.l`) gets an alias under the long name in the defining module. Version 1 modules (las) match exactly | Existing C libraries were built with 8 significant characters; rebuilding them is phase 10. Matching as the old toolchain did keeps them usable, without blurring names between new objects |
| D20 | **Native tools are cross-built on Linux** by `native/GNUmakefile`: each `.c` goes through the host `dcpp` → `dcc68` → `dco68` → `las`, and host `rlink` links `cstart.r` + objects + `sys.l` + `clib.l` from CoCoC. Extra memory per tool (`-M`, pages): rlink 10, dco68 10 and dcpp 135 as in their original native makefiles; dcc68 10 (D25). The optimizer runs with `-2` (D4) | Same passes the `dcc` driver runs on the F256, so the native build *is* a DCC build, and the host tools are the ones already verified. Building on the F256 itself comes later (las and dcc68 can't be linked natively until phase 8) |
| D21 | **Reference for native las output: `las/build/nosz`** (host las built with `-DNOSIZING`) | `sizing.c` needs `fork()`, so the 6809 las keeps RMA's long `,pcr` forms in lwasm mode. Native output is compared with a host build that does the same; RMA mode is unaffected (sizing is lwasm-only) |
| D22 | **Two native test tools.** `harness/native.py` runs the tools in the Wildbits MAME and compares their output with the host's: the proof. `harness/os9run.py` runs an OS-9 program on a host model (the owner's `cpu6809.py` from tempest, plus the system calls a CoCoC program makes), with call traces and backtraces by function name: the debugging aid | A MAME round trip takes minutes and shows only the end result; the model reproduced every MAME result in seconds and named the failing function |
| D23 | rlink stores **names at their own length** (`setname`, `misc.c`) instead of `SYMLEN + 1` arrays; module names in `MAXNAME + 1` | At 64 bytes a name (phase 5's `SYMLEN` 63), linking anything against `clib.l` ran the 6809 out of memory. Same idea as las's compact entries (phase 2) |
| D24 | **All use of memory beyond one 64K process goes in phase 8**: las's symbol table *and* rlink's link of large programs (las, dcc68) | Owner's decision, 2026-10-02: handle all block switching in one phase. The F256 has 1800K+ of RAM |
| D25 | **dcc68 fits the F256's module limit**: compiled with `-s` (no stack checks, as natively), without `-DPROF` (`dcc -p` profiling is unavailable natively), and with `sbrk` instead of `ibrk` for its tables (`CC09/cj.h`), starting from a small data area. Module 48,290 bytes | Measured in MAME with test modules: a module in 6 blocks may be at most **48,384 bytes** (it must end by `$FD00`; the top 768 bytes are reserved on this NitrOS9), else it takes a 7th block. And F$Fork refuses (`#207`) a process whose module blocks plus *initial* data blocks fill all 8 slots, while F$Mem can grow a running process to 8 (native las did, 24K of data). CoCoC's shipped dcc68 (48,521 bytes, 16K initial data) can't start on the F256 for both reasons |
| D26 | **Symbols are opaque handles** (phase 8 step 1): callers get a `SYMH`/`REFH` (an unsigned int) and read or change a symbol only through `symtab.c`'s accessors (`sym_typ`, `sym_setmsk`, `sym_name`, `ref_new`, ...); `struct symblstr` and `struct ref_xtrnal` are private to `symtab.c`. `sym_setval` is a macro that casts to `SVAL` | A pointer into banked storage goes stale when another block is mapped, so no code outside `symtab.c` may hold one; making the structs invisible lets the compiler enforce it (every old access was a compile error). The cast guards the 6809 build, where a value passed without a prototype must already be a `long` |
| D27 | **Banked storage** (`las/bank.c`): symbols and reference records are allocated 2-byte aligned in 8K blocks from F$AllRAM; a handle is 16 bits, block+1 in the top 4 and offset/2 in the low 12 (15 blocks, 120K). One window slot, switched with F$ClrBlk + F$MapBlk (F$MapBlk takes the highest free slot, the one below las's module). `bank_done()` unmaps and F$DelRAMs at all three exits. Inside `symtab.c` no statement uses two entries | Largest need measured: 50.7K (`krnp2`), so 15 blocks leave room. A block F$AllRAM gave stays allocated after the process ends unless returned. `build/bankwin` emulates the single window on Linux by copying blocks in and out, so a stale pointer reads the wrong block: regress 741/741, and putting back one two-entry statement fails 242 of 741 (negative control) |
| D28 | las **`--args=FILE`** reads more arguments from FILE (separated by spaces or line ends) | The OS-9 shell reads at most 200 characters of a line, and cstart splits arguments at commas as well as spaces, so a NitrOS9 las command (`--pragma=` list, `-D`s) can't be typed natively. Measured in MAME: a 213-character line ran as two commands |
| D29 | rlink gets **no banked storage**: `lwsel.c`'s module table is allocated when `--lwlink` runs instead of being a 14K static array | That array was most of rlink's 19.7K of static data. Without it native rlink links las and dcc68 (the largest tools) in about 29K of its 40K, so banking rlink isn't needed for anything we build. Revisit if a larger program needs it |

## 3. Source styles the fork must accept

| Style | Example | Markers | Fork output |
|---|---|---|---|
| Module | `level1/wildbits/cmds/scfg.asm` | `mod` … `emod` | finished module (header + CRC), no linker |
| lwasm object | `level2/wildbits/cmds/gfxstatus.as` | `section code/bss`, `__start`, `export`/`import` | ROF → rlink |
| RMA object | `ccompiler/sources/cstart.a`, DCC output | `psect`/`vsect`/`endsect`, `label:` | ROF → rlink (already works) |

Mapping lwasm object style onto ROF:

| lwasm | ROF / RMA equivalent |
|---|---|
| `section code` | `psect` body |
| `section bss` | `vsect` |
| `__start` | psect entry parameter |
| `export` / `undefextern` | `label:` global / auto-external |
| lwlink header defaults (type, attr, stack, name) | root psect parameters — fork must supply defaults |

## 4. lwasm compatibility requirements

NitrOS9 assembles with:

```
lwasm --6309 --format=os9 --pragma=pcaspcr,nosymbolcase,condundefzero,undefextern,dollarnotlocal,noforwardrefmax
```

### lwasm mode

`las --lwasm` switches on lwasm's behaviour where it differs from RMA's
(decision D12). The harness passes it, with the build's `--pragma=` list, for
every NitrOS9 assembly. Without it `las` behaves like RMA, so the RMA/DCC
regression inputs keep guarding RMA behaviour (450 of 450 identical after
phase 4). A few fixes apply in both modes because they are plain bugs or
harmless there: line endings, `;`/indented comments, `use` file names ending
at a blank, over-long lines, a last line without a newline.

### Status after phase 4 (2026-10-02)

| Feature | Files | Status |
|---|---|---|
| `mod` / `emod`, module output | most | ✅ phase 3 |
| `-D NAME[=value]`, `--pragma=` | all | ✅ (`nosymbolcase`, `condundefzero` implemented; others accepted) |
| `;` comments (column 1, indented, after a label), `* ` indented | ~80 | ✅ both modes |
| CR, LF and CR LF line endings; last line without newline | 18 | ✅ both modes |
| `use name  ; comment`, trailing spaces after a `use` name | 7 | ✅ both modes |
| `ifp1` true in both passes; conditionals decided in pass 1 and replayed in pass 2 | all | ✅ lwasm mode (see D12) |
| case-insensitive symbols (`nosymbolcase`) | all | ✅ `HW.PAGE` = `HW.Page` |
| undefined symbol in a condition = 0 (`condundefzero`) | ~20 | ✅ |
| `ifdef`, `ifndef`, `if`, `endif` | 5 | ✅ lwasm mode |
| `foo@` / `foo?` local labels: scope ends at a blank line, a macro definition's end; an expansion gets its own scope and restores the caller's | 14+ | ✅ lwasm mode |
| operators `\|` `^`(xor) `%` `<<` `>>`, unary `~`, lwasm precedence | ~50 uses | ✅ lwasm mode. RMA binds `&`/`!` tighter than `*`/`+`; lwasm the reverse |
| `'A'` (closing quote), `"AB` two-character values, `0x` hex | several | ✅ lwasm mode |
| `fcb`/`fdb` values out of range keep the low bits (8-bit instruction operands still an error) | few | ✅ lwasm mode |
| code and data outside a module (`fcb`, instructions at top level), `org`/`rmb` at top level | 28 | ✅ |
| label on the `mod` line is address 0 | 1 | ✅ |
| `fcn`, `fill` | 2 | ✅ |
| `setdp` (RMA matched it as `set`) | 1 | ✅ lwasm mode: directive names must match exactly |
| `0,u` keeps a 5-bit zero offset; `<d,s` with symbol `d` is not D-register indexing | 3 | ✅ lwasm mode |
| `struct` / `endstruct` | 1 (`minted`) | ❌ not yet |
| `section` / `endsect` (lwasm object format) | 88 + `opts.o` | ✅ phase 5 |
| shortest instruction forms (`,pcr` 8/16-bit, …) | 39 DIFF files | ✅ phase 3b (D16) |
| `use` searched next to the *including* file for nested `use` | — | ❌ harness passes the source's directory first; nested `use` not exact |

## 5. Long symbol names

The defs use names up to 24 characters; 167 names collide at 8 characters
(e.g. `MMU_SLOT_0`…`MMU_SLOT_7`). The limit is in several tools:

| Tool | Limit | Location | Action |
|---|---|---|---|
| RMA (C fork) | 63 | `rma/rstruct.h:8` `SYMLEN 64` | OK; ROF output already writes full names (tested) |
| Microware RMA binary | 8 | `CoCoC/Cmds/rma`, `c.asm` | don't use |
| rlink | 9, **silently truncated** | `mw-rlink/rof.h:39`, `rl_02.c:671`, `rl_03.c:292` | ✅ 63 (phase 5); 8-character match for ROF version 0 objects (D19) |
| DCC | 8 | `CC09/cj.h:130` `NAMESIZE 8` | ✅ 31 (D18); optimizer labels 11 → 32 |

ROF stores names null-terminated, so the file format doesn't change.

## 6. Symbol table memory

### Budget

A Level 2 process has one 64K address space shared by its code, data and stack.
For a DCC-compiled RMA fork, expect roughly **25–30K** free for tables.

**Measured in phase 7:** the DCC-compiled las module is 34,149 bytes (RMA's
original native build: 20,143). Level 2 maps it in 8K blocks at the top of
the 64K (on this NitrOS9 a module must end by `$FD00`, D25), so data can
only reach `$6000` (24K). Static data and the
declared stack take 7,872 bytes, which leaves about **16K** for symbols.
On the F256, `scfg.asm` stops with "symbol table overflow"; it needs about
41K (1,933 symbols). The toolchain's own C sources fit (largest:
`rma_main.a`, 24,064 bytes of data in total, just under the limit).

### Measured demand (lwasm, Level 2 wildbits defsfile)

| Source | Symbols | Names > 8 chars | Name bytes |
|---|---|---|---|
| empty program (defs only) | 1,627 | 415 | 13.4K |
| `scfg.asm` | 1,911 | 497 | 15.5K |
| `rbf.asm` | 1,977 | 415 | 15.4K |
| `fm.asm` | 2,353 | 609 | 18.6K |

Current RMA entry = 76 bytes on the 6809 (`char symblnam[64]` inline plus 12 bytes of fields; 84 in the 32-bit Linux build) → `fm.asm` ≈ 179K.
Compact entry = 12 bytes + name + 1 (phase 2, done). Neither fits alone.

### Measured after phase 2 (las, 6809 sizes)

From `harness/symstats.py` (report: `harness/reports/symstats.md`), using the
`las/build/stats` measurement build over all regress inputs that run to the end:

| Inputs | Before (76 bytes/symbol) | After | Over 25K |
|---|---|---|---|
| 178 RMA sources + 198 DCC-compiled C files | largest 40.4K | largest 10.7K incl. references | 14 → 0 |
| 265 NitrOS9 Level 2 files (full defs) | median 109K, largest 185K | median 30.6K, largest 49K | 151 → 149 |

Compact entries cut the table about 3.5×. The C toolchain's own inputs now
fit; NitrOS9 sources don't, because the defs alone add about 1,500 symbols
(average name 8 characters). Phases 8 and 9 are still required for them.
NitrOS9 reference counts are a floor: those files stop generating code after
`mod` fails today.

### Design: one interface, three layers

```
RMA parser / expressions / output
          │  sym_find(name)  sym_add(name, value, type)
  ┌───────┴──────────────────────────────────────┐
  │ compact entries   — what one symbol looks like │
  │ banked blocks     — where entries live        │
  │ precompiled defs  — what gets loaded at all   │
  └──────────────────────────────────────────────┘
```

1. **Compact entries.** `char symblnam[1]` as the last member, allocated at
   `sizeof(entry) + strlen(name)`. No lookup cost. Remove or size-aware the free
   list at `labels.c:544` (`d078f`).
2. **Banked blocks.** Extra 8K RAM blocks via `F$AllRAM`, mapped with
   `F$MapBlk` into a window. Hash picks the bucket, bucket belongs to one block,
   so a lookup maps at most one block. Pointers become (block, offset).
3. **Precompiled defs.** Assemble `defsfile` once into a saved image of those
   blocks. On `use defsfile`, if the image is current, map it in read-only
   instead of parsing ~1,600 equates on each pass. One image per configuration;
   we only have one (`Level=2`, wildbits).

### Lookup cost (simulated on the real defs, 1,647 symbols)

| Scheme | Avg compares | Worst |
|---|---|---|
| RMA now: 64 buckets, char-sum hash, BST per bucket | 5.2 | 14 |
| same, CRC hash | 5.0 | 12 |
| single BST | 17.8 | 46 |
| 512 buckets, chained, CRC hash | 2.6 | 9 |

Current structure is good enough. Bank switches are the new cost to watch.

## 7. Libraries

`.as` programs link against `libwildbitsl2.a`, `libnet.a`, `libalib.a` (LWOBJ).
Rebuild each as a ROF `.l` with the fork. Keep the one-routine-per-file split
so rlink pulls in only what's referenced. Needs `section` support first.

## 8. Verification

`harness/lwdiff.py` (phase 0, done). It dry-runs a NitrOS9 recipe
(`make -n -B`, default `recipes/wildbits/l2`, `PLATFORM=jr2`) to get every
lwasm command the real build runs, then for each one:

1. assembles with lwasm exactly as the build would (the reference)
2. assembles the same source with `las`, same include paths and defines
3. compares the output bytes (module format) or records whether it
   assembled (object format; compared after linking in phase 5)

It assembles `corpus/`: clones of the owner's forks `jfed6000/nitros9` and
`jfed6000/nitros9-languages` (branch `devsys`), pinned to the commits in
`harness/corpus.lock` so results only change when `las` changes. Moving to
newer NitrOS9 is a deliberate step (`harness/corpus.sh update`, rerun,
commit the lock). Each report and history line records the corpus commits.

It writes `harness/reports/<recipe>-<platform>.md` (summary, blockers ranked
by first error, per-file status) and appends a line to
`harness/reports/history.csv`. Run it after every change; the history file is
the project's progress record. Takes about 2 seconds.

Not yet exercised: the MATCH path, since no module-format file can assemble
until phase 3. Its first real match should be checked by hand.

`harness/regress.py` (phase 1) checks that a change did **not** alter
behaviour. It builds a reference `las` from a git revision (default `HEAD`)
and compares it with the working tree on about 740 inputs: 252 hand-written
RMA sources (232 assemble cleanly), 198 C files compiled by the host DCC (all
assemble), and the 290 NitrOS9 build commands. It compares object files
(timestamp masked), listings with symbol tables (`-l -s`, date masked),
stderr and exit codes. Use it for refactors (phases 1, 2, 8, 9), where any
difference is a failure; use lwdiff for intended changes. Checked both ways:
unchanged code passes, and a deliberate change (64 → 32 hash buckets) fails
617 of 740.

**An intended change to RMA behaviour (phase 3).** Giving the top level a
location counter changed 58 of the 450 RMA/DCC regression inputs. All 58
include CoCoC's `Defs/os9defs.a`-style defs, which (edited for lwasm) define a
`csect` macro under `IFGE LWASM`. RMA defines the macro anyway (undefined
symbol in a condition), and before phase 3 its `ORG` and the following
top-level `rmb` lines failed silently in pass 1, so every system call name
(`F$Sleep`…) stayed undefined and became an external reference. Now they get
their real values. Checked file by file: no file gained a new kind of error,
errors fell (15 C library files 273 → 14 each), `getc.a` now assembles
cleanly. Use `regress.py --skip lwdiff` while a change is meant to alter
NitrOS9 results.

**Native (phase 7).** `harness/native.py` runs three suites on the F256 in
MAME: `las` (native las assembles the 56 `.a` files the cross-build made
from las, rlink and DCC's C sources; each ROF must equal host las's, date
masked), `link` (native rlink links rlink, dcc, dcpp and dco68; each module
must equal the cross-built one) and `cc` (native `dcc` compiles, links and
runs `harness/native/hello.c`; the module must equal the host passes').
`harness/os9run.py` runs the same programs on a host model for debugging
(`--map obj/TOOL.map --break FUNC` prints who called FUNC).

Both harnesses run every `las` call in its own empty directory, because las
writes its macro work file under a fixed name (`rma.tmp`, `rma.h:204`) in the
current directory.

## 9. Roadmap

| Phase | Work | Done when | Status |
|---|---|---|---|
| **0. Harness** | Linux build of fork; diff harness over Level 2 wildbits tree; baseline report | report lists every file and its status | **done** 2026-10-02. Baseline: 0 of 290 match (267 errors, 23 fatal) |
| **1. Symbol interface** | Put RMA's symbol code behind `sym_find` / `sym_add` | output byte-identical to unmodified RMA | **done** 2026-10-02. `las/symtab.c`: `sym_find`, `sym_add`, `sym_walk`; tree code removed from `labels.c`/`file_io.c`. regress.py: 740 of 740 identical |
| **2. Compact entries** | Variable-length names | identical output, memory use measured | **done** 2026-10-02. `symblnam[1]` as last member, allocated at the name's length. regress 740/740; AddressSanitizer build clean on all 740 (and catches a deliberate 4-byte under-allocation). Measured: see §6 |
| **3. Module mode** | `mod`/`emod`, header, CRC | simple `.asm` commands match lwasm | **done** 2026-10-02. 72 of 290 match lwasm byte for byte (verified independently: header parity and CRC recomputed). Also top-level `org`/`rmb`, `.` and `*` counters, `-D`. AddressSanitizer clean |
| **3b. Operand sizing** | Choose instruction sizes like lwasm: `,pcr` 8/16-bit offsets (and check extended vs direct, index offsets) | the DIFF files match | **done** 2026-10-02. `las/sizing.c` (D16); rlink `--lwlink` layout order (D17); `harness/sizediff.py` lists instructions of different length. 199 of 200 module files match (`minted` needs `struct`); 13 of 13 linked programs byte-identical to lwlink. Also fixed: `--defs` values in conditions (skipped `os9.d` in object files), branches after `emod` counted from the wrong counter. RMA/DCC regression 450/450; AddressSanitizer clean (las, rlink) |
| **4. lwasm syntax** | lwasm syntax and semantics in lwasm mode; see §4 status table | most `.asm` files match | **done** 2026-10-02 (except `struct`, 1 file). 160 of 290 match (was 72). Every remaining DIFF is phase 3b sizing; remaining errors are phase 5 `section` files. RMA/DCC regression 450/450; AddressSanitizer clean |
| **5. Object mode** | `section`/`export` → ROF; header defaults | `.as` files link with rlink and match | **done** 2026-10-02. `las/lwobj.c` (sections → psect/vsect/csect, `__os9`, export pragma, `rmb` by section type, `dtb`, `=`), `las/preload.c` (`--defs`), rlink fork with `--lwlink` (D13–D15). 90 of 90 object files assemble; 4 of 13 linked programs byte-identical to lwlink; the other 9 differ only by instruction sizing (10 objects larger than lwasm's, phase 3b). AddressSanitizer clean (las and rlink) |
| **6. Long names downstream** | rlink and DCC name limits; DCC `ASSEMBLER` → `"las"` | 24-char names survive compile → link | **done** 2026-10-02. rlink `SYMLEN` 63 (phase 5) and 8-character matching for Microware objects (D19); DCC fork (D18) with 31-character names, driver runs `las`. `harness/dccdiff.py`: all 198 CoCoC sources that compile give identical output apart from longer names (59 files). A test program with names that collide at 8 characters, calling `localtime()`/`intercept()` from CoCoC's `clib.l`, builds with `dcc` and every call lands on the right address (the original rlink fails it). Fixed in DCC: `getword` overflow, two uninitialised pointers in declarations (`xlkmap.c` now compiles), an out-of-bounds read in the optimizer. regress uses the devsys DCC: 451/451. AddressSanitizer clean (dcc68, dco68, rlink, las) |
| **7. Native build** | Compile fork + rlink + DCC with DCC; run on F256 | the tools run on the F256 and give the host's output on every input that fits in 64K (`scfg.asm` moved to phase 8: it can't fit without banked symbols, §6) | **done** 2026-10-02. All six tools cross-build with DCC (`native/GNUmakefile`: las 34,149, rlink 18,654, dcc 9,589, dcpp 19,014, dcc68 48,290, dco68 11,558 bytes). In MAME (`harness/native.py`): native las assembles all 56 `.a` files of the toolchain's own C sources identically to host las; native rlink links rlink, dcc, dcpp and dco68 identically; native `dcc` compiles, links and runs a C program, module identical to the host passes'. las and dcc68 can't be linked natively yet (rlink runs out of memory: phase 8, D24). Fixed: three 6809-only las bugs (§10); rlink names at their own length (D23) and an `ext_ref` reuse overflow that change exposed; K&R and library gaps in rlink, COpt (§10); dcc68's memory (D25). Host results unchanged: regress 741/741, lwdiff 199 match and 13/13 links, AddressSanitizer clean (rlink) |
| **8. Banked blocks** | `F$AllRAM`/`F$MapBlk` storage for las's symbols and rlink's link data (D24) | `scfg.asm`, `fm.asm`, `rbf.asm`, kernel assemble natively; native rlink links las and dcc68 | **in progress** 2026-10-02. Symbols as handles (D26) and banked storage (D27, `las/bank.c`); rlink needed no banking (D29); `--args=FILE` (D28). In MAME, `scfg.asm` assembles natively, byte-identical to `las/build/nosz` (6 blocks, ~22,500 window switches, 8K of data); native rlink links all six tools identically. Host: regress 741/741, `build/bankwin` 741/741 (negative control fails 242), AddressSanitizer clean (las, rlink), lwdiff unchanged. Also fixed: `%.*s` in `symkey` (CoCoC printf has no `*`), commons written with address 0 on the 6809 (`PUTINT`). Remaining: the full native run of all 200 module sources (`native.py lw`) |
| **9. Precompiled defs** | Saved defs images | measurable speedup, same output | not started |
| **10. Libraries** | alib, net, wildbitsl2 as ROF `.l` | `.as` programs build natively | **started** in phase 5: a ROF library is its members' `.r` files concatenated; the harness builds all three from las output and they link. Native build of the libraries remains |

Phases 3–6 can run alongside each other. Phase 7 should happen early enough to
catch DCC/K&R surprises.

### Phase 7 test plan: MAME

Native tests run in the Wildbits MAME, as in
`~/projects/wild/tempest/docs/port-guide.md` §13 ("Running the Wildbits MAME"):

- Binary `~/projects/wild/mame/mame`, machine `wbjr2` (Jr2 image), `-bios turbo`.
  Headless: `-video none -sound none -nothrottle -skip_gameinfo`, driven by a Lua
  `-autoboot_script` (`manager.machine.natkeyboard:post(...)`, subscription kept
  in a global), or by the image's own `startup` file, which is deterministic. A
  marker command after the test (`echo done >/dd/MARKER`) shows it finished.
- Mount a **copy** of the disk image (MAME writes into it). Image sizes must be
  `odd × 2^n` blocks: pad **upward** only; truncating cuts the filesystem
  (files then fail with error 214).
- Move files on and off the image with ToolShed's `os9` (`~/projects/coco-shelf/bin/os9`,
  e.g. `os9 copy`, `os9 attr`).
- The check: the tools on the F256 process real inputs; copy the outputs off
  the image and compare them byte for byte with the host tools'. MAME proves
  correctness (crashes, OS-9 errors, output); its timing is no evidence, and
  it is never modified.

**As built (`harness/native.py`).** Starts from the newest Jr2 image
(`~/projects/wild/netrek/nitros9/recipes/wildbits/l2/l2_wildbitsjr2.dsk`,
`--image` for another), copies it to `harness/out/native/test.dsk` and pads
it to 128 MB (2^27 bytes). Installs the tools and libraries, stages each
suite's inputs (text with CR line ends, `os9 copy -l`) and writes a
`startup` that begins with `-x` (the shell otherwise stops at the first
error) and ends by writing `/dd/T/MARKER`. A Lua frame callback polls the
image with `os9 dir` every 600 frames and exits MAME when the marker
appears. MAME's exit log includes the text screen. Lessons: a link's `-o=`
needs a full path (rlink's "x" mode, §10); a tool linked without its `-M`
overflows its stack and can hang the machine; never stop MAME with
`pgrep -f`/`pkill -f` on a pattern that is also in your own shell's command
line.

## 10. Open questions and risks

- [ ] Does RMA already treat symbols case-insensitively? (`nosymbolcase`)
- [ ] How should `condundefzero` interact with RMA's undefined-symbol handling in pass 1?
- [ ] lwasm's exact scoping rule for `@` local labels — confirm from lwasm source before implementing.
- [ ] Defaults for lwlink-style header values (type, attr, rev, stack) — read them from `lwlink/script.c`.
- [x] Size of the DCC-compiled fork on the 6809: 34,675 bytes, leaving about 16K for symbols (§6, phase 7).
- [ ] `F$MapBlk` cost per call on the F256 — sets how much bank switching hurts. Counted (logic, so valid in the model): `scfg.asm` makes about 22,500 window switches (each F$ClrBlk + F$MapBlk) for 78M instructions. Time it on the hardware; if it matters, a second window slot or keeping each hash bucket's tree in one block would cut switches.
- [ ] How much RAM does the target F256 have free for symbol blocks? The machine has 1800K+ (owner, 2026-10-02); measure what a running system leaves free in phase 8.
- [x] DCC limits when compiling the tools natively (phase 7). What it took: K&R declarations in rlink's `proto.h`; `#define void int` in COpt; lwsel's struct members renamed (K&R C has one namespace for all struct members, and `refs` was taken); `atexit` and `ftruncate` replaced (CoCoC's clib has neither; `_ss_size` sets the size); the source directory searched before CoCoC's `Defs` (COpt has its own `ctype.h`). Three 6809-only bugs in las: `MLCod.q_long` and `MLCod.shrtlng.f_byt` (K&R took them as offset 0 of `MLCod`, clearing the opcode), and a `long` passed where `do_rmb`'s callees take an `int` (no prototypes, so the callee read the high word: 0). gcc's `-Wtraditional-conversion` over a copy where those values are `long` found the last kind; only two sites.
- [ ] Does anything in the build depend on lwasm listing or map output?
- [ ] `errexit()` exits with `errno`, which is 0 unless a system call just failed, so some fatal errors (e.g. "bad -D value", "symbol table overflow") report success to the shell. Fix: exit 1 when errno is 0. Changes exit codes, so do it as its own small step.
- [x] `ifp1` blocks were read only in pass 1, so the defs' effect on the location counter (lwasm leaves `*` = 8 after the Level 2 wildbits defs) differed between passes. Solved in phase 4: lwasm mode makes `ifp1` true in both passes and replays conditionals (D12).
- [ ] RMA mode still matches directive names as prefixes, so `setdp` is taken as `set` there (lwasm mode requires exact names). Fixing it in RMA mode would change RMA behaviour; decide if RMA-mode sources ever need `setdp`.
- [ ] `struct`/`endstruct` (lwasm) not implemented; 1 file (`minted`).
- [ ] `wbinfo` contains `dtb` (assembly date and time), so its link matches lwlink only when both run in the same second. The harness should mask those 6 bytes or treat the file as nondeterministic.
- [ ] `--defs` resolves a value at assembly time where lwasm left it to the linker. Where that changes an instruction's size choice (e.g. an `equ` defined from defs constants then used as an address), las and lwasm may differ. None seen in the corpus after phase 3b.
- [ ] **Sizing on the 6809** (still open after phase 7; the native build has no sizing, D21). The trial needs `fork()`, which OS-9 lacks; the CoCo build compiles sizing out and keeps RMA's long `,pcr` forms. Options: `F$Fork` las itself with a hidden option and a pipe, or reset pass-1 state in-process. The trial also doubles pass-1 time.
- [ ] Sizing model limits: a literal `,pcr` target (`leax 5,pcr`) and targets that are `equ *` symbols are treated as unreachable (long); forward-referenced constants in direct/extended and `n,X` operands keep RMA's long form where lwasm would shorten them; distances use las's layout, which differs from lwasm's if a file interleaves sections. None occur in the corpus.
- [ ] lwasm's symbolic `org`: `scf.d` does `ORG PD.OPT` with `PD.OPT` external, so lwasm's `PD.EKO` is "PD.OPT+4" and `PD.EKO-PD.OPT` cancels to 4 (5-bit offset). las makes the labels absolute and keeps `PD.OPT` as a reference, so `opts.o` has three 16-bit offsets (not linked by any of the 13 programs). Fix: give labels under a symbolic `org` the base as a reference, and cancel +X/−X reference pairs.
- [ ] **C libraries with long names** (phase 10): CoCoC's `clib.l` etc. still have 8-character names (`localtim`), served by D19's aliasing. Rebuilding them with the devsys DCC removes the need. Only one direction is handled: an old object referencing `localtim` would not find a rebuilt library's `localtime`.
- [ ] DCC's `ttl` lines still cut function names to 8 characters (cosmetic: listing titles only).
- [x] On the F256, `dcc` looks for its libraries on the default drive. `harness/native.py` installs the layout it needs: the six tools in `/dd/CMDS`; `cstart.r`, `clib.l`, `sys.l`, `level1.patterns`, `level2.patterns` in `/dd/LIB`; CoCoC's headers in `/dd/DEFS`; `/dd/TMP` for temporary files.
- [ ] On OS-9, rlink opens its output with mode `"wx+"`: a bare `-o=name` goes to the **execution directory** (`/dd/CMDS`), as Microware's `cc` did; the Linux build writes to the current directory. Keep (it's how `dcc` installs programs) or change for building NitrOS9 natively?
- [ ] lwasm mode is untested natively: every NitrOS9 source needs the defs, which don't fit until phase 8. The native suites cover RMA mode (DCC output) only. Once phase 8 lands, run lwdiff's inputs natively against `las/build/nosz`.
- [ ] dcpp with too little memory stops early **with exit status 0** and a truncated output file (seen with 16 pages). The `dcc` driver then compiles half a file. Make dcpp report it.
- [x] `harness/os9run.py` now models the F256's two limits (D25), F$AllRAM/F$MapBlk/F$ClrBlk/F$DelRAM with real block contents, and reports blocks never given back.
- [ ] `harness/os9run.py` doesn't model `F$Link`, so the `dcc` driver (which links `Init` to find the default drive) only runs in MAME.
- [ ] las merges all of a file's code-like sections into one psect; lwlink places `code`, `.text`, `data`, `.data` separately. A file mixing them is placed by its first (`__lwsec`).
- [ ] Bugs fixed in the forked rlink during phase 5: `grab()` returned unzeroed memory that rlink relied on being zero (every link crashed under AddressSanitizer; worked by luck); `fprintf(OutFile, modname)` used the name as a format string; a 16-character psect name read into a 10-byte buffer; the unresolved-references report dereferenced a null pointer; the makefile lacked header dependencies (a SYMLEN change left stale objects). In las: an exported `set` symbol was counted twice in the ROF header (malformed ROF).
- [ ] RMA assembles each input file as a separate unit (both passes, then that file's ROF trailer). Fine for ROF libraries; for modules, pass one source file.
- [ ] las's macro work file has a fixed name (`rma.tmp` in the current directory), so two las runs in one directory collide, natively too. Use a unique name (process ID) or a temp directory.
- [x] (Answered in phase 8, D26/D27: entries and names both move to banks; fields only through accessors, handles instead of pointers, `r_offset` holds a SYMH.) Phase 8 (banked blocks): symbol fields (`smbltyp`, `locmsk`, `s_ofst`, `reftbl`) are still read directly through pointers in `labels.c`, `directives.c`, `registers.c`, `file_io.c`, and `ref_xtrnal.r_offset` stores symbol pointers cast to `int`. Phase 1 isolated *finding and storing*; banking also needs those pointers to stay valid, so decide in phase 8 whether entries stay in main memory and only names move to banks.
- [ ] Variable-length symbols on the 6809: the Linux build pads `struct symblstr` to a multiple of 4, so AddressSanitizer can't see an under-allocation of 1–3 bytes there. The 6809 build has no padding, so such a bug would corrupt memory only natively. Keep `sizeof (struct symblstr) + strlen (name)` and don't trim it.
- [ ] Phase 2/8: `LOC1BYT` in `locmsk` doubles as the tree's "real right child" flag and is printed in the listing's symbol table. A different storage layout will change that column; it isn't in lwasm's output, so harmless for lwdiff, but regress.py will flag it and the reference must be moved deliberately.
