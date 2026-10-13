/*
	$Id$
*/
#include "prep.h"
#ifdef UNIX
#include <strings.h>
#endif
#include <errno.h>


#define MAXIFS 32

static short nextncnt;


char *
nextname(fname, delimiter)
char *fname;
char delimiter;
{
	static char nambuf[100];

	if (delimiter == '"' && nextncnt < 0) {
		nextncnt = 0;
		return strcpy(nambuf, fname);
	}

	if (nextncnt < inclcount) {
		sprintf (nambuf, "%s/%s", incl[nextncnt++], fname);
		return nambuf;
	} else return NULL;
}


/* -h=image: the headers dcc68's header image (dcc/CC09/pch.c) was made
 * of. Their text isn't passed on, because dcc68 loads their declarations
 * from the image; a header the image doesn't hold is passed on as usual.
 * Each skipped header's text must be what the image was made from (its
 * checksum), or the file is compiled without the image. */

static char **hnames;
static unsigned *hsums, *hgot;	/* each header's checksum: image's, ours */
static char *hseen;
static int nhnames, hcur = -1;

/* a header doesn't match the image: say so and stop with IMGMISS, which
 * tells dcc to compile this file without the image (nothing is wrong
 * with the file itself) */
static
imgmiss(msg)
char *msg;
{
	fprintf(stderr, "cpp: %s\n", msg);
	errexit(IMGMISS);
	return 0;
}

static unsigned
rdimg16(fp)
register FILE *fp;
{
	register unsigned h;

	h = getc(fp) << 8;
	return h | (getc(fp) & 0xff);
}

readhdrs(name)
char *name;
{
	register FILE *fp;
	register int i, n;
	char *p;

	if ((fp = fopen(name, "r")) == NULL
			|| rdimg16(fp) != ('D' << 8 | 'C') || rdimg16(fp) != ('I' << 8 | '1')) {
		fprintf(stderr, "cpp: can't read header image %s\n", name);
		errexit(FAILURE);
	}
	fseek(fp, (long) rdimg16(fp), 0);
	nhnames = getc(fp);
	hnames = (char **) grab(nhnames * sizeof(char *));
	hsums = (unsigned *) grab(nhnames * sizeof(unsigned));
	hgot = (unsigned *) grab(nhnames * sizeof(unsigned));
	hseen = (char *) grab(nhnames);
	for (i = 0; i < nhnames; ++i) {
		n = getc(fp);
		hnames[i] = p = (char *) grab(n + 1);
		while (n--) *p++ = getc(fp);
		*p = '\0';
		hsums[i] = rdimg16(fp);
		hgot[i] = hseen[i] = 0;
	}
	fclose(fp);
}

/* The checksum of a header's text, a line at a time; the same function
 * as dcc68w's (dcc/CC09/pch.c), which wrote the image's. Blank lines
 * don't count, so a header that a guard empties adds nothing. */
static unsigned
hdrsum(sum, s)
register unsigned sum;
register char *s;
{
	register char *p;

	for (p = s; *p == ' ' || *p == '\t'; ++p) ;
	if (!*p) return sum;
	while (*s) sum = (((sum << 1) | (sum >> 15 & 1)) ^ (*s++ & 0xff)) & 0xffff;
	return ((sum << 1) | (sum >> 15 & 1)) & 0xffff;
}

/* a header's line (main.c): if the image holds the header, add the line
 * to its checksum and say so (1: not passed on); 0: pass it on */
hdrline(s)
char *s;
{
	register int i;

	if (hcur < 0 || strcmp(filename, hnames[hcur])) {
		for (i = 0; i < nhnames && strcmp(filename, hnames[i]); ++i) ;
		if (i == nhnames) return 0;	/* not in the image: read as usual */
		hcur = i;
	}
	hseen[hcur] = 1;
	hgot[hcur] = hdrsum(hgot[hcur], s);
	return 1;
}

/* at the end: every header must have given the image's text */
hdrcheck()
{
	register int i;
	char msg[LINESIZE];

	for (i = 0; i < nhnames; ++i)
		if (hseen[i] && hgot[i] != hsums[i]) {
			sprintf(msg, "header differs from image: %s", hnames[i]);
			imgmiss(msg);
		}
	return 0;
}


doinclude()
{
	register char *nameptr, c;
	char fname[64], *last;
	register FILE *fp;
	int lasterr;

	if (process) {							/* ok to do this? */
		skipsp(1);							/* skip spaces */
		symptr = lptr;
		gch(KEEPSP);
		c = cch;
		nameptr = fname;
		if (c == '"' || c == '<') {
			gch(SKIPSP);
			if (c == '"') nextncnt = -1;
			else {
				c = '>';
				nextncnt = 0;
			}
			while (cch && cch != c) {
				*nameptr++ = cch;
				gch(SKIPSP);
			}
			*nameptr = '\0';
			skipsp(1);	/* get rid of (possible) comment */
			for (last = fname; nameptr = nextname(fname, c); last = nameptr) {
				if (fp = fopen(nameptr, "r")) {
					register include *nptr;

					nptr = (include*)grab(sizeof(include));
					nptr->next = inclptr;
					nptr->lno = lineno;
					nptr->fp = in;
					strcpy(nptr->fname, filename);
					strcpy(nptr->modname, modname);
					setfile(strcpy(filename, nameptr));
					strcpy(modname, makename(nameptr));
					putesc(NEWFNAME, filename, modname);
					inclptr = nptr;

					putesc(NEWLINO, 0);
					setline(lineno = 0);
					in = fp;
					return 0;
				} else lasterr = errno;
			}
			sprintf (fname, "can't open %s (err=%d)", last, lasterr);
			fatal (fname);
		} else fatal("incorrect include file syntax");
	}
}


dodefine(func)
int func;
{
	char name[NAMESIZE];

	if (process) {
		skipsp(1);
		gch(KEEPSP);
		if (isalpha(cch) || cch == '_') {
			getword(name, NAMESIZE);
			if (func == DEFINE) {
				skipsp(1);
				/*
				 * ANSI allows "redefinition" if it's not really different,
				 * which will take some hacking...for now, we'll leave it
				 * alone.
				 */
				if (findmac(name)) {
					lwarning("redefined macro");
				} else if (strcmp(name, "defined") == 0) {
					lerror("can't #define defined");
				} else {
					addmac(name, 0);
				}
			} else {
				delmac(name);
			}
		} else {
			lerror("illegal macro name");
		}
	}
}


int	iftop, elseflag;
static struct
{
	int		oldproc, oldelse, hitaltern;
} ifstack[MAXIFS];


docond()
{
	if (iftop < MAXIFS) {
		/*
		 * skipsp(1);
		 * gch(SKIPSP);
		 */
		symptr = lptr;
		ifstack[iftop].oldelse = elseflag;
		if (ifstack[iftop].oldproc = process) {
			ifstack[iftop].hitaltern = process = eval();
		} else {
			ifstack[iftop].hitaltern = FALSE;
		}
		iftop++;
		elseflag = TRUE;
	} else {
		lerror("#if nesting too deep");
	}
}


doifdef(_bool)
{
	char name[NAMESIZE];
	
	if (iftop < MAXIFS) {
		skipsp(1);
		gch(SKIPSP);
		symptr = lptr;
		if (isalpha(cch) || cch == '_') {
			getword(name, NAMESIZE);
			ifstack[iftop].oldelse = elseflag;
			if (ifstack[iftop].oldproc = process)
				ifstack[iftop].hitaltern = process = _bool ^ !findmac(name);
			else
				ifstack[iftop].hitaltern = FALSE;
			iftop++;
			elseflag = TRUE;
		} else {
			lerror("illegal #if macro name");
		}
	} else {
		lerror("#if nesting too deep");
	}
}


doelif()
{
	if (elseflag) {
		/*
		 * skipsp(1);
		 * gch(SKIPSP);
		 */
		symptr = lptr;
		if (ifstack[iftop - 1].oldproc && !ifstack[iftop - 1].hitaltern)
			ifstack[iftop - 1].hitaltern = process = eval();
		else
			process = FALSE;
	} else {
		lerror("no #if for #elif");
	}
}


doelse()
{
	if (elseflag) {
		process = ifstack[iftop - 1].oldproc && !ifstack[iftop - 1].hitaltern;
		elseflag = FALSE;
	} else {
		lerror("no #if for #else");
	}
}


unstack()
{
	if (iftop) {
		process = ifstack[--iftop].oldproc;
		elseflag = ifstack[iftop].oldelse;
	} else {
		lerror("too many #endifs");
	}
}


doliner()
{
	register int i;
	char name[NAMESIZE];

	if (process) {
		skipsp(1);
		gch (KEEPSP);
		getword(name, NAMESIZE);
		if (i = atoi(name)) {
			setline(lineno = i);
		}
		skipsp(1);
		gch (KEEPSP);
		getfile(name, NAMESIZE);
		if (*name)
		{
			setfile(strcpy(filename, name));
			strcpy(modname, makename(filename));
		}
		putesc(NEWFNAME, filename, modname);
	}
}


dowarning()
{
	char			lin[LINESIZE];
	register char	*sptr;

	if (process) {
		skipsp(1);
		gch (KEEPSP);
		sptr = lin;
		while (cch && cch != '\n') {
			*sptr++ = cch;
			gch (KEEPSP);
		}
		*sptr = '\0';
		warning(lin);
	}
}


doerror()
{
	char			lin[LINESIZE];
	register char	*sptr;
	if (process) {
		skipsp(1);
		gch (KEEPSP);
		sptr = lin;
		while (cch && cch != '\n') {
			*sptr++ = cch;
			gch (KEEPSP);
		}
		*sptr = '\0';
		lerror(lin);
	}
}
