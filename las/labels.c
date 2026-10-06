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

static void SetArow (                  /* L204a() */
#ifdef __STDC__
SYMH parm
#endif
);

static int phaserr ();

#ifdef SYMSTATS
long StatRefs;      /* measurement build: reference records created */
#endif

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
    register SYMH _mysmbl;

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

    if (LwasmMode && sec_export (AltLbl))
    {
        LblTyp |= CODENT;   /* lwasm export pragma: export it (lwobj.c) */
    }

    _mysmbl = CurntLabel = sym_find (AltLbl);

    if (PASS1)
    {
        if ( ! _mysmbl)      /* If not an existing label */
        {
            CurntLabel = _mysmbl = sym_add (AltLbl);
        }

        if ( ! (sym_msk (_mysmbl) & RELATIVE))
        {
            sym_settyp (_mysmbl, (sym_typ (_mysmbl) & 0xf0) | WhereLoc);
            sym_setmsk (_mysmbl, sym_msk (_mysmbl) | (LblTyp));

            if (S_Addrs)
            {
                sym_setval (_mysmbl, *S_Addrs);
            }

            if (SzTrial && (S_Addrs == (InTop ? &AbsCtr : &CodeSize)))
            {
                sz_label (_mysmbl);     /* a code label (sizing.c) */
            }

            /* Count a global for the ROF header once, when first defined.
             * RMA counted at every definition, so an exported "set" symbol
             * set again was counted twice and the header promised more
             * globals than were written. */

            if ((sym_msk (_mysmbl) & 0xc6) == (RELATIVE | CODENT | DIRENT))
            {
                ++GlobCnt;
            }
        }
        else        /* If a previously-defined label */
        {
            /* I _believe_ that this condition would inevitably
             * result in an error later on when reparsing on pass 2
             */

            if ((sym_typ (_mysmbl) & 0x0f) != (CODENT | INIENT))
            {
                sym_setmsk (_mysmbl, sym_msk (_mysmbl) | (NEGMASK));
            }
        }

        return 1;
    }

    /* Pass 2 from here on out */

    if ( ! _mysmbl || ! (sym_msk (_mysmbl) & RELATIVE))
    {
        /* A label pass 1 never defined: the two passes read different
         * lines. Name it, to make the cause findable. */

        static char msg[SYMLEN + 40];

        sprintf (msg, "symbol lost in pass 2: %s", AltLbl);
        errexit (msg);
    }

    sym_setmsk (_mysmbl, sym_msk (_mysmbl) & ((~DIRLOC)));

    if ((sym_msk (_mysmbl)) & NEGMASK)
    {
        e_report ("redefined name");
        return 1;
    }
    else
    {
        if ( (S_Addrs)                                &&
             ((sym_typ (_mysmbl) & 0x0f) == TypeBasic) &&    /* Default Type */
             (sym_val (_mysmbl) != *S_Addrs)               )
        {
            phaserr ();
            sym_setval (_mysmbl, *S_Addrs);
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
    register SYMH entry;
    REFH _refptr;
    int s_type;

    entry = CurntLabel = sym_find (d0587);

    /* If name not found, create a new entry if pass 1, else error */

    if ( ! entry)
    {
        if (PASS1)
        {
            entry = CurntLabel = sym_add (d0587);
        }
        else
        {
            errexit ("new symbol in pass two");
        }
    }

    s_type = sym_typ (entry) & 0x0f;

    /* below = IS CODLOC or if not CODLOC, NOT RELATIVE */

    if ((sym_msk (entry) ^ RELATIVE) & (RELATIVE | CODLOC))
    {
        IsExtrn = 1;

        if (PASS1)
        {
            sym_setmsk (entry, sym_msk (entry) | (DIRLOC));
        }

        /* If "set" or "equ" ? */

        if ( (sym_msk (entry) & RELATIVE)   &&
             ((s_type == (CODENT | DIRENT)) || (s_type == (CODENT | INIENT)))    )
        {
            _refptr = sym_refs (entry);

            /* Populate d0791 with refs to this symbol */

            while (_refptr)
            {
                OptBPtr->ETyp = ref_typ (_refptr);

                /* FIXME : I _think the following cast is in order
                 * because I believe that r_offset for "set" and "equ"
                 * is a ptr to struct symbstr rather than address
                 */

                OptBPtr->RAddr = (SYMH)ref_off (_refptr);
                SetArow (OptBPtr->RAddr);
                ++OptBPtr;
                _refptr = ref_next (_refptr);
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
            if (sym_msk (entry) & DIRLOC)
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
        NmbrInt = sym_val (entry);
    }
    return 1;
}

static void
#ifdef __STDC__
SetArow (SYMH parm)                  /* L204a() */
#else
SetArow (parm)
     SYMH parm;
#endif
{
    register SYMH sbl = parm;

    if ( ! HadArrow)
    {
        if ((sym_typ (sbl) & (CODENT | DIRENT)) == DIRENT)
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

    if ( ! ((sym_msk (CurntLabel)) & NEGMASK))
    {
        if ((newmode != (CODENT | INIENT)) &&
                ((sym_typ (CurntLabel) & 0x0f) == (CODENT | INIENT)))
        {
            sym_setmsk (CurntLabel, sym_msk (CurntLabel) | NEGMASK);
        }
        else
        {
            sym_settyp (CurntLabel, ((sym_typ (CurntLabel) & 0xf0) | newmode));
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
    register SYMH reg = CurntLabel;
    struct ref_ent *var1;
    REFH var2;
    int _symboltype;

    _symboltype = sym_typ (reg) & 0x0f;

    if (OptBPtr == d0791)       /* If no refs to this */
    {
        if ((sym_typ (reg) & CODENT) && (sym_msk (reg) & CODLOC))
        {
            sym_setmsk (reg, sym_msk (reg) & (~CODLOC));      /* Unset CODLOC */
        }
        else
        {
            if (      (Pass2 > 0)
                 &&   (address != sym_val (reg))
                 && ! (sym_msk (reg) & NEGMASK)
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
                sym_setmsk (reg, sym_msk (reg) | (CODLOC));

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
                var2 = sym_refs (reg);    /* FIXME */
                var1 = OptBPtr;

                while (var2 && (d0791 < var1))
                {
                    --var1;

                    /* See "FIXME 0" above */

                    if ((ref_off (var2) != (int)(var1->RAddr)))
                    {
                        break;
                    }

                    var2 = ref_next (var2);    /* ERROR */
                }

                if ((var2 != 0) || (d0791 != var1) ||
                    (sym_val (reg) != (int) address))
                {
                    phaserr ();
                }
                else
                {
                    if (sym_msk (reg) & CODENT)
                    {
                        var2 = sym_refs (reg);    /* FIXME incompat. ptr type */

                        while (var2)
                        {
                            /* FIXME : cast to avoid errors.  */
                            if ( ! (sym_msk ((SYMH) ref_off (var2)) &
                                                (NEGMASK | DIRENT)) )
                            {
                                ilExtRef ();
                            }

                            var2 = ref_next (var2);    /* FIXME ERROR */
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

    sym_setval (reg, address);
}

static int
phaserr (ZILCH)
{
    return ( e_report ("phasing error") );
}

/* ******************************************************************** *
 * RefCreat() - Creates a new reference record                          *
 * Passed : (1) mylabel - the symbol                                    *
 *          (2) rtyp  - type to store in RfTyp                          *
 *          (3) adrs  - offset to for r_ofset                           *
 *          This reference will replace whatever ref is stored in       *
 *          mylabel's reftbl and that ref will become this ref's NxtRef *
 * ******************************************************************** */

void
#ifdef __STDC__
RefCreat (SYMH mylabel, int rtyp, int adrs)      /*   L21c9()  */
#else
RefCreat (mylabel, rtyp, adrs)    /*   L21c9()  */
     SYMH mylabel;
     char rtyp;
     int adrs;
#endif
{
#ifdef SYMSTATS
    ++StatRefs;
#endif
    sym_setrefs (mylabel, ref_new (rtyp, adrs, sym_refs (mylabel)));
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
    /* ibrk: from the gap between the static data and the stack, which
     * stdio's buffers share, so las's data stays in one 8K block (D37) */
    if ((memgot = ibrk (memreq)) != -1)
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
