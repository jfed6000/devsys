# Fixes for NitrOS9 main

Problems in NitrOS9 `main` found while building devsys. `~/projects/wild/main/nitros9`
is kept a pristine copy of upstream `main`: don't change it. Each fix goes on its own
branch and is submitted upstream as a pull request, when the owner decides to. Until
then devsys lives with them.

| # | Found | Where | Problem | Fix | Status |
|---|---|---|---|---|---|
| 1 | 2026-10-12, K2 card 4 (D47) | `lib/alib/fgets.as` `FGETS_NOCR` | On an error the routine's `puls d,x,pc` puts the caller's old B back over the error code from `I$ReadLn`; carry stays set. `grep` then misses `E$EOF` and exits with a leftover byte (`ERROR #115` after a match, 110 without, in the shell). Affects `/dd/CMDS/grep` too | On the error path `stb 1,s` (B into the saved D) before `puls d,x,pc`; tried 2026-10-12: grep exits 0, a missing file still gives 216, MAME rehearsal clean, lwdiff on main unchanged otherwise (grep 383 → 387 bytes) | open, no PR |
| 2 | 2026-10-12, boot disk rebuild | `rules.mak` (library rule, line 124) | A library is built with `lwar -c $@ $?`: after one member's source changes, the library is re-created holding only that member (`libalib.a` 16,289 → 173 bytes) and the next link fails (`PUTS` not found). Programs aren't relinked when a library they use changes | Build the library from all its members (`$^`), and make each linked program depend on its libraries | open, no PR. devsys workaround: delete `boot/wildbits/l2/.lib/libNAME.a` and the `.mods/` programs using it, then `make` |
| 3 | 2026-10-12, K2 card 4 | `level1/cmds/grep.as` | `-n` counts only lines at least as long as the pattern (`subb <pattlen` / `bmi ReadIn` come before the count), so line numbers are low (README line 52 counted as 44). On the 6809 (K2 and MAME) the printed number is stranger still (`0040::`, `0040=:`), see DESIGN §10 | Count each line before the length test | open, no PR; numbering on the real CPU not yet understood |
| 4 | 2026-10-12, boot disks after pulling `9062f685` | `rules.mak` / recipe makefiles | A module or library member depends only on its own source, not on the defs it includes (`use defsfile`, `wildbits.d`): after `defs/wildbits.d` changed, `make` rebuilt nothing that uses it (`libwildbitsl2.a` stayed 58,080 bytes; built clean it is 60,858) | Generate include dependencies (lwasm `--dependnoerr`/`--depend` into `.d` files) or make objects depend on `defs/*` | open, no PR. devsys workaround: after pulling `main`, `rm -rf .mods .obj .lib` in `boot/wildbits/l2` before `make` |
