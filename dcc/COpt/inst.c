#include "op.h"

/* devsys: instructions, labels, reference cells and operand texts are
 * banked records named by handles (op.h, bank.c); the logic is the
 * original's, statement for statement */

direct int inscount;
int insmax = MAXINS;
direct H ilist;
direct int lbf, lbdone;
static direct H freeins;
static direct int insused;

/* branch mnemonics */
char *bratab[] = {
    "ra", "sr", "eq", "ne", "lt", "ge", "le", "gt", "lo",
    "hs", "ls", "hi", "pl", "mi", "cc", "cs", "vc", "vs",
};

#define NUMBRA ((sizeof bratab) / (sizeof (char *)))

char **
srchmnem(mnemp,tblbeg,tblend)
char *mnemp;
char **tblbeg, **tblend;
{
    register char **nxtmnem;

    for (nxtmnem = tblbeg; nxtmnem < tblend; ++nxtmnem)
        if (strcmp(mnemp, *nxtmnem) == 0) {
            return nxtmnem;
        }
    return NULL;
}


insinit()
{
    register instruction *ip;

    /* the list head: a whole (zeroed) instruction, as code may read
     * the fields of ilist's "instruction" */
    ilist = bank_static(sizeof(instruction));
    ip = INS(ilist);
    ip->succ = ip->pred = ilist;
    inscount = 0;
    insused = 0;
    freeins = 0;
}


/* insert a new instruction before ins; mnem and args are in ordinary
 * memory (args is changed: a branch's label is parsed in place) */
H
insins(ins, mnem, args, l)
H ins;
char *mnem, *args;
H *l;
{
    register instruction *ip;
    H i, lp, t, pred;
    char *mnemp, *argp, **mnemtp, mbuf[MNEMSIZE+1];
    int argl, it;

    i = newins();

#ifdef DEBUG
    debug("insins: ");
    for (lp = (l ? *l : 0); lp; lp = L_NEXTL(lp))
        debug("%s%s", LAB(lp)->lname, L_NEXTL(lp) ? "/" : "");
    debug(" %s %s\n", mnem, args);
#endif

    t = l ? *l : 0;
    if (t) {
        for (lp = *l; lp; lp = L_NEXTL(lp))
            LAB(lp)->dest = i;
        *l = 0;
    }
    strcpy(mbuf, mnem);
    it = mnemix(mbuf);
    pred = I_PRED(ins);
    ip = INS(i);                /* its fields at one mapping (step 24) */
    ip->llist = t;
    ip->mnem = it;
    ip->succ = ins;
    ip->pred = pred;
    if (*(argp = args))
        while (*++argp);
    argl = argp - args;

    INS(pred)->succ = i;
    INS(ins)->pred = i;
    ++inscount;

    switch (*(mnemp = mbuf)) {
    case 'l':
        if (*++mnemp != 'b')
            break;
    case 'b':
        if ((mnemtp = srchmnem(mnemp + 1, bratab, bratab + NUMBRA)) != 0) {
            INS(i)->itype |= (BRANCH | LONG | (mnemtp - bratab));
            INS(i)->mnem = mnemix(*mnemtp);
            parse(LABEL, args, args, &argl);
            lp = findlabel(args);
            if (mnemtp == bratab) {
                if (lp && (t = L_DEST(lp)))
                    movlab(i, t);
                pred = I_PRED(i);
                if (I_LLIST(i) == 0
                    && pred != ilist
                    && (I_ITYPE(pred) & CODEBRK))
                {
                    remins(i);
                    ++opsdone;
                    return 0;
                }
                INS(i)->itype |= CODEBRK;
            }
            ++lbf;
            if (lp == 0)
                lp = inslabel(args);
            INS(i)->itype |= DESTPTR;
            INS(i)->arg = lp;
            insref(i, lp);
        }
        break;
    case 'p':
        if (strcmp(mnemp, "puls") == 0 && argl >= 3      /* devsys: "puls u" */
            && strcmp(args + argl - 3, ",pc") == 0)     /* read before args */
            INS(i)->itype |= CODEBRK;
        break;
    case 'r':
        if (strcmp(mnemp, "rts") == 0)
            INS(i)->itype |= CODEBRK;
        break;
    }
    if (!((it = I_ITYPE(i)) & BRANCH)) {
        if (it & CODEBRK)
            retadd(ins, i);
        t = newarg(argl);
        strcpy(ARG(t), args);
        INS(i)->arg = t;
    }
#ifdef DEBUG
    debug("inscount = %d, inserted: ", inscount);
    prtins(i);
#endif
    return i;
}


H
newins()
{
    register instruction *ip;
    H i;

    if ((i = freeins) == 0) {
        if (++insused > insmax && insused - insmax > 3)   /* no overflow */
            error("run out of instructions");
        i = bank_alloc(sizeof(instruction));
    } else {
        ip = INS(i);
        freeins = ip->succ;
        ip->itype = 0;
        ip->arg = 0;
    }
    return i;
}


/* A label's reference cells: succ runs from the newest to the oldest
 * and ends at 0, as before; back runs the other way round and the
 * newest cell's back is the oldest (step 24). remref looks from both
 * ends: delete removes the oldest branch, whose cell was at the end of
 * the list, and walking there grew with the number of branches to the
 * label (dev: 138,238 record reads) */
typedef struct refstruct {
    H succ, pred, back;     /* pred: the branch */
} refcell;

#define REF(h)  ((refcell *) bank_ptr (h))

insref(ip, lp)
H ip, lp;
{
    register refcell *r;
    H rlp, t, b;

    rlp = newchain();
    t = L_RLIST(lp);
    b = t ? REF(t)->back : rlp;     /* the oldest */
    r = REF(rlp);
    r->pred = ip;
    r->succ = t;
    r->back = b;
    if (t)
        REF(t)->back = rlp;
    LAB(lp)->rlist = rlp;
}


movlab(i1, i2)
H i1, i2;
{
    H l, lastl, t;

    if (i1 != i2 && (l = I_LLIST(i1))) {
        do {
            lastl = l;
            LAB(l)->dest = i2;
        } while ((l = L_NEXTL(l)) != 0);
        t = I_LLIST(i2);
        LAB(lastl)->nextl = t;
        t = I_LLIST(i1);
        INS(i2)->llist = t;
        INS(i1)->llist = 0;
    }
}


/* a copy of a non-branch instruction's operand, in ordinary memory */
char *
argcopy(i, buf)
H i;
char *buf;
{
    H a;

    a = I_ARG(i);
    return strcpy(buf, ARG(a));
}


delete()
{
    register instruction *ip;
    H i, l, a;
    int itype;
    char *mbuf;             /* the mnemonic (mnemtab: stays valid) */

    if ((i = I_SUCC(ilist)) == ilist)
        error("deleting too many instructions");

    ip = INS(i);            /* its fields at one mapping (step 26) */
    itype = ip->itype;
    l = ip->llist;
    if ((itype & LONGBRA) == LONGBRA)
        fix(i, FORWARD);
    if (l != 0)
        fix(i, BACK);
    /* OPUTS and OPUTC (fputs and putc; on the 6809 into the buffer
     * directly, step 26), not fprintf: its formatting cost more than
     * the characters (step 24) */
    while (l) {
        OPUTS(LAB(l)->lname);
        if (L_LFLAGS(l) & GLOBAL)
            OPUTC(':');
        if ((l = L_NEXTL(l)) != 0)
            OPUTC('\n');
    }

    OPUTC(' ');
    ip = INS(i);            /* fix may have changed them */
    itype = ip->itype;
    mbuf = mnemtab[ip->mnem];
    a = ip->arg;
    if (itype & BRANCH) {
        if (itype & LONG)
            OPUTC('l');
        OPUTC('b');
        OPUTS(mbuf);
        OPUTC(' ');
        OPUTS((itype & DESTPTR ? LAB(a)->lname : ARG(a)));
    } else {
        OPUTS(mbuf);
        if (ARG(a)[0]) {
            OPUTC(' ');
            OPUTS(ARG(a));
        }
    }
    OPUTC('\n');
    remins(i);
}


/* is a long branch to one of instruction i's labels left? If not, fix's
 * walk back from i finds nothing (step 24: so for most labels, te 923
 * of 1,077). A label's reference cells are exactly the branches whose
 * operand it is, and every long branch has DESTPTR (insins) */
static
longref(i)
H i;
{
    H l, r, t;

    for (l = I_LLIST(i); l; l = L_NEXTL(l))
        for (r = L_RLIST(l); r; r = C_SUCC(r)) {
            t = C_PRED(r);
            if ((I_ITYPE(t) & LONGBRA) == LONGBRA)
                return TRUE;
        }
    return FALSE;
}

/* Fix: Added return type void */
void
fix(i, direction)
H i;
{
    H ip = i, dp, lp;
    int c, t;
    char abuf[ARGSIZE+1];

    if (direction == FORWARD) {
        lp = (I_ITYPE(ip) & DESTPTR ? I_ARG(ip) : findlabel(argcopy(ip, abuf)));
        if (lp && (dp = L_DEST(lp))) {
            for (c = BRALIM; c && (ip = I_SUCC(ip)) != ilist; --c) {
                if (dp == ip) {
#ifdef DEBUG
                    debug("fix: ");
                    prtins(i);
#endif
                    INS(ip)->itype |= BLOCKDUP;
                    INS(i)->itype &= ~LONG;
                    ++lbdone;
                    return;
                }
            }
        }
    } else {
        if (!longref(i))
            return;
        for (c = BRALIM; c && (ip = I_SUCC(ip)) != ilist; --c) {
            if (((t = I_ITYPE(ip)) & LONGBRA) == LONGBRA) {
                lp = (t & DESTPTR) ? I_ARG(ip) : findlabel(argcopy(ip, abuf));
                if (lp && i == L_DEST(lp)) {
#ifdef DEBUG
                    debug("fix: ");
                    prtins(ip);
#endif
                    INS(ip)->itype &= ~LONG;
                    ++lbdone;
                    return;
                }
            }
        }
    }
}


remins(i)
H i;
{
    register instruction *ip;
    H l, a, p, s;
    int it;

#ifdef DEBUG
    debug("remins: ");
    prtins(i);
#endif
    ip = INS(i);                /* its fields at one mapping (step 24) */
    a = ip->arg;
    it = ip->itype;
    l = ip->llist;
    p = ip->pred;
    s = ip->succ;
    if (it & BRANCH) {
        remref(i, a);
    } else {
        freearg(a);
        if (it & CODEBRK)
            retrem(i);
    }
    for ( ; l; l = L_NEXTL(l)) {    /* return labels */
        LAB(l)->dest = 0;
        freelabel(l);
    }
    /* remove instruction from the list */
    INS(p)->succ = s;
    INS(s)->pred = p;

    INS(i)->succ = freeins;
    freeins = i;

    --inscount;
}

remref(ip, lp)
H ip, lp;
{
    register refcell *r;
    H head, a, b, n, p;

    /* a branch has one cell on its label's list, so the cell found
     * from either end is the one the walk from the head found */
    if (lp && (head = L_RLIST(lp)) != 0) {
        for (a = head, b = REF(head)->back; ; a = n, b = p) {
            if (C_PRED(a) == ip) {
                b = a;
                break;
            }
            if (C_PRED(b) == ip)
                break;
            if (a == b || (n = C_SUCC(a)) == b)
                return 0;           /* not there */
            p = REF(b)->back;
        }
        r = REF(b);
        n = r->succ;
        p = r->back;
        if (b == head) {
            if (n)
                REF(n)->back = p;
            LAB(lp)->rlist = n;
        } else {
            REF(p)->succ = n;
            REF(n ? n : head)->back = p;
        }
        freelabel(lp);
        freechain(b);
    }
}

/* The returns (rts, puls ...,pc: CODEBRK and not BRANCH), oldest to
 * newest, so finddupl can look for an earlier return without walking
 * every instruction before it: with the whole file in memory that walk
 * grew with the square of its length (step 24). A cell is a record
 * {newer, older, the instruction}; an instruction stays a return for
 * life (only branches change type), and a return is only ever added
 * at the end of the code (insins before ilist) */
direct H retnew;                /* the newest return's cell, 0: none */
static direct H retold, _rfree;

typedef struct rstruct {
    H newer, older, ins;
} rcell;

#define RCL(h)  ((rcell *) bank_ptr (h))

/* instruction i, a return, has been inserted before ins */
retadd(ins, i)
H ins, i;
{
    register rcell *r;
    H c, t;

    if (ins != ilist)
        error("return inserted inside the code");
    if ((c = _rfree) != 0)
        _rfree = C_SUCC(c);
    else
        c = bank_alloc(sizeof(rcell));
    t = retnew;
    r = RCL(c);
    r->newer = 0;
    r->older = t;
    r->ins = i;
    if (t)
        RCL(t)->newer = c;
    else
        retold = c;
    retnew = c;
}

/* return i is being removed: look for its cell from both ends (delete
 * removes the oldest, a pattern or duplsrch one near the newest) */
retrem(i)
H i;
{
    register rcell *r;
    H a, b, n, o;

    for (a = retold, b = retnew; ; a = n, b = o) {
        if (a == 0)
            error("lost a return");
        if (RCL(a)->ins == i) {
            b = a;
            break;
        }
        if (RCL(b)->ins == i)
            break;
        n = RCL(a)->newer;
        o = RCL(b)->older;
    }
    r = RCL(b);
    n = r->newer;
    o = r->older;
    if (n)
        RCL(n)->older = o;
    else
        retnew = o;
    if (o)
        RCL(o)->newer = n;
    else
        retold = n;
    CHN(b)->succ = _rfree;
    _rfree = b;
}

static direct H _cfree = 0;

H
newchain()
{
    register chain *c;
    H cp;

    if ((cp = _cfree) != 0)
        _cfree = C_SUCC(cp);
    else
        cp = bank_alloc(sizeof(refcell));     /* a label's are */
    c = CHN(cp);
    c->succ = c->pred = 0;
    return cp;
}


freechain(p)
H p;
{
    CHN(p)->succ = _cfree;
    _cfree = p;
}

/* Operand texts and labels are as long as their text needs, in 8-byte
 * steps (step 21: most operands are a few characters, and fixed 16- and
 * 91-byte slots made te and dev-cc too big). Freed ones wait on a list
 * per size; a record's size is worked out again from its text. */
direct H tfree[13];             /* free records by size / 8 */

#ifndef _OS9
/* a record of at least n bytes (on the 6809 in assembly, bank.c) */
H
talloc(n)
register int n;
{
    register H h;

    n = (n + 7) >> 3;
    if (h = tfree[n]) tfree[n] = C_SUCC(h);
    else h = bank_alloc(n << 3);
    return h;
}

/* record h, of at least n bytes, is free */
tfreerec(h, n)
H h;
register int n;
{
    n = (n + 7) >> 3;
    CHN(h)->succ = tfree[n];
    tfree[n] = h;
}
#endif

/* room for a siz-character operand, empty */
H
newarg(siz)
int siz;
{
    H ap;

    ap = talloc(siz + 1);
    CHN(ap)->succ = 0;
    return ap;
}

/* instruction i's operand is now s: in its record if s fits (its
 * present text says how big the record is at least), else a new one.
 * (Until step 21 this was strcpy into the record, which ran past a
 * 16-byte one when a pattern made the operand longer) */
setarg(i, s)
H i;
char *s;
{
    register H a;
    register int n;

    a = I_ARG(i);
    n = strlen(s);
    if (n > (strlen(ARG(a)) | 7)) {     /* the most the record holds */
        freearg(a);
        a = talloc(n + 1);
        INS(i)->arg = a;
    }
    strcpy(ARG(a), s);
}

/* The mnemonics: each distinct one once, in ordinary memory, found
 * through 64 hash chains; an instruction holds its index. Step 26: the
 * chains were 32, by the first two characters, so ldd, lda, ldx, ...
 * shared one (5.3 strcmps a lookup on te); now every character counts
 * (h * 2 + c: 1.3). Only the low 6 bits are used, so the 6809 adds in 8
 * bits. A chain's order changes nothing but the time */
#define MAXMNEM 255
#define MHASH   64
char *mnemtab[MAXMNEM];
static unsigned char mnext[MAXMNEM], mhead[MHASH];  /* index + 1, 0: end */
static int nmnem;

#ifndef _OS9
mnemix(s)
register char *s;
{
    register int i;
    int h;

    for (h = 0, i = 0; s[i]; ++i)
        h = h * 2 + s[i];
    h &= MHASH - 1;
    for (i = mhead[h]; i; i = mnext[i - 1])
        if (strcmp(mnemtab[i - 1], s) == 0)
            return i - 1;
    if (nmnem == MAXMNEM)
        error("too many mnemonics");
    mnemtab[nmnem] = strcpy(grab(strlen(s) + 1), s);
    mnext[nmnem] = mhead[h];
    mhead[h] = ++nmnem;
    return nmnem - 1;
}
#else
/* the same in assembly (mnemix(s) returns s's index) */
#asm
mnemix: pshs u
 ldu 4,s
 clrb
 leax ,u
mnh1 tst ,x
 beq mnh2
 aslb
 addb ,x+
 bra mnh1
mnh2 andb #63 MHASH - 1
 clra
 ldx #mhead
 leax d,x
 pshs x
 ldb ,x
mnx1 beq mnx3
 decb
 pshs b
 clra
 aslb
 rola
 ldx #mnemtab
 ldd d,x
 pshs d,u
 lbsr strcmp
 leas 4,s
 std -2,s
 puls b
 beq mnx2
 ldx #mnext
 abx
 ldb ,x
 bra mnx1
mnx2 clra
 bra mnx4
mnx3 ldd nmnem,y
 cmpd #255
 bne mnx5
 leax mnxerr,pcr
 pshs x
 lbsr error
mnx5 pshs u
 pshs u
 lbsr strlen
 addd #1
 std ,s
 lbsr grab
 std ,s
 lbsr strcpy
 leas 4,s
 pshs d
 ldd nmnem,y
 aslb
 rola
 ldx #mnemtab
 leax d,x
 puls d
 std ,x
 ldb [,s]
 pshs b
 ldd nmnem,y
 ldx #mnext
 leax d,x
 puls b
 stb ,x
 ldd nmnem,y
 addd #1
 std nmnem,y
 stb [,s]
 subd #1
mnx4 leas 2,s
 puls u,pc
mnxerr fcc "too many mnemonics"
 fcb 0
#endasm
#endif


freearg(p)
H p;
{
    if (p != 0)
        tfreerec(p, strlen(ARG(p)) + 1);
    return 0;
}
