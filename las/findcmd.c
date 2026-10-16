/* ************************************************ *
 * Part2 of c source for "rma"                      *
 * $Id::                                        $   *
 * ************************************************ */

#define NEED_CTYPE              /* isblank, notblank (MovLbl, SkipSpac) */
#include "rma.h"

#define _ischar(c) (_chcodes[(int)c]&(_DIGIT|_LOWER|_UPPER|_CONTROL))

#ifdef COCO
char *lblcopy ();
CODFORM *fcscan ();
#endif

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

#ifdef COCO
    return fcscan (src_str, cod_strt, cod_stop);    /* below (step 27) */
#else
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
#endif
}

/* **************************************************************** *
 * MovLbl and SkipSpac scan every line, often several times (te:    *
 * 24,054 and 57,917 calls). They were in labels.c; here they are   *
 * compiled without stack checks, like the rest of this file        *
 * (las_NOCHECK, step 27). Neither recurses: SkipSpac calls nothing *
 * and MovLbl only strncmp, so the margin its checked caller        *
 * reserved (the caller's frame + 64 bytes) covers MovLbl's 16-byte *
 * frame and strncmp's few.                                         *
 * **************************************************************** */

/* ******************************************************************** *
 * MovLbl() - Moves a label to destpos (parameter) if the first char    *
 *            in the buffer is a legal label char.                      *
 * Returns: (1) if label found and moved                                *
 *          (0) if no label                                             *
 * ******************************************************************** */

int
#ifdef __STDC__
MovLbl (char *destpos)
#else
MovLbl (destpos)
     char *destpos;
#endif
{
    int count;

    if (isalpha ((int)(*SrcChar)))
    {
        char *start = destpos,
             *lim = destpos + (SYMLEN - 1);
#ifdef COCO
        destpos = lblcopy (destpos, lim);   /* below (step 27) */
#else
        register char *src = SrcChar;

        while ((destpos < lim) && (_ischar (*src)))
        {
            *(destpos++) = *(src++);
        }

        SrcChar = src;
#endif

        /* What "while ((count-- > 0) && ...)" from SYMLEN - 1 left (D64) */

        count = (destpos < lim) ? (lim - destpos) - 1 : -1;

        /* lwasm: "sizeof{name}" is one name, a struct's size (lwstruct.c) */

        if (LwasmMode && (*SrcChar == '{') && (destpos - start == 6) &&
            (strncmp (start, "sizeof", 6) == 0))
        {
            while ((count-- > 0) && *SrcChar && (*SrcChar != '}') &&
                   notblank (*SrcChar))
            {
                *(destpos++) = *(SrcChar++);
            }

            if ((count > 0) && (*SrcChar == '}'))
            {
                *(destpos++) = *(SrcChar++);
            }
        }

        *destpos = '\0';

        while (_ischar (*SrcChar))
        {
            ++SrcChar;
        }

        return 1;
    }

    return 0;
}

char
SkipSpac (ZILCH)
{
    register char *pt = SrcChar;

    while (isblank(*pt))
    {
        pt++;
    }

    return *(SrcChar = pt);   /* Reset SrcChar to this char */
}

#ifdef COCO
/* lblcopy (dest, lim): MovLbl's copy on the 6809 (step 27: DCC's loop
 * took about 22 instructions a character, this 9). Copies characters
 * from SrcChar to dest while dest < lim and _ischar (the table byte
 * & 15; ldb + lda b,u index it by the signed char, as DCC's code does),
 * advances SrcChar past them and returns dest. Y is the destination
 * (SrcChar is in the direct page): no operand here may end
 * in ",y", which dco68 -2 would make an extended address */
#asm
lblcopy: pshs y,u
 ldx <SrcChar
 ldu #_chcodes
 ldy 6,s dest
lbc1 cmpy 8,s lim
 bhs lbc2
 ldb ,x
 lda b,u
 bita #15 _DIGIT|_LOWER|_UPPER|_CONTROL
 beq lbc2
 stb ,y+
 leax 1,x
 bra lbc1
lbc2 tfr y,d
 puls y
 stx <SrcChar
 puls u,pc
#endasm
#endif

#ifdef COCO
/* fcscan (src, strt, stop): the rest of FindCmd on the 6809 (step 27:
 * in C, 15% of las's time on te). Copies src to CmdMnemonic in lower
 * case as the C does (a character that isn't lower case gets 0x20 or'd
 * in: _tolower), then scans the entries strt..stop of one letter for the
 * first that is a prefix of it: entry and mnemonic's first two
 * characters compared as one 16-bit number, the rest one by one. Sets
 * CmdNamEnd on a match. Entries are CODFORMs: the name pointer, then 2
 * bytes. As elsewhere, no operand may end in ",y" (dco68 -2) */
#asm
fcscan: pshs y,u
 ldx 6,s src
 ldy #CmdMnemonic
 ldu #_chcodes
fcs1 ldb ,x+
 beq fcs2
 lda b,u
 bita #4 _LOWER
 bne fcs1a
 orb #$20
fcs1a stb ,y+
 bra fcs1
fcs2 stb ,y+
 ldx #CmdMnemonic
 ldd ,x
 pshs d the first two characters, w
 ldu 10,s strt
fcs3 cmpu 12,s stop
 bhs fcs9
 ldx ,u
 ldd ,x
 cmpd ,s
 beq fcs5
 cmpa ,s
 bne fcs9 past the entries for the letter
 tstb
 bne fcs4
 ldd #CmdMnemonic+1 one character, the same: a prefix
 bra fcs8
fcs4 leau 4,u
 bra fcs3
fcs5 pshs u
 leax 1,x
 ldu #CmdMnemonic+1
fcs6 ldb ,x+
 beq fcs7 the entry ended: a prefix
 cmpb ,u+
 beq fcs6
 puls u
 bra fcs4
fcs7 tfr u,d q: U is at the character of the mnemonic after the prefix
 puls u
fcs8 std <CmdNamEnd
 tfr u,d
 leas 2,s
 puls y,u,pc
fcs9 clra
 clrb
 leas 2,s
 puls y,u,pc
#endasm
#endif
