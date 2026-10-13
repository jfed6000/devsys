/* clibtest.c - what step 18 (docs/DESIGN.md D52) fixed in the C library
 * and in dcc68's constants. Each line was wrong or didn't link before:
 *  - unsigned long /, % and >> call _ludiv, _lumod, _lushr, which
 *    CoCoC's clib.l didn't have (unresolved);
 *  - 0.0 was 2^126 when dcc68 ran on the 6809, and a literal whose
 *    digits' low 32 bits had the top bit set was 2^32 short there;
 *  - HUGE_VAL was 0 when dcc68 ran on Linux, so ldexp's overflow was 0;
 *  - sintersect was defined only as "sinterse" (8 characters);
 *  - isatty and sleep together: a name clash (300 constants exported);
 *  - the time functions are KLibc's C ones now. */

#include <stdio.h>
#include <math.h>
#include <sets.h>
#include <time.h>

int bad;

check (ok, what)
int ok;
char *what;
{
    if (!ok) {
        printf ("%s wrong\n", what);
        ++bad;
    }
}

main ()
{
    unsigned long u = 0xEE6B2800;    /* 4,000,000,000 */
    double z = 0.0, big = 4294967295.0, h;
    char *a, *b;
    time_t t = 86400L * 365;

    check (u / 7 == 571428571, "ulong /");
    check (u % 7 == 3, "ulong %");
    check (u >> 4 == 250000000, "ulong >>");
    check (z == 0 && z + 1 == 1, "0.0");
    check (big == (double) 65535 * 65537, "4294967295.0");
    check (2147483647.0 == (double) 32767 * 65538 + 1, "2147483647.0");
    h = ldexp (1.0, 200);
    check (h == HUGE_VAL && h > 1e38, "ldexp overflow");
    a = allocset ();
    b = allocset ();
    addc2set (a, 'x');
    addc2set (a, 'y');
    addc2set (b, 'y');
    sintersect (a, b);
    check (smember (a, 'y') && !smember (a, 'x'), "sintersect");
    if (isatty (1))
        sleep (0);
    check (mktime (localtime (&t)) == t, "mktime (localtime)");
    if (bad)
        printf ("clibtest: %d wrong\n", bad);
    else
        printf ("clibtest: all right\n");
    exit (bad);
}
