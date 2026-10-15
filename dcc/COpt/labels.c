#include "op.h"

/* devsys: labels are banked records (op.h); each bucket's list head is
 * a banked chain record, ltable holds their handles */

H ltable[128];

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

    p = newlabel(strlen(s));

    l = ltable[lhash(s)];
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

    l = ltable[lhash(s)];

    for (p = C_SUCC(l); p != l; p = L_SUCC(p)) {
        if (strcmp(s,LAB(p)->lname) == 0)
            return p;
    }
    return 0;
}

/* a label's bucket (step 24): hash()'s sum of the characters put te's
 * 1,343 labels, mostly _123 and _$12, in 120 buckets up to 72 deep, and
 * a lookup reads every label before it in its bucket. Each character
 * shifted in by 2 spreads them as well as random (te: at most 18). Only
 * the low 7 bits are used. A name
 * is always in its own bucket, in the order it came, so which bucket
 * that is changes nothing else */
lhash(s)
register char *s;
{
    register int i = 0;

    while (*s)
        i = (i << 2) + *s++;
    return i & 127;
}

hash(s)
register char *s;
{
    int i = 0;

    while (s && *s)
        i += *s++;

    return i & 127;
}

/* a label with room for a len-character name (inst.c's talloc) */
H
newlabel(len)
int len;
{
    register label *lb;
    H p;

    p = talloc(LABHDR + len + 1);
    lb = LAB(p);
    lb->dest = 0;
    lb->rlist = 0;
    lb->nextl = 0;
    lb->lflags = 0;
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
        /* add to the free records */
        tfreerec(p, LABHDR + strlen(LAB(p)->lname) + 1);
    }
}

char *newbra(lblp)
char *lblp;
{
    static int labnum = 0;

    sprintf(lblp,"_$%d",++labnum);
    return lblp;
}
