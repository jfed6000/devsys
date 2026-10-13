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
    int argl;

    i = newins();

#ifdef DEBUG
    debug("insins: ");
    for (lp = (l ? *l : 0); lp; lp = L_NEXTL(lp))
        debug("%s%s", LAB(lp)->lname, L_NEXTL(lp) ? "/" : "");
    debug(" %s %s\n", mnem, args);
#endif

    t = l ? *l : 0;
    INS(i)->llist = t;
    if (t) {
        for (lp = *l; lp; lp = L_NEXTL(lp))
            LAB(lp)->dest = i;
        *l = 0;
    }
    strcpy(mbuf, mnem);
    strcpy(INS(i)->mnem, mbuf);
    if (*(argp = args))
        while (*++argp);
    argl = argp - args;

    pred = I_PRED(ins);
    ip = INS(i);
    ip->succ = ins;
    ip->pred = pred;
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
            strcpy(INS(i)->mnem, *mnemtp);
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
    if (!(I_ITYPE(i) & BRANCH)) {
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


insref(ip, lp)
H ip, lp;
{
    H rlp, t;

    rlp = newchain();
    CHN(rlp)->pred = ip;
    t = L_RLIST(lp);
    CHN(rlp)->succ = t;
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
    H i, l, a;
    int itype;
    char mbuf[MNEMSIZE+1];

    if ((i = I_SUCC(ilist)) == ilist)
        error("deleting too many instructions");

    if ((I_ITYPE(i) & LONGBRA) == LONGBRA)
        fix(i, FORWARD);
    if ((l = I_LLIST(i)) != 0)
        fix(i, BACK);
    while (l) {
        fprintf(out, "%s", LAB(l)->lname);
        if (L_LFLAGS(l) & GLOBAL)
            putc(':', out);
        if ((l = L_NEXTL(l)) != 0)
            putc('\n', out);
    }

    putc(' ', out);
    itype = I_ITYPE(i);
    strcpy(mbuf, INS(i)->mnem);
    a = I_ARG(i);
    if (itype & BRANCH) {
        fprintf(out, "%sb%s %s", (itype & LONG ? "l" : ""), mbuf,
            (itype & DESTPTR ? LAB(a)->lname : ARG(a)));
    } else {
        fprintf(out, "%s", mbuf);
        if (ARG(a)[0])
            fprintf(out, " %s", ARG(a));
    }
    putc('\n', out);
    remins(i);
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

#ifdef DEBUG
    debug("remins: ");
    prtins(i);
#endif
    a = I_ARG(i);
    if (I_ITYPE(i) & BRANCH) {
        remref(i, a);
    } else
        freearg(a);
    for (l = I_LLIST(i); l; l = L_NEXTL(l)) {   /* return labels */
        LAB(l)->dest = 0;
        freelabel(l);
    }
    /* remove instruction from the list */
    ip = INS(i);
    p = ip->pred;
    s = ip->succ;
    INS(p)->succ = s;
    INS(s)->pred = p;

    INS(i)->succ = freeins;
    freeins = i;

    --inscount;
}

remref(ip, lp)
H ip, lp;
{
    H rlp, prev, n;

    if (lp) {
        for (prev = 0, rlp = L_RLIST(lp); rlp; prev = rlp, rlp = C_SUCC(rlp))
            if (C_PRED(rlp) == ip) {
                n = C_SUCC(rlp);
                if (prev)
                    CHN(prev)->succ = n;
                else
                    LAB(lp)->rlist = n;
                freelabel(lp);
                freechain(rlp);
                break;
            }
    }
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
        cp = bank_alloc(sizeof(chain));
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

static direct H safree = 0, lafree = 0;


H
newarg(siz)
int siz;
{
    H ap;

    if (siz < 16)
        if ((ap = safree) != 0)
            safree = C_SUCC(ap);
        else
            ap = bank_alloc(16);
    else if ((ap = lafree) != 0)
        lafree = C_SUCC(ap);
    else
        ap = bank_alloc(ARGSIZE + 1);
    CHN(ap)->succ = 0;
    return ap;
}


freearg(p)
H p;
{
    if (p != 0)
    {
        if (strlen(ARG(p)) < 16) {
            CHN(p)->succ = safree;
            safree = p;
        } else {
            CHN(p)->succ = lafree;
            lafree = p;
        }
    }
    return 0;
}
