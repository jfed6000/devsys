/* **************************************************************** *
 * sizing.c - choose ",pcr" offset sizes the way lwasm does         *
 *                                                                  *
 * RMA gives an operand the long form whenever its value isn't      *
 * known yet, so both passes agree on every instruction's length.   *
 * lwasm instead leaves lines whose size depends on a value it      *
 * doesn't know yet "unsettled", and settles them later:            *
 *                                                                  *
 *  - a ",pcr" offset becomes 8 bits when every line between the    *
 *    instruction and its target is settled and the offset fits in  *
 *    -128..127, or when counting every unsettled line at its       *
 *    longest still gives an offset within -100..100 (a margin);    *
 *  - a line whose operand can't be known while assembling (an      *
 *    external, a label in a relocatable section) stays unsettled;  *
 *  - lwasm records a longest length only for ordinary instructions *
 *    (insn_gen.c), not for leax/leay/leas/leau, so an unsettled    *
 *    lea line between rules out the worst-case estimate;           *
 *  - when nothing more settles, the first unsettled line, in       *
 *    source order, is forced to its long form, and lwasm tries     *
 *    again. (lwasm pass3.c, pass4.c and insn_indexed.c)            *
 *                                                                  *
 * Sizes only ever shrink, so apart from that forced step the       *
 * result doesn't depend on the order lines are looked at.          *
 *                                                                  *
 * las does the same in three steps:                                *
 *                                                                  *
 *  1. Pass 1 is the trial (sz_trial): it assembles as RMA always   *
 *     has, every ",pcr" long, and records each ",pcr" site         *
 *     (sz_pcr), each line lwasm might leave unsettled (sz_other)   *
 *     and each code label's section (sz_label).                    *
 *  2. At its end, sz_finish() replays lwasm's rule on that record  *
 *     and keeps one bit per ",pcr" site (1 = short).               *
 *  3. If any site is short, every label after it is too far on:    *
 *     las runs settling passes (rma_main.c, D46) with those sizes  *
 *     until nothing moves, then pass 2, whose phase check still    *
 *     guards the result.                                           *
 *                                                                  *
 * Until D55 the trial was a child process (Unix fork(), OS-9       *
 * F$Fork of las itself with a shared block for the answers) and    *
 * the parent ran its own pass 1 with the sizes: three passes, and  *
 * on OS-9 a second start-up. Settling corrects the values instead. *
 *                                                                  *
 * The trial's records (Items, Labels) live in banked memory        *
 * (bank.c), in chunks: on the 6809 las's own data is one 8K block  *
 * (D37), too small for 2,000 labels.                               *
 *                                                                  *
 * -DNOSIZING builds a host las without sizing (build/nosz).        *
 *                                                                  *
 * Addresses are las's: if a file interleaves lwasm sections, las   *
 * lays them out in source order and lwasm doesn't.                 *
 * **************************************************************** */

#include "rma.h"

#ifndef NOSIZING
#  define SZ_TRIAL
#endif

static int PcrSites;            /* ",pcr" sites seen in this pass */
static char *SzDec;             /* the sizes: a bit per site, */
static int SzCount, SzCap;      /* 1 = short; SzCap bytes */

#define SZ_SHORT(n) (SzDec[(n) >> 3] & (1 << ((n) & 7)))

#ifdef SZ_TRIAL

#define K_PCR   1               /* a ",pcr" site */
#define K_OTHER 2               /* a line lwasm might leave unsettled */

struct szitem
{
    int start;                  /* address of the line (trial layout) */
    int tconst;                 /* ",pcr": constant added to the target */
    int tabs;                   /* ",pcr": absolute target (module);
                                 * sz_decide puts tsym's value here */
    BANKH tsym;                 /* ",pcr": target label, or 0 */
    BANKH osym[4];              /* K_OTHER: the operand's symbols */
    char len8;                  /* ",pcr": length of the 8-bit form */
    char sec;                   /* lwasm section of the line */
    char nosym;                 /* K_OTHER: how many in osym */
    char kind;
    char nomax;                 /* a lea: lwasm has no longest length */
    char done;                  /* settled */
    char shrt;                  /* settled at the 8-bit form */
};

struct szlabel                  /* K&R: member names must be unique */
{
    BANKH lsym;
    char lsec;
};

/* Items and labels are stored in chunks from bank_alloc. A pointer
 * from item() or bank_ptr() is good only until another block is
 * mapped (a symbol accessor, another chunk): as in symtab.c, copy
 * fields to locals around any such call. */

#define ISHIFT  7               /* 128 items a chunk (2,944 bytes) */
#define LSHIFT  9               /* 512 labels a chunk (1,536 bytes) */
#define ICHUNK  (1 << ISHIFT)
#define LCHUNK  (1 << LSHIFT)
#define MAXCHUNK 16             /* 2,048 items, 8,192 labels */

static unsigned IChunk[MAXCHUNK], LChunk[MAXCHUNK];
static int NItems, NLabels;
static char SzFull;             /* ran out of chunks: the trial fails */
static struct szitem Spare;     /* where records go once SzFull */

static struct szitem *
#ifdef __STDC__
item (int i)
#else
item (i)
     int i;
#endif
{
    return (struct szitem *) bank_ptr (IChunk[i >> ISHIFT]) +
           (i & (ICHUNK - 1));
}

static struct szitem *
new_item (ZILCH)
{
    if ((NItems & (ICHUNK - 1)) == 0)
    {
        if ((NItems >> ISHIFT) == MAXCHUNK)
        {
            SzFull = 1;
            memset (&Spare, 0, sizeof (Spare));
            return &Spare;
        }

        if ( ! IChunk[NItems >> ISHIFT])        /* kept for the next file */
        {
            IChunk[NItems >> ISHIFT] =
                bank_alloc (ICHUNK * sizeof (struct szitem));
        }
    }

    return (struct szitem *) memset ((char *) item (NItems++), 0,
                                     sizeof (struct szitem));
}

/* Is the line being assembled a leax/leay/leas/leau? */

static int
is_lea (ZILCH)
{
    return ! MLCod.prebyte && ((MLCod.opcode & 0xfc) == 0x30);
}

#endif /* SZ_TRIAL */

/* ******************************************************************** *
 * sz_pass() - start of a pass: count ",pcr" sites from the beginning   *
 * ******************************************************************** */

void
sz_pass (ZILCH)
{
    PcrSites = 0;
}

/* ******************************************************************** *
 * sz_label() - trial: a label defined in the code counter, with its    *
 *              lwasm section (do_label). The symbol's type gets        *
 *              SZ_CODE, which nothing else uses; only labels inside a  *
 *              section are listed, with the section. The defs' struct  *
 *              offsets alone are 2,500 labels in NitrOS9 main's krnp2, *
 *              and a list of them all overflowed the trial's blocks.   *
 * ******************************************************************** */

void
#ifdef __STDC__
sz_label (SYMH sym)
#else
sz_label (sym)
     SYMH sym;
#endif
{
#ifdef SZ_TRIAL
    register struct szlabel *lb;

    if ( ! SzTrial)
    {
        return;
    }

    sym_settyp (sym, sym_typ (sym) | SZ_CODE);

    if ( ! CurSecId)
    {
        return;                 /* outside any section: section 0 */
    }

    if ((NLabels & (LCHUNK - 1)) == 0)
    {
        if ((NLabels >> LSHIFT) == MAXCHUNK)
        {
            SzFull = 1;
            return;
        }

        if ( ! LChunk[NLabels >> LSHIFT])
        {
            LChunk[NLabels >> LSHIFT] =
                bank_alloc (LCHUNK * sizeof (struct szlabel));
        }
    }

    lb = (struct szlabel *) bank_ptr (LChunk[NLabels >> LSHIFT]) +
         (NLabels & (LCHUNK - 1));
    lb->lsym = sym;
    lb->lsec = CurSecId;
    ++NLabels;
#endif
}

/* ******************************************************************** *
 * sz_pcr() - A ",pcr" operand without "<" or ">" (pc_rel). value is    *
 *            the operand as evaluated (target plus constant; a forward *
 *            label counts 0), len8 the instruction's 8-bit length.     *
 *            Returns 1 if it takes the 8-bit form.                     *
 * ******************************************************************** */

int
#ifdef __STDC__
sz_pcr (int value, int len8)
#else
sz_pcr (value, len8)
     int value;
     int len8;
#endif
{
    int site = PcrSites++;

#ifdef SZ_TRIAL
    if (SzTrial)
    {
        register struct szitem *it;
        int nrefs = (int) (OptBPtr - d0791);
        int kind = K_PCR, tconst = 0, tabs = 0;
        SYMH tsym = 0;

        /* everything that maps a symbol first: then the item */

        if ((nrefs == 1) && ! (d0791->ETyp & NEGMASK) && d0791->RAddr)
        {
            tsym = d0791->RAddr;
            tconst = value - (int) sym_val (tsym);
        }
        else if (nrefs == 0)
        {
            tabs = value;
        }
        else
        {
            kind = K_OTHER;     /* lwasm can't know it either */
        }

        it = new_item ();
        it->kind = kind;
        it->start = PC_BASE;
        it->len8 = len8;
        it->sec = CurSecId;
        it->nomax = is_lea ();
        it->tsym = tsym;
        it->tconst = tconst;
        it->tabs = tabs;
        return 0;
    }
#endif

    return SzDec && (site < SzCount) && SZ_SHORT (site);
}

/* ******************************************************************** *
 * sz_other() - trial: a direct/extended or indexed operand without     *
 *              "<" or ">" naming symbols not known yet. If they never  *
 *              become plain numbers, lwasm leaves the line unsettled   *
 *              until it is forced.                                     *
 * ******************************************************************** */

void
sz_other (ZILCH)
{
#ifdef SZ_TRIAL
    register struct szitem *it;
    register struct ref_ent *r;

    if ( ! SzTrial || (d0791 == OptBPtr))
    {
        return;
    }

    it = new_item ();
    it->kind = K_OTHER;
    it->start = PC_BASE;
    it->sec = CurSecId;
    it->nomax = is_lea ();

    for (r = d0791; r < OptBPtr; ++r)
    {
        if (it->nosym < 4)
        {
            it->osym[(int) it->nosym++] = r->RAddr;
        }
        else
        {
            it->osym[3] = 0;        /* too many to check: unsettled */
        }
    }
#endif
}

#ifdef SZ_TRIAL

/* The lwasm section a code label was defined in, or -1 */

static int
#ifdef __STDC__
label_sec (SYMH sym)
#else
label_sec (sym)
     SYMH sym;
#endif
{
    register struct szlabel *lb;
    register int i;
    int c, n;

    /* (a defs image recorded in a trial keeps SZ_CODE in the types) */

    if ( ! (sym_typ (sym) & SZ_CODE))
    {
        return -1;              /* not a code label */
    }

    for (c = (NLabels - 1) >> LSHIFT; c >= 0; --c)
    {
        n = NLabels - (c << LSHIFT);

        if (n > LCHUNK)
        {
            n = LCHUNK;
        }

        lb = (struct szlabel *) bank_ptr (LChunk[c]);

        for (i = n - 1; i >= 0; --i)
        {
            if (lb[i].lsym == sym)
            {
                return lb[i].lsec;
            }
        }
    }

    return 0;
}

/* Would lwasm know this symbol as a number while assembling? A code
 * label in a module is an absolute address; in a section it isn't. */

static int
#ifdef __STDC__
lw_known (SYMH sym)
#else
lw_known (sym)
     SYMH sym;
#endif
{
    if ( ! sym || ! (sym_msk (sym) & RELATIVE))
    {
        return 0;           /* never defined: external */
    }

    if (UsedModule)
    {
        return 1;
    }

    return ! (sym_msk (sym) & CODLOC) && (label_sec (sym) < 0);
}

/* ******************************************************************** *
 * try_pcr() - lwasm's resolve for one unsettled ",pcr" line: settle it *
 *             if the offset is known, or at 8 bits if even the worst   *
 *             case is within the margin. Returns 1 if it settled.      *
 * ******************************************************************** */

static int
#ifdef __STDC__
try_pcr (int i)
#else
try_pcr (i)
     int i;
#endif
{
    register struct szitem *b = item (i);
    int lbl = b->tabs;          /* the target (sz_decide) */
    int s = b->start;
    int len8 = b->len8;
    int tconst = b->tconst;
    int lo, hi, k, off, shrunk = 0, known = 1, guess = 1;

    /* The lines between: after this one and before the target's, or
     * from the target's up to this one */

    if (lbl > s)
    {
        lo = s + 1;
        hi = lbl;
    }
    else
    {
        lo = lbl;
        hi = s;
    }

    for (k = 0; k < NItems; ++k)
    {
        b = item (k);

        if ((k == i) || (b->start < lo) || (b->start >= hi))
        {
            continue;
        }

        if ( ! b->done)
        {
            known = 0;          /* counted at its longest, as in the trial */

            if (b->nomax)
            {
                guess = 0;      /* ... unless lwasm has no longest length */
            }
        }
        else if (b->shrt)
        {
            ++shrunk;           /* each 8-bit ",pcr" saves one byte */
        }
    }

    /* target - (start + 8-bit length), with this line at 8 bits: going
     * forward the target moves back by the bytes saved between and by
     * this line's own byte; going back, this line moves back. */

    if (lbl > s)
    {
        off = (lbl - shrunk - 1) - (s + len8);
    }
    else
    {
        off = lbl - (s - shrunk + len8);
    }

    off += tconst;
    b = item (i);

    if (known)
    {
        b->done = 1;
        b->shrt = (off >= -128) && (off <= 127);
        return 1;
    }

    if (guess && (off >= -100) && (off <= 100))
    {
        b->done = 1;
        b->shrt = 1;
        return 1;
    }

    return 0;
}

/* Settle whatever can be settled; lwasm's resolve1 */

static void
closure (ZILCH)
{
    register struct szitem *it;
    int i, changed;

    do
    {
        changed = 0;

        for (i = 0; i < NItems; ++i)
        {
            it = item (i);

            if ((it->kind == K_PCR) && ! it->done && try_pcr (i))
            {
                changed = 1;
            }
        }
    } while (changed);
}

/* ******************************************************************** *
 * sz_decide() - lwasm's resolve1 and resolve2 on the recorded lines    *
 * ******************************************************************** */

static void
sz_decide (ZILCH)
{
    register struct szitem *it;
    SYMH tsym, osym[4];
    int i, j, kind, nosym, sec, tabs, done;

    for (i = 0; i < NItems; ++i)
    {
        /* copy out: label_sec, sym_msk and sym_val map other blocks */

        it = item (i);
        kind = it->kind;
        tsym = it->tsym;
        tabs = it->tabs;
        sec = it->sec;
        nosym = it->nosym;

        for (j = 0; j < 4; ++j)
        {
            osym[j] = it->osym[j];
        }

        if (kind == K_PCR)
        {
            /* A target lwasm can never reach: an external, a symbol
             * that isn't a code label, or one in another section */

            if (tsym ? ((label_sec (tsym) != sec) ||
                        ! (sym_msk (tsym) & RELATIVE))
                     : ! UsedModule)
            {
                kind = K_OTHER;
                nosym = 0;
            }
            else if (tsym)
            {
                tabs = (int) sym_val (tsym);
            }
        }

        done = 0;

        if (kind == K_OTHER)
        {
            /* settled from the start if every symbol is a number */

            done = (nosym > 0);

            for (j = 0; j < nosym; ++j)
            {
                if ( ! lw_known (osym[j]))
                {
                    done = 0;
                }
            }
        }

        it = item (i);
        it->kind = kind;
        it->nosym = nosym;
        it->tabs = tabs;
        it->done = done;
    }

    for (;;)
    {
        closure ();

        for (i = 0; (i < NItems) && item (i)->done; ++i)
        {
        }

        if (i == NItems)
        {
            break;
        }

        it = item (i);
        it->done = 1;           /* forced: the long form */
        it->shrt = 0;
    }
}

/* The decision for item i: 1 = short */

static int
#ifdef __STDC__
item_short (int i)
#else
item_short (i)
     int i;
#endif
{
    register struct szitem *it = item (i);

    return (it->kind == K_PCR) && it->shrt;
}

#endif /* SZ_TRIAL */

#ifdef SZ_TRIAL

/* ******************************************************************** *
 * sz_trial() - Before pass 1 of a file in lwasm mode: pass 1 is the    *
 *              trial                                                   *
 * ******************************************************************** */

void
sz_trial (ZILCH)
{
    SzTrial = 1;
    SzCount = NItems = NLabels = SzFull = 0;
}

/* Clear sizing's mark in a symbol's type: it would reach the ROF's
 * reference types (registers.c) and the symbol table listing */

static void
#ifdef __STDC__
unmark (SYMH sym)
#else
unmark (sym)
     SYMH sym;
#endif
{
    if (sym_typ (sym) & SZ_CODE)
    {
        sym_settyp (sym, sym_typ (sym) & ~SZ_CODE);
    }
}

/* ******************************************************************** *
 * sz_finish() - The end of the trial pass 1: decide. Returns 1 if a    *
 *               site is short, so the values must settle.              *
 * ******************************************************************** */

int
sz_finish (ZILCH)
{
    int i, k = 0, any = 0, need = (PcrSites >> 3) + 1;
    char img = ImgOn;

    sz_decide ();

    if (need > SzCap)
    {
        SzDec = (char *) getmem (need);     /* getmem maps no blocks */
        SzCap = need;
    }

    memset (SzDec, 0, need);

    /* every ",pcr" site in order, including those lwasm can't reach
     * (all recorded with len8 set) */

    for (i = 0; i < NItems; ++i)
    {
        if (item (i)->len8)
        {
            if (item_short (i))
            {
                SzDec[k >> 3] |= 1 << (k & 7);
                any = 1;
            }

            ++k;
        }
    }

    if (SzFull || (k != PcrSites))
    {
        /* RMA's sizes, every ",pcr" long: pass 1's layout */

        fprintf (stderr, "las: sizing trial failed; using long offsets\n");
        any = 0;
    }

    SzCount = any ? k : 0;
    SzTrial = ImgOn = 0;        /* not a change the defs record sees */
    sym_walk (unmark);
    ImgOn = img;
    return any;
}

#else /* ! SZ_TRIAL */

void
sz_trial (ZILCH)
{
}

int
sz_finish (ZILCH)
{
    return 0;
}

#endif /* SZ_TRIAL */
