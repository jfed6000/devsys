#include "op.h"

/* devsys: labels are banked records (op.h); each bucket's list head is
 * a banked chain record, ltable holds their handles */

H ltable[128];
direct H lfree;

labelinit()
{
    register int i;
    register chain *p;
    H h;

    for (i = 0; i < 128; i++) {
        ltable[i] = h = bank_static(sizeof(chain));
        p = CHN(h);
        p->succ = p->pred = h;
    }
}

/* s is in ordinary memory */
H
inslabel(s)
char *s;
{
    register label *lb;
    H l, p, t;

    p = newlabel();

    l = ltable[hash(s)];
    strcpy(LAB(p)->lname,s);

    /* put at back of list */
    t = C_PRED(l);
    lb = LAB(p);
    lb->succ = l;
    lb->pred = t;
    CHN(t)->succ = p;
    CHN(l)->pred = p;

    return p;
}

/* s is in ordinary memory */
H
findlabel(s)
char *s;
{
    H p, l;

    l = ltable[hash(s)];

    for (p = C_SUCC(l); p != l; p = L_SUCC(p)) {
        if (strcmp(s,LAB(p)->lname) == 0)
            return p;
    }
    return 0;
}

hash(s)
register char *s;
{
    int i = 0;

    while (s && *s)
        i += *s++;

    return i & 127;
}

H
newlabel()
{
    register label *lb;
    H p;

    if (p = lfree) {
        lb = LAB(p);
        lfree = lb->succ;
        lb->dest = 0;
        lb->rlist = 0;
        lb->nextl = 0;
        lb->lflags = 0;
    } else p = bank_alloc(sizeof(label));

    return p;
}

freelabel(p)
H p;
{
    register label *lb;
    H s, pr;

    lb = LAB(p);
    if (lb->dest == 0 && lb->rlist == 0) {
        /* remove from ltable */
        s = lb->succ;
        pr = lb->pred;
        CHN(pr)->succ = s;
        CHN(s)->pred = pr;
        /* add to free list */
        LAB(p)->succ = lfree;
        lfree = p;
    }
}

char *newbra(lblp)
char *lblp;
{
    static int labnum = 0;

    sprintf(lblp,"_$%d",++labnum);
    return lblp;
}
