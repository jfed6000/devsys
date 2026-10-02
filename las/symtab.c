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
 * sym_walk's order (bucket 0..63, alphabetical within a bucket)    *
 * decides the order of the listing's symbol table and of the ROF's *
 * global and external lists, so it must not change by accident.   *
 * **************************************************************** */

#include "rma.h"

#define NBUCKETS 64

static struct symblstr *Buckets[NBUCKETS];

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
        sum += *p;
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
            cmp = strcmp (name, ((*node = n)->symblnam));

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

    cmp = search (name, &parent);
    new = (struct symblstr *) getmem (sizeof (struct symblstr));

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
