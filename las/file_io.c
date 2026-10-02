
/* ******************************************************************** *
 * part 8 - for RMA                                                     $
 * This deals with input/output.. both printing/listing and the rof,    $
 *                          symbol table etc..                          $
 *                                                                      $
 * $Id::                                                                $
 * ******************************************************************** */

#include <time.h>
#include "rma.h"

#define BEGIN 0                 /* fseek "place" */
#define DO_FF putc( 0x0c, PrtPth );
#define DO_CR putc ( '\n', PrtPth );

#ifdef COCO
#	define PUTINT(i,c) fwrite(&i,2,1,c)
#else
#	define PUTINT(i,c) putc( i>>8&0xff,c);putc(i&0xff,c);
#endif

static long CurFPos = 0;

static struct binhead
{                               /* header structure */
#ifdef COCO
    long h_sync;                /* d03e0 */
    _ushort h_tylan;            /* d03e4 */
#else
    char h_sync[4];
    char h_tylan[2];
#endif
    char h_valid,                                      /* d03e6 */
         h_date[5],                                    /* d03e7 */
         h_edit,                                       /* d03ec */
         h_spare;                                      /* d03ed */
#ifdef COCO
    _ushort h_udata,    /* Uninit NDP data size */
            h_udpd,     /* Uninit DP data size  */
            h_idata,    /* Init NDP data size   */
            h_idpd,     /* Init DP data size    */
            h_ocode,    /* Code size            */
            h_stack,    /* Stack size           */
      h_entry;          /* Program entry point  */
} RofHdr = {
    0x62cd2387, 0, 0,
   {0, 0, 0, 0, 0},
    0, 1, 0, 0, 0, 0, 0, 0, 0
};
#else
    char h_udata[2],
         h_udpd[2],
         h_idata[2],
         h_idpd[2],
         h_ocode[2],
         h_stack[2],
         h_entry[2];
} RofHdr =
{
    {'\x62', '\xcd', '\x23', '\x87'},
    { 0, 0},
    0,
    {0, 0, 0, 0, 0},
    0, 1,
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
};

#endif

static void do_reflist (
#ifdef __STDC__
    struct ref_xtrnal *parm
#endif
);

static int DecSFil ();

static void PrtSmbl (
#ifdef __STDC__
    struct symblstr *
#endif
);

static void wrtxtrn (
#ifdef __STDC__
    struct symblstr *parm1
#endif
);

static void wrt_global (
#ifdef __STDC__
    struct symblstr *parm
#endif
);

static void ck_xtrnl();

/* ******************************************************** *
 * GetOpts - Parse command line for options.                *
 *          This uses the same routine that interprets      *
 *          inline options (read from source file)          *
 * ******************************************************** */

/* ******************************************************** *
 * set_pragmas() - lwasm's --pragma=a,b,c. The ones las     *
 *          implements change its behaviour; the rest are   *
 *          accepted and ignored, so lwasm command lines    *
 *          work unchanged.                                 *
 * ******************************************************** */

static void
#ifdef __STDC__
set_pragmas (char *list)
#else
set_pragmas (list)
     char *list;
#endif
{
    char name[32];
    register int n;

    while (*list)
    {
        for (n = 0; *list && (*list != ',') && (n < 31); ++list)
        {
            name[n++] = _tolower (*list);
        }

        name[n] = '\0';

        if (*list == ',')
        {
            ++list;
        }

        if (strcmp (name, "nosymbolcase") == 0)
        {
            PragNoCase = 1;
        }
        else if (strcmp (name, "condundefzero") == 0)
        {
            PragUndefZero = 1;
        }
        else if (strcmp (name, "export") == 0)
        {
            PragExport = 1;
        }
    }
}

/* -DNAME[=value] definitions from the command line (lwasm's -D) */

static char *Defines[32];
static int DefCount;

/* ******************************************************** *
 * do_defines() - Create a "set" symbol for each -D option, *
 *          as if the source began with "NAME set value".   *
 *          The value defaults to 1 and may be decimal,     *
 *          $hex or 0x hex, like lwasm's.                   *
 * ******************************************************** */

void
do_defines (ZILCH)
{
    int i;

    for (i = 0; i < DefCount; ++i)
    {
        char name[SYMLEN];
        register char *p = Defines[i];
        register int n = 0;
        long val = 1;
        struct symblstr *sym;

        while (*p && (*p != '=') && (n < SYMLEN - 1))
        {
            name[n++] = *p++;
        }

        name[n] = '\0';

        if (*p == '=')
        {
            int base = 10;

            ++p;

            if (*p == '$')
            {
                base = 16;
                ++p;
            }
            else if ((p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X')))
            {
                base = 16;
                p += 2;
            }

            for (val = 0; *p; ++p)
            {
                int d = ((*p >= '0') && (*p <= '9')) ? (*p - '0')
                      : ((*p >= 'a') && (*p <= 'f')) ? (*p - 'a' + 10)
                      : ((*p >= 'A') && (*p <= 'F')) ? (*p - 'A' + 10) : 99;

                if (d >= base)
                {
                    errexit ("bad -D value");
                }

                val = val * base + d;
            }
        }

        if (n == 0)
        {
            errexit ("bad -D option");
        }

        if ( ! (sym = sym_find (name)))
        {
            sym = sym_add (name);
        }

        sym->smbltyp = CODENT | INIENT;     /* a "set" symbol */
        /* defined, a plain number. Keep LOC1BYT: it is the symbol
         * table tree's "real right child" flag (symtab.c), and this runs
         * again before pass 2 when the symbol may have children. */

        sym->locmsk = (sym->locmsk & LOC1BYT) | RELATIVE | DIRENT;
        sym->s_ofst = val;
    }
}

void
#ifdef __STDC__
GetOpts (unsigned _argc, char **_argv)  /* parse parameter line */
#else
GetOpts (_argc, _argv)
     unsigned _argc;
     char **_argv;
#endif
{
    char var;

    SrcFile = OutFile = 0;
    PrtPth = stdout;
    ++_argv;

    while (--_argc)
    {
        if (strncmp (*_argv, "--pragma=", 9) == 0)
        {
            set_pragmas (*_argv + 9);
        }
        else if (strcmp (*_argv, "--lwasm") == 0)
        {
            LwasmMode = 1;
        }
        else if (strncmp (*_argv, "--defs=", 7) == 0)
        {
            preload_add (*_argv + 7);   /* preload.c */
        }
        else if ((**_argv == '-') && ((*_argv)[1] == 'D'))
        {
            /* -D is case-sensitive: lower-case -d is the page depth */

            if (DefCount >= 32)
            {
                errexit ("too many -D options");
            }

            Defines[DefCount++] = *_argv + 2;
        }
        else if (**_argv == '-')
        {
            SrcChar = *_argv + 1;
            set_opts ();        /* Check to see if SrcChar must be passed */

            if ((OFlg) && ((var = _toupper (SrcChar[-1])) != '\0') &&
                (var == 'O') && (*SrcChar == '='))
            {
                if (O_Nam)
                {
                    errexit ("too many object files");
                }
                else
                {
                    O_Nam = ++SrcChar;
                }
            }

            if (_toupper(*SrcChar) == 'I')
            {
                char *p = ++SrcChar;

                if (*p == '=')
                {
                    p++;
                }

                incdirs[inccount++] = p;
            }
        }
        else
        {
            if (LastSrc > &SrcNam[31])
            {
                errexit ("too many input files");
            }

            *(LastSrc++) = *_argv;
        }

        ++_argv;
    }

    if (LastSrc == SrcNam)
    {
        errexit ("no input file");
    }

    if ( ! O_Nam)
    {
        O_Nam = "output.r";
    }
}

FILE *
#ifdef __STDC__
f_opn (const char *fna, char *fmo)
#else
f_opn (fna, fmo)
     char *fna,
      *fmo;
#endif
{
    FILE *fptr;

    if ( ! (fptr = fopen (fna, fmo)) )
    {
        fprintf (stderr, "\"%s\" - ", fna);
        errexit ("can't open file");
    }

    return fptr;
}

/* Open a use-file
 * Returns File * to file on success, NULL on failure
 */

FILE *
#ifdef __STDC__
u_opn(char *fna, char *fmo)
#else
u_opn(fna, fmo)
    char *fna,
         *fmo;
#endif
{
     FILE           *fptr;
     char           f[128];
     int            i;

    /* First try the raw entry */

    if ((fptr = fopen(fna, fmo)) != 0)
    {
        return fptr;
    }

    /* Try each include-path to open the use-file
     * If file not found, return NULL
     */

    for (i = 0; i < inccount; i++)
    {
        /* Prepend include dirs in front */

        sprintf(f, "%s/%s", incdirs[i], fna);

        if ((fptr = fopen(f, fmo)) != 0)
        {
            return fptr;
        }
    }

    fprintf(stderr, "\"%s\" - ", fna);
    errexit("can't open file");
    return 0;  /* Doesn't happen but it does keep gcc quiet */
}

/* ******************************************************************** *
 * pagehdr() - If we're not on Pass 2 and not listing, do nothing.      *
 *             If anything has been printed, first eject the page.      *
 *             Print header with current time, set list_line to 2       *
 * ******************************************************************** */

void
pagehdr (ZILCH)
    /*  Prints header for listing */
{
#ifdef COCO
    long now;
    struct sgtbuf tm;
#else
    time_t now;
    struct tm *tm;
#endif

    if (Pass2 && doList)
    {
        if (list_lin)
        {
            if (ff_flg)
            {
                DO_FF;
            }
            else
            {
                while (list_lin < PgDepth)
                {
                    DO_CR;
                    ++list_lin;
                }
            }
        }

#ifdef COCO
        getime (&tm);
#else
        now = time (0);
        tm = localtime (&now);
#endif
        printf ("%s  %02d/%02d/%02d  %02d:%02d", "las 0.1 (from Microware RMA V2.0)",
#ifdef COCO
                tm.t_month, tm.t_day, tm.t_year + 1900,
                tm.t_hour, tm.t_minute
#else
                tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900,
                tm->tm_hour, tm->tm_min
#endif
            );

        printf ("   %-22s Page %03d\n", SrcNam[0], ++ListPag);
        printf ("%s - %s\n\n", _nam, _ttl);
        list_lin = 3;
    }
}

/* **************************************************************** *
 * newline() - Does a newline to listing.                           *
 *             Ejects page and prints header if needed.             *
 * **************************************************************** */

void
newline (ZILCH)
{
    if (Pass2 && doList)
    {
        if ((PgDepth - 3) < list_lin)
        {
            pagehdr ();
        }

        DO_CR;
        ++list_lin;
    }
}

/* ******************************************************************** *
 * getsrcline() - Retrieve a line from a source.  The source line will  *
 *                come from either the current source file, or the      *
 *                macro file if we're reading a macro.                  *
 * Returns: 1 on successful read of either sourcefile or macro          *
 *          0 on end of sourcefile(s)                                   *
 * ******************************************************************** */

int
getsrcline (ZILCH)
{
    char _curchr;
    register char *_dest;

    /* If we are in a macro, get it from macro file,
     * else get it from sourcefile
     */

    if (getmacline () == 0)
    {
        do
        {
            if (d00c4 && (ThisFile == FrstFil))
            {
                printf ("Asm:");
            }

            _dest = SrcChar = InpBuf;

            /* Read up to end of line, EOF, or read error. A line may end
             * with LF (Unix), CR (OS-9) or CR LF (DOS/Windows); the codes
             * are written as numbers because '\n' is CR on OS-9. A line
             * longer than InpBuf is cut short rather than overrunning it,
             * and a last line with no line ending still counts.
             */

            while ((_curchr = getc (SrcFile)) != -1)
            {
                if ((_curchr == 0x0a) || (_curchr == 0x0d))
                {
                    if (_curchr == 0x0d)
                    {
                        int _next = getc (SrcFile);

                        if ((_next != 0x0a) && (_next != -1))
                        {
                            ungetc (_next, SrcFile);
                        }
                    }

                    *_dest = '\0';
                    ++ListLin;
                    return 1;
                }

                if (_dest < &InpBuf[sizeof (InpBuf) - 1])
                {
                    *(_dest++) = _curchr;
                }
            }

            if (_dest != InpBuf)    /* last line had no line ending */
            {
                *_dest = '\0';
                ++ListLin;
                return 1;
            }
        } while (DecSFil ());

        return 0;
    }
    else
    {
        return 1;
    }
}

/* ************************************************************ *
 * DecSFil() - Close current input file, decrements file stack  *
 *             and points SrcFile to previous file in stack     *
 * Returns: 0 when all sourcefiles are exhausted                *
 *          1 otherwise                                         *
 * ************************************************************ */

static int
DecSFil (ZILCH)
{
    if (ThisFile == FrstFil)
    {
        return 0;
    }

    if (fclose (SrcFile))
    {
        e_report ("file close error");
    }

    SrcFile = *(--ThisFile);
    return 1;
}

/* ******************************************** *
 * DoSymTbl() - Print Symbol table to stdout    *
 * ******************************************** */

void
DoSymTbl (ZILCH)
{
    printf ("\nSymbol Table\n");
    PgWidth -= 24;
    sym_walk (PrtSmbl);

    if (SColumn > 0)
    {
        printf ("\n");
    }

    printf ("\n");
}

/* ************************************************************ *
 * PrtSmbl() - Called by sym_walk() to format and print symbol   *
 *             description to stdout for symbol defined in      *
 *             'struct symblstr *' passed in the parameter      *
 * ************************************************************ */

static void
#ifdef __STDC__
PrtSmbl (struct symblstr *smbl)
#else
PrtSmbl (smbl)
     struct symblstr *smbl;
#endif
{
    register struct symblstr *sblptr = smbl;

    if (SColumn > 0)
    {
        printf ("  |");
    }

    printf ("  %-9s %02x %02x %04x", sblptr->symblnam,
                                     sblptr->smbltyp,
                                     (sblptr->locmsk) & 0xff,
                                     sblptr->s_ofst);

    if (((SColumn += 25) > PgWidth) || NarrowList)
    {
        printf ("\n");
        SColumn = 0;
    }
}

/* ******************************************************************** *
 * do_psect() - Interprets the psect line and sets up the ROF header    *
 *              Also, on Pass 2, establishes some of the file positions *
 *              in the output file if there is an output file           *
 *              and writes Global data                                  *
 * ******************************************************************** */

void
do_psect (ZILCH)
{
    char rof_nam[16];
    register char *destptr;
#ifdef COCO
    long now;
    struct sgtbuf tm;
#else
    time_t now;
    struct tm *tm;
#endif

    if (*SrcChar != '\0')
    {
        int _namlen;
        destptr = rof_nam;
        _namlen = 16;

        /* move psect name to local rof_nam */

        while ((_namlen--) && (*SrcChar != '\0') && (*SrcChar != ','))
        {
            *(destptr++) = *(SrcChar++);
        }

        *destptr = '\0';

        /* Type */

        if (iscomma ())
        {
            getNmbr ();
            ck_xtrnl ();
#ifdef COCO
            RofHdr.h_tylan = NmbrInt << 8;
#else
            RofHdr.h_tylan[0] = NmbrInt & 0xff;
#endif
            /* Language */

            if (iscomma ())
            {
                getNmbr ();
                ck_xtrnl ();
#ifdef COCO
                RofHdr.h_tylan |= NmbrInt;
#else
                RofHdr.h_tylan[1] = NmbrInt & 0xff;
#endif
                /* Edition */

                if (iscomma ())
                {
                    getNmbr ();
                    ck_xtrnl ();
                    RofHdr.h_edit = NmbrInt;

                    /* Stack size */

                    if (iscomma ())
                    {
                        getNmbr ();
                        ck_xtrnl ();
#ifdef COCO
                        RofHdr.h_stack = NmbrInt;
#else
                        storInt (RofHdr.h_stack, NmbrInt);
#endif
                        /* Code entry address */

                        if (iscomma ())
                        {
                            getNmbr ();
                            ck_xtrnl ();
#ifdef COCO
                            RofHdr.h_entry = NmbrInt;
#else
                            storInt (RofHdr.h_entry, NmbrInt);
#endif
                        }
                    }
                }
            }
        }
    }
    else
    {
        strcpy (rof_nam, "program");
    }

#ifdef COCO
    getime (&tm);
    _strass (RofHdr.h_date, &tm, 5);
#else
    now = time (0);
    tm = localtime (&now);
    RofHdr.h_date[0] = (tm->tm_year) & 0xff;
    RofHdr.h_date[1] = (tm->tm_mon) & 0xff;
    RofHdr.h_date[2] = (tm->tm_mday) & 0xff;
    RofHdr.h_date[3] = (tm->tm_hour) & 0xff;
    RofHdr.h_date[4] = (tm->tm_min) & 0xff;
#endif
    RofHdr.h_valid = HadError;
    RofHdr.h_spare = rversion;
#ifdef COCO
    RofHdr.h_udata = U_data;
    RofHdr.h_udpd = U_dpd;
    RofHdr.h_idata = I_data;
    RofHdr.h_idpd = I_dpd;
    RofHdr.h_ocode = CodeSize;
#else
    storInt (RofHdr.h_udata, U_data);
    storInt (RofHdr.h_udpd, U_dpd);
    storInt (RofHdr.h_idata, I_data);
    storInt (RofHdr.h_idpd, I_dpd);
    storInt (RofHdr.h_ocode, CodeSize);
#endif

    /* If applicable:
     * Write ROF header and write Global data info
     * Initialize file positions for begin of code, idpd, and idat
     */

    if (Pass2 && OutFile)
    {
        RofHdLoc = ftell (OutFile);     /*  ?? OutFile assumed */
        fwrite (&RofHdr, sizeof (RofHdr), 1, OutFile);
        fputs (rof_nam, OutFile);
        putc ('\0', OutFile);
        /*fwrite( &GlobCnt, 2, 1, OutFile ); */
        PUTINT (GlobCnt, OutFile);

        if (GlobCnt)
        {
            sym_walk (wrt_global);
        }

        O_idatBegin = (O_idpdBegin = (OCodeBegin = CurFPos = ftell (OutFile)) +
                 (long) CodeSize) + (long) I_dpd;
    }
}

static void
ck_xtrnl (ZILCH)
{
    if (Pass2 && RefExtern)
    {
        e_report ("no external allowed");
    }
}


void
wrtvalid (ZILCH)                 /* Write Valid into ROF header */
{
    unsigned hd_sz = (unsigned) (&(RofHdr.h_valid)) - (unsigned) (&RofHdr);

    if ((OutFile == 0) || (OFlg <= 0))
    {
        return;
    }

    fseek (OutFile, (RofHdLoc + (long) hd_sz), BEGIN);
    putc (HadError, OutFile);
    CurFPos = RofHdLoc + (long) hd_sz + 1;
}


void
#ifdef __STDC__
w_ocod (char *_codaddr, int _codlen)
#else
w_ocod (_codaddr, _codlen)
     char *_codaddr;
     int _codlen;
#endif
{
    register long *_filpos;

    if (S_Addrs && _codlen)
    {
        *S_Addrs += _codlen;

        if (InModule || InTop)
        {
            mod_write (_codaddr, _codlen);
            return;
        }

        if ( (Pass2) && (DLocField & DIRENT) && (OutFile) && (OFlg > 0) )
        {
            if (WhereLoc & CODENT)
            {
                _filpos = &OCodeBegin;
            }
            else
            {
                if (WhereLoc & DIRENT)
                {
                    _filpos = &O_idpdBegin;
                }
                else
                {               /* INIENT (clear/init data) */
                    _filpos = &O_idatBegin;
                }
            }

            if (CurFPos != *_filpos)
            {
                fseek (OutFile, *_filpos, BEGIN);
            }

            fwrite (_codaddr, _codlen, 1, OutFile);
            CurFPos = *_filpos = *_filpos + (long) _codlen;
        }
    }
}

/* **************************************************************** *
 * wrt_refs() - Write all external, local, dp, non-dp refs          *
 *              and common symbols if applicable                    *
 * **************************************************************** */

void
wrt_refs (ZILCH)
{
    if (OutFile)
    {
        fseek (OutFile, O_idatBegin, BEGIN);

        /* Write external references */

        PUTINT (ref_cnt, OutFile);

        if (ref_cnt)
        {
            sym_walk (wrtxtrn);
        }

        /* Write local count */

        PUTINT (locl_cnt, OutFile);

        if (locl_cnt)                   /* Write locals, if any */
        {
            if (LRefs_Code.reftbl)              /* refs to code */
            {
                do_reflist (LRefs_Code.reftbl);
            }

            if (LRefs_DPD.reftbl)              /* refs to dp data */
            {
                do_reflist (LRefs_DPD.reftbl);
            }

            if (LRefs_NonDPdat.reftbl)              /* refs to non-dp data */
            {
                do_reflist (LRefs_NonDPdat.reftbl);
            }
        }

        /* Write commons if rof version > 0 */

        if (rversion)
        {
            /*fwrite( &comn_cnt, 2, 1, OutFile ); */
            PUTINT (comn_cnt, OutFile);

            if (comn_cnt)
            {
                sym_walk (wrtCmmns);
            }
        }
    }
}

/* ************************************************************ *
 * wrt_global() - sym_walk function.                             *
 *                Writes an entry contained in the symblstr     *
 *                passed as a parameter, into the ROF file      *
 * ************************************************************ */

static void
#ifdef __STDC__
wrt_global (struct symblstr *parm)
#else
wrt_global (parm)
     struct symblstr *parm;
#endif
{
    register struct symblstr *reg = parm;

    short sbloffst;
    char styp;

    /* if bit for 0x40 (NEGMASK) NOT SET */

    if ( (reg->locmsk & 0xc6) == (RELATIVE | CODENT | DIRENT) &&
         (reg->smbltyp != (CODENT | DIRENT | INIENT)) )
    {
        fputs (reg->symblnam, OutFile);
        putc ('\0', OutFile);           /* Null-termiat name */

        /* write symbol type (8-bit), offset (16-bit) */

        styp = reg->smbltyp;
        sbloffst = reg->s_ofst;

        /* rlink only accepts a constant for a plain value; an exported
         * "set" symbol's final value is just that (lwasm mode, where the
         * export pragma exports set symbols too) */

        if (LwasmMode && ((styp & 0x0f) == (CODENT | INIENT)))
        {
            styp = (styp & 0xf0) | CODENT | DIRENT;
        }
#ifdef COCO
        fwrite( &styp, 3, 1, OutFile );
#else
        putc (styp, OutFile);
        PUTINT (sbloffst, OutFile);
#endif
    }
}

/* ******************************************************** *
 * CountRfs() : counts references and apparently reverses   *
 *              the direction ->NxtRef points in the line   *
 * Used for externals and commons                           *
 * ******************************************************** */


static int
CountRfs
#ifdef __STDC__
    (struct symblstr *smbls)
#else
    (smbls)
    struct symblstr *smbls;
#endif
{
    register struct ref_xtrnal *_curnt;
    struct ref_xtrnal *next;
    int _refcount;

    _refcount = 0;
    next = smbls->reftbl;          /* init next */
    smbls->reftbl = 0;

    while ( (_curnt = next) )
    {
        next = _curnt->NxtRef;
        _curnt->NxtRef = smbls->reftbl;     /* Point NxtRef to prev ref    */
        smbls->reftbl = _curnt;             /* point smbls->reftbl to last */
        ++_refcount;
    }

    return _refcount;
}

static void
wrtxtrn 
#ifdef __STDC__
    (struct symblstr *parm1)
#else
    (parm1)
     struct symblstr *parm1;
#endif
{
    register struct symblstr *smbldef = parm1;
    short numrefs;

    if (    ! ((smbldef->locmsk & (NEGMASK | DIRENT)))
         &&   (smbldef->reftbl)
         &&   ((numrefs = smbldef->smbltyp & 0x0f) != (CODENT | DIRENT))
         &&   (numrefs != (CODENT | INIENT)))
    {
        /* Write null-terminated symbol name */
        fputs (smbldef->symblnam, OutFile);
        putc ('\0', OutFile);

        numrefs = CountRfs (smbldef);   /* Ref count */
        PUTINT (numrefs, OutFile);

        /* List all references to this name */

        do_reflist (smbldef->reftbl);
    }
}

/* ******************************************************************** *
 * do_reflist() - Writes the whole list of references passed in the     *
 *                parameter                                             *
 * ******************************************************************** */

static void
#ifdef __STDC__
do_reflist (struct ref_xtrnal *parm)
#else
do_reflist (parm)
     struct ref_xtrnal *parm;
#endif
{
    register struct ref_xtrnal *sref = parm;

    while (sref)
    {
#   ifdef COCO
        fwrite (sref, 3, 1, OutFile);
#   else
        putc (sref->RfTyp, OutFile);
        PUTINT ((int) sref->r_offset, OutFile);
#   endif
        sref = sref->NxtRef;
    }
}

void
#ifdef __STDC__
wrtCmmns (struct symblstr *parm)
#else
wrtCmmns (parm)
     struct symblstr *parm;
#endif
{
    register struct symblstr *reg = parm;
    int var;

    /* DIRENT && CODENT && RELATIVE && ! NEGMASK */

    if (((reg->locmsk & 0xc6) == (RELATIVE | CODENT | DIRENT)) &&
         ((reg->smbltyp & 0x0f) == 7))    /* Code for Common */
    {
        fputs (reg->symblnam, OutFile);
        putc ('\0', OutFile);

        PUTINT (reg->s_ofst, OutFile); /* Address of common  */

        var = CountRfs (reg);          /* Count refs to this */
        PUTINT (var, OutFile);

        do_reflist (reg->reftbl);      /* Write all refs     */
    }
}
