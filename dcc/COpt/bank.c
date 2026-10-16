/* bank.c - dco68's records in 8K blocks of extra RAM (devsys)
 *
 * las's storage (las/bank.c, DESIGN.md D27) for dco68: a Level 2 process
 * sees 64K, but a whole file's instructions need 110K for dcc68's optim.c
 * and 360K for te (a 4,212-line editor, step 20), so they live in blocks from F$AllRAM, mapped one at
 * a time into the window (F$MapBlk / F$ClrBlk).
 *
 *      bank_alloc (size)   new zeroed storage: returns its handle
 *      bank_ptr (handle)   a pointer to it, good until bank_ptr is
 *                          called for a handle in another block
 *      bank_done ()        give the blocks back (every exit calls it)
 *
 * A handle is 16 bits: block number + 1 in the top 6, the offset in the
 * block / 8 in the low 10 (storage is 8-byte aligned). So there are at
 * most 62 blocks (496K) besides the static records, and a handle is
 * never 0. (Until step 20, 4-byte units and 31 blocks: te needs 45.)
 *
 * Three builds:
 *   _OS9     the real thing: one window, mapped by system calls
 *   BANKWIN  Linux, test build: the same single window, emulated by
 *            copying a block in and out, so a pointer kept across a
 *            switch sees the wrong block's bytes, as on the 6809
 *   other    Linux: every block stays where it is
 */

#include "op.h"

#define BLKSIZE 8192
#define MAXBLK  62
#define STATBLK 62              /* handles into Static, ordinary memory */
#define STATSIZE 16             /* ilist (step 26: the label buckets moved to labels.c) */
#ifndef NWIN
/* windows: the 8 slots of the 64K hold dco68's module (3, topslot.c),
 * its data (2: the patterns alone take about 3.5K) and 3 windows (D61) */
#  define NWIN  3
#endif

#define HANDLE(b, o) ((((unsigned) (b) + 1) << 10) | ((unsigned) (o) >> 3))
#define HBLK(h) ((int) ((h) >> 10) - 1)
#define HOFF(h) (((h) & 0x03ff) << 3)

static int NBlk;                /* blocks allocated */
static int Used;                /* bytes used in the last one */
static H NextH;                 /* the handle of its first free byte (step
                                 * 26: HANDLE's << 10 is a library loop on
                                 * the 6809, at every allocation) */
int bankcur = -1;               /* the block last used */
char *bankaddr;                 /* its address */
#ifdef _OS9
/* for the 6809's record readers (step 26): the high byte of bankcur's
 * handles without the offset's 2 bits ((bankcur + 1) << 2; 0 matches
 * no handle), and bankaddr less the block's part of handle << 3
 * (((bankcur + 1) & 7) << 13), so handle << 3 + bankbias is the address */
char bankhi;
char *bankbias;
#endif
static char Static[STATSIZE];   /* bank_static's records */
static int StatUsed;
#if defined(_OS9) || defined(BANKWIN)
long Switches;                  /* window changes (-D statistics) */
static int WinBlk[NWIN];        /* block + 1 in each window, 0: none */
static unsigned WinUse[NWIN];   /* when each was last used (LRU) */
static unsigned Clock;
#endif

#ifdef _OS9
static unsigned BlkNum[MAXBLK]; /* physical block numbers */
static char *Window[NWIN];      /* where each window is, once mapped */
static int allram(), clrblk(), delram();
static char *mapblk();
#else
static char *Blk[MAXBLK];
#  ifdef BANKWIN
static char Window[NWIN][BLKSIZE];
#  endif
#endif

static
newblock()
{
#ifdef _OS9
    if (NBlk == 0)
        topslot();      /* F$MapBlk mustn't give the top slot (D51, D61) */
#endif
    if (NBlk == MAXBLK)
        error("out of memory (%d blocks)", MAXBLK);
#ifdef _OS9
    if ((BlkNum[NBlk] = allram()) == -1)
        error("out of memory (no free RAM block)");
#else
    if ((Blk[NBlk] = (char *) calloc(BLKSIZE, 1)) == NULL)
        error("out of memory");
#endif
    ++NBlk;
    Used = 0;
    NextH = HANDLE(NBlk - 1, 0);
}

#if defined(_OS9) || defined(BANKWIN)
/* block b into a window: the one that holds it, or the least recently
 * used one; returns its address */
static char *
map(b)
int b;
{
    register int w;
    int lru;

    for (w = 0; w < NWIN; ++w)
        if (WinBlk[w] == b + 1)
            goto found;
    for (w = lru = 0; w < NWIN; ++w)
        if (WinUse[w] < WinUse[lru])
            lru = w;
    w = lru;
    ++Switches;
#ifdef _OS9
    if (Window[w])
        clrblk(Window[w]);
    if ((Window[w] = mapblk(BlkNum[b])) == NULL)
        error("can't map a block");
#else
    if (WinBlk[w])
        memcpy(Blk[WinBlk[w] - 1], Window[w], BLKSIZE);
    memcpy(Window[w], Blk[b], BLKSIZE);
#endif
    WinBlk[w] = b + 1;
found:
    if (++Clock == 0)           /* wrapped: start the ages again */
        for (lru = 0; lru < NWIN; ++lru)
            WinUse[lru] = 0;
    WinUse[w] = Clock;
    return Window[w];
}
#endif

/* block b as the current one (bankcur, bankaddr): its address. On the
 * 6809 with three windows in assembly below (step 24: in C, finding
 * the window that already holds b took 9% of dco68's time on te) */
#if !defined(_OS9) || NWIN != 3
bank_map(b)
int b;
{
    bankcur = b;
    if (b == STATBLK)
        bankaddr = Static;
    else
#if defined(_OS9) || defined(BANKWIN)
        bankaddr = map(b);
#else
        bankaddr = Blk[b];
#endif
}
#else
/* bankcur = b; then Static, a window that holds b (its age renewed, as
 * map does), or map's choice. When Clock would wrap, map does it all */
#asm
bank_map: ldd 2,s
 std bankcur,y
 cmpd #62 STATBLK
 bne bnm1
 leax Static,y
 tfr x,d
 bra bnmset
bnm1 addd #1
 leax WinBlk,y
 cmpd ,x
 beq bnm2
 cmpd 2,x
 beq bnm3
 cmpd 4,x
 beq bnm4
bnm5 ldd 2,s
 pshs d
 lbsr map
 leas 2,s
 bra bnmset
bnm4 ldb #4
 bra bnm6
bnm3 ldb #2
 bra bnm6
bnm2 clrb
bnm6 pshs u
 clra
 tfr d,u
 ldd Clock,y
 addd #1
 beq bnm7
 std Clock,y
 leax WinUse,y
 tfr u,d
 leax d,x
 ldd Clock,y
 std ,x
 leax Window,y
 tfr u,d
 ldd d,x
 puls u
* D = the block's address: bankaddr, bankhi, bankbias
bnmset std bankaddr,y
 ldb bankcur+1,y
 incb
 lslb
 lslb
 stb bankhi,y
 lslb
 lslb
 lslb
 pshs b
 ldd bankaddr,y
 suba ,s+
 std bankbias,y
 rts
bnm7 puls u
 bra bnm5
#endasm
#endif

#ifndef _OS9
char *
bank_ptr(h)
H h;
{
    register int b = HBLK(h);

    if (b != bankcur)
        bank_map(b);
    return bankaddr + HOFF(h);
}
#else
/* bank_ptr(h) on the 6809: the block is the high byte / 4 - 1, the
 * offset the low 10 bits * 8 (HBLK, HOFF). In assembly because DCC's
 * shifts are library loops a bit at a time (h >> 11 and << 2 took 43%
 * of dco68's time), and every record access comes here. Step 26: h is
 * in the current block if its high byte without the low 2 bits is
 * bankhi, and then its address is h << 3 + bankbias */
#asm
bank_ptr: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkp1
 bsr bkmap
bkp1 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 leax d,x
 tfr x,d
 rts

* bkmap: X = the bias (as bankbias) for handle h, the caller's argument
* (4,s here: it is called by bank_ptr and the field readers when h's
* high byte isn't bankhi). A handle into Static leaves the current
* block alone (step 26: ilist, the list head, is there, and each use
* of it was a change of the current block and another back, 50,000 on
* te); any other maps its block. Keeps U and Y
bkmap lda 4,s
 anda #$FC
 cmpa #$FC
 bne bkm1
 leax Static,y
 leax -$E000,x (STATBLK + 1) << 13, the part of h << 3 above the offset
 rts
bkm1 lsra
 lsra
 deca
 tfr a,b
 clra
 pshs d
 lbsr bank_map
 ldx bankbias,y
 puls d,pc

* bkgetN(h): the 2-byte field at offset N of record h (op.h's I_SUCC...).
* Every record read comes here: each has bank_ptr's test inline (step
* 26: it was a call to a shared address routine, 17% of the time on te)
bkget0: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkg0
 bsr bkmap
bkg0 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 ldd d,x
 rts
bkget2: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkg2
 bsr bkmap
bkg2 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 leax d,x
 ldd 2,x
 rts
bkget4: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkg4
 bsr bkmap
bkg4 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 leax d,x
 ldd 4,x
 rts
bkget6: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkg6
 bsr bkmap
bkg6 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 leax d,x
 ldd 6,x
 rts
bkget8: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkg8
 bsr bkmap
bkg8 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 leax d,x
 ldd 8,x
 rts
bkget10: ldx bankbias,y
 lda 2,s
 anda #$FC
 cmpa bankhi,y
 beq bkg10
 bsr bkmap
bkg10 ldd 2,s
 lslb
 rola
 lslb
 rola
 lslb
 rola
 leax d,x
 ldd 10,x
 rts

* mnemof(h): instruction h's mnemonic, mnemtab[field 8] (op.h's MNEMOF)
mnemof: ldd 2,s
 pshs d
 lbsr bkget8
 leas 2,s
 aslb
 rola
 leax mnemtab,y
 ldd d,x
 rts

* talloc(n): a record of at least n bytes, from tfree's list for its
* size (n rounded up to 8) or new (inst.c, which has the C version)
talloc: ldd 2,s
 addd #7
 andb #$F8
 pshs d
 lsra
 rorb
 lsra
 rorb
 ldx #tfree
 leax d,x
 ldd ,x
 bne tal1
 lbsr bank_alloc
 puls x,pc
tal1 pshs d,x
 pshs d
 lbsr bank_ptr
 leas 2,s
 tfr d,x
 ldd ,x
 std [2,s]
 puls d,x
 leas 2,s
 rts

* tfreerec(h, n): record h, of at least n bytes, goes on tfree's list
tfreerec: ldd 4,s
 addd #7
 andb #$F8
 lsra
 rorb
 lsra
 rorb
 ldx #tfree
 leax d,x
 pshs x
 ldd 4,s
 pshs d
 lbsr bank_ptr
 leas 2,s
 tfr d,x
 ldd [,s]
 std ,x
 ldd 4,s
 std [,s++]
 rts
#endasm
#endif

H
bank_alloc(size)
int size;
{
    H h;

    size = (size + 7) & ~7;
    if (NBlk == 0 || Used + size > BLKSIZE)
        newblock();
    h = NextH;                  /* HANDLE(NBlk - 1, Used) */
    NextH += size >> 3;
    Used += size;
    memset(bank_ptr(h), 0, size);
    return h;
}

/* new zeroed storage in ordinary memory, reached by handle like the rest:
 * for the list heads, which every insertion touches */
H
bank_static(size)
int size;
{
    H h;

    size = (size + 7) & ~7;
    if (StatUsed + size > STATSIZE)
        error("out of static records");
    h = HANDLE(STATBLK, StatUsed);
    StatUsed += size;
    return h;
}

/* the blocks used so far (-D statistics) */
bank_blocks()
{
    return NBlk;
}

/* On OS-9 a block from F$AllRAM stays allocated after the process ends
 * unless it is given back, so every exit must call this */
bank_done()
{
#ifdef _OS9
    register int b;

    for (b = 0; b < NWIN; ++b)
        if (Window[b]) {
            clrblk(Window[b]);
            Window[b] = NULL;
        }
    for (b = 0; b < NBlk; ++b)
        delram(BlkNum[b]);
#endif
    NBlk = 0;
    bankcur = -1;
}

#ifdef _OS9
/* The system calls (as las/bank.c). DCC passes arguments on the stack
 * (the first at 2,s on entry) and returns in D; U and Y must be kept.
 *   allram ()         F$AllRAM, 1 block: its number, or -1
 *   mapblk (n)        F$MapBlk, block n: its address, or 0
 *   clrblk (addr)     F$ClrBlk, 1 block at addr
 *   delram (n)        F$DelRAM, block n */
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
