/* **************************************************************** *
 * lwsel.c - choose and order modules the way lwlink does (--lwlink)*
 *                                                                  *
 * rlink reads each library once, in order, takes a member when it  *
 * defines something still needed, and lays modules out in the      *
 * order it read them, mainline first. lwlink differs twice:        *
 *                                                                  *
 *  - choosing: it keeps taking members until nothing new is        *
 *    needed, each time the first member (by library, then position *
 *    in it) that defines a still-undefined name;                   *
 *  - ordering: its os9 script places sections by name first (code, *
 *    .text, data, .data) and command-line order second, libraries  *
 *    included at their place on the command line. NitrOS9 links    *
 *    "-lwildbitsl2 -lnet -lalib prog.o", so a net member (section  *
 *    code) lands before the program's own code, while an alib      *
 *    member (.text) lands after it.                                *
 *                                                                  *
 * lw_select() does both up front. It indexes every module's        *
 * exported and referenced names, and the rank las exports as       *
 * __lwsec; works out the members lwlink would take; and copies     *
 * every module of the program, in lwlink's order, into one         *
 * temporary ROF file that becomes rlink's only input. The          *
 * libraries stay: rlink's own scan then only finds constants-only  *
 * members (like the defs library), which add no code or data.      *
 *                                                                  *
 * las merges a file's sections into one psect, so a file mixing    *
 * code with .text or data is placed by its first one only.         *
 * **************************************************************** */

#include "rlink.h"
#include <stdlib.h>
#ifndef _OS9
#include <unistd.h>
#endif

#define MAXMODS 512
#define MODSTEP 32              /* Mods grows by this many */

struct lwmod
{
    char  *file;
    long   start, end;          /* byte range in the file */
    int    lib;                 /* library index, -1 for an input file */
    int    ord;                 /* place on the command line */
    int    rank;                /* section order, from __lwsec */
    int    taken;
    int    hassize;             /* has code or data (not constants-only) */
    int    ndefs, nrefs;
    char **mdefs, **mrefs;      /* m: K&R C has one namespace for all
                                 * struct members (dref_node has refs) */
};

static struct lwmod *Mods;      /* grown as modules are indexed: MAXMODS
                                 * of them took 13K of the 6809's 64K */
static int NMods, MaxMods;
static char TmpName[256];

static int
rd (fp)
    FILE *fp;
{
    int hi = getc(fp);
    int lo = getc(fp);

    return ((hi & 0xff) << 8) | (lo & 0xff);
}

static char *
rdname (fp)
    FILE *fp;
{
    char buf[SYMLEN + 1];
    char *p;
    int c, n = 0;

    while (((c = getc(fp)) != EOF) && c)
    {
        if (n < SYMLEN)
            buf[n++] = c;
    }

    buf[n] = '\0';
    p = (char *)malloc(n + 1);

    if (!p)
        fatal("out of memory in lw_select");

    strcpy(p, buf);
    return p;
}

/* Index every module in a file: what it exports, what it references */

static void
index_file (name, lib, ord)
    char *name;
    int lib;
    int ord;
{
    FILE *fp = fopen(name, "r");
    unsigned char h[28];

    if (!fp)
        fatal("can't open '%s'", name);

    while (fread(h, 28, 1, fp) == 1)
    {
        struct lwmod *m;
        int count, i, n, value, nonames;
        long skip;

        if (NMods >= MAXMODS)
            fatal("too many modules for --lwlink");

        if (NMods == MaxMods)
        {
            MaxMods += MODSTEP;
            Mods = (struct lwmod *)(Mods ? realloc(Mods, MaxMods * sizeof(*Mods))
                                         : malloc(MaxMods * sizeof(*Mods)));

            if (!Mods)
                fatal("out of memory in lw_select");
        }

        m = &Mods[NMods++];
        m->file = name;
        m->start = ftell(fp) - 28;
        m->lib = lib;
        m->ord = ord;
        m->rank = 0;
        m->taken = (lib < 0);
        m->hassize = (((h[14] << 8) | h[15]) + ((h[16] << 8) | h[17]) +
                      ((h[18] << 8) | h[19]) + ((h[20] << 8) | h[21]) +
                      ((h[22] << 8) | h[23])) != 0;
        free(rdname(fp));                       /* psect name */

        /* A constants-only library member (the defs library: 2,000
         * names, 30K) is never copied, so its names aren't kept: the
         * 6809 hasn't the room. rlink's own library scan still finds
         * it. It doesn't count as a definer here (DESIGN.md D31) */

        count = rd(fp);
        nonames = (lib >= 0) && !m->hassize;
        m->ndefs = nonames ? 0 : count;
        m->mdefs = (char **)malloc((m->ndefs + 1) * sizeof(char *));

        for (i = 0; i < count; ++i)
        {
            if (nonames)
            {
                while (((n = getc(fp)) != EOF) && n)
                    ;
                getc(fp);
                rd(fp);
                continue;
            }

            m->mdefs[i] = rdname(fp);
            getc(fp);                           /* type */
            value = rd(fp);

            if (!strcmp(m->mdefs[i], "__lwsec"))
                m->rank = value;
        }

        skip = ((h[22] << 8) | h[23]) + ((h[18] << 8) | h[19]) +
               ((h[20] << 8) | h[21]);
        fseek(fp, skip, 1);                     /* code and idata */

        count = rd(fp);
        m->nrefs = count;
        m->mrefs = (char **)malloc((count + 1) * sizeof(char *));

        for (i = 0; i < count; ++i)
        {
            m->mrefs[i] = rdname(fp);
            n = rd(fp);
            fseek(fp, 3L * n, 1);
        }

        fseek(fp, 3L * rd(fp), 1);              /* local refs */

        /* commons: only in ROF version 1 (las); Microware's are 0 */

        for (count = h[13] ? rd(fp) : 0, i = 0; i < count; ++i)
        {
            free(rdname(fp));
            rd(fp);
            fseek(fp, 3L * rd(fp), 1);
        }

        m->end = ftell(fp);
    }

    fclose(fp);
}

static int
defined_by_taken (name)
    char *name;
{
    int i, j;

    for (i = 0; i < NMods; ++i)
    {
        if (!Mods[i].taken)
            continue;

        for (j = 0; j < Mods[i].ndefs; ++j)
        {
            if (!strcmp(Mods[i].mdefs[j], name))
                return 1;
        }
    }

    return 0;
}

/* The first library member, in library then file order, defining name */

static struct lwmod *
first_definer (name)
    char *name;
{
    int i, j;

    for (i = 0; i < NMods; ++i)
    {
        if (Mods[i].lib < 0 || Mods[i].taken)
            continue;

        for (j = 0; j < Mods[i].ndefs; ++j)
        {
            if (!strcmp(Mods[i].mdefs[j], name))
                return &Mods[i];
        }
    }

    return 0;
}

/* lwlink's order: section rank, then command line, then file order */

static int
lw_order (a, b)
    struct lwmod **a, **b;
{
    if ((*a)->rank != (*b)->rank)
        return (*a)->rank - (*b)->rank;

    if ((*a)->ord != (*b)->ord)
        return (*a)->ord - (*b)->ord;

    return (int)(*a - *b);
}

/* Remove the temporary file. Called by fatal(), tidy() and at the end
 * of main(), not through atexit(): CoCoC's C library has no atexit. */

void
lw_cleanup ()
{
    if (TmpName[0])
        unlink(TmpName);
}

lw_select ()
{
    struct lwmod **order;
    FILE *out;
    int i, j, n, changed;

    /* Number the files in command-line order: input file i came after
     * the libraries given before it (lpos, rlink_main.c) */

    for (i = 0; i < FileCount; ++i)
    {
        for (n = j = 0; j < lfilenum; ++j)
        {
            if (lpos[j] <= i)
                ++n;
        }

        index_file(filenames[i], -1, i + n);
    }

    for (i = 0; i < lfilenum; ++i)
        index_file(lfiles[i], i, lpos[i] + i);

    /* Keep taking members until nothing new is needed */

    do
    {
        changed = 0;

        for (i = 0; i < NMods; ++i)
        {
            if (!Mods[i].taken)
                continue;

            for (j = 0; j < Mods[i].nrefs; ++j)
            {
                struct lwmod *d;

                if (defined_by_taken(Mods[i].mrefs[j]))
                    continue;

                if ((d = first_definer(Mods[i].mrefs[j])))
                {
                    d->taken = 1;
                    changed = 1;
                }
            }
        }
    } while (changed);

    /* Every module of the program in lwlink's order, as one input.
     * Constants-only members stay in their libraries. */

    order = (struct lwmod **)malloc((NMods + 1) * sizeof(*order));

    if (!order)
        fatal("out of memory in lw_select");

    for (n = i = 0; i < NMods; ++i)
    {
        if (Mods[i].lib < 0 || (Mods[i].taken && Mods[i].hassize))
            order[n++] = &Mods[i];
    }

    qsort(order, n, sizeof(*order), lw_order);
    sprintf(TmpName, "%.200s.lwsel.r", OutfName);

    if (!(out = fopen(TmpName, "w")))
        fatal("can't create '%s'", TmpName);

    for (i = 0; i < n; ++i)
    {
        FILE *in;
        long k;

        if (!(in = fopen(order[i]->file, "r")))
            fatal("can't open '%s'", order[i]->file);

        fseek(in, order[i]->start, 0);

        for (k = order[i]->end - order[i]->start; k > 0; --k)
            putc(getc(in), out);

        fclose(in);
    }

    fclose(out);
    free(order);

    /* The index isn't needed again: give it back for rlink's own use */

    for (i = 0; i < NMods; ++i)
    {
        for (j = 0; j < Mods[i].ndefs; ++j)
            free(Mods[i].mdefs[j]);

        for (j = 0; j < Mods[i].nrefs; ++j)
            free(Mods[i].mrefs[j]);

        free(Mods[i].mdefs);
        free(Mods[i].mrefs);
    }

    free(Mods);
    Mods = 0;
    NMods = MaxMods = 0;

    filenames = (char **)malloc(2 * sizeof(char *));
    filenames[0] = TmpName;
    FileCount = 1;
    return 0;
}
