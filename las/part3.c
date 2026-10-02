/* ******************************************************** *
 * Part3.c                                                  *
 *                                                          *
 * $Id::                                                $   *
 * ******************************************************** */

#include "rma.h"

int is_pass1 (ZILCH);

static int poprnd (
#ifdef __STDC__
    int
#endif
);

#ifdef COCO
#   define strchr(a, b) index(a, b)
#endif

int
l_brnch (ZILCH)

    /* lbra & lbsr */
{
    NumBytes = 3;
    return (brnch_size ());
}

int
do_aim (ZILCH)
    /* aim, oim, tim, eim  */
{

    AIMFlg = 1;

    if (cc_stuff ())
    {                           /* first, get memory byte */
        if (*SrcChar == ',')
        {
            ++SrcChar;
            ++opr_ptr; /* Bump union ptr by one byte */
            /* to allow for extra byte    */
            return bit_stuf ();
        }
    }
    return 0;
}

int
do_band (ZILCH)
{
    char bitpchr,
      namhold[SYMLEN],
     *sr_tmp;
    unsigned bitpos;
    int count;
    char *boff = "01234567CVZNIHFE";

    NumBytes = 4;

    switch (getreg (0))
    {
        case 0x08:             /* accA */
            opr_ptr[0] |= 0x40;
            break;
        case 0x09:             /* accB */
            opr_ptr[0] |= 0x80;
            break;
        case 0x0a:             /* CC   */
            break;              /* default */
        default:
            return bd_rgnam ();
    }

    if (*SrcChar == '.')
    {
        bitpchr = *(++SrcChar);

        if (islower ( (int)bitpchr) )
        {
            bitpchr = _toupper (bitpchr);
        }

        if ( (bitpos = (int) strchr (boff, bitpchr)) )
        {
            ++SrcChar;
            bitpos -= (int) boff;   /* make bitpos offset into string */

            if ( ((opr_ptr[0]) && (bitpos < 8)) || ! (opr_ptr[0]) )
            {
                if (bitpos > 7)
                {
                    bitpos -= 8;
                }

                opr_ptr[0] |= bitpos;

                if (*SrcChar == ',')
                {
                    ++SrcChar;

                    if (*SrcChar == '<')
                    {
                        ++SrcChar;
                    }

                    count = 0;

                    while ( (count < SYMLEN - 1) && (namhold[count] = *(SrcChar++)) &&
                            (namhold[count] != '.')                          )
                    {
                        ++count;
                    }

                    namhold[count] = '\0';
                    sr_tmp = --SrcChar; /* Tmp save SrcChar */
                    SrcChar = namhold;

                    if (getNmbr () == 0)
                    {
                        SrcChar = sr_tmp;
                        return (e_report ("bad number for band operation"));
                    }

                    SrcChar = sr_tmp;

                    if ((NmbrInt > -256) && (IndxVal < 256))
                    {
                        IndxVal = NmbrInt;
                        rmcoderefs ();
                        genreflist ();
                        opr_ptr[1] = IndxVal;

                        if (*SrcChar == '.')
                        {
                            ++SrcChar;

                            if ( (bitpos = (int) strchr (boff, *SrcChar)) )
                            {
                                ++SrcChar;
                                bitpos -= (int) boff;

                                if (bitpos < 8)
                                {
                                    opr_ptr[0] |= (bitpos << 3);
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return ilAdrMod ();
}

int
cc_stuff (ZILCH)
    /* orcc, andcc, & cwai */
{
    NumBytes = 2;

    if (immediat ())
    {
        LOC1Msk |= LOC1BYT;
        proc_byte ();
        opr_ptr[0] = NmbrInt;
        return 1;
    }

    return (ilAdrMod ());
}

int
q_immed (ZILCH)
    /* does q register (needed for long usage on immediate mode */
{
    if (immediat ())
    {
        NumBytes += 4;
        do_long ();
#ifdef COCO
        opr_ptr->q_long = NmbrInt;
#else
        storInt (opr_ptr, (NmbrInt >> 16) & 0xffff);   /* save 16 MSB */
        storInt (&(opr_ptr[2]), NmbrInt & 0xffff);     /* and 16 LSB */
#endif
        return (not_immed ());
    }
    else
    {
        MLCod.prebyte = 0x10;   /* all other modes require MLCod.prebytee */
        CmdCod = &MLCod.prebyte;
        ++NumBytes;
        MLCod.opcode = 0xcc;    /* Opcode is also different */
        return (regofst ());
    }
}

int
do_md (ZILCH)
{
    if ( ! immediat ())
    {
        return 0;
    }

    ++NumBytes;
    proc_byte ();
    opr_ptr[0] = NmbrInt;
    return 1;
}

int
int_stuf (ZILCH)
    /* add's etc for 16-bit stuff */
{
    if (immediat ())
    {
        NumBytes += 2;
        proc_int ();
#ifdef COCO
        opr_ptr->C_Int[0] = NmbrInt;
#else
        storInt (opr_ptr, NmbrInt);
#endif
        return (not_immed ());
    }
    else
    {
        return (regofst ());
    }
}

         /* add, sub, cmp, ld */
         /* for special cases that include "E" & "F" regs
            if "E" or "F" not found, falls through to chr_stuf() */

int
chr_ef (ZILCH)
    /* 8-bit processes */
{
    int lngreg = 0;

    switch (*CmdNamEnd)
    {
        case 'e':
            *(--CmdCod) = 0x11;
            ++NumBytes;
            break;
        case 'f':
            *(--CmdCod) = 0x11;
            ++NumBytes;
            MLCod.opcode |= 0x40;
            break;
        default:
            return (chr_stuf ());       /* then check for standard regs */
    }

    return poprnd (lngreg);
}

         /* sbc,and, bit, st, eor, adc, etc */
int
chr_stuf (ZILCH)
    /* 8-bit processes */
{
    int lngreg = 0;

    switch (*CmdNamEnd)
    {
        case 'd':
            *(--CmdCod) = 0x10;
            ++NumBytes;
            lngreg = 1;
            break;
        case 'b':
            MLCod.opcode |= 0x40;
            break;
        case 'a':
            break;
        default:
            return (bd_rgnam ());
    }

    return poprnd (lngreg);
}

/* ******************************************************************** *
 * poprand() - Process the operand ref_ent                              *
 * ******************************************************************** */

static int
#ifdef __STDC__
poprnd (int notbyte)
#else
poprnd (notbyte)
     int notbyte;
#endif
{
    if (immediat ())
    {
        ++NumBytes;

        if (notbyte)
        {
            ++NumBytes;
            proc_int ();
#ifdef COCO
            opr_ptr->C_Int[0] = NmbrInt;
#else
            storInt (opr_ptr, NmbrInt);
#endif
        }
        else    /* Single byte */
        {
            LOC1Msk |= LOC1BYT;
            proc_byte ();
            opr_ptr[0] = NmbrInt;
        }

        return (not_immed ());
    }
    else
    {
        return (regofst ());
    }
}

int
do_divd (ZILCH)
{
    return poprnd (0);
}

int
bit_stuf (ZILCH)
{
    if (  (MLCod.opcode != 1)           &&
        ! AIMFlg && (MLCod.opcode != 5) &&
          (MLCod.opcode != 0x0b)        &&
          (MLCod.opcode != 0x0e)        &&
          *CmdNamEnd                       )
    {
        switch (*CmdNamEnd)
        {
            case 'w':
                /* If not neg, asr, lsl, asl */
                if ( MLCod.opcode        &&
                     (MLCod.opcode != 7) &&
                     (MLCod.opcode != 8)    )
                {
                    MLCod.opcode |= 0x50;
                    *(--CmdCod) = 0x10;
                    ++NumBytes;
                }

                break;
            case 'd':
                MLCod.opcode |= 0x40;
                *(--CmdCod) = 0x10;
                ++NumBytes;
                break;
            case 'a':
                MLCod.opcode |= '\x40';
                break;
            case 'b':
                MLCod.opcode |= '\x50';
                break;
            case 'e':
            case 'f':
                if ((MLCod.opcode == 3) || (MLCod.opcode >= 0x0a))
                {
                    MLCod.opcode |= ((*CmdNamEnd == 'e') ? 0x40 : 0x50);
                    *(--CmdCod) = 0x11;
                    ++NumBytes;
                    break;
                }
            default:
                return (bd_rgnam ());
        }

        return 1;
    }

    if (regofst ())
    {
        if (MLCod.opcode & 0xe0)
        {
            MLCod.opcode |= 0x40;
        }

        return 1;
    }

    return 0;
}

int
no_opcod (ZILCH)
    /* No opcode, just a null subroutine */
{
    return 1;
}

int
lea_s (ZILCH)
    /* leax, leas, etc. */
{
    if (regofst ())
    {
        return 1;
    }
    else
    {
        return (ilAdrMod ());
    }
}

int
do_tfm (ZILCH)
{
    int rg0,
      rg1;
    char nxtc;
    int sgn0,
      sgn1;

    sgn0 = sgn1 = 0;
    NumBytes += 1;

    if (((rg0 = getreg (0)) >= 0x100) && (rg0 <= 0x104) &&
        (((nxtc = *SrcChar) == ',') || (nxtc == '+') || (nxtc == '-')))
    {
        if (nxtc != ',')
        {
            ++SrcChar;
            if (nxtc == '+')
            {
                ++sgn0;
            }
            else
            {
                --sgn0;
            }
            if (*SrcChar != ',')
            {
                return bdrglist ();
            }
        }
        
        ++SrcChar;
        
        if (((rg1 = getreg (0)) >= 0x100) && (rg1 <= 0x104))
        {
            if (((nxtc = *SrcChar) == '+') || (nxtc == '-'))
            {
                ++SrcChar;
                if (nxtc == '+')
                {
                    ++sgn1;
                }
                else
                {
                    --sgn1;
                }
            }
            
            if (sgn0 || sgn1)
            {
                if ((sgn0 > 0) && (sgn1 > 0))
                {
                    MLCod.opcode = 0x38;
                }
                else
                {
                    if ((sgn0 < 0) && (sgn1 < 0))
                    {
                        MLCod.opcode = 0x39;
                    }
                    else
                    {
                        if ((sgn0 > 0) && (sgn1 == 0))
                        {
                            MLCod.opcode = 0x3a;
                        }
                        else
                        {
                            if ((sgn0 == 0) && (sgn1 > 0))
                            {
                                MLCod.opcode = 0x3b;
                            }
                            else
                            {
                                return bdrglist ();
                            }
                        }
                    }
                }
            }

            opr_ptr[0] = (rg0 << 4 | rg1);
            return 1;
        }
    }

    return bdrglist ();
}

int
tfr_exg (ZILCH)
{
    int rg0,
        rg1;

    NumBytes = 2;
    
    if ( (rg0 = getreg (0)) && (*SrcChar == ',') )
    {
        ++SrcChar;

        if ( (rg1 = getreg (0)) )
        {
            if (((rg0 ^ rg1) & 8) == 0)
            {
                opr_ptr[0] = (rg0 << 4 | rg1);
                return 1;
            }
            else
            {
                return (e_report ("register size mismatch"));
            }
        }
    }

    return bdrglist ();
}

int
do_addr (ZILCH)
{
    int rg0,
      rg1;

    NumBytes = 3;  /* Orig code ws 2 - did we do this for 6309 ? */
    
    if ((rg0 = getreg (0)) && (*SrcChar == ','))
    {
        ++SrcChar;
    
        if ( (rg1 = getreg (0)) )
        {
            if (rg1 != 0x0a)
            {
                opr_ptr[0] = (rg0 << 4 | rg1);
                return 1;
            }
            else
            {
                return (e_report ("storing to CCR"));
            }
        }
    }

    return bdrglist ();
}

int
stk_stuf (ZILCH)
    /* Psh.. pul.. on/off stacks */
{
    int var2;

    NumBytes = 2;
    SkipSpac ();
    
    while ((var2 = getreg (1)))
    {
        opr_ptr[0] |= var2;
    
        if (*SrcChar != ',')
        {
            return 1;
        }

        ++SrcChar;
    }

    return (bdrglist ());
}

/* ******************************************************************** *
 * lb_() - All lb's except "lbra" and "lbsr"                            *
 * ******************************************************************** */

int
lb_ (ZILCH)
    /* Long branches */
{
    NumBytes = 4;

    if ( (Cod_Ent = FindCmd (&(LabelName[1]), S_branch, SBsr_End)) )
    {
        MLCod.opcode = Cod_Ent->op_cod;
        return (brnch_size ());               /* in part4.c */
    }
    else
    {
        return (e_report ("bad mnemonic"));
    }
}

/* ******************************************************** *
 * do_brnch() - Handles short branches                      *
 * ******************************************************** */

int
do_brnch (ZILCH)
{
    int _jmpsize;

    NumBytes = 2;

    if (getNmbr ())
    {
        if (Pass2)
        {
            _jmpsize = (int)NmbrInt - (unsigned) CodeSize - 2;
            LOC1Msk |= RELATIVE | LOC1BYT;
            rmcoderefs ();

            if (d0791 == OptBPtr)
            {
                if ((_jmpsize > 127) || (_jmpsize < -128))
                {
                    _jmpsize = -2;
                    e_report ("branch out of range");
                }
            }
            else
            {
                genreflist ();
            }

#ifdef COCO
            MLCod.shrtlng.f_byt = _jmpsize;
#else
            opr_ptr[0] = _jmpsize;
#endif
        }

        return 1;
    }

    return 0;
}

/* equset(): handles fcc's, fcb's , etc */

int
equset (ZILCH)
{
    NumBytes = 0;
    return ( (*CnstTbl[(int)(MLCod.opcode)]) () );
}

int
asm_dirct (ZILCH)
    /* setdp, org, etc */
{
    NumBytes = 0;
    return ( (*d03b6[(int)(MLCod.opcode)]) () );
}

/* ************************************************************ *
 * do_ifs() - Handles if statememts.                            *
 * ************************************************************ */

int
do_ifs (ZILCH)
{
    NumBytes = 0;
    ++_If_Dpth;

    if (_if_tbl[(int)(MLCod.opcode)] != is_pass1)
    {
        if (getNmbr () == 0)
        {
            return 0;
        }

        if (d0791 != OptBPtr)
        {
            return (ilExtRef ());
        }
    }

    if ((*_if_tbl[(int)(MLCod.opcode)]) () == 0)
    {
        ++FalseIf;     /* Inc FalseIf on FALSE */
    }

    doList &= (C_Flg > 0);
    return 1;
}

int
ilExtRef (ZILCH)
{
    return (e_report ("illegal external reference"));
}

/* ******************************************** *
 * do_endc() - Handle "endc" or "else"          *
 *         "endc": reduce _If_Dpth and FalseIf  *
 *         "else": invert case of FalseIf       *
 *                                              *
 *         Sets doList to proper state          *
 * ******************************************** */

int
do_endc (ZILCH)
{
    NumBytes = 0;

    if (_If_Dpth)
    {
        if (MLCod.opcode == 0)  /* "endc" */
        {
            --_If_Dpth;

            if (FalseIf)
            {
                --FalseIf;
            }
        }
        else        /* then it's "else" */
        {
            (FalseIf) ? --FalseIf : ++FalseIf;  /* Invert FalseIf */
        }

        doList &= (C_Flg > 0);
        return 1;
    }

    return (e_report ("conditional nesting error"));
}

int
_psect (ZILCH)
{
    codsetup ();    /* Set up now to allow reading of real commands */
    do_psect ();
    CodeSize = 0;
    I_dpd = U_dpd = I_data = U_data = 0;
    return 1;
}

/* ************************************************************ *
 * codsetup() - Sets up for code data within a psect            *
 * ************************************************************ */

int
codsetup (ZILCH)
{
    NumBytes = 0;
    CodTbBgn = S_realcmds;
    CodTbEnd = SLbrEnd;
    jmp_ptr = codtabl;
    CurLclRef = &LRefs_Code;
    TypeBasic = CODENT;
    DLocField = RELATIVE | CODLOC | DIRENT; /* DIRENT is CONENT?? */
    TmpLocMsk = CODLOC;
    S_Addrs = &CodeSize;
    CurntOrg = 0;
    return 1;
}

/* Sets up to do a csect section */

int
csectsetup (ZILCH)
{
    NumBytes = 0;
    CodTbBgn = S_Rmb;
    CodTbEnd = S_Rmbend;
    jmp_ptr = CsecTbl;
    CurLclRef = 0;
    TypeBasic = CODENT | DIRENT;
    DLocField = (DLocField & DIRENT) | RELATIVE;
    S_Addrs = (int *) 0;
    TmpLocMsk = 0;
    Adrs_Ptr = CurntOrg = &CsectOrg;
    CsectOrg = 0;

    if (SkipSpac ())
    {
        if (getNmbr () == 0)
        {
            return 0;
        }

        if (OptBPtr != d0791)
        {
            return ilExtRef ();
        }

        CsectOrg = NmbrInt & 0xffff;
    }

    return 1;
}

/* Sets up to do a vsect section if OUTSIDE a psect */

int
vsectsetup (ZILCH)
{
    NumBytes = 0;
    CodTbBgn = S_Rmb3;
    CodTbEnd = Rmb3End;
    jmp_ptr = VsecTbl;
    TypeBasic = 0;

    if (is_dp ())
    {
        TypeBasic = DIRENT;
    }

    return 1;
}

int
end_csect (ZILCH)
{
    Adrs_Ptr = CurntOrg;

    if (DLocField & DIRENT)
    {
        return (codsetup ());
    }
    else
    {
        return (precodeset ());
    }
}

               /* sets up a macro definition */

int
setmac (ZILCH)
{
    if (MacDefining)
    {
        return (nstmacdf ());
    }
    else
    {
        if (Label == 0)
        {
            return (e_report ("label missing"));
        }
        else
        {
            l2069 (8);      /* LOC1BYT ?? */

            if (PASS1)
            {
                createMac ();
            }

            return (MacDefining = 1);
        }
    }
}

int
endmac (ZILCH)
{
    return (e_report ("ENDM without MACRO"));
}
