/* **************************************************************** *
 * rma.c first section of "RMA" assembler program                   $
 *                                                                  $
 * $Id::                                                            $
 * **************************************************************** */

/* las's globals are defined in globals.c */

/* rma.h before stdlib.h: rma.h redefines void and const for what
 * follows, so this order makes stdlib.h read as it does in las's header
 * image (dcc -h, step 17), and the image fits this file too */
#define NEED_CTYPE
#include "rma.h"

#include <stdlib.h>
#include <errno.h>

#ifndef COCO
#   include <unistd.h>
#endif

char cmntlen[SYMLEN];   /* To hold " %1.xs val for comment length */

static int AsmblLin (ZILCH);

static void read_fil (ZILCH);
static void settle (ZILCH);

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

    ArgC = argc;
    ArgV = argv;
    GetOpts (argc, argv);
    preload_load ();
    do_defines ();

    if (ListFlg)
    {
        listON = ON;
    }

    LRefs_Code = sym_anon ();
    LRefs_DPD = sym_anon ();
    LRefs_NonDPdat = sym_anon ();
    sym_settyp (LRefs_Code, CODENT);
    sym_settyp (LRefs_DPD, INIENT | DIRENT);
    sym_settyp (LRefs_NonDPdat, INIENT);
    sym_setmsk (LRefs_Code, DIRENT);
    sym_setmsk (LRefs_DPD, DIRENT);
    sym_setmsk (LRefs_NonDPdat, DIRENT);
    NowMac = 0;

    if (OFlg && ! SzTrial)     /* the OS-9 trial writes nothing */
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
        CurSrcName = *_curntsrc;

        sym_setrefs (LRefs_Code, 0);
        sym_setrefs (LRefs_DPD, 0);
        sym_setrefs (LRefs_NonDPdat, 0);
        Pass2 = Unsettled = *_nam = _ttl[0] = '\0';

        /* lwasm sizes ",pcr" offsets itself: a trial pass 1 in a child
         * decides them first (sizing.c) */

        if (LwasmMode)
        {
            sz_trial ();
            rewind (SrcFile);   /* the trial moved the shared file offset */
        }

        /* Do pass 1 */

        sz_pass ();
        read_fil ();

        if (SzTrial)
        {
            sz_finish ();       /* the trial ends here */
        }

        /* lwasm settles values over as many passes as it takes; RMA has
         * two, so pass 2 must find what pass 1 left. When pass 1 met a
         * count it couldn't know yet (directives.c), las runs pass 2's
         * code without output, listing or errors, which corrects each
         * value it reaches, until nothing moves */

        if (Unsettled)
        {
            settle ();
        }

        /* Now pass 2 */

        rewind (SrcFile);
        Pass2 = TRUE;
        sz_pass ();
        read_fil ();

        if ( ! UsedModule)      /* a module has no ROF header or refs */
        {
            wrtvalid ();
            wrt_refs ();
        }

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

#ifdef SYMSTATS
    {
        extern long StatRefs;

        sym_stats (StatRefs);
        bank_stats ();
    }
#endif

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

        bank_done ();
        exit (1);
    }

    bank_done ();
    exit (0);
}

static void
read_fil (ZILCH)
{

    precodeset ();
    mod_pass ();
    sec_pass ();
    img_pass ();
    HadError = d0025 = FALSE;
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
    int _modcmd, _inst = 0;

    if (TopPending)         /* after emod in lwasm mode (module.c) */
    {
        TopPending = 0;
        top_setup ();
    }

    IdxLitZero = 0;

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

    /* Empty line or comment. Like lwasm, a line whose first non-blank
     * character is '*' or ';' is a comment, wherever it starts. */

    if ((_curch = *SrcChar) == '\0')
    {
        LocalCtx = ++NextCtx;         /* blank line: ends a local label block */
        return 1;
    }

    if ((_curch == '*') || (_curch == ';'))
    {
        return 1;
    }

    if (isblank (_curch))
    {
        register char *_p = SrcChar;

        while (isblank (*_p))
        {
            ++_p;
        }

        if ((*_p == '*') || (*_p == ';'))
        {
            return 1;
        }
    }

    /* lwasm: struct, endstruct and the fields between (lwstruct.c) */

    if (LwasmMode && ! MacDefining && ! FalseIf && ! InRept)
    {
        int r = st_line ();

        if (r >= 0)
        {
            return r;
        }
    }

    if (notblank(_curch))
    {
        Label = SrcChar;

        /* Get label (if any) */

        if ( ! (MacDefining) && ! (FalseIf) && ! (InRept))
        {
            if (InTop && mod_peek ())
            {
                AbsCtr = 0;     /* lwasm: the mod line is at address 0 */
            }

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
        if ( ! Label)
        {
            LocalCtx = ++NextCtx;     /* blank line: ends a local label block */
        }

        return 1;
    }

    if (LwasmMode && (_curch == ';'))
    {
        return 1;           /* lwasm: "label   ; comment" */
    }

    if (LwasmMode && (_curch == '=') && Label)
    {
        /* lwasm: "label = value" is "label equ value" */

        ++SrcChar;
        SkipSpac ();
        Oprand = SrcChar;
        MLCod.opcode = 0x05;            /* equ, as in S_Nam */

        if ((*Drectives[3]) () == 0)
        {
            return 0;
        }

        if (*SrcChar)
        {
            *(SrcChar++) = '\0';
            SkipSpac ();
        }

        return 1;
    }

    /* Move operator to LabelName */

    if (MovLbl (LabelName) != 0)
    {
        Operatr = LabelName;
        SkipSpac ();

        if (ImgRec && ! FalseIf && ! MacDefining)
        {
            img_op (LabelName);     /* recording the defs (defsimg.c) */
        }

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

            /* FindCmd matches table entries as prefixes, so "set" also
             * matched "setdp" (RMA never assembled setdp correctly).
             * lwasm mode requires the whole directive name. */

            if ((Cod_Ent = FindCmd (LabelName, S_Nam, SNamEnd)) &&
                LwasmMode && (strlen (Cod_Ent->ccmd) != strlen (CmdMnemonic)))
            {
                Cod_Ent = 0;
            }

            if (Cod_Ent)
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
                            LocalCtx = ++NextCtx;   /* lwasm: endm breaks */
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

                        /* Inside a module, emod, rmb and org come first */

                        _modcmd = 0;

                        if ((Cod_Ent == 0) && InModule &&
                            (Cod_Ent = FindCmd (LabelName, S_Module, SModEnd)))
                        {
                            _modcmd = 1;
                        }

                        if ((Cod_Ent == 0) && InTop &&
                            (Cod_Ent = FindCmd (LabelName, S_TopLw, STopLwEnd)))
                        {
                            _modcmd = 2;
                        }

                        /* lwasm object format: section, endsect (lwobj.c) */

                        if ((Cod_Ent == 0) && LwasmMode &&
                            (Cod_Ent = FindCmd (LabelName, S_LwSect, SLwSectEnd)))
                        {
                            _modcmd = 3;
                        }

                        /* If a Cod_Ent has not yet been found, try
                         * the default list
                         */

                        if (Cod_Ent == 0)
                        {
                            Cod_Ent = FindCmd (LabelName, CodTbBgn, CodTbEnd);
                        }

                        /* lwasm: zmb and bsz are zero-filled bytes.  In a
                         * section that is the section's rmb (zeros in code,
                         * only reserved in bss, lwobj.c); elsewhere rzb */

                        if ((Cod_Ent == 0) && LwasmMode &&
                            FindCmd (LabelName, S_LwZero, SLwZeroEnd))
                        {
                            if (InSection)
                            {
                                Cod_Ent = FindCmd ("rmb", S_LwSect, SLwSectEnd);
                                _modcmd = 3;
                            }
                            else
                            {
                                Cod_Ent = FindCmd ("rzb", CodTbBgn, CodTbEnd);
                            }
                        }

                        /* lwasm: a struct's name reserves an instance,
                         * as "rmb sizeof{name}" would here (lwstruct.c) */

                        _inst = 0;

                        if ((Cod_Ent == 0) && LwasmMode &&
                            ((_inst = st_inst (LabelName)) != 0))
                        {
                            if (_inst < 0)
                            {
                                return 0;
                            }

                            if (InTop && (Cod_Ent = FindCmd ("rmb", S_TopLw,
                                                             STopLwEnd)))
                            {
                                _modcmd = 2;
                            }
                            else if ((Cod_Ent = FindCmd ("rmb", S_LwSect,
                                                         SLwSectEnd)))
                            {
                                _modcmd = 3;
                            }
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
                            /* .opers: K&R C took "MLCod.q_long" as offset
                             * 0 of MLCod and cleared prebyte and opcode */
                            MLCod.opers.q_long = 0;
#else
                            memset (MLCod.opers, 0, sizeof (MLCod.opers));
#endif
                            Oprand = SrcChar;

                            /* We will mask off the upper bytes here rather
                             * than above (see Note:).  In addition, we need
                             * to use 0x1f rather than 0x0f, since we now have
                             * more than 16 entries in one of the tables.
                             */

                            if ( ! ((_modcmd == 1) ? (*ModTbl[(jsrOfst & 0x1f)]) ()
                                  : (_modcmd == 2) ? (*TopLwTbl[(jsrOfst & 0x1f)]) ()
                                  : (_modcmd == 3) ? (*LwSectTbl[(jsrOfst & 0x1f)]) ()
                                  : (*jmp_ptr[(jsrOfst & 0x1f)]) ()))
                            {
                                return 0;
                            }
                            else
                            {
                                if ((_inst > 0) && ! st_post ())
                                {
                                    return 0;
                                }

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
    if (Settle)         /* the real pass 2 will say it */
    {
        return 0;
    }

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


/* settle() - settling passes before pass 2 (see the call) */

#define SETTLEMAX 8             /* then pass 2 reports what still moves */

static void
settle (ZILCH)
{
    FILE *out = OutFile;
    char list = listON;
    int n = 0;

    img_settle ();
    OutFile = 0;
    listON = 0;
    Pass2 = Settle = TRUE;

    while (Unsettled && (n++ < SETTLEMAX))
    {
        Unsettled = 0;
        rewind (SrcFile);
        sz_pass ();
        read_fil ();
    }

    Settle = 0;
    OutFile = out;
    listON = list;
}

void
#ifdef __STDC__
errexit (char *report)
#else
errexit (report)
     char *report;
#endif
{
    if ( ! SzTrial)             /* the parent will say it (sizing.c) */
    {
        fprintf (stderr, "asm: %s\n", report);
    }

    bank_done ();

    /* errno is 0 unless a system call just failed: "bad -D value" and
     * "symbol table overflow" exited 0, success to the shell (step 14) */

    exit (errno ? errno : 1);
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

