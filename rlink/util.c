#include <stdlib.h>
#include "rlink.h"

#ifdef _OS9
# define mgw getw
#else
INTTYPE
mgw (inf)
FILE *inf;
{
	unsigned char high = getc (inf);
	unsigned char low = getc (inf);
	return (high << 8) + low;
}
#endif

getref (out, name)
def_ref *out;
char *name;
{
#ifdef _OS9
	fread (out, sizeof (def_ref), 1, InFile);
#else
	out->r_flag = getc (InFile);
	out->r_offset = mgw (InFile);

	if (ferror (InFile)) rderr (name);
#endif
}


int
gethead (out)
binhead *out;
{
#ifdef _OS9
	return fread (out, sizeof (binhead), 1, InFile);
#else
	if (fread (out, 4, 1, InFile) == 0)
		return 0;

	out->h_tylan = mgw (InFile);
	out->h_valid = getc (InFile);
	fread (out->h_date, 5, 1, InFile);
	out->h_edit = getc (InFile);
	out->h_vers = getc (InFile);
	out->h_glbl = mgw (InFile);
	out->h_dglbl = mgw (InFile);
	out->h_data = mgw (InFile);
	out->h_ddata = mgw (InFile);
	out->h_ocode = mgw (InFile);
	out->h_stack = mgw (InFile);
	out->h_entry = mgw (InFile);

	return 1;
#endif
}


rderr (s)
char *s;
{
    fatal ("error reading file %s", s);
}


wrerr()
{
	if (ferror (OutFile))
		fatal ("error writing file %s", OutfName);
}
