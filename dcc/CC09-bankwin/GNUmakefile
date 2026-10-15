# dcc68 with the 6809's single window emulated (bank.c BANKWIN: a block
# is copied in and out on every switch, so a pointer kept across a switch
# reads the wrong block) and the switch count printed at exit (SYMSTATS).
# Test build for symdb.c (step 20). vpath, not VPATH: VPATH would also
# find ../CC09's objects and binary and take them as up to date.
vpath %.c ../CC09
vpath %.h ../CC09
include ../CC09/GNUmakefile
CFLAGS += -DBANKWIN -DSYMSTATS -I../CC09 -UMSGFILE -DMSGFILE=\"$(abspath ../CC09)/dcc68.msg\"
DEPS := $(addprefix ../CC09/,cj.h msgs.h bank.h)
