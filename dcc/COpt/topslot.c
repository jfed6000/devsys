/* topslot.c - native dco68 must fill its top memory slot (devsys, D61)
 *
 * dco68 maps its record blocks with F$MapBlk, which gives the highest
 * free slot of the address space. Unless dco68's own code reaches the
 * top slot ($E000-$FFFF), that slot is free, and a block mapped there
 * covers the system's page at $FE00: the machine hangs at the next
 * interrupt (D51, docs/nitros9-fixes.md #5). A module reaches the top
 * slot when it ends by $FD00 in its last block: at most 15,616 bytes
 * in two blocks, 16,385 to 23,808 in three.
 *
 * Two blocks were full (D58: 15,602 bytes), so the zeros below move
 * dco68 to three: its code starts at $A000 and must end at $E000 or
 * above. That leaves about 6K for dco68 to grow; when it needs more,
 * take zeros away (about 700 can go before etext falls below $E000;
 * native/GNUmakefile checks). They are in this file, not one of their
 * own, because llink (like rlink) keeps an object only when something
 * uses one of its names: bank.c calls topslot() before the first block. */

#ifdef _OS9
topslot()
{
    extern int etext();

    if ((unsigned) etext < 0xE000)
        error("dco68 doesn't fill its top memory slot ($E000): too few zeros in topslot.c");
}

#asm
 rzb 2048
#endasm
#endif
