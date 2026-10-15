/*
    Modification history for cmain.c:
        25-May-83       Make *p be the register variable.
        24-Apr-84 LAC   Conversion for UNIX.
*/

#define MAIN
#include "cj.h"
#include <signal.h>


char strname[] = "cstr.XXXXXX";

#ifdef  SPLIT
direct char *infile;

direct int eflag;
#endif

#ifndef PASS2
/* pass1 only */
direct  int     *inclptr;       /* include stack pointer */
#endif     // <<<- inserted to match preceeding "ifndef PASS2"

main(argc,argv)
char **argv;
{
        register char *p;

        int tidy();

#ifdef __unix__
        signal(SIGINT,tidy);
#else
        intercept(tidy);
#endif

        {
            register symrec *p;

            /* zeroed: no dimensions, offset 0 */
            p = SYMP(sdummy = symanon());
            p->type = INT;
            p->size = INTSIZE;
            p->storage = EXTDEF;
            strcpy(p->name,"_dummy_");
        }

        lexinit();
        in = stdin;
        code = stdout;

        while (--argc>0 ) {
                if (*(p= *++argv)=='-') {
                        while(*++p) switch(*p ) {
#ifdef  SPLIT
                            case 'e':
                                eflag = 1;
                                break;
#endif
                            case 's':
                                sflag = 1; break;
#ifdef  PTREE
                            case 'd':
                                dflag = 1;
                                break;
#endif
                            case 'o':
                                if(*++p=='=') {
                                    if((code=fopen(++p,"w"))==NULL) {
                                        fprintf(stderr,"can't open %s\n",p);
                                        errexit();
                                    }
                                }
                                goto done;
#ifdef PROF
                            case 'p':
                                   pflag=1;
                                   break;
#endif
                            default:
                                fprintf(stderr,"unknown flag: -%c\n",*p);
                                errexit();
                        }
                        done: ;
                }
                else {
#ifdef DEBUG
                        if((in = fopen(*argv,"r")) == NULL)
#else
                        if((in = freopen(*argv,"r",stdin)) == NULL)
#endif
                                fatal(M_CANT_OPEN_INPUT_FILE);
#       ifdef   SPLIT
                        infile = *argv;
#       endif
                }
        }


#ifdef  PTREE
        if(dflag)
                getkeys();
#endif

        preinit();
        getsym();
        while(sym!=EOF)
                extdef();          /* go do it!!! */

        epilogue();

        if(ferror(code))
                fatal(M_ERROR_WRITING_ASSEMBLY);
        fflush(stdout);

        if (errcount) {
                fprintf(stderr,"errors: %d\n",errcount);
                errexit();
        }
        symdone();
        exit(0);
}


errexit()
{
#ifdef  SPLIT
                if(infile)
                        unlink(infile);
#endif
                symdone();
                exit(1);
}
