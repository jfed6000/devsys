/*
    Modification history for inits.c:
        24-May-83       Added direct page initializers.
                        Make stringlen and datstring be DIRECT.
        14-Jun-83       Spelled initialise with a z.
        14-Apr-84 LAC   Conversion for UNIX.
*/

#include "cj.h"
static ilist();
static defobject();
static datdef();
static iskip();
extern direct int stringlen;
direct int datstring;


initialise(ptr,tsc,type)
register SYM ptr;
{
    register char *p;
    register CELL dimp;
    register int count;

    switch (tsc) {
        case TYPEDEF:
        case EXTERN:
        case EXTERND:
            return initerr();   /* can't initialize these dudes! */
        case DIRECT:
        case STATICD:
            vsect(DDP);     /* dp variable section */
            break;
        default:
            vsect(NDP);     /* non-dp variable section */
            break;
    }

    if (blklev == 0) {      /* outside a function */
        p = symname(ptr);
        if (tsc != STATIC && tsc != STATICD) nlabel(p,1);
        else {
            SP_STORAGE(ptr,EXTDEF);
            nlabel(p,0);        /* local to file */
        }
    } else {
        olbl(SG_OFFSET(ptr));  /* function local */
        nl();
    }

    if (type == (CHAR | ARRAY) || type == (UCHAR | ARRAY)) {
        datstring = 1;
        getsym();
        if (sym == STRING) {
            dimp = SG_DIMS(ptr);
            if ((count = car(dimp)) == 0) setcar(dimp,stringlen);
            else if ((count -= stringlen) >= 0) oz(count);
            else error(M_TOO_LONG);

            getsym();
            goto out;
        }
        datstring = POINTSIZE;
    } else {
        datstring = POINTSIZE;
        getsym();
    }

    if (type == STRUCT || isary(type))
        ilist(type,ptr,type == STRUCT ? SG_X(ptr) : SG_DIMS(ptr),0);
    else if (!ilist(type,ptr,SG_DIMS(ptr),1)) iskip();
out:
    endsect();
    datstring = 0;
}

#define INFINITY (-1)

static 
ilist(type,ptr,list,level)
register SYM ptr;
register CELL list;     /* dimensions, or for a struct its members */
{
    register CELL newlist;
    int flag,t,i;
    unsigned n;
    CELL e;

    if(level == 0) need(LBRACE);
    else if(sym == LBRACE) {
        flag = 1;
        getsym();
    } else flag = 0;

    if (isary(type)) {
        if ((n = car(list)) == 0) n = INFINITY;
        if ((t = decref(type)) == STRUCT)
            newlist = SG_X(ptr);
        else newlist = cdr(list);
        for (i = 0; sym != RBRACE; ) {
            if (ilist(t,ptr,newlist,level+1) == 0) return 0;
            if (++i < n && sym == COMMA) getsym();
            else break;
        }
        if (n == INFINITY) setcar(list,i);
        else if (i < n) zero(getsize(t,SG_SIZE(ptr),cdr(list))*(n-i));
out:
        if (level == 0 || flag) {
            if(sym == COMMA) getsym();
            if(sym == RBRACE) getsym();
            else {
                error(M_TOO_MANY_ELEMENTS);
                iskip();
            }
        }
        return 1;
    } else if (type == STRUCT) {
        if ((e = list) == 0) {
            error(M_UNIONS_NOT_ALLOWED);
            iskip();
            return 0;
        }
        while (sym != RBRACE) {
            register symrec *p = SYMP(ptr = car(e));

            list = (t = p->type) == STRUCT ? p->x : p->dims;
            if (ilist(t,ptr,list,level+1) == 0) return 0;
            if ((e = cdr(e)) && sym == COMMA) getsym();
            else break;
        }
        while (e) {
            register symrec *p = SYMP(car(e));

            zero(getsize(p->type,p->size,p->dims));
            e = cdr(e);
        }
        goto out;
    } else if (defobject(type)) {
        if (flag) need(RBRACE);
        return 1;
    }
    error(M_CONSTANT_EXPRESSION_REQUIRED);
    iskip();
    return 0;
}


zero(n)
{
    ot("rzb ");
    od(n);
    nl();
}

static defobject(type)
register int type;
{
    register expnode *p, *p2;
    int addon, done, t;
    extern direct int datflag;

    if ((p = optim(parsexp(LEV_2))) == NULL) return 0;

    addon = 0;
    done = 1;
    t = p->type;
    if (isptr(type))
        switch(t) {
            case LONG:
                cvt(p,INT);
                break;
            case ULONG:
                cvt(p,UNSIGN);
            case INT:
            case UNSIGN:
                break;
            default:
                if (!isptr(t)) goto err;
        }
    else if(isptr(t)) {
        if (!isint(type)) goto err;
    }
#ifdef  DOFLOATS
    else cvt(p,type == FLOAT ? DOUBLE : type);
#else
    else cvt(p,type);
#endif

    if (p->op == PLUS || p->op == MINUS) {
        if((p2 = p->right)->op != CONST) {
err:
            done = 0;
            goto finish;
        } else addon = (p->op == MINUS) ? -p2->val.num : p2->val.num;
        reltree(p2);
        p2 = p;
        p = p->left;
        release(p2);
    }

    if (p->op == AMPER)
        switch (SG_STORAGE((p2 = p->left)->val.sp)) {
            default:
                done = 0;
                break;
            case STATIC:
            case EXTERN:
            case EXTDEF:
            case DIRECT:
            case EXTERND:
                defword();
                datflag = 1;
                deref(NODE,p2,addon);
                datflag = 0;
                nl();
        }
    else switch (p->op) {
#ifdef  DOFLOATS
            case FCONST:
                if (type == FLOAT) D_TOF(p->val.fp, p->val.dp);
#endif
            case CONST:
                datdef(p->val.num,type);
                break;
            case LCONST:
                datdef(p->val.lp,type);
                break;
            case STRING:
                defword();
                label(p->val.num);
                break;
            default:
                done = 0;
                break;
        }
finish:
    reltree(p);
    return done;
}


static 
datdef(p,type)
register int *p;
{
    register int size;
	int flt = 0;

    switch (type) {
        case CHAR:
        case UCHAR:
            defbyte();
            od((int)p & 0xff);
            nl();
            return;
        case INT:
        case UNSIGN:    size = (INTSIZE/2);     break;
        default:        size = (POINTSIZE/2);   break;
        case LONG:
        case ULONG:     size = (LONGSIZE/2);    break;
#ifdef DOFLOATS
        case FLOAT:     size = (FLOATSIZE/2);   flt = 1; break;
        case DOUBLE:    size = (DOUBLESIZE/2);  flt = 1; break;
#endif
    }
    defcon(p,size,flt);
}


initerr()
{
    error(M_CANNOT_INITIALIZE);
    iskip();
}


static
iskip()
{
    for (;;)
        switch (sym) {
            case COMMA :
            case SEMICOL:
            case EOF:
                return;
            default:
                getsym();
        }
}

