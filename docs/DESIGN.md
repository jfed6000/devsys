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

Measured against the 370 files in `level1/wildbits`, `level2/wildbits`,
`level1/modules`, `level1/cmds` and `defs`:

| Feature | Files using it | RMA today |
|---|---|---|
| `mod` / `emod` | most | ❌ bad mnemonic |
| `foo@` local labels (scoped) | 127 | ❌ treated as global → "redefined name" |
| `fill`, `ifndef` | 50 | ❌ |
| `<<`, `>>` | ~40 uses | ❌ bad operator |
| `section` / `endsect` | wildbits boot/feu, 28 `.as` files | ❌ |
| `fcn`, `ifdef` | several | ❌ |
| case-insensitive symbols (`nosymbolcase`) | all | ? — check |
| undefined symbol in condition = 0 (`condundefzero`) | ~400 `ifne H6309` etc. | ? — check |
| `pcaspcr`, `dollarnotlocal` | all | ? — check |

### Found by the harness (phase 0 baseline, 2026-10-02)

The harness (§8) found these in the first errors of the Level 2 jr2 build
(290 assemblies). Counts are files whose *first* error is this one.

| Feature | Files | RMA today |
|---|---|---|
| `;` comments, including `;;; NAME` at column 1 | 74 | ❌ only `*` starts a comment line → "bad label" |
| `; comment` after an operand (`use defsfile  ; ...`) | 5 | ❌ comment read as part of the file name → fatal |
| CRLF line endings | 18 | ❌ `\r` kept in names (`"defsfile\r"`) → fatal. Natively, files may have CR, LF or CRLF; accept all three |
| `fcb`/`fcc`/`rmb`/`org` with no section (data files, hand-built modules) | 35 | ❌ RMA only allows them inside `psect`/`vsect` |
| `ifne H6309` etc. with `H6309` undefined (`condundefzero`) | 2+ | ❌ "illegal external reference" |
| `equ`/`set` using a not-yet-defined symbol | 5 | ❌ "illegal external reference" |
| `^` operator (`1024^2`) | 1 | ❌ bad operator. Check what lwasm means by `^` before copying it |
| `setdp` with no label | 1 | ❌ "label missing" |
| `-D` command-line defines | all | ❌ no option. Harness uses a prelude file for now |
| `use` file searched next to the including file | many | ❌ RMA searches the current directory, then `-i` paths. lwasm searches the *including file's* directory, then `-I` paths, never the current directory. Harness runs las in an empty directory with the source's directory as the first `-i`, which matches for the top-level file but not for nested `use` |
| Trailing spaces after a `use` file name (`use   os9.d   `) | 2 | ❌ spaces kept in the name → fatal (found 2026-10-02 once las read the same `basic09/defsfile` lwasm does) |

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

Both harnesses run every `las` call in its own empty directory, because las
writes its macro work file under a fixed name (`rma.tmp`, `rma.h:204`) in the
current directory.

## 9. Roadmap

| Phase | Work | Done when | Status |
|---|---|---|---|
| **0. Harness** | Linux build of fork; diff harness over Level 2 wildbits tree; baseline report | report lists every file and its status | **done** 2026-10-02. Baseline: 0 of 290 match (267 errors, 23 fatal) |
| **1. Symbol interface** | Put RMA's symbol code behind `sym_find` / `sym_add` | output byte-identical to unmodified RMA | **done** 2026-10-02. `las/symtab.c`: `sym_find`, `sym_add`, `sym_walk`; tree code removed from `labels.c`/`file_io.c`. regress.py: 740 of 740 identical |
| **2. Compact entries** | Variable-length names | identical output, memory use measured | **done** 2026-10-02. `symblnam[1]` as last member, allocated at the name's length. regress 740/740; AddressSanitizer build clean on all 740 (and catches a deliberate 4-byte under-allocation). Measured: see §6 |
| **3. Module mode** | `mod`/`emod`, header, CRC | simple `.asm` commands match lwasm | not started |
| **4. lwasm syntax** | `@` locals, `ifdef/ifndef`, `fill`, `fcn`, `<<`/`>>`, `;` comments, CR/LF/CRLF, `-D`, data outside sections, pragma behaviours (see §4) | most `.asm` files match | not started |
| **5. Object mode** | `section`/`export` → ROF; header defaults | `.as` files link with rlink and match | not started |
| **6. Long names downstream** | rlink and DCC name limits; DCC `ASSEMBLER` → `"las"` | 24-char names survive compile → link | not started |
| **7. Native build** | Compile fork + rlink + DCC with DCC; run on F256 | assembles `scfg.asm` on the F256 | not started |
| **8. Banked blocks** | `F$AllRAM`/`F$MapBlk` symbol storage | `fm.asm`, `rbf.asm`, kernel assemble natively | not started |
| **9. Precompiled defs** | Saved defs images | measurable speedup, same output | not started |
| **10. Libraries** | alib, net, wildbitsl2 as ROF `.l` | `.as` programs build natively | not started |

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
- [ ] las's macro work file has a fixed name (`rma.tmp` in the current directory), so two las runs in one directory collide, natively too. Use a unique name (process ID) or a temp directory.
- [ ] Phase 8 (banked blocks): symbol fields (`smbltyp`, `locmsk`, `s_ofst`, `reftbl`) are still read directly through pointers in `labels.c`, `directives.c`, `registers.c`, `file_io.c`, and `ref_xtrnal.r_offset` stores symbol pointers cast to `int`. Phase 1 isolated *finding and storing*; banking also needs those pointers to stay valid, so decide in phase 8 whether entries stay in main memory and only names move to banks.
- [ ] Variable-length symbols on the 6809: the Linux build pads `struct symblstr` to a multiple of 4, so AddressSanitizer can't see an under-allocation of 1–3 bytes there. The 6809 build has no padding, so such a bug would corrupt memory only natively. Keep `sizeof (struct symblstr) + strlen (name)` and don't trim it.
- [ ] Phase 2/8: `LOC1BYT` in `locmsk` doubles as the tree's "real right child" flag and is printed in the listing's symbol table. A different storage layout will change that column; it isn't in lwasm's output, so harmless for lwdiff, but regress.py will flag it and the reference must be moved deliberately.
