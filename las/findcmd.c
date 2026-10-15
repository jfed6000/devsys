/* ************************************************ *
 * Part2 of c source for "rma"                      *
 * $Id::                                        $   *
 * ************************************************ */

#include "rma.h"

/* **************************************************************** *
 * cmdinit - sorts each table by its entries' first letter, once at *
 * start (D64).  An entry can match only a mnemonic that starts     *
 * with the entry's first letter, and FindCmd takes the first match *
 * in table order.  The sort is stable (entries with the same first *
 * letter keep their order), so a binary search for the letter and  *
 * a scan of its entries find what a scan of the whole table found. *
 * **************************************************************** */

static void
#ifdef __STDC__
cmdsort (CODFORM *tbl, CODFORM *end)
#else
cmdsort (tbl, end)
     CODFORM *tbl, *end;
#endif
{
    CODFORM *p, *q;
    char *cmd, op, ofs;

    for (p = tbl + 1; p < end; ++p)
    {
        cmd = p->ccmd;          /* (DCC has no structure assignment) */
        op = p->op_cod;
        ofs = p->tofst;

        for (q = p; (q > tbl) && (*(q - 1)->ccmd > *cmd); --q)
        {
            q->ccmd = (q - 1)->ccmd;
            q->op_cod = (q - 1)->op_cod;
            q->tofst = (q - 1)->tofst;
        }

        q->ccmd = cmd;
        q->op_cod = op;
        q->tofst = ofs;
    }
}

/* The instruction and directive tables, asked about on nearly every
 * line, also get a start per letter: their entries starting with letter
 * 'a'+k are those from first[k] to first[k+1] (the number of entries
 * whose first character is below 'a'+k, 'a'+k+1). */

static unsigned char rc_first[27], nm_first[27];

/* A string's first two characters as one number (one 16-bit load on
 * the 6809), for comparing them at once in FindCmd */

#ifdef COCO
#define PAIR(p) (*(int *)(p))
#else
#define PAIR(p) ((((unsigned char *)(p))[0] << 8) | ((unsigned char *)(p))[1])
#endif

static void
#ifdef __STDC__
cmdfirst (CODFORM *tbl, CODFORM *end, unsigned char *first)
#else
cmdfirst (tbl, end, first)
     CODFORM *tbl, *end;
     unsigned char *first;
#endif
{
    CODFORM *p;
    int k;

    for (k = 0; k < 27; ++k)
    {
        for (p = tbl; (p < end) && (*p->ccmd < 'a' + k); ++p)
        {
        }

        first[k] = p - tbl;
    }
}

void
cmdinit (ZILCH)
{
    cmdsort (S_Psec, PSecEnd);
    cmdsort (S_realcmds, SLbrEnd);
    cmdsort (S_Module, SModEnd);
    cmdsort (S_TopLw, STopLwEnd);
    cmdsort (S_LwSect, SLwSectEnd);
    cmdsort (S_LwZero, SLwZeroEnd);
    cmdsort (S_Rmb, S_Rmbend);
    cmdsort (Data_Equ, DtaEqEnd);
    cmdsort (S_Rmb3, Rmb3End);
    cmdsort (S_Nam, SNamEnd);
    cmdsort (S_branch, SBsr_End);
    cmdfirst (S_realcmds, SLbrEnd, rc_first);
    cmdfirst (S_Nam, SNamEnd, nm_first);
}

/* **************************************************************** *
 * FindCmd - parses one of the CODFORM lists for a match of a cmd   *
 * returns: address of CODFORM if found,   NULL if no match         *
 * The first entry (in source order) that is a prefix of the        *
 * mnemonic; the tables are sorted by first letter (cmdinit).       *
 * **************************************************************** */

CODFORM *
#ifdef __STDC__
FindCmd (char *src_str, CODFORM *cod_strt, CODFORM *cod_stop)
#else
FindCmd (src_str, cod_strt, cod_stop)
     char *src_str;
     CODFORM *cod_strt,
             *cod_stop;
#endif
{
    CODFORM *cod_ptr, *hi;
    register char *cmp_str;
    unsigned char *first;
    char *q, c;
    int w;

    /* The entries for the mnemonic's first letter (lower case, as the
     * copy below makes it): from the index, or by binary search */

    if ((c = *src_str) == '\0')
    {
        return 0;       /* (every entry has a name) */
    }

    if ( ! islower ((int)c))
    {
        c = _tolower (c);
    }

    first = (cod_strt == S_realcmds) ? rc_first
          : (cod_strt == S_Nam) ? nm_first : 0;

    if (first && (c >= 'a') && (c <= 'z'))
    {
        first += c - 'a';
        cod_stop = cod_strt + first[1];
        cod_strt += first[0];
    }
    else
    {
        hi = cod_stop;

        while (cod_strt < hi)
        {
            cod_ptr = cod_strt + ((hi - cod_strt) >> 1);

            if (*cod_ptr->ccmd < c)
            {
                cod_strt = cod_ptr + 1;
            }
            else
            {
                hi = cod_ptr;
            }
        }
    }

    if ((cod_strt >= cod_stop) || (*cod_strt->ccmd != c))
    {
        return 0;       /* no entry starts with it: nothing to copy */
    }

    /* Copy lower-case chars from src_str to CmdMnemonic */

    cmp_str = CmdMnemonic;

    while ((c = *src_str++) != '\0')
    {
        if ( ! islower ((int)c))
        {
            c = _tolower (c);
        }

        *cmp_str++ = c;
    }

    *cmp_str = '\0';

    /* Scan the letter's entries, the first two characters compared at
     * once: an entry of two or more whose pair differs can't match */

    c = CmdMnemonic[0];
    w = PAIR (CmdMnemonic);

    for (cod_ptr = cod_strt; cod_ptr < cod_stop; ++cod_ptr)
    {
        cmp_str = cod_ptr->ccmd;

        if (PAIR (cmp_str) != w)
        {
            if (*cmp_str != c)
            {
                break;          /* past the letter's entries */
            }

            if (cmp_str[1])
            {
                continue;
            }
        }

        /* The first character matches (and the second, unless the entry
         * ends there): compare the rest.  CmdNamEnd is read only after a
         * match, so it is set only then. */

        for (q = CmdMnemonic + 1, ++cmp_str; *cmp_str && (*cmp_str == *q);
             ++cmp_str, ++q)
        {
        }

        if (*cmp_str == '\0')
        {
            CmdNamEnd = q;
            return cod_ptr;
        }
    }

    return 0;
}
