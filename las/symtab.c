/* **************************************************************** *
 * symtab.c - the symbol table                                      *
 *                                                                  *
 * Everything that knows how symbols are stored lives here. The     *
 * rest of las finds, adds and visits symbols only through:         *
 *                                                                  *
 *      sym_find (name)     the symbol, or 0 if not defined         *
 *      sym_add (name)      a new, zeroed symbol (name must be new) *
 *      sym_walk (fn)       call fn for every symbol, in table order*
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
            sprintf (key, "%.*s~%d", SYMLEN - 1, name, LocalCtx);
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

static struct symblstr *Buckets[NBUCKETS];

#ifdef SYMSTATS
/* Measurement build only (make -C build/stats): see sym_stats() */
static long StatSyms, StatNameChars, StatBytes;
#endif

static struct symblstr **
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
search (char *name, struct symblstr **node)
#else
search (name, node)
     char *name;
     struct symblstr **node;
#endif
{
    int cmp = 1;
    register struct symblstr *n;

    if ( (n = *node = *bucket (name)) )
    {
        do
        {
            cmp = namecmp (name, ((*node = n)->symblnam));

            if (cmp < 0)
            {
                n = n->left;
            }
            else
            {
                if ((cmp == 0) || ! (n->locmsk & LOC1BYT))
                {
                    break;
                }

                n = n->right;
            }
        } while (n);
    }

    return cmp;
}

struct symblstr *
#ifdef __STDC__
sym_find (char *name)
#else
sym_find (name)
     char *name;
#endif
{
    struct symblstr *node;

    name = symkey (name);
    return search (name, &node) ? 0 : node;
}

/* ******************************************************************** *
 * sym_add() - Create a symbol. The caller must know name isn't in the  *
 *             table yet (sym_find returned 0).                         *
 * ******************************************************************** */

struct symblstr *
#ifdef __STDC__
sym_add (char *name)
#else
sym_add (name)
     char *name;
#endif
{
    register struct symblstr *new;
    struct symblstr *parent;
    int cmp;

    name = symkey (name);
    cmp = search (name, &parent);

    /* sizeof already includes symblnam[1], room for the terminating 0 */
    new = (struct symblstr *) getmem (sizeof (struct symblstr) + strlen (name));
#ifdef SYMSTATS
    ++StatSyms;
    StatNameChars += strlen (name);
    StatBytes += sizeof (struct symblstr) + strlen (name);
#endif

    if (parent)
    {
        if (cmp < 0)
        {
            new->right = parent;        /* thread up to the parent */
            parent->left = new;
        }
        else
        {
            new->right = parent->right; /* inherit the parent's thread */
            parent->right = new;
            parent->locmsk |= LOC1BYT;
        }
    }
    else
    {
        *bucket (name) = new;
        new->right = 0;
    }

    strcpy (new->symblnam, name);
    return new;
}

/* ******************************************************************** *
 * next() - The node after this one in alphabetical order, or 0.        *
 * ******************************************************************** */

static struct symblstr *
#ifdef __STDC__
next (struct symblstr *node)
#else
next (node)
     struct symblstr *node;
#endif
{
    register struct symblstr *n = node->right;

    if (node->locmsk & LOC1BYT)
    {
        while (n->left)
        {
            n = n->left;
        }
    }

    return n;
}

void
#ifdef __STDC__
sym_walk (void (*fn) (struct symblstr *))
#else
sym_walk (fn)
     void (*fn) ();
#endif
{
    register struct symblstr *n;
    int b;

    for (b = 0; b < NBUCKETS; ++b)
    {
        if ( (n = Buckets[b]) )
        {
            while (n->left)
            {
                n = n->left;
            }

            do
            {
                (*fn) (n);
            } while ( (n = next (n)) );
        }
    }
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
