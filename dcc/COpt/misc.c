#include "op.h"

#ifdef UNIX
# include <unistd.h>
#endif

#define GRABSIZE 512

char *
grab(n)
unsigned n;
{
    char *ptr;

    if ((ptr = sbrk(n)) == (char *)-1) error("memory overflow");
    return (char *)ptr;
}

error(s1,s2,s3,s4)
char *s1,*s2,*s3,*s4;
{
    fprintf(stderr,"error: ");
    fprintf(stderr,s1,s2,s3,s4);
    putc('\n',stderr);
    bank_done();        /* devsys: give the RAM blocks back */
    exit(1);
}

#ifdef DEBUG
debug(fmt,s1,s2,s3,s4)
char *fmt,*s1,*s2,*s3,*s4;
{
    if (dbflag) fprintf(stderr,fmt,s1,s2,s3,s4);
}

prtins(i)
H i;
{
    H l, a;
    int t;

    if (dbflag) {
        fprintf(stderr,"@%04x ",i);
        for (l = I_LLIST(i); l; l = L_NEXTL(l))
            fprintf(stderr,"%s%s",LAB(l)->lname,L_NEXTL(l) ? "/" : "");
        if (i == ilist)
            fprintf(stderr," (list head)\n");
        else {
            t = I_ITYPE(i);
            fprintf(stderr," %s ",INS(i)->mnem);
            a = I_ARG(i);
            if (a == 0)         /* a branch whose label isn't set yet */
                putc('\n',stderr);
            else
                fprintf(stderr,"%s\n",(t & BRANCH ? LAB(a)->lname : ARG(a)));
        }
    }
}
#endif
