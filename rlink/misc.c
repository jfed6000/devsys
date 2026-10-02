#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifndef _OS9
#   include <unistd.h>
#endif
#include "rlink.h"

direct struct ext_ref *ExtStart;

/*int    *D0164[64];
char *InFile;
char *OutfName;*/
extern struct ext_ref   curExtRef;
SYMDAT  *AllSyms[371];
extern direct int   printsym;
extern direct UINT t_code, t_idat, t_udat,  t_idpd,  t_udpd;

#ifdef _OS9
struct ext_ref *findext();
char *grab();
#endif

int
hash (word)
register char *word;
{
    register int n = 0;

    while (*word)
    {
        n += *(word++);
    }

    return (n % 371);
}

/* Search for symbol with given name
 * Returns it if found, NULL if not */

SYMDAT *
lookup (name)
char name[];
{
	register SYMDAT *tab;

	for (tab = AllSyms[hash (name)]; tab; tab = tab->snext) {
		if (name[0] == tab->sname[0] && strcmp (name, tab->sname) == 0)
			return tab;
	}

	return NULL;
}

/*
 * Add a symbol to the table -- "install"?
 */
int
L1bcf(symbl)
    SYMDAT *symbl;
{
    int idx;

    if (!lookup(symbl->sname))        /* else L1c1f */
    {
        idx = hash(symbl->sname);
        symbl->snext = AllSyms[idx];
        AllSyms[idx] = symbl;

        if (findext(symbl->sname, 0))
        {
            symbl->Type |= 0x100;
        }

        return 0;
    }

    return 1;
}

int
fatal (msg, s1, s2, s3)
char *msg;
{
    int errcode = errno;

    fprintf (stderr, "linker fatal: ");
    fprintf (stderr, msg, s1, s2, s3);
    fprintf (stderr, "\n");
    lw_cleanup ();
    exit (errcode ? errcode : (errcode = 1));
}

int
req_mem (linpt)
register char *linpt;
{
    int curchr;
    unsigned ttl = 0;

	/* cheap atoi */
    while (isdigit((curchr = *(linpt++))))
    {
        ttl = (ttl * 10) + (curchr - '0');
    }

    if ((curchr == 'k') || (curchr == 'K'))
    {
        ttl *= 4;
    }

    return ttl << 8;
}

/* Pull ref out of list and place in base of ExStart */

ref2base (thisref)
    register struct ext_ref *thisref;
{
    thisref->prev->next = thisref->next;
    thisref->next->prev = thisref->prev;

    if (thisref != &curExtRef)
    {
        thisref->next = ExtStart;
        ExtStart = thisref;
    }
}

struct ext_ref *
findext (srchname, findonly)
    char *srchname; int findonly;
{
    struct ext_ref *reftmp = 0;
    register struct ext_ref *listref = curExtRef.next;

    while (listref != &curExtRef)
    {
        if (listref->name[0] == srchname[0])   /* else L1d51 */
        {
            if ((strcmp (listref->name, srchname) == 0))  /* else L1d51 */
            {
                if (findonly)  /* else L1d46 */
                {
                    return listref;
                }
                else
                {
                    /* Pull this out of curExtRef and move to ExtStart */
                    reftmp = listref->next; /* Save place in search list */
                    ref2base (listref);
                    listref = reftmp;
                }
            }
        }

        listref = listref->next;
    }       /* L1d53 */

    return reftmp;
}

/* Find ref
 * If not in curExtRef, put it there */

struct ext_ref *
L1d61(nam)
    char *nam;
{
    register struct ext_ref *eref;

    /* If in curExtRef, return it */
    if ((eref = findext(nam, 1)))
        return eref;

    /* If not found, get ExtStart Item or create one if none */
    if (ExtStart)
    {
        eref = ExtStart;
        ExtStart = eref->next; /* Move next item to base */
    }
    else
    {
        /* Released entries go onto ExtStart, and rl_05.c takes nodes
         * from there as dref_nodes, so each must be big enough for one */
        eref = (struct ext_ref *)grab(sizeof(struct ext_ref) >
                sizeof(struct dref_node) ? sizeof(struct ext_ref)
                : sizeof(struct dref_node));
    }

    /* Set up curExtRef pointers */
    eref->prev = &curExtRef;
    eref->next = curExtRef.next;
    return (curExtRef.next = curExtRef.next->prev = eref);
    /*curExtRef.next = eref;*/
    /*return eref;*/
}

char *
grab (size)
unsigned	size;
{
	register char *p;

#ifdef __unix
	if (size & 1) ++size;			/* a certain OS pukes on odd requests */
#endif
#ifdef _OSK
	if ((p = (char*) ebrk(size)) == -1)
#else
	/* malloc on OS-9 too (it was sbrk): lw_select's index, freed
	 * before linking, is then reused; memory freed to malloc is lost
	 * to sbrk. Native --lwlink links of play ran out without it */
	if ((p = (char*) malloc(size)) == NULL)
#endif
	{
		if (obmemreq)
			fprintf (stderr, "need %d bytes for linkbuf\n", obmemreq);
		fatal("out of memory");
	}

	/* rlink expects new structures to start zeroed (e.g. OBFILES's
	 * obn link); malloc and sbrk don't promise that. Under
	 * AddressSanitizer every link crashed reading such a field. */
	memset (p, 0, size);
	return p;
}


/* ******************************************************************** *
 * setname() - Store a name for a SYMDAT or ext_ref at its own length.  *
 *          old is the entry's current name: an entry rlink is reusing  *
 *          keeps that buffer when the new name fits. Returns the name. *
 * ******************************************************************** */

char *
setname (old, name)
char *old, *name;
{
    int len = strlen (name);

    if (len > SYMLEN)
        len = SYMLEN;

    if ( ! old || (strlen (old) < len))
    {
        if (old)
            free (old);         /* a recycled SYMDAT's shorter name */

        old = grab (len + 1);
    }

    strncpy (old, name, len);
    old[len] = '\0';
    return old;
}


char *
get_loc (p1)
    int p1;
{
    switch (p1 & 7)
    {
        case CODENT:     /* L1df9 */
            return "code";
        case 0:     /* L1dff */
            return "udat";
        case INIENT:            /* L1e05 */
            return "idat";
        case DIRENT:            /* L1e0b */
            return "udpd";
        case INIENT | DIRENT:   /* L1e11 */
            return "idpd";
        case CODENT | CONENT:   /* L1e17 */
            return "cnst";
        case CODENT | CONENT | SETENT:     /* L1e1d */
            return "comm";
        default:            /* L1e23 */
            return "???";
    }
}

direct char *D0005 = "     %-9s %s %04x\n";

int
writemap ()
{
    OBFILES *tt; /*Struct */

    printf ("Linkage map for %s  File - %s", modname, OutfName);
    puts ("\n\nSection          Code IDat UDat IDpD UDpD File\n");
    tt = FirstMod;

    while (tt)
    {
        if (tt->x19 & 0x100)
        {
            printf("%-16s %04x %04x %04x %02x   %02x %s\n",
                    tt->obname, tt->codesiz, tt->IDat, tt->UDat,
                    tt->IDpD, tt->UDpD, tt->roffile);

            if (printsym) /* else L1edb */
            {
                register SYMDAT *sd = tt->symlist;

                while (sd)
                {
                    printf(D0005, sd->sname, get_loc(sd->Type), sd->Offset);
                    sd = sd->sprev;
                }   /* L1ed7 */
            }
        }

        tt = tt->obn;
    }       /* L1ee0 */

    if (commnsyms)  /* else L1f13 */
    {
        register SYMDAT *rsd;

        printf("Common blocks:\n");
        rsd = commnsyms;

        while (rsd)
        {
            printf(D0005, rsd->sname, get_loc(rsd->Type), rsd->Offset);
            rsd = rsd->sprev;
        }
    }

    puts("                 ---- ---- ---- --");
    printf("                 %04x %04x %04x %02x  %02x\n",
            t_code, t_idat, t_udat,  t_idpd,  t_udpd);
}

int
unrsolved()
{
    register struct ext_ref *sd;

    if (curExtRef.next != &curExtRef)    /* else L1f9c */
    {
        fprintf(stderr, "Unresolved references:\n");
        sd = curExtRef.next;

        while (sd != &curExtRef)
        {
            if (sd->myOb)       /* may be unset: don't crash reporting */
            {
                fprintf(stderr, " %-16s %-16s in %-16.16s\n",   /* L219d */
                        sd->name, sd->myOb->obname, sd->myOb->roffile);
            }
            else
            {
                fprintf(stderr, " %s\n", sd->name);
            }
            sd = sd->next;
        }

        return 1;
    }

    return 0;
}

getname (s, max)
register char *s; int max;
{
	int tmp;

    while (tmp = getc (InFile)) {
        if (max) {
            --max;
            *s++ = tmp;
        }
    }

    if (ferror (InFile))
    {
        in_rderr();
    }

    *s = '\0';
}

char *
_fgets (dest, fp)
    register char *dest; FILE *fp;
{
    int tmp;

    while (((tmp = getc(fp)) != '\r') && (tmp != EOF))
    {
        *(dest++) = tmp;
    }

    if (tmp == EOF)
    {
        return 0;
    }

    *dest = '\0';
    return dest;
}


tidy ()
{
	int errcode = errno;

	if (OutFile) {
		fclose (OutFile);
		unlink (OutfName);
	}

	lw_cleanup ();
	exit (errno);
}
