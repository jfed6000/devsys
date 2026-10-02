/* ************************************************************ *
 * part5.c - for "rma"                                          *
 * This module handles assembler directives such as fcb, rmb    *
 * os9 call, rmb, etc                                           *
 * $Id::                                                    $   *
 * ************************************************************ */

#define NEED_CTYPE
#include <time.h>
#include "rma.h"

#ifndef COCO
char nmbr_str[8];             /*Little-Endian ordered string */
#endif

struct optns
{
    char optn;
    char *optadr;
};

struct optns cmdopts[] = {
    {'l', &ListFlg},    /* Output formatted listing                         */
    {'o', &OFlg},       /* Output ROF file                                  */
    {'c', &C_Flg},      /* Suppress conditional assembly lines in listng    */
    {'f', &ff_flg},     /* Use Form-Feed to eject page                      */
    {'g', &g_flg},      /* List ALL code bytes generated                    */
    {'e', &e_flg},      /* Supress error codes in listing                   */
    {'s', &S_Flg},      /* Print Symbol table                               */
    {'x', &ListMacs},   /* Don't expand macro listing                       */
    {'n', &NarrowList}, /* "Narrow" listing -no line #'s, 1-col symbol tbl  */
    {'k', &R_Keep}      /* Keep (do not delete) ROF file on error           */
};

extern FILE *MacFile;

#ifdef __STDC__
static int copy_to (char *);
#else
static int copy_to();
#endif

static int bad_opt ();

/* ******************************************************************** *
 * senddata() - Output one operand entry, NumByes length                *
 * ******************************************************************** */

static void
senddata (ZILCH)
{
    w_ocod (CmdCod, NumBytes);

    if (doList)
    {
        printline ();

        if (Pass2 && listON && (ListFlg > 0) && (g_flg > 0))
        {
            doList = 1;
        }
        else
        {
            doList = 0;
        }

        Label = Operatr = Oprand = coment = 0;

        if (S_Addrs)
        {
            CodeAddrs = *S_Addrs;
        }

    }

    NumBytes = 0;
}

static void
#ifdef __STDC__
addbyte (int parm)
#else
addbyte (parm)
     char parm;
#endif
{
    if (NumBytes >= 4)
    {
        senddata ();
    }

    *(char *) (CmdCod + (NumBytes++)) = parm;
}

int
l1723 (ZILCH)
{
    register SYMH smbl_tmp = CurntLabel;
    int flag = 0;

    if (getNmbr ())
    {
        if (LwasmMode)
        {
            pl_fix (d0791); /* org/rmb counts: value from --defs */
        }

        rmcoderefs ();   /* Delete certain refs ? */

        if (OptBPtr == d0791)
        {
            flag = 1;
        }
        else
        {
            NmbrInt = ilExtRef ();
        }
    }

    CurntLabel = smbl_tmp;
    return flag;
}

int
do_rzb (ZILCH)
{
    if (l1723 ())
    {
        while (NmbrInt--)
        {
            addbyte (0);
        }

        return 1;
    }

    return 0;
}

static unsigned int
_curntorg (ZILCH)
{
    if (CurntOrg == 0)
    {
        return 0;
    }
    else
    {
        return (*CurntOrg);
    }
}

static int
#ifdef __STDC__
savcurorg (int parm)
#else
savcurorg (parm)
     int parm;
#endif
{
    if (CurntOrg)
    {
        *CurntOrg = parm;
    }

    return parm;
}

/* ************************************************************ *
 * l1780() - Setup routine for "equ" or "set"                   *
 * ************************************************************ */

static int
#ifdef __STDC__
l1780 (int parm)
#else
l1780 (parm)
     int parm;
#endif
{
    register SYMH reg;

    if (Label == 0)
    {
        return e_report ("label missing");
    }

    l2069 (parm);
    reg = CurntLabel;

    if (getNmbr () == 0)
    {
        return 0;
    }

    if (LwasmMode)
    {
        pl_fix (d0791);     /* "type equ Prgrm": value from --defs */
    }

    CurntLabel = reg;
    rmcoderefs ();
    l20a4 (NmbrInt);
    Adrs_Ptr = &NmbrInt;
#ifdef COCO
    ++Adrs_Ptr;                 /* no??? then bump to second half go get an int     */
#endif
    return 1;
}

/* ************************************************************************ *
 * _getstring() - Reads string data from input buffer and writes to outfile *
 *                if need be.                                               *
 * Passed : int fcs_flg: 1 = fcc, 2 = fcs                                   *
 * ************************************************************************ */

static int
#ifdef __STDC__
_getstring (int isFCS)
#else
_getstring (isFCS)
     int isFCS;
#endif
{
    char *_endptr,
          _delim,   /* The first char (the delimiter) */
          _chnext;

    if ((_delim = *SrcChar) != '\0')
    {
        _endptr = SrcChar;

        /* Find match for _delim - the end of the string */

        while ((_chnext = *(++_endptr)) != '\0')
        {
            if (_chnext == _delim)
            {
                break;
            }
        }

        if (_chnext != '\0')        /* Matching delim found */
        {
            if ((_endptr -= 2) >= SrcChar)  /* Back up to next-to-last char */
            {
                while (SrcChar < _endptr)
                {
                    addbyte (*(++SrcChar));
                }

                _chnext = *(++SrcChar);

                /* If we're dealing with "fcs", set MSBit of final char */

                if (isFCS == 2)
                {
                    _chnext |= 0x80;
                }

                addbyte (_chnext);
            }

            SrcChar += 2;
            return 1;
        }

        /* If we get here, we have not found a matching delimiter */

        SrcChar = _endptr;
    }

    return (cnstDef ());
}

int
do_rmb (ZILCH)
{
    if (l1723 ())   /* not external ref */
    {
        *(Adrs_Ptr = &CodeAddrs) = _curntorg ();

        /* The casts matter on the 6809 (no prototypes there): l20a4
         * takes a long, savcurorg an int, and NmbrInt is a long. */

        if (Label)
        {
            l2069 (WhereLoc & ~INIENT);
            l20a4 ((long) CodeAddrs);
        }

        savcurorg ((int) (CodeAddrs + NmbrInt));
        return 1;
    }

    return 0;
}

int
do_equ (ZILCH)
{
    return (l1780 (CODENT | DIRENT));
}

int
do_set (ZILCH)
{
    return (l1780 (CODENT | INIENT));
}

int
do_comon (ZILCH)
{
    if (Label == 0)
    {
        return e_report ("label missing");
    }

    if (l1723 () == 0)
    {
        return 0;
    }

    l2069 (7);
    sym_setmsk (CurntLabel, sym_msk (CurntLabel) | (CODENT));

    if (Pass2)
    {
        ++comn_cnt;
    }

    if (sym_val (CurntLabel) < NmbrInt)
    {
        sym_setval (CurntLabel, NmbrInt);
    }

    Adrs_Ptr = (int *)(&NmbrInt);
#ifdef COCO
    ++Adrs_Ptr;
#endif
    return 1;
}

int
set_fcc (ZILCH)
{
    return (_getstring (1));         /* return may not be needed */
}

/* lwasm: one zero byte, for rmb in a code-like section (lwobj.c) */

void
addzero (ZILCH)
{
    addbyte (0);
}

/* lwasm: "dtb" writes the assembly date and time: year since 1900,
 * month, day, hour, minute, second */

int
set_dtb (ZILCH)
{
    static char dtbuf[4];
    long now = time (0);
    struct tm *tm = localtime (&now);

    CmdCod = dtbuf;
    NumBytes = 0;
    addbyte (tm->tm_year);
    addbyte (tm->tm_mon + 1);
    addbyte (tm->tm_mday);
    addbyte (tm->tm_hour);
    addbyte (tm->tm_min);
    addbyte (tm->tm_sec);
    return 1;
}

/* lwasm: "fcn" is fcc followed by a 0 byte */

int
set_fcn (ZILCH)
{
    NumBytes = 0;       /* called directly, not through equset */

    if ( ! _getstring (1))
    {
        return 0;
    }

    addbyte (0);
    return 1;
}

/* lwasm: "fill value,count" writes count copies of a byte */

int
set_fill (ZILCH)
{
    static char fillbuf[4];
    int value;
    long count;

    if ( ! getNmbr ())
    {
        return 0;
    }

    value = NmbrInt;

    if ( ! iscomma () || ! getNmbr ())
    {
        return 0;
    }

    count = NmbrInt;
    CmdCod = fillbuf;
    NumBytes = 0;

    while (count-- > 0)
    {
        addbyte (value);
    }

    return 1;
}

int
set_fcs (ZILCH)
{
    return (_getstring (2));         /* return may not be needed */
}

int
cnstDef (ZILCH)
{
    return ( e_report ("constant definition") );
}

/* ************************************************************************ *
 * getfc_data () - Process an "fcb" or "fdb" operand.  It accomodates       *
 *            either a single entry, or a comma-separated list of entries.  *
 *            The last element is left unwritten, but if a comma-separated  *
 *            list, all but the last are written to the output file         *
 * Passed : (1) - Function to call to handle retrieving of data from        *
 *                source line, stored in NmbrInt.                           *
 *          (2) - Length of data element (1 or 2 bytes)                     *
 * ************************************************************************ */

static int
#ifdef __STDC__
getfc_data (int (*adr) (void), int cnt)
#else
getfc_data (adr, cnt)
     int (*adr) ();
     int cnt;
#endif
{
    for (;;)
    {
        NumBytes = cnt;
        InDataDir = 1;
        (*adr) ();      /* Function to retrieve data into NmbrInt */
        InDataDir = 0;

        /* For non-coco systems, move value to nmbr_str, LITTLEENDIAN */

#ifndef COCO
        switch (cnt)
        {
            case 1:
                *nmbr_str = (char) NmbrInt;
                break;
            case 2:
                storInt (nmbr_str, NmbrInt);
                break;
            default:
                break;
        }
#endif
        if (*SrcChar != ',')
        {
            return 1;
        }

        senddata ();
        ++SrcChar;
    }
}

int
set_fcb (ZILCH)
{
#ifdef COCO
    CmdCod = (char *) (&NmbrInt) + sizeof (NmbrInt) - 1;
#else
    CmdCod = nmbr_str;
#endif
    return (getfc_data (proc_byte, 1));
}

int
set_fdb (ZILCH)
{
#ifdef COCO
    CmdCod = (char *) (&NmbrInt) + sizeof (NmbrInt) - 2;
#else
    CmdCod = nmbr_str;
#endif
    return (getfc_data (proc_int, 2));
}

int
do_os9 (ZILCH)
{
    NumBytes = 3;
    proc_byte ();                   /* SEE IF parameter is needed */
    MLCod.opcode = 16;
    opr_ptr[0] = 0x3f;
    opr_ptr[1] = NmbrInt;
    return 1;
}

/* ******************************************************************** *
 * iscomma() - if current character is comma, bump SrcChar              *
 * Returns : TRUE if comma, FALSE if not, after error report            *
 * ******************************************************************** */

int
iscomma (ZILCH)
{
    if (*SrcChar == ',')
    {
        ++SrcChar;
        return 1;
    }
    else
    {
        return ( e_report ("comma expected") );
    }
}

/* **************************************************************** *
 * l198d() - Calls l1723()                                          *
 *          if S_Addrs is non-zero, stores NmbrInt in address       *
 *            pointed to by S_Addrs                                 *
 * Returns : *S_Addrs if valid, else 0 (S_Addrs) Possibly not valid *
 * **************************************************************** */

/* This is listed in a jumptbl in rtables.h, but don't think it's ever used */
int
l198d (ZILCH)
{
    l1723 ();

    if (S_Addrs)
    {
        return (*S_Addrs = NmbrInt);
    }

    return (int)S_Addrs;
}

/* ************************************************************ *
 * This following null routine is ??????    . It is listed in   *
 * one of the JMPTBL entries. It might be a deleted function..  *
 *       -w  maybe ???       Inspect later...                   *
 * We prototype it as int and add a return value to satisfy gcc *
 * ************************************************************ */

int
donothing (ZILCH)
{
#ifndef COCO
    return 1;
#endif
}

int
do_nam (ZILCH)
{
    return copy_to (_nam);
}

        /* copy a string to dest_str */

static int
#ifdef __STDC__
copy_to (char *dest_str)
#else
copy_to (dest_str)
     char *dest_str;
#endif
{
    register char *dst = dest_str;

    if (Pass2 || (dst[0] == '\0'))
    {
        while ( (*(dst++) = *(SrcChar++)) )
        {
        };

        --SrcChar;
    }

    return 1;
}

int
do_ttl (ZILCH)
{
    return (copy_to (_ttl));
}

int
do_pag (ZILCH)
{
    pagehdr ();
    return (doList = 0);
}

int
do_newline (ZILCH)
{
    /* _isnum() reads in a number string and stores it in NmbrInt */

    if (_isnum ())  /* if valid number and non-zero */
    {
        while (NmbrInt--)
        {
            newline ();
        }

        doList = 0;
    }

    return 0;   /* This isn't in COCO code, but gcc wants a return */
}

int
_isnum ()
{
    return ( _getnum () ? 1 : e_report ("bad number") );
}

/* ********************************
 * Process 'opt' directive
 * ********************************
 */

int
set_opts ()
{
    int ch_opt;
    char _opt_chr;
    register struct optns *tcmd;

    _opt_chr = SkipSpac ();

    for (;;)
    {
        if ((ch_opt = (_opt_chr != '-')) == 0)
        {
            ch_opt = -1;        /* to turn OFF */
            _opt_chr = *(++SrcChar);
        }
        else
        {
            ch_opt = 1;         /* to turn ON option */
        }

        _opt_chr = _tolower (_opt_chr);
        tcmd = &cmdopts[0];

        /* Parse list of cmds not requiring a value */

        while (tcmd < endof (cmdopts))
        {
            if (_opt_chr == tcmd->optn)
            {
                *tcmd->optadr += ch_opt;
                ++SrcChar;      /*  Bump for nxt chr (fixed from orig) */
                break;
            }

            ++tcmd;
        }

        /* If one of the above cmds not matched, check for remaining cmds */

        if (tcmd >= endof (cmdopts))
        {
            switch (_opt_chr)
            {
                case 'd':       /* Page depth */
                    ++SrcChar;

                    if (_isnum ())
                    {
                        PgDepth = NmbrInt;
                    }
                    else
                    {
                        return (bad_opt ());
                    }

                    break;
                case 'w':       /* Page width */
                    ++SrcChar;

                    if (_isnum ())
                    {
                        PgWidth = NmbrInt;
                    }
                    else
                    {
                        return (bad_opt ());
                    }

                    break;
                case 'v':       /* Version */
                    ++SrcChar;

                    if (_isnum () && (NmbrInt >= 0) && (NmbrInt < 2))
                    {
                        rversion = NmbrInt;
                    }
                    else
                    {
                        return (bad_opt ());
                    }

                    break;
                default:
                    return (bad_opt ());
            }
        }

        /* Original pre-bump here moved up to while... loop above
           to make d-w-v opts work correctly  */
        if (*SrcChar != ',')
        {
            break;
        }

        _opt_chr = *(++SrcChar);
    }       /* End for (;;) loop */

    return 1;
}

static int
bad_opt (ZILCH)
               /*     l1ad9()     */
{
    return (e_report ("bad option"));
}

/* I believe this is another function that is never used */

int
l1ae2 (ZILCH)
{
    if (getNmbr ())
    {
        rmcoderefs ();
    }

    if ((OptBPtr = d0791))
    {
        d0027 = NmbrInt;
        Adrs_Ptr = (int *)(&NmbrInt);

        /* We bump Adrs_ptr because NmbrInt is a long */

#ifdef COCO
        ++Adrs_Ptr;
#endif
        return 1;
    }

    return (ilExtRef ());
}

/* **************************************************************** *
 * do_use() - process a " use xxx" line.                            *
 *            Opens file, appends the FILE * onto the file stack,   *
 *            saving the old file, and dumps the rest of the line   *
 *            up to a space or end of line                          *
 * **************************************************************** */

int
do_use (ZILCH)
{
    char _chr;

    register char *_end = SrcChar;

    /* The file name ends at the first blank: anything after it is a
     * comment (lwasm allows "use defsfile  ; comment"). */

    while (*_end && ! isblank (*_end))
    {
        ++_end;
    }

    _chr = *_end;
    *_end = '\0';
    *(ThisFile++) = SrcFile;
    SrcFile = u_opn (SrcChar, "r");
    *_end = _chr;
    SrcChar = _end;

    return 1;
}

/* ******************************************************************** *
 * is_def(), is_ndef() - lwasm's "ifdef"/"ifndef": is the symbol named  *
 *          in the operand defined (so far in this pass)?               *
 * ******************************************************************** */

static int
symdefined (ZILCH)
{
    char name[SYMLEN];
    SYMH sym;

    if ( ! MovLbl (name))
    {
        e_report ("symbol name expected");
        return 0;
    }

    sym = sym_find (name);
    return (sym && (sym_msk (sym) & RELATIVE));
}

int
is_def (ZILCH)
{
    return symdefined ();
}

int
is_ndef (ZILCH)
{
    return ! symdefined ();
}

int
_isnul (ZILCH)
{
    return (NmbrInt == 0);
}

int
not_nul (ZILCH)
{
    return (NmbrInt != 0);
}

int
is_neg (ZILCH)
{
    return (NmbrInt < 0);
}

int
le_zero (ZILCH)
{
    return (NmbrInt <= 0);
}

int
ge_zero (ZILCH)
{
    return (NmbrInt >= 0);
}

int
is_pos (ZILCH)
{
    return (NmbrInt > 0);
}

int
is_pass1 (ZILCH)
{
    /* lwasm doesn't support ifp1 and treats it as always true, so
     * both passes read the same lines (e.g. the defs) */

    return (LwasmMode ? 1 : PASS1);
}

/* ******************************************************** *
 * set_vsec() - For entering a vsect INSIDE a "psect"       *
 * ******************************************************** */

int
set_vsec (ZILCH)
{
    CodTbBgn = Data_Equ;
    CodTbEnd = DtaEqEnd;        /* &s_rmb3 */
    jmp_ptr = VsecInCode;
    CurLclRef = LRefs_NonDPdat;
    TypeBasic = INIENT;
    TmpLocMsk = 0;
    S_Addrs = &I_data;
    CurntOrg = &U_data;

    if (is_dp ())
    {                           /* if vsect "dp" */
        CurLclRef = LRefs_DPD;
        TypeBasic = DIRENT | INIENT;
        TmpLocMsk = DIRLOC;
        S_Addrs = &I_dpd;
        CurntOrg = &U_dpd;
    }

    CodeAddrs = *S_Addrs;           /* CHECK OUT */
    return 1;
}

int
is_dp (ZILCH)
{
    char var;

    if (((var = _toupper (*SrcChar)) != '\0') && (var == 'D') &&
        (_toupper (SrcChar[1]) == 'P'))
    {

        if ((SrcChar[2] == '\0') || isblank(SrcChar[2]))
        {
            SrcChar += 2;
            return 1;
        }
        else
        {
            return (e_report ("DP section ???"));
        }
    }

    return 0;
}

/* ************************************************************ *
 * precodeset() - Does setup if psect has not yet been          *
 *                 entered.                                     *
 * ************************************************************ */

int
precodeset (ZILCH)
{
    NumBytes = 0;
    CodTbBgn = S_Psec;
    CodTbEnd = PSecEnd;

    jmp_ptr = j_secs;
    CurLclRef = 0;
    TypeBasic = CODENT;
    DLocField = CODLOC | RELATIVE;
    S_Addrs = 0;
    CurntOrg = &AbsCtr;     /* the top level's location counter (module.c) */
    TmpLocMsk = 0;
    return 1;
}

int
tel_fail (ZILCH)
{
    return (e_report ((Oprand == 0) ? "fail" : Oprand));
}

int
do_rept (ZILCH)
{
    if (reptFpos)
    {
        return (e_report ("nested REPT"));
    }
    else
    {
        if (getNmbr () == 0)    /* rept 0 => do nothing */
        {
            return 0;
        }

        rmcoderefs ();

        if (OptBPtr != d0791)
        {
            return ilExtRef ();
        }

        if ((ReptCount = NmbrInt) != 0)
        {
            reptFpos = ftell (DoingMacro ? MacFile : SrcFile);
            return 1;
        }

        InRept = 1;
        return 1;
    }
}

int
end_rept (ZILCH)
{
    if (InRept)
    {
        InRept = 0;
        return 1;
    }

    if (reptFpos == 0)
    {
        return (e_report ("ENDR without REPT"));
    }
    else
    {
        if (--ReptCount > 0)
        {
            fseek ((DoingMacro ? MacFile : SrcFile), reptFpos, 0);
            doList = 0;
        }
        else
        {
            reptFpos = 0;
        }

        return 1;
    }
}
