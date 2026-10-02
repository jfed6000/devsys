/* **************************************************************** *
 * Part6.c - mostly deals with Labels                               *
 *                                                                  *
 * $Id::                                                        $   *
 * **************************************************************** */

#include <stdlib.h>

#define NEED_CTYPE
#include "rma.h"

#ifdef COCO
void * getmem ();
#endif

#define _ischar(c) (_chcodes[(int)c]&(_DIGIT|_LOWER|_UPPER|_CONTROL))

static int FndTreeSlot (
#ifdef __STDC__
    char *, struct symblstr **
#endif
);

static struct  symblstr * NewLabel (
#ifdef __STDC__
    char *, struct symblstr **, int
#endif
);

static void SetArow (                  /* L204a() */
#ifdef __STDC__
struct symblstr *parm
#endif
);

static int phaserr ();

/* ******************************************************************** *
 * MovLbl() - Moves a label to destpos (parameter) if the first char    *
 *            in the buffer is a legal label char.                      *
 * Returns: (1) if label found and moved                                *
 *          (0) if no label                                             *
 * ******************************************************************** */

int
#ifdef __STDC__
MovLbl (char *destpos)
#else
MovLbl (destpos)
     char *destpos;
#endif
{
    int count;

    if (isalpha ((int)(*SrcChar)))
    {
        count = SYMLEN - 1;

        while ((count-- > 0) && (_ischar (*SrcChar)))
        {
            *(destpos++) = *(SrcChar++);
        }

        *destpos = '\0';

        while (_ischar (*SrcChar))
        {
            ++SrcChar;
        }

        return 1;
    }

    return 0;
}

/* **************************************************************** *
 * do_label() - Sets up Label definitition from buffer SrcChr       *
 * Returns: 1 if label found in label field (in buffer)             *
 *          0 if label not in label field                           *
 * **************************************************************** */

int
do_label (ZILCH)
{
    register struct symblstr *_mysmbl;
    int _cmpretrn;

    CurntLabel = 0;

    if (MovLbl (AltLbl) == 0)
    {
        return 0;
    }

    if (*SrcChar == ':')
    {
        ++SrcChar;
        LblTyp |= CODENT;
    }

    _cmpretrn = FndTreeSlot (AltLbl, &CurntLabel);

    /* FndTreeSlot has set CurntLabel to point to last element in the tree */

    _mysmbl = CurntLabel;

    if (PASS1)
    {
        if (_cmpretrn)      /* If not an existing label */
        {
            CurntLabel = _mysmbl = NewLabel (AltLbl, &CurntLabel, _cmpretrn);
        }

        if ( ! (_mysmbl->locmsk & RELATIVE))
        {
            _mysmbl->smbltyp = (_mysmbl->smbltyp & 0xf0) | WhereLoc;
            _mysmbl->locmsk |= LblTyp;

            if (S_Addrs)
            {
                _mysmbl->s_ofst = *S_Addrs;
            }
        }
        else        /* If a previously-defined label */
        {
            /* I _believe_ that this condition would inevitably
             * result in an error later on when reparsing on pass 2
             */

            if ((_mysmbl->smbltyp & 0x0f) != (CODENT | INIENT))
            {
                _mysmbl->locmsk |= NEGMASK;
            }
        }

        if ((_mysmbl->locmsk & 0xc6) == (RELATIVE | CODENT | DIRENT))
        {       /* I.E. if NOT NEGMASK */
            ++GlobCnt;
        }

        return 1;
    }

    /* Pass 2 from here on out */

    if (_cmpretrn || ! (_mysmbl->locmsk & RELATIVE))
    {
        errexit ("symbol lost!?");
    }

    _mysmbl->locmsk &= (~DIRLOC);

    if ((_mysmbl->locmsk) & NEGMASK)
    {
        e_report ("redefined name");
        return 1;
    }
    else
    {
        if ( (S_Addrs)                                &&
             ((_mysmbl->smbltyp & 0x0f) == TypeBasic) &&    /* Default Type */
             (_mysmbl->s_ofst != *S_Addrs)               )
        {
            phaserr ();
            _mysmbl->s_ofst = *S_Addrs;
        }
    }

    return 1;
}

/* ******************************************************************** *
 * lblnumeric() - Creates and sets up a label for numeric assignments   *
 *           Adjusts locmsk and sets s_addr for label                   *
 *           Sets ETyp and RAddr for "set"/"equ" ref_ent                *
 * ******************************************************************** */

int
lblnumeric (ZILCH)
{
    register struct symblstr *entry;
    struct ref_xtrnal *_refptr;
    int cmp_case,
        s_type;

    cmp_case = FndTreeSlot (d0587, &CurntLabel);
    entry = CurntLabel;

    /* If name not found, create a new entry if pass 1, else error */

    if (cmp_case)
    {
        if (PASS1)
        {
            entry = CurntLabel = NewLabel (d0587, &CurntLabel, cmp_case);
        }
        else
        {
            errexit ("new symbol in pass two");
        }
    }

    s_type = entry->smbltyp & 0x0f;

    /* below = IS CODLOC or if not CODLOC, NOT RELATIVE */

    if ((entry->locmsk ^ RELATIVE) & (RELATIVE | CODLOC))
    {
        IsExtrn = 1;

        if (PASS1)
        {
            entry->locmsk |= DIRLOC;
        }

        /* If "set" or "equ" ? */

        if ( (entry->locmsk & RELATIVE)   &&
             ((s_type == (CODENT | DIRENT)) || (s_type == (CODENT | INIENT)))    )
        {
            _refptr = entry->reftbl;

            /* Populate d0791 with refs to this symbol */

            while (_refptr)
            {
                OptBPtr->ETyp = _refptr->RfTyp;

                /* FIXME : I _think the following cast is in order
                 * because I believe that r_offset for "set" and "equ"
                 * is a ptr to struct symbstr rather than address
                 */

                OptBPtr->RAddr = (struct symblstr *)_refptr->r_offset;
                SetArow (OptBPtr->RAddr);
                ++OptBPtr;
                _refptr = _refptr->NxtRef;
            }
        }
        else        /* not "equ" or "set" ? */
        {
            OptBPtr->ETyp = 0;
            (OptBPtr++)->RAddr = entry;
            SetArow (entry);
        }
    }
    else
    {
        if (Pass2)
        {
            if (entry->locmsk & DIRLOC)
            {
                SetArow (entry);
            }
        }
    }

    if (s_type == 7)    /* CODENT | DIRENT | INIENT ??? COMMON ??? */
    {
        NmbrInt = 0;
    }
    else
    {
        NmbrInt = entry->s_ofst;
    }
    return 1;
}

static void
#ifdef __STDC__
SetArow (struct symblstr *parm)                  /* L204a() */
#else
SetArow (parm)
     struct symblstr *parm;
#endif
{
    register struct symblstr *sbl = parm;

    if ( ! HadArrow)
    {
        if ((sbl->smbltyp & (CODENT | DIRENT)) == DIRENT)
        {
            HadArrow = 1;
        }
        else
        {
            HadArrow = -1;
        }
    }
}

/* ******************************************************************** *
 * l2069() -                                                            *
 * This function is called from setmac(), l1780() (for "equ" & "set"),  *
 *      do_common(), and do_rmb()                                       *
 * Does nothing if labeldef is already NEGMASK                         *
 * ******************************************************************** */

void
#ifdef __STDC__
l2069 (int newmode)
#else
l2069 (newmode)
     char newmode;
#endif
{
    /* Do nothing if already constant */

    if ( ! ((CurntLabel->locmsk) & NEGMASK))
    {
        if ((newmode != (CODENT | INIENT)) &&
                ((CurntLabel->smbltyp & 0x0f) == (CODENT | INIENT)))
        {
            (CurntLabel->locmsk) |= NEGMASK;
        }
        else
        {
            CurntLabel->smbltyp = ((CurntLabel->smbltyp & 0xf0) | newmode);
        }
    }
}

/* ************************************************************************ *
 * l20a4() - Used for "equ", "set", or "rmb"                                *
 * ************************************************************************ */

void
#ifdef __STDC__
l20a4 (int address)
#else
l20a4 (address)
#  ifdef COCO
    long
#  else
    int
#  endif
        address;
#endif
{
    register struct symblstr *reg = CurntLabel;
    struct ref_ent *var1;
    struct ref_xtrnal *var2;
    int _symboltype;

    _symboltype = reg->smbltyp & 0x0f;

    if (OptBPtr == d0791)       /* If no refs to this */
    {
        if ((reg->smbltyp & CODENT) && (reg->locmsk & CODLOC))
        {
            reg->locmsk &=  ~CODLOC;      /* Unset CODLOC */
        }
        else
        {
            if (      (Pass2 > 0)
                 &&   (address != reg->s_ofst)
                 && ! (reg->locmsk & NEGMASK)
                 &&   (_symboltype != (CODENT | INIENT)))
            {
                phaserr ();
            }
        }
    }
    else        /* d0791 has valid refs */
    {
        if (PASS1)
        {
            if (_symboltype == (CODENT | DIRENT))  /* Constant */
            {
                reg->locmsk |= CODLOC;

                while (OptBPtr > d0791)
                {
                    --OptBPtr;

                    /* FIXME 0 : Since we're dealing with "equ", r_offset
                     * is a symblstr * rather than an address, so cast
                     * the symblstr * to int to satisfy prototyping
                     */

                    RefCreat (reg, OptBPtr->ETyp, (int)OptBPtr->RAddr);
                }
            }
        }
        else           /* Pass 2 */
        {
            if (_symboltype == (CODENT | DIRENT))
            {
                var2 = reg->reftbl;    /* FIXME */
                var1 = OptBPtr;

                while (var2 && (d0791 < var1))
                {
                    --var1;

                    /* See "FIXME 0" above */

                    if ((var2->r_offset != (int)(var1->RAddr)))
                    {
                        break;
                    }

                    var2 = var2->NxtRef;    /* ERROR */
                }

                if ((var2 != 0) || (d0791 != var1) ||
                    (reg->s_ofst != (int) address))
                {
                    phaserr ();
                }
                else
                {
                    if (reg->locmsk & CODENT)
                    {
                        var2 = reg->reftbl;    /* FIXME incompat. ptr type */

                        while (var2)
                        {
                            /* FIXME : cast to avoid errors.  */
                            if ( ! ((((struct symblstr *)
                                            (var2->r_offset))->locmsk) &
                                                (NEGMASK | DIRENT)) )
                            {
                                ilExtRef ();
                            }

                            var2 = var2->NxtRef;    /* FIXME ERROR */
                        }
                    }
                }
            }
            else    /* Not (CODENT | DIRENT) */
            {
                ilExtRef ();
            }
        }
    }

    reg->s_ofst = address;
}

static int
phaserr (ZILCH)
{
    return ( e_report ("phasing error") );
}

/* ******************************************************************** *
 * RefCreat() - Creates a new ref_xtrnal                                *
 * Passed : (1) mylabel - struct symblstr                               *
 *          (2) rtyp  - type to store in RfTyp                          *
 *          (3) adrs  - offset to for r_ofset                           *
 *          This reference will replace whatever ref is stored in       *
 *          mylabel->reftbl and that ref will become this ref's NxtRef  *
 * ******************************************************************** */

void
#ifdef __STDC__
RefCreat (struct symblstr *mylabel, int rtyp, int adrs)      /*   L21c9()  */
#else
RefCreat (mylabel, rtyp, adrs)    /*   L21c9()  */
     struct symblstr *mylabel;
     char rtyp;
     int adrs;
#endif
{
    register struct ref_xtrnal *newref;

    newref = (struct ref_xtrnal *) getmem (sizeof (struct ref_xtrnal));
    newref->RfTyp = rtyp;
    newref->r_offset = adrs;
    newref->NxtRef = mylabel->reftbl;
    mylabel->reftbl = newref;
}


char
SkipSpac (ZILCH)
{
    register char *pt = SrcChar;

    while (isblank(*pt))
    {
        pt++;
    }

    return *(SrcChar = pt);   /* Reset SrcChar to this char */
}

/* ******************************************************************** *
 * WlkTreLft() - For symblstr passed as a parameter, goes to the right  *
 *              leg and then proceeds to the leftmost end of that leg   *
 * ******************************************************************** */

struct symblstr *
#ifdef __STDC__
WlkTreLft (struct symblstr *thistree)
#else
WlkTreLft (thistree)
     struct symblstr *thistree;
#endif
{
    register struct symblstr *ntree = thistree->right;

    if (thistree->locmsk & LOC1BYT)
    {
        while (ntree->left)
        {
            ntree = ntree->left;
        }
    }

    return ntree;
}

/* ******************************************************************** *
 * NewLabel() - Create a new label structure.                           *
 *      A label tree is set up such that for any left-member, if it has *
 *      no right-member, its right points upward to the label which     *
 *      will give the next right branch for a parse of the whole tree   *
 * ******************************************************************** */

static struct symblstr *
#ifdef __STDC__
NewLabel (char *newnam,                 /* string name for label            */
          struct symblstr **smbl_up,    /* previous member in tree          */
          int cmp_cas)                  /* whether new mbr is left or right */
#else
NewLabel (newnam, smbl_up, cmp_cas)
     char *newnam;
     struct symblstr **smbl_up;
     int cmp_cas;
#endif
{
    register struct symblstr *nwtree;
    struct symblstr *_prevtree;

    /* I don't believe that d078f will ever be nonzero, as
     * there are no other refs to d078f anywhere else besides here
     * It seems that d078f is a list of freed symblstr's
     */

    if (d078f)
    {
        nwtree = d078f;
        d078f = d078f->left;
#ifdef COCO
        nwtree->left = nwtree->reftbl = nwtree->smbltyp =
            nwtree->locmsk = nwtree->s_ofst = 0;
#else
        memset (nwtree, 0, sizeof (struct symblstr));   /* easier */
#endif
    }
    else
    {
        /* It seems that it would be advisable to null out the structure,
         * but it wasn't in original code.  I suppose all members are assigned
         * to in all cases.  In non-coco versions, calloc() is used.
         */

        nwtree = (struct symblstr *) getmem (sizeof (struct symblstr));
    }

    /* If we are not just starting the member's tree */

    if ( (_prevtree = *smbl_up) )
    {
        if (cmp_cas < 0)
        {
            nwtree->right = _prevtree;
            _prevtree->left = nwtree;
        }
        else
        {
            /* Insert new tree to right of _prevtree and before
             * the old _prevtree->right if one exists
             */

            nwtree->right = _prevtree->right;
            _prevtree->right = nwtree;
            _prevtree->locmsk |= LOC1BYT;
        }
    }
    else
    {
        *CurSmblTree = nwtree;
        nwtree->right = 0;
    }

    strcpy (nwtree->symblnam, newnam);
    return nwtree;
}

/* ************************************************************************ *
 * FndTreeSlot() -Trace Tree down to find an empty space at the right       *
 *              place.                                                      *
 * Returns: strcmp() result, it will be 0 if a match                        *
 *          A summation of the chars in new_nam is performed to determine   *
 *            which tree the label belongs to.                              *
 *          If this particular tree is still empty, cmp_case = 1 is         *
 *            returned.                                                     *
 *          If name is not found, dst_smbl (parm2) will point to the label  *
 *            which is just above the location where this entry belongs.    *
 *          If name is found, *dst_smbl will be that smblstr                *
 *          The return will determine whether new member will be placed     *
 *            on left or right.                                             *
 * ************************************************************************ */

static int
#ifdef __STDC__
FndTreeSlot (char *new_nam, struct symblstr **dst_smbl)
#else
FndTreeSlot (new_nam, dst_smbl)
     char *new_nam;
     struct symblstr **dst_smbl;
#endif
{
    int cmp_case = 1;
    register struct symblstr *_nowentry;

    {
        register char *__chr = new_nam;
        int __sum = 0;

        /* Get the sum of the chars in new_nam
         * This determines the tree in SmblTreArr that is used.
         * There are 64 trees.
         * This might be needed for speed on the COCO, but for
         * the faster cross compiler machines, a single tree
         * would no doubt be quite satisfactory.
         */

        do
        {
            __sum += *__chr;
        } while (*(++__chr) != '\0');

        CurSmblTree = &SblTrePtr[__sum & 0x3f];
    }

    /* Do only if this tree already has an entry */

    if ( (_nowentry = *dst_smbl = *CurSmblTree) )
    {
        do
        {
            cmp_case = strcmp (new_nam, ((*dst_smbl = _nowentry)->symblnam));

            if ((cmp_case < 0))
            {
                _nowentry = _nowentry->left;
            }
            else
            {
                /* the following test for cmp_case works, but,
                 * since "<" was tested above, it is simply "==" */

                if ((cmp_case <= 0) || ! ((_nowentry->locmsk) & LOC1BYT))
                {
                    break;
                }

                _nowentry = _nowentry->right;
            }
        } while (_nowentry);
    }

    return cmp_case;
}

void *
#ifdef __STDC__
getmem (int memreq)
#else
getmem (memreq)
     int memreq;
#endif
{
    void *memgot;

# ifndef COCO
    if ((memgot = calloc (memreq, 1)))
#else
    if ((memgot = sbrk (memreq)) != -1)
#endif
    {
        return memgot;
    }
    else
    {
        errexit ("symbol table overflow");
    }

    return 0;   /* This never happens.. but it keeps gcc quiet */
}
