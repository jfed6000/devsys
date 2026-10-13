# dcc68 built to write header images (pch.c, -DMKIMG): "dcc68w -w=IMAGE
# headers.i" (step 17). Same sources and messages as ../CC09. vpath, not
# VPATH: VPATH would also find ../CC09's objects and binary (built without
# MKIMG) and take them as up to date.
vpath %.c ../CC09
vpath %.h ../CC09
include ../CC09/GNUmakefile
BINARY	:= dcc68w
.DEFAULT_GOAL := dcc68w
CFLAGS += -DMKIMG -I../CC09 -UMSGFILE -DMSGFILE=\"$(abspath ../CC09)/dcc68.msg\"
DEPS := $(addprefix ../CC09/,cj.h msgs.h)

$(BINARY): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
