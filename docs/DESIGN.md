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
| D20 | **Native tools are cross-built on Linux** by `native/GNUmakefile`: each `.c` goes through the host `dcpp` → `dcc68` → `dco68` → `las`, and host `rlink` links `cstart.r` + objects + `sys.l` + `clib.l` from CoCoC. Extra memory per tool (`-M`, pages): rlink 10, dco68 10 and dcpp 135 as in their original native makefiles; dcc68 10 (D25), las 16 (D30). The optimizer runs with `-2` (D4) | Same passes the `dcc` driver runs on the F256, so the native build *is* a DCC build, and the host tools are the ones already verified. Since phase 8 native rlink links all six tools, so the toolchain could rebuild itself on the F256; the cross-build stays the normal way to make it |
| D21 | ~~**Reference for native las output: `las/build/nosz`**~~ (host las built with `-DNOSIZING`). **Superseded by D38** (step 13): native las sizes too, so the harnesses compare with `las/build/unix` | `sizing.c` needed `fork()`, so until step 13 the 6809 las kept RMA's long `,pcr` forms in lwasm mode. Native output was compared with a host build that did the same; RMA mode is unaffected (sizing is lwasm-only) |
| D22 | **Two native test tools.** `harness/native.py` runs the tools in the Wildbits MAME and compares their output with the host's: the proof. `harness/os9run.py` runs an OS-9 program on a host model (the owner's `cpu6809.py` from tempest, plus the system calls a CoCoC program makes), with call traces and backtraces by function name: the debugging aid | A MAME round trip takes minutes and shows only the end result; the model reproduced every MAME result in seconds and named the failing function |
| D23 | rlink stores **names at their own length** (`setname`, `misc.c`) instead of `SYMLEN + 1` arrays; module names in `MAXNAME + 1` | At 64 bytes a name (phase 5's `SYMLEN` 63), linking anything against `clib.l` ran the 6809 out of memory. Same idea as las's compact entries (phase 2) |
| D24 | **All use of memory beyond one 64K process goes in phase 8**: las's symbol table *and* rlink's link of large programs (las, dcc68) | Owner's decision, 2026-10-02: handle all block switching in one phase. The F256 has 1800K+ of RAM |
| D25 | **dcc68 fits the F256's module limit**: compiled with `-s` (no stack checks, as natively), without `-DPROF` (`dcc -p` profiling is unavailable natively), and with `sbrk` instead of `ibrk` for its tables (`CC09/cj.h`), starting from a small data area. Module 48,290 bytes (48,355 after D33: **29 bytes** under the limit, so any growth of dcc68 needs room found first) | Measured in MAME with test modules: a module in 6 blocks may be at most **48,384 bytes** (it must end by `$FD00`; the top 768 bytes are reserved on this NitrOS9), else it takes a 7th block. And F$Fork refuses (`#207`) a process whose module blocks plus *initial* data blocks fill all 8 slots, while F$Mem can grow a running process to 8 (native las did, 24K of data). CoCoC's shipped dcc68 (48,521 bytes, 16K initial data) can't start on the F256 for both reasons |
| D26 | **Symbols are opaque handles** (phase 8 step 1): callers get a `SYMH`/`REFH` (an unsigned int) and read or change a symbol only through `symtab.c`'s accessors (`sym_typ`, `sym_setmsk`, `sym_name`, `ref_new`, ...); `struct symblstr` and `struct ref_xtrnal` are private to `symtab.c`. `sym_setval` is a macro that casts to `SVAL` | A pointer into banked storage goes stale when another block is mapped, so no code outside `symtab.c` may hold one; making the structs invisible lets the compiler enforce it (every old access was a compile error). The cast guards the 6809 build, where a value passed without a prototype must already be a `long` |
| D27 | **Banked storage** (`las/bank.c`): symbols and reference records are allocated 2-byte aligned in 8K blocks from F$AllRAM; a handle is 16 bits, block+1 in the top 4 and offset/2 in the low 12 (15 blocks, 120K). One window slot, switched with F$ClrBlk + F$MapBlk (F$MapBlk takes the highest free slot, the one below las's module). `bank_done()` unmaps and F$DelRAMs at all three exits. Inside `symtab.c` no statement uses two entries | Largest need measured: 50.7K (`krnp2`), so 15 blocks leave room. A block F$AllRAM gave stays allocated after the process ends unless returned. `build/bankwin` emulates the single window on Linux by copying blocks in and out, so a stale pointer reads the wrong block: regress 741/741, and putting back one two-entry statement fails 242 of 741 (negative control) |
| D28 | las **`--args=FILE`** reads more arguments from FILE (separated by spaces or line ends) | The OS-9 shell reads at most 200 characters of a line, and cstart splits arguments at commas as well as spaces, so a NitrOS9 las command (`--pragma=` list, `-D`s) can't be typed natively. Measured in MAME: a 213-character line ran as two commands |
| D29 | rlink gets **no banked storage**: `lwsel.c`'s module table is allocated when `--lwlink` runs instead of being a 14K static array | That array was most of rlink's 19.7K of static data. Without it native rlink links las and dcc68 (the largest tools) in about 29K of its 40K, so banking rlink isn't needed for anything we build. Revisit if a larger program needs it |
| D30 | Native las gets **16 pages of extra memory** (`las_MEM`, `native/GNUmakefile`) | Stack and stdio buffers share the space between las's data and its stack; a source with macros opens the work file `rma.tmp`, whose 512-byte buffer comes out of it, and `llwbsd` then overflowed the stack (CoCoC's `_stkcheck` says "STACK OVERFLOW" when the stack meets the heap). las's 8,448 bytes of data already take two 8K blocks below the window slot, so 4K more costs no slot (`F$Fork` still has its module 5 + data 2 + window 1) |
| D31 | **Native rlink `--lwlink` fits in 64K**: lwsel doesn't keep the names of constants-only library members (never copied; rlink's own library scan still resolves them), its module table grows 32 at a time instead of 512 entries (13K) up front, and the index is freed after the temp file is written; `grab()` uses `malloc` on OS-9 too (was `sbrk`) so that memory is reused, and `setname` frees a recycled entry's shorter name | Linking the 13 `.as` programs natively ran out first in `lw_select` (the defs library exports 2,074 constants), then in pass 1 (`play`): 19K freed to `malloc` was out of `sbrk`'s reach. Now `play` peaks at 24.5K of heap. Side effect: a constants-only member no longer counts as a definer for lwlink's member choice, so if one and a later code member export the same name, rlink takes the code member where lwlink wouldn't (§10). Host links unchanged: lwdiff 13/13, AddressSanitizer clean |
| D32 | **`--defs` constants live in the banked blocks** (`preload.c`, entries by handle like symbols, D27) | The Level 2 defs export 2,074 constants, about 30K: in las's heap every object file failed natively with "symbol table overflow". `preload_find` compares against `sym_name()`'s copy in main memory, so mapping entry blocks can't invalidate it |
| D33 | **dcc68 tests B, not D, after a call to a `char` function** used as a condition (`if (f())`, `while (f())`, `f() ? :`), `CC09/bool.c` | The callee leaves its result in B only (`ldb ...; puls u,pc`); dcc68 tested the whole of D (`std -2,s`), so A's leftover garbage made the condition true. Found in las: `csectsetup`'s `if (SkipSpac ())` called `getNmbr` on an empty operand, so native las rejected `section _constant` (and would have rejected a bare RMA `csect`). The only such site in all six tools; none in CoCoC's sources (dccdiff 198/198 unchanged). `!f()` and `&&` already used the flags from the callee's `ldb` |
| D34 | **`native.py` runs in parallel**: `--shard K/N` (every Nth `lw` job), `--work DIR`, `--no-build`; and an **`obj` suite** (phase 10) | One MAME keeps one core busy, so 200 `lw` jobs took 2 hours; 8 shards take 22 minutes. `obj` assembles the 90 object-format sources natively (the constants library first, then the others with it as `--defs`), builds the three libraries with `merge`, links the 13 programs with `rlink --lwlink`, and compares objects with `las/build/nosz` and programs with host rlink on the host objects. `wbinfo`'s `dtb` (assembly date and time) is masked |
| D35 | **Precompiled defs: `las --defsimg=FILE`** (`las/defsimg.c`, lwasm mode). The image stands for the first `use` at the top of a single-source assembly: the symbols the defs created, in creation order (S), what pass 2's reading changes (Z), the conditionals' results, the counters they move (location counter, local-label blocks, line count, RMA's `if` count) and their `nam`/`ttl` lines (T, done again with `copy_to`'s pass rule). If FILE is missing, unfinished or a file is out of date, that `use` is read and recorded; otherwise it is loaded, **only if reading would do exactly the same**: the symbols defined before the use are exactly those at recording (names, types, values: P), each `use` resolves to the same path with the same date and size (F; OS-9's FD date via `SS.FD`, Linux `stat`), same options and location counter. Else the defs are read and the image left alone (another configuration). Pass 2 uses the image only if those symbols are unchanged and no defs symbol has changed since pass 1 (the setters report changes: `ImgOn`); else it reads the defs, which pass 1's state makes exact. Recording gives up on anything else in the defs (only `equ set rmb org`, conditionals, `use nam ttl pag spc`; no errors, references, or changes to older symbols) | An exact rule beats a clever one. A first version tracked every name the defs looked up; RMA creates an entry for any name used before it is defined, so "absent" names show up as created symbols, and "same symbols before, exactly" covers both them and collisions with ~3K less 6809 code. Replaying creations (not saving raw blocks) keeps each bucket's tree shape whatever came before, so lookups and the listing's symbol table are unchanged. Measured on `echo` natively (model): 33.7M → 5.3M instructions, F$MapBlk 12,025 → 5,868 |
| D36 | **las must stay at most 40,192 bytes** (until D37, which raised it to 48,384) (5 blocks ending by `$FD00`, as D25 for 6): with 6 module blocks + 2 data blocks `F$Fork` fails. Phase 9 took it from 36,576 to 40,168 (24 to spare): `symtab.c`, `bank.c`, `defsimg.c`, `module.c`, `preload.c` are compiled without stack checks (`las_NOCHECK`, 6 bytes a function); `defsimg.c` uses short wrappers (`gc`, `pc`, `rd`, `wr`, `seek`, `mark`) because each DCC call passing `Fp` and a long costs 10–20 bytes, and writes values in the machine's byte order (no long shifts) | These files don't recurse and every call into them comes from a checked function; the symbol accessors are also las's hottest calls (`echo` without an image: 35.1M → 33.7M instructions). Any growth of las now needs room found first, as for dcc68 |
| D37 | **las's data fits one 8K block, so its code may take 6** (at most 48,384 bytes, as dcc68, D25): `CondBits` holds 2,048 conditionals instead of 16,384 (256 bytes; the corpus's most is 168), `SecNames` keeps pointers to names in `getmem` memory instead of 32 × 32 bytes, `getmem` uses `ibrk` instead of `sbrk` on the 6809, and `las_MEM` is 8 pages. Static data 7,596 → 4,844 bytes; header data size 7,786, so with up to 200 bytes of parameters F$Fork gives 8,192 at most | Measured first (step 12, 348 inputs): las needed about 9.5K at its worst; now 7,067 of 7,936 bytes (`llwbsd`, 869 free between heap and stack; RMA-mode inputs about 2.2K free). With 6 module blocks, 1 data block and the window slot fill all 8, so nothing may grow the data area later: `sbrk` grows it with F$Mem above the stack, while `ibrk` takes from the gap between the static data and the stack, which stdio's buffers already share. One pool, three graceful limits: stdio falls back to unbuffered when `ibrk` is out, `getmem` says "symbol table overflow", the stack check says "STACK OVERFLOW". Fixes a latent overflow too: a section name of 32 or more characters overran its `SecNames` row. Chosen over a helper program (owner, 2026-10-05): 8K of code room instead of the 1.5K a helper could take out |
| D38 | **Native sizing: las `F$Fork`s itself** (`las/sizing.c`, step 13). Before pass 1 the parent takes one RAM block (`bank_share`), writes the child's parameters into it (its own arguments plus a hidden `--sztrial=N`, N the block number), `os9fork`s module `las` and `wait`s. The child, the same module in memory with its own data and symbol blocks, does the start-up again (options, `--defs`, `-D`), runs the trial pass 1 and decides exactly as on Linux, then maps block N (`bank_mapnum`), writes `SZ`, the count and a byte per `,pcr` site, unmaps it, frees its own blocks and exits 0. The parent packs the answers into a bitmap (one bit per site, `getmem`) and frees the block. The trial's records live in banked chunks on every build (128 items of 23 bytes, 512 labels of 3 bytes, at most 16 chunks each). The child may load a defs image but never records one; if a decision needs the section of a symbol the image made (`img_made`), which `sz_label` never saw, it exits 3 and the parent runs it again with `--sztrial=N,n` (read the defs). Linux keeps `fork()` and a pipe. Natively only for one source file (several: a warning and long offsets) | OS-9 has no `fork()`: `F$Fork` starts a fresh process, so the child must redo what came before pass 1; with one source file that is the same state Linux forks with. **No pipe**: the F256's boot has no PipeMan, Pipe or Nil modules. A shared block needs no device, and it is the owner's "share blocks by number" idea (§10) in its smallest form: the symbol blocks themselves aren't shared, because the trial needs its own symbols as they stand at the end of its own pass 1, and the parent has none yet. Records in banked memory because las's data is one block (D37) with ~770 bytes free on `llwbsd`, and the trial records up to 2,480 code labels (`cbmfont`; most items: 99, `vs`); the same chunked code on Linux means regress and `build/bankwin` test its pointer discipline. Cost: las 40,090 → 43,420 bytes (4,964 to spare), static data +101 bytes (header data size 7,889), and each lwasm-mode assembly runs pass 1 twice |
| D39 | **The `dcc` driver: `-w` works and pass failures reach the exit code** (`dcc/DCC/dcc.c`; the same fix in the owner's CoCoC tree, installed as `~/projects/bin/dcc`). `-w` no longer gives dcc68 `-n`; output still goes to `/nil` (`/dev/null` on Linux). On Linux `runit`'s child exits with the pass's own exit code (`WEXITSTATUS` of `system()`), and the parent decodes `wait`'s status before comparing it with the allowed level | `-n` came from CoCoC's older `cc` driver, which gave it only to the one-pass `c.comp` (`#ifndef TWOPASS`); dcc68 never had it, so `dcc -w` always failed with `unknown flag: -n`, on every platform. It went unnoticed because on Linux every pass failure was lost: `exit(system(cmd))` passes `system()`'s wait status (exit code × 256), and `exit` keeps the low byte, so a dcc68 with 2 errors gave 0, the driver ran dco68 and the assembler on the broken output, and `dcc` exited 0. Now: good file 0, errors 1, with or without `-w`. The 6809 build only loses the `-n` (`os9fork`'s status was already right). `dccdiff` unchanged (compiler passes), regress 451/451, native suites (las, link, cc, lw, obj, img) |
| D40 | **las picks its mode from the source** when neither `--lwasm` nor the new `--rma` is given (`file_io.c` `guess_mode`, after the options are read, before `--defs` and `-D`). It reads the source files (not `use` files) for the first line whose operation is `psect` (RMA), or `section` or `mod` (lwasm); with none, lwasm. The flags still force a mode | Owner, 2026-10-07: a convenience, mostly for typing `las` by hand on the F256. The mode changes how every line is read from the first (the `use` of the defs comes before `mod`), so it has to be known before pass 1: a prescan, not a switch at the directive. The fallback is lwasm because RMA can't put code or data outside a `psect` ("bad mnemonic"; a file of equates alone gives an empty object), and the 28 sources without one are lwasm raw data files (fonts, patterns, pixmaps, `modpatch`). Measured on every input: 262 of 290 NitrOS9 sources decide by `section`/`mod`, the other 28 fall back; all 252 hand-written RMA sources and DCC's output have `psect`; none disagree with the flag, and all 290 assemble to the same bytes without `--lwasm`. Reads a character at a time with an 8-byte word, no line buffer (D37's one data block): native las 43,420 → 43,901 bytes, data size +1. `regress` 741/741 (also asan, bankwin), `lwdiff` and `imgcheck` unchanged; `native.py lw` now runs without `--lwasm` against a flagged host reference |
| D41 | **lwasm's `struct`** (`las/lwstruct.c`, lwasm mode). `name struct` … `endstruct` (or `ends`): fields are `rmb`, `rmd`/`rmw` (×2), `rmq` (×4), `.blkb`/`.ds`/`.rs`, or another struct's name (nested); `endstruct` defines `name.field` (nested: `name.inner.field`) as constants and `sizeof{name}`. Neither the struct's name nor its fields' labels become symbols, and no counter moves. Outside a struct, a struct's name as the operation is an instance: assembled as `rmb sizeof{name}` would be there (`l1723` takes the size from `StSize`), then `var.field` gets `var`'s symbol kind at `var` + offset. `sizeof{...}` is read as one name (`MovLbl`). Types live in `getmem` memory from pass 1; pass 2 defines the same symbols again. Inside a struct, directives (conditionals, macros, `equ`) still work; other operations are skipped, as lwasm skips them | The last lwasm feature the NitrOS9 build used: `minted` now assembles, so lwdiff is 200 of 200 module sources (and native `lw` 200/200, `img` 114/114). Checked against lwasm beyond minted with test sources: nesting, `rmd`/`rmq`, instances in `bss` and `data` sections (linked with lwlink and `rlink --lwlink`: identical), `sizeof` of types and instances. Deliberate differences: lwasm skips conditionals inside a struct (so both arms' fields are defined, an error) where las obeys them; and an instance **inside a module** is an error in las, because lwasm puts it at the code counter and writes no bytes for it, so the module's size field and CRC don't match its contents. A K&R lesson again: the two record types needed unique member names (`sf_`, `st_`). Cost: native las 43,901 → 46,193 bytes (2,191 to spare), data size 7,890 → 7,904 (the label of an instance is still in `AltLbl` when its fields are defined, so no 64-byte copy) |
| D42 | **rlink is renamed `llink`**, and **picks its layout itself**. Directory `llink/`, program and 6809 module `llink`; the DCC driver runs `llink`; lwdiff's option is `--llink`. Internal file names stay (`rlink.h`, `rlink_main.c`), as `las` kept `rma_main.c`. Entries above keep the name they were written with. With neither `--lwlink` nor the new `--mw`, `lw_detect` (`rlink_main.c`) reads the input files' ROF headers and globals before `lw_select`, up to the mainline (the module with a type/language): lwlink's layout if it exports `__lwsec`, else Microware's; none found (or names only in a `-z` file): Microware | Owner, 2026-10-08. Two programs called `rlink` were a real hazard: `~/projects/bin/rlink` (the original, 9-character names) is the only `rlink` on PATH, and the devsys DCC driver ran whichever `rlink` came first, so a C program built with our `dcc` on Linux silently linked with the old one; on the F256 both can't live in `/dd/CMDS`. `llink` pairs with `las`. Detection, measured on every object the harnesses make: all 19 lwasm-style mainlines export `__lwsec`, none of 9 Microware-style ones (`cstart.r`, RMA programs) and none of 677 Microware library modules do. The mainline decides, not any member: the constants-only `wildbitsl2` has no `__lwsec`. Byte-at-a-time reading (the 6809's memory): native `llink` 19,155 → 19,901 bytes. Checked: lwdiff's 13 links now run with no flag and match lwlink; the cross-built las, dcpp, dcc68 and dco68, linked with no flag, are byte-identical to the old rlink's; `--mw` on `gfxstatus` gives Microware's layout (1,360 bytes, not 1,341); ASan clean |
| D43 | **Step 14, the failures that said nothing.** (1) **dco68's `puls x,u,pc`**: dcc68 now pops a 2-byte remainder into D, not with `leas 2,s`, after a result that is in X (`long`, `unsigned long`, `float`, `double`): `xmodstk` (codgen.c), used by `doreturn` and after a call (tranexp.c); a new `level1.patterns` rule merges `puls d / puls u,pc` into `puls d,u,pc`. (2) `las`'s `errexit` exits 1 when `errno` is 0. (3) dcpp out of memory needed no fix (below). (4) `las`'s macro work file is `rmaPID.tmp`, named when first opened (the sizing trial is another process). devsys's dco68 now reads its patterns from `dcc/COpt` (`DCCDIR`), not `~/.dcc`. Tests: `harness/silent.py` (all four, 2 s) and `harness/native/longret.c`, which `native.py cc` now builds and runs with the native `dcc` | (1) was wider than §10 said: any function returning in X with exactly 2 bytes on the stack at its return, e.g. `long f(a) int a; { int b; b = a; return (long) b; }` (the `_litol` result), not only a call's result. `longret.c` printed 3 wrong results with CoCoC's passes, none with ours, at the same size (4,810 bytes). Same size everywhere: `puls d` is as long as `leas 2,s`, and `puls d,u,pc` as `puls x,u,pc`. dccdiff: 12 CoCoC files differ, at 14 sites, all `leas 2,s` → `puls d` after a mid-function call to a long function (harmless either way); none was a broken return, so CoCoC's libraries never hit the bug. dcc68's own code (`xmodstk`) didn't fit its 29 spare bytes (+72, 48,427 > 48,384): three messages used more than once now have one copy each, as `multidef()` already did (`storerr`, `ptrmis`, `indir` in misc.c), dcc68 48,367 (17 to spare). `~/.dcc` was shared by CoCoC's and devsys's dco68, so a pattern change could never reach one without the other, and dccdiff, comparing the two, couldn't see it. (2) shows up in regress as 4 RMA inputs (CoCoC `keysense.a`, `shadow.a`, `devtyp.a`, `sleep.a`) now exiting 1, objects unchanged: they `use /dd/defs/os9defs.a`, missing on Linux, then stop at the fatal "new symbol in pass two" with half an object, which RMA called success. (3) a 16-page dcpp writes its fatal record ("out of memory") into its output and exits 0 by design (CoCoC lets dcc68 report it); dcc68 reports it and exits 1, and in MAME `dcc` stops there with no object (`ERROR #001`). The half-compiled file §10 recorded was the Linux driver losing exit statuses, fixed in D39. (4) four las runs at once in one directory: 9 of 12 crashed, all 12 objects wrong; now 0 of 20 differ. Native las 46,262 bytes |
| D44 | **The boot disks are built in devsys, from NitrOS9 `main`**: `boot/wildbits/l2/makefile` (`make PLATFORM=k2` / `jr2` there) builds `l2_wildbits_devsys{k2,jr2}.dsk` from `~/projects/wild/main/nitros9` (`make N9=DIR` for another tree). It includes that tree's own `recipes/wildbits/l2/makefile`, so it builds what `main`'s l2 recipe builds, and adds PipeMan, Piper, Pipe and the lean `/nil` (`nzdrv`/`nzdesc`, `NIL=1`) to `BOOTMODS_EXTRA`. NitrOS9's recipes include their helpers by paths relative to the recipe folder (`../wildbits.mak`, `../../rules.mak`, `../../libs.mak`); one-line files of those names in `boot/` and `boot/wildbits/` include the real ones. `NITROS9DIR` is set with `override` (recipes/rules.mak would cut the current directory at `/recipes/`). `native.py` and `card.py` use these disks by default | Owner, 2026-10-11: `main` carries the rc24 memory map, and the Wildbits MAME (`~/projects/wild/mame`) was updated to it; the harnesses had been booting a disk from the netrek tree (a working branch, `wb/multiterm`, with the owner's uncommitted kernel work), and the pipes/`/nil` recipe lived in that tree too (`recipes/wildbits/l2_devsys`, never committed, now superseded). Building in devsys keeps every output here and edits nothing in `main`, except what any NitrOS9 build writes into its own tree (`defs/buildinfo`, ignored there; the fonts and backgrounds under `level1/wildbits/sys`). Bootfiles: K2 32,000 bytes of 32,256 (pipes and `/nil` take 768 after `padup256`), Jr2 31,488. `native.py cc` now also checks `dcc -w` through `/nil` and a shell pipe (on the old netrek disk both fail: "can't open /nil"). Every native suite on the new disk: las 59/59, link 6/6, cc 2/2 with `/nil` and a pipe, lw 200/200, obj 90/90 + 13/13, img 114/114. The same `img` run is a third faster in emulated time, 1,706 s without an image and 789 s with one (2,564 s and 1,163 s on the netrek disk); not yet explained (the rc24 kernel and MAME changed together). The NitrOS9 *source* the harness assembles is separate: `corpus/`, pinned by `harness/corpus.lock` |
| D45 | **make: CoCoC's, forked into devsys** (`make/`, from CoCoC `68fdbf9` `Source/Make`: Tim Koonce's public-domain OS-9 make). Built by `native/GNUmakefile` like the tools (tool `make`, `-s`); installed as `/dd/CMDS/make` with its rules in `/dd/SYS/make.default`. Fork changes: `<types.h>` before `<os9.h>` and `make`'s own `time()` renamed `curtime()`, both for CoCoC's newer headers; `make.default` names `dcc` (compile and link, `-r=` and `-f=`; `-o` is "don't optimize" in dcc) and `las` (`RMA`, `.a.r`). `native.py make` (default set): a C and RMA project with only the link spelled out; builds, runs, and after a minute and `touch util.c` compiles only `util.c` and relinks; with a broken `util.c` stops before the link | Owner, 2026-10-11, after checking for another make: Microware's (§10) has its rules in its code, CoCoC's in a text file, which is what lets the same `make` drive `dcc`, `las` and `llink`. Size: 14,775 bytes, data 5,789; las reproduces its 8 sources (`las` suite 67/67) and llink its link (7/7). OS-9 dates are to the minute and `make` rebuilds a target that is **not newer** than its input, so a target built in the same minute as its inputs is rebuilt by the next `make` in that minute (redundant, never wrong); the test sleeps a minute (3,700 ticks) between steps. NitrOS9's `copy` keeps the source's date, so a copied-in source can look older than its `.r`: `touch` it |
| D46 | **las settles values before pass 2 when a count is defined after its use**, and three fixes found assembling NitrOS9 `main` (step 16's first need: its `wildbitsl2` library). (1) **Settling** (`rma_main.c` `settle`): NitrOS9 `main`'s `defs/wildbits_vtio.d` reserves `gr.SprTbl rmb G.TermMax*8` 140 lines before `G.TermMax equ 9` (the multiterminal vtio, main `2516a467`). RMA's two passes can't do that: pass 1 sizes the `rmb` as 0, so every label after it moves in pass 2 ("phasing error"). In lwasm mode, pass 1 now notes a count or origin (`rmb`, `rzb`, `org`, `fill`, struct instances: `l1723`) whose value uses a symbol it hasn't reached (`unsettled()`, `directives.c`); if it did, las runs pass 2's code as a *settling pass* before the real one: no output (`OutFile` 0), no listing (`listON` 0), no error reports, no reference records (`genreflist`) or common counts; `phaserr` only notes that a value moved. Pass 2 already corrects each label it reaches (`sym_setval` after the phase check), so the loop repeats until nothing moves (at most 8), then the real pass 2 runs as before, phase check included, and reports anything still moving. A defs image: a loaded one is applied by the first settling pass and the defs are read after that (pass 2's existing fallback); one being recorded is given up (`img_settle`; the next run records). (2) **The zero register**: lwasm takes `0` as the 6309's zero register (code `$C`) in `tfr`/`exg`, also for a 6809 (`grfdrv256`: `tfr 0,dp` = `1F CB`); las's register table had blanks there. Lwasm mode only; it goes with either register size. The table holds it as `'0' & 0xdf`, because las's `_toupper` is `c & 0xdf` (digits too). (3) **The sizing trial ran out of blocks** on `main`, silently: `krnp2` (2,560 labels recorded, mostly the defs' struct offsets) and `sysgo.o` (`use defsfile` plus 2,430 `--defs` constants) got "symbol table overflow" in the trial child, which only says "sizing trial failed"; the file then assembles with long offsets, 2 bytes longer than lwasm. Fixed twice over: the trial marks a code label in the symbol's own type byte (`SZ_CODE`, $80, unused) and lists only labels inside an lwasm `section` (the list existed to tell sections apart), so `label_sec` costs nothing for a module and no longer searches 2,500 labels per `,pcr` on the 6809; and on Linux, handles stored inside banked records are 16 bits (`BANKH`, as on the 6809), so the host las runs out of blocks where native las would, no sooner (a symbol took 20 bytes plus its name, now 14; the 6809's 12). `lwdiff` now prints a failed trial by name | Owner, 2026-10-12: "do it the right way" (the alternative was moving one line in `main`), on a branch (`las-settle`) until every suite passed. Settling costs nothing unless pass 1 saw such a count: on `main` 16 of 305 files (the vtio family, `wildbitsl2`) take one settling pass, ~+50% for them. lwasm keeps passing until values settle, so this is the same rule with RMA's structure: pass 2's code already does the correcting, which made the settling pass mostly switches. Measured: lwdiff on `main` (K2): **215 of 215 modules identical, 90 objects, 13 of 13 links** (was 199 match, 16 errors, 89 fatal, 13 link failures); corpus unchanged (200, 13); regress 741/741 (asan, bankwin 743/743); imgcheck 290/290 (config and one), variants 14/14; silent 4/4; `vtio` and `term` identical without an image, while recording (given up) and with an image recorded by `ioman`. New native test `harness/native/settle.asm` in suite `las` (identical to lwasm; the old las gave 8 errors; natively its options go in an `--args` file, as the OS-9 shell splits arguments at commas). All MAME suites: las 68/68 (with settle.asm), link 7/7, cc 2/2 + /nil + pipe, make 3/3, lw 200/200, obj 90/90 + 3 libraries + 13/13, img 114/114 (1,647 s / 783 s). Native las 46,262 → 46,533 bytes, data size 7,904 → 7,906 |
| D47 | **The install disk** (step 16, `harness/card.py`, its files in `install/`). On the devsys boot image (D44): the seven tools in `/dd/CMDS`; CoCoC's C libraries and headers as before; NitrOS9 `main`'s assembler defs in **`/dd/DEFS/NITROS9`** (all of `defs/` plus the l2 recipe's `defsfile`, which adds `MappedIOStart/End`); `main`'s three libraries **`/dd/LIB/alib.l`, `net.l`, `wildbitsl2.l`**, assembled by the host las from the recipe's own job list (lwdiff's dry run) and joined as `merge` would; two **options files** for `las --args=`: `/dd/SYS/n9mod.opt` (module sources) and `n9obj.opt` (object sources, plus `--defs=/dd/LIB/wildbitsl2.l`): the `-D` settings and pragmas `main`'s build uses most for that format (read from the dry run, so `-Dk2` on the K2 card, `-Djr2` in the rehearsal), `-i=/dd/DEFS/NITROS9` and a defs image `/dd/SYS/n9mod.img`/`n9obj.img`, no `--lwasm` (D40 picks the mode); **help**: `install/help/*.hp` (las, llink, dcc, make from CoCoC's `make.hlp`, devsys) merged into `/dd/SYS/helpmsg`, replacing same-named entries; `/dd/DEVSYS`: `README`, `EX` (`hello.asm` with a `struct`, `hello.c`, `main`'s `grep.as`, a make project `M` building `mkdemo`), `G` (selftest's 8 `level1/cmds` sources from `main`), `selftest`. Selftest uses only installed files: dcc + run + `dcc -w`; the 8 commands without an image and with `n9mod.opt` (records the image, then loads it); `hello.asm` assembled **twice to one file** and run; `grep.as` with `n9obj.opt`, linked with the three libraries and run; make; `help las`. `card.py check` compares with the host las on the same layout and options, and that with lwasm/lwlink's own build of `main` | Owner's success test: build an assembly and a C program on the K2 from the disk alone, and exercise `struct`, mode detection and `dcc -w`. An options file over a new `--nitros9` flag: no las code (las has 1,851 bytes left), and the settings come from `main`'s makefiles rather than being frozen into las. Defs in a subdirectory: CoCoC's `/dd/DEFS/defsfile` (RMA-style, `os9defs`) has the same name as NitrOS9's. Libraries by the host las: native las gives the same bytes (`native.py obj`), so building them on the card would only cost time. Not added: a make rule for `.asm` modules (las writes to the data directory, make's `ODIR` is the execution directory's). Rehearsal in MAME (Jr2): **all good**: hello identical, `dcc -w` through /nil, 8 modules identical without and with the image and equal to `main`'s lwasm build, image 31,082 bytes, hello.asm identical and runs, grep object and link identical (equal to lwlink's build) and finds its line, make builds `mkdemo` ("mkdemo: 5 42"), `help las` shows ours; MAME times: C 4 s, 8 modules **234 s / 109 s**, hello.asm + grep 24 s, make 5 s; 1,864K free before and after. Wall clock 114 s |

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
every NitrOS9 assembly. Without a flag, las picks the mode from the source
(D40: `psect` means RMA, `section` or `mod` lwasm); `--rma` forces RMA. Every
RMA source has a `psect`, so the RMA/DCC
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
| `struct` / `endstruct`, `sizeof{}`, instances | 1 (`minted`) | ✅ lwasm mode (D41) |
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
3. **Precompiled defs** (phase 9, D35). Record what `use defsfile` did
   once, in a file; on later runs, if the image is current, create its
   symbols directly instead of parsing ~1,600 equates in each pass. Not a
   raw image of the blocks: the table before the `use` (the `-D` symbols)
   decides each bucket's tree, so the symbols are added again in their
   creation order. One image per configuration (`-D` set, options, files).

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

The MATCH path was first exercised in phase 3 (72 files), checked
independently: header parity and CRC recomputed.

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
(`--map obj/TOOL.map --break FUNC` prints who called FUNC; `FUNC+HEX`
stops at an address inside it, e.g. `_stkcheck+2C` for CoCoC's stack
overflow).

Phase 8 and 10 added two suites: `lw` (the 200 NitrOS9 module sources,
compared with `las/build/nosz`) and `obj` (D34). **Run every suite after
any change to a native tool**, not just the ones the change is about:
phase 8 checked `lw` and native rlink but not `las`, and broke every
external reference native las wrote (`do_reflist` still `fwrite`d the
3-byte record from the address in `sref`, which had become a handle; the
host build took the portable branch, so no host check could see it). The
`las` suite found it at once: 2 of 57.

**Real hardware (step 11).** `harness/card.py` builds a card from the
owner's K2 boot image: the six tools in `/dd/CMDS`, CoCoC's libraries in
`/dd/LIB` and headers in `/dd/DEFS`, `/dd/TMP`, and `/dd/DEVSYS` with
`hello.c`, eight `level1/cmds` sources with their defs and the procedure
file `selftest` (dcc builds and runs hello; las assembles the eight without
and with a defs image; `date -t` between steps; everything to
`/dd/DEVSYS/R`). `card.py check` compares a card read back with the host
tools; `card.py mame` runs the same card for the Jr2 in MAME first (the
Wildbits MAME has no K2). The K2 image's `DEFS` was empty and its `CMDS`
has none of the six names, so installing replaces nothing. Selftest runs
once per card write: the shell won't redirect into an existing file (#218). The second card adds `mfree` before and after and
`mmap` at the end: free RAM, and a check that every tool gave its blocks
back (`mfree` prints the block count in hex). Step 16 (D47) makes the card the install disk: NitrOS9 `main`'s defs, libraries and options files, help, a README and examples, and a selftest that uses only what is installed (C, 8 commands with and without the image, an assembly module with `struct`, `grep` linked with `main`'s libraries, make, `help`); `check` also compares the host results with lwasm's and lwlink's build of `main`.

**Defs images (phase 9).** `harness/imgcheck.py` runs lwdiff's 290 jobs
three times (no image, recording, loading) and requires the same object,
messages, exit status and symbol table: per configuration, with one image
shared by every job (`--share one`: most loads must refuse), and on the
asan, bankwin and nosz builds. `--variants` adds 14 cases that must or must
not load (`-DH6309` added or changed, a touched `os9.d`, a source that
changes a defs symbol or defines a defs name first, no `nam`), each also
compared with plain assembly. Natively, `native.py img` assembles the
commands of `level1/cmds` without and then with an image.

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
| **4. lwasm syntax** | lwasm syntax and semantics in lwasm mode; see §4 status table | most `.asm` files match | **done** 2026-10-02 (`struct`, the 1 file left, on 2026-10-07: D41). 160 of 290 match (was 72). Every remaining DIFF is phase 3b sizing; remaining errors are phase 5 `section` files. RMA/DCC regression 450/450; AddressSanitizer clean |
| **5. Object mode** | `section`/`export` → ROF; header defaults | `.as` files link with rlink and match | **done** 2026-10-02. `las/lwobj.c` (sections → psect/vsect/csect, `__os9`, export pragma, `rmb` by section type, `dtb`, `=`), `las/preload.c` (`--defs`), rlink fork with `--lwlink` (D13–D15). 90 of 90 object files assemble; 4 of 13 linked programs byte-identical to lwlink; the other 9 differ only by instruction sizing (10 objects larger than lwasm's, phase 3b). AddressSanitizer clean (las and rlink) |
| **6. Long names downstream** | rlink and DCC name limits; DCC `ASSEMBLER` → `"las"` | 24-char names survive compile → link | **done** 2026-10-02. rlink `SYMLEN` 63 (phase 5) and 8-character matching for Microware objects (D19); DCC fork (D18) with 31-character names, driver runs `las`. `harness/dccdiff.py`: all 198 CoCoC sources that compile give identical output apart from longer names (59 files). A test program with names that collide at 8 characters, calling `localtime()`/`intercept()` from CoCoC's `clib.l`, builds with `dcc` and every call lands on the right address (the original rlink fails it). Fixed in DCC: `getword` overflow, two uninitialised pointers in declarations (`xlkmap.c` now compiles), an out-of-bounds read in the optimizer. regress uses the devsys DCC: 451/451. AddressSanitizer clean (dcc68, dco68, rlink, las) |
| **7. Native build** | Compile fork + rlink + DCC with DCC; run on F256 | the tools run on the F256 and give the host's output on every input that fits in 64K (`scfg.asm` moved to phase 8: it can't fit without banked symbols, §6) | **done** 2026-10-02. All six tools cross-build with DCC (`native/GNUmakefile`: las 34,149, rlink 18,654, dcc 9,589, dcpp 19,014, dcc68 48,290, dco68 11,558 bytes). In MAME (`harness/native.py`): native las assembles all 56 `.a` files of the toolchain's own C sources identically to host las; native rlink links rlink, dcc, dcpp and dco68 identically; native `dcc` compiles, links and runs a C program, module identical to the host passes'. las and dcc68 can't be linked natively yet (rlink runs out of memory: phase 8, D24). Fixed: three 6809-only las bugs (§10); rlink names at their own length (D23) and an `ext_ref` reuse overflow that change exposed; K&R and library gaps in rlink, COpt (§10); dcc68's memory (D25). Host results unchanged: regress 741/741, lwdiff 199 match and 13/13 links, AddressSanitizer clean (rlink) |
| **8. Banked blocks** | `F$AllRAM`/`F$MapBlk` storage for las's symbols and rlink's link data (D24) | `scfg.asm`, `fm.asm`, `rbf.asm`, kernel assemble natively; native rlink links las and dcc68 | **done** 2026-10-02. Symbols as handles (D26) and banked storage (D27, `las/bank.c`); rlink needed no banking (D29); `--args=FILE` (D28). In MAME: **199 of 200** NitrOS9 module sources assemble natively, byte-identical to `las/build/nosz` (`native.py lw`, 8 shards, 22 minutes, D34), including `scfg`, `rbf`, `scf`, `krn`, `krnp2` (there is no `fm` source in this build); the one left is `minted` (`struct`, also on Linux). Native rlink links all six tools identically. Host: regress 741/741, `build/bankwin` 741/741 (negative control fails 242), AddressSanitizer clean (las, rlink), lwdiff unchanged. Also fixed: `%.*s` in `symkey` (CoCoC printf has no `*`), commons written with address 0 on the 6809 (`PUTINT`); and, found while finishing (§8): every external reference native las wrote was empty (`do_reflist` `fwrite` of a handle, a phase 8 regression the `las` suite caught: 2 → 57 of 57), and `llwbsd` overflowed the stack (D30) |
| **9. Precompiled defs** | Saved defs images | measurable speedup, same output | **done** 2026-10-02. `las --defsimg=FILE` (`las/defsimg.c`, D35). In MAME (`native.py img`): the 57 commands of `level1/cmds` assembled by native las without and then with an image, **112 of 112** modules identical to `las/build/nosz` both ways (`minted` left out: `struct`, fails on Linux too); emulated time **1,556 s → 667 s** (2.3×). Model, `echo`: 33.7M → 5.3M instructions. Host: `imgcheck.py` 290/290 per configuration and with one shared image, 14/14 variants, on the unix, asan, bankwin and nosz builds; regress 741/741 (also asan, bankwin); lwdiff 199 match, 13/13 links. Native las 40,168 bytes (D36). All MAME suites rerun: las 58/58, link 6/6, cc, lw 199/200, obj 90/90 + 3 libraries + 13/13. Found: dco68 loses a `long` returned from a one-argument call (§10) |
| **10. Libraries** | alib, net, wildbitsl2 as ROF `.l` | `.as` programs build natively | **done** 2026-10-02. In MAME (`native.py obj`, D34): native las assembles all **90** object-format sources (89 byte-identical to `las/build/nosz`, `wbinfo` differing only in its `dtb` date), `merge` builds the three libraries, and native `rlink --lwlink` links **13 of 13** programs identical to host rlink (`wbinfo`: date and CRC). Fixed on the way: `--defs` constants in banked blocks (D32); dcc68's test of a `char` function result (D33); rlink's buffer overlap after `freopen` (§10) and its memory in `--lwlink` links (D31); the harness staged `use ../defs/x.d` in the wrong place, which emptied the constants object. Host: regress 741/741, bankwin 741/741, AddressSanitizer clean (las, rlink), lwdiff 199 match and 13/13 links, dccdiff 198/198 |

**After the roadmap** (owner, 2026-10-02; steps 14–19 owner, 2026-10-08: what a complete assembly and C environment on the machine still needs), in this order:

| Step | Work | Done when | Status |
|---|---|---|---|
| **11. Real hardware** | A card for the owner's **K2** (SD card): `harness/card.py build` → `dd` → `/dd/DEVSYS/selftest` on the machine → `dd` back → `card.py check` | the K2 gives the Linux tools' output; F256 timings recorded | **done** 2026-10-05. The owner ran the 2026-10-02 card (las before D37) on the K2 and read it back (`harness/in/k2_back.dsk`); `card.py check`: **all good**. dcc built `hello.c` identical to the host passes and it ran; the 8 `level1/cmds` modules identical to `las/build/nosz` without and with a defs image. K2 times: hello 4 s, 8 modules **158 s** without an image and **70 s** with it (MAME on the Jr2: 190 s and 84 s, so the K2 is about 17% faster). **Second card** (2026-10-05, D37 las, `mfree`/`mmap` added): all good, 157 s / 70 s, **1,656K free** before and after (no blocks kept) |
| **12. Room in las** | Measure las's data and stack use; then a helper program (fork and share, §10) or a one-block data area (6 module slots) | room for native sizing | **done** 2026-10-05. One-block data area (D37, owner's choice over a helper program): static data 7,596 → 4,844, data area one block (7,936 bytes, never grows), las 40,090 bytes of 48,384 (**8,294 to spare**, was 24). Model: `llwbsd` leaves 869 bytes between heap and stack, RMA-mode inputs ~2.2K. Host: regress 741/741 (also asan, bankwin), lwdiff 199 match and 13/13 links, imgcheck 290/290 on every build. All MAME suites: las 58/58, link 6/6, cc, lw 199/200, obj 90/90 + 3 libraries + 13/13, img 112/112 (1,555 s → 667 s, unchanged) |
| **13. Native sizing** | `F$Fork` las itself for the sizing trial, sharing the symbol blocks by number, sizes back through a pipe (§10 "Fork and share") | native lwasm-mode output equals lwasm's | **done** 2026-10-05 (D38). las `F$Fork`s itself with `--sztrial=N`; the sizes come back in one shared RAM block (the F256 boot has no pipe modules), not the symbol blocks (the trial needs its own). Trial records in banked chunks on every build. In MAME, native las output now equals host las **with** sizing (so lwasm's): `lw` **199/200** (`minted`: `struct`), `obj` 90/90 + 3 libraries + 13/13 links, `img` 112/112 both ways, las 58/58, link 6/6, cc. Cost: `level1/cmds` 1,555 → **2,505 s** without an image, 667 → **1,145 s** with one (pass 1 runs twice); las 43,420 bytes (4,964 to spare), data 7,889. Host: regress 741/741 (asan, bankwin too), lwdiff 199 + 13/13, imgcheck 290/290 on every build and `--share one`, variants 14/14. K2 card (`harness/out/devsys_k2.dsk`) rehearsed in MAME: all good, 8 modules 309 s / 142 s (was 190 / 84), 1,648K free before and after. **On the K2** (third card, read back as `harness/in/k2_back3.dsk`): `card.py check` all good; `copy` is the one of the 8 whose output sizing changes (1,469 bytes, 1,470 without), so native sizing is proven on the hardware; 8 modules **255 s / 120 s** (was 157 / 70), 1,656K free before and after. Model fix: `os9run.py` F$Fork syncs mapped blocks. Native harnesses now compare with `las/build/unix` (D21 superseded); `native.py --seconds` default 7,200 |
| **14. Silent failures** | The bugs that report success or make wrong code without a word (§10): dco68 loses a `long` returned straight from a one-argument call; `las`'s `errexit` exits 0 when `errno` is 0; `dcpp` out of memory stops early with status 0; `las`'s macro work file is always `rma.tmp` | each has a test that fails before the fix and passes after; regress, dccdiff and every native suite pass | **done** 2026-10-09 (D43). `harness/silent.py` 4/4 (each failed before); `native.py cc` builds and runs `longret.c` natively. dco68's bug was wider than recorded; dcpp's needed no fix; regress differs only in 4 exit codes that now report a real failure |
| **15. make** | CoCoC's `make` (`Source/Make`) built with the devsys DCC, cross and then natively; its help file | a small project with a makefile (C and assembly) builds and rebuilds only what changed in MAME, and stops at a failing step | **done** 2026-10-11 (D45). `native.py make` 3/3 in MAME on the devsys disk; `make` 14,775 bytes |
| **16. Install disk** | A devsys disk from the devsys boot recipe (`boot/wildbits/l2`, D44: NitrOS9 `main`, pipes, `/nil`): tools in `/dd/CMDS`; C libraries and headers; the NitrOS9 assembler defs and the natively built `alib`/`net`/`wildbitsl2`; a defs image; a short way to give NitrOS9's options (`--nitros9` or a standard `--args` file). The harnesses move to the devsys disks (done 2026-10-11, D44). A fourth K2 card | the owner builds an assembly and a C program on the K2 from that disk alone; the card also exercises `struct`, mode detection and `dcc -w` | **done** 2026-10-12. First, las had to assemble `main` (D46: lwdiff on `main` 215/215, 13/13 links). The disk (D47): rehearsed in MAME, all good. **On the K2** (fourth card, read back as `harness/in/k2_back4.dsk`): `card.py check` **all good**: hello identical, `dcc -w`, the 8 modules identical without and with the image and equal to `main`'s lwasm build, hello.asm identical and runs, grep identical to lwlink's build and finds its line, make builds `mkdemo`, `help las` ours. K2 times: C 7 s, 8 modules **220 s / 102 s** (third card 255 / 120; MAME now 234 / 110, so the K2 is about 6% faster than this MAME), hello.asm + grep 23 s, make 7 s; **1,888K free** before and after. The `ERROR #115` on screen is `main`'s grep exit status (§10) |
| **17. Self-hosting** | Build `las`, `llink` and DCC on the F256 with themselves (measure dcc68's and dcpp's memory on las's larger C files first) | the natively built tools are byte-identical to the cross-built ones | |
| **18. Long-name C libraries** | Rebuild `clib.l`, `sys.l` and `cstart.r` from CoCoC's `Source/Libs` with the devsys DCC and `las` | C programs link without D19's aliases; `native.py cc` and dccdiff unchanged | |
| **19. Speed** | Skip the sizing trial when it can't change anything (§10 "Native sizing costs a second pass 1"); measure on the K2 | `level1/cmds` without an image back near 1,555 s in MAME, same output | |

**Step 12 measurement** (2026-10-05). Native las under `os9run.py --stats`
(now also reports the deepest stack, the function there, and CoCoC's
`ibrk` top `_mtop`) on 348 inputs: the 200 NitrOS9 module sources, the 90
object sources (with `--defs`) and the 58 RMA-mode tool sources. All give
the host's results except `minted` (`struct`, as on Linux). Peaks:

| Part | Bytes | What |
|---|---|---|
| Static data | 7,596 | `end` = $1DAC: `CondBits` 2,048 (module.c, 16,384 conditionals), `SecNames` 1,024 (lwobj.c), rma_main ~2.5K (command tables, line buffers), defsimg 381, stdio `_iob` 208 |
| `ibrk` heap | 768–1,536 | stdio buffers: `_setbase` takes 256 (disk) or 128 (terminal) per FILE slot and never returns them, so it grows with `use` nesting (`llwbsd` the most) |
| Stack | 136 assembling; 349 at start | the start-up peak is `args_file`'s 256-byte `word` (via `getc`, `_setbase`, `getstat`), when the heap is still small |
| `sbrk` (`getmem`) | 0–256 | macro descriptors; only `llwbsd` grew the data area (13,056 → 13,312) |

So las needed about **9.5K** at its worst, in a 13K data area (2 blocks).
The two big arrays were almost empty, so D37 shrank them instead of
moving them into the banked blocks. Free between
the heap and the stack: never less than 3,296 bytes (`shellplus`).

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

**As built (`harness/native.py`).** Starts from the devsys Jr2 boot disk
(`boot/wildbits/l2/l2_wildbits_devsysjr2.dsk`, D44; until 2026-10-11 the
netrek tree's `l2_wildbitsjr2.dsk`; `--image` for another), copies it to `harness/out/native/test.dsk` and pads
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
- [ ] `F$MapBlk` cost per call on the F256 — sets how much bank switching hurts. Phase 9 suggests it now dominates: with an image, `echo` runs 6.4× fewer instructions (model) but `level1/cmds` is only 2.3× faster in MAME, and a load still makes ~5,900 window switches (each `sym_add` walks a bucket's tree across blocks). Next speedup: fewer switches while loading (e.g. keep a bucket's tree in one block, or load into blocks in bucket order). Counted (logic, so valid in the model): `scfg.asm` makes about 22,500 window switches (each F$ClrBlk + F$MapBlk) for 78M instructions. Time it on the hardware; if it matters, a second window slot or keeping each hash bucket's tree in one block would cut switches.
- [x] How much RAM does the target F256 have free for symbol blocks? The machine has 1800K+ (owner, 2026-10-02); measure what a running system leaves free in phase 8. The step 11 selftest didn't measure it; the second card (2026-10-05) runs `mfree` before and after (and `mmap`), and `card.py check` fails if blocks weren't given back. MAME's Jr2 image: 1,648K free (206 blocks). **The K2: 1,656K free (207 blocks)**, the same after the run (2026-10-05). las can hold at most 15 symbol blocks (`MAXBLK`, 120K), about 7% of that.
- [x] Do the K2's results match MAME's? Yes (step 11, 2026-10-05): every output of the selftest is identical to the Linux tools', and the K2 runs it about 17% faster than MAME's Jr2.
- [x] DCC limits when compiling the tools natively (phase 7). What it took: K&R declarations in rlink's `proto.h`; `#define void int` in COpt; lwsel's struct members renamed (K&R C has one namespace for all struct members, and `refs` was taken); `atexit` and `ftruncate` replaced (CoCoC's clib has neither; `_ss_size` sets the size); the source directory searched before CoCoC's `Defs` (COpt has its own `ctype.h`). Three 6809-only bugs in las: `MLCod.q_long` and `MLCod.shrtlng.f_byt` (K&R took them as offset 0 of `MLCod`, clearing the opcode), and a `long` passed where `do_rmb`'s callees take an `int` (no prototypes, so the callee read the high word: 0). gcc's `-Wtraditional-conversion` over a copy where those values are `long` found the last kind; only two sites.
- [ ] Does anything in the build depend on lwasm listing or map output?
- [x] (D43, step 14) `errexit()` exits with `errno`, which is 0 unless a system call just failed, so some fatal errors (e.g. "bad -D value", "symbol table overflow") report success to the shell. Fix: exit 1 when errno is 0. Changes exit codes, so do it as its own small step.
- [x] `ifp1` blocks were read only in pass 1, so the defs' effect on the location counter (lwasm leaves `*` = 8 after the Level 2 wildbits defs) differed between passes. Solved in phase 4: lwasm mode makes `ifp1` true in both passes and replays conditionals (D12).
- [ ] RMA mode still matches directive names as prefixes, so `setdp` is taken as `set` there (lwasm mode requires exact names). Fixing it in RMA mode would change RMA behaviour; decide if RMA-mode sources ever need `setdp`.
- [x] `struct`/`endstruct` (lwasm): done 2026-10-07 (D41); `minted` matches, lwdiff 200/200.
- [ ] las has no `export` **directive** (lwasm's `export name`), only the `export` pragma the NitrOS9 build uses; no corpus source uses the directive. Found testing D41.
- [ ] In a **raw** file (no module, no section), lwasm fills an `rmb` between data with zero bytes; las writes nothing, so later bytes move up (`fcb 1 / x rmb 2 / fcb 2`: lwasm `01 00 00 02`, las `01 02`). Not in the corpus (lwdiff matches); a struct instance there behaves as `rmb`. Found testing D41.
- [ ] `wbinfo` contains `dtb` (assembly date and time), so its link matches lwlink only when both run in the same second. The harness should mask those 6 bytes or treat the file as nondeterministic. `sysgo` has `dts` likewise: `imgcheck.py` on the slow AddressSanitizer build once reported `sysgo.o: load: data differs` (2026-10-05), passing on rerun.
- [ ] `--defs` resolves a value at assembly time where lwasm left it to the linker. Where that changes an instruction's size choice (e.g. an `equ` defined from defs constants then used as an address), las and lwasm may differ. None seen in the corpus after phase 3b.
- [x] **Sizing on the 6809** (answered in step 13, D38): las `F$Fork`s itself with a hidden `--sztrial=N` and gets the sizes back in a shared RAM block (no pipe modules on the F256). Native lwasm-mode output now equals host las's, so lwasm's. The trial does double pass-1 time (see below).
- [x] **Fork and share** (owner, 2026-10-02: the way past 64K per process). Use (1) done in step 13 (D38) with one shared block for the answers rather than the symbol blocks (the trial needs its own symbols); the parent allocates and frees it, the child only maps it. Use (2) not needed so far. OS-9 programs are reentrant, so a second process running a module already in memory costs only its data blocks, and the F256 has 1800K. `F$MapBlk` maps any block number with no ownership check (`level2/modules/kernel/fmapblk.asm`), so las's symbol blocks are shared memory: pass the block numbers to a child. CoCoC has `os9fork` and `wait` (the `dcc` driver uses them, proven by `native.py cc`); a named pipe (`/pipe/name`) needs no `dup`. Two uses: (1) **native sizing**: `F$Fork` las itself with a hidden option to run the trial pass and send one byte per site through a pipe, making native output lwasm-identical (D21); cheap in RAM, the trial code must fit in las's own map (D37: 8,294 bytes free). (2) **code room** (not needed after D37 gave las 8K): move a separable job into a smaller helper program (the `--defs` ROF reader, 1.5K, could write a defs image las already loads; the listing; image recording). Open: who frees shared blocks (`F$DelRAM` once, after both finish), and choose between a helper and a one-block data area (measured: las needs ~9.5K of data, §9 step 12)
- [ ] **Native sizing costs a second pass 1** (D38). MAME, `level1/cmds` (57 modules): without an image 2,505 s (1,555 before step 13), with one 1,145 s (667). Only 74 of the 290 corpus files have a `,pcr` site at all; for the others the trial changes nothing. A cheap exact test that a source has none would save most of it, but `,pcr` can come from `use` files and macros, so it would have to look at what pass 1 reads. Possible: let the parent skip the trial when the trial of an identical configuration found no sites (a note in the defs image), or measure on the K2 first and decide if it matters.
- [x] **`dcc -w` needs `/nil`**, which the F256 boot didn't have. /Nil and pipes now come from the devsys boot recipe, `boot/wildbits/l2` (D44, 2026-10-11; first from a recipe `l2_devsys` in the netrek tree, 2026-10-07): the l2 recipe plus PipeMan/Piper/Pipe and the lean /Nil (`nzdrv.asm`/`nzdesc.asm` with `NIL=1`: 79 bytes, not VRN's 506), in `BOOTMODS_EXTRA`. Bootfile K2 30,976 / Jr2 31,232 of 32,256. Disks `l2_wildbits_devsys{k2,jr2}.dsk` (the recipe's own contents: no Joust/Netrek/Tempest, which were copied onto the l2 disks by hand). Checked in MAME (Jr2): `mdir` lists the five modules, `>/nil` and `list /nil` work, `echo ! tee` pipes. `-w` itself was broken (`-n` to dcc68); fixed in D39, and `dcc -w` now runs natively on the devsys disk (`dcc68: -o=/nil`). Harnesses still use the l2 disks.
- [ ] Native sizing is for **one source file** only (D38): with several, the child would have to assemble the earlier files first, each with its own trial. lwasm takes one file, and the harness never passes more; native las warns and keeps long offsets.
- [ ] Native las's trial child is silent: `errexit` and the file-name prefixes print nothing when `SzTrial` is set, as the Linux child's output goes to `/dev/null`. A trial that fails says only "sizing trial failed; using long offsets", then the parent's own pass 1 reports the real error. D46 found the costly case: the trial alone ran out of blocks (the parent didn't), so the file assembled with long offsets and showed only as 2 bytes of difference; `lwdiff` now names such files ("sizing trial failed").
- [ ] (D46) **The sizing trial doesn't settle.** It decides `,pcr` sizes from its own pass 1, where a count defined later is still 0, so a `,pcr` across such an `rmb` **in the code** would be sized on the wrong distance (the settled result is consistent, but may be long where lwasm is short). No source does this (`main`'s forward counts are in `org 0` structures); lwdiff would show it.
- [ ] (D46) lwasm **differs from itself** on a forward count used by an `rmb` inside a module: `buf rmb CNT*2 / tail rmb 1 / fsize equ .` outside the module, `CNT equ 5` after, then `rmb fsize` in the module's data: lwasm's header says data size 1 (it sized `buf` with `CNT` = 0), las 11, the arithmetic. No source in `main` does it; las is not changed to copy it.
- [ ] (D46) A **circular** count (`a rmb b` / `b rmb 1`) is an error in lwasm ("Cannot resolve line address"); las settles on 0 without a word, as it did before. Only nonsense source gets here.
- [ ] `harness/os9run.py` models a mapped block as a copy in the slot. A parent that keeps a shared block mapped while its child ran wrote its stale copy back over the child's answers (found in step 13; on the machine both map the same RAM). F$Fork now writes the parent's mapped blocks back before the child and rereads them after. Other sharing patterns (two processes mapped at once, running concurrently) still aren't modelled.
- [ ] Sizing model limits: a literal `,pcr` target (`leax 5,pcr`) and targets that are `equ *` symbols are treated as unreachable (long); forward-referenced constants in direct/extended and `n,X` operands keep RMA's long form where lwasm would shorten them; distances use las's layout, which differs from lwasm's if a file interleaves sections. None occur in the corpus.
- [ ] lwasm's symbolic `org`: `scf.d` does `ORG PD.OPT` with `PD.OPT` external, so lwasm's `PD.EKO` is "PD.OPT+4" and `PD.EKO-PD.OPT` cancels to 4 (5-bit offset). las makes the labels absolute and keeps `PD.OPT` as a reference, so `opts.o` has three 16-bit offsets (not linked by any of the 13 programs). Fix: give labels under a symbolic `org` the base as a reference, and cancel +X/−X reference pairs.
- [ ] **C libraries with long names** (phase 10): CoCoC's `clib.l` etc. still have 8-character names (`localtim`), served by D19's aliasing. Rebuilding them with the devsys DCC removes the need. Only one direction is handled: an old object referencing `localtim` would not find a rebuilt library's `localtime`.
- [ ] DCC's `ttl` lines still cut function names to 8 characters (cosmetic: listing titles only).
- [x] On the F256, `dcc` looks for its libraries on the default drive. `harness/native.py` installs the layout it needs: the six tools in `/dd/CMDS`; `cstart.r`, `clib.l`, `sys.l`, `level1.patterns`, `level2.patterns` in `/dd/LIB`; CoCoC's headers in `/dd/DEFS`; `/dd/TMP` for temporary files.
- [x] **rlink could choose `--lwlink` itself**: done 2026-10-08 (D42, with the rename to `llink`). (owner asked, 2026-10-08), as las chooses its mode (D40). Signal: the mainline, the one input module with a non-zero type/language. Every lwasm-style mainline exports `__lwsec` (D17): the 13 NitrOS9 programs and the 6 `.as` mainlines in regress. No Microware-style module does: `cstart.r` and all 328 modules of CoCoC's libraries, 8 RMA mainlines, 151 RMA and 199 DCC library-style modules. Library members agree, except the constants-only `wildbitsl2` (no code section, so no `__lwsec`), which is why the mainline should decide, not any member. The choice must come before `lw_select` (rlink_main.c), so it needs a scan of the input files' ROF headers and global symbols first. No mainline among the inputs: Microware, today's default; `--lwlink` (and a new `--mw`) still force a layout.
- [ ] **Microware's own OS-9 `make` exists too** (owner, 2026-10-11: be aware of it): `nitros9-languages/ccompiler/make_dis.asm`, a 6,404-line recovered disassembly (edition `$12`) that NitrOS9's ccompiler `Makefile` assembles with lwasm, with its help page `level2/sys/make.hp`; also in `corpus/`. Its implicit rules build Microware `cc` commands in code (`-r=`, `-o=`, `-f=`; macros `CFLAGS`, `RFLAGS`, `LFLAGS`, `ODIR`, `RDIR`, `SDIR`), so changing them means editing the disassembly; `-b` and explicit commands would work with our tools. Step 15 uses CoCoC's `make` instead (rules in a text file). Both are called `make`: only one can be `/dd/CMDS/make`. Also a free test for `las`: an lwasm module source outside lwdiff's job list.
- [x] (D47: the install disk's `helpmsg` gets devsys's own `@MAKE`, from CoCoC's `make.hlp`; main's wildbits disk had no make entry) **`help make` describes the other make.** NitrOS9's disk has `level2/sys/make.hp`, Microware's options (`-b -d -f -i -n -s -t -u -z`); CoCoC's `make` has `-f -m -n -i -s -d -p -b -r -t -q -u -z` and its own help files (`make/Help/*.hlp`, for CoCoC's help system). Step 16 decides what the install disk's `help` shows.
- [ ] (D47: kept for now; the README says to give llink and `dcc -f=` a full path) On OS-9, llink opens its output with mode `"wx+"`: a bare `-o=name` goes to the **execution directory** (`/dd/CMDS`), as Microware's `cc` did; the Linux build writes to the current directory. Keep (it's how `dcc` installs programs) or change for building NitrOS9 natively?
- [x] lwasm mode is untested natively. Answered in phases 8 and 10: `native.py lw` (200 module sources) and `obj` (90 object sources, libraries, 13 links) run lwdiff's inputs natively against `las/build/nosz`.
- [ ] lwsel (D31) ignores constants-only library members when choosing members, so a name exported both by one and by a later code member picks the code member (lwlink would take the constants member and not the code). Not in the corpus. If it matters: keep a hash of constants-only names (2 bytes each) and check a match against the file.
- [ ] `harness/os9run.py` modelled `SS.Size` with `fstat`, which missed Python's buffered writes, so `lseek(fd, 0, 2)` on a file just written returned the old size: native las's nested macros looked broken in the model but worked in MAME. Fixed (flush first); another reminder that MAME is the proof.
- [ ] CoCoC's stdio keeps a FILE slot's first buffer (`_setbase`, `ibrk`) through `fclose`/`freopen`, so setting `_bufsiz` on a used slot runs past it. rlink did that after every `freopen` (1,024 on a 256-byte buffer) and overwrote OutFile's buffer in `--lwlink` links; now it sets it only while `_base` is 0. las set `macfp1->_bufsiz = 512` the same way (the `--args` file may have used the slot first); guarded too. No other `_bufsiz` writes in the tools.
- [x] (D43, step 14: not a bug today; dcc68 reports dcpp's record and `dcc` stops) dcpp with too little memory stops early **with exit status 0** and a truncated output file (seen with 16 pages). The `dcc` driver then compiles half a file. Make dcpp report it.
- [x] `harness/os9run.py` now models the F256's two limits (D25), F$AllRAM/F$MapBlk/F$ClrBlk/F$DelRAM with real block contents, and reports blocks never given back.
- [ ] `harness/os9run.py` doesn't model `F$Link`, so the `dcc` driver (which links `Init` to find the default drive) only runs in MAME.
- [ ] las merges all of a file's code-like sections into one psect; lwlink places `code`, `.text`, `data`, `.data` separately. A file mixing them is placed by its first (`__lwsec`).
- [x] (D43, step 14: fixed in dcc68, and wider than described here) **dco68 loses a `long` returned straight from a one-argument call** (phase 9). `level1.patterns` turns `leas 2,s / puls u,pc` into `puls x,u,pc`, which is right when the result is in D but clobbers X, where DCC returns a `long` (a pointer to it). `long tell () { return ftell (Fp); }` returned garbage natively; `defsimg.c` uses `mark (&pos, back)` instead. No such function in the six tools today (checked every `lbsr …; leas 2,s; puls u,pc` site: the one `_lmove` is in a void function). Fixing the pattern costs 2 bytes per site in every tool (las has 24 to spare); a safer fix would let dcc68 mark functions that return in X
- [ ] (D47) **`grep -n` numbers lines differently on the 6809 and in `os9run.py`**: `main`'s grep counts in BCD with `inca` / `daa`; for README line 52 MAME **and the K2** print `0040::` (digits `0040:`, so the count bytes are `$04 $0A`), the model `00044:` (grep counts only lines at least as long as the pattern: 44 is what an ideal `daa` gives). The K2 agrees with MAME, so the model differs from the real CPU somewhere on this path (`daa` with a stale H or C, or a flag another instruction leaves); not yet found. A plain stale-H `daa` doesn't explain `$04 $0A` either. Our `grep` is byte-identical to lwlink's build, so the toolchain is not involved.
- [x] (D47) **`ERROR #115` after the selftest's grep line, on the K2 and in MAME** (owner, 2026-10-12): `main`'s `grep` exits with a leftover byte in B, 115 when it found a line, 110 when not (model). Cause in `main`'s `lib/alib/fgets.as`: `FGETS_NOCR` saves D with `pshs d,x` and restores it with `puls d,x,pc` on the error path too, so `I$ReadLn`'s `E$EOF` in B is replaced by the caller's old B; grep's `cmpb #E$EOF` then fails and it exits with that byte. Same in lwlink's build (`/dd/CMDS/grep`). Fix belongs in `main` (store B into the saved D at `1,s` before `puls` when carry is set); not changed here. The selftest result is right; the README warns about the message.
- [ ] Defs image limits (D35): one image per configuration, so jobs whose `-D` values differ (`NOS9VER=0` vs `NOS9VER=`) need separate files even where the defs never read that symbol; an image is for the machine that wrote it (byte order, date format); OS-9 dates are to the minute, so an edit within the minute that keeps a file's size isn't noticed; a `use` resolving to another directory's `defsfile` re-records (the image holds paths). With an image loaded, the listing has no defs lines (the symbol table is the same)
- [ ] `imgcheck.py` once reported 289 of 290 with the AddressSanitizer build and never again in 9 runs; the difference wasn't printed. A real race found while looking: on Linux the sizing trial (a child) loaded the image while pass 1 might be re-recording it (`"w"` truncates); the trial now reads the defs (same effect, no code in them). Watch for it
- [ ] Bugs fixed in the forked rlink during phase 5: `grab()` returned unzeroed memory that rlink relied on being zero (every link crashed under AddressSanitizer; worked by luck); `fprintf(OutFile, modname)` used the name as a format string; a 16-character psect name read into a 10-byte buffer; the unresolved-references report dereferenced a null pointer; the makefile lacked header dependencies (a SYMLEN change left stale objects). In las: an exported `set` symbol was counted twice in the ROF header (malformed ROF).
- [ ] RMA assembles each input file as a separate unit (both passes, then that file's ROF trailer). Fine for ROF libraries; for modules, pass one source file.
- [x] (D43, step 14: `rmaPID.tmp`) las's macro work file has a fixed name (`rma.tmp` in the current directory), so two las runs in one directory collide, natively too. Use a unique name (process ID) or a temp directory.
- [x] (Answered in phase 8, D26/D27: entries and names both move to banks; fields only through accessors, handles instead of pointers, `r_offset` holds a SYMH.) Phase 8 (banked blocks): symbol fields (`smbltyp`, `locmsk`, `s_ofst`, `reftbl`) are still read directly through pointers in `labels.c`, `directives.c`, `registers.c`, `file_io.c`, and `ref_xtrnal.r_offset` stores symbol pointers cast to `int`. Phase 1 isolated *finding and storing*; banking also needs those pointers to stay valid, so decide in phase 8 whether entries stay in main memory and only names move to banks.
- [ ] Variable-length symbols on the 6809: the Linux build pads `struct symblstr` to a multiple of 4, so AddressSanitizer can't see an under-allocation of 1–3 bytes there. The 6809 build has no padding, so such a bug would corrupt memory only natively. Keep `sizeof (struct symblstr) + strlen (name)` and don't trim it.
- [ ] Phase 2/8: `LOC1BYT` in `locmsk` doubles as the tree's "real right child" flag and is printed in the listing's symbol table. A different storage layout will change that column; it isn't in lwasm's output, so harmless for lwdiff, but regress.py will flag it and the reference must be moved deliberately.
