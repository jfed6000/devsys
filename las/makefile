# make_rma : makefile to compile the new 6309-compatible "rma"

RDIR = RELS
ODIR = test
OBJS = rma_main.r findcmd.r part3.r registers.r directives.r labels.r \
   numeric.r file_io.r macros.r symtab.r

PROTOS = rma_main.c findcmd.c part3.c registers.c directives.c labels.c \
   numeric.c file_io.c macros.c symtab.c

CFLAGS = -v=/dd/defs -dCOCO
#CFLAGS = -dCOCO
LFLAGS = -l=/dd/lib/c3/clib.l -l=/dd/lib/c3/sys.l -o=$(ODIR)/las -n=las
CC = c3

%.r: %.c
#	$(CC) $(CFLAGS) $@ -r

all: $(OBJS) $(ODIR)/las

.PHONY: all

rma_main.r findcmd.r part3.r registers.r directives.r labels.r file_io.r macros.r symtab.r:
	$(CC) $(CFLAGS) $*.c -r -s

numeric.r:
	$(CC) $(CFLAGS) numeric.c -r

#LFLAGS = -l=/dd/lib/clib.l -l=/dd/lib/sys.l -M=11 -E=13
#CC	= gcc-3.3

rma_main.r: rma.h rtables.h

# This file will be named "r63", following the 68xxx - type modules

$(ODIR)/las: $(OBJS) rma.h
	rlink /dd/lib/c3/cstart_c3.r $(OBJS) $(LFLAGS) -m -s > $(ODIR)/symbols

proto.h: $(PROTOS)
	echo >proto.h
	cproto $(PROTOS) -o proto.z
	mv proto.z proto.h

clean:
	del r63 *.r
