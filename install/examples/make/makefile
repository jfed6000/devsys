* A C program with one function in C (util.c) and one in RMA-style
* assembly (dbl.a). make's rules in /dd/sys/make.default build the .r
* files; only the link is spelled out. "make" builds it, "mkdemo" runs it.
* dcc -f=mkdemo puts the program in the execution directory (/dd/cmds).
OBJS = main.r util.r dbl.r

mkdemo: $(OBJS)
    $(LINK) $(OBJS) -f=mkdemo
