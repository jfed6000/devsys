# As CC09-bankwin, with 2-byte fields as on the 6809 (FIELD16), so the
# blocks fill as they would there. Test build for symdb.c (step 20).
vpath %.c ../CC09
vpath %.h ../CC09
include ../CC09/GNUmakefile
CFLAGS += -DBANKWIN -DSYMSTATS -DFIELD16 -I../CC09 -UMSGFILE -DMSGFILE=\"$(abspath ../CC09)/dcc68.msg\"
DEPS := $(addprefix ../CC09/,cj.h msgs.h bank.h)
