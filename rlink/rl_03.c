#include "module.h"
#ifndef _OS9
#include <unistd.h>
#include <sys/types.h>
#endif
#include "rlink.h"

char      idpdBuf[256];
char      _Crc[3] = { '\xff', '\xff', '\xff' };
char     *D0084 = idpdBuf;
LONGTYPE  cod_end;
LONGTYPE  IDatLoc;
LONGTYPE  IDpdLoc;
LONGTYPE  DTRefLoc;

static int idat_crc(int tcount);
extern void getref(def_ref *ref, char *filename);

// creatdnodes();

#ifdef _OS9
#   define WMODE "wx+"
#else
#   define WMODE "w+"
#endif


/* ******************************************************************** *
 * wrtlwmod() - --lwlink: write the module the way lwlink does for      *
 *          lwasm-style sources: header, all code, name, an edition     *
 *          byte only if the program's __os9 section gave one, CRC. No  *
 *          name before the code and no initialised-data tables (those  *
 *          are for Microware's C start-up code). las exports __os9     *
 *          with bit 0 set when __os9 set an edition.                   *
 * ******************************************************************** */

static void
wrtlwmod()
{
    mod_exec mod_hdr;
    SYMDAT *os9;
    int edbyte;
    char *mod_nm;
    static char os9name[] = "__os9";

    inpBuf = grab(obmemreq = maxbufsiz);
    obmemreq = 0;

    if (t_idat || t_idpd)
    {
        fatal("initialised data has no place in an lwlink-style module");
    }

    os9 = lookup(os9name);
    edbyte = (os9 && (os9->Offset & 1)) ? 1 : 0;

    mod_hdr.m_sync = MODSYNC;
    mod_hdr.m_size = o9_int(13 + t_code + namelen + edbyte + 3);
    mod_hdr.m_name = o9_int(13 + t_code);
    mod_hdr.m_tylan = ((int)TyLang) >> 8;
    mod_hdr.m_attrev = TyLang & 0xff;
    mod_hdr.m_parity = pty_gen((char *)(&mod_hdr.m_sync), 8);
    /* lwlink's entry is __start ("entry __start" in its os9 script);
     * by now its offset includes the module header */

    {
        SYMDAT *st = lookup ("__start");

        mod_hdr.m_exec = o9_int(st ? st->Offset : FirstMod->entry + 13);
    }
    mod_hdr.m_store = o9_int(t_idpd + t_udpd + t_idat + t_udat + extramem);

    if (!(OutFile = fopen(OutfName, WMODE)))
    {
        fatal("can't create output file");
    }

    if (ftruncate(fileno(OutFile), o9_int(mod_hdr.m_size)))
    {
        fatal("can't set file size");
    }

#ifdef _OS9
    crc((char *)(&mod_hdr.m_sync), 13, _Crc);
    fwrite(&mod_hdr.m_sync, sizeof(mod_exec), 1, OutFile);
#else
    fwrite(&mod_hdr, 8, 1, OutFile);
    crc((char *)(&mod_hdr), 8, _Crc);
    fputc((int)mod_hdr.m_parity, OutFile);
    crc((char *)(&mod_hdr.m_parity), 1, _Crc);
    fwrite(&mod_hdr.m_exec, 4, 1, OutFile);
    crc((char *)(&mod_hdr.m_exec), 4, _Crc);
#endif
    wrerr();

    /* Code of every psect, in link order, from offset 13 */

    creatdnodes(t_drefs);
    cod_end = 13;
    curObfil = FirstMod;

    while (curObfil)
    {
        if (curObfil->x19 & 0x100)
        {
            L14d3();
            wrerr();
        }

        curObfil = curObfil->obn;
    }

    /* Name, last character with bit 7 set, then the edition */

    fseek(OutFile, 13 + t_code, 0);
    mod_nm = modname;

    while(*(mod_nm++));

    mod_nm -= 2;
    *mod_nm |= 0x80;
    crc(modname, namelen, _Crc);
    fputs(modname, OutFile);
    *mod_nm &= 0x7f;

    if (edbyte)
    {
        crc(&edition, 1, _Crc);
        putc(edition, OutFile);
    }

    _Crc[0] = ~_Crc[0];
    _Crc[1] = ~_Crc[1];
    _Crc[2] = ~_Crc[2];
    fwrite(_Crc, 3, 1, OutFile);
    wrerr();
    fclose(OutFile);

    if (printmap)
        writemap();
}

wrtmodule()
{
    mod_exec mod_hdr;
    char *mod_nm;

    if (LwLink)
    {
        wrtlwmod();
        return 0;
    }

    inpBuf = grab(obmemreq = maxbufsiz);
    obmemreq = 0;
    mod_hdr.m_sync = MODSYNC;
    mod_hdr.m_size = o9_int(((namelen + t_drefs) * 2) + 13 + t_code + t_idat +  t_idpd + 13 );

    /*fprintf(stderr, "namelen: %d\n", namelen);
    fprintf(stderr, "t_drefs: %d\n", t_drefs);
    fprintf(stderr, " t_code: %d\n", t_code);
    fprintf(stderr, " t_idat: %d\n", t_idat);
    fprintf(stderr, " t_idpd: %d\n", t_idpd);*/
    mod_hdr.m_name = o9_int(0x0d);
    /* The following cast makes the code match the original,
     * but, in reality, it would probably be better as an unsigned
     * because we'd never want signed shift here */
    mod_hdr.m_tylan = ((int)TyLang) >> 8;
    mod_hdr.m_attrev = TyLang & 0xff;
    mod_hdr.m_parity = pty_gen((char *)(&mod_hdr.m_sync), 8);

    mod_hdr.m_exec = o9_int(B09Mod ? D002f : FirstMod->entry + bgn_codsiz);
    mod_hdr.m_store = o9_int(t_idpd + t_udpd + t_idat + t_udat + extramem);

    if (!(OutFile = fopen(OutfName, WMODE)))
    {
        fatal("can't create output file");
    }

#ifdef _OS9
    OutFile->_bufsiz = 512;
#endif

    if (ftruncate(fileno(OutFile), o9_int(mod_hdr.m_size)))
    {
        fatal("can't set file size");
    }

    /* Write module header to OutFile */
#ifdef _OS9
    crc((char *)(&mod_hdr.m_sync), 13, _Crc);
    fwrite(&mod_hdr.m_sync, sizeof(mod_exec), 1, OutFile);
#else
    /* The write has to be split up because of bit alignment */
    fwrite(&mod_hdr, 8, 1, OutFile);
    crc((char *)(&mod_hdr), 8, _Crc);
    fputc((int)mod_hdr.m_parity, OutFile);
    crc((char *)(&mod_hdr.m_parity), 1, _Crc);
    fwrite(&mod_hdr.m_exec, 4, 1, OutFile);
    crc((char *)(&mod_hdr.m_exec), 4, _Crc);
#endif
    wrerr();

    /* Write module name to header */
    mod_nm = modname;

    while(*(mod_nm++));

    mod_nm -= 2;    /* return to last char of name string */
    *mod_nm |= 0x80;    /* Last char in modname has hi bit set */
    crc(modname, namelen, _Crc);
    fputs(modname, OutFile);    /* was fprintf: a '%' in a name broke it */
    wrerr();
    crc(&edition, 1, _Crc);
    putc(edition, OutFile);
    *mod_nm &= 0x7f;    /* Reset last char of modname */

    creatdnodes(t_drefs);   /* Create all necessary dnodes */
    cod_end = ftell(OutFile);
    IDpdLoc = cod_end + t_code;
    IDatLoc = IDpdLoc + 2 + t_idpd + 2;
    DTRefLoc = IDatLoc + t_idat;
    curObfil = FirstMod;

    while (curObfil)
    {
        if (curObfil->x19 & 0x100)
        {
            L14d3();
            wrerr();
        }

        curObfil = curObfil->obn;
    }

    /* Init DPD */
    fseek(OutFile, IDpdLoc, 0);
#ifdef __LIL_END__
    t_idpd = o9_int(t_idpd);
#endif
    crc((char *)&t_idpd, 2, _Crc);
#ifdef __LIL_END__
    t_idpd = o9_int(t_idpd);
#endif
    putwrd(t_idpd, OutFile);
    crc(idpdBuf, t_idpd, _Crc);
    fwrite(idpdBuf, 1, t_idpd, OutFile);  /* Write all idpd data */
    wrerr();
#ifdef __LIL_END__
    t_idat = o9_int(t_idat);
#endif
    crc((char *)&t_idat, 2, _Crc);
#ifdef __LIL_END__
    t_idat = o9_int(t_idat);
#endif
    putwrd(t_idat, OutFile);

    if (t_idat)
    {
        idat_crc(t_idat);
    }

    /* Data-Text Refs */
    fseek(OutFile, DTRefLoc, 0);
#ifdef __LIL_END__
    dtcount = o9_int(dtcount);
#endif
    crc((char *)&dtcount, 2, _Crc);
#ifdef __LIL_END__
    dtcount = o9_int(dtcount);
#endif
    putwrd(dtcount, OutFile);     /* Write Data-Text count */
    writrefs(dtxtrefs);
    /* Data-Data Refs */
#ifdef __LIL_END__
    ddcount = o9_int(ddcount);
#endif
    crc((char *)&ddcount, 2, _Crc);
#ifdef __LIL_END__
    ddcount = o9_int(ddcount);
#endif
    putwrd(ddcount, OutFile);     /* Write Data-Data count */
    writrefs(ddtarefs);

    crc(modname, namelen + 1, _Crc);
    fputs(modname, OutFile);

    putc(0, OutFile);
    _Crc[0] = ~_Crc[0];
    _Crc[1] = ~_Crc[1];
    _Crc[2] = ~_Crc[2];
    fwrite(_Crc, 3, 1, OutFile);
    wrerr();
    fclose(OutFile);

    if (printmap)
        writemap();

    if (B09Mod && okstatic)
        fprintf(stderr, "BASIC09 static data size is %d bytes\n", t_udat);
}


/* Load idat and calculate CRC for it */
idat_crc(tcount)
    int tcount;
{
    char buf[128];
    int nbyts;

    fseek(OutFile, ftell(OutFile), 0);

    while ((tcount > 0) && (nbyts = fread(buf, 1, sizeof(buf), OutFile))) 
    {
        wrerr();

        if (nbyts > tcount)
            nbyts = tcount;

        crc(buf, nbyts, _Crc);
        tcount -= nbyts;
    }
}


L14d3()
{
    SYMDAT *thissym;
    def_ref syminf;
    int     totcnt;

    if (curRofNa != curObfil->roffile) /* else L1513 */
    {
        if (!(InFile = freopen((curRofNa = curObfil->roffile), "r", InFile)))
            fatal("can't reopen input file %s", curObfil->roffile);
    }

    fseek(InFile, curObfil->rcodbgn, 0);

    /* Read in code + init data */
    if (curObfil->codesiz + curObfil->IDpD + curObfil->IDat > 0)
    {
        if (!(fread(inpBuf, curObfil->codesiz + curObfil->IDpD + curObfil->IDat,
                        1, InFile)))
        {
            rderr(curObfil->roffile);
        }
    }

    /* Do External Refs */
    totcnt = getwrd(InFile);

    if (ferror(InFile))   /* else L15f3 */
    {
        rderr(curObfil->roffile);
    }

    while (totcnt--)
    {
        char mynam[SYMLEN+1];
        register int symcnt;

        getname(mynam, SYMLEN);
        symcnt = getwrd(InFile);

        if (ferror(InFile))
        {
            rderr(curObfil->roffile);
        }

        if (!(thissym = lookup(mynam)))   /* else L15e9 */
            fatal("symbol %s not found in codgen", mynam);

        while (symcnt--)
        {
			getref (&syminf, curObfil->roffile);

            if (ferror(InFile))
                rderr(curObfil->roffile);

            procref(&syminf, thissym);
        }
    }   /* @L15f3 */

    /* Local Refs */
    totcnt = getwrd(InFile);

    if (ferror(InFile))
        rderr(curObfil->roffile);

    while (totcnt--)
    {
#ifdef _OS9
        fread (&syminf, 3, 1, InFile);
#else
        syminf.r_flag = fgetc(InFile);
        syminf.r_offset = getwrd(InFile);
#endif

        if (ferror(InFile))
            rderr(curObfil->roffile);

        procref(&syminf, 0);
    }

    /* Common Data */
    if (curObfil->spare > 0)   /* else L1708 */
    {
        totcnt = getwrd(InFile);

        while (totcnt--)
        {
            char nam[SYMLEN + 1];
            register int count;

            getname(nam, SYMLEN);
            count = getwrd(InFile);

            if (ferror(InFile))
                rderr(curObfil->roffile);

            if (!(thissym =  lookup(nam)))
                fatal("common block %s not found", nam);

            count = getwrd(InFile);

            while (count--)
            {
                fread(&syminf, 3, 1, InFile);

                if (ferror(InFile))
                    rderr(curObfil->roffile);

                procref(&syminf, thissym);
            }
        }
    }

    crc(inpBuf, curObfil->codesiz, _Crc);

    if (!D0061)
        fseek(OutFile, cod_end, 0);

    /* Write code to module */
    fwrite(inpBuf, curObfil->codesiz, 1, OutFile);  /* L1737 */
    cod_end += curObfil->codesiz;
    D0061 = 1;
    wrerr();
    totcnt = 0;
    {
        register char *cp = &inpBuf[curObfil->codesiz];

        while (totcnt < curObfil->IDpD)
        {
            *(D0084++) = *(cp++);
            ++totcnt;
        }
    }

    if (curObfil->IDat)        /* else L1800 */
    {
        fseek(OutFile, IDatLoc, 0);
        fwrite(&inpBuf[curObfil->codesiz + curObfil->IDpD], curObfil->IDat,
                1, OutFile);
        IDatLoc += curObfil->IDat;
        wrerr();
        D0061 = 0;
    }

    bgn_udat += curObfil->UDat;
    bgn_udpd += curObfil->UDpD;
    bgn_idat += curObfil->IDat;
    bgn_idpd += curObfil->IDpD;
    bgn_codsiz += curObfil->codesiz;
    curObfil->UDat = bgn_udat - curObfil->UDat;
    curObfil->UDpD = bgn_udpd - curObfil->UDpD;
    curObfil->IDat = bgn_idat - curObfil->IDat;
    curObfil->IDpD = bgn_idpd - curObfil->IDpD;
    curObfil->codesiz = bgn_codsiz - curObfil->codesiz;
}

/* Process a ref */

/*void*/
procref(b3, ob)
    def_ref *b3; SYMDAT *ob;
{
    union {
        int  it;
        UINT *ipt;
        char  *cp;
    } realaddr;

    INTTYPE myval;
    char    reftype,
            obtype,
            myloc;

    realaddr.cp = &inpBuf[b3->r_offset];
    reftype = b3->r_flag;

    if (ob)     /* else L189a */
    {
        myval = ob->Offset;
        obtype = (char)(ob->Type);

        if ((reftype & RELATIVE))        /* else L18e0 */
        {
            myval -= bgn_codsiz;
        }
    }
    else
    {
        switch ((obtype = reftype) & 0x7)       /* L189a */
        {
            case 0:                 /*L18a5*/
                myval = bgn_udat;
                break;
            case INIENT:            /*L18a9*/
                myval = bgn_idat;
                break;
            case DIRENT:            /*L18ad*/
                myval = bgn_udpd;
                break;
            case DIRENT | INIENT:   /*L18b1*/
                myval = bgn_idpd;
                break;
            case CODENT:            /*L18b5*/
                myval = bgn_codsiz;
                break;
            default:                /*L18bb*/
                fatal("ref type error");
        }
    }

    if (reftype & NEGMASK)
    {
        myval = -myval;
    }

    if ((myloc = (reftype & LOCMASK)) != CODLOC)
    {   /* It's a data def */
        realaddr.it += curObfil->codesiz;

        if (!(myloc & DIRLOC))  /* non-dpd */
        {
            realaddr.it += curObfil->IDpD;
        }
    }

    if (reftype & LOC1BYT)  /* else L1970 */
    {
        if (((obtype & 0x07) == CODENT) && (myloc == CODLOC))  /* else L1965 */
        {
            int vvtyp;

            if (((vvtyp = *realaddr.cp + myval) < -128) || (vvtyp > 127))
            {
                fatal("\npcr byte ref to '%s' out-of-range at $%04x in psect %s",
                        ob->sname, b3->r_offset, curObfil);
            }
        }

        *realaddr.cp += myval;
    }
    else
    {   /* L1970 */
#ifdef _OS9
        *realaddr.ipt += myval;
#else
        *realaddr.ipt = o9_int(o9_int(*realaddr.ipt) + myval);
#endif
    }

    if (myloc != CODLOC)   /* else L19c4 */
    {
        register UINT dst;

        dst = (myloc & DIRLOC ? bgn_idpd : bgn_idat) + b3->r_offset;

        if ((obtype & 0x07) == CODENT)    /* else L19b0 */
        {
            getrefs(&dtxtrefs, dst);
            ++dtcount;
        }
        else
        {
            getrefs(&ddtarefs, dst);
            ++ddcount;
        }
    }
}

int
pty_gen(ip, c)
    register char *ip; int c;
{
    int c8_00 = -1;

    while (c--)
    {
        c8_00 ^= *(ip++);
    }

    return c8_00;
}

/* Temporary fix ??? */
#ifdef _OS9
#asm
ftruncat pshs   x,u
      tfr    b,a
      ldb    #SS.Size
      ldx    #0
      ldu    6,s
      os9    I$SetStt
      puls   x,u
      lbra   _sysret
#endasm
#else

int
crc(start, count, accum)
    char *start;
    int count;
    char *accum;
{
    unsigned char   a;
    unsigned int idx;

    for (idx = 0; idx <count; idx++)
    {
        a = start[idx];
        a ^= accum[0];
        accum[0] = accum[1];
        accum[1] = accum[2];
        accum[1] ^= (a >> 7);
        accum[2] = (a << 1);
        accum[1] ^= (a >> 2);
        accum[2] ^= (a << 6);
        a ^= (a << 1);
        a ^= (a << 2);
        a ^= (a << 4);

        if (a & 0x80)
        {
            accum[0] ^= 0x80;
            accum[2] ^= 0x21;
        }
    }

	return 0;
}
#endif

#ifdef __LIL_END__
INTTYPE
getwrd (stream)
FILE *stream;
{
	unsigned char high = getc (stream);
	unsigned char low = getc (stream);

	return (high << 8) + low;
}

int
putwrd (wrd, stream)
INTTYPE wrd;
FILE *stream;
{
    if ((putc(wrd >> 8, stream) != EOF) && (putc(wrd & 0xff, stream) != EOF))
        return wrd;
    else
        return EOF;
}
#endif
