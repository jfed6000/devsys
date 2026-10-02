#include "rlink.h"

direct int   rbfcount;

extern UINT idpdBuf[];
direct UINT *refptr = idpdBuf;

extern struct dref_node *ExtStart; 
extern char      _Crc[];

/* ************************************************ *
 * Add new dnodes for any nodes above what has      *
 * already been created.  These are prepended       *
 *  to the ExtStart list                            *
 * ************************************************ */

creatdnodes(maxcount)
    int maxcount;
{
    register struct dref_node *new_ref;

    ++maxcount;
    new_ref = ExtStart;

    /* Walk through list reducing maxcount by the count of existing refs */

    while (new_ref && (maxcount > 0))
    {
        new_ref = new_ref->dnext;
        maxcount -= 7;      /* Each dnode contains 7 refs */
    }

    /* Create new nodes for the remaining count *
     * These are prepended to the begin of list */

    while (maxcount > 0)
    {
        new_ref = (struct dref_node *)grab(sizeof(struct dref_node));
        new_ref->dnext = ExtStart;
        ExtStart = new_ref;
        maxcount -= 7;
    }
}

/* Set range of _count_ words to 0xffff */

setrefvoid(mylist, count)
    register UINT *mylist;
    int count;
{
    while (count-- > 0)
    {
        *(mylist++) = -1;
    }
}

/* Find slot in array where element = 0xffff */

char *
emptyref(reg01)
    register INTTYPE *reg01;
{
    int var1 = 0;

    while (var1 < 7)
    {
        if (*reg01 < 0)
            return (char *)reg01;  // Cast to char * to match declaration

        ++var1;
        ++reg01;
    }

    return 0;
}

notused(p1)
    char *p1;
{
    register struct dref_node *r1 = ExtStart;

    while (r1)
    {
        fprintf(stderr, "  %s p=%04x next=%04x\n", p1, r1, r1->dnext);
        r1 = r1->dnext;
    }
}

getrefs(baseref, addr)
    register struct dref_node **baseref; int addr;
{
    UINT *v0;
    /*notused(baseref->name);*/

    /* Initialize baseref if not already */

    if (!(*baseref))    /* else 22b2 */
    {
        *baseref = ExtStart;
        ExtStart = ExtStart->dnext;
        (*baseref)->dnext = 0;
        setrefvoid((*baseref)->refs, 7);
    }

    if (!(v0 = (UINT *)emptyref((*baseref)->refs)))    /* else L22f6 */
    {
        if (!ExtStart)
        {
            fatal("out of dref nodes");
        }
        else
        {
            struct dref_node *tmpref = ExtStart;

            ExtStart = ExtStart->dnext;
            tmpref->dnext = *baseref;
            *baseref = tmpref;
            setrefvoid((v0 = (*baseref)->refs), 7);
        }
    }

    *v0 = addr;
}

writrefs(lref)
    register struct dref_node *lref;
{
    int idx;
    INTTYPE *curref;

    while (lref)
    {
        idx = 0;
        curref = lref->refs;

        while (idx < 7)
        {
            if (*curref == EOF)
                break;

            putref(*(curref++));
            ++idx;
        }

        lref = lref->dnext;
    }

    flushbuf();
}

putref(parm1)
    UINT parm1;
{
    *(refptr++) = o9_int(parm1);

    if ((rbfcount += 2) >= 256)
    {
        flushbuf();
    }
}

flushbuf()
{
    crc(idpdBuf, rbfcount, _Crc);
    fwrite(idpdBuf, rbfcount, 1, OutFile);
    wrerr();
    refptr = idpdBuf;
    rbfcount = 0;
}
