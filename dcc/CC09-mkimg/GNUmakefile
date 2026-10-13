# dcc68 built to write header images (pch.c, -DMKIMG): "dcc68 -w=IMAGE
# headers.i" (step 17). Same sources and messages as ../CC09.
VPATH = ../CC09
include ../CC09/GNUmakefile
CFLAGS += -DMKIMG -I../CC09 -UMSGFILE -DMSGFILE=\"$(abspath ../CC09)/dcc68.msg\"
DEPS := $(addprefix ../CC09/,cj.h msgs.h)
