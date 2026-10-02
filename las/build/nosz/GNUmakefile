# Reference build for the 6809 las (phase 7): same sources as build/unix,
# with -DNOSIZING so lwasm-mode ",pcr" sizing (sizing.c) is left out, as it
# is on OS-9, which has no fork(). Native output must match this build's.

include ../unix/GNUmakefile

CFLAGS += -DNOSIZING
