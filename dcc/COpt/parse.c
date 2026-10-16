#include "op.h"
#include "ctype.h"

#ifndef _OS9
char *
parse(type,s,p,globflag)
int type,*globflag;
register char *s,*p;
{
    int size;
#ifdef DEBUG
    char *save = s;
#endif

    switch(type) {
        case LABEL:
            size = LABELSIZE;
            if (*p == ' ' || *p == '\t') break;
            *s++ = *p++;
            while (--size && isalnum(*p)) *s++ = *p++;
            while (isalnum(*p)) ++p;
            if (*globflag = *p == ':') ++p;
            break;
        case MNEM:
            size = MNEMSIZE;
            while (*p == ' ' || *p == '\t') ++p;
            while (size-- && isalnum(*p)) *s++ = *p++;
            while (isalnum(*p)) ++p;
            break;
        case ARGS:
            size = ARGSIZE;
            while (*p == ' ' || *p == '\t') ++p;
            while (size-- && *p) *s++ = *p++;
            break;
        default:
            error("parse called with bad type: %d",type);
    }

    *s = '\0';
#ifdef DEBUG
    switch(type) {
        case LABEL:
            debug("parsed label: \"%s\"\n", save);
            break;
        case MNEM:
            debug("parsed mnemonic: \"%s\"\n",save);
            break;
        case ARGS:
            debug("parsed args: \"%s\"\n",save);
    }
#endif
    return p;
}
#else
/* The same on the 6809 in assembly (step 26: DCC kept p on the stack,
 * about 14 instructions a character, 6% of the time on te). X is p, Y
 * is s, U the character table (isalnum is _chcodes2[c] > 0, c a signed
 * char: tst b,u reads the same byte), A counts. No operand here may
 * end in ",y": dco68 -2, which this file goes through too, makes every
 * such one an extended address (level2.patterns: data at 0), hence
 * "clr ,y+" at the end. Y is put back before error is called; error
 * doesn't return */
#asm
parse: pshs y,u
 ldy 8,s s
 ldx 10,s p
 ldu #_chcodes2
 ldd 6,s type
 cmpd #1 LABEL
 beq prlab
 cmpd #2 MNEM
 beq prmn
 cmpd #3 ARGS
 beq prarg
 ldy ,s
 pshs d
 leax prerr,pcr
 pshs x
 lbsr error
prerr fcc "parse called with bad type: %d"
 fcb 0
prlab ldb ,x
 cmpb #$20
 beq prend
 cmpb #9
 beq prend
 stb ,y+
 leax 1,x
 lda #31 (LABELSIZE - 1 more)
prl1 ldb ,x
 tst b,u
 ble prl2
 stb ,y+
 leax 1,x
 deca
 bne prl1
prl2 ldb ,x
 tst b,u
 ble prl3
 leax 1,x
 bra prl2
prl3 ldu 12,s globflag
 clra
 clrb
 std ,u
 lda ,x
 cmpa #':
 bne prend
 leax 1,x
 incb
 std ,u
 bra prend
prmn bsr prskip
 lda #10 MNEMSIZE
prm1 ldb ,x
 tst b,u
 ble prm2
 stb ,y+
 leax 1,x
 deca
 bne prm1
prm2 ldb ,x
 tst b,u
 ble prend
 leax 1,x
 bra prm2
prarg bsr prskip
 lda #90 ARGSIZE
pra1 ldb ,x
 beq prend
 stb ,y+
 leax 1,x
 deca
 bne pra1
prend clr ,y+
 tfr x,d
 puls y,u,pc
prskip ldb ,x
 cmpb #$20
 beq prs1
 cmpb #9
 bne prs2
prs1 leax 1,x
 bra prskip
prs2 rts
#endasm
#endif
