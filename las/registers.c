/* ************************************************************ *
 * Part 4 of "rma" assember                                     *
 *                                                              *
 * $Id::                                                    $   *
 * ************************************************************ */

#define NEED_CTYPE
#include "rma.h"

#ifdef COCO
char skparrow ();
#endif

#define INDX_BYT opr_ptr[0]
/*#define AIM_INDXD if (AIMFlg) {C_Oprnd.opers[0] |= 0x60;}*/
#define AIM_INDXD

static int ckpcr (ZILCH);
static int l11be (ZILCH);
static int indx_setup ();
static int not_bracket ();
static int not_pcr ();
static int l11e8 ();
static int l1215 ();
static int l1584 ();

struct regstrct
{
    char regnam[2],
      RType;
};

struct regstrct RegCode[] = {
    {{'A', '\0'}, 0x02}, {{'B', '\0'}, 0x04}, {{'C', 'C'}, 0x01},
    {{'D', 'P'}, 0x08}, {{'0' & 0xdf, '\0'}, 0}, {{'\xfe', '\0'}, 0},
    {{'E', '\0'}, 0x00}, {{'F', '\0'}, 0x00},
    {{'D', '\0'}, 0x06}, {{'X', '\0'}, 0x10}, {{'Y', '\0'}, 0x20},
    {{'U', '\0'}, 0x40}, {{'S', '\0'}, 0x40}, {{'P', 'C'}, 0x80},
    {{'W', '\0'}, 0x00}, {{'V', '\0'}, 0x00}
};

         /* Count of entries in "regstrct" array (6309) */
#define REGCOUNT sizeof(RegCode)/sizeof(RegCode[0])

int
immediat (ZILCH)
{
    if (SkipSpac () == '#')
    {
        ++SrcChar;

        if (LwasmMode)
        {
            SkipSpac ();    /* lwasm allows "#   value" */
        }

        return 1;
    }
    return 0;
}

int
not_immed (ZILCH)
{
    if (jsrOfst & NO_IMMED)
    {
        NumBytes = 3;
        return (ilAdrMod ());
    }
    else
    {
        return 1;
    }
}

int
brnch_size (ZILCH)
{
    LOC1Msk |= RELATIVE;

    if (proc_int ())
    {
#ifndef COCO
        short _jmpsiz;

        _jmpsiz = NmbrInt - (int) PC_BASE - NumBytes;
        storInt (opr_ptr, _jmpsiz);

        if (_jmpsiz > 127 || _jmpsiz < -128)
#else
        if (((opr_ptr->C_Int[0] = (int) NmbrInt - PC_BASE - NumBytes) > 127)
            || opr_ptr->C_Int[0] < -128)
#endif
        {
            LongJmp[0] = 1;
        }
        else
        {
            LongJmp[0] = 0;
        }

        return 1;
    }

    return 0;
}

/* ************************************************************************ *
 * getreg(PhsPul) = l102b() : Parses the Register name structure to match   *
 *              up  register bit patterns                                   *
 * Returns: if not PshPul                                                   *
 *                case not register (X,Y,U,S,PC), offset into table | 8     *
 *                case register, offset-4 | $100                            *
 *          if PshPul : push/pull code for register (0 if invalid)          *
 * ************************************************************************ */

int
getreg (PshPul)
     int PshPul;
{
    int regptr;
    char var2;

    regptr = 0;

    while (regptr < REGCOUNT)
    {
        if (RegCode[regptr].regnam[0] == _toupper (*SrcChar))
        {
            /* First char matched */
            if ((var2 = RegCode[regptr].regnam[1]))
            {
                /* second char not null, check if it matches */
                if (_toupper (SrcChar[1]) == var2)
                {
                    ++SrcChar;  /* bump ptr for extra char */
                }
                else
                {
                    ++regptr;   /* Bump regptr and get next */
                    continue;
                }
            }

            ++SrcChar;

            /* lwasm: "0" is the 6309's zero register (code $C) in
             * tfr/exg, also for a 6809 ("tfr 0,dp" in grfdrv256). The
             * table holds it as _toupper leaves it: '0' & 0xdf */

            if ((regptr == 5) || ((regptr == 4) && ( ! LwasmMode || PshPul)))
            {
                return 0;       /* Blank areas */
            }

            if (PshPul == 0)
            {
                if (regptr < 8)
                {               /* Not register (X,Y,U,S,PC) */
                    return (regptr | 8);
                }
                else
                {
                    return ((regptr - 8) | 0x0100);
                }
            }

            /* No psh/pul for E,F,W or V
               not needed?? RType returns correct value??  */
/*         if ( (regptr!=6) && (regptr!=7) && (regptr < 14) ) {*/
            return (RegCode[regptr].RType);
/*         }
         else {
            return 0;
         }*/
        }

        ++regptr;               /* bump to next positin */
    }

    return 0;                   /* No match found */
}

/* ******************************************************************** *
 * regofst() - Most code-parse functions, if they are not immediate     *
 *             fall thorough to this function here.                     *
 * ******************************************************************** */

int
regofst (ZILCH)      /*              l10d1()                 */
{                    /* Parses indexed mode for A/B/D offset */
    char c;

    if ( (HadBrkt = ((c = SkipSpac ()) == '[')) )
    {
        c = *(++SrcChar);
    }

    c = skparrow (c);
    ExplArrow = HadArrow;               /* "<" or ">" written (sizing.c) */

    if (c == ',')                       /* ",R" */
    {
        AIM_INDXD;
        return l1215 ();
    }

    /* "a,x" etc. is accumulator-offset indexing. In lwasm, a size
     * prefix means an expression follows: "std <d,s" is the symbol d,
     * an 8-bit offset, not D-register offset. */

    if ((SrcChar[1] == ',') && ! (LwasmMode && HadArrow))
    {
        switch (_toupper (c))
        {
            case 'A':
                INDX_BYT = 0x86;
                AIM_INDXD;
                return ((_toupper (SrcChar[2]) == 'W') ? ilAdrMod() : ckpcr());
            case 'B':
                INDX_BYT = 0x85;
                AIM_INDXD;
                return ((_toupper (SrcChar[2]) == 'W') ? ilAdrMod() : ckpcr());
            case 'D':
                INDX_BYT = 0x8b;
                AIM_INDXD;
                return ((_toupper (SrcChar[2]) == 'W') ? ilAdrMod() : ckpcr());
            case 'E':
                INDX_BYT = 0x87;
                AIM_INDXD;
                return ((_toupper (SrcChar[2]) == 'W') ? ilAdrMod() : ckpcr());
            case 'F':
                INDX_BYT = 0x8a;
                AIM_INDXD;
                return ((_toupper (SrcChar[2]) == 'W') ? ilAdrMod() : ckpcr());
            case 'W':
                INDX_BYT = 0x8e;
                AIM_INDXD;
                return ((_toupper (SrcChar[2]) == 'W') ? ilAdrMod() : ckpcr());
        }
    }

    {
        /* lwasm keeps a 5-bit zero offset when the offset is written as
         * exactly "0" (sta 0,u = A7 40); any other zero (a symbol, 1-1,
         * $0) becomes the no-offset form (sta ,u = A7 C4), as RMA always
         * does. Remember which it was before the expression is read. */

        register char *_p = SrcChar;

        while (isblank (*_p))
        {
            ++_p;
        }

        IdxLitZero = 0;

        if (LwasmMode && (*_p == '0'))
        {
            ++_p;

            while (isblank (*_p))
            {
                ++_p;
            }

            IdxLitZero = (*_p == ',');
        }
    }

    getNmbr ();
    IndxVal = NmbrInt;

    if (*SrcChar == ',')
    {
        AIM_INDXD;
        return l156b ();
    }
    else
    {
        /* if  no '[' && dp referencing */

        if ((HadBrkt == 0) && (HadArrow >= 0))
        {
            if ((HadArrow > 0) || ((IndxVal >> 8) == d0027))
            {
                /* if dp referencing || == DP_blk */
                return l11e8 ();
            }
        }
/*      if ( AIMFlg ) {
         C_Oprnd.opers[0] |= 0x70;
      }*/

        return l11be ();
    }
}

/* **************************************************************** *
 * skparrow() - If we are at an arrow ">" or "<", set HadArrow and  *
 *              bump SrcChar to next character, and return that     *
 *              char.  If no arrow, return char passed as a param.  *
 * HadArrow set to :  -1 = extended, 1 = direct, 0 otherwise        *
 * **************************************************************** */

char
skparrow (ch)
     char ch;
{
    if (ch == '>')
    {
        HadArrow = -1;
    }
    else
    {
        if (ch == '<')
        {
            HadArrow = 1;
        }
        else
        {
            HadArrow = 0;
        }
    }

    if (HadArrow)
    {
        return (*(++SrcChar));
    }
    else
    {
        return (ch);
    }
}

static int
ckpcr (ZILCH)
{
    SrcChar += 2;

    return ( not_pcr () ? indx_setup () : ilIdxReg () );
}

static int
l11be (ZILCH)
{
    NumBytes += 2;

    if (HadBrkt)
    {
        INDX_BYT = 0x9f;        /* Let "C_Oprnd" be struct ref_ent */
#ifdef COCO
        opr_ptr->shrtlng.f_int = IndxVal; /*   for now                   */
#else
        storInt (&(opr_ptr[1]), IndxVal);
#endif
        return (indx_setup ());
    }
    else
    {
        if (LwasmMode && ! ExplArrow)
        {
            sz_other ();        /* lwasm may leave it unsettled (sizing.c) */
        }

        rmcoderefs ();
        genreflist ();
#ifdef COCO
        opr_ptr->C_Int[0] = IndxVal;
#else
        storInt (opr_ptr, IndxVal);
#endif
        MLCod.opcode |= 0x30;
        return 1;
    }
}

static int
l11e8 ()
{
    if (LwasmMode && ! ExplArrow)
    {
        sz_other ();            /* lwasm may leave it unsettled (sizing.c) */
    }

    ++NumBytes;
    LOC1Msk |= LOC1BYT;
    rmcoderefs ();
    genreflist ();
    INDX_BYT = IndxVal;

    if (MLCod.opcode & 0xf0)
    {
        MLCod.opcode |= 0x10;
    }

    return 1;
}

/* **************************************************************** *
 * l1215() - Handles ,R indexing                                    *
 * **************************************************************** */

static int
l1215 ()
{
    ++SrcChar;
    IndxVal = 0;

    /* Predecrement */

    if (*SrcChar == '-')
    {
        ++SrcChar;

        if (*SrcChar == '-')
        {
            ++SrcChar;
            INDX_BYT = 0x83;    /*  ,--R */
        }
        else
        {
            not_bracket ();
            INDX_BYT = 0x82;    /*  ,-R  */
        }

        /* We're at register name now, get mode if legal */

        if (not_pcr ())
        {
            return (indx_setup ());
        }
        else
        {
            return (ilIdxReg ());   /* Can't predecrement PC */
        }
    }
    else
    {
        if (not_pcr () == 0)    /* If it's PC, handle it and return */
        {
            return pc_rel ();
        }

        if (*SrcChar == '+')
        {
            ++SrcChar;

            if (*SrcChar == '+')
            {
                ++SrcChar;

                if ( ! Is_W)
                {
                    INDX_BYT |= 0x81;   /*   ,R++ */
                }
                else
                {
                    INDX_BYT |= 0x40;
                }
            }
            else
            {
                if (Is_W)       /* 6309 specific */
                {
                    return 0;
                }

                not_bracket ();
                INDX_BYT |= 0x80;       /*   ,R+  */
            }

            return (indx_setup ());
        }
        else        /* It's ,R without inc/dec */
        {
            return (l1584 ());
        }
    }

    /* Was: return 0, I suspect this was causing clr ,-s and others to be incorrect - BGP */
    return 1;
}

/* ************************************************************ *
 * not_bracket() - Returns 1 if not in brackets.                *
 *                 Returns 0 if in brackets after reporting err *
 * ************************************************************ */

static int
not_bracket ()
{
    return ( HadBrkt ? ilAdrMod () : 1 );
}

int
ilAdrMod ()
{
    return (e_report ("illegal addressing mode"));
}

/* ******************************************************** *
 * not_pcr() - Handle setup for all registers except PC     *
 * ******************************************************** */

static int
not_pcr ()                      /* process 0,[reg] */
{
    char _indx_msk = 0xff;

    switch (_toupper (*SrcChar))
    {
        case 'X':
            _indx_msk = '\0';
            break;
        case 'Y':
            _indx_msk = '\x20';
            break;
        case 'U':
            _indx_msk = '\x40';
            break;
        case 'S':
            _indx_msk = '\x60';
            break;
        case 'W':              /* do most of processing here */
            Is_W = TRUE;

            /* ,-W  not allowed */
            if ((INDX_BYT == '\x82'))
            {
                return 0;       /* let fall through to error condition */
            }

            if (INDX_BYT == '\x83')
            {                   /* if ,--W       */
                INDX_BYT = '\xef';
            }
            else
            {
                INDX_BYT = '\x8f';

                if (HadArrow)
                {
                    HadArrow = -1;  /* Force extended addressing if spec'd. */
                }
/*               if ( SrcChar[1] == '+' ) {
                  if ( SrcChar[2] == '+' ) {
                     SrcChar += 2;
                     INDX_BYT |= 0x40;
                  }
                  else {                   / * If ,W+, let caller fall 
                     return 0;                  / *    to error   
                  }
               }
               else {
                  if ( IndxVal || HadArrow<0 ) {
                     INDX_BYT |= 0x01;
                     NumBytes += 2;
#ifdef COCO
                     opr_ptr->shrtlng.f_int = IndxVal;
#else
                     nptr = &IndxVal;
                     nptr += sizeof(IndxVal)-2;   / * int part
                     opr_ptr[1] = *(nptr++);
                     opr_ptr[2] = *nptr;
#endif
              / * force extended addressing, no other offset modes allowed 
                     HadArrow = -1;
                  }
               }*/
            }

            ++SrcChar;
/*         if (HadBrkt) {
            if ( *(SrcChar) != ']' ) {
               return( e_report( "bracket missing" ) );
            }
            INDX_BYT ^= 0x1f;
            ++SrcChar;
            HadBrkt = 0;
         }*/
            return 1;
        default:
            return 0;
    }

    /* Cleanup for an X-Y-U-S match */

    ++SrcChar;
    INDX_BYT |= _indx_msk;
    return 1;
}

/* This was the way to get the original compilation for the above
   function (to be used after the switch, but it would not work
   without the "default" above.  For some extremely strange reason,
   the below method would always return 1, even though the assembly
   code matched the original WORD FOR WORD!!!  The above method produces
   the correct result.

   if ( var != '\xff' ) {
      ++SrcChar;
      INDX_BYT |= var;
      return 1;
   }
   else {
      return 0;
   }
*/

int
ilIdxReg ()
{
    return (e_report ("illegal index register"));
}

static int
indx_setup ()
{
    MLCod.opcode |= 0x20;
    ++NumBytes;
    ++d0021;

    if (HadBrkt)
    {
        if ( ! Is_W)
        {
            INDX_BYT |= 0x10;
        }
        else
        {
            INDX_BYT ^= 0x1f;
        }
        if (*SrcChar != ']')
        {
            e_report ("bracket missing");
        }
        else
        {
            ++SrcChar;
        }
    }

    switch (*SrcChar)
    {
        case ' ':
        case '\t':
        case '\0':
            rmcoderefs ();
            genreflist ();
            return 1;
        default:
            return (ilAdrMod ());
    }
}

/* ******************************************************************** *
 * genreflist() - Generates reference structs for refs contained in     *
 *                d0791 and cleans out the list in d0791                *
 * ******************************************************************** */

void
genreflist ()
{
    int _refdest;
    int rtyp;

    if (Pass2 && ! Settle && (OptBPtr > d0791))
    {
        if (S_Addrs == 0)
        {
            ilExtRef ();        /* report "illegal external reference" */
            /* return; */
        }
        else
        {
            _refdest = *S_Addrs + NumBytes - ((LOC1Msk & LOC1BYT) ? 1 : 2);

            /* Generate list of all refs from OptBPtr downward to d0791 */

            while (OptBPtr > d0791)
            {
                register SYMH __lbl;

                __lbl = (--OptBPtr)->RAddr;
                rtyp = sym_typ (__lbl) | LOC1Msk | OptBPtr->ETyp;

                if ((sym_msk (__lbl) & (NEGMASK | DIRENT)) == 0)
                {
                    RefExtern = 1;

                    if (sym_refs (__lbl) == 0)
                    {
                        ++ref_cnt;
                    }

                    RefCreat (__lbl, rtyp, _refdest);
                }
                else
                {
                    if ((sym_typ (__lbl) & 0x0f) == 7)       /* common ? */
                    {
                        RefCreat (__lbl, rtyp, _refdest);
                    }
                    else        /* It's a local ref */
                    {
                        ++locl_cnt;
                        RefCreat (CurLclRef, rtyp, _refdest);
                    }
                }
            }   /* end while (OptBPtr > d0791) */
        }
    }
}

/* ************************************************************************ *
 * l149b() - Only happens if reference of "baseref" is to CODE.             *
 *           Beginning at entry point in d0971 passed as "baseref", parses  *
 *           through remaining segment of d0971 for a ref_ent, in CODE      *
 *           section with ETyp having NEGMASK opposite that of "baseref"s   *
 *           "symbltyp".  If found, "baseref" and that entry in d0971 is    *
 *           deleted, and OptBPtr reduced by one, and function exits.       *
 * ************************************************************************ */

static int
#ifdef __STDC__
l149b (struct ref_ent *baseref)
#else
l149b (baseref)
     register struct ref_ent *baseref;
#endif
{
    struct ref_ent *_curref;
    SYMH _smblstr;
    int _basetyp;

    if (sym_msk (_smblstr = baseref->RAddr) & DIRENT)
    {
        _basetyp = sym_typ (_smblstr) | baseref->ETyp;
        _curref = baseref;

        /* Begin parse at baseref +  1 */

        while (++_curref < OptBPtr)
        {
            _smblstr = _curref->RAddr;

            /* Must be inside "psect" */

            if (sym_msk (_smblstr) & DIRENT)
            {
                if (((sym_typ (_smblstr) | _curref->ETyp) ^ _basetyp) == NEGMASK)
                {
                    /* Delete "baseref" and Move all ref_ent's from
                     * "baseref" + 1 to _ref down one slot, up to
                     * the matching ref/
                     */

                    while (baseref + 1 < _curref)
                    {
#ifdef COCO
                        baseref->ETyp = (baseref + 1)->ETyp;
                        baseref->RAddr = (baseref + 1)->RAddr;
#else
                        memcpy (baseref, baseref + 1, sizeof (struct ref_ent));
#endif
                        ++baseref;
                    }

                    /* Now move remainder of d0971 down 2,
                     * overwriting the match found
                     */

                    while (++_curref < OptBPtr)
                    {
                        baseref->ETyp = _curref->ETyp;
                        baseref->RAddr = _curref->RAddr;
                        ++baseref;
                    }

                    OptBPtr = baseref;      /* OptBPtr -= 1 */
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* ******************************************************************** *
 * cod_codref() - An is-<something>                                     *
 *           Is in code and ref to code ?                               *
 * ******************************************************************** */

static int
#ifdef __STDC__
cod_codref (SYMH parm)
#else
cod_codref (parm)
     register SYMH parm;
#endif
{
    if ( ((sym_typ (parm) & 0x0f) == CODENT) &&
         (sym_msk (parm) & DIRENT)            &&
         ((LOC1Msk & (RELATIVE | LOCMASK)) == (RELATIVE | CODLOC)) )   /* not DIRLOC ? */
    {
        return 1;
    }

    return 0;
}

/* **************************************************************** *
 * rmcoderefs() - Deletes code-code references, and those that are  *
 *           refs to symbols with opposite NEGMASK.  (On Pass 2)    *
 * **************************************************************** */

int
rmcoderefs ()
{
    struct ref_ent *_delref;

    if (Pass2 && (OptBPtr > d0791))
    {
        register struct ref_ent *_curntref;

        _curntref = d0791;

        while (_curntref < OptBPtr)
        {
            /* If _curntref and an opposite NEGMASK not deleted */

            if (l149b (_curntref) == 0)
            {
                /* If it's a code-code ref, delete the ref */

                if (cod_codref (_curntref->RAddr))
                {
                    _delref = _curntref;

                    /* Move current ref_ent back one slot */

                    while (++_delref < OptBPtr)
                    {
#ifdef COCO
                        (_delref - 1)->ETyp = _delref->ETyp;
                        (_delref - 1)->RAddr = _delref->RAddr;
#else
                        memcpy (_delref - 1, _delref, sizeof (struct ref_ent));
#endif
                    }

                    --OptBPtr;

                    /* If this happens, an unchanged _curntref will now be
                     * pointing to the next ref, so no bump
                     */
                }
                else
                {
                    ++_curntref;
                }
            }   /* end "if ( ! l149b() )" */
        }   /* end "while (_curntref < OptBPtr)" */
    }

    return 0;
}

int
l156b ()                        /* THIS FUNCTION IS OK */
{
    ++SrcChar;

    if ( ! not_pcr ())
    {
        return (pc_rel ());
    }
    else
    {
        return (l1584 ());
    }
}

/* **************************************************************** *
 * l1584() - Handle mmnn,R modes                                    *
 * **************************************************************** */

static int
l1584 ()
{
    if (LwasmMode && ! ExplArrow)
    {
        sz_other ();            /* lwasm may leave it unsettled (sizing.c) */
    }

    if (  (HadArrow < 0)    ||
          ((HadArrow == 0) && ((IndxVal > 127) || (IndxVal < -128)))
#ifndef COMPARE
           /* All assemblers I've seen use 16-bit mode only for extended  */
        /*|| (Is_W && IndxVal)*/
#endif
       )
    {       /* extended (16-bit) mode */
        /*if (Is_W && (IndxVal > 0))
        {
        }*/

#ifndef COCO
        storInt (&(opr_ptr[1]), IndxVal);
#else
        opr_ptr->shrtlng.f_int = IndxVal;
#endif
        NumBytes += 2;

        if ( ! Is_W)
        {
            INDX_BYT |= 0x89;   /*      mmnn,R     */
        }
        else
        {
            INDX_BYT |= 0x20;
        }

        return (indx_setup ());
    }

    if ( ! HadArrow && (IndxVal == 0) && ! IdxLitZero)
    {
        if ( ! Is_W)
        {
            INDX_BYT |= 0x84;   /*  set  0,R  to  ,R   */
        }
        else
        {
            INDX_BYT &= 0x9f;
        }

        return (indx_setup ());
    }
    else
    {       /* 8-bit offset */

        if ( (HadArrow > 0)     ||
              HadBrkt           ||
              (IndxVal > 0x0f)  ||
              (IndxVal < -(0x10))   )
        {
            opr_ptr[1] = IndxVal;
            ++NumBytes;
            LOC1Msk |= LOC1BYT;
            INDX_BYT |= 0x88;
            return (indx_setup ());
        }
        else        /* 5-bit mode */
        {
            INDX_BYT |= (IndxVal & 0x1f);
            return (indx_setup ());
        }
    }
}

/* *********************************
 * Sets up pc-relative addressing
 */

int
pc_rel ()                       /* originally  l1602() */
{
    if ((_toupper (*SrcChar) != 'P') || (_toupper (SrcChar[1]) != 'C'))
    {
        return (ilIdxReg ());   /* No match, quit */
    }

    SrcChar += 2;               /* bump past "PC" string */

    if (_toupper (*SrcChar) == 'R')      /* if "pcr" instead of "pc" */
    {
        ++SrcChar;
    }

    ++NumBytes;
    LOC1Msk |= RELATIVE;

    /* lwasm chooses the offset size itself unless "<" or ">" is written:
     * sizing.c decides, as lwasm would */

    if (LwasmMode && ! ExplArrow)
    {
        HadArrow = sz_pcr ((int) IndxVal, NumBytes + 1) ? 1 : -1;
    }

    IndxVal -= (PC_BASE + NumBytes + 1);

    if (HadArrow > 0)
    {
        if (LwasmMode && Pass2 && ! ExplArrow &&
                ((IndxVal < -128) || (IndxVal > 127)))
        {
            e_report ("8-bit pcr offset out of range (sizing.c)");
        }

        opr_ptr[1] = IndxVal;
        LOC1Msk |= LOC1BYT;
        INDX_BYT |= 0x8c;
    }
    else
    {
        ++NumBytes;
#ifndef COCO
        --IndxVal;
        storInt (&opr_ptr[1], IndxVal);
#else
        opr_ptr->shrtlng.f_int = (--IndxVal);
#endif
        INDX_BYT |= 0x8d;
    }

    return (indx_setup ());
}

/* New addition:
 * storInt() - saves an integer value in LITTLE-ENDIAN format
 */

#ifndef COCO
void
storInt (char *dst, int vlu)
{
    *dst = (vlu >> 8) & 0xff;
    dst[1] = vlu & 0xff;
}
#endif
