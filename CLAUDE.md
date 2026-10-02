# devsys — native NitrOS9 development system for the F256

Goal: build programs on the Foenix F256 itself (6809, NitrOS9 **Level 2
only**) from the same sources lwasm assembles on Linux. Plan: fork the C
version of RMA as the single assembler (named `las`, in `las/`), use DCC as the C compiler, and use ROF
+ rlink as the only native object format.

## Repository

`devsys` is one git repo (branch `main`, no remote) holding `las/`, `docs/`,
and later our forks of rlink and DCC. No upstream remotes: this is a clean
break, nothing is contributed back. The user will set up the project's own
remote later; don't add one or push.

## Key files

| File | What it is |
|---|---|
| `las/` | The assembler: our fork of RMA (from upstream commit `e82adeb`, no upstream history kept). Build: `cd las/build/unix && make`. |
| `harness/lwdiff.py` | Compares `las` with lwasm over the real Level 2 build. Run `python3 harness/lwdiff.py` after every change to `las`; `--only NAME --verbose` for one file. Report in `harness/reports/`. |
| `corpus/` | NitrOS9 source the harness assembles: clones of the user's forks (`jfed6000/nitros9`, `jfed6000/nitros9-languages`, branch `devsys`), pinned by `harness/corpus.lock`. Not version-controlled. Set up or restore with `harness/corpus.sh`; `harness/corpus.sh update` moves to the newest `devsys` commits. Don't edit files in it. |
| `harness/regress.py` | Checks that a change to `las` did **not** change behaviour: compares the working tree with `HEAD` (or `--ref REV`) on ~740 inputs. Must pass for refactors (phases 1, 2, 8, 9). |
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
- Host builds: `las` at `las/build/unix/las`; the unmodified RMA at
  `~/projects/dcc/rma/build/unix/rma` is the reference for "no behaviour change" checks; DCC passes at
  `~/projects/dcc/CoCoC/Source/Compiler/{CPP/dcpp,CC09/dcc68,COpt/dco68}`.
