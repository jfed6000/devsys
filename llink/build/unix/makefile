#CFLAGS = -I/dd/defs
#LFLAGS = -l=/dd/lib/clib.l
#CC = co_cc3

vpath %c ../..
vpath %h ../..

PROG = co_rlink
CFLAGS = -g
RELS = rlink_main.o rl_02.o rl_03.o rl_04.o rl_05.o

rlink: $(RELS)
	$(CC) $(LFLAGS) $(RELS) -o $(PROG)
#	rlink /dd/lib/cstart_rma.r $(RELS) $(LFLAGS) -o=t_rlink -n=rlink

$(RELS):

clean:
	rm $(PROG) *.o
