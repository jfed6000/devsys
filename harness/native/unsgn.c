/* unsgn.c - an unsigned comparison with 0 must not test a stale carry
 * (docs/DESIGN.md D59). dco68 dropped "cmpd #0" after "ldd": a load sets
 * N and Z as the compare does but leaves C alone, so "while (n > 0)"
 * (bhi, which tests C and Z) went by whatever carry the instruction
 * before had left. Here that is "n - 8192", which sets it: llink's
 * bigbuf.c counted 1 block where a 31K psect needs 4. */

#include <stdio.h>

int blocks (n) unsigned n;      /* n > 0: bhi */
{
    int b = 0;

    while (n > 0)
    {
        ++b;
        n = n > 8192 ? n - 8192 : 0;
    }
    return b;
}

int blocks2 (n) unsigned n;     /* n <= 0: bls */
{
    int b = 0;

    for (;;)
    {
        if (n <= 0)
            break;
        ++b;
        n = n > 8192 ? n - 8192 : 0;
    }
    return b;
}

main ()
{
    int bad = 0;

    if (blocks (31826) != 4) { printf ("blocks wrong\n"); ++bad; }
    if (blocks2 (31826) != 4) { printf ("blocks2 wrong\n"); ++bad; }
    if (blocks (0) != 0) { printf ("blocks(0) wrong\n"); ++bad; }
    if (bad)
        printf ("unsgn: %d wrong\n", bad);
    else
        printf ("unsgn: all right\n");
    exit (bad);
}
