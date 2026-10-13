/*
    pch.c - precompiled header images for dcc68 (devsys, step 17 measurement)

    An image holds the declarations a set of headers makes: external
    variables and functions, typedefs, struct tags and members. With
    "-p=image", dcc68 reads none of that header text; when lookup() makes
    a new symbol, pchload() looks for its name in the image and, if it is
    there, fills the symbol in as the header would have. Names a file
    never uses are never loaded, so they cost no memory.

    The image (all numbers 16-bit, high byte first):
        "DCI1"
        names           where the header list is (dcpp -h checks that a
                        file includes only headers the image was made of)
        heads[2][128]   the first record of each hash chain (0: none);
                        [0] the main table, [1] struct tags and members
        filter[256]     2048 bits: a name whose bit is clear isn't here,
                        so most misses cost no file access
        records:        next, name length, name, type, size, offset,
                        storage (byte), block level (byte), dimension
                        count (byte) and the dimensions, member list
                        (offset of a list, or 0), and for an incomplete
                        struct (USTRUCT) the tag's name (length, name)
        lists:          member count (byte), then each member's record
        header list:    count (byte), then for each header its name as
                        dcpp opened it (length, name) and the checksum of
                        its text as dcpp passed it on (hdrsum()): dcpp -h
                        checks it, so a header that has changed, or that
                        expands differently (a #define before it), is
                        caught instead of compiled wrong

    Built with -DMKIMG this file instead writes an image ("-w=image") of
    everything a headers-only file declared; a header set that defines
    anything (storage, functions) can't be imaged and gets none.
*/

#include "cj.h"

#define HEADS   6
#define FILTER  (HEADS + 2 * 128 * 2)
#define RECORDS (FILTER + 256)

static
pchbit(name,tab)
register char *name;
{
    register unsigned h = tab;

    while (*name) h = h * 31 + *name++;
    return h & 2047;
}

#ifndef MKIMG

static char pfilter[256];

typedef struct lcache {
    unsigned id;                /* the list's offset in the image */
    elem *list;
    struct lcache *lnext;
} lcache;

static lcache *lists;

static unsigned
rd16()
{
    register int h;

    h = getc(pchfp) << 8;
    return h | (getc(pchfp) & 0xff);
}

static
pchseek(off)
unsigned off;
{
    fseek(pchfp,(long) off,0);
}

pchopen(name)
char *name;
{
    register int i;

    if ((pchfp = fopen(name,"r")) == NULL
                || rd16() != ('D' << 8 | 'C') || rd16() != ('I' << 8 | '1'))
        fatal(M_CANT_OPEN_HEADER_IMAGE);
    pchseek(FILTER);
    for (i = 0; i < 256; ++i) pfilter[i] = getc(pchfp);
}

static
rdname(name)
register char *name;
{
    register int n;

    n = getc(pchfp);
    while (n--) *name++ = getc(pchfp);
    *name = '\0';
}

/* the symbol for a name in struct tags and members */
static symnode *
moslook(name)
char *name;
{
    register symnode *p;
    int save;

    save = mosflg;
    mosflg = 1;
    p = lookup(name);
    mosflg = save;
    return p;
}

/* a member list: one copy per image list, so struct types compare equal */
static elem *
getlist(id)
unsigned id;
{
    register lcache *c;
    register elem *e;
    elem *first, *last;
    int n, i;
    unsigned off;
    char name[NAMESIZE+1];

    for (c = lists; c; c = c->lnext)
        if (c->id == id) return c->list;

    pchseek(id);
    n = getc(pchfp);
    first = last = NULL;
    for (i = 0; i < n; ++i) {
        e = (elem *) grab(sizeof(elem));
        e->strnext = NULL;      /* elements: below */
        if (last) last->strnext = e;
        else first = e;
        last = e;
    }

    /* known before its members load: a member may point to this struct */
    c = (lcache *) grab(sizeof(lcache));
    c->id = id;
    c->list = first;
    c->lnext = lists;
    lists = c;

    for (e = first, i = 0; e; e = e->strnext, ++i) {
        pchseek(id + 1 + 2 * i);
        off = rd16();
        pchseek(off + 2);
        rdname(name);
        e->element = moslook(name);
    }
    return first;
}

/* a new symbol: fill it in from the image if its name is there */
pchload(ptr,name)
register symnode *ptr;
char *name;
{
    register dimnode *d, **dp;
    int tab, b, nd;
    unsigned off;
    char tag[NAMESIZE+1], rec[NAMESIZE+1];

    tab = mosflg ? 1 : 0;
    b = pchbit(name,tab);
    if (!(pfilter[b >> 3] & (1 << (b & 7)))) return;

    pchseek(HEADS + (tab * 128 + hash(name)) * 2);
    off = rd16();
    while (off) {
        pchseek(off);
        off = rd16();               /* the next record in this chain */
        rdname(rec);
        if (strncmp(rec,name,NAMESIZE) == 0) goto found;
    }
    return;
found:
    /* the record in order; nothing below moves in the image until
     * getlist() and moslook(), which come last */
    ptr->type = (short) rd16();
    ptr->size = (short) rd16();
    ptr->offset = (short) rd16();
    ptr->storage = getc(pchfp);
    ptr->blklev = getc(pchfp);
    dp = &ptr->dimptr;
    for (nd = getc(pchfp); nd; --nd) {
        *dp = d = (dimnode *) grab(sizeof(dimnode));
        d->dim = rd16();
        dp = &d->dptr;
    }
    *dp = NULL;
    off = rd16();
    if (btype(ptr->type) == USTRUCT) rdname(tag);
    ptr->x.elems = off ? getlist(off) : NULL;
    if (btype(ptr->type) == USTRUCT) ptr->size = (int) moslook(tag);
}

#else   /* MKIMG: write the image */

static symnode **syms;          /* what goes in, by table */
static unsigned *soff;          /* each one's record offset */
static int nsyms, ntab0;
static char *hdrs[64];          /* the headers the image is made of */
static unsigned hsums[64];      /* and their text's checksums */
static int nhdrs, hcur = -1;
static char *mainfile;

/* a file dcpp opened (get.c): the first is the source, the rest headers */
pchname(name)
char *name;
{
    register int i;

    hcur = -1;
    if (!mainfile) {
        mainfile = strcpy(grab(strlen(name) + 1),name);
        return;
    }
    if (strcmp(name,mainfile) == 0) return;
    for (i = 0; i < nhdrs; ++i)
        if (strcmp(hdrs[i],name) == 0) {
            hcur = i;
            return;
        }
    if (nhdrs == 64) fatal(M_IMAGE_TOO_MANY_HEADERS);
    hdrs[hcur = nhdrs++] = strcpy(grab(strlen(name) + 1),name);
}

/* a line of text (get.c): add it to its header's checksum */
pchline(s)
char *s;
{
    if (hcur >= 0) hsums[hcur] = hdrsum(hsums[hcur],s);
}

/* The checksum of a header's text, a line at a time; the same function
 * as dcpp's (dcc/CPP/dues.c). Blank lines don't count, so a header that
 * a guard empties the second time adds nothing. */
hdrsum(sum,s)
register unsigned sum;
register char *s;
{
    register char *p;

    for (p = s; *p == ' ' || *p == '\t'; ++p) ;
    if (!*p) return sum;
    while (*s) sum = (((sum << 1) | (sum >> 15 & 1)) ^ (*s++ & 0xff)) & 0xffff;
    return ((sum << 1) | (sum >> 15 & 1)) & 0xffff;
}

static elem **elists;           /* the distinct member lists */
static unsigned *eoff;
static int nlists;

static
wr16(f,v)
FILE *f;
{
    putc((v >> 8) & 0xff,f);
    putc(v & 0xff,f);
}

static
imageable(p)
register symnode *p;
{
    if (p->type == UNDECL || p->type == KEYWORD || p->type == LABEL)
        return 0;
    switch (p->storage) {
        case EXTERN:
        case EXTERND:
        case TYPEDEF:
        case STRTAG:
        case MOS:
            return 1;
    }
    return -1;                  /* a definition: no image */
}

static
dimcount(d)
register dimnode *d;
{
    register int n;

    for (n = 0; d; d = d->dptr) ++n;
    return n;
}

static
recsize(p)
register symnode *p;
{
    register int n;

    n = 2 + 1 + strlen(p->sname) + 6 + 3 + 2 * dimcount(p->dimptr) + 2;
    if (btype(p->type) == USTRUCT)
        n += 1 + strlen(((symnode *) p->size)->sname);
    return n;
}

static unsigned
offof(p)
register symnode *p;
{
    register int i;

    for (i = 0; i < nsyms; ++i) if (syms[i] == p) return soff[i];
    fatal(M_IMAGE_MEMBER_NOT_IN_IMAGE);
}

static unsigned
listof(e)
register elem *e;
{
    register int i;

    for (i = 0; i < nlists; ++i) if (elists[i] == e) return eoff[i];
    return 0;
}

static
wrname(f,s)
FILE *f;
register char *s;
{
    putc(strlen(s),f);
    while (*s) putc(*s++,f);
}

pchwrite(name)
char *name;
{
    register symnode *p;
    register int i, t;
    symnode **tab;
    int b, n;
    unsigned off;
    char filter[256];
    FILE *f;
    elem *e;

    /* count, then collect: table 0 first, chain by chain */
    n = 0;
    for (t = 0; t < 2; ++t) {
        tab = t ? mostab : hashtab;
        for (b = 0; b < 128; ++b)
            for (p = tab[b]; p; p = p->hlink)
                switch (imageable(p)) {
                    case -1: fprintf(stderr,"image: %s is defined\n",p->sname);
                             errexit();
                    case 1: ++n;
                }
    }
    syms = (symnode **) grab(n * sizeof(symnode *));
    soff = (unsigned *) grab(n * sizeof(unsigned));
    elists = (elem **) grab(n * sizeof(elem *));
    eoff = (unsigned *) grab(n * sizeof(unsigned));

    off = RECORDS;
    for (t = 0; t < 2; ++t) {
        tab = t ? mostab : hashtab;
        for (b = 0; b < 128; ++b)
            for (p = tab[b]; p; p = p->hlink)
                if (imageable(p) == 1) {
                    syms[nsyms] = p;
                    soff[nsyms++] = off;
                    off += recsize(p);
                    if ((e = p->x.elems) && !listof(e)) {
                        elists[nlists] = e;
                        eoff[nlists++] = 1;     /* placed below */
                    }
                }
        if (t == 0) ntab0 = nsyms;
    }
    for (i = 0; i < nlists; ++i) {
        eoff[i] = off;
        for (n = 0, e = elists[i]; e; e = e->strnext) ++n;
        if (n > 255) fatal(M_IMAGE_STRUCT_TOO_BIG);
        off += 1 + 2 * n;
    }

    if ((f = fopen(name,"w")) == NULL) fatal(M_CANT_WRITE_IMAGE);
    fputs("DCI1",f);
    wr16(f,off);                    /* the header list, after the lists */
    for (t = 0; t < 2; ++t)
        for (b = 0; b < 128; ++b) {
            /* the first of this chain in the image */
            for (i = t ? ntab0 : 0; i < (t ? nsyms : ntab0); ++i)
                if (hash(syms[i]->sname) == b) break;
            wr16(f,i < (t ? nsyms : ntab0) ? soff[i] : 0);
        }
    for (i = 0; i < 256; ++i) filter[i] = 0;
    for (i = 0; i < nsyms; ++i) {
        b = pchbit(syms[i]->sname,i >= ntab0);
        filter[b >> 3] |= 1 << (b & 7);
    }
    for (i = 0; i < 256; ++i) putc(filter[i],f);

    for (i = 0; i < nsyms; ++i) {
        p = syms[i];
        /* the next record in the same chain and table */
        for (t = i + 1; t < (i < ntab0 ? ntab0 : nsyms); ++t)
            if (hash(syms[t]->sname) == hash(p->sname)) break;
        wr16(f,t < (i < ntab0 ? ntab0 : nsyms) ? soff[t] : 0);
        wrname(f,p->sname);
        wr16(f,p->type);
        /* an incomplete struct's size is its tag, a pointer: the tag's
         * name follows instead, so every machine writes the same image */
        wr16(f,btype(p->type) == USTRUCT ? 0 : p->size);
        wr16(f,p->offset);
        putc(p->storage,f);
        putc(p->blklev,f);
        {
            register dimnode *d;

            if ((n = dimcount(p->dimptr)) > 255)
                fatal(M_IMAGE_TOO_MANY_DIMENSIONS);
            putc(n,f);
            for (d = p->dimptr; d; d = d->dptr) wr16(f,d->dim);
        }
        wr16(f,p->x.elems ? listof(p->x.elems) : 0);
        if (btype(p->type) == USTRUCT) wrname(f,((symnode *) p->size)->sname);
    }
    for (i = 0; i < nlists; ++i) {
        for (n = 0, e = elists[i]; e; e = e->strnext) ++n;
        putc(n,f);
        for (e = elists[i]; e; e = e->strnext) wr16(f,offof(e->element));
    }
    putc(nhdrs,f);
    for (i = 0; i < nhdrs; ++i) {
        wrname(f,hdrs[i]);
        wr16(f,hsums[i]);
    }
    if (fclose(f) == EOF) fatal(M_CANT_WRITE_IMAGE);
}

#endif
