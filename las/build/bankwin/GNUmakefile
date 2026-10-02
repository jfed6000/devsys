# Test build for phase 8: same sources as build/unix, with -DBANKWIN so
# bank.c emulates the 6809's single window onto the symbol blocks (a
# block is copied in and out on every switch). A pointer into the table
# kept across a switch then reads the wrong block, as on the F256, and
# regress.py --current build/bankwin/las shows it.

include ../unix/GNUmakefile

CFLAGS += -DBANKWIN
