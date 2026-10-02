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
| rlink | 9, **silently truncated** | `mw-rlink/rof.h:39`, `rl_02.c:671`, `rl_03.c:292` | raise to match RMA |
| DCC | 8 | `CC09/cj.h:130` `NAMESIZE 8` | raise |

ROF stores names null-terminated, so the file format doesn't change.

## 6. Symbol table memory

### Budget

A Level 2 process has one 64K address space shared by its code, data and stack.
For a DCC-compiled RMA fork, expect roughly **25–30K** free for tables.

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
| **6. Long names downstream** | rlink and DCC name limits; DCC `ASSEMBLER` → `"las"` | 24-char names survive compile → link | **rlink done** in phase 5 (`SYMLEN` 63). DCC's `NAMESIZE 8` and `ASSEMBLER` still to do |
| **7. Native build** | Compile fork + rlink + DCC with DCC; run on F256 | assembles `scfg.asm` on the F256 | not started |
| **8. Banked blocks** | `F$AllRAM`/`F$MapBlk` symbol storage | `fm.asm`, `rbf.asm`, kernel assemble natively | not started |
| **9. Precompiled defs** | Saved defs images | measurable speedup, same output | not started |
| **10. Libraries** | alib, net, wildbitsl2 as ROF `.l` | `.as` programs build natively | **started** in phase 5: a ROF library is its members' `.r` files concatenated; the harness builds all three from las output and they link. Native build of the libraries remains |

Phases 3–6 can run alongside each other. Phase 7 should happen early enough to
catch DCC/K&R surprises.

## 10. Open questions and risks

- [ ] Does RMA already treat symbols case-insensitively? (`nosymbolcase`)
- [ ] How should `condundefzero` interact with RMA's undefined-symbol handling in pass 1?
- [ ] lwasm's exact scoping rule for `@` local labels — confirm from lwasm source before implementing.
- [ ] Defaults for lwlink-style header values (type, attr, rev, stack) — read them from `lwlink/script.c`.
- [ ] Size of the DCC-compiled fork on the 6809 — sets the real memory budget.
- [ ] `F$MapBlk` cost per call on the F256 — sets how much bank switching hurts.
- [ ] How much RAM does the target F256 have free for symbol blocks?
- [ ] DCC limits (no `void`, `enum`, `const`) when compiling the fork natively.
- [ ] Does anything in the build depend on lwasm listing or map output?
- [ ] `errexit()` exits with `errno`, which is 0 unless a system call just failed, so some fatal errors (e.g. "bad -D value", "symbol table overflow") report success to the shell. Fix: exit 1 when errno is 0. Changes exit codes, so do it as its own small step.
- [x] `ifp1` blocks were read only in pass 1, so the defs' effect on the location counter (lwasm leaves `*` = 8 after the Level 2 wildbits defs) differed between passes. Solved in phase 4: lwasm mode makes `ifp1` true in both passes and replays conditionals (D12).
- [ ] RMA mode still matches directive names as prefixes, so `setdp` is taken as `set` there (lwasm mode requires exact names). Fixing it in RMA mode would change RMA behaviour; decide if RMA-mode sources ever need `setdp`.
- [ ] `struct`/`endstruct` (lwasm) not implemented; 1 file (`minted`).
- [ ] `wbinfo` contains `dtb` (assembly date and time), so its link matches lwlink only when both run in the same second. The harness should mask those 6 bytes or treat the file as nondeterministic.
- [ ] `--defs` resolves a value at assembly time where lwasm left it to the linker. Where that changes an instruction's size choice (e.g. an `equ` defined from defs constants then used as an address), las and lwasm may differ. None seen in the corpus after phase 3b.
- [ ] **Sizing on the 6809 (phase 7).** The trial needs `fork()`, which OS-9 lacks; the CoCo build compiles sizing out and keeps RMA's long `,pcr` forms. Options: `F$Fork` las itself with a hidden option and a pipe, or reset pass-1 state in-process. The trial also doubles pass-1 time.
- [ ] Sizing model limits: a literal `,pcr` target (`leax 5,pcr`) and targets that are `equ *` symbols are treated as unreachable (long); forward-referenced constants in direct/extended and `n,X` operands keep RMA's long form where lwasm would shorten them; distances use las's layout, which differs from lwasm's if a file interleaves sections. None occur in the corpus.
- [ ] lwasm's symbolic `org`: `scf.d` does `ORG PD.OPT` with `PD.OPT` external, so lwasm's `PD.EKO` is "PD.OPT+4" and `PD.EKO-PD.OPT` cancels to 4 (5-bit offset). las makes the labels absolute and keeps `PD.OPT` as a reference, so `opts.o` has three 16-bit offsets (not linked by any of the 13 programs). Fix: give labels under a symbolic `org` the base as a reference, and cancel +X/−X reference pairs.
- [ ] las merges all of a file's code-like sections into one psect; lwlink places `code`, `.text`, `data`, `.data` separately. A file mixing them is placed by its first (`__lwsec`).
- [ ] Bugs fixed in the forked rlink during phase 5: `grab()` returned unzeroed memory that rlink relied on being zero (every link crashed under AddressSanitizer; worked by luck); `fprintf(OutFile, modname)` used the name as a format string; a 16-character psect name read into a 10-byte buffer; the unresolved-references report dereferenced a null pointer; the makefile lacked header dependencies (a SYMLEN change left stale objects). In las: an exported `set` symbol was counted twice in the ROF header (malformed ROF).
- [ ] RMA assembles each input file as a separate unit (both passes, then that file's ROF trailer). Fine for ROF libraries; for modules, pass one source file.
- [ ] las's macro work file has a fixed name (`rma.tmp` in the current directory), so two las runs in one directory collide, natively too. Use a unique name (process ID) or a temp directory.
- [ ] Phase 8 (banked blocks): symbol fields (`smbltyp`, `locmsk`, `s_ofst`, `reftbl`) are still read directly through pointers in `labels.c`, `directives.c`, `registers.c`, `file_io.c`, and `ref_xtrnal.r_offset` stores symbol pointers cast to `int`. Phase 1 isolated *finding and storing*; banking also needs those pointers to stay valid, so decide in phase 8 whether entries stay in main memory and only names move to banks.
- [ ] Variable-length symbols on the 6809: the Linux build pads `struct symblstr` to a multiple of 4, so AddressSanitizer can't see an under-allocation of 1–3 bytes there. The 6809 build has no padding, so such a bug would corrupt memory only natively. Keep `sizeof (struct symblstr) + strlen (name)` and don't trim it.
- [ ] Phase 2/8: `LOC1BYT` in `locmsk` doubles as the tree's "real right child" flag and is printed in the listing's symbol table. A different storage layout will change that column; it isn't in lwasm's output, so harmless for lwdiff, but regress.py will flag it and the reference must be moved deliberately.
