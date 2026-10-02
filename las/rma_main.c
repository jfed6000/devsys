/* **************************************************************** *
 * rma.c first section of "RMA" assembler program                   $
 *                                                                  $
 * $Id::                                                            $
 * **************************************************************** */

#define MAIN

#include <stdlib.h>
#include <errno.h>

#ifndef COCO
#   include <unistd.h>
#endif

#define NEED_CTYPE
#include "rma.h"

char cmntlen[SYMLEN];   /* To hold " %1.xs val for comment length */

static int AsmblLin (ZILCH);

static void read_fil (ZILCH);

extern direct MACDSCR *NowMac;      /* part9.c */
extern direct int d00d8, d00da;     /* part9.c */

int
#ifdef __STDC__
main (int argc, char *argv[])
#else
main (argc, argv)
     int argc;
     char *argv[];
#endif
{
    const char **_curntsrc;

    incdirs[0] = ".";
    inccount++;

    GetOpts (argc, argv);

    if (ListFlg)
    {
        listON = ON;
    }

    LRefs_Code.smbltyp = CODENT;
    LRefs_DPD.smbltyp = INIENT | DIRENT;
    LRefs_NonDPdat.smbltyp = INIENT;
    LRefs_Code.locmsk = LRefs_DPD.locmsk = LRefs_NonDPdat.locmsk = DIRENT;
    NowMac = 0;

    if (OFlg)
    {
#ifdef WIN32    /* Unix systems handle ".b" mode, but OSK, at least doesn't */
        OutFile = f_opn (O_Nam, "wb");
#else
        OutFile = f_opn (O_Nam, "w");
#endif
    }

    /* Process each source file in the list */

    _curntsrc = SrcNam;

    while (_curntsrc < LastSrc)
    {
        SrcFile = f_opn (*_curntsrc, "r");

        LRefs_Code.reftbl = LRefs_DPD.reftbl
                          = LRefs_NonDPdat.reftbl
                          = (struct ref_xtrnal *) 0;
        Pass2 = *_nam = _ttl[0] = '\0';

        /* Do pass 1 */

        read_fil ();

        /* Now pass 2 */

        rewind (SrcFile);
        Pass2 = TRUE;
        read_fil ();

        wrtvalid ();
        wrt_refs ();

        if (fclose (SrcFile) != 0)
        {
            e_report ("file close error");
        }

        if (S_Flg > 0)
        {
            DoSymTbl ();
        }

        ++_curntsrc;
    }

    closmac ();

    if (HadError)
    {
        /* if an output file exists, and "keep r-file" has not
           been specified then delete it
         */
        if (OutFile && (R_Keep < 1))
        {
            fclose (OutFile);
            unlink (O_Nam);
        }

        exit (1);
    }

    exit (0);
}

static void
read_fil (ZILCH)
{

    precodeset ();
    HadError = d0025 = FALSE;
    SblTrePtr = SmblTreArr;
    _If_Dpth = FalseIf = d0027 = ListLin = ListPag = 0;
    list_lin = d00da = 0;

    if (Pass2 && listON && (ListFlg > 0))
    {
        doList = ON;
    }
    else
    {
        doList = OFF;
    }

    pagehdr ();

    while (getsrcline ())
    {
        if (AsmblLin ())
        {
            coment = SrcChar;
        }

        printline ();
    }

    /* Done..  eject page if need be */

    if (Pass2 && listON)
    {
        if (ff_flg)
        {
            putc ('\f', PrtPth);
        }
        else
        {
            while (list_lin < PgDepth)
            {
                putc ('\n', PrtPth);
                ++list_lin;
            }
        }
    }
}

#define TRM_0064 if ( Oprand == SrcChar ) { Oprand=0; } else { if ( Oprand != 0 ) { if (*SrcChar) { *SrcChar++=0; SkipSpac();}}} return 1;

static int
AsmblLin (ZILCH)
{
    char _curch;
    MACDSCR *_macdsc;

    NumBytes = Is_W
             = AIMFlg
             = IndxVal
             = d001d
             = d0021
             = HadArrow
             = LongJmp[0]
             = RefExtern = 0;

#ifdef COCO
    opr_ptr = &(MLCod.opers);
#else
    opr_ptr = MLCod.opers;
#endif

    if ((Adrs_Ptr = S_Addrs))
    {
        Adrs_Ptr = &CodeAddrs;
        CodeAddrs = *S_Addrs;
    }

    Label = Operatr = Oprand = coment = CmdCod = 0;
    WhereLoc = TypeBasic;
    LblTyp = DLocField;
    LOC1Msk = TmpLocMsk;
    OptBPtr = &d0791[0];

    if (Pass2 && listON && (ListFlg > 0) && ( ! FalseIf || (C_Flg > 0)))
    {
        doList = ON;
    }
    else
    {
        doList = OFF;
    }

    d00d8 = 0;

    if (DoingMacro && (ListMacs <= OFF))
    {
        doList = OFF;
    }

    /* Empty line or comment */

    if (((_curch = *SrcChar) == '\0') || (_curch == '*'))
    {
        return 1;
    }

    if (notblank(_curch))
    {
        Label = SrcChar;

        /* Get label (if any) */

        if ( ! (MacDefining) && ! (FalseIf) && ! (InRept))
        {
            if (do_label () == 0)
            {
                return (e_report ("bad label"));
            }
        }
        else
        {
            /* Move past any possible extra chars in label name */

            while ((_curch = *SrcChar) != '\0')
            {
                if (isblank(_curch))
                {
                    break;
                }

                ++SrcChar;
            }
        }

        if (isblank (*SrcChar))
        {
            *(SrcChar++) = '\0';
        }
    }

    if ((_curch = SkipSpac ()) == '\0')
    {
        return 1;
    }

    /* Move operator to LabelName */

    if (MovLbl (LabelName) != 0)
    {
        Operatr = LabelName;
        SkipSpac ();

        /* If opcode is a macro, process and return */

        if ( ! (InRept) && ((_macdsc = McNamCmp (LabelName)) != 0))
        {
            if (MacDefining)
            {
                mcWrtLin ();
                return 1;
            }
            else
            {
                pushMac (_macdsc);
                return 1;
            }
        }
        else
        {
                    /* Parse for assembler directives */

            if ((Cod_Ent = FindCmd (LabelName, S_Nam, SNamEnd)))
            {
                jsrOfst = (Cod_Ent->tofst & 0x0f);

                if (InRept)
                {
                    if ((jsrOfst == 0) && (Cod_Ent->op_cod == '\x0b'))
                    {
                        return (end_rept ());        /* line was "endr" */
                    }
                    else
                    {
                        doList = OFF;
                        return 1;
                    }
                }
                else                /* not InRept */
                {
                    if (MacDefining)
                    {
                        if (jsrOfst == 4)       /* "macro" */
                        {
                            return (nstmacdf ());   /* can't nest macro defs */
                        }

                        if (jsrOfst == 5)       /* "endm" */
                        {
                            add_nul ();
                            MacDefining = 0;
                            return 1;
                        }
                        else
                        {
                                    /* Write assembler line to macro file */
                            mcWrtLin ();
                            return 1;
                        }
                    }

                    if (FalseIf)
                    {
                        if (jsrOfst != 1)       /* ! "if" statememt */
                        {
                            if (    (jsrOfst != 2)   /* ! "endc" or "else" */
                                 || (--FalseIf == 0)
                                 || (Cod_Ent->op_cod == 0) )    /* endc */
                            {
                                return 1;
                            }
                        }

                        /* Condition: increased IfDepth or
                         *            "else" and FalseIf depth was > 1
                         */

                        ++FalseIf;
                        return 1;
                    }

                    MLCod.opcode = Cod_Ent->op_cod;
                    Oprand = SrcChar;

                    /* Here, we call a general function that will call
                     * a specific function, found in the "opcode" (2nd)
                     * field
                     */

                    if ((*Drectives[jsrOfst]) () == 0)
                    {
                        return 0;
                    }
                    else
                    {
                        /* TRM_0064; */
                        if (Oprand == SrcChar)
                        {
                            Oprand = 0;
                        }
                        else
                        {
                            if (Oprand != 0)
                            {
                                if (*SrcChar)
                                {
                                    *(SrcChar++) = '\0';
                                    SkipSpac ();
                                }
                            }
                        }

                        return 1;
                    }
                }
            }       /* end pseudo-cmds' */
            else
            {
                if (InRept)
                {
                    doList = OFF;
                    return 1;
                }
                else
                {
                    if (MacDefining)
                    {
                        mcWrtLin ();
                        return 1;
                    }
                    else
                    {
                        if (FalseIf)
                        {
                            if ((doList == OFF) || (*SrcChar == '\0'))
                            {
                                return 1;
                            }
                            else
                            {
                                Oprand = SrcChar;

                                while (*(++SrcChar) != '\0')
                                {
                                    if (isblank(*SrcChar))
                                    {
                                        break;
                                    }
                                }

                                if (*SrcChar != '\0')
                                {
                                    *(SrcChar++) = '\0';
                                }

                                return 1;
                            }
                        }       /* end if _IfisTrue */

                        /* If in general psect, check to see if it's a branch
                         * instruction
                         */

                        if (CodTbBgn == S_realcmds)
                        {
                            if ((LabelName[0] == 'b') ||
                                (LabelName[0] == 'B'))
                            {
                                Cod_Ent
                                    = FindCmd (LabelName, S_branch, SBsr_End);
                            }
                        }

                        /* If a Cod_Ent has not yet been found, try
                         * the default list
                         */

                        if (Cod_Ent == 0)
                        {
                            Cod_Ent = FindCmd (LabelName, CodTbBgn, CodTbEnd);
                        }

                        /* Finally.. if we have a valid Cod_Ent
                         * Set up and write the code to the ROF (if -o)
                         */

                        if (Cod_Ent != 0)
                        {
                            /* Note:  The original code AND'ed Cod_Ent
                             * with 0x0f here, but this invalidates the
                             * test for NO_IMMED in the subroutines.  This
                             * is a bug in the original code.
                             */
                            jsrOfst = Cod_Ent->tofst;
                            NumBytes = 1;   /* Default if no prebyte */
                            CmdCod = &MLCod.opcode;

                            /* Set up Prebyte if applicable */

                            if ((MLCod.prebyte =
                                 Cod_Ent->tofst & (PRE_10 | PRE_11)) != 0)
                            {
                                if ((MLCod.prebyte & PRE_10))
                                {
                                    MLCod.prebyte = 0x10;
                                }
                                else
                                {
                                    MLCod.prebyte = 0x11;
                                }

                                /* Prebyte adds another byte
                                 * Point Code start to prebyte
                                 */

                                NumBytes = 2;
                                CmdCod = &MLCod.prebyte;
                            }

                            MLCod.opcode = Cod_Ent->op_cod;
#ifdef COCO
                            MLCod.q_long = 0;
#else
                            memset (MLCod.opers, 0, sizeof (MLCod.opers));
#endif
                            Oprand = SrcChar;

                            /* We will mask off the upper bytes here rather
                             * than above (see Note:).  In addition, we need
                             * to use 0x1f rather than 0x0f, since we now have
                             * more than 16 entries in one of the tables.
                             */

                            if ( ! (*jmp_ptr[(jsrOfst & 0x1f)]) ())
                            {
                                return 0;
                            }
                            else
                            {
                                w_ocod (CmdCod, NumBytes);

                                if (Oprand == SrcChar)
                                {
                                    Oprand = 0;
                                }
                                else
                                {
                                    if (Oprand != 0)
                                    {
                                        if (*SrcChar)
                                        {
                                            *(SrcChar++) = '\0';
                                            SkipSpac ();
                                        }
                                    }
                                }

                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    coment = SrcChar;
    return (e_report ("bad mnemonic"));
}

int
printline (ZILCH)
{
    register char *_codptr;

    if (doList == OFF)
    {
        return 0;
    }

    if ((PgDepth - 3) < list_lin)
    {
        pagehdr ();
    }

    doList = OFF;

    if (NarrowList == 0)
    {
        printf ("%05d", ListLin);       /* Line number */
    }

    if (Label || Operatr || (NumBytes > 0))
    {                           /* l05e6 */
        int __count;

        _codptr = CmdCod;

        /* Code offset */

        if (Adrs_Ptr != 0)
        {
            printf (" %04x", *Adrs_Ptr);
        }
        else
        {
            fputs ("     ", stdout);
        }

        putc (RefExtern ? '=' : ' ', stdout);

        /* Code data (if any) */

        if (_codptr != 0)
        {
            for (__count = 0; __count < 5; __count++)
            {
                if (__count < NumBytes)
                {
                    printf ("%02x", (*(_codptr++)) & 0xff);
                }
                else
                {
                    fputs ("  ", stdout);
                }
            }
        }
        else
        {
            fputs ("          ", stdout);
        }
    }

    if (Label || Operatr)
    {                           /* l0689 */
        if (Label == 0)
        {
            Label = "\0";
        }

        if (Operatr == 0)
        {
            Operatr = "\0";
        }

        if (Oprand == 0)
        {
            Oprand = "\0";
        }

        /* Label-Operator-Operand */

        printf ("%c%-8s %-5s ", (DoingMacro && (d00d8 == 0)) ?
                '+' : ' ', Label, Operatr);
        printf (d00d8 ? "%s" : "%-10s", Oprand);
    }

    /* Comment output - this has been modified to attempt avoiding
     * overruns of line length.
     */
    if (coment)
    {
        int llen;

        if (Oprand && (Label || Operatr || (NumBytes > 0)))
        {
            llen = PgWidth - 39 - (strlen(Oprand) <= 10 ? 10 : strlen(Oprand));
        }
        else
        {
            llen = PgWidth - 7;
        }

        if (llen > 3)
        {
            sprintf (cmntlen, " %%1.%ds", llen);
            printf (cmntlen, coment);
        }
    }

    putc ('\n', stdout);
    return (++list_lin);
}

int
bdrglist (ZILCH)
{
    return (e_report ("bad register list"));
}

int
bd_rgnam (ZILCH)
{
    return (e_report ("bad register name"));
}

int
nstmacdf (ZILCH)
{
    return (e_report ("nested MACRO definitions"));
}

int
#ifdef __STDC__
e_report (char *report)
#else
e_report (report)
     char *report;
#endif
{
    if (Pass2)
    {
        doList = 1;

        if ((list_lin == 0) || (list_lin > PgDepth - 4))
        {
            pagehdr ();
        }

        printf ("*** error - %s ***\n", report);
        ++list_lin;
    }

    ++HadError;
    return 0;
}


void
#ifdef __STDC__
errexit (char *report)
#else
errexit (report)
     char *report;
#endif
{
    fprintf (stderr, "asm: %s\n", report);
    exit (errno);
}

#ifdef _OSK
        /* Add coco functions not included with OSK */

char *
strchr (st, c)
     char *st,
           c;
{
    char c1;

    while (c1 = *(st++))
    {
        if (c1 == c)
            return --st;
    }
    return 0;
}
#endif

