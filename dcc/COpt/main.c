#include "op.h"
/* #include "../ident.c"*/

FILE *in,*out,*datfile;
direct int dbflag,l2flag,inserts,inlcnt;

#ifdef DEBUG
direct int debugon,debugcnt;
#endif

extern int insmax;

main(argc,argv)
char **argv;
{
    int c;
    char *argp,*infile,*outfile;

    in = stdin;
    out = stdout;
    infile = outfile = NULL;

#ifdef DEBUG
    debugcnt = 29999;
#endif

    while (++argv, --argc) {
        if (*(argp = *argv) == '-')
            switch(*++argp) {
                case '\0':
                case 'D':
                    dbflag = 1;
                    break;
                case '2':
                    l2flag = 1;
                    break;
#ifdef DEBUG
                case 'd':
                    debugon = 1;
                    debugcnt = 0;
                    if (*++argp != '=') --argp;
                    while (c = *++argp)
                        debugcnt = debugcnt * 10 + (c - '0');
                    break;
#endif
                case 'i':
                    insmax = 0;
                    if (*++argp != '=') --argp;
                    while (c = *++argp)
                        insmax = insmax * 10 + (c - '0');
                    break;
                default:
                    error("unknown option '%s'\n",--argp);
            }
        else if (!infile) infile = argp;
        else if (!outfile) outfile = argp;
        else error("too many files");
    }

    if (infile)
        if((in = freopen(infile,"r",stdin)) == NULL)
            error("can't open %s",infile);

    if (outfile)
        if((out = fopen(outfile,"w")) == NULL)
            error("can't open %s",outfile);

    datfile = out;

#ifdef DEBUG
    debug("files open,");
#endif
    labelinit();
#ifdef DEBUG
    debug(" labels initted,");
#endif
    insinit();
#ifdef DEBUG
    debug(" instructs initted,");
#endif
    optinit();
#ifdef DEBUG
    debug("opts initted.\n");
#endif

    optimise();
#ifdef DEBUG
    debug("finished optimise\n");
#endif

    if(dbflag) {
        int totbytes,bytes;

        fprintf(stderr,"statistics:\n");
        fprintf(stderr,"   total instructions: %d\n",inserts);
        fprintf(stderr,"        long branches:  %5d, %5d, %3d%%\n",
                lbf,lbdone,percent(lbdone,lbf)) ;
        fprintf(stderr,"        deleted      :         %5d, %3d%%\n",
                opsdone,percent(opsdone,inserts));
        bytes = 2 * lbdone;
        bytes += estimate(opsdone);
        totbytes = estimate(inserts);
        fprintf(stderr,"        total bytes  :  %5d, %5d, %3d%%\n",
                totbytes,bytes,percent(bytes,totbytes));
        fprintf(stderr,"   memory: %d blocks of 8K",bank_blocks());
#if defined(_OS9) || defined(BANKWIN)
        { extern long Switches;
          fprintf(stderr,", %ld switches",Switches); }
#endif
        putc('\n',stderr);
    }
    bank_done();
}

int estimate(n)
{
    return n + n + (n * 3 / 5);
}

int percent(x,y)
unsigned x,y;
{
    return (y>50) ? x / ((y + 50) / 100) : 0;
}


static direct int datflag,endflag;
#define DIRECT  2

optimise()
{
    char line[LINESIZE];
    char lab[LABELSIZE+1],mnem[MNEMSIZE+1],args[ARGSIZE+1];
    H l,lp1,lp2,t;
    char *p;
    int dpflag,globflag;

    l = 0;
    datflag = endflag = FALSE;
    inlcnt = 0;

    while (IGETS(line,LINESIZE,in) != NULL) {     /* without the '\n' */
        ++inlcnt;
#ifdef DEBUG
        if (inlcnt >= debugcnt) dbflag = debugon;
        debug("\nline %5d: \"%s\"\n",inlcnt,line);
#endif
        p = line;

        if(*p == '\0' || *p == '*')
            continue;

        p = parse(LABEL,lab,p,&globflag);
        p = parse(MNEM,mnem,p);
        while (*p == ' ' || *p == '\t') ++p;

        /* scrutinise the first character for a match
         * before bothering 'strcmp()', he's very busy
         */
        switch(*mnem) {
            case 'e':
                if (strcmp(mnem,"endsect") == 0) {
                    endsect();
                    endflag = TRUE;
                    continue;
                }
                break;
            case 'i':
                if (strcmp(mnem,"info") == 0)
                    goto put;
                break;
            case 'n':
                if(strcmp(mnem,"nam") == 0) {
put:                OPUTS(line);
                    OPUTC('\n');
                    continue;
                }
                break;
            case 'p':
                if(strcmp(mnem,"psect") == 0) {
                    endsect();
                    goto put;
                }
                break;
            case 't':
                if (strcmp(mnem,"ttl") == 0)
                    goto put;
                break;
            case 'v':
                if (strcmp(mnem,"vsect") == 0) {
                    dpflag = (p[0] == 'd' && p[1] == 'p'
                                ? DIRECT : TRUE);
                    if (datflag == dpflag && endflag) {
                        endflag = FALSE;
                        continue;
                    }
                    endsect();
                    datflag = dpflag;
                    goto put;
                }
                break;
        }

        endsect();
        if (datflag) {
            OPUTS(line);        /* datfile is out */
            OPUTC('\n');
        } else {
            if (*lab) {
                if ((lp1 = findlabel(lab)) == 0)
                    lp1 = inslabel(lab);
                else {
                    /* devsys: a label defined twice (never in dcc68's
                     * output; a file without CR line ends, read in
                     * 99-character pieces, has them) linked itself into
                     * l and insins went round it for ever */
                    for (t = l; t && t != lp1; t = L_NEXTL(t));
                    if (t || L_DEST(lp1))
                        error("label %s defined twice", lab);
                }
                if (globflag) LAB(lp1)->lflags |= GLOBAL;
                for (lp2 = lp1; (t = L_NEXTL(lp2)) != 0; lp2 = t);
                LAB(lp2)->nextl = l;
                l = lp1;
            }
            if (*mnem) {
                parse(ARGS,args,p);
                if (insins(ilist,mnem,args,&l)) {
                    optim(I_PRED(ilist));
                    finddupl(I_PRED(ilist));
                }
                ++inserts;
                if (inscount >= insmax) delete();
            }
        }
    }

    if (l) {
        char nop[1];
        nop[0] = '\0';
        insins(ilist,"nop",nop,&l);
    }
    endsect();
    while (inscount) delete();
}

endsect()
{
    if (endflag) {
        if (datflag) datflag = FALSE;
        else while (inscount) delete();
        OPUTS(" endsect\n");
        endflag = FALSE;
    }
}

#ifndef _OS9
/* fgets, but the line ends where its '\n' was (main.c's IGETS) */
char *
lgets(s, n, fp)
char *s;
int n;
FILE *fp;
{
    register char *lp;

    if (fgets(s, n, fp) == NULL)
        return NULL;
    for (lp = s; *lp && *lp != '\n'; ++lp)
        ;
    *lp = '\0';
    return s;
}
#else
/* dco68 reads and writes every character of the file through stdio:
 * fgets calls getc for each, fputs calls putc for each, about 30
 * instructions a character (step 26: 14% of the time on te). These do
 * what they would, from and into the FILE's buffer directly (stdio.h:
 * _ptr at 0, _end at 4, _flag at 6), calling getc or putc only when
 * those would do more than take or store a character.
 *
 *   igets(s, n, fp)  fgets, but the line ends where its CR was (main
 *                    used to look for it again): up to n - 1 characters,
 *                    through the first CR; NULL if getc returns EOF,
 *                    even after some (as CoCoC's fgets: a last line
 *                    without a CR is lost)
 *   oputs(s)         fputs(s, out)
 *   oputc(c)         putc(c, out)
 *
 * putc only stores if the file is set up (_INIT), written to before
 * (_WRITTEN), for writing, without error, buffered and not SCF (which
 * is flushed at every CR), and the character leaves room in the buffer:
 * otherwise it may flush. getc takes the next character if _ptr < _end
 * and the file isn't one written in update mode (_WRITTEN). */
#asm
igets: pshs u
 leas -4,s
 ldx 12,s fp
 lda 6,x
 bita #1 _WRITTEN: getc returns EOF
 bne igs9
 ldd 10,s
 std 2,s count
 ldx 8,s s
 clr ,x
igs1 ldu 12,s
 ldd 4,u _end
 std ,s
 ldu ,u _ptr
igs2 ldd 2,s
 subd #1
 std 2,s
 beq igs5 n - 1 characters
 cmpu ,s
 bhs igs6 the buffer is empty
 ldb ,u+
 stb ,x+
 cmpb #$0D
 bne igs2
 leax -1,x the line ends at the CR
igs5 tfr u,d
 ldu 12,s
 std ,u _ptr
igs7 clr ,x
 ldd 8,s s
igs8 leas 4,s
 puls u,pc
igs6 tfr u,d
 ldu 12,s
 std ,u _ptr
 pshs x
 pshs u
 lbsr getc
 leas 2,s
 puls x
 cmpd #-1
 beq igs10
 cmpb #$0D
 beq igs7 the line ends here
 stb ,x+
 bra igs1
igs10 clr ,x
 clra
 clrb
 bra igs8
igs9 ldd 12,s
 pshs d
 ldd 12,s
 pshs d
 ldd 12,s
 pshs d
 lbsr fgets
 leas 6,s
 bra igs8 (NULL: written in update mode)

oputs: pshs u
 ldx 4,s s
 ldu out,y
 ldd 6,u _flag
 anda #$81 _INIT _WRITTEN
 andb #$66 _UNBUF _SCF _ERR _WRITE
 cmpd #$8102
 bne ops5
 ldd 4,u _end
 subd #1
 pshs d
 ldu ,u _ptr
ops1 ldb ,x
 beq ops3
 cmpu ,s
 bhs ops3 putc would flush: the rest through fputs
 leax 1,x
 stb ,u+
 bra ops1
ops3 tfr u,d
 ldu out,y
 std ,u _ptr
 leas 2,s
 tst ,x
 beq ops6
ops5 ldd out,y
 pshs d
 pshs x
 lbsr fputs
 leas 4,s
ops6 puls u,pc

oputc: ldx out,y
 ldd 6,x _flag
 anda #$81
 andb #$66
 cmpd #$8102
 bne opc5
 ldd ,x _ptr
 addd #1
 cmpd 4,x _end
 bhs opc5
 std ,x
 ldx ,x
 ldb 3,s
 stb -1,x
 rts
opc5 ldd out,y
 pshs d
 ldd 4,s
 pshs d
 lbsr putc
 leas 4,s
 rts
#endasm
#endif
