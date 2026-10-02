/* hello.c - harness/native.py "cc" suite: compiled and linked on the F256 by
 * the native dcc, and on Linux by the host passes; both must agree.
 * Uses a name longer than 8 characters and a CoCoC library call. */

#include <stdio.h>

int long_counter_name;

count_upwards (limit)
int limit;
{
    int i;

    for (i = 0; i < limit; ++i)
        long_counter_name += i;
    return long_counter_name;
}

main ()
{
    printf ("hello from dcc on the F256: %d\n", count_upwards (10));
    exit (0);
}
