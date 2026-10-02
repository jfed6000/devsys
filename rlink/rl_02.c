#include "rlink.h"

direct int  iobufsiz = 0x400;
direct SYMDAT *curntsym = 0;

struct ext_ref   curExtRef = {&curExtRef, &curExtRef,0,0,0,0,0,0};  /*D0067*/
char   *symEtxt = "etext";
char   *symEDat = "edata";
char   *symEnd = "end";
char   *symDpsiz = "dpsiz";
char   *symBTxt = "btext";

extern direct int   B09Mod;
extern direct UINT   TyLang;   /* D0017 */
extern direct UINT   namelen;

extern char *nxtfnam();
static SYMDAT *newsym();

static addsym ();
SYMDAT *lastobfile();

static void skipxtrn();

readrofs ()
{
    binhead bh;
    binhead *v0;

    rofcount = 0;

    /* Process each ROF file */
    while (RofFil = nxtfnam ())
    {
#ifdef _OS9
        if (!(InFile = freopen (RofFil, "r", InFile)))
        {
            cantopen ();
        }

        InFile->_bufsiz = iobufsiz;
#else
        if (InFile)
            fclose(InFile);

        if (!(InFile = fopen(RofFil, "r")))
            cantopen();
#endif

        /* Process each ROF in file */
        /* This while processes one ROF in a file,
         * repeating for each ROF */

        while (fread (&bh, sizeof (binhead), 1, InFile)) /* L0856 */
        {
#ifdef __LIL_END__
            {
                /* Swap bytes on word elements */
                UINT *pt = &bh.h_glbl;
                bh.h_tylan = o9_int(bh.h_tylan);

                while (pt <= &bh.h_entry)
                {
                    *pt = o9_int(*pt);
                    ++pt;
                }
            }
#endif
            if (bh.h_sync != ROFSYNC)
            {
                notROF ();
            }
            else if (bh.h_valid != 0)
            {
                asmerrs ();
            }
            else if (bh.h_vers > 1)
            {
                toonew ();
            }
            else if (B09Mod) /* L07a7 */
            {
                if (bh.h_tylan)
                {
                    illmainline ();
                }
            }
            else if (LwLink)
            {
                /* lwlink's layout can put library code before the
                 * mainline (lwsel.c), so it may be any module */

                if (bh.h_tylan)
                {
                    if (TyLang)
                    {
                        twomains ();
                    }

                    TyLang = bh.h_tylan;

                    if (edition == 0)
                    {
                        edition = bh.h_edit;
                    }
                }
            }
            else if (rofcount)  /* L07af */
            {
                if (bh.h_tylan)
                {
                    twomains ();
                }
            }

            else
            {
                if (bh.h_tylan == 0)  /* L07bc */
                {
                    fatal ("'%s' contains no mainline", RofFil);
                }
                else
                {
                    TyLang = bh.h_tylan;   /* L07d3 */
                }

                if (edition == 0)       /* L07d5 */
                {
                    edition = bh.h_edit;
                }
            }

            L0937(&bh, 0);             /* L07e1 */

            /* Initialize builtin syms only once */
            if (rofcount == 0) /* L07ee - else L0856 */
            {
                OBFILES *d0 = FirstMod;

                addsym (d0, symEtxt, 0x100 | CODENT);
                addsym (d0, symBTxt, 0x100 | CODENT);
                addsym (d0, symEDat, 0x100 | INIENT);
                addsym (d0, symEnd, 0x100);
                addsym (d0, symDpsiz, 0x100 | DIRENT);
            }
        }       /* end process each ROF in file */

        ++rofcount;
    }                   /* L0878 - end process each file */

    if (LwLink && ! TyLang)
    {
        fatal ("no mainline");
    }

    /* All files processed - now set up data */
    /* Do Libraries */

    while ((curExtRef.next != &curExtRef) && (RofFil = nxtlib()))
    {
        fclose (InFile);

        if (!(InFile = fopen (RofFil, "r")))
        {
            cantopen ();
        }

        while ((curExtRef.next != &curExtRef) &&
                (fread(&bh, sizeof(binhead), 1, InFile)))
        {
#ifdef __LIL_END__
            {
                /* Swap bytes on word elements */
                UINT *pt = &bh.h_glbl;

                while (pt <= &bh.h_entry)
                {
                    *pt = o9_int(*pt);
                    ++pt;
                }
            }
#endif
            if (bh.h_sync != ROFSYNC)
            {
                notROF ();
                continue;
            }

            if (bh.h_valid)
            {
                asmerrs ();
                continue;
            }

            if (bh.h_vers > 1)
            {
                toonew();
                continue;
            }   /* go to L08f6 */
            else if (bh.h_tylan)    /* else L08e9 */
            {
                if (B09Mod)     /* else L08e4 */
                {
                    illmainline (); /* go to L08f6 */
                    continue;
                }
                else
                {
                    twomains ();
                    continue;
                }
            }
            else
            {
                L0937 (&bh, 1);
            }
        }        /* L08f6 */
    }       /* L091d */
}

/* -------------------------------------------------------------------- *
 * Acquire information from current ROF.   This may not be an           *
 *      entire file.                                                    *
 * On entry, the file is positioned at the first byte past the binhead  *
 * -------------------------------------------------------------------- */

L0937 (bp, do_vars)
    binhead *bp; int do_vars;
{
    OBFILES *newobj;
    SYMDAT *my_sym;      /* v39 */
    UINT   v37;
    UINT   commnloc;
    UINT   gcount;         /* v33 */
    UINT   v31;
    UINT   nosize;
    UINT   v27;
    UINT   v25;
    UINT   v23;
    UINT   v21;
    UINT   v19;
    UINT   v17;
    /* Don't know why they did it this way, Only SYMLEN +1 needed
     * Will maintain it this way to keep stack like original */
    char    tmpnam[SYMLEN + 7];
/*    SYMDAT *v01*/
    char    v00;

    if (!CurntOB)
    {
        newobj = (OBFILES *)grab(sizeof(OBFILES));
    }
    else
    {
        newobj = CurntOB;
        CurntOB = newobj->obn;
    }

    newobj->drefs = (UINT)(uintptr_t)(newobj->symlist = 0);

    /* Read in module name */
    getname (newobj->obname, 16);
    newobj->roffile = RofFil;
#ifdef _OS9
    _strass (&newobj->edition, &bp->h_edit, 16);
#else
    /* To avoid possibility of byte alignment */
    /*memcpy(&newobj->edition, &bp->h_edit, 16);*/
    newobj->edition = bp->h_edit;
    newobj->spare = bp->h_vers;
    memcpy(&newobj->UDat, &bp->h_glbl, 14);
#endif

    if (do_vars)
    {
        nosize = ((newobj->UDat + newobj->UDpD + newobj->IDat + newobj->IDpD +
                newobj->codesiz) == 0);
    }
    else
    {
        nosize = 0;
    }

    /* Globals */

    gcount = getwrd(InFile);

    /* Create SYMDAT for each global definition */
    while (gcount--)    /* L0a1c */
    {
        my_sym = newsym();
        getname(my_sym->sname, SYMLEN);
        my_sym->Type = getc(InFile);
        my_sym->Offset = getwrd(InFile);

        /* nosize true only if do_vars is true and no data or code */
        if (nosize && !findext(my_sym->sname, 1))      /* L09e9 */
        {
            my_sym->sprev = curntsym;
            curntsym = my_sym;
        }
        else
        {
            my_sym->sprev = newobj->symlist;
            newobj->symlist = my_sym;
        }
    }

    /* If data or code for real */
    if (!nosize && do_vars)        /* else L0aab */
    {
        my_sym = newobj->symlist;

        while (my_sym)
        {
            if (findext(my_sym->sname, 1))
            {
                break;
            }

            my_sym = my_sym->sprev;
        }

        /* If symbol not already present, append it to end of
         * obfile's symlist move to end of rof and return
         */
        if (!my_sym)     /* symbol not found */
        {
            SYMDAT *lstob;

            if ((lstob = lastobfile (newobj->symlist)))
            {
                lstob->sprev = curntsym;
                curntsym = newobj->symlist;
            }

            /* L0a80 */
            goextref (newobj);       /* skip to external refs */
            skipxtrn ();
            skiprefs (getwrd (InFile));  /* Skip local refs */
            skipcomn (bp->h_vers);        /* Skip commons */
            newobj->obn = CurntOB;
            CurntOB = newobj;
            return 0;
        }
    }

    my_sym = newobj->symlist;

    /* Search for duplicate SYMDATs */
    while (my_sym)
    {
        /* lwlink accepts the same name exported by two objects (lwasm's
         * export pragma exports every label, e.g. "loop"); each object's
         * own references were resolved when it was assembled. */

        if (L1bcf (my_sym) && ! LwLink)
        {
            fprintf (stderr, "symbol already defined: %-8s in %s\n",
                    my_sym, newobj);
            ++dup_find;
        }

        my_sym = my_sym->sprev;
    }

    if ( ! D003f )      /* else l0af9 */
    {
        FirstMod = D003f = newobj;
    }
    else
    {
        D003f = D003f->obn = newobj;
    }
    /* L0b03 */
    newobj->rcodbgn = ftell (InFile);
    goextref (newobj);

    /* Get external refs */
    gcount = getwrd (InFile);

    while (gcount--)
    {
        getname(tmpnam, SYMLEN);
        
        if ((my_sym = lookup (tmpnam)))  /* L0b32 - else L0b49 */
        {
            my_sym->Type |= 0x100;
        }
        else
        {
            struct ext_ref *er;

            er = L1d61(tmpnam);
            strcpy(er->name, tmpnam);
            er->myOb = newobj;
        }

        newobj->drefs += ncodlocs(getwrd(InFile));     /* L0b6b */
    }   /* L0b82 */

    gcount = getwrd(InFile);     /* Locals count */

    if (ferror(InFile))
    {
        in_rderr();
    }

    while (gcount--)
    {
        newobj->drefs += ncodlocs(1);     /* L0ba8 */ 
    }

    if (bp->h_vers > 0)     /* else L0c78 */
    {
        gcount = getwrd(InFile);

        while (gcount--)
        {
            getname(tmpnam, SYMLEN);
             commnloc = getwrd(InFile);
            
            if ((my_sym =  lookup(tmpnam)))    /* else L0c27 */
            {
                if ((my_sym->Type & 7) != 7)
                {
                    fatal("common panic mode 1!");
                }

                if ( commnloc > my_sym->Offset)
                {
                    my_sym->Offset =  commnloc;
                }

                skiprefs(getwrd(InFile));
                continue;
            }

            my_sym = newsym();
            my_sym->sprev = commnsyms;
            commnsyms = my_sym;
            strcpy(my_sym->sname, tmpnam);
            my_sym->Type = 7;
            my_sym->Offset =  commnloc;

            if (L1bcf(my_sym))
                fatal("common panic mode 2!");


            skiprefs(getwrd(InFile));
        }
    }
}

adj_ofst()
{
    register SYMDAT *curSym;
    OBFILES *curObj;

    maxbufsiz = 0;
    curObj = FirstMod;

    while (curObj)
    {
        curSym = curObj->symlist;

        /* A module counts as used when something references one of its
         * symbols; the mainline counted because the builtin symbols are
         * attached to the first module. In --lwlink mode lw_select chose
         * every module and the mainline needn't be first, so all count. */

        if (LwLink && curSym)
        {
            curSym->Type |= 0x100;
        }

        while (curSym)
        {
            if (curSym->Type & 0x100)
            {
                curObj->x19 |= 0x100;
                t_udat += curObj->UDat;
                t_udpd += curObj->UDpD;
                t_idat += curObj->IDat;
                t_idpd += curObj->IDpD;
                extramem += curObj->stack;
                t_code += curObj->codesiz;
                t_drefs += curObj->drefs;

                if (curObj->codesiz + curObj->IDat + curObj->IDpD <= maxbufsiz)
                {
                    break;
                }

                maxbufsiz = curObj->codesiz + curObj->IDat + curObj->IDpD;
                break;
            }
            else
            {
                curSym = curSym->sprev;
            }
        }       /* L0d17 */

        curObj = curObj->obn;
    }       /* L0d22 */

    curSym = commnsyms;

    while (curSym)
    {
        t_udat += curSym->Offset;
        curSym = curSym->sprev;
    }

    if ((curSym = lookup(symEtxt)))
    {
        curSym->Offset = t_code;
    }

    if ((curSym = lookup(symEDat)))
    {
        curSym->Offset = t_idat;
    }

    if ((curSym = lookup(symEnd)))
    {
        curSym->Offset = t_udat;
    }

    if ((curSym = lookup(symDpsiz)))
    {
        curSym->Offset =  t_udpd;
    }
}

adjsmofst()
{
    OBFILES *thisOB;
    char v1;
    int  v0;
    
    register SYMDAT *mySym;

    setbgns();
    thisOB = FirstMod;

    while (thisOB)
    {
        if ((thisOB->x19) & 0x100)
        {
            mySym = thisOB->symlist;

            while (mySym)
            {
                switch (mySym->Type & 7)
                {
                    case 0: /* L0db2 */
                        mySym->Offset += bgn_udat;
                        break;
                    case INIENT: /* L0db8 */
                        mySym->Offset += bgn_idat;
                        break;
                    case DIRENT: /* L0dbe */
                        mySym->Offset += bgn_udpd;
                        break;
                    case DIRENT | INIENT: /* L0dc4 */
                        mySym->Offset += bgn_idpd;
                        break;
                    case CODENT: /* L0dca */
                        mySym->Offset += bgn_codsiz;
                        break;
                    case CODENT | DIRENT: /* L0e07 */
                        break;
                    default:
                        fatal("unknown entry type in %s:%s",
                                thisOB->obname, thisOB->roffile);
                }

                mySym = mySym->sprev;
            }   /* while @ L0e09 */

            bgn_udat += thisOB->UDat;
            bgn_idat += thisOB->IDat;
            bgn_udpd += thisOB->UDpD;
            bgn_idpd += thisOB->IDpD;
            bgn_codsiz += thisOB->codesiz;
        }

        thisOB = thisOB->obn;   /* L0e48 */
    }       /* L0e4b */

    mySym = commnsyms;

    while (mySym)
    {
        v0 = mySym->Offset;
        bgn_udat = (mySym->Offset = bgn_udat) + v0;
        mySym = mySym->sprev;
    }

    if (mySym =  lookup(symBTxt))
    {
        mySym->Offset = 0;
    }

    setbgns();
}

setbgns()
{
    bgn_idpd = 0;
    bgn_udpd = bgn_idpd + t_idpd;
    bgn_idat = bgn_udpd + t_udpd;
    bgn_udat = bgn_idat + t_idat;
    /* Code starts after the header, the name and the edition byte; in
     * lwlink's layout it starts right after the 13-byte header */

    bgn_codsiz = LwLink ? 13 : namelen + 14;
}

static
addsym(oldObj, oNam, oTyp)
    OBFILES *oldObj; char *oNam; int oTyp;
{
    SYMDAT *newsm;

    newsm = newsym();
    newsm->sprev = oldObj->symlist;
    oldObj->symlist = newsm;
    strncpy(newsm->sname, oNam, SYMLEN);
    newsm->Type = oTyp;
    L1bcf(newsm);
}

static SYMDAT *
newsym()
{
    register SYMDAT *rgsym;

    if ((rgsym = curntsym))
    {
        curntsym = rgsym->sprev;
    }
    else
    {
        rgsym = (SYMDAT *)grab(sizeof(SYMDAT));
    }

    return rgsym;
}

goextref(ob)
    OBFILES * ob;
{
    fseek(InFile, (long)(ob->codesiz + ob->IDpD + ob->IDat), 1);
}

skiprefs(nrefs)
    UINT nrefs;
{
    fseek(InFile, (long)(nrefs * 3), 1);
}

static
void skipxtrn()
{
    char nam[SYMLEN + 1];
    int nrefs = getwrd(InFile);

    while (nrefs--)
    {
        getname(nam, SYMLEN);
        skiprefs(getwrd(InFile));
    }
}

int
ncodlocs (maxcount)
    register int maxcount;
{
    int   loccount;
    UINT  ofst;
    char  typ;

    loccount = 0;

    while (maxcount--)
    {
#ifdef _OS9
        if ( ! fread (&typ, 3, 1, InFile))
#else
        typ = (char)getc(InFile);
        ofst = getwrd(InFile);

        if ( (typ == EOF) || (ofst == EOF))
#endif
        {
            in_rderr();
        }

        if ((typ & (DIRLOC | CODLOC)) != CODLOC)
        {
            ++loccount;
        }
    }

    return loccount;
}

skipcomn (vrsn)
    UINT vrsn;
{
    char vname[SYMLEN + 1];
    int comcnt;

    if (vrsn > 0)  /* else L100f */
    {
        comcnt = getwrd (InFile);

        while (comcnt--)
        {
            getname (vname, SYMLEN);
            getwrd (InFile);
            skiprefs (getwrd (InFile));
        }
    }
}

SYMDAT *
lastobfile(curob)
    register SYMDAT *curob;
{
    SYMDAT *tmp_ob;

    tmp_ob = curob;

    while (curob)
    {
        curob = (tmp_ob = curob)->sprev;
    }

    return tmp_ob;
}
