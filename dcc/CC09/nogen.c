/*
    nogen.c - the code generator, as nothing (devsys, step 17)

    dcc68w (dcc68 built with -DMKIMG) only parses: it reads a file of
    #includes and writes the declarations to a header image (pch.c). A
    file of declarations generates no code and has no statements or
    initialisers, so neither the code generator's files (codgen, tranexp,
    bool, regscont, longs, floats, scale) nor stats, inits and local are
    linked; these stand in for what the parser calls. Anything that would
    be a definition stops dcc68w: an image can't hold one. Without them
    dcc68w fits four blocks and has 32K for a whole header set's symbols
    (las's needs more than 24K).
*/

#include "cj.h"

extern int errno;

nodef()
{
    fatal(M_IMAGE_DEFINITION);
}

statement() { nodef(); }
initialise() { nodef(); }
defglob() { nodef(); }
extstat() { nodef(); }
locstat() { nodef(); }
startfunc() { nodef(); }
initerr() { nodef(); }
endfunc() {}
epilogue() {}
comment() {}

tidy()
{
    int err = errno ? errno : 1;

    if (strfile) {
        fclose(strfile);
        unlink(strname);
    }
    _exit(err);
}

direct int datflag;
direct int datstring;           /* inits.c: never set without initialisers */
direct char *dregcont, *xregcont;

os() {}
ol() {}
ot() {}
nl() {}
od() {}
olbl() {}
nlabel() {}
defcon() {}
deref() {}
gen() {}
clrconts() {}
ldxexp() {}
lddexp() {}
lload() {}
dload() {}
modstk() {}
xmodstk() {}
uselabel() {}
setlabel() {}
label() {}

expnode *
tranexp(t)
expnode *t;
{
    return t;
}

expnode *
tranbool()
{
    return 0;
}

expnode *
treecopy(t)
expnode *t;
{
    return 0;
}
