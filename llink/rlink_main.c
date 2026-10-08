/* rlink_main.c - mainline routine to build llink (devsys fork of rlink) */

#define MAIN

#include <stdlib.h>
#include "rlink.h"

int tidy();

static char *basenam();
static B09fatal();
static int lw_detect();

int
main (argc, argv)
    int argc; char **argv;
{
    int curopt;
    int parmlen;
    int v2;
    int v1;
    unsigned int v0;

#ifdef _OS9
    intercept (tidy);
#endif
    filenames = argv;

    while (--argc > 0)
    {
        register char *curarg;

        if (strcmp (*(argv + 1), "--lwlink") == 0)
        {
            /* lwlink's module layout, for modules built from lwasm-style
             * sources by las (see wrtlwmod in rl_03.c) */

            LwLink = LayoutSet = 1;
            ++argv;
            continue;
        }

        if (strcmp (*(argv + 1), "--mw") == 0)
        {
            LwLink = 0;     /* Microware's layout, whatever the objects */
            LayoutSet = 1;
            ++argv;
            continue;
        }

        if (*(curarg = *(++argv)) == '-')
        {
            ++curarg;
            parmlen = 0;

            while (curopt = *(curarg++))
            {
                if (parmlen != 0)
                {
                    fatal("unused '=' in %s", *argv);
                }

                if (*curarg == '=')
                {
                    ++curarg;
                    parmlen = 1;
                }

                switch (curopt)
                {
                    case 'b':       /* Basic09 entry point */
                        B09Mod = 1;

                        if (parmlen)
                        {
                            B09EntPt = curarg;
                        }
                        break;
                    case 'E':
                    case 'e':       /* Edition           */
                        edition = atoi (curarg);
                        break;
                    case 'l':       /* library           */
                        if (lfilenum < 16)
                        {
                            lpos[lfilenum] = FileCount;  /* --lwlink order */
                            lfiles[lfilenum++] = curarg;
                        }
                        else
                        {
                            fatal ("too many libraries: -l%s\n", curarg);
                        }
                        break;
                    case 'M':       /* Extra memory      */
                        v0 = req_mem (curarg);

                        if (v0 >= 256)
                        {
                            extramem = v0;
                        }
                        break;
                    case 'm':       /* Print linkage map */
                        printmap = 1;
                        continue;
                    case 'n':       /* Name */
                        modname = curarg;
                        break;
                    case 'o':       /* Output object file */
                        OutfName = curarg;

                        if (modname == 0)
                        {
                            modname = basenam (curarg);
                        }
                        break;
                    case 's':       /* Print symbol table */
                        printsym = 1;
                        printmap = 1;
                        continue;  /* To inner while */
                    case 't':       /* Allow static data in BASIC09 modules */
                        okstatic = 1;
                        continue;  /* To inner while */
                    case 'z':
                        if (parmlen)
                        {
                            NameFile = curarg;
                            break;   /* To inner while */
                        }
                        continue;
                    default:
                        fatal ("unknown option %c in %s", (char *)curopt, *argv);
                        break;
                }
                break;
            }
        }
        else        /* no '-', it's a filename */
        {
            filenames[FileCount++] = curarg;
        }
    }
    /* This conditional could be removed when need to compare
     * with original is no longer expected */
#ifndef _OS9
    /* This comes later in original but should be done now */
    if (!OutfName)
    {
        fatal("no output file");
    }

    if ((FileCount == 0) && !NameFile)
        fatal("no input files");

#endif

    if (!LayoutSet)
    {
        LwLink = lw_detect();   /* the mainline's layout (D42) */
    }

    if (LwLink)
    {
        lw_select();    /* lwlink's choice and order of modules (lwsel.c) */
    }

    readrofs();   /* 02f0 */

    if (unrsolved())
    {
        fatal("unresolved references");
    }

    namelen = strlen (modname);
    adj_ofst();
    adjsmofst();

    if (B09Mod) /* else L0345 */
    {
        if (t_idat ||  t_idpd)
        {
            B09fatal ("no init data allowed");
        }

        if ( t_udpd)
        {
            B09fatal ("no dp data allowed");
        }

        if (!okstatic && t_udat)
        {
            B09fatal ("no static data");
        }
    }

    if (( t_udpd +  t_idpd > 256)) /* L0345 */
    {
        fatal ("direct page allocation is %u bytes",
                (char *)( t_udpd +  t_idpd));
    }

    if (dup_find)  /* L035c */
    {
        fatal ("name clash");
    }

    if (!OutfName)
    {
        fatal("no output file");
    }

    if (B09Mod) /* L0378 - else L03c3 */
    {
        if (! B09EntPt)
        {
            B09fatal ("no entry point name");  /* jump to L03be */
        }
        else
        {
            SYMDAT *p0 =  lookup (B09EntPt);     /* some struct pointer */

            if ((p0 != 0) && ((p0->Type & 7) == CODENT))
            {   /* Analyze struct */
                D002f = p0->Offset;
            }
            else
            {
                B09fatal ("entry point '%s' not found", B09EntPt);
            }
        }
        
        TyLang = 0x2181;    /* L03be */
    }
    
    wrtmodule ();   /* L03c3 */
    lw_cleanup ();
}

/* ================================================================ *
 * Handles opening of file ptr and allocates memory for the file    *
 * structure                                                        *
 * First gets all files from commandline, then reads from names     *
 * file if provided by "z" option                                   *
 * Returns pointer to filename                                      *
 * ================================================================ */

char *
nxtfnam ()
{
    char *fnam;

    if (CurntFil < FileCount)   /* filenames come from command line */
    {
        return filenames[CurntFil++];
    }

    if (!NameFile) /* filenames come from a file */
    {
        return 0;
    }

    if (!NamFilFP)
    {
        if ((NamFilFP = fopen(NameFile, "r")) == 0)
        {
            fatal("can't open '%s' name file", NameFile);
        }
    }

    if (_fgets(fnam = grab (32), NamFilFP))
    {
        return fnam;
    }

    return 0;
}

char *
nxtlib ()
{
    if (LibFile < lfilenum)   /* else L045c */
    {
        return lfiles[LibFile++];
    }

    return 0;
}

static
B09fatal (msg, fmt)
char *msg, *fmt;
{
/*    int m1;
    int m2;*/

    fprintf (stderr, msg, fmt);
    putc ('\n', stderr);
    fatal ("BASIC09 conflict");
}

illmainline ()
{
    B09fatal ("no mainline allowed");
}

cantopen ()
{
    fatal ("can't open '%s'", RofFil);
}

notROF ()
{
    fatal ("'%s' is not a relocatable module", RofFil);
}

asmerrs ()
{
    fatal ("'%s' contains assembly errors", RofFil);
}

toonew ()
{
    fatal ("'%s' is too new for this version of assembler", RofFil);
}

twomains ()
{
    fatal ("mainline found in both %s and %s", FirstMod->roffile, RofFil);
}

in_rderr ()
{
    fatal ("error reading input file %s", RofFil);
}

static char *
basenam (arg)
register char *arg;
{
    int   curchr;
    char *path = arg;

    while ((curchr = *(arg++)))
    {
        if (curchr == '/')
        {
            path = arg;
        }
    }

    return path;
}

#ifdef __LIL_END__
unsigned int
o9_int (val)
UINT val;
{
    return ((val & 0xff00) >> 8) + ((val & 0xff) << 8);
}
#endif


/* ******************************************************************** *
 * lw_detect() - With neither --lwlink nor --mw: lwlink's layout if the *
 *               mainline (the input module with a type/language) was   *
 *               made by las from lwasm-style sources, which export the *
 *               constant __lwsec (D17); else Microware's. Reads the    *
 *               input files' headers and globals a byte at a time, up  *
 *               to the mainline: on the 6809 memory is short. A -z     *
 *               file of names isn't read; no mainline found: Microware *
 * ******************************************************************** */

static int
dt_word (fp)
    FILE *fp;
{
    int hi = getc (fp);

    return (hi << 8) | (getc (fp) & 0xff);
}

static void
dt_skip (fp, n)
    FILE *fp;
    unsigned n;
{
    while (n-- && (getc (fp) != EOF))
    {
    }
}

static int
lw_detect ()
{
    static char lwsec[] = "__lwsec";
    FILE *fp;
    unsigned char h[28];
    unsigned tylan, vers, code, n, k;
    int i, c, j, lw;

    for (i = 0; i < FileCount; ++i)
    {
        if (!(fp = fopen (filenames[i], "r")))
        {
            continue;           /* readrofs reports it */
        }

        while (fread (h, sizeof (h), 1, fp) == 1)
        {
            if ((h[0] != 0x62) || (h[1] != 0xcd) || (h[2] != 0x23) ||
                    (h[3] != 0x87))
            {
                break;          /* not a ROF: readrofs reports it */
            }

            tylan = (h[4] << 8) | h[5];
            vers = h[13];
            code = ((h[22] << 8) | h[23]) + ((h[18] << 8) | h[19]) +
                   ((h[20] << 8) | h[21]);

            while ((c = getc (fp)) > 0)
            {
            }                   /* the module name */

            lw = 0;

            for (n = dt_word (fp); n; --n)      /* globals */
            {
                j = 0;

                while ((c = getc (fp)) > 0)
                {
                    if ((j >= 0) && (lwsec[j] == c))
                    {
                        ++j;
                    }
                    else
                    {
                        j = -1;
                    }
                }

                if (j == sizeof (lwsec) - 1)
                {
                    lw = 1;
                }

                dt_skip (fp, 3);        /* type, value */
            }

            if (tylan)
            {
                fclose (fp);
                return lw;              /* the mainline decides */
            }

            dt_skip (fp, code);

            for (n = dt_word (fp); n; --n)      /* external references */
            {
                while ((c = getc (fp)) > 0)
                {
                }

                dt_skip (fp, 3 * dt_word (fp));
            }

            dt_skip (fp, 3 * dt_word (fp));     /* local references */

            if (vers > 0)                       /* common blocks */
            {
                for (n = dt_word (fp); n; --n)
                {
                    while ((c = getc (fp)) > 0)
                    {
                    }

                    k = dt_word (fp);           /* size */
                    dt_skip (fp, 3 * dt_word (fp));
                }
            }
        }

        fclose (fp);
    }

    return 0;
}
