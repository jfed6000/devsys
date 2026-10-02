/* **************************************************************** *
 * lwobj.c - lwasm's object format ("section" files) as ROF         *
 *                                                                  *
 * In lwasm mode a source that uses "section" is assembled to ROF,  *
 * for rlink, the way lwasm --format=obj assembles it to LWOBJ for  *
 * lwlink. The file becomes one psect:                              *
 *                                                                  *
 *   code, .text, data, .data, others  the psect's code             *
 *   bss, .bss, name,bss               a vsect (uninitialised data) *
 *   _constant(s), name,constant       constants, counted from 0    *
 *                                     (like a csect)               *
 *   __os9                             type, lang, attr, rev,       *
 *                                     edition and stack settings   *
 *                                     for the module header        *
 *                                                                  *
 * The psect header carries what lwlink would put in the module     *
 * header: from __os9 if the file has one, else lwlink's defaults   *
 * (type 1, lang 1, attr 8, rev 0); the entry point is __start.     *
 * Only a file with __start or __os9 gets a module type; library    *
 * members get 0, as in Microware's clib.l, so rlink sees one root. *
 *                                                                  *
 * With the export pragma, every non-local label defined inside a   *
 * section is exported (do_label), as lwasm does. Symbols that are  *
 * never defined become external references, which RMA already      *
 * does in a psect (lwasm's undefextern).                           *
 * **************************************************************** */

#define NEED_CTYPE
#include "rma.h"

#define SEC_NONE    0
#define SEC_CODE    1
#define SEC_BSS     2
#define SEC_CONST   3
#define SEC_OS9     4

/* RMA's _tolower is (c | 0x20), which also changes '_' into 0x7f */

#define LOWER(c) ((((c) >= 'A') && ((c) <= 'Z')) ? ((c) + ('a' - 'A')) : (c))

static int SecKind;             /* the section we are in */
static char SecOrgBad;          /* after an unresolvable org (sec_org) */
static char SecOpen;            /* the file's psect has been opened */

/* ******************************************************************** *
 * sec_pass() - Reset at the start of each pass.                        *
 * ******************************************************************** */

void
sec_pass (ZILCH)
{
    SecKind = SEC_NONE;
    SecOpen = 0;
    InSection = 0;
    SecOrgBad = 0;

    if ( ! Pass2)
    {
        SawOs9Sec = 0;
    }
}

/* ******************************************************************** *
 * symval() - Value of a symbol defined in pass 1, or dflt.             *
 * ******************************************************************** */

static int
#ifdef __STDC__
symval (char *name, int dflt)
#else
symval (name, dflt)
     char *name;
     int dflt;
#endif
{
    struct symblstr *sym = sym_find (name);

    return (sym && (sym->locmsk & RELATIVE)) ? (int) sym->s_ofst : dflt;
}

/* ******************************************************************** *
 * open_psect() - The first section in a file opens its psect. The      *
 *          header values are only written in pass 2, by when pass 1    *
 *          has defined __os9's symbols and __start.                    *
 * ******************************************************************** */

static void
open_psect (ZILCH)
{
    char buf[SYMLEN + 64];
    char name[16];
    register const char *p, *base;
    register int n;
    int type, lang, attr, rev;
    char *save;

    /* psect name: the source file's name, without directories or
     * extension, at most 15 characters */

    for (base = p = CurSrcName; *p; ++p)
    {
        if (*p == '/')
        {
            base = p + 1;
        }
    }

    for (n = 0; base[n] && (base[n] != '.') && (n < 15); ++n)
    {
        name[n] = base[n];
    }

    name[n] = '\0';

    if ( ! n)
    {
        strcpy (name, "program");
    }

    if (SawOs9Sec || symval ("__start", -1) >= 0)
    {
        /* lwlink's defaults, overridden by __os9. Only read these names
         * when the file has a __os9 section: lwlink only looks inside it,
         * and a program may have an ordinary label called "stack". */

        type = SawOs9Sec ? symval ("type", 1) : 1;
        lang = SawOs9Sec ? symval ("lang", 1) : 1;
        attr = SawOs9Sec ? symval ("attr", 8) : 8;
        rev = SawOs9Sec ? symval ("rev", 0) : 0;

        if (type > 15)
        {
            type >>= 4;         /* written as Prgrm ($10) */
        }

        if (attr > 15)
        {
            attr >>= 4;         /* written as ReEnt ($80) */
        }

        sprintf (buf, "%s,%d,%d,%d,%d,%d", name,
                 ((type & 15) << 4) | (lang & 15),
                 ((attr & 15) << 4) | (rev & 15),
                 SawOs9Sec ? (symval ("edition", 0) & 0xff) : 0,
                 SawOs9Sec ? symval ("stack", 0) : 0,
                 symval ("__start", 0));
    }
    else
    {
        sprintf (buf, "%s,0,0,0,0,0", name);
    }

    InTop = 0;
    save = SrcChar;
    SrcChar = buf;
    _psect ();
    SrcChar = save;
    SecOpen = 1;
}

/* ******************************************************************** *
 * os9_marker() - After a __os9 section: export the constant __os9,     *
 *          bit 0 set if it gave an edition. rlink --lwlink writes the  *
 *          edition byte only then, as lwlink does.                     *
 * ******************************************************************** */

static void
os9_marker (ZILCH)
{
    static char mname[] = "__os9";
    struct symblstr *sym = sym_find (mname);

    if ( ! sym)
    {
        sym = sym_add (mname);
        ++GlobCnt;                      /* one more global in the ROF */
    }

    sym->smbltyp = CODENT | DIRENT;     /* a constant */
    sym->locmsk = (sym->locmsk & LOC1BYT) | RELATIVE | CODENT | DIRENT;
    sym->s_ofst = (symval ("edition", -1) >= 0) ? 1 : 0;
}

/* ******************************************************************** *
 * leave_section() - Back to the psect's code context.                  *
 * ******************************************************************** */

static void
leave_section (ZILCH)
{
    if (SecKind == SEC_OS9)
    {
        os9_marker ();
    }

    if (SecKind != SEC_CODE)
    {
        codsetup ();
    }

    SecKind = SEC_NONE;
    InSection = 0;
}

/* ******************************************************************** *
 * do_section() - "section name[,flags]"                                *
 * ******************************************************************** */

int
do_section (ZILCH)
{
    char name[32], flags[32];
    register int n;

    NumBytes = 0;

    for (n = 0; *SrcChar && (*SrcChar != ',') && ! isblank (*SrcChar) &&
                (n < 31); ++SrcChar)
    {
        name[n++] = LOWER (*SrcChar);
    }

    name[n] = '\0';
    flags[0] = '\0';

    if (*SrcChar == ',')
    {
        ++SrcChar;

        for (n = 0; *SrcChar && ! isblank (*SrcChar) && (n < 31); ++SrcChar)
        {
            flags[n++] = LOWER (*SrcChar);
        }

        flags[n] = '\0';
    }

    if ( ! name[0])
    {
        return e_report ("section name expected");
    }

    if ( ! SecOpen)
    {
        open_psect ();
    }

    leave_section ();
    InSection = 1;
    SecOrgBad = 0;

    if (strcmp (name, "__os9") == 0)
    {
        SecKind = SEC_OS9;
        SawOs9Sec = 1;
    }
    else if ((strcmp (name, "_constant") == 0) ||
             (strcmp (name, "_constants") == 0) ||
             (strcmp (flags, "constant") == 0))
    {
        SecKind = SEC_CONST;
    }
    else if ((strcmp (name, "bss") == 0) || (strcmp (name, ".bss") == 0) ||
             (strcmp (flags, "bss") == 0))
    {
        SecKind = SEC_BSS;
    }
    else
    {
        SecKind = SEC_CODE;
    }

    if ((SecKind == SEC_OS9) || (SecKind == SEC_CONST))
    {
        char *save = SrcChar;

        SrcChar = "";       /* constants counted from 0 */
        csectsetup ();
        SrcChar = save;
    }
    else if (SecKind == SEC_BSS)
    {
        char *save = SrcChar;

        SrcChar = "";       /* not "dp" */
        set_vsec ();
        SrcChar = save;
    }

    return 1;
}

/* ******************************************************************** *
 * do_endsect() - "endsect" / "endsection"                              *
 * ******************************************************************** */

int
do_endsect (ZILCH)
{
    NumBytes = 0;
    leave_section ();
    return 1;
}

/* ******************************************************************** *
 * sec_export() - Should do_label export a label defined now? lwasm     *
 *          with the export pragma exports every non-local symbol in a  *
 *          section; __os9's settings stay local (lwlink reads them).   *
 * ******************************************************************** */

int
#ifdef __STDC__
sec_export (char *name)
#else
sec_export (name)
     char *name;
#endif
{
    if ( ! (PragExport && InSection) || (SecKind == SEC_OS9) || SecOrgBad)
    {
        return 0;
    }

    for (; *name; ++name)
    {
        if ((*name == '@') || (*name == '?'))
        {
            return 0;
        }
    }

    return 1;
}

/* ******************************************************************** *
 * sec_org() - "org" in a constant section sets its counter (the defs   *
 *             use org/rmb for structure offsets).                      *
 * ******************************************************************** */

int
sec_org (ZILCH)
{
    NumBytes = 0;

    if ( ! getNmbr ())
    {
        return 0;
    }

    /* An org lwasm can't resolve (e.g. "ORG V.User" when only V.USER is
     * defined, without nosymbolcase) isn't an error there: the labels
     * that follow simply aren't exported. Do the same. */

    SecOrgBad = (OptBPtr != d0791);
    OptBPtr = d0791;
    CsectOrg = SecOrgBad ? 0 : (NmbrInt & 0xffff);
    *(Adrs_Ptr = &CodeAddrs) = CsectOrg;
    return 1;
}

/* ******************************************************************** *
 * sec_rmb() - "rmb" in lwasm's object format: in a code-like section   *
 *          (e.g. .data) it reserves zero bytes in the module image, as *
 *          lwasm writes them; in bss and constant sections it just    *
 *          moves the counter, as RMA's rmb does.                       *
 * ******************************************************************** */

int
sec_rmb (ZILCH)
{
    static char zeros[4];
    long count;

    NumBytes = 0;

    if (SecKind != SEC_CODE)
    {
        return do_rmb ();
    }

    if ( ! l1723 ())
    {
        return 0;
    }

    count = NmbrInt;
    CmdCod = zeros;

    while (count-- > 0)
    {
        addzero ();
    }

    return 1;
}
