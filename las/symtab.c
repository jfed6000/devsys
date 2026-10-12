/* **************************************************************** *
 * symtab.c - the symbol table                                      *
 *                                                                  *
 * Everything that knows how symbols are stored lives here. The     *
 * rest of las finds, adds and visits symbols only through:         *
 *                                                                  *
 *      sym_find (name)     the symbol, or 0 if not defined         *
 *      sym_add (name)      a new, zeroed symbol (name must be new) *
 *      sym_anon ()         a zeroed symbol outside the table       *
 *      sym_walk (fn)       call fn for every symbol, in table order*
 *                                                                  *
 * and reads and changes them only through the accessors below      *
 * (sym_typ, sym_setval, ...). A symbol is a SYMH handle, a         *
 * reference record a REFH: callers never hold a pointer into the   *
 * table, so the storage can move to banked memory (phase 8).       *
 *                                                                  *
 * Storage (inherited from Microware RMA): a name's character sum   *
 * picks one of 64 buckets; each bucket is a threaded binary tree.  *
 * A node's 'right' is either its right child or, when it has none, *
 * a thread up to the next node in order. LOC1BYT in locmsk marks   *
 * a real right child. Note that the listing's symbol table prints  *
 * locmsk, so that bit shows there.                                 *
 *                                                                  *
 * Each symbol is allocated at its name's real length: symblnam is  *
 * the struct's last member, declared [1], and sym_add() asks for   *
 * sizeof (struct symblstr) + strlen (name) bytes.                  *
 *                                                                  *
 * sym_walk's order (bucket 0..63, alphabetical within a bucket)    *
 * decides the order of the listing's symbol table and of the ROF's *
 * global and external lists, so it must not change by accident.   *
 * **************************************************************** */

#include "rma.h"

#define NBUCKETS 64

struct symblstr
{
    char smbltyp,               /* Symbol type flag                     */
         locmsk;
    SVAL s_ofst;                /* Offset into code or value for sbl def*/
    BANKH left,                 /* Next entry, alphabet < this          */
          right;                /* Next entry, alphabet > this          */
    BANKH reftbl;                /* Last ref in the chained list of all
                                 * refs to this symbol                  */
    char symblnam[1];           /* Symbol Name. Must stay the last
                                 * member: sym_add() allocates each
                                 * symbol at its name's real length,
                                 * so the array runs past [0].        */
};

/* One record for each reference in the program */

struct ref_xtrnal
{
    char RfTyp;
    int r_offset;       /* Offset in the module, unless "set" or "equ":
                         * then it's the SYMH of the symbol referred to */
    BANKH NxtRef;       /* Actually, previous entry */
};

/* Handle to pointer. Entries live in banked blocks (bank.c), and on
 * the 6809 a pointer is only good until another block is mapped in. So
 * pointers are only used inside this file, and no statement here may
 * use two entries: "SP (a)->x = SP (b)->y" could map b's block in
 * after taking a's pointer. Copy through a local instead. */

#define SP(h) ((struct symblstr *) bank_ptr (h))
#define RP(h) ((struct ref_xtrnal *) bank_ptr (h))

/* With PragNoCase (lwasm's nosymbolcase) names are hashed and compared
 * ignoring case; the table keeps the spelling first seen. */

#define FOLD(c) (PragNoCase ? _tolower (c) : (c))

/* ******************************************************************** *
 * symkey() - In lwasm mode a name containing '@' or '?' is local to    *
 *            its block (LocalCtx), so the table stores it as           *
 *            "name~block": l@ in block 3 and l@ in block 7 are two     *
 *            different symbols. Other names are used as they are.      *
 * ******************************************************************** */

static char *
#ifdef __STDC__
symkey (char *name)
#else
symkey (name)
     char *name;
#endif
{
    static char key[SYMLEN + 8];
    register char *p;

    if ( ! LwasmMode)
    {
        return name;
    }

    for (p = name; *p; ++p)
    {
        if ((*p == '@') || (*p == '?'))
        {
            /* not "%.*s": CoCoC's printf takes no '*' precision */
            strncpy (key, name, SYMLEN - 1);
            key[SYMLEN - 1] = '\0';
            sprintf (key + strlen (key), "~%d", LocalCtx);
            return key;
        }
    }

    return name;
}

static int
#ifdef __STDC__
namecmp (char *a, char *b)
#else
namecmp (a, b)
     register char *a, *b;
#endif
{
    if ( ! PragNoCase)
    {
        return strcmp (a, b);
    }

    while (*a && (_tolower (*a) == _tolower (*b)))
    {
        ++a;
        ++b;
    }

    return _tolower (*a) - _tolower (*b);
}

static SYMH Buckets[NBUCKETS];

#ifdef SYMSTATS
/* Measurement build only (make -C build/stats): see sym_stats() */
static long StatSyms, StatNameChars, StatBytes;
#endif

static SYMH *
#ifdef __STDC__
bucket (char *name)
#else
bucket (name)
     char *name;
#endif
{
    register char *p = name;
    int sum = 0;

    do
    {
        sum += FOLD (*p);
    } while (*(++p) != '\0');

    return &Buckets[sum & (NBUCKETS - 1)];
}

/* ******************************************************************** *
 * search() - Look for name in its bucket's tree.                       *
 * Returns: strcmp() of name against the last node visited: 0 if found  *
 *          (*node is the symbol), otherwise the side the new node      *
 *          belongs on, below *node. Empty tree: returns 1, *node = 0.   *
 * ******************************************************************** */

static int
#ifdef __STDC__
search (char *name, SYMH *node)
#else
search (name, node)
     char *name;
     SYMH *node;
#endif
{
    int cmp = 1;
    register SYMH n;

    if ( (n = *node = *bucket (name)) )
    {
        do
        {
            cmp = namecmp (name, SP (*node = n)->symblnam);

            if (cmp < 0)
            {
                n = SP (n)->left;
            }
            else
            {
                if ((cmp == 0) || ! (SP (n)->locmsk & LOC1BYT))
                {
                    break;
                }

                n = SP (n)->right;
            }
        } while (n);
    }

    return cmp;
}

SYMH
#ifdef __STDC__
sym_find (char *name)
#else
sym_find (name)
     char *name;
#endif
{
    SYMH node;

    name = symkey (name);
    return search (name, &node) ? 0 : node;
}

/* ******************************************************************** *
 * sym_add() - Create a symbol. The caller must know name isn't in the  *
 *             table yet (sym_find returned 0).                         *
 * ******************************************************************** */

static SYMH
#ifdef __STDC__
newsym (char *name)
#else
newsym (name)
     char *name;
#endif
{
    /* sizeof already includes symblnam[1], room for the terminating 0 */
    SYMH new = bank_alloc (sizeof (struct symblstr) + strlen (name));

    strcpy (SP (new)->symblnam, name);
    return new;
}

/* ******************************************************************** *
 * sym_anon() - A zeroed symbol with no name, not in the table. The     *
 *              local reference lists (LRefs_Code ...) are kept in such *
 *              symbols.                                                *
 * ******************************************************************** */

SYMH
sym_anon (ZILCH)
{
    return newsym ("");
}

SYMH
#ifdef __STDC__
sym_add (char *name)
#else
sym_add (name)
     char *name;
#endif
{
    register SYMH new;
    SYMH parent;
    int cmp;

    name = symkey (name);
    cmp = search (name, &parent);
    new = newsym (name);
#ifdef SYMSTATS
    ++StatSyms;
    StatNameChars += strlen (name);
    StatBytes += sizeof (struct symblstr) + strlen (name);
#endif

    if (parent)
    {
        if (cmp < 0)
        {
            SP (new)->right = parent;   /* thread up to the parent */
            SP (parent)->left = new;
        }
        else
        {
            SYMH thread = SP (parent)->right;

            SP (new)->right = thread;   /* inherit the parent's thread */
            SP (parent)->right = new;
            SP (parent)->locmsk |= LOC1BYT;
        }
    }
    else
    {
        *bucket (name) = new;
        SP (new)->right = 0;
    }

    return new;
}

/* ******************************************************************** *
 * sym_after() - The symbol created after h, if nothing else was        *
 *               allocated between them (defsimg.c)                     *
 * ******************************************************************** */

SYMH
#ifdef __STDC__
sym_after (SYMH h)
#else
sym_after (h)
     SYMH h;
#endif
{
    return bank_after (h, sizeof (struct symblstr) + strlen (SP (h)->symblnam));
}

/* ******************************************************************** *
 * next() - The node after this one in alphabetical order, or 0.        *
 * ******************************************************************** */

static SYMH
#ifdef __STDC__
next (SYMH node)
#else
next (node)
     SYMH node;
#endif
{
    register SYMH n = SP (node)->right;

    if (SP (node)->locmsk & LOC1BYT)
    {
        while (SP (n)->left)
        {
            n = SP (n)->left;
        }
    }

    return n;
}

void
#ifdef __STDC__
sym_walk (void (*fn) (SYMH))
#else
sym_walk (fn)
     void (*fn) ();
#endif
{
    register SYMH n;
    int b;

    for (b = 0; b < NBUCKETS; ++b)
    {
        if ( (n = Buckets[b]) )
        {
            while (SP (n)->left)
            {
                n = SP (n)->left;
            }

            do
            {
                (*fn) (n);
            } while ( (n = next (n)) );
        }
    }
}

/* ******************************************************************** *
 * Accessors. sym_typ and sym_msk return the stored char as an int,     *
 * sign included, exactly as reading the field did.                     *
 * ******************************************************************** */

int
#ifdef __STDC__
sym_typ (SYMH h)
#else
sym_typ (h)
     SYMH h;
#endif
{
    return SP (h)->smbltyp;
}

int
#ifdef __STDC__
sym_msk (SYMH h)
#else
sym_msk (h)
     SYMH h;
#endif
{
    return SP (h)->locmsk;
}

SVAL
#ifdef __STDC__
sym_val (SYMH h)
#else
sym_val (h)
     SYMH h;
#endif
{
    return SP (h)->s_ofst;
}

REFH
#ifdef __STDC__
sym_refs (SYMH h)
#else
sym_refs (h)
     SYMH h;
#endif
{
    return SP (h)->reftbl;
}

void
#ifdef __STDC__
sym_settyp (SYMH h, int v)
#else
sym_settyp (h, v)
     SYMH h;
     int v;
#endif
{
    register struct symblstr *s = SP (h);

    if (ImgOn && (s->smbltyp != (char) v))
    {
        img_touch (h);          /* defsimg.c watches the defs' symbols */
        s = SP (h);
    }

    s->smbltyp = v;
}

void
#ifdef __STDC__
sym_setmsk (SYMH h, int v)
#else
sym_setmsk (h, v)
     SYMH h;
     int v;
#endif
{
    register struct symblstr *s = SP (h);

    if (ImgOn && (s->locmsk != (char) v))
    {
        img_touch (h);
        s = SP (h);
    }

    s->locmsk = v;
}

/* Called through the sym_setval() macro (proto.h), which casts the
 * value: an int passed where the 6809 build expects a long would be
 * read wrongly, with no prototype to convert it */

void
#ifdef __STDC__
sym_setv (SYMH h, SVAL v)
#else
sym_setv (h, v)
     SYMH h;
     SVAL v;
#endif
{
    register struct symblstr *s = SP (h);

    if (ImgOn && (s->s_ofst != v))
    {
        img_touch (h);
        s = SP (h);
    }

    s->s_ofst = v;
}

void
#ifdef __STDC__
sym_setrefs (SYMH h, REFH r)
#else
sym_setrefs (h, r)
     SYMH h;
     REFH r;
#endif
{
    register struct symblstr *s = SP (h);

    if (ImgOn && (s->reftbl != r))
    {
        img_touch (h);
        s = SP (h);
    }

    s->reftbl = r;
}

/* sym_name() - The symbol's name, copied: good until the next call. */

char *
#ifdef __STDC__
sym_name (SYMH h)
#else
sym_name (h)
     SYMH h;
#endif
{
    static char name[SYMLEN + 8];

    strcpy (name, SP (h)->symblnam);
    return name;
}

/* ******************************************************************** *
 * Reference records. ref_new() makes one whose NxtRef is next.         *
 * ******************************************************************** */

REFH
#ifdef __STDC__
ref_new (int typ, int offset, REFH next)
#else
ref_new (typ, offset, next)
     int typ;
     int offset;
     REFH next;
#endif
{
    REFH r;

    if (ImgRec)
    {
        img_bad ();             /* the defs can't be recorded with these */
    }

    r = bank_alloc (sizeof (struct ref_xtrnal));

    RP (r)->RfTyp = typ;
    RP (r)->r_offset = offset;
    RP (r)->NxtRef = next;
    return r;
}

int
#ifdef __STDC__
ref_typ (REFH r)
#else
ref_typ (r)
     REFH r;
#endif
{
    return RP (r)->RfTyp;
}

int
#ifdef __STDC__
ref_off (REFH r)
#else
ref_off (r)
     REFH r;
#endif
{
    return RP (r)->r_offset;
}

REFH
#ifdef __STDC__
ref_next (REFH r)
#else
ref_next (r)
     REFH r;
#endif
{
    return RP (r)->NxtRef;
}

void
#ifdef __STDC__
ref_setnext (REFH r, REFH next)
#else
ref_setnext (r, next)
     REFH r;
     REFH next;
#endif
{
    RP (r)->NxtRef = next;
}

#ifdef SYMSTATS
/* ******************************************************************** *
 * sym_stats() - Print symbol table memory use to stderr. Only in the   *
 *               measurement build. On the 6809 a symbol's fixed part   *
 *               is 12 bytes (2 chars, a long, 3 pointers) plus the     *
 *               name and its terminating 0; before compact entries it  *
 *               was 12 + 64 = 76 bytes whatever the name's length.     *
 * ******************************************************************** */

void
#ifdef __STDC__
sym_stats (long refs)
#else
sym_stats (refs)
     long refs;
#endif
{
    fprintf (stderr,
        "symstats: symbols %ld namechars %ld hostbytes %ld "
        "bytes6809 %ld fixed6809 %ld refs %ld refbytes6809 %ld\n",
        StatSyms, StatNameChars, StatBytes,
        StatSyms * 13 + StatNameChars, StatSyms * 76,
        refs, refs * 5);
}
#endif
