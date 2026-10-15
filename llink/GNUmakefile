CFLAGS	= -g -m32 -Wno-implicit-int
LFLAGS	= -m32
#CC = c3

OBJS	= \
	rlink_main.o \
	globals.o \
	rl_02.o \
	rl_03.o \
	misc.o \
	rl_05.o \
	util.o \
	lwsel.o \
	bigbuf.o

llink: $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS)

# Every object depends on the headers: a struct size change (e.g. SYMLEN)
# must rebuild everything, or the objects disagree about layouts.
$(OBJS): rlink.h rof.h module.h types.h proto.h

clean:
	-rm llink $(OBJS)
