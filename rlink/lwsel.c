/* **************************************************************** *
 * lwsel.c - choose library members the way lwlink does (--lwlink)  *
 *                                                                  *
 * rlink reads each library once, in order, and lays modules out in *
 * the order it takes them. lwlink instead keeps taking members     *
 * until nothing new is needed, each time the first member (by      *
 * library, then position in it) that defines a still-undefined     *
 * name, and lays out the members it took in library order.         *
 *                                                                  *
 * lw_select() does that choice up front: it indexes every module's *
 * exported and referenced names, works out the members lwlink      *
 * would take, copies those with code or data, in library order,    *
 * into one temporary ROF file, and makes that an ordinary input    *
 * after the given files. The libraries stay: rlink's own scan then *
 * only finds constants-only members (like the defs library), which *
 * it handles without including a module.                           *
 * **************************************************************** */

#include "rlink.h"
#include <stdlib.h>
#ifndef _OS9
#include <unistd.h>
#endif

#define MAXMODS 512

struct lwmod
{
    char  *file;
    long   start, end;          /* byte range in the file */
    int    lib;                 /* library index, -1 for an input file */
    int    taken;
    int    hassize;             /* has code or data (not constants-only) */
    int    ndefs, nrefs;
    char **defs, **refs;
};

static struct lwmod Mods[MAXMODS];
static int NMods;
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
index_file (name, lib)
    char *name;
    int lib;
{
    FILE *fp = fopen(name, "r");
    unsigned char h[28];

    if (!fp)
        fatal("can't open '%s'", name);

    while (fread(h, 28, 1, fp) == 1)
    {
        struct lwmod *m;
        int count, i, n;
        long skip;

        if (NMods >= MAXMODS)
            fatal("too many modules for --lwlink");

        m = &Mods[NMods++];
        m->file = name;
        m->start = ftell(fp) - 28;
        m->lib = lib;
        m->taken = (lib < 0);
        m->hassize = (((h[14] << 8) | h[15]) + ((h[16] << 8) | h[17]) +
                      ((h[18] << 8) | h[19]) + ((h[20] << 8) | h[21]) +
                      ((h[22] << 8) | h[23])) != 0;
        free(rdname(fp));                       /* psect name */

        count = rd(fp);
        m->ndefs = count;
        m->defs = (char **)malloc((count + 1) * sizeof(char *));

        for (i = 0; i < count; ++i)
        {
            m->defs[i] = rdname(fp);
            getc(fp);                           /* type */
            rd(fp);                             /* value */
        }

        skip = ((h[22] << 8) | h[23]) + ((h[18] << 8) | h[19]) +
               ((h[20] << 8) | h[21]);
        fseek(fp, skip, 1);                     /* code and idata */

        count = rd(fp);
        m->nrefs = count;
        m->refs = (char **)malloc((count + 1) * sizeof(char *));

        for (i = 0; i < count; ++i)
        {
            m->refs[i] = rdname(fp);
            n = rd(fp);
            fseek(fp, 3L * n, 1);
        }

        fseek(fp, 3L * rd(fp), 1);              /* local refs */

        for (count = rd(fp), i = 0; i < count; ++i)   /* commons */
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
            if (!strcmp(Mods[i].defs[j], name))
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
            if (!strcmp(Mods[i].defs[j], name))
                return &Mods[i];
        }
    }

    return 0;
}

static void
lw_cleanup ()
{
    if (TmpName[0])
        unlink(TmpName);
}

lw_select ()
{
    char **newnames;
    FILE *out;
    int i, j, changed;

    for (i = 0; i < FileCount; ++i)
        index_file(filenames[i], -1);

    for (i = 0; i < lfilenum; ++i)
        index_file(lfiles[i], i);

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

                if (defined_by_taken(Mods[i].refs[j]))
                    continue;

                if ((d = first_definer(Mods[i].refs[j])))
                {
                    d->taken = 1;
                    changed = 1;
                }
            }
        }
    } while (changed);

    /* The chosen members, in library order, as one extra input file */

    for (i = 0; i < NMods; ++i)
    {
        if (Mods[i].lib >= 0 && Mods[i].taken && Mods[i].hassize)
            break;
    }

    if (i == NMods)     /* no code or data needed from the libraries */
    {
        return 0;
    }

    sprintf(TmpName, "%.200s.lwsel.r", OutfName);

    if (!(out = fopen(TmpName, "w")))
        fatal("can't create '%s'", TmpName);

    atexit(lw_cleanup);

    for (i = 0; i < NMods; ++i)
    {
        FILE *in;
        long n;

        if (Mods[i].lib < 0 || !Mods[i].taken || !Mods[i].hassize)
            continue;

        if (!(in = fopen(Mods[i].file, "r")))
            fatal("can't open '%s'", Mods[i].file);

        fseek(in, Mods[i].start, 0);

        for (n = Mods[i].end - Mods[i].start; n > 0; --n)
            putc(getc(in), out);

        fclose(in);
    }

    fclose(out);

    newnames = (char **)malloc((FileCount + 2) * sizeof(char *));

    for (i = 0; i < FileCount; ++i)
        newnames[i] = filenames[i];

    newnames[FileCount++] = TmpName;
    filenames = newnames;
    return 0;
}
