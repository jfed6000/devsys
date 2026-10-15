/*
    symdb.c - dcc68's symbols and lists, kept by handle (devsys, step 20)

    Every symbol and every list cell lives in banked storage (bank.c)
    and is named by a handle, a 16-bit number, never by a pointer:

        sget(s,f) / sput(s,f,v)   read / write field f of symbol s (in
                                  the code: cj.h's SG_F(s) / SP_F(s,v))
        symname(s)                its name (a copy, good until the next call)
        lookup(name)              find a name, or make a new symbol for it
        symanon()                 a symbol in no table (the dummy)
        pushdown(s) / pullup(s)   hide an outer declaration / bring it back
        symfree(s)                drop a symbol at its block's end
        symdone()                 give all the storage back (every exit)

        cons(a,d)                 a new cell holding a, followed by d
        car(c) / cdr(c)           a cell's value / the next cell (cj.h:
        setcar(c,a) / setcdr(c,d) a cell's two fields are a symbol's first)

    A symbol is S_NFIELDS ints and then its name. Lists are cells of two
    ints: dimensions (car = the size) and struct members (car = the SYM).
    No pointer into storage survives a call here: another block may be
    mapped by then.
*/

#include "cj.h"
#include "bank.h"

#define FIELDS(s)   ((FIELD *) bank_ptr(s))

direct SYM freesym;             /* free symbols (short names only) */
direct SYM freedown;            /* free push-down records */

#ifndef _OS9
sget(s,f)
SYM s;
{
    return FIELDS(s)[f];
}

sput(s,f,v)
SYM s;
{
    FIELDS(s)[f] = v;
}
#else
direct SYM symh;                /* the handle for sgN/spN/symp (cj.h) */
direct int symv;                /* and the value for spN */

/* sgN(): field N of symbol symh; spN(): set it to symv; symp(): symh's
 * record (cj.h's SG_, SP_, SYMP). A field is 2 bytes, at offset 2N.
 * "fcb $8C" is cmpx #, which skips the next entry's ldb. No sg6, sp6
 * or sp7: nothing uses them (a use would fail to link) */
#asm
sg9: ldb #18
 fcb $8C
sg8: ldb #16
 fcb $8C
sg7: ldb #14
 fcb $8C
sg5: ldb #10
 fcb $8C
sg4: ldb #8
 fcb $8C
sg3: ldb #6
 fcb $8C
sg2: ldb #4
 fcb $8C
sg1: ldb #2
 fcb $8C
sg0: ldb #0
 bsr symf
 ldd ,x
 rts

sp9: ldb #18
 fcb $8C
sp8: ldb #16
 fcb $8C
sp5: ldb #10
 fcb $8C
sp4: ldb #8
 fcb $8C
sp3: ldb #6
 fcb $8C
sp2: ldb #4
 fcb $8C
sp1: ldb #2
 fcb $8C
sp0: ldb #0
 bsr symf
 ldd <symv
 std ,x
 rts

symp: clrb
 bsr symf
 tfr x,d
 rts

* X = the address of the field at offset B of symbol symh
symf pshs b
 ldd <symh
 pshs d
 lbsr bank_ptr
 leas 2,s
 tfr d,x
 puls b
 abx
 rts
#endasm
#endif

char *
symname(s)
SYM s;
{
    static char name[NAMESIZE+1];

    return strcpy(name,SYMP(s)->name);
}

SYM
lookup(name)
char name[];
{
    /* the symbol for 'name'; if there is none, make one */
    register symrec *p;
    register SYM s;
    SYM *tptr;
    int b, len;

    /* which symbol table is it in, which chain? */
    b = hash(name);
    tptr = (mosflg ? mostab : hashtab) + b;

    for (s = *tptr; s; s = p->hlink) {
        p = SYMP(s);
        if (p->name[0] == name[0] && strncmp(name,p->name,NAMESIZE) == 0)
            return s;
    }

    /* Entries have room for SHORTNAME characters, so short names cost
     * what they always did; a longer name gets a longer entry. Freed
     * entries are only reused for short names. */
    len = strlen(name);
    if (len <= SHORTNAME && (s = freesym)) freesym = SG_HLINK(s);
    else s = bank_alloc(S_NFIELDS * sizeof(FIELD)
                        + (len > SHORTNAME ? len : SHORTNAME) + 1);

    p = SYMP(s);
    strcpy(p->name,name);
    p->type = UNDECL;
    p->storage = 0;

    /* put the new entry on the front of the list;  the chance  */
    /* that a lookup is for a recently declared variable is     */
    /* high - this should therefore speed things up             */
    p->hlink = *tptr;
    *tptr = s;

    /* where symfree() unlinks it from: 1..128 hashtab, 129..256 mostab */
    p->down = (mosflg ? 128 : 0) + b + 1;

    return s;
}

SYM
symanon()
{
    return bank_alloc(S_NFIELDS * sizeof(FIELD) + SHORTNAME + 1);
}

/* copy the fields pushdown() saves from one record to another */
static
copydown(from,to)
SYM from, to;
{
    FIELD save[S_NSAVED];

    move(FIELDS(from),save,sizeof save);
    move(save,FIELDS(to),sizeof save);
}

/* to push down an outer block declaration */
pushdown(s)
SYM s;
{
    register FIELD *p;
    SYM d;

    if (d = freedown) freedown = SG_NEXT(d);
    else d = bank_alloc(S_NSAVED * sizeof(FIELD));

    copydown(s,d);
    p = FIELDS(s);
    memset(p,0,S_NSAVED * sizeof(FIELD));
    p[S_DOWN] = d;
}

/* to recover outer block declaration */
pullup(s)
SYM s;
{
    register SYM d;

    copydown(d = SG_DOWN(s),s);
    SP_NEXT(d,freedown);
    freedown = d;
}

/* a symbol's block has ended: unlink it, or bring back what it hid */
symfree(s)
SYM s;
{
    register SYM *tab, p, n;
    int d;

    d = SG_DOWN(s);
    if (d > 0 && d <= 256) {
        tab = d > 128 ? &mostab[d - 129] : &hashtab[d - 1];
        if ((p = *tab) == s) *tab = SG_HLINK(s);
        else {
            while ((n = SG_HLINK(p)) != s) p = n;
            n = SG_HLINK(s);
            SP_HLINK(p,n);
        }
        SP_HLINK(s,freesym);
        freesym = s;
    } else pullup(s);
}

/* give the storage back: on OS-9 every exit must */
symdone()
{
#ifdef SYMSTATS
    bank_stats();
#endif
    bank_done();
}

CELL
cons(a,d)
CELL d;
{
    register CELL c;

    c = bank_alloc(2 * sizeof(FIELD));
    setcar(c,a);
    setcdr(c,d);
    return c;
}
