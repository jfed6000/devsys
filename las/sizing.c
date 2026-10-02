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
 *  1. A trial pass 1, in a child process (sz_trial), assembles as  *
 *     RMA always has: every ",pcr" long. It records each ",pcr"    *
 *     site (sz_pcr), each line lwasm might leave unsettled         *
 *     (sz_other) and each code label's section (sz_label).         *
 *  2. At its end, sz_finish() replays lwasm's rule on that record  *
 *     and sends one byte per ",pcr" site (1 = short) to the parent.*
 *  3. The parent, which did nothing meanwhile, assembles for real  *
 *     with those sizes (sz_pcr returns them), so every label and   *
 *     equate has its final value in pass 1, and pass 2's phase     *
 *     check still guards the result.                               *
 *                                                                  *
 * Only in lwasm mode, and only where fork() exists. Elsewhere the  *
 * sizes stay as RMA chose them. -DNOSIZING builds a host las       *
 * without it (build/nosz), the reference for the 6809 build.       *
 *                                                                  *
 * Addresses are las's: if a file interleaves lwasm sections, las   *
 * lays them out in source order and lwasm doesn't.                 *
 * **************************************************************** */

#include "rma.h"

#if (defined(__unix) || defined(__unix__) || defined(__linux__)) \
    && !defined(NOSIZING)
#  define SZ_FORK
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/wait.h>
#  include <stdlib.h>
#endif

static int PcrSites;            /* ",pcr" sites seen in this pass */
static char *SzDec;             /* one byte per site, 1 = short */
static int SzCount;

#ifdef SZ_FORK

#define K_PCR   1               /* a ",pcr" site */
#define K_OTHER 2               /* a line lwasm might leave unsettled */

struct szitem
{
    int start;                  /* address of the line (trial layout) */
    int len8;                   /* ",pcr": length of the 8-bit form */
    int tconst;                 /* ",pcr": constant added to the target */
    int tabs;                   /* ",pcr": absolute target (module) */
    SYMH tsym;      /* ",pcr": target label, or 0 */
    SYMH osym[4];   /* K_OTHER: the operand's symbols */
    int sec;                    /* lwasm section of the line */
    char nosym;                 /* K_OTHER: how many in osym */
    char kind;
    char nomax;                 /* a lea: lwasm has no longest length */
    char done;                  /* settled */
    char shrt;                  /* settled at the 8-bit form */
};

struct szlabel
{
    SYMH sym;
    int sec;
};

static struct szitem *Items;
static int NItems, MaxItems;
static struct szlabel *Labels;
static int NLabels, MaxLabels;
static int SzFd = -1;           /* trial: where the sizes go */

static struct szitem *
new_item (ZILCH)
{
    if (NItems == MaxItems)
    {
        MaxItems = MaxItems ? MaxItems * 2 : 256;
        Items = (struct szitem *) realloc (Items,
                                           MaxItems * sizeof (struct szitem));

        if ( ! Items)
        {
            _exit (2);
        }
    }

    memset (&Items[NItems], 0, sizeof (struct szitem));
    return &Items[NItems++];
}

/* Is the line being assembled a leax/leay/leas/leau? */

static int
is_lea (ZILCH)
{
    return ! MLCod.prebyte && ((MLCod.opcode & 0xfc) == 0x30);
}

#endif /* SZ_FORK */

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
 *              lwasm section (do_label)                                *
 * ******************************************************************** */

void
#ifdef __STDC__
sz_label (SYMH sym)
#else
sz_label (sym)
     SYMH sym;
#endif
{
#ifdef SZ_FORK
    if ( ! SzTrial)
    {
        return;
    }

    if (NLabels == MaxLabels)
    {
        MaxLabels = MaxLabels ? MaxLabels * 2 : 256;
        Labels = (struct szlabel *) realloc (Labels,
                                       MaxLabels * sizeof (struct szlabel));

        if ( ! Labels)
        {
            _exit (2);
        }
    }

    Labels[NLabels].sym = sym;
    Labels[NLabels++].sec = CurSecId;
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

#ifdef SZ_FORK
    if (SzTrial)
    {
        register struct szitem *it = new_item ();
        int nrefs = (int) (OptBPtr - d0791);

        it->kind = K_PCR;
        it->start = PC_BASE;
        it->len8 = len8;
        it->sec = CurSecId;
        it->nomax = is_lea ();

        if ((nrefs == 1) && ! (d0791->ETyp & NEGMASK) && d0791->RAddr)
        {
            it->tsym = d0791->RAddr;
            it->tconst = value - (int) sym_val (d0791->RAddr);
        }
        else if (nrefs == 0)
        {
            it->tabs = value;
        }
        else
        {
            it->kind = K_OTHER;     /* lwasm can't know it either */
        }

        return 0;
    }
#endif

    return SzDec && (site < SzCount) && SzDec[site];
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
#ifdef SZ_FORK
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

#ifdef SZ_FORK

/* The lwasm section a code label was defined in, or -1 */

static int
#ifdef __STDC__
label_sec (SYMH sym)
#else
label_sec (sym)
     SYMH sym;
#endif
{
    register int i;

    for (i = NLabels - 1; i >= 0; --i)
    {
        if (Labels[i].sym == sym)
        {
            return Labels[i].sec;
        }
    }

    return -1;
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
    register struct szitem *it = &Items[i];
    int lbl = it->tsym ? (int) sym_val (it->tsym) : it->tabs;
    int s = it->start;
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
        register struct szitem *b = &Items[k];

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
        off = (lbl - shrunk - 1) - (s + it->len8);
    }
    else
    {
        off = lbl - (s - shrunk + it->len8);
    }

    off += it->tconst;

    if (known)
    {
        it->done = 1;
        it->shrt = (off >= -128) && (off <= 127);
        return 1;
    }

    if (guess && (off >= -100) && (off <= 100))
    {
        it->done = 1;
        it->shrt = 1;
        return 1;
    }

    return 0;
}

/* Settle whatever can be settled; lwasm's resolve1 */

static void
closure (ZILCH)
{
    int i, changed;

    do
    {
        changed = 0;

        for (i = 0; i < NItems; ++i)
        {
            if (Items[i].kind == K_PCR && ! Items[i].done && try_pcr (i))
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
    int i, j;

    for (i = 0; i < NItems; ++i)
    {
        register struct szitem *it = &Items[i];

        if (it->kind == K_PCR)
        {
            /* A target lwasm can never reach: an external, a symbol
             * that isn't a code label, or one in another section */

            if (it->tsym ? ((label_sec (it->tsym) != it->sec) ||
                            ! (sym_msk (it->tsym) & RELATIVE))
                         : ! UsedModule)
            {
                it->kind = K_OTHER;
                it->nosym = 0;
            }
        }

        if (it->kind == K_OTHER)
        {
            /* settled from the start if every symbol is a number */

            it->done = (it->nosym > 0);

            for (j = 0; j < it->nosym; ++j)
            {
                if ( ! lw_known (it->osym[j]))
                {
                    it->done = 0;
                }
            }
        }
    }

    for (;;)
    {
        closure ();

        for (i = 0; (i < NItems) && Items[i].done; ++i)
        {
        }

        if (i == NItems)
        {
            break;
        }

        Items[i].done = 1;      /* forced: the long form */
        Items[i].shrt = 0;
    }
}

/* ******************************************************************** *
 * sz_trial() - Before pass 1 of a file in lwasm mode: fork. The child  *
 *          returns, runs the trial pass 1 with its output thrown away, *
 *          and ends in sz_finish(). The parent waits for the sizes and *
 *          then assembles the file for real.                           *
 * ******************************************************************** */

void
sz_trial (ZILCH)
{
    int fds[2], status, n, got;
    pid_t pid;

    SzCount = 0;

    if (pipe (fds) < 0)
    {
        return;
    }

    fflush (NULL);

    if ((pid = fork ()) < 0)
    {
        close (fds[0]);
        close (fds[1]);
        return;
    }

    if (pid == 0)               /* the trial */
    {
        int nul = open ("/dev/null", O_WRONLY);

        close (fds[0]);
        SzFd = fds[1];
        SzTrial = 1;
        NItems = NLabels = 0;

        if (nul >= 0)
        {
            dup2 (nul, 1);
            dup2 (nul, 2);
        }

        if (OutFile)
        {
            OutFile = fopen ("/dev/null", "w");
        }

        return;
    }

    close (fds[1]);

    /* The sizes: a count, then one byte per site */

    if (read (fds[0], &n, sizeof (n)) == sizeof (n) && (n > 0))
    {
        SzDec = (char *) realloc (SzDec, n);

        for (got = 0; SzDec && (got < n); )
        {
            int r = read (fds[0], SzDec + got, n - got);

            if (r <= 0)
            {
                break;
            }

            got += r;
        }

        if (SzDec && (got == n))
        {
            SzCount = n;
        }
    }

    close (fds[0]);

    if ((waitpid (pid, &status, 0) != pid) || ! WIFEXITED (status) ||
            (WEXITSTATUS (status) != 0))
    {
        /* The trial failed: RMA's sizes (every ",pcr" long). The real
         * pass 1 reports the same errors if the source is at fault. */

        fprintf (stderr, "las: sizing trial failed; using long offsets\n");
        SzCount = 0;
    }
}

/* ******************************************************************** *
 * sz_finish() - The trial's end: decide, send the sizes, and exit.     *
 * ******************************************************************** */

void
sz_finish (ZILCH)
{
    int i, n, k = 0;
    char *dec;

    sz_decide ();

    /* every ",pcr" site in order, including those lwasm can't reach
     * (all recorded with len8 set) */

    n = PcrSites;
    dec = (char *) malloc (n ? n : 1);

    for (i = 0; i < NItems && dec; ++i)
    {
        if (Items[i].len8)
        {
            dec[k++] = (Items[i].kind == K_PCR) && Items[i].shrt;
        }
    }

    if ( ! dec || (k != n) ||
            (write (SzFd, &n, sizeof (n)) != sizeof (n)) ||
            (write (SzFd, dec, n) != n))
    {
        _exit (1);
    }

    _exit (0);
}

#else /* ! SZ_FORK */

void
sz_trial (ZILCH)
{
}

void
sz_finish (ZILCH)
{
}

#endif /* SZ_FORK */
