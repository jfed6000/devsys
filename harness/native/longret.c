/* longret.c - a long or double result, returned in X, must survive the
 * function's return (step 14, docs/DESIGN.md D43). dco68 used to turn
 * "leas 2,s / puls u,pc" into "puls x,u,pc", which overwrote it: f1 and
 * f5 return a call's result after popping its one argument, f3 returns
 * a conversion with one int local on the stack. */

#include <stdio.h>

long lg (a) int a; { return (long) a * 1000; }
long f1 (a) int a; { return lg (a); }
long f3 (a) int a; { int b; b = a; return (long) b; }
double dg (a) int a; { return (double) a / 4; }
double f5 (a) int a; { return dg (a); }

main ()
{
    int bad = 0;

    if (f1 (7) != 7000L) { printf ("f1 wrong\n"); ++bad; }
    if (f3 (-5) != -5L) { printf ("f3 wrong\n"); ++bad; }
    if (f5 (10) != 2.5) { printf ("f5 wrong\n"); ++bad; }
    if (bad)
        printf ("longret: %d wrong\n", bad);
    else
        printf ("longret: all right\n");
    exit (bad);
}
