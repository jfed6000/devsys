CFLAGS =  -dCOCO
LFLAGS = -l=/opt/c3/lib/clib.l -l=/opt/c3/lib/sys.l
CC = c3

RELS = rlink_main.r rl_02.r rl_03.r rl_04.r rl_05.r

%.r: %.c
	$(CC) $< $(CFLAGS) -r

llink: $(RELS)
	rlink /dd/lib/cstart_rma.r $(RELS) $(LFLAGS) -o=t_llink -n=llink -M=10 -E10 -m -s >symbols

$(RELS):
