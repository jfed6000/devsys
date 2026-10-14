/* regwidth.c - dcc68 must not take a register for a value of another
 * width at the same place (docs/DESIGN.md D53). Its register tracking
 * (REGCONTS) matched expressions by operator, value and modifier only:
 * after "ldb [7,s]" for p->c[0] it took D to hold p->i too, and stored D
 * where "ldd [7,s]" was needed. Each function reads one place as a char
 * and as an int, in either order. */

#include <stdio.h>

union u { char c[2]; int i; };

int w1 (p) union u *p;          /* char, then int */
{
    char c; int i;

    c = p->c[0];
    i = p->i;
    return c + i;
}

int w2 (p) union u *p;          /* int, then char */
{
    char c; int i;

    i = p->i;
    c = p->c[0];
    return c + i;
}

int w3 (p) union u *p;          /* char, then int, as a value */
{
    int r;

    r = p->c[0];
    if (p->i == 0x1234) r += 1000;
    return r;
}

int w4 (p) union u *p;          /* int, then char, as a value */
{
    int r;

    r = p->i;
    r += p->c[0];
    return r;
}

main ()
{
    union u x;
    int bad = 0;

    x.i = 0x1234;
    if (w1 (&x) != 0x1234 + 0x12) { printf ("w1 wrong\n"); ++bad; }
    if (w2 (&x) != 0x1234 + 0x12) { printf ("w2 wrong\n"); ++bad; }
    if (w3 (&x) != 1000 + 0x12) { printf ("w3 wrong\n"); ++bad; }
    if (w4 (&x) != 0x1234 + 0x12) { printf ("w4 wrong\n"); ++bad; }
    if (bad)
        printf ("regwidth: %d wrong\n", bad);
    else
        printf ("regwidth: all right\n");
    exit (bad);
}
