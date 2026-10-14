/* **************************************************************** *
 * defsimg.c - precompiled defs (--defsimg=FILE)                    *
 *                                                                  *
 * A NitrOS9 source begins with "use defsfile": some 2,000 lines of *
 * equ/rmb/set that define about 1,500 symbols, read in both        *
 * passes. On the 6809 that is nearly all of a small module's time. *
 * An image is what those lines did: the symbols they created (in   *
 * creation order, so each bucket's tree gets the same shape), the  *
 * conditionals' results, the location counter and a few counters. *
 * Loading it replaces reading the files.                           *
 *                                                                  *
 * The image is for the first "use" at the top of the source. If    *
 * FILE is missing or out of date, that use is read normally and    *
 * recorded, and FILE is written. It is loaded only if reading the  *
 * files would do exactly the same, which holds when:               *
 *   - the symbols defined before the use are exactly those defined *
 *     when it was recorded: same names, types and values (P        *
 *     records; usually the -D symbols). Then whatever the defs     *
 *     looked up is as it was, and nothing they create exists yet.  *
 *   - each "use" resolves to the same path, whose date and size    *
 *     are unchanged (F records)                                    *
 *   - the options and the location counter are the same           *
 * Otherwise the files are read as usual, and the image is left     *
 * alone (it is for another configuration).                         *
 *                                                                  *
 * Pass 2 normally reads the defs again, which changes a few of     *
 * their symbols (an include guard's "set" symbol): the image holds *
 * those final states too (Z records). It is used in pass 2 only if *
 * the symbols from before the use are still as they were and no    *
 * defs symbol has changed since pass 1; else pass 2 reads the      *
 * files, which is always right: pass 1 left the state reading      *
 * would have left. The same holds for each settling pass (pass 2's *
 * code, rma_main.c): an image is applied in every one. A recording *
 * records Z in the first, and the passes after read the defs.      *
 *                                                                  *
 * Recording gives up (and writes no image) on anything else in the *
 * defs: instructions, macros, sections, errors, references, a      *
 * change to a symbol defined before the use.                       *
 *                                                                  *
 * Without --defsimg, in lwasm mode, the same recording is kept in   *
 * memory for the run (Mem): lwasm mode reads the defs in every     *
 * pass (ifp1 is always true), so pass 2 and any settling pass      *
 * apply the record instead of reading them again. Only what those  *
 * passes need: the counters (G), the last nam and ttl (T), and the *
 * range of handles the defs created. Reading the defs in pass 2    *
 * changes exactly one thing in that range: each defined symbol a   *
 * line referenced before its definition loses DIRLOC (the Z        *
 * records of every image, D54). The symbols before the use must    *
 * not change after the record (img_touch), as P checks for a file. *
 * Not when listing: pass 2 lists the defs' lines.                  *
 *                                                                  *
 * File: "LASI", a byte '1' once complete, version, options, the    *
 * use's name, then tagged records: P, F and T (nam/ttl lines, done *
 * again on loading), G (counters, conditionals), S (the symbols    *
 * created), Z, E (end). Values are in the machine's byte order: an *
 * image is for the machine that made it (its dates are, too).      *
 * **************************************************************** */

#include "rma.h"

#ifndef COCO
#  include <sys/stat.h>
#  include <unistd.h>
#endif

/* The file starts "LASI", '0' ('1' once complete), the version (3:
 * since D55 the symbols' types keep sizing's SZ_CODE) */
#define MAGIC   "LASI0\003"
#define DONE    "LASI1\003"
#define DIFFMAX 64              /* symbols pass 2 may change */

/* States */
#define IDLE    0
#define REC1    1               /* recording pass 1 */
#define RECWAIT 2               /* recorded pass 1, watching symbols */
#define REC2    3               /* recording what pass 2 changes */
#define LOADED  4               /* loaded in pass 1, watching symbols */

char *ImgName;                  /* --defsimg=FILE */
char ImgRec,                    /* recording pass 1 */
     ImgOn;                     /* symtab.c reports changes */
FILE **ImgBase;                 /* use-file stack at the use (file_io.c) */

static int State, UseSeen, Touched;
static char Mem;                /* recording in memory, not a file */
static unsigned MemT;           /* Mem: last nam and ttl (bank_alloc) */
#define MEMT 128                /* bytes for each, as _nam and _ttl */
static FILE *Fp;
static SYMH Lo, Hi;             /* the defs' symbols: handles Lo..Hi-1 */
static long PPos, FPos, CondPos, ZPos;  /* where records start */

/* What the defs did: the G record */
static int G[8];
#define Abs0    G[0]            /* location counter before */
#define Abs1    G[1]            /* and after */
#define DNext   G[2]            /* local label blocks begun (NextCtx) */
#define LMode   G[3]            /* 1: LocalCtx = NextCtx after */
#define DList   G[4]            /* lines read (ListLin) */
#define DGlob   G[5]            /* globals counted (GlobCnt) */
#define DIf     G[6]            /* RMA's if count (_If_Dpth) */
#define NCond   G[7]            /* conditionals in the log */

/* At the start of a recording */
static int Next0, Local0, List0, Glob0, Cond0, If0, Err0;

static SYMH Diffs[DIFFMAX];
static int NDiff, Count;

/* Host builds: LASIMG_TRACE=1 says what happened, on stderr (harness) */
#ifdef COCO
#  define TRACE(s)
#else
#  include <stdlib.h>
#  define TRACE(s) if (getenv ("LASIMG_TRACE")) fprintf (stderr, "defsimg: %s\n", s)
#endif

static char Ops[] =
" equ set rmb org if ifeq ifne iflt ifle ifgt ifge ifdef ifndef ifp1 \
else endc endif use nam ttl pag spc ";

/* ******************************************************************** *
 * Reading and writing. Short wrappers: on the 6809 every call that     *
 * passes Fp and a long costs bytes, and las has few to spare (D35).    *
 * ******************************************************************** */

/* gc() - the next byte, 0..255 (CoCoC's getc gives a byte over $7F
 * its sign); 255 at the end of the file, which is no record's tag */

static int
gc (ZILCH)
{
    return getc (Fp) & 0xff;
}

static void
#ifdef __STDC__
pc (int c)
#else
pc (c)
     int c;
#endif
{
    putc (c, Fp);
}

static int
#ifdef __STDC__
rd (char *p, int n)
#else
rd (p, n)
     char *p;
     int n;
#endif
{
    return fread (p, n, 1, Fp) == 1;
}

static void
#ifdef __STDC__
wr (char *p, int n)
#else
wr (p, n)
     char *p;
     int n;
#endif
{
    fwrite (p, n, 1, Fp);
}

/* mark() - *pos = the file position less back. Not "long tell ()
 * { return ftell (Fp); }": dco68's pattern "leas 2,s / puls u,pc ->
 * puls x,u,pc" loses a long result, which DCC returns through X */

static void
#ifdef __STDC__
mark (long *pos, int back)
#else
mark (pos, back)
     long *pos;
     int back;
#endif
{
    *pos = ftell (Fp) - back;
}

static void
#ifdef __STDC__
seek (long pos)
#else
seek (pos)
     long pos;
#endif
{
    fseek (Fp, pos, 0);
}

static void
#ifdef __STDC__
puts0 (char *s)
#else
puts0 (s)
     char *s;
#endif
{
    wr (s, strlen (s) + 1);
}

static int
#ifdef __STDC__
gets0 (char *s, int max)
#else
gets0 (s, max)
     char *s;
     int max;
#endif
{
    register int c;

    while ((c = gc ()) > 0)
    {
        if (--max <= 0)
        {
            return 0;
        }

        *s++ = c;
    }

    *s = '\0';
    return c == 0;
}

/* A symbol: tag, typ, msk (without the tree bit), value, name */

static void
#ifdef __STDC__
putsym (int tag, SYMH h)
#else
putsym (tag, h)
     int tag;
     SYMH h;
#endif
{
    SVAL v = sym_val (h);

    pc (tag);
    pc (sym_typ (h));
    pc (sym_msk (h) & ~LOC1BYT);
    wr ((char *) &v, sizeof (SVAL));
    puts0 (sym_name (h));
}

static int RTyp, RMsk;
static SVAL RVal;
static char RName[SYMLEN + 8];

static int
getsym (ZILCH)
{
    RTyp = gc ();
    RMsk = gc ();
    rd ((char *) &RVal, sizeof (SVAL));
    return gets0 (RName, sizeof (RName));
}

/* Is h the symbol just read? (A symbol from before the use has no
 * references: those come from code.) Sizing's mark in the type isn't
 * part of it: the trial (pass 1) sets it, its end clears it. */

static int
#ifdef __STDC__
same (SYMH h)
#else
same (h)
     SYMH h;
#endif
{
    return h && (((sym_typ (h) ^ RTyp) & 0xff & ~SZ_CODE) == 0) &&
           ((sym_msk (h) & ~LOC1BYT & 0xff) == RMsk) &&
           (sym_val (h) == RVal);
}

/* Give symbol h the fields just read, keeping its tree bit */

static void
#ifdef __STDC__
setsym (SYMH h)
#else
setsym (h)
     SYMH h;
#endif
{
    sym_settyp (h, RTyp);
    sym_setmsk (h, RMsk | (sym_msk (h) & LOC1BYT));
    sym_setval (h, RVal);
}

/* ******************************************************************** *
 * stamp() - A file's date and size, 9 bytes. 0 if it can't be read.   *
 * ******************************************************************** */

#ifdef COCO
static int getfd ();
#endif

static int
#ifdef __STDC__
stamp (char *path, char *buf)
#else
stamp (path, buf)
     char *path;
     char *buf;
#endif
{
#ifdef COCO
    char fd[16];
    FILE *f;
    int ok;

    if ( ! (f = fopen (path, "r")))
    {
        return 0;
    }

    ok = (getfd (fileno (f), fd) != -1);
    fclose (f);
    memcpy (buf, fd + 3, 5);    /* last written: y m d h m */
    memcpy (buf + 5, fd + 9, 4);        /* size */
    return ok;
#else
    struct stat st;
    int t, s;

    if (stat (path, &st) != 0)
    {
        return 0;
    }

    t = (int) st.st_mtime;
    s = (int) st.st_size;
    memcpy (buf, (char *) &t, 4);
    memcpy (buf + 4, (char *) &s, 4);
    buf[8] = 0;
    return 1;
#endif
}

#ifdef COCO
/* getfd (path, buf): I$GetStt SS.FD, the first 16 bytes of the file
 * descriptor. Returns -1 on error. */
#asm
getfd pshs y,u
 lda 7,s
 ldb #$0F SS.FD
 ldx 8,s
 ldy #16
 os9 $8D I$GetStt
 bcc gfd1
 ldd #-1
 puls y,u,pc
gfd1 clra
 clrb
 puls y,u,pc
#endasm
#endif

static int
options (ZILCH)
{
    return 1 | (PragNoCase << 1) | (PragUndefZero << 2) | (PragExport << 3);
}

/* Where the defs may be: at the top of a module source, outside any
 * module, section or macro */

static int
placeok (ZILCH)
{
    return InTop && ! InModule && ! InSection && ! DoingMacro &&
           ! MacDefining && (CurntOrg == &AbsCtr) && (S_Addrs == &AbsCtr);
}

static void
#ifdef __STDC__
counter (SYMH h)
#else
counter (h)
     SYMH h;
#endif
{
    ++Count;
}

/* ******************************************************************** *
 * before() - Read the P records: is each of those symbols still as it  *
 *            was? In pass 1 there must be no others either.            *
 * ******************************************************************** */

static int
before (ZILCH)
{
    int c, n = 0;

    seek (PPos);

    while ((c = gc ()) == 'P')
    {
        if ( ! getsym () || ! same (sym_find (RName)))
        {
            return 0;
        }

        ++n;
    }

    Count = 0;

    if ( ! Pass2)
    {
        sym_walk (counter);
    }

    return Count == (Pass2 ? 0 : n);
}

/* ******************************************************************** *
 * readg() - Read the G record (its tag already read). Nothing changes  *
 *           until applyg(): the image may still be refused.            *
 * applyg() - The counters and the conditionals' log as reading the     *
 *           defs would leave them: pass 1 adds the conditionals to the *
 *           log, pass 2 skips them.                                    *
 * ******************************************************************** */

static void
readg (ZILCH)
{
    rd ((char *) G, sizeof (G));
    mark (&CondPos, 0);
}

static void
applyg (ZILCH)
{
    int i, c = 0;

    AbsCtr = Abs1;
    NextCtx += DNext;

    if (LMode)
    {
        LocalCtx = NextCtx;
    }

    ListLin += DList;
    _If_Dpth += DIf;    /* RMA's count: a false if's endc doesn't lower it */

    if (Pass2)
    {
        cond_skip (NCond);
        return;
    }

    GlobCnt += DGlob;
    seek (CondPos);

    for (i = 0; i < NCond; ++i)
    {
        if ((i & 7) == 0)
        {
            c = gc ();
        }

        cond_log ((c >> (i & 7)) & 1);
    }
}

/* ******************************************************************** *
 * files() - The F and T records, from FPos. check: is each file still  *
 *           the one recorded (same path, date and size)? Else do the   *
 *           defs' nam and ttl lines again (T): pass 1 sets a title     *
 *           only if it is still empty, pass 2 always (copy_to in       *
 *           directives.c). Returns the next record's tag, or -1 if a   *
 *           file changed.                                              *
 * ******************************************************************** */

static int
#ifdef __STDC__
files (int check)
#else
files (check)
     int check;
#endif
{
    char buf[128], st[9], st2[9];
    register char *t;
    int c;
    FILE *f;

    seek (FPos);

    while (((c = gc ()) == 'F') || (c == 'T'))
    {
        if (c == 'T')
        {
            t = (gc () == 'n') ? _nam : _ttl;
            gets0 (buf, sizeof (buf));

            if ( ! check && (Pass2 || ! *t))
            {
                strcpy (t, buf);
            }

            continue;
        }

        /* the use's name, the path it resolved to, the stamp */

        gets0 (buf, sizeof (buf));

        if (check)
        {
            if ( ! (f = u_find (buf, "r")))
            {
                return -1;
            }

            fclose (f);
        }

        gets0 (buf, sizeof (buf));
        rd (st, 9);

        if (check && (strcmp (buf, UsePath) || ! stamp (buf, st2) ||
            memcmp (st, st2, 9)))
        {
            return -1;
        }
    }

    return c;
}

/* ******************************************************************** *
 * load() - Pass 1: 1 if loaded, 0 if the image is for something else, *
 *          -1 if it is missing, unfinished or out of date (record a    *
 *          new one)                                                    *
 * ******************************************************************** */

static int
#ifdef __STDC__
load (char *use)
#else
load (use)
     char *use;
#endif
{
    char buf[128];
    int c = -1;
    SYMH h;

    if ( ! (Fp = fopen (ImgName, "r")))
    {
        return -1;
    }

    if ( ! rd (buf, 6) || strncmp (buf, DONE, 6))
    {
        goto out;
    }

    c = 0;

    if ((gc () != options ()) || ! gets0 (buf, sizeof (buf)) ||
        strcmp (buf, use))
    {
        goto out;
    }

    mark (&PPos, 0);

    if ( ! before ())
    {
        goto out;
    }

    mark (&FPos, 1);            /* before() read the next record's tag */

    if ((c = files (1)) != 'G')
    {
        c = -1;                 /* a file changed: record again */
        goto out;
    }

    readg ();
    c = 0;

    if (AbsCtr != Abs0)
    {
        goto out;
    }

    /* Good: create the symbols. Lo..Hi are the defs' handles. */

    seek (CondPos + ((NCond + 7) >> 3));
    Lo = bank_mark ();

    for (Hi = 0; gc () == 'S'; Hi = h + 1)
    {
        getsym ();
        setsym (h = sym_add (RName));
    }

    mark (&ZPos, 1);
    applyg ();
    files (0);
    c = 1;

  out:
    fclose (Fp);
    Fp = 0;
    return c;
}

/* ******************************************************************** *
 * Recording                                                            *
 * ******************************************************************** */

static void
#ifdef __STDC__
pname (SYMH h)
#else
pname (h)
     SYMH h;
#endif
{
    putsym ('P', h);
}

static void
#ifdef __STDC__
record (char *use)
#else
record (use)
     char *use;
#endif
{
    if ( ! ImgName)
    {
        Mem = 1;                /* nam/ttl seen: none yet */
        *bank_ptr (MemT = bank_alloc (2 * MEMT)) = 0;
        bank_ptr (MemT)[MEMT] = 0;
    }
    else if ( ! (Fp = fopen (ImgName, "w")))
    {
        return;
    }
    else
    {
        wr (MAGIC, 6);
        pc (options ());
        puts0 (use);
        mark (&PPos, 0);
        sym_walk (pname);
    }

    State = REC1;
    ImgRec = ImgOn = 1;
    ImgBase = ThisFile;
    Lo = bank_mark ();
    Abs0 = AbsCtr;
    Next0 = NextCtx;
    Local0 = LocalCtx;
    List0 = ListLin;
    Glob0 = GlobCnt;
    Cond0 = cond_count ();
    If0 = _If_Dpth;
    Err0 = HadError;
}

/* img_bad() - Give up recording: something the image can't hold */

void
img_bad (ZILCH)
{
    if (Fp)
    {
        fclose (Fp);
        unlink (ImgName);
        TRACE ("not recorded");
    }

    Fp = 0;
    ImgRec = ImgOn = Mem = 0;
    ImgBase = 0;
    State = IDLE;
}

/* ******************************************************************** *
 * img_touch() - symtab.c: symbol h was changed                         *
 * ******************************************************************** */

void
#ifdef __STDC__
img_touch (SYMH h)
#else
img_touch (h)
     SYMH h;
#endif
{
    int i;

    if ((h < Lo) || ((State != REC1) && (h >= Hi)))
    {
        if ((State == REC1) || (State == REC2))
        {
            img_bad ();         /* the defs changed an older symbol */
        }
        else if (Mem && (h < Lo))
        {
            Touched = 1;        /* a symbol from before the use changed */
        }

        return;
    }

    if (State == REC2)
    {
        for (i = 0; (i < NDiff) && (Diffs[i] != h); ++i)
        {
        }

        if (i == DIFFMAX)
        {
            img_bad ();
        }
        else if (i == NDiff)
        {
            Diffs[NDiff++] = h;
        }
    }
    else if (State != REC1)
    {
        Touched = 1;            /* the source changed a defs symbol */
    }
}

/* img_op() - AsmblLin, while recording: the line's operation */

void
#ifdef __STDC__
img_op (char *op)
#else
img_op (op)
     char *op;
#endif
{
    char w[12];
    register char *p = w;
    int n;

    *p++ = ' ';

    while (*op && (p < w + 10))
    {
        *p++ = _tolower (*op++);
    }

    *p++ = ' ';
    *p = '\0';
    n = p - w;

    for (p = Ops; p[1] && strncmp (p, w, n); p = strchr (p + 1, ' '))
    {
    }

    if (*op || ! p[1])
    {
        img_bad ();
    }
    else if ((w[1] == 'n' && w[2] == 'a') || (w[1] == 't' && w[2] == 't'))
    {
        if (Mem)                /* pass 2 leaves the last of each */
        {
            p = bank_ptr (MemT) + ((w[1] == 't') ? MEMT : 0);
            *p = 1;
            strncpy (p + 1, SrcChar, MEMT - 2);
            p[MEMT - 1] = '\0';
            return;
        }

        pc ('T');               /* nam/ttl: the operand, as copy_to takes it */
        pc (w[1]);
        puts0 (SrcChar);
    }
}

/* img_file() - do_use, while recording: a use opened UsePath */

void
#ifdef __STDC__
img_file (char *use)
#else
img_file (use)
     char *use;
#endif
{
    char st[9];

    if (Mem)
    {
        return;                 /* the files don't change during a run */
    }

    if ( ! stamp (UsePath, st))
    {
        img_bad ();
        return;
    }

    pc ('F');
    puts0 (use);
    puts0 (UsePath);
    wr (st, 9);
}

/* End of the defs in pass 1: write G and S */

static void
endrec1 (ZILCH)
{
    int i, c;
    SYMH h;

    ImgRec = 0;

    if (FalseIf || (HadError != Err0) || ! placeok ())
    {
        img_bad ();
        return;
    }

    Abs1 = AbsCtr;
    DNext = NextCtx - Next0;
    DList = ListLin - List0;
    DGlob = GlobCnt - Glob0;
    DIf = _If_Dpth - If0;
    NCond = cond_count () - Cond0;
    LMode = (LocalCtx != Local0);

    if (LMode && (LocalCtx != NextCtx))
    {
        img_bad ();
        return;
    }

    if ( ! Mem)
    {
        pc ('G');
        wr ((char *) G, sizeof (G));

        for (i = 0, c = 0; i < NCond; ++i)
        {
            c |= cond_bit (Cond0 + i) << (i & 7);

            if (((i & 7) == 7) || (i == NCond - 1))
            {
                pc (c);
                c = 0;
            }
        }
    }

    Hi = bank_mark ();

    for (h = bank_after (Lo, 0); h < Hi; h = sym_after (h))
    {
        if (sym_refs (h))
        {
            img_bad ();
            return;
        }

        if ( ! Mem)
        {
            putsym ('S', h);
        }
    }

    State = RECWAIT;
    Touched = 0;
}

/* End of the defs in pass 2: write Z and finish the file */

static void
endrec2 (ZILCH)
{
    int i;

    if ((AbsCtr != Abs1) || FalseIf || (HadError != Err0))
    {
        img_bad ();
        return;
    }

    for (i = 0; i < NDiff; ++i)
    {
        putsym ('Z', Diffs[i]);
    }

    pc ('E');
    seek (4L);
    pc ('1');
    i = fclose (Fp);
    Fp = 0;
    State = IDLE;
    ImgOn = 0;

    if (i)
    {
        unlink (ImgName);
    }

    TRACE ("recorded");
}

/* img_end() - DecSFil: the use being recorded has ended */

void
img_end (ZILCH)
{
    ImgBase = 0;

    if (State == REC1)
    {
        endrec1 ();
    }
    else
    {
        endrec2 ();
    }
}

/* img_pass() - read_fil: a pass starts */

void
img_pass (ZILCH)
{
    UseSeen = 0;

    if ( ! Pass2)
    {
        img_bad ();
    }
}

/* ******************************************************************** *
 * pass2() - Pass 2 reaches the use. Still good? A loaded image is      *
 *           applied (returns 1); a recording goes on to record what    *
 *           reading the defs again changes.                            *
 * ******************************************************************** */

/* mem2() - pass 2 with a record in memory: apply it if nothing it
 * depends on changed (returns 1); then it stays for the next pass */

static int
mem2 (ZILCH)
{
    register char *p;
    SYMH h;

    ImgOn = 0;

    if (Touched || ! placeok () || (AbsCtr != Abs0))
    {
        img_bad ();
        TRACE ("pass 2 read");
        return 0;
    }

    applyg ();

    for (h = bank_after (Lo, 0); h < Hi; h = sym_after (h))
    {
        if ((sym_msk (h) & (RELATIVE | DIRLOC)) == (RELATIVE | DIRLOC))
        {
            sym_setmsk (h, sym_msk (h) & ~DIRLOC);
        }
    }

    p = bank_ptr (MemT);

    if (*p)
    {
        strcpy (_nam, p + 1);
    }

    if (p[MEMT])
    {
        strcpy (_ttl, p + MEMT + 1);
    }

    ImgOn = 1;
    TRACE ("pass 2 applied the record");
    return 1;
}

static int
pass2 (ZILCH)
{
    FILE *w = Fp;               /* the image being written, if recording */
    int ok;

    if (Mem)
    {
        return mem2 ();
    }

    if (w)
    {
        fflush (w);
    }

    ImgOn = 0;
    ok = ! Touched && placeok () && (AbsCtr == Abs0) &&
         (Fp = fopen (ImgName, "r")) && before ();

    if (ok && ! w)
    {
        applyg ();
        files (0);
        seek (ZPos);

        while ((gc () == 'Z') && getsym ())
        {
            setsym (sym_find (RName));
        }
    }

    if (Fp)
    {
        fclose (Fp);
    }

    Fp = w;

    if ( ! w)
    {
        State = ok ? LOADED : IDLE;     /* again in the next pass */
        ImgOn = ok;
        TRACE (ok ? "pass 2 loaded" : "pass 2 read");
        return ok;
    }

    if ( ! ok)
    {
        img_bad ();
        return 0;
    }

    State = REC2;
    ImgOn = 1;
    NDiff = 0;
    ImgBase = ThisFile;
    Err0 = HadError;
    return 0;
}

/* ******************************************************************** *
 * img_use() - do_use: "use name" at ThisFile. Returns 1 if the image   *
 *             stands for the file (don't read it), 0 to read it.       *
 * ******************************************************************** */

int
#ifdef __STDC__
img_use (char *name)
#else
img_use (name)
     char *name;
#endif
{
    int r;

    if ((! ImgName && (ListFlg > 0)) || ! LwasmMode || UseSeen ||
        (ThisFile != FrstFil) || (LastSrc - SrcNam != 1) || preload_count ())
    {
        return 0;
    }

    UseSeen = 1;

    if (Pass2)
    {
        return (State == LOADED) || (State == RECWAIT) ? pass2 () : 0;
    }

    if ( ! placeok ())
    {
        return 0;
    }

    if ( ! ImgName)
    {
        record (name);          /* in memory, for the passes after */
        return 0;
    }

    r = load (name);
    TRACE (r > 0 ? "loaded" : (r ? "missing or out of date" : "other configuration"));

    if (r > 0)
    {
        State = LOADED;
        Touched = 0;
        ImgOn = 1;
        return 1;
    }

    if (r)
    {
        record (name);
    }

    return 0;
}
