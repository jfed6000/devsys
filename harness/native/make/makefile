* native.py's make suite (D45): the .r files come from make.default's
* .c.r and .a.r rules; only the link is spelled out
OBJS = main.r util.r dbl.r

prog: $(OBJS)
    $(LINK) $(OBJS) -f=prog
