# Measurement build: same sources as build/unix, with -DSYMSTATS so las
# prints symbol table memory use to stderr (see sym_stats in symtab.c).
# Not for normal use: the extra stderr output would show up in regress.py.

include ../unix/GNUmakefile

CFLAGS += -DSYMSTATS
