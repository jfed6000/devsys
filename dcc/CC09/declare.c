/*
    Modification history for declare.c:
        23-May-83       Handle direct initializers.
        25-May-83       Make initstruct pointers be DIRECT.
                        Better use of register variables.
        14-Apr-84 LAC   Conversion for UNIX.
        11-Feb-86 LAC   fixed array subscript ordering bug.
*/

#include "cj.h"
#define  MPOINT 1

#ifdef USE_YREG
# define MAXREG	2
#else
# define MAXREG	1
#endif

/**********************************
 *                                *
 *  declarations and definitions  *
 *                                *
 **********************************/

static direct int reguse;
static direct struct initstruct *initlist,*initlast,*initfree;

#ifdef FUNCNAME
direct int fnline;
#endif


extdef()
{
    register SYM ptr;
    register symrec *p;
    SYM ptemp;
    CELL dimptr, tdp;
    int size,ssize,tsc,sclass,type,temp;
    CELL eptr;

    while (sym == RBRACE) {
        error(M_TOO_MANY_BRACKETS);
        getsym();
    }

    if (sym == LBRACE) {
        error(M_FUNCTION_HEADER_MISSING);
        block(0);
        clear(&labelist);
        getsym();
        return;
    }

    switch (sclass = setclass()) {
        case REG:
        case AUTO:
            storerr();
        case 0:
            sclass = EXTDEF;
    }

    if ((type = settype(&size,&dimptr,&eptr)) == UNDECL) type = INT;

#ifdef FUNCNAME
     fnline = symline;
#endif
    for ( ; ; ) {
        tdp = dimptr;
        temp = declarator(&ptemp,&tdp,type);
        ptr = ptemp;     /* decl ptr to register */

        if (ptr == 0) {
            if (temp != STRUCT && temp != UNION) identerr();
            goto next;
        }
/*
        if(isftn(temp))
            tsc = (sym==COMMA || sym==SEMICOL) ? EXTERN : EXTDEF;
        else tsc=sclass;
*/
        tsc = isftn(temp) ? ((sym == COMMA || sym == SEMICOL) ? EXTERN
                                                        : EXTDEF) : sclass;

        if (SG_TYPE(ptr) != UNDECL) {
            if (check_same(ptr,temp,eptr)) ;
            else if (tsc != EXTERN) {
                p = SYMP(ptr);
                if (p->storage != EXTERN && p->storage != EXTERND)
                    multidef();
                else {
                    p->storage = tsc;
                    p->dims = tdp;
                    p->x = eptr;
                    goto onlist;
                }
            }
        } else {
            p = SYMP(ptr);
            p->type = temp;
            p->blklev = 0;
            p->storage = tsc;
            p->x = eptr;
onlist:
            ssize = sizeup(ptr,tdp,size);
            if (!isftn(temp)) {
                if (sym == ASSIGN) initialise(ptr,tsc,temp);
                else if (ssize == 0 && tsc!=EXTERN) sizerr();
                else switch(tsc) {
                        case STATICD:
                        case STATIC:
                            extstat(ptr,ssize,(tsc == STATICD));
                            break;
                        case DIRECT:
                        case EXTDEF:
                            defglob(ptr,ssize,(tsc == DIRECT));
                            break;
                    }
                if (tsc == STATIC || tsc == STATICD)
                    SP_STORAGE(ptr,tsc == STATIC ? EXTDEF : DIRECT);
            }
        }
        if (isftn(temp)) {
            temp = decref(temp);
            if (isary(temp) || isftn(temp)
                            || temp == STRUCT || temp == UNION) {
                error(M_FUNCTION_TYPE_ERROR);
                SP_TYPE(ptr,INT | FUNCTION);
                temp = INT;
            }
            if (tsc == EXTERN) clear(&arglist);
            else {
                ftype = temp;
                newfunc(ptr,sclass);
                return;
            }
        }
next:
        if (sym != COMMA) break;
        getsym();
    }
    if (need(SEMICOL)) junk();
}


argdef()
{
    register SYM ptr;
    register symrec *p;
    SYM ptemp;
    CELL dimptr, tdp;
    int sclass,type,size,stemp,temp;
    CELL eptr;

    switch (sclass = setclass()) {
        default:    error(M_ARGUMENT_STORAGE);
        case 0:     sclass = AUTO;
        case REG:   break;
    }
    if ((type = settype(&size,&dimptr,&eptr)) == UNDECL) type = INT;

    for ( ; ; ) {
        tdp = dimptr;
        temp = declarator(&ptemp,&tdp,type);
        ptr = ptemp;        /* decl ptr to register */

        if (isftn(temp) || temp == STRUCT || temp == USTRUCT) {
            error(M_ARGUMENT_ERROR);
            goto next;
        } else if (isary(temp)) {
            temp = incref(decref(temp));
            tdp = tdp ? cdr(tdp) : 0;
        }
#ifdef DOFLOATS
        else if (temp == FLOAT) temp = DOUBLE;
#endif
        if (ptr == 0) {
            identerr();
            goto next;
        }
        stemp = chkreg(sclass,temp);

        switch ((p = SYMP(ptr))->storage) {
            case ARG:
                p->type = temp;
                p->storage = stemp;
                p->x = eptr;
                if (!sizeup(ptr,tdp,size)) sizerr();
                break;
            case AUTO:
            case REG:
                multidef();
                break;
            default:
                error(M_NOT_AN_ARGUMENT);
        }
next:
        if (sym == ASSIGN) initerr();
        if (sym != COMMA) break;
        getsym();
    }
    if (need(SEMICOL)) junk();
}


blkdef()
{
    register SYM ptr;
    register symrec *p;
    SYM ptemp;
    CELL dimptr, tdp;
    expnode *ep;
    int sclass,type,temp,size,stemp,ssize;
    CELL eptr;
    initnode *i;

    switch (sclass = setclass()) {
        case DIRECT:
            storerr();
            /* fall through to.. */
        case 0:
            sclass = AUTO;
    }
    if ((type = settype(&size,&dimptr,&eptr)) == UNDECL) type = INT;

    for ( ; ; ) {
        tdp = dimptr;
        temp = declarator(&ptemp,&tdp,type);
        ptr = ptemp;
        if (ptr == 0) {
            if (temp != STRUCT && temp != UNION) identerr();
            goto next;
        }
        if (isftn(temp) || sclass == EXTERN) {
            if ((p = SYMP(ptr))->type == UNDECL){
                p->type = temp;
                p->storage = EXTERN;
                p->blklev = 0;
                p->x = eptr;
                sizeup(ptr,tdp,size);
            } else check_same(ptr,temp,eptr);
            goto next;
        }
        stemp = chkreg(sclass,temp);

        if ((p = SYMP(ptr))->type != UNDECL) {
            if (p->blklev == blklev) {
                multidef();
                goto next;
            }
            pushdown(ptr);
            p = SYMP(ptr);
        }
        p->type = temp;
        p->storage = stemp;
        p->x = eptr;
        p->blklev = blklev;
        p->next = vlist;
        vlist = ptr;

        if (!(ssize = sizeup(ptr,tdp,size)) && sym != ASSIGN) sizerr();
        switch (stemp) {
            case AUTO:
                stlev -= ssize;
                SP_OFFSET(ptr,stlev);
                break;
            case STATICD:
            case STATIC:
                SP_OFFSET(ptr,getlabel());
        }

        if (sym == ASSIGN) {
            if (stemp == STATIC || stemp == STATICD)
                initialise(ptr,stemp,temp);
            else {
                getsym();
                if (isary(temp) || temp == STRUCT) initerr();
                else {
                    if (ep = parsexp(LEV_2)) {
                        if (initfree) {
                            initfree = (i = initfree)->initnext;
                            i->initnext = NULL;
                        } else i = (initnode *) grab(sizeof(initnode));
                        i->initp = ep;
                        i->initname = ptr;
                        if (initlist) initlast->initnext = i;
                        else initlist = i;
                        initlast = i;
                    } else initerr();
                }
            }
        } else switch(stemp) {
                case STATIC:
                case STATICD:
                    locstat(SG_OFFSET(ptr),ssize,(stemp == STATICD));
            }
next:
        if (sym != COMMA) break;
        getsym();
    }
    need(SEMICOL);
}


check_same(ptr,type,eptr)
register SYM ptr;
register int type;
register CELL eptr;
{
     register symrec *p = SYMP(ptr);

     if (p->type != type || (type == STRUCT && p->x != eptr)) {
          error(M_DECLARATION_MISMATCH);
          return TRUE;
     }
     return FALSE;
}


chkreg(sclass,type)
{
    /* if the user wants a register variable
     * and it is allowed allocate one.
     */
    if (sclass == REG) {
        /* user wants a register variable */
        if (reguse < MAXREG)          /* still registers free */
            switch (type) {
                /* types allowed: int, unsigned and pointer */
                default:
                    if (!isptr(type)) break;
                    /* else fall through to... */
                case INT:
                case UNSIGN:
                    /* he can have one! */
#ifdef USE_YREG
                    return ++reguse == 1 ? UREG : YREG;
#else
                    ++reguse; return UREG;
#endif
            }
        /* otherwise make it auto - this is only called
         * inside a function or its argument declaration
         * area.
         */
        return AUTO;
    }
    /* he did not want one */
    return sclass;
}


newfunc(fptr,sclass)
SYM fptr;
{
    register SYM p1;
    register symrec *p;
    SYM p2;
    int offset,stemp,paramlev;
    char *cp;
#ifdef PROF
    int prlab;
#endif

    blklev = 1;
    callflag = reguse = stlev = maxpush = sp = 0;

    while (sym != LBRACE) argdef();

    cp = symname(fptr);

#ifdef PROF
    /* make the profiler function name string */
    if (pflag) profname(cp,prlab=getlabel());
#endif

    paramlev = 0;

    p1 = arglist;
#ifdef REGPARMS /* FIXME: see if we can do this for static funcs */
    if (p1) {
# ifdef DEBUG
        if (dflag) {printf("newfunc node:\n"); pnode(p1);}
# endif
        switch (SG_TYPE(p1)) {
            case LONG:
            case ULONG:
# ifdef DOFLOATS
            case FLOAT:
            case DOUBLE:
# endif
                /* generate register save & stack checking */
# ifdef PROF
                startfunc(cp,(sclass != STATIC),0,prlab);
# else
                startfunc(cp,(sclass != STATIC),0);
# endif
                break;
            default:
                switch (SG_STORAGE(p1)) {
                    case UREG:
#ifdef USE_YREG
                    case YREG:
#endif
# ifdef PROF
                        startfunc(cp,(sclass != STATIC),SG_STORAGE(p1),prlab);
# else
                        startfunc(cp,(sclass != STATIC),SG_STORAGE(p1));
# endif
                        break;
                    case ARG:
                        SP_STORAGE(p1,AUTO);
                    case AUTO:
# ifdef PROF
                        startfunc(cp,(sclass != STATIC),DREG,prlab);
# else
                        startfunc(cp,(sclass != STATIC),DREG);
# endif
                        SP_OFFSET(p1,(paramlev -= 2));
                        if (SG_TYPE(p1) == CHAR || SG_TYPE(p1) == UCHAR)
                            SP_OFFSET(p1,SG_OFFSET(p1) + 1);
                        break;
                    default:
                        error(M_ARGUMENT_TYPE_IN_NEWFUNC);
                }
                p1 = SG_NEXT(p1);
        }
    } else {
# ifdef PROF
        startfunc(cp,(sclass != STATIC),prlab);
# else
        startfunc(cp,(sclass != STATIC));
# endif
    }
#else   /* !REGPARMS */
# ifdef PROF
    startfunc(cp,(sclass != STATIC),prlab);
# else
    startfunc(cp,(sclass != STATIC));
# endif
#endif

    stlev = sp = paramlev;
    offset = 2 + (MAXREG * 2);
    while (p1) {
        int size;

        p = SYMP(p1);
        p->offset = offset;
        switch (p->type) {
            case LONG:
            case ULONG:
                size = LONGSIZE;
                break;
#ifdef  DOFLOATS
            case FLOAT:
                size = FLOATSIZE;
                break;
            case DOUBLE:
                size = DOUBLESIZE;
                break;
#endif
            case CHAR:
            case UCHAR:
                ++p->offset;
            default:
                size = INTSIZE;
        }
        switch (stemp = p->storage) {
            case UREG:
#ifdef USE_YREG
            case YREG:
#endif
                gen(stemp,offset-sp);
                break;
            case ARG:
                p->storage = AUTO;
        }
        offset += size;
        p1 = SG_NEXT(p1);
    }
    p2 = arglist;
    arglist = 0;

#ifdef REGCONTS
    clrconts();         /* clear register contents */
#endif
    block(-paramlev);

    clear(&p2);
    clear(&labelist);
    if (lastst != RETURN) gen(RETURN,0,0);
    lastst = 0;

    /* generate stack reservation */
    endfunc();

    blklev = 0;
    if (sym == EOF) error(M_FUNCTION_UNFINISHED);
    getsym();
}


block(stkadj)
int stkadj;
{
    register expnode *p;
    struct initstruct *i;
    register SYM varlist;
    int savlev;

    varlist = vlist;
    vlist = 0;
    getsym();
    ++blklev;
    savlev = stlev + stkadj;

    while (issclass() || istype()) blkdef();
    if (maxpush > stlev) maxpush = stlev;
    sp = modstk(stlev);

    while (initlist) {
        i = initlist;
        p = i->initp;
        p = newnode(NAME,0,0,i->initname,p->lno,p->pnt);
        p = newnode(ASSIGN,p,i->initp,0,p->lno,p->pnt);
        reltree(tranexp(optim(p)));
        i = i->initnext;
        initlist->initnext = initfree;
        initfree = initlist;
        initlist = i;
    }

    while (sym != RBRACE && sym != EOF) statement();

#ifdef REGCONTS
    if (vlist) clrconts();
#endif
     clear(&vlist);
     vlist = varlist;
     --blklev;
     stlev = savlev;
/*
     if(lastst!=RETURN)
          sp=modstk(savlev);
     else sp=savlev;
*/
     sp = (lastst != RETURN) ? modstk(savlev) : savlev;
}


declist(list)
register SYM *list;
{
    register SYM ptr,last;
    register symrec *p;

    *list = 0;
    getsym();

    for (;;) {
        if (sym == RPAREN) break;
        if (sym == NAME) {
            ptr = symval;
            if ((p = SYMP(ptr))->storage == ARG) error(M_NAMED_TWICE);
            else if (p->type != UNDECL) pushdown(ptr);
            if (*list) SP_NEXT(last,ptr);
            else *list = ptr;
            p = SYMP(ptr);
            p->type = INT;
            p->storage = ARG;
            p->blklev = 1;
            p->size = 2;
            p->next = 0;
            last = ptr;
            getsym();
        } else identerr();

        if (sym != COMMA) break;
        getsym();
    }
    need(RPAREN);
}


setclass()
{
     int class;
     if(issclass()) {
          class=symval;
          getsym();
          if(sym==KEYWORD && symval==DIRECT) {
               switch(class) {
                    case STATIC: class=STATICD; break;
                    case EXTERN: class=EXTERND; break;
               }
               getsym();
          }
          return class;
     }
     return 0;
}

int
modifier (size)
int *size;
{
    int is_long=0, isshort=0, issign=0, isunsign=0;
    int type = UNDECL, tsize = 0;

    while (sym == KEYWORD) {
        switch (symval) {
        default:    /* some keyword other than what we're interested in... */
            goto done;
        /* type modifiers (nonterminal) */
        case SIGN:
            if (issign || isunsign) goto err;
            issign=1;
            break;
        case UNSIGN:
            if (issign || isunsign) goto err;
            isunsign=1;
            break;
        case LONG:
            if (is_long || isshort) goto err;
            is_long=1;
            break;
        case SHORT:
            if (is_long || isshort) goto err;
            isshort=1;
            break;
        /* final types (terminal) */
        case CHAR:
            if (is_long || isshort) goto err;
            tsize = 1;
            type = isunsign ? UCHAR : CHAR;
            getsym();
            goto done;
        case INT:
            if (is_long) {
                tsize = LONGSIZE;
                type = isunsign ? ULONG : LONG;
            } else {
                tsize = INTSIZE;
                type = isunsign ? UNSIGN : INT;
            }
            getsym();
            goto done;
#ifdef DOFLOATS
        case DOUBLE:
            if (isshort || issign || isunsign) goto err;
            tsize = DOUBLESIZE;
            type = DOUBLE;
            getsym();
            goto done;
        case FLOAT:
            if (isshort || issign || isunsign) goto err;
            if (is_long) {
                tsize = DOUBLESIZE;
                type = DOUBLE;
            } else {
                tsize = FLOATSIZE;
                type = FLOAT;
            }
            getsym();
            goto done;
#endif
        }
        getsym();
    }
done:
    if (!type) {    /* allow only modifiers, default "int" size */
        if (is_long) {
            tsize = LONGSIZE;
            type = isunsign ? ULONG : LONG;
        } else {
            tsize = INTSIZE;
            type = isunsign ? UNSIGN : INT;
        }
    }
    *size = tsize;
    return type;
err:
    error (M_BAD_TYPE);
    return 0;
}

settype(size,dimptr,ellist)
int *size;
CELL *dimptr;
CELL *ellist;
{
    register SYM ptr, tagptr;
    register symrec *p;
    CELL dptr;
    int offset,msize,mtype,savflg,dtype,s;
    CELL eptr,elast,elocal;
    int type = UNDECL, tsize = INTSIZE;

    *ellist = 0;
    *dimptr = 0;        /* devsys: the early returns for "struct tag x[n]"
                         * left the caller's dimension list unset (a crash
                         * in declarator() on XLisp/xlkmap.c) */
    if (sym==KEYWORD) {
        switch (type=symval) {
            case LONG:
            case SHORT:
            case SIGN:
            case UNSIGN:
                type = modifier(&tsize);
                break;
            case CHAR:
                tsize=1;
            case INT:
                getsym();
                break;
#ifdef DOFLOATS
            case DOUBLE:
                tsize=DOUBLESIZE;
                getsym();
                break;
            case FLOAT:
                tsize=FLOATSIZE;
                getsym();
#endif
                break;
default:
                type = UNDECL;
                break;
            case UNION:
            case STRUCT:
                tsize=offset=0;
                ++mosflg;	/* go into special "struct" lexer mode */
                tagptr=0;
                getsym();
                --mosflg;	/* and get back out */
                if (sym==NAME) {
                    p = SYMP(tagptr = symval);
                    if (p->type==UNDECL) {
                        p->type = USTRUCT;
                        p->storage = STRTAG;
                    } else if (p->storage!=STRTAG)
                        error(M_NAME_CLASH);
                    getsym();
                    if (sym != LBRACE) {
                        if((p = SYMP(tagptr))->type==STRUCT) {
                            *size=p->size;
                            *ellist=p->x;
                            return STRUCT;
                        }
                        /* incomplete: the size field holds the tag */
                        *size = tagptr;
                        return USTRUCT;
                    } else if(SG_TYPE(tagptr)==STRUCT) multidef();
                }
                if (sym!=LBRACE) error(M_STRUCT_SYNTAX);
                else {
                    ++mosflg;	/* go into special "struct" lexer mode */
                    do {
                        savflg=mosflg;
                        mosflg=0;
                        getsym();
                        mosflg=savflg;
                        if (sym==RBRACE) break;
                        mtype=settype(&msize,&dptr,&elocal);
                        while (1) {
                            SYM ptemp;
                            CELL tdptr;

                            tdptr = dptr;
                            if (sym == SEMICOL) break;
                            ++blklev;
                            dtype = declarator(&ptemp,&tdptr,mtype);
                            ptr = ptemp;     /* decl ptr to register */
                            --blklev;
                            if (ptr == 0) {
                                identerr();
                                goto next;
                            }
                            if ((p = SYMP(ptr))->type != UNDECL) {
                                if (p->blklev == blklev) {
                                    if (p->type != dtype
                                            || p->storage != MOS
                                            || p->offset != offset)
                                        error(M_STRUCT_MEMBER_MISMATCH);
                                } else pushdown(ptr);
                            }
                            if (dtype == USTRUCT)
                                    error(M_UNDEFINED_STRUCTURE);
                            p = SYMP(ptr);
                            p->type = dtype;
                            p->storage = MOS;
                            p->offset = offset;
                            p->x = elocal;
                            if (s = sizeup(ptr,tdptr,msize)) {
                                if (type == STRUCT) tsize = (offset += s);
                                else tsize = s > tsize ? s : tsize;
                            } else sizerr();
                            p = SYMP(ptr);
                            p->blklev = blklev;
                            p->next = vlist;
                            vlist = ptr;

                            if (type == STRUCT) {
                                eptr = cons(ptr,0);
                                if (*ellist) setcdr(elast,eptr);
                                else *ellist = eptr;
                                elast = eptr;
                            }
next:
                            if (sym != COMMA) break;
                            getsym();
                        }
                    } while(sym == SEMICOL);
                    --mosflg;	/* get back out of "struct mode" */
                    if (tagptr) {
                        p = SYMP(tagptr);
                        p->size = tsize;
                        p->type = STRUCT;
                        p->x = *ellist;
                    }
                    need(RBRACE);
                    break;
                }
        }
    } else if (sym == NAME) {
        if ((p = SYMP(symval))->storage == TYPEDEF) {
            *size = p->size;
            *dimptr = p->dims;
            *ellist = p->x;
            type = p->type;
            getsym();
            return type;
        }
    }
    *size = tsize;
    *dimptr = 0;
    return type;
}


declarator(ptr,dptr,bastype)
SYM *ptr;
CELL *dptr;
{
    register CELL tempdim, p, p1;
    int dtype, savmos, count;
    CELL dummy;
    SYM args;

    *ptr = 0;
    dtype = 0;

    while (sym == STAR) {
        dtype = incref(dtype);
        getsym();
    }
    if (sym == NAME) {
        *ptr = symval;
        getsym();
    } else if (sym == LPAREN) {
        getsym();
        ++blklev;
        bastype = declarator(ptr,dptr,bastype);
        --blklev;
        need(RPAREN);
    }

    if (sym == LPAREN) {
        dtype = (dtype<<2) + FUNCTION;
        if (blklev) {
            declist(&args);
            clear(&args);
        } else declist(&arglist);
    } else {
        tempdim = p1 = 0;
        savmos = mosflg;
        count = mosflg = 0;
        while (sym == LBRACK) {
            dtype = (dtype << 2) + ARRAY;
            getsym();
            p = cons(0,0);
            if (count == 0 && sym == RBRACK) setcar(p,0);
            else setcar(p,constexp(0));

            if (p1) setcdr(p1,p);
            else tempdim = p;
            p1 = p;
            need(RBRACK);
            ++count;
        }
        mosflg = savmos;
        if (tempdim) {
            if (dummy = *dptr) {
                while (cdr(dummy)) dummy = cdr(dummy);
                setcdr(dummy,tempdim);
            } else {
                setcdr(p,0);
                *dptr = tempdim;
            }
        }
    }

    return shiftin(bastype,dtype);
}


shiftin (a,b)
{
    int temp;

    temp = a;
    while (temp & XTYPE) {
        temp >>= 2;
        b <<= 2;
    }
    return (a + b);
}


sizeup(ptr,dimptr,size)
register SYM ptr;
register CELL dimptr;
{
    register int n,temp;
    register symrec *p;

    p = SYMP(ptr);
    n = basesize(temp = p->type,size);
    p->size = n ? n : size;
    p->dims = dimptr;

    return getsize(temp,n,dimptr);
}


/* the size of one of t's basic type; 0 for an incomplete struct */
basesize(t,size)
{
    register int n;

    n = 0;
    switch (btype(t)) {
        case CHAR:
        case UCHAR:     n = 1; break;
        case INT:
        case UNSIGN:    n = INTSIZE; break;
        case LONG:
        case ULONG:     n = LONGSIZE; break;
#ifdef  DOFLOATS
        case FLOAT:     n = FLOATSIZE; break;
        case DOUBLE:    n = DOUBLESIZE; break;
#endif
        case UNION:
        case STRUCT:    n = size; break;
        case USTRUCT:   n = 0; break;
    }
    return n;
}


getsize(t,size,dptr)
register CELL dptr;
{
    int n;

    if (isptr(t) || isftn(t)) return POINTSIZE;

    if (isary(t)) {
        n = 1;
        do {
            n *= car(dptr);
            dptr = cdr(dptr);
        } while (isary(t = decref(t)));

        return n * (isptr(t) ? POINTSIZE : size);
    }
    return size;
}


clear(list)
SYM *list;
{
    register SYM this, next;
    register symrec *p;
    char err[60];

    this = *list;

    while (this) {
        p = SYMP(this);
        next = p->next;
        switch (p->storage) {
#ifdef USE_YREG
            case YREG:
#endif
            case UREG: --reguse;
        }
        if (p->type == LABEL) {
/*
            if(!(this[ELEMS] & GONETO)) {
                strncpy(err,this+(SNAME>>1),8);
                strcat(err," : label unused");
                error(err);
            }
*/
            if(!(p->x & DEFINED)) {
                error(strncat(strcpy(err,msg(M_LABEL_UNDEFINED)),
                                symname(this),NAMESIZE));
            }
        }

        symfree(this);
        this = next;
    }

    *list = 0;
}


sizerr()
{
    error(M_CANNOT_EVALUATE_SIZE);
}


identerr()
{
    error(M_IDENTIFIER_MISSING);
}

