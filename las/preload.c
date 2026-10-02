/* **************************************************************** *
 * preload.c - constants known only to the linker (--defs=file.r)   *
 *                                                                  *
 * lwasm-style object sources don't include the defs: names like    *
 * F$Exit or Prgrm are external, and the linker takes their values  *
 * from a library that exports the defs (wildbitsl2). lwasm's       *
 * object format can hand the linker whole expressions on such      *
 * names ((Prgrm+Objct)*256, ^Carry, "type equ Prgrm"); ROF can     *
 * only say "add this symbol's value".                              *
 *                                                                  *
 * --defs=file reads the constants a ROF file or library exports.   *
 * They are NOT defined in the symbol table: references to them     *
 * stay external, with the instruction sizes lwasm uses for         *
 * externals, and rlink fills them in. preload_resolve() is called  *
 * only where ROF can't express the reference or the value is       *
 * needed while assembling (operators other than + and -, equ/set,  *
 * org/rmb counts); it replaces those references with their values. *
 * Never in conditions: lwasm can't see link-time values there, and *
 * include guards like "IFNE OS9.D-1" would skip the defs.          *
 * **************************************************************** */

#include "rma.h"

#define PLBUCKETS 128

struct pldef
{
    struct pldef *next;
    int value;
    char name[1];               /* allocated at the name's length */
};

static struct pldef *PlTable[PLBUCKETS];
static char *PlFiles[8];
static int PlCount;

/* ******************************************************************** *
 * preload_add() - remember a --defs file (GetOpts)                     *
 * ******************************************************************** */

void
#ifdef __STDC__
preload_add (char *file)
#else
preload_add (file)
     char *file;
#endif
{
    if (PlCount >= 8)
    {
        errexit ("too many --defs files");
    }

    PlFiles[PlCount++] = file;
}

static int
#ifdef __STDC__
plhash (char *name)
#else
plhash (name)
     register char *name;
#endif
{
    register int sum = 0;

    while (*name)
    {
        sum += PragNoCase ? _tolower (*name) : *name;
        ++name;
    }

    return sum & (PLBUCKETS - 1);
}

static int
#ifdef __STDC__
plsame (char *a, char *b)
#else
plsame (a, b)
     register char *a, *b;
#endif
{
    if ( ! PragNoCase)
    {
        return strcmp (a, b) == 0;
    }

    while (*a && (_tolower (*a) == _tolower (*b)))
    {
        ++a;
        ++b;
    }

    return *a == *b;
}

/* ******************************************************************** *
 * preload_find() - the preloaded value of name; returns 0 if unknown   *
 * ******************************************************************** */

int
#ifdef __STDC__
preload_find (char *name, int *value)
#else
preload_find (name, value)
     char *name;
     int *value;
#endif
{
    register struct pldef *d;

    for (d = PlTable[plhash (name)]; d; d = d->next)
    {
        if (plsame (d->name, name))
        {
            *value = d->value;
            return 1;
        }
    }

    return 0;
}

/* ROF reading helpers */

static int
#ifdef __STDC__
rword (FILE *f)
#else
rword (f)
     FILE *f;
#endif
{
    int hi = getc (f);
    int lo = getc (f);

    return ((hi & 0xff) << 8) | (lo & 0xff);
}

static void
#ifdef __STDC__
rname (FILE *f, char *buf, int max)
#else
rname (f, buf, max)
     FILE *f;
     char *buf;
     int max;
#endif
{
    int c, n = 0;

    while (((c = getc (f)) != EOF) && c)
    {
        if (n < max - 1)
        {
            buf[n++] = c;
        }
    }

    buf[n] = '\0';
}

/* ******************************************************************** *
 * preload_load() - read the constants exported by each --defs file.    *
 *          A ROF library is ROF modules one after another.             *
 * ******************************************************************** */

void
preload_load (ZILCH)
{
    int fi;

    for (fi = 0; fi < PlCount; ++fi)
    {
        FILE *f = fopen (PlFiles[fi], "r");
        char name[SYMLEN + 1];
        unsigned char hdr[28];

        if ( ! f)
        {
            fprintf (stderr, "\"%s\" - ", PlFiles[fi]);
            errexit ("can't open --defs file");
        }

        while (fread (hdr, 28, 1, f) == 1)
        {
            long skip;
            int count, i;

            if ((hdr[0] != 0x62) || (hdr[1] != 0xcd) ||
                (hdr[2] != 0x23) || (hdr[3] != 0x87))
            {
                errexit ("--defs file is not ROF");
            }

            rname (f, name, sizeof (name));         /* psect name */

            for (count = rword (f), i = 0; i < count; ++i)
            {
                int type, value;

                rname (f, name, sizeof (name));
                type = getc (f);
                value = rword (f);

                if ((type & 7) == (CODENT | DIRENT))    /* constants only */
                {
                    struct pldef *d = (struct pldef *)
                            getmem (sizeof (struct pldef) + strlen (name));
                    int h = plhash (name);

                    strcpy (d->name, name);
                    d->value = (short) value;
                    d->next = PlTable[h];
                    PlTable[h] = d;
                }
            }

            /* skip code and initialised data, then the reference lists */

            skip = ((hdr[22] << 8) | hdr[23]) + ((hdr[18] << 8) | hdr[19]) +
                   ((hdr[20] << 8) | hdr[21]);
            fseek (f, skip, 1);

            for (count = rword (f), i = 0; i < count; ++i)
            {
                rname (f, name, sizeof (name));
                fseek (f, 3L * rword (f), 1);
            }

            fseek (f, 3L * rword (f), 1);               /* local refs */

            for (count = rword (f), i = 0; i < count; ++i)
            {
                rname (f, name, sizeof (name));
                rword (f);                              /* size */
                fseek (f, 3L * rword (f), 1);
            }
        }

        fclose (f);
    }
}

/* ******************************************************************** *
 * preload_resolve() - If every reference from "from" to OptBPtr is a   *
 *          preloaded constant (an undefined symbol --defs knows), add  *
 *          their values (negated where marked) to *value, drop those   *
 *          references and return 1. Otherwise change nothing, return 0.*
 * ******************************************************************** */

int
#ifdef __STDC__
preload_resolve (struct ref_ent *from, long *value)
#else
preload_resolve (from, value)
     struct ref_ent *from;
     long *value;
#endif
{
    register struct ref_ent *r;
    long sum = 0;
    int v;

    if ( ! PlCount || (from >= OptBPtr))
    {
        return 0;
    }

    for (r = from; r < OptBPtr; ++r)
    {
        if ( ! r->RAddr || (r->RAddr->locmsk & RELATIVE) ||
             ! preload_find (r->RAddr->symblnam, &v))
        {
            return 0;
        }

        sum += (r->ETyp & NEGMASK) ? -v : v;
    }

    *value += sum;
    OptBPtr = from;
    PlUsed = 1;
    return 1;
}

/* ******************************************************************** *
 * pl_fix() - preload_resolve() on NmbrInt, the value just evaluated    *
 *            (NmbrInt is a long on the CoCo, an int elsewhere).        *
 * ******************************************************************** */

int
#ifdef __STDC__
pl_fix (struct ref_ent *from)
#else
pl_fix (from)
     struct ref_ent *from;
#endif
{
    long t = NmbrInt;

    if (preload_resolve (from, &t))
    {
        NmbrInt = t;
        return 1;
    }

    return 0;
}
