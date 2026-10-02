#!/bin/sh
# Rebuild the real examples used in toolchain.html into docs/learn/examples/out/.
# Run from anywhere. Requires lwasm (coco-shelf), the host RMA build and the host DCC build.
set -e
unset LD_PRELOAD

HERE=$(cd "$(dirname "$0")/.." && pwd)
EX="$HERE/examples"
OUT="$EX/out"
NITROS9=${NITROS9:-$HOME/projects/wild/Sept2026/nitros9}
LWASM=${LWASM:-$HOME/projects/coco-shelf/bin/lwasm.orig}
RMA=${RMA:-$HOME/projects/dcc/rma/build/unix/rma}
DCC=${DCC:-$HOME/projects/dcc/CoCoC/Source/Compiler}

mkdir -p "$OUT"
cd "$OUT"

# Module format: hello.asm -> hello (same flags as nitros9/rules.mak, Level 2 wildbits defs)
"$LWASM" --6309 --format=os9 \
    --pragma=pcaspcr,nosymbolcase,condundefzero,undefextern,dollarnotlocal,noforwardrefmax \
    -I "$NITROS9/defs" -I "$NITROS9/level2/wildbits" -Dwildbits=1 \
    --list="$OUT/hello.lst" --output="$OUT/hello" "$EX/hello.asm"

# C pipeline: sum.c -> sum.i -> sum.a -> sum.opt.a -> sum.r
"$DCC/CPP/dcpp" "$EX/sum.c" > sum.i
"$DCC/CC09/dcc68" sum.i -o=sum.a
"$DCC/COpt/dco68" < sum.a > sum.opt.a
"$RMA" sum.opt.a -o=sum.r -l > sum.lst

# Symbol table kinds: kinds.a -> kinds.lst (listing with -s symbol table), used in chapter 9
LAS=${LAS:-$HOME/projects/devsys/las/build/unix/las}
"$LAS" "$EX/kinds.a" -l -s -o="$OUT/kinds.r" > kinds.lst

echo "Built into $OUT:"
ls -l "$OUT"
