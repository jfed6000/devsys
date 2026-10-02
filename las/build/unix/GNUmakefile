# make_rma : makefile to compile the new 6309-compatible "rma"

vpath %.c ../..
vpath %.h ../..

RDIR = RELS
ODIR = .
OBJS = rma_main.o findcmd.o part3.o registers.o directives.o labels.o \
   numeric.o file_io.o macros.o symtab.o bank.o module.o lwobj.o preload.o sizing.o

PROTOS = rma_main.c findcmd.c part3.c registers.c directives.c labels.c \
   numeric.c file_io.c macros.c symtab.c bank.c module.c lwobj.c preload.c sizing.c

CFLAGS = -m32 -g -Wall
LFLAGS = -m32

#LFLAGS = -l=/dd/lib/clib.l -l=/dd/lib/sys.l -M=11 -E=13
#CC	= gcc-3.3

# This file will be named "r63", following the 68xxx - type modules

all: $(OBJS) las

rma_main.o: rma.h rtables.h

las: $(OBJS) rma.h
	$(CC) $(LFLAGS) $(OBJS) -o las

proto.h: $(PROTOS)
	echo >proto.h
	cproto $(PROTOS) -o proto.z
	mv proto.z proto.h

clean:
	rm -f las *.o
