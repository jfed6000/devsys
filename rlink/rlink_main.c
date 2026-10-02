/* rlink_main.c - mainline routine to build rlink */

#define MAIN

#include <stdlib.h>
#include "rlink.h"

int tidy();

static char *basenam();
static B09fatal();

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

            LwLink = 1;
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
o9_int(UINT val)
{
    return ((val & 0xff00) >> 8) + ((val & 0xff) << 8);
}
#endif
