/* native.py's make suite (step 15, D45): a C program with one function
 * from util.c and one from dbl.a, built by make's default rules */
#include <stdio.h>

main ()
{
    printf ("prog: %d %d\n", add (2, 3), twice (21));
}
