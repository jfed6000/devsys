/* ************************************************ *
 * Part2 of c source for "rma"                      *
 * $Id::                                        $   *
 * ************************************************ */

#include "rma.h"

/* **************************************************************** *
 * FindCmd - parses one of the CODFORM lists for a match of a cmd   *
 * returns: address of CODFORM if found,   NULL if no match         *
 * **************************************************************** */

CODFORM *
#ifdef __STDC__
FindCmd (char *src_str, CODFORM *cod_strt, CODFORM *cod_stop)
#else
FindCmd (src_str, cod_strt, cod_stop)
     char *src_str,
          *cod_strt,
          *cod_stop;
#endif
{
    CODFORM *cod_ptr;
    register char *cmp_str = CmdMnemonic;

    /* Copy lower-case chars from src_str to cmp_str */

    while (*src_str != '\0')
    {
        if (islower ((int)*src_str))
        {
            *cmp_str = *src_str;
        }
        else
        {
            *cmp_str = (_tolower (*src_str));
        }

        ++cmp_str;
        ++src_str;
    }

    *cmp_str = '\0';
    cod_ptr = cod_strt;
    
    while (cod_ptr < cod_stop)
    {
        cmp_str = cod_ptr->ccmd;
        CmdNamEnd = CmdMnemonic;
    
        while (*cmp_str)
        {
            if (*cmp_str != *CmdNamEnd++)
            {
                break;
            }
        
            ++cmp_str;
        }

        if (*cmp_str == '\0')
        {
            return cod_ptr;
        }
        
        ++cod_ptr;
    }

    return 0;
}
