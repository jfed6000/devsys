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
5. **Every new term goes in the glossary** (chapter 11), alphabetically.
6. **Keep "Where this leads" (chapter 10) in step with `DESIGN.md`.** It is a
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
| 3. Module mode | How the module header parity byte and the 24-bit CRC are computed |
| 4. lwasm syntax | Local label scoping, conditional assembly, operator precedence, case folding |
| 5. Object mode | How `section`/`export` map onto ROF; linker defaults for the module header |
| 6. Long names | Why name limits existed; how ROF stores names |
| 7. Native build | Bootstrapping: building tools with themselves; cross vs native compilers |
| 8. Banked blocks | `F$AllRAM`/`F$MapBlk` in detail; (block, offset) pointers; hashing to blocks |
| 9. Precompiled defs | Saving and reloading a symbol table image; cache invalidation |
| 10. Libraries | Library members, link order, how rlink searches `.l` files |
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
