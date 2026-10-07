/* **************************************************************** *
 * lwstruct.c - lwasm's "struct" (lwasm mode only)                  *
 *                                                                  *
 *   name     struct          a structure type: its fields follow,  *
 *   field    rmb   n         counted from 0 (rmd/rmw x2, rmq x4,   *
 *   inner    other           .blkb/.ds/.rs as rmb; another struct's *
 *            endstruct       name nests it). "ends" also ends it.  *
 *                                                                  *
 * endstruct defines name.field (the field's offset; nested ones as *
 * name.inner.field) and sizeof{name}. Neither the struct's name    *
 * nor its fields' labels become symbols of their own, and the      *
 * location counters don't move. Inside a struct lwasm obeys only   *
 * these operations and skips the rest; las also keeps directives   *
 * (conditionals, macros, equ) working there.                       *
 *                                                                  *
 *   var      name            outside a struct: one instance. It    *
 *                            reserves sizeof{name} bytes as "rmb"  *
 *                            would where it stands, and defines    *
 *                            var.field (var + offset, the same     *
 *                            kind of symbol as var) and            *
 *                            sizeof{var}.                          *
 *                                                                  *
 * lwasm puts an instance inside a module at the code counter and   *
 * writes a module whose size field doesn't match its bytes; las    *
 * reports an error there instead.                                  *
 *                                                                  *
 * The types are made in pass 1 and kept (getmem) for pass 2, which *
 * defines the same symbols again with the same values.             *
 * **************************************************************** */

#define NEED_CTYPE
#include "rma.h"

/* K&R C (DCC) has one namespace for all struct members: unique names */

typedef struct stfld
{
    struct stfld *sf_next;
    struct sttab *sf_sub;   /* a nested struct, or 0 */
    int sf_size;
    char sf_name[1];        /* "" for a field without a label */
} STFLD;

typedef struct sttab
{
    struct sttab *st_next;
    STFLD *st_fields, *st_last;
    int st_size;
    char st_name[1];
} STTAB;

static STTAB *Structs,      /* every struct type, newest first */
             *Cur,          /* the one being defined */
             *Inst;         /* st_inst: an instance to define */
static char InStruct;
static int Off;             /* the next field's offset (the listing) */
static SYMH InstLbl;        /* st_inst: the instance's label (its name
                             * is still in AltLbl at st_post) */

/* st_find() - The struct type called name, or 0 */

static STTAB *
#ifdef __STDC__
st_find (char *name)
#else
st_find (name)
     char *name;
#endif
{
    register STTAB *t;

    for (t = Structs; t && strcmp (t->st_name, name); t = t->st_next)
    {
    }

    return t;
}

/* st_kw() - Is op the keyword kw (lower case), in any case? */

static int
#ifdef __STDC__
st_kw (char *op, char *kw)
#else
st_kw (op, kw)
     char *op, *kw;
#endif
{
    while (*kw && (_tolower (*op) == *kw))
    {
        ++op;
        ++kw;
    }

    return ( ! *op && ! *kw);
}

/* ******************************************************************** *
 * st_sym() - Define name: a constant val, or (like != 0) the same kind *
 *            of symbol as like, at like's value + val. Pass 2 defines  *
 *            it again; in pass 1 a name already defined is an error.   *
 * ******************************************************************** */

static int
#ifdef __STDC__
st_sym (char *name, int val, SYMH like)
#else
st_sym (name, val, like)
     char *name;
     int val;
     SYMH like;
#endif
{
    register SYMH s;

    if ( ! (s = sym_find (name)))
    {
        s = sym_add (name);
    }
    else if (PASS1 && (sym_msk (s) & RELATIVE))
    {
        return e_report ("redefined name");
    }

    /* Keep LOC1BYT: it is the symbol table tree's "real right child"
     * flag (symtab.c). CODENT in the mask would export it. */

    if (like)
    {
        sym_settyp (s, sym_typ (like));
        sym_setmsk (s, (sym_msk (s) & LOC1BYT) |
                       (sym_msk (like) & ~(LOC1BYT | CODENT | NEGMASK)));
        sym_setval (s, sym_val (like) + val);
    }
    else
    {
        sym_settyp (s, CODENT | DIRENT);           /* as equ */
        sym_setmsk (s, (sym_msk (s) & LOC1BYT) | RELATIVE | DIRENT);
        sym_setval (s, val);
    }

    return 1;
}

/* st_define() - pref.field for each field of t (base + offset), and
 *               sizeof{pref} */

static int
#ifdef __STDC__
st_define (STTAB *t, char *pref, int base, SYMH like)
#else
st_define (t, pref, base, like)
     STTAB *t;
     char *pref;
     int base;
     SYMH like;
#endif
{
    char name[SYMLEN + 8];
    register STFLD *f;
    int off = 0;

    for (f = t->st_fields; f; f = f->sf_next)
    {
        if (f->sf_name[0])
        {
            if (strlen (pref) + strlen (f->sf_name) + 1 >= SYMLEN)
            {
                return e_report ("struct field name too long");
            }

            sprintf (name, "%s.%s", pref, f->sf_name);

            if ( ! st_sym (name, base + off, like) ||
                 (f->sf_sub && ! st_define (f->sf_sub, name, base + off, like)))
            {
                return 0;
            }
        }

        off += f->sf_size;
    }

    if (strlen (pref) + 8 >= SYMLEN)
    {
        return e_report ("struct name too long");
    }

    sprintf (name, "sizeof{%s}", pref);
    return st_sym (name, t->st_size, (SYMH) 0);
}

/* st_field() - Add a field (pass 1; pass 2 has them) */

static void
#ifdef __STDC__
st_field (char *name, int size, STTAB *sub)
#else
st_field (name, size, sub)
     char *name;
     int size;
     STTAB *sub;
#endif
{
    register STFLD *f;

    CodeAddrs = Off;            /* the listing shows the offset */
    Adrs_Ptr = &CodeAddrs;
    Off += size;

    if (Pass2)
    {
        return;
    }

    f = (STFLD *) getmem (sizeof (STFLD) + strlen (name));
    f->sf_next = 0;
    f->sf_sub = sub;
    f->sf_size = size;
    strcpy (f->sf_name, name);

    if (Cur->st_last)
    {
        Cur->st_last->sf_next = f;
    }
    else
    {
        Cur->st_fields = f;
    }

    Cur->st_last = f;
    Cur->st_size += size;
}

/* ******************************************************************** *
 * st_line() - AsmblLin, lwasm mode, before the label is defined: a     *
 *             struct, endstruct or field line. Returns -1 for any      *
 *             other line (assembled as usual), else AsmblLin's result. *
 * ******************************************************************** */

int
st_line (ZILCH)
{
    char op[SYMLEN], lbl[SYMLEN];
    register char *p = SrcChar;
    int n, mult = 0;
    STTAB *sub = 0;
    CODFORM *c;

    while (*p && notblank (*p))         /* past the label */
    {
        ++p;
    }

    while (isblank (*p))
    {
        ++p;
    }

    for (n = 0; *p && notblank (*p); ++p)
    {
        if (n < SYMLEN - 1)
        {
            op[n++] = *p;
        }
    }

    op[n] = '\0';

    if (st_kw (op, "struct"))
    {
        mult = -1;
    }
    else if (st_kw (op, "endstruct") || st_kw (op, "ends"))
    {
        mult = -2;
    }
    else if ( ! InStruct)
    {
        return -1;
    }
    else if (st_kw (op, "rmb") || st_kw (op, ".blkb") ||
             st_kw (op, ".ds") || st_kw (op, ".rs"))
    {
        mult = 1;
    }
    else if (st_kw (op, "rmd") || st_kw (op, "rmw"))
    {
        mult = 2;
    }
    else if (st_kw (op, "rmq"))
    {
        mult = 4;
    }
    else if ( ! (sub = st_find (op)))
    {
        /* Not a field. Directives (conditionals, macros, equ) work as
         * usual; anything else lwasm skips inside a struct. */

        if (McNamCmp (op) ||
            (((c = FindCmd (op, S_Nam, SNamEnd)) != 0) &&
             (strlen (c->ccmd) == strlen (op))))
        {
            return -1;
        }

        mult = -3;
    }

    if (ImgRec)
    {
        img_op (op);            /* not in the defs image (defsimg.c) */
    }

    /* Split the line as AsmblLin does, for the listing */

    lbl[0] = '\0';
    NumBytes = 0;
    Adrs_Ptr = 0;

    if (notblank (*SrcChar))
    {
        Label = SrcChar;

        if ( ! MovLbl (lbl))
        {
            while (*SrcChar && notblank (*SrcChar))
            {
                ++SrcChar;
            }
        }

        if (*SrcChar)
        {
            *(SrcChar++) = '\0';
        }
    }

    SkipSpac ();
    Operatr = SrcChar;

    while (*SrcChar && notblank (*SrcChar))
    {
        ++SrcChar;
    }

    if (*SrcChar)
    {
        *(SrcChar++) = '\0';
    }

    SkipSpac ();
    Oprand = SrcChar;

    if (mult == -1)                     /* struct */
    {
        if (InStruct)
        {
            return e_report ("struct inside a struct");
        }

        if ( ! lbl[0])
        {
            return e_report ("label missing");
        }

        if ( ! (Cur = st_find (lbl)))
        {
            Cur = (STTAB *) getmem (sizeof (STTAB) + strlen (lbl));
            Cur->st_fields = Cur->st_last = 0;
            Cur->st_size = 0;
            strcpy (Cur->st_name, lbl);
            Cur->st_next = Structs;
            Structs = Cur;
        }
        else if (PASS1)
        {
            return e_report ("struct defined twice");
        }

        InStruct = 1;
        Off = 0;
    }
    else if (mult == -2)                /* endstruct */
    {
        if ( ! InStruct)
        {
            return e_report ("endstruct without struct");
        }

        InStruct = 0;
        CodeAddrs = Off;
        Adrs_Ptr = &CodeAddrs;

        if ( ! st_define (Cur, Cur->st_name, 0, (SYMH) 0))
        {
            return 0;
        }
    }
    else if (sub)                       /* a nested struct */
    {
        st_field (lbl, sub->st_size, sub);
    }
    else if (mult > 0)                  /* rmb and the like */
    {
        if ( ! l1723 ())
        {
            return 0;
        }

        st_field (lbl, (int) NmbrInt * mult, (STTAB *) 0);
    }

    if (Oprand == SrcChar)
    {
        Oprand = 0;
    }
    else if (*SrcChar)
    {
        *(SrcChar++) = '\0';
        SkipSpac ();
    }

    return 1;
}

/* ******************************************************************** *
 * st_inst() - AsmblLin, lwasm mode, an operation nothing else knows:   *
 *             if it names a struct, set up an instance. Returns 1:     *
 *             assemble the line as "rmb" with the struct's size (l1723 *
 *             takes it from StSize), then call st_post(); 0: not a     *
 *             struct; -1: an error, reported.                          *
 * ******************************************************************** */

int
#ifdef __STDC__
st_inst (char *op)
#else
st_inst (op)
     char *op;
#endif
{
    if ( ! (Inst = st_find (op)))
    {
        return 0;
    }

    if (InModule)
    {
        e_report ("struct instance in a module: use rmb sizeof{...}");
        return -1;
    }

    if ( ! Label || ! CurntLabel)
    {
        e_report ("label missing");
        return -1;
    }

    InstLbl = CurntLabel;
    StSize = Inst->st_size;
    StPre = 1;
    return 1;
}

/* st_post() - After the instance's rmb: var.field and sizeof{var} */

int
st_post (ZILCH)
{
    return st_define (Inst, AltLbl, 0, InstLbl);
}
