#include "op.h"
#include "ctype.h"

/* does s match p, where '*' is any characters, '?' any one and the
 * rest themselves? (step 24) CoCoC's patmatch, which the 6809 used, is
 * the same test, but a call for every character after a '*' (6% of
 * dco68's time on te); Linux used fnmatch, where '[' also starts a
 * set, so an operand like [2,s] compared as a pattern (^1) differed
 * between the machines. One backtrack point is enough for '*' */
static
glob (p, s)
register char *p;
char *s;
{
	char *pstar, *sstar;

	pstar = NULL;
	for (;;) {
		if (*p == '*') {
			pstar = ++p;
			sstar = s;
		} else if (*s && (*p == '?' || *p == *s)) {
			++p;
			++s;
		} else if (*s == '\0' && *p == '\0') {
			return 1;
		} else if (pstar == NULL || *sstar == '\0') {
			return 0;
		} else {
			p = pstar;
			s = ++sstar;
		}
	}
}

match (s, p)
char *s,*p;
{
	if (p == NULL || glob (p, s)) {
		return 1;
	}

	while (*p) {
		if (*p == '<') {
			if (isdigit (*s)) {
				unsigned n = 0;
				do {
					n = n * 10 + *s++ - '0';
				} while (isdigit (*s));

				if (n > 255)
					return 0;
			} else return 0;
			p++;
		} else if (*s++ != *p++)
			return 0;
	}
	return (*s == '\0');
}


static char *
mcpy (dp, pp, source)
char *dp, *pp, *source;
{
	register char *sp = source;
	int skip, count;

	skip = count = 0;
	for ( ; *pp; pp++) {
		if (*pp == *sp) {
			skip++;
			continue;
		} else if (*pp == '?') {
			count++;
			continue;
		} else if (*pp == '*') {
			pp++;
			for (sp = source+skip+count; *sp; sp++) {
				if (strcmp (pp, sp)) {	/* rest identical? */
					count++;
					continue;
				} else goto done;
			}
			continue;
		} else {
			skip++;
		}
	}
done:
	strncpy (dp, source+skip, count);
	dp += count;
	*dp = '\0';
}


matchcpy (driver, src1, pat1, src2, pat2)
char *driver, *src1, *pat1, *src2, *pat2;
{
	char tmp[ARGSIZE];
	register char *dp = driver;
	char *tp = tmp;

	*tmp = '\0';
	while (*dp) switch (*dp) {
		default:
			*tp++ = *dp++;
			break;
		case '\\':
			dp++;
			if (!*dp) {
				error ("invalid reference");
			} else if (*dp == '1') {
				dp++;	/* skip past the 1 */
				tp = mcpy (tp, pat1, src1);
			} else if (*dp == '2') {
				dp++;	/* skip past the 2 */
				tp = mcpy (tp, pat2, src2);
			}
	}
	if (strlen(tmp))
		strcpy (src1, tmp);
}
