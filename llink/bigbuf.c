/* ******************************************************************** *
 * bigbuf.c - the psect being linked, in 8K blocks (devsys, step 21)    *
 *                                                                      *
 * L14d3() (rl_03.c) reads a psect's code and initialised data whole,   *
 * patches its references, then writes it. In one malloc'ed buffer that *
 * was the size of the biggest psect: a 5,257-line C file is one 31K    *
 * psect, more than llink's data has room for. So the buffer is up to 8 *
 * blocks of 8K (a psect is under 64K): on OS-9 from F$AllRAM, reached  *
 * one at a time through a window (F$MapBlk / F$ClrBlk), as las keeps   *
 * its symbols (las/bank.c); on Linux 8K pieces from malloc, so the     *
 * block boundaries are tested there too.                               *
 *                                                                      *
 *      bb_alloc (n)            room for n bytes                        *
 *      bb_ptr (o)              byte o: good until the next bb_ call    *
 *      bb_read (fp, n)         n bytes from fp, from byte 0            *
 *      bb_write (o, n, fp, c)  n bytes from byte o to fp; c: and CRC   *
 *      bb_free ()              give the blocks back (lw_cleanup)       *
 * ******************************************************************** */

#include "rlink.h"

extern char _Crc[];             /* rl_03.c: the module CRC so far */

#define BLKSIZE 8192
#define MAXBB   8

static int NBb;                 /* blocks allocated */

#ifdef COCO
static unsigned BbNum[MAXBB];   /* their physical numbers */
static int BbCur = -1;          /* the block in the window */
static char *BbWin;             /* where the window is, once mapped */

static int allram ();
static char *mapblk ();
static int clrblk ();
static int delram ();
#else
static char *Bb[MAXBB];
#endif

void
bb_alloc (n)
unsigned n;
{
    bb_free ();

    while (n > 0)
    {
#ifdef COCO
        if (NBb == MAXBB || (BbNum[NBb] = allram ()) == -1)
#else
        if (NBb == MAXBB || ! (Bb[NBb] = (char *) malloc (BLKSIZE)))
#endif
        {
            fatal ("out of memory (a psect of %u bytes)", n + NBb * BLKSIZE);
        }

        ++NBb;
        n = n > BLKSIZE ? n - BLKSIZE : 0;
    }
}

char *
bb_ptr (o)
unsigned o;
{
    register int b = o / BLKSIZE;

#ifdef COCO
    if (b != BbCur)
    {
        if (BbWin)
        {
            clrblk (BbWin);
        }

        if ( ! (BbWin = mapblk (BbNum[b])))
        {
            fatal ("can't map a block");
        }

        BbCur = b;
    }

    return BbWin + (o & (BLKSIZE - 1));
#else
    return Bb[b] + (o & (BLKSIZE - 1));
#endif
}

/* the length of the piece from byte o to the end of its block, or n */
static unsigned
piece (o, n)
unsigned o, n;
{
    unsigned left = BLKSIZE - (o & (BLKSIZE - 1));

    return n < left ? n : left;
}

int
bb_read (fp, n)
FILE *fp;
unsigned n;
{
    unsigned o, len;

    for (o = 0; n > 0; o += len, n -= len)
    {
        len = piece (o, n);

        if (fread (bb_ptr (o), len, 1, fp) != 1)
        {
            return 0;
        }
    }

    return 1;
}

void
bb_write (o, n, fp, docrc)
unsigned o, n;
FILE *fp;
int docrc;
{
    unsigned len;
    char *p;

    for ( ; n > 0; o += len, n -= len)
    {
        len = piece (o, n);
        p = bb_ptr (o);

        if (docrc)
        {
            crc (p, len, _Crc);
        }

        fwrite (p, len, 1, fp);
    }
}

void
bb_free ()
{
#ifdef COCO
    if (BbWin)
    {
        clrblk (BbWin);
        BbWin = 0;
    }

    BbCur = -1;

    while (NBb > 0)
    {
        delram (BbNum[--NBb]);
    }
#else
    while (NBb > 0)
    {
        free (Bb[--NBb]);
    }
#endif
}

#ifdef COCO
/* ******************************************************************** *
 * The system calls (as las/bank.c). DCC passes arguments on the stack  *
 * (the first at 2,s on entry) and returns in D; U and Y must be kept.  *
 *   allram ()         F$AllRAM, 1 block: its number, or -1             *
 *   mapblk (n)        F$MapBlk, block n: its address, or 0             *
 *   clrblk (addr)     F$ClrBlk, 1 block at addr                        *
 *   delram (n)        F$DelRAM, block n                                *
 * ******************************************************************** */

#asm
allram pshs y,u
 ldb #1
 os9 $39 F$AllRAM
 bcc allr1
 ldd #-1
allr1 puls y,u,pc

mapblk pshs y,u
 ldx 6,s
 ldb #1
 os9 $4F F$MapBlk
 bcc mapb1
 ldu #0
mapb1 tfr u,d
 puls y,u,pc

clrblk pshs y,u
 ldu 6,s
 ldb #1
 os9 $50 F$ClrBlk
 puls y,u,pc

delram pshs y,u
 ldx 6,s
 ldb #1
 os9 $51 F$DelRAM
 puls y,u,pc
#endasm
#endif
