# Maintaining the toolchain primer

`toolchain.html` teaches the reader (the project owner) how the toolchain
works: compilers, assemblers, linkers, object formats, memory, symbol tables.
The reader knows the F256 and NitrOS9 well but is new to toolchain internals.
The page should grow with the project, so the reader always understands the
work being done.

## When to update it

Update the page in the same session as any of these:

- A new concept comes up while working (a new directive, a file format detail,
  an algorithm, an OS call). If you had to explain it to the reader in chat,
  it belongs in the page.
- A decision in `../DESIGN.md` changes something the page explains.
- A new measurement replaces one quoted in the page (symbol counts, sizes,
  lookup costs).
- A roadmap phase finishes. Each phase usually teaches something; see the
  backlog below.

Then add a line to the changelog at the bottom of this file.

## Content rules

1. **Real examples only.** Every listing, hex dump and measurement must come
   from an actual run of the tools. Never type bytes by hand or invent output.
   Put example sources in `examples/`, add the commands to
   `tools/build_examples.sh`, and run it.
2. **Verify hex dumps.** After editing any dump, run
   `tools/build_examples.sh` then `python3 tools/check_hex.py`. Both must
   pass. A new dump needs an entry in `HEX_DUMPS` (and `IGNORE` for any
   timestamp bytes).
   Chapter 9's hashing figures (bucket sizes, comparison counts, the bucket
   25 tree) come from `tools/hash_data.py`; rerun it if the defs or
   `las/symtab.c` change, and update the page to match.
3. **Concept first, then the example.** Say what the thing is and why it
   exists in plain words, then show it in the reader's own files.
4. **Cite real paths** (`level2/wildbits/cmds/gfxstatus.as`,
   `rma/labels.c:621`) so the reader can go and look.
5. **Every new term goes in the glossary** (chapter 13), alphabetically.
6. **Keep "Where this leads" (chapter 12) in step with `DESIGN.md`.** It is a
   one-paragraph summary, not a second roadmap.
7. **Update the footer** with the date and tool versions of the latest runs.

## Writing style

- Short, direct sentences. Active voice. One idea per paragraph.
- Explain why, not just what. The reader asked to understand the reasons.
- Define a term the first time it appears, or link to the glossary.
- No filler: no "it's worth noting", no em-dash asides, no "not X but Y".
- Use "you" for the reader and "we" for the project.

## Page mechanics

- **Chapters** are `<section class="ch" id="...">` with an
  `<h2><span class="num">CHAPTER NN</span>Title</h2>`, plus a matching
  `<li><a href="#id">` in `nav.rail`. Chapters are a learning sequence: put a
  new chapter where it fits in that sequence and renumber the ones after it,
  in both the heading and the rail.
- **Colour meanings are fixed** across the page. Format colours: module =
  amber (`--mod`), lwasm object = teal (`--lwo`), RMA object = violet
  (`--rma`). Hex dump classes: `c-hdr` header, `c-name` names, `c-sym`
  symbols/data, `c-code` code, `c-ref` references/fixups, `c-crc` checksum.
  Reuse these; add a new token only for a genuinely new kind of thing, and
  define it in all three theme blocks.
- **Hex dumps**: add a `var xxxSegs = [["HEX", "class", "label", "meaning"], ...]`
  array in the page script, a `<div class="hex" id="hex-xxx">` and
  `<ul class="legend" id="legend-xxx">` in the chapter, and a
  `render(xxxSegs, "hex-xxx", "legend-xxx")` call.
- **Tables** go inside `<div class="scroll">` so they scroll on phones.
- Every colour comes from a token in the `:root` block; light and dark
  themes must both work. Check the page at phone width (about 400px).
- The page is standalone (opens from disk). Only external resource: Google
  Fonts, with fallback stacks.

## Backlog: topics to add as the work reaches them

| Roadmap phase | Topic for the page |
|---|---|
| 0. Harness | ✅ Chapter 10: differential testing, dry runs, first vs knock-on errors |
| 1. Symbol interface | ✅ Ch 9 "Hiding the table behind an interface"; ch 10 "Testing a change that shouldn't change anything" |
| 2. Compact entries | ✅ Ch 9 "Making the record fit the name", "Checking for memory mistakes" |
| 3. Module mode | ✅ Ch 2 "Two location counters", "The header check byte", "The CRC"; ch 4 "Choosing instruction sizes" |
| 3b. Operand sizing | ✅ "Choosing instruction sizes" (lwasm's settle / worst case / force rule, gfxstatus example, the trial run, fork's shared file offset); ch 6 section-name layout order |
| 4. lwasm syntax | ✅ Ch 4 "Local labels", "Conditions across two passes", "Same symbols, different meaning", "Two dialects in one assembler" |
| 5. Object mode | ✅ Ch 6 "Two linkers, two module layouts", "Choosing library members", "Values only the linker knows"; ch 9 uninitialised memory |
| 6. Long names | ✅ Ch 7 "Long names, end to end" (each tool's limit, the collision example, ROF version and aliases for old libraries, testing for one kind of difference) |
| 7. Native build | ✅ Ch 11 "Running on the F256": cross vs native, bootstrapping, the measured 64K map and the F256's two module limits, K&R traps (member namespace, arguments without prototypes), the host model, testing in MAME |
| 8. Banked blocks | ✅ Ch 9 "Moving the table into banked blocks": handles instead of pointers, the 16-bit handle, the window and its memory map, the two-entries trap, the window-emulation test and its negative control, F$DelRAM at exit, results, "measure before you build" (rlink). Ch 11: the shell's 200-character line, cstart's comma splitting, --args |
| 9. Precompiled defs | ✅ Ch 9 "Saving the defs: precompiled images": what reading the defs changes besides symbols, replaying creations to keep tree shape, cache invalidation (the blunt exact rule, H6309 as the stale-image danger, pass 2's check via the setters), imgcheck and its variants, the 5-slot size limit and stack checks, the peephole optimizer bug (`puls x,u,pc` and a `long` in X) |
| 10. Libraries | ✅ Ch 11 "Building NitrOS9's programs on the F256" (the obj suite's four steps, `merge` as a librarian, `dtb` masking) and the bugs only the F256 showed: a `char` result in half a register (calling convention, `std -2,s` vs `tstb`), two files sharing one stdio buffer, the stack meeting the heap, a shortcut that outlived its pointer, measuring where memory goes (`sbrk` vs `malloc`). Ch 11 "A small model": when the model is wrong. Still open: how rlink searches `.l` files in detail |
| 11. Real hardware | ✅ Ch 11 "Taking it to the real machine" (the card loop, MAME rehearsal, procedure files, #218) and "One program, several processes" (overlays vs processes, reentrancy, F$Fork vs Unix fork, shared blocks through F$MapBlk, the two plans) and the K2's results: identical output, about 17% faster than MAME |
| 12. Room in las | ✅ Ch 11 "Measure first: where a C program's data goes" (static data, ibrk heap, stack, sbrk heap; CoCoC's guards; the os9run measurement and its surprises) and "One block of data, six of code" (D37: CondBits, SecNames, getmem on ibrk, las_MEM; three graceful limits). Glossary: ibrk / sbrk, stack, static data |
| any | Macros and `rept`; the 6809 calling conventions used by DCC; how `cstart` sets up a C program |

## Changelog

- 2026-10-02 — Created: chapters 1–11, module and ROF hex dumps, build and
  check tools.
- 2026-10-02 — Added chapter 10, Testing against lwasm (phase 0). Renumbered
  Where this leads → 11, Glossary → 12. Glossary: CRLF, differential testing,
  dry run, knock-on error, reference.
- 2026-10-02 — Chapter 10: added "Holding the test still" (pinned corpus).
- 2026-10-02 — Phase 1: ch 9 interface section; ch 10 regression testing and
  negative controls. Glossary: interface, negative control, refactor,
  regression test.
- 2026-10-02 — Ch 4: "Files bigger than memory" (what streams, what stays in memory).
- 2026-10-02 — Ch 9: replaced "Finding a name quickly" with a full hashing
  section: hash calculator, bucket-size chart, real bucket 25 tree with a
  search path, the algorithm and `symtab.c` code, threaded trees. Data from
  `tools/hash_data.py`. Glossary: binary search tree, collision, hash
  function, threaded tree (and sorted the glossary).
- 2026-10-02 — Ch 9: "What one symbol looks like": struct symblstr, decoded
  smbltyp/locmsk/s_ofst/reftbl from `examples/kinds.a` (built by
  build_examples.sh). Corrected entry size 73 → 76 bytes (179K for fm.asm).
- 2026-10-02 — Phase 2: ch 9 sections on variable-length records (measured
  results from harness/symstats.py) and AddressSanitizer/padding. Glossary:
  AddressSanitizer, padding.
- 2026-10-02 — Phase 3: ch 2 sections on location counters, the header check
  byte (worked XOR table for hello) and the CRC (incl. the 80 0F E3 residue);
  ch 4 "Choosing instruction sizes". Glossary: check byte, location counter,
  XOR; CRC entry expanded.
- 2026-10-02 — Phase 4: four ch 4 sections (local label scopes, the include
  guard and condition replay, precedence differences, the two dialects).
  Glossary: dialect, include guard, local label, operator precedence.
- 2026-10-02 — Phase 5: three ch 6 sections (rlink vs lwlink layouts, library
  member choice, link-time expressions and --defs) and a ch 9 paragraph on
  rlink's uninitialised-memory bug. Glossary: mainline, uninitialised memory.
- 2026-10-02 — Ch 6 "Values only the linker knows": worked examples (F$Exit,
  '0*256+C$SPAC, (Prgrm+Objct)*256 in lwobjdump vs rofdump form), the __os9
  header case (inetd), and play's 8-bit truncation.
- 2026-10-02 — Phase 3b: "Choosing instruction sizes" rewritten with lwasm's
  actual rule and a real example; how las's trial run works; fork() shares file
  offsets. Ch 6: lwlink orders by section name before command line. Glossary:
  trial run, unsettled.
- 2026-10-02 — Phase 6: ch 7 "Long names, end to end". Glossary: alias,
  ROF version.
- 2026-10-02 — Ch 7 "How long can a name be, and why": every name limit in
  the chain and its reason, C89's 31/6 rule (and C99's 63/31), measurements
  from CoCoC and the defs, what a limit costs.
- 2026-10-02 — Phase 7: new chapter 11 "Running on the F256" (cross and
  native, bootstrapping, size table, measured las memory map, the $FD00 and
  F$Fork limits, K&R member namespace and prototype-less arguments, the
  os9run model, MAME results). Renumbered Where this leads → 12, Glossary →
  13; ch 12 mentions phase 8's scope. Glossary: bootstrapping, cross
  compiler, native, prototype.
- 2026-10-02 — Phase 8: ch 9 "Moving the table into banked blocks"; the
  three-part fix table no longer promises one block per bucket; ch 11 notes
  the shell's line limit, cstart's commas and --args; ch 12 updated.
  Glossary: handle, window.
- 2026-10-02 — Phase 10 and the end of phase 8: ch 11 sharding paragraph,
  "Building NitrOS9's programs on the F256" with five bug stories (char
  result in B, shared stdio buffer, stack meets heap, fwrite of a handle,
  measuring memory), "A small model" gains the model-was-wrong case, size
  note under the table; ch 12 updated. New example `examples/charcond.c`
  (built with CoCoC's and devsys's dcc68). Glossary: buffer, calling
  convention, heap, sharding, sign extension.
- 2026-10-02 — Phase 9: ch 9 "Saving the defs: precompiled images"; updated the three-part-fix table and ch 12. Glossary: cache invalidation, defs image, peephole optimizer.
- 2026-10-02 — Ch 11: "Taking it to the real machine" and "One program, several processes"; ch 12 next steps. Glossary: overlay, procedure file, reentrant.
- 2026-10-05 — Step 12: ch 11 "Measure first: where a C program's data goes" and "One block of data, six of code" (two memory maps from the llwbsd measurement); ch 12 next steps. Glossary: ibrk / sbrk, stack, static data.
- 2026-10-05 — Step 11: ch 11 "Taking it to the real machine" gets the K2's results and timings; ch 12 updated.
- 2026-10-05 — Ch 11: the second K2 card (one-block las, `mfree`/`mmap`): 1,656K free, all blocks given back.
- 2026-10-05 — Step 13: ch 11 "Sizing on the F256: a child that starts from the top" (F$Fork vs fork(), the shared block, the model bug, timings); ch 12 updated. Glossary: pipe, shared memory.
- 2026-10-05 — Ch 11: why the sizing answers use a shared block even where pipes exist (PipeMan's end-of-file and blocking edges; las and rlink can't read from a pipe).
- 2026-10-05 — Ch 11: the third K2 card (native sizing on the hardware: `copy` 1,469 bytes; 255 s / 120 s).
- 2026-10-07 — Ch 7: "How the driver knows a pass failed" (Linux `dcc` lost every pass's exit status through `exit(system())`; `-w`'s `-n` from `cc`'s one-pass `c.comp`; /nil on the devsys disks). Glossary: driver, exit status.
- 2026-10-07 — Appendix A "What we changed in DCC": every source change since the fork (pass, file, what, why, D number), the build-only options, and how dccdiff proves nothing else changed.
- 2026-10-07 — Ch 3: "How `las` tells them apart" (mode from the first `psect`/`section`/`mod`, D40; why a prescan, why lwasm when none); ch 4 and glossary "dialect" updated.
- 2026-10-07 — Ch 4: "Structures: names for offsets" (minted's `undo_item`, why fields get a prefix, nesting and instances, D41); ch 11 and 12: all 200 module sources. Glossary: struct.
- 2026-10-08 — Ch 6: "One linker, two layouts, and a new name" (rlink fork renamed `llink`; layout from the mainline's `__lwsec`, D42); ch 1 note on the devsys names; ch 3 table, ch 12, Appendix A row, glossary (llink; alias, mainline, ROF updated).
- 2026-10-09 — Step 14: ch 9 "Fixed in step 14, at no cost" (dco68's `puls x,u,pc`: wider than thought, `puls d` + a merge rule, same size; why string literals cost bytes in DCC); ch 7 the errexit/dcpp follow-up to "How the driver knows a pass failed".
- 2026-10-11 — Ch 11: "Where the disk comes from" (devsys builds its boot disks from NitrOS9 `main`, D44; make includes are relative to the running directory, hence the stand-in files); ch 7 note updated.
- 2026-10-11 — Step 15: ch 11 "A make for the F256" (what make does, default rules in a text file and why CoCoC's over Microware's, dates to the minute, copy keeps dates). Glossary: make.
- 2026-10-12 — Ch 4: "When a size comes later: settling" (main's `rmb G.TermMax*8` before `G.TermMax equ 9`; the settling pass, D46; the zero register in `tfr 0,dp`; the sizing trial that ran out of blocks silently); the stale "except struct" in "Choosing instruction sizes" removed; ch 12 updated. Glossary: settling pass, zero register.
- 2026-10-12 — Step 16: ch 11 "The install disk: everything a build needs, in one place" (the /dd layout, why NitrOS9's defs get a subdirectory, options files over a --nitros9 flag, helpmsg, data vs execution directory, the selftest, grep's BCD line numbers in MAME); ch 12 and footer updated. Glossary: execution directory, options file.
- 2026-10-12 — Step 16 on the K2: ch 11 the fourth card's results; the K2 agrees with MAME on grep's numbering; ERROR #115 traced to alib's FGETS_NOCR restoring B over the error code (pshs/puls as a courtesy that breaks the error contract); ch 12 and footer.
- 2026-10-12 — ch 11: the FGETS_NOCR fix (stb 1,s into the saved B), tried and kept out of the pristine main clone (docs/nitros9-fixes.md); NitrOS9's `lwar -c $@ $?` library rule (make's `$?`) re-creating a library from only the changed member.
- 2026-10-12 — ch 11: make's dependency rule and why NitrOS9's makefiles miss a changed defs file (fixes list #4); clean rebuilds after a pull.
- 2026-10-12 — ch 10: "A new source in main: one directive, three names" (zmb/bsz/rzb, D48; zero bytes in a module vs reserved space in bss).
- 2026-10-13 — ch 07: "Inside dcc68: one pass, one statement at a time" (its four stages with 6809 code sizes from the map, what grab() keeps and for how long, why header names fill its 16K). Glossary: expression tree, free list, one-pass compiler.
- 2026-10-13 — ch 07: "Pointers or a database" (handles instead of raw pointers; the 205 field accesses found by renaming fields and compiling; the pointer shortcuts; symnode as a property list, elem/dimnode as cons cells). Glossary: cons cell, property list.
- 2026-10-13 — ch 07: "The prototype: right answers, too big" (symdb branch: identical output on 313 inputs, single-window emulation, switch counts, 3,297 bytes over the module limit and why: a 12-byte call per field access).
- 2026-10-13 — ch 07: "What big compilers do about headers" (precompiled headers, Clang's lazy loading on lookup, the link to las's defs image and dprune; macros vs declarations). Glossary: precompiled header.
- 2026-10-13 — ch 07: "Measured: an image loader in dcc68" (pch branch: filter + on-disk hash chains, struct lists loaded once, 55/55 identical, +1,230 bytes vs banking's 3,314, file_io 14,080 of 16,384 on the model).
- 2026-10-13 — ch 07: "Messages in a file" (dcc68's 92 messages as 2-byte codes read from /dd/SYS/dcc68.msg, as OS-9's errmsg; open/read so "out of memory" still prints; dcc68 48,341 bytes with the image loader).
- 2026-10-13 — ch 07: "When is an image the right one?" (dcpp -h checks each header's checksum, exit 2 and dcc falls back; why the check must be exact: guards move size_t, an #ifdef in a struct; the precompiled-header prefix model; dcc68w, a 4-block parser-only writer with 32K of data). Glossary: checksum.
- 2026-10-13 — ch 07: "Fitting las to its image" (rma.h redefines void/const for later headers, so the project header comes first; the MAIN file and the rmabase.h split; headers not in an image pass as text; las built natively in MAME with images, 18/18); ch 12: self-hosting and header images.
- 2026-10-13 — ch 11: "The F256 builds its own tools" (the self suite, 80 objects and 8 modules identical; llink's prefix header and globals.c; dco68's window and tail merging with the dcc.c example; the whole file in banked blocks, the switch-count table, list heads in ordinary memory, four LRU windows, bank_ptr in assembly after cclsr took 37%; the $E000 top-slot hang and F$FreeHB); glossary: least recently used, prefix header, tail merging, window extended; ch 12 summary; Appendix A: D50's dcc68/dcpp/dcc changes and D51's dco68 changes
- 2026-10-13 — ch 07: "Building the C library ourselves" (clib/ from KLibc and SysLib, library order, libdiff --names8: 126 of 134 identical and why the rest differ; _ludiv didn't link, devtyp/sleep exported os9defs' labels, sintersect, Kreider's cstart fixes with Microware's 896 stack) and "Floating-point constants: the same bytes on both machines" (the 6809 double format with consts.c, IEEE vs the 6809's arithmetic, HUGE_VAL = 0, fp6809.c, fltcheck.py, native 0.0 / 4294967295.0 / 65535 bugs, REGCONTS ignores size); "Old libraries, new names" updated; ch 12 summary; Appendix A: D52's dcc68 changes; glossary: BSS, exponent bias, hidden bit, mantissa, register tracking, start-up code. Example consts.c added to build_examples.sh.
- 2026-10-13 — ch 07: "Remembering what a register holds" (REGCONTS notes, the union example regreuse.c from CoCoC's and devsys's dcc68, why only the top of the tree is compared and subtree stays blind, the 346-file scan, regwidth.c, room from lexinit's table); Appendix A two rows; glossary: big-endian, register tracking updated.
- 2026-10-13 — Step 19, first half: ch 11 "Where the time goes: measure, then read the defs once" (os9run --mark/--profile on attr.asm; three equal passes because lwasm mode reads the defs in every pass; the in-memory record, DIRLOC and os9.d's guard; 1,647 → 1,352 s, D54). Glossary: profile.
- 2026-10-13 — Step 19, second half: ch 11 "The trial without a child" (why the child existed; pass 1 as the trial and settling instead; DIRLOC across passes and the latent settling fault; the listing title; what was deleted; 914 s / 610 s, D55); ch 12 summary.
- 2026-10-14 — Step 19: ch 11 "A shift by twelve" (cclsr, DCC's one-bit-a-time shift loop, 18% of each pass from bank_ptr's h >> 12; the high byte and four lsra; model 27.1M → 20.9M, MAME 914 → 791 s / 610 → 535 s, and why MAME gains less than the model, D56); ch 12 summary; footer.
- 2026-10-15 — ch 11 "A shift by twelve": the fifth K2 card (8 commands 95 s / 63 s, from 220 / 102; ERROR #115 is main's grep); footer.
- 2026-10-15 — Step 20: ch 07 "Step 20: te, and the database after all" (te's 325 symbols at line 555, why long names weren't the cause, the symdb design back on handles, per-field entry points and the direct-page handle with the fcb $8C skip chain, SYMP's pointer rule, header images dropped and why, dco68's handles in 8-byte units, symcheck, te built in MAME in 81 s identical, disk backing kept open); ch 12 summary; footer.
- 2026-10-15 — ch 07 "Step 20: te, and the database after all": te built on the K2 in 83 s, identical; why Linux is about 2,000 times faster (0.04 s).
- 2026-10-15 — Step 21: ch 07 "Step 21: dev-cc, and a bug that hid in a pattern file" (dco68's records sized to their contents: mnemonics interned, operand and label texts at their length, the operand overwrite; llink's psect in blocks; the ldd / bhi stale-carry bug in CoCoC's cmp #0 rule, why bhi tests C, what else it hit, unsgn.c); ch 12 summary.
- 2026-10-15 — Step 21 on the K2: ch 07 dev-cc section ends with card 7 (dev identical, about 2 minutes).
- 2026-10-15 — Step 22: ch 07 "Step 22: room to change" (dco68 padded into three blocks and why the top slot sets the limit; llink drops an object nothing references; the 64K as eight slots, three windows; a library module linked for one call: rewind/fseek; the cstr leak and the model's missing I$DeletX; las's data as one budget); ch 12 summary.
- 2026-10-15 — Step 23: ch 07 "Step 23: your other programs, and an error las never gave" (native.py progs, 12 of jfed's programs identical; why an undefined name is an external reference in RMA's object files and why module output lost it; why lwdiff never caught it; silent check 8); ch 12 summary.
- 2026-10-15 — Step 24: ch 07 "Step 24: a faster dco68, and searches that grew with the square" (quadratic walks: returns, reference cells, long-branch walk; repeated pattern reads, first-character rejection, mnemonics by address; the label hash; why the output can't change; fnmatch's `[`); ch 12 summary.
- 2026-10-15 — Step 25: ch 07 "Step 25: a faster las, and a table read from the top" (linear search; prefix matching makes order part of the meaning, `ldd` before `ld` or it becomes `ldw`; stable sort by first letter; reading from stdio's buffer with one test; mapping a block once; why the buckets stay); ch 12 summary.
- 2026-10-15 — Step 26: ch 07 "Step 26: dco68 again, and the cost of a walk" (counting window changes by caller; a bucket walk maps a block per label; a hash that ignores digits' high bits; a block address as handle × 8 + a bias computed at mapping, worked example; necessary conditions and runs in the pattern loop; reading and writing stdio's buffer directly; dco68 -2 rewriting `,y` in our own assembly; the hang on LF input); ch 12 summary.
- 2026-10-15 — Step 27: ch 07 "Step 27: las again, and a tree that became a list" (sorted insertion turns a binary tree into a chain; why the shape can't change; comparing two characters as one number; long arithmetic as library calls and a constant that made it worse; per-character loops in assembly; why the window changes stay; llink out of slots: K&R malloc's headers and its 1K steps, records cut from chunks); ch 12 summary.
- 2026-10-15 — Step 28: ch 07 "Step 28: dcc68, and a faster putc for everyone" (putc's eight tests and one combined test; fixing it in the library for every program; multiplying a multi-byte number by 10 with mul, byte by byte with a carry; the reading and copying loops; why the window changes stay); ch 12 summary.
- 2026-10-16 — ch 07 "One C library, not two: why a disassembly isn't a source" (NitrOS9's lib/kreiderclib is Os9disasm output: sleep as `equ *-2`, lost strings, cldiv's sign bug run on os9run; D68); glossary: disassembler.
