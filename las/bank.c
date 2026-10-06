/* **************************************************************** *
 * bank.c - storage for symbols and references in 8K blocks         *
 *                                                                  *
 * A Level 2 process sees 64K, and las's code takes 5 of its eight  *
 * 8K slots, which leaves too little room for the symbols of a      *
 * NitrOS9 source (DESIGN.md §6). So symbols and reference records  *
 * live in extra RAM blocks (F$AllRAM) that are mapped one at a     *
 * time into a free slot, the window (F$MapBlk / F$ClrBlk).         *
 *                                                                  *
 *      bank_alloc (size)   new zeroed storage: returns its handle  *
 *      bank_ptr (handle)   a pointer to it, good until bank_ptr is *
 *                          called for a handle in another block    *
 *      bank_done ()        give the blocks back (call before exit) *
 *                                                                  *
 * OS-9 only, for the sizing trial (sizing.c), a block shared with  *
 * another process by its number:                                   *
 *      bank_share ()       a new block: its number, or -1          *
 *      bank_mapnum (n)     block n in the window; any process may  *
 *                          map any block (F$MapBlk doesn't check)  *
 *      bank_unshare (n)    give block n back                       *
 *                                                                  *
 * A handle is 16 bits: block number + 1 in the top 4, the offset   *
 * in the block / 2 in the low 12 (storage is 2-byte aligned). So   *
 * there are at most 15 blocks (120K), and a handle is never 0.     *
 *                                                                  *
 * Three builds:                                                    *
 *   COCO     the real thing: one window, mapped by system calls    *
 *   BANKWIN  Linux, test build (build/bankwin): the same single    *
 *            window, emulated by copying a block in and out, so a  *
 *            pointer kept across a switch sees the wrong block's   *
 *            bytes, as it would on the 6809                        *
 *   other    Linux: every block stays where it is                  *
 * **************************************************************** */

#include "rma.h"
#ifndef COCO
#  include <stdlib.h>
#endif

#define BLKSIZE 8192
#define MAXBLK  15

#define HANDLE(b, o) ((((unsigned) (b) + 1) << 12) | ((unsigned) (o) >> 1))
#define HBLK(h) ((int) ((h) >> 12) - 1)
#define HOFF(h) (((h) & 0x0fff) << 1)

static int NBlk;                /* blocks allocated */
static int Used;                /* bytes used in the last one */
static int BlkUsed[MAXBLK];     /* bytes used in each full one */
static int Cur = -1;            /* block in the window */
#if defined(COCO) || defined(BANKWIN)
static long Switches;           /* window changes (bank_stats) */
#endif

#ifdef COCO
static unsigned BlkNum[MAXBLK]; /* physical block numbers */
static char *Window;            /* where the window is, once mapped */
static int Shared = -1;         /* bank_share's block */

static int allram ();
static char *mapblk ();
static int clrblk ();
static int delram ();
#else
static char *Blk[MAXBLK];
#  ifdef BANKWIN
static char Window[BLKSIZE];
#  endif
#endif

/* ******************************************************************** *
 * newblock() - Add a block. Out of handles or out of RAM is the same   *
 *              error RMA always gave for a full table.                 *
 * ******************************************************************** */

static void
newblock (ZILCH)
{
    if (NBlk == MAXBLK)
    {
        errexit ("symbol table overflow");
    }

#ifdef COCO
    {
        int b = allram ();

        if (b == -1)
        {
            errexit ("symbol table overflow");
        }

        BlkNum[NBlk] = b;
    }
#else
    if ( ! (Blk[NBlk] = (char *) calloc (BLKSIZE, 1)))
    {
        errexit ("symbol table overflow");
    }
#endif

    if (NBlk)
    {
        BlkUsed[NBlk - 1] = Used;
    }

    ++NBlk;
    Used = 0;
}

#if defined(COCO) || defined(BANKWIN)

/* ******************************************************************** *
 * map() - Put block b in the window.                                   *
 * ******************************************************************** */

static void
#ifdef __STDC__
map (int b)
#else
map (b)
     int b;
#endif
{
    ++Switches;
#ifdef COCO
    if (Window)
    {
        clrblk (Window);
    }

    if ( ! (Window = mapblk (BlkNum[b])))
    {
        errexit ("can't map symbol block");
    }
#else
    if (Cur >= 0)
    {
        memcpy (Blk[Cur], Window, BLKSIZE);
    }

    memcpy (Window, Blk[b], BLKSIZE);
#endif
    Cur = b;
}

#endif

char *
#ifdef __STDC__
bank_ptr (unsigned h)
#else
bank_ptr (h)
     unsigned h;
#endif
{
    register int b = HBLK (h);

#if defined(COCO) || defined(BANKWIN)
    if (b != Cur)
    {
        map (b);
    }

    return Window + HOFF (h);
#else
    return Blk[b] + HOFF (h);
#endif
}

unsigned
#ifdef __STDC__
bank_alloc (int size)
#else
bank_alloc (size)
     int size;
#endif
{
    unsigned h;

    size = (size + 1) & ~1;

    if ((NBlk == 0) || (Used + size > BLKSIZE))
    {
        newblock ();
    }

    h = HANDLE (NBlk - 1, Used);
    Used += size;
    memset (bank_ptr (h), 0, size);
    return h;
}

/* ******************************************************************** *
 * bank_mark() - A handle below every later allocation's (handles grow  *
 *               with each allocation).                                 *
 * bank_after() - The handle allocated after h, whose size was size.    *
 * (defsimg.c walks the symbols the defs created with them.)            *
 * ******************************************************************** */

unsigned
bank_mark (ZILCH)
{
    return NBlk ? HANDLE (NBlk - 1, Used) : HANDLE (0, 0);
}

unsigned
#ifdef __STDC__
bank_after (unsigned h, int size)
#else
bank_after (h, size)
     unsigned h;
     int size;
#endif
{
    int b = HBLK (h);
    int o = HOFF (h) + ((size + 1) & ~1);

    if ((b < NBlk - 1) && (o >= BlkUsed[b]))
    {
        return HANDLE (b + 1, 0);
    }

    return HANDLE (b, o);
}

/* ******************************************************************** *
 * bank_done() - Unmap the window and free the blocks. On OS-9 a block  *
 *               from F$AllRAM stays allocated after the process ends   *
 *               unless it is given back, so every exit must call this. *
 * ******************************************************************** */

void
bank_done (ZILCH)
{
#ifdef COCO
    int b;

    if (Window)
    {
        clrblk (Window);
        Window = 0;
    }

    for (b = 0; b < NBlk; ++b)
    {
        delram (BlkNum[b]);
    }

    if (Shared != -1)
    {
        delram (Shared);
        Shared = -1;
    }
#endif
    NBlk = 0;
    Cur = -1;
}

#ifdef COCO
int
bank_share (ZILCH)
{
    return Shared = allram ();
}

char *
#ifdef __STDC__
bank_mapnum (int n)
#else
bank_mapnum (n)
     int n;
#endif
{
    ++Switches;

    if (Window)
    {
        clrblk (Window);
    }

    if ( ! (Window = mapblk (n)))
    {
        errexit ("can't map shared block");
    }

    Cur = -1;                   /* the next bank_ptr maps its block */
    return Window;
}

void
#ifdef __STDC__
bank_unshare (int n)
#else
bank_unshare (n)
     int n;
#endif
{
    if (Window)
    {
        clrblk (Window);
        Window = 0;
        Cur = -1;
    }

    delram (n);
    Shared = -1;
}
#endif

#ifdef SYMSTATS
/* Measurement build: blocks used and window changes, to stderr */

void
bank_stats (ZILCH)
{
#if defined(COCO) || defined(BANKWIN)
    fprintf (stderr, "bankstats: blocks %d lastused %d switches %ld\n",
             NBlk, Used, Switches);
#else
    fprintf (stderr, "bankstats: blocks %d lastused %d\n", NBlk, Used);
#endif
}
#endif

#ifdef COCO
/* ******************************************************************** *
 * The system calls. DCC passes arguments on the stack (the first at    *
 * 2,s on entry) and returns in D; U and Y must be kept.                *
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
