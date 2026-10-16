#include "op.h"

/* devsys: labels are banked records (op.h). A bucket's labels are a
 * list from lhead to ltail (each label's succ and pred, 0 at the ends),
 * in the order they came. The heads are in ordinary memory (step 26:
 * they were 128 banked chain records, 8 bytes each), so walking an
 * empty bucket maps nothing; a walk maps every label it passes, each
 * likely in another block: findlabel caused 62% of te's window
 * changes. So 512 buckets and a hash that spreads te's names */

#define NBUCKET 512
static H lhead[NBUCKET], ltail[NBUCKET];

labelinit()
{
}

/* s is in ordinary memory */
H
inslabel(s)
char *s;
{
    register label *lb;
    H p, t;
    int b;

    p = newlabel(strlen(s));
    b = lhash(s);
    t = ltail[b];

    /* put at back of list */
    lb = LAB(p);
    strcpy(lb->lname, s);
    lb->succ = 0;
    lb->pred = t;
    if (t)
        LAB(t)->succ = p;
    else
        lhead[b] = p;
    ltail[b] = p;

    return p;
}

/* s is in ordinary memory */
H
findlabel(s)
char *s;
{
    H p;

    for (p = lhead[lhash(s)]; p; p = L_SUCC(p)) {
        if (strcmp(s,LAB(p)->lname) == 0)
            return p;
    }
    return 0;
}

/* a label's bucket. A name is always in its own bucket, in the order it
 * came, so which bucket that is changes nothing else. Step 24: hash()'s
 * sum of the characters put te's labels, mostly _123 and _$12, up to 72
 * deep in 128 buckets; each character shifted in by 2 did better. Step
 * 26, 512 buckets: by 3, and the high byte folded into the low one, as
 * the digits of _123 differ only in their low 4 bits */
lhash(s)
register char *s;
{
    register H i = 0;       /* 16 bits on both machines */

    while (*s)
        i = (i << 3) + *s++;
    return (i ^ (i >> 8)) & (NBUCKET - 1);
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
    int b, n;

    lb = LAB(p);
    if (lb->dest == 0 && lb->rlist == 0) {
        /* remove from its bucket */
        s = lb->succ;
        pr = lb->pred;
        n = strlen(lb->lname);
        b = (s && pr) ? 0 : lhash(lb->lname);
        if (pr)
            LAB(pr)->succ = s;
        else
            lhead[b] = s;
        if (s)
            LAB(s)->pred = pr;
        else
            ltail[b] = pr;
        /* add to the free records */
        tfreerec(p, LABHDR + n + 1);
    }
}

char *newbra(lblp)
char *lblp;
{
    static int labnum = 0;

    sprintf(lblp,"_$%d",++labnum);
    return lblp;
}
