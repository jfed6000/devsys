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
| 11. Real hardware | ✅ Ch 11 "Taking it to the real machine" (the card loop, MAME rehearsal, procedure files, #218) and "One program, several processes" (overlays vs processes, reentrancy, F$Fork vs Unix fork, shared blocks through F$MapBlk, the two plans). Still to add: the K2's real results and timings |
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
