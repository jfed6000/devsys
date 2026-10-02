
/* **************************************************************** *
 * Part9.c - for "RMA"                                              *
 * Most of these seem to be MACRO - related                         *
 *                                                                  *
 * $Id::                                                        $   *
 * **************************************************************** */

/* The "rma.tmp" file.  Macro definitions are held in a temporary file
 * in the current directory.  It is basically a text file, except for
 * null separators.
 * Structure of the file:
 * the macro name - (null-terminated)
 * The text from te source file defining the macro, copied verbatim with
 * no line-end conversions or anything
 * A final NULL, signifying the end of that macro.
 * This is repeated for each macro defined.
 */

#ifndef COCO
#   include <unistd.h>
#endif

#define NEED_CTYPE
#include "rma.h"

         /*   "place" for fseek     */
#define BEGIN 0
#define HERE 1
#define END 2

struct macro_ptr
{
    FILE *myfileptr;
    long PrevMpos;
    int lblno;
    char *params;
};

#define MAX_MAC 8

/* Stack for Recursive macro calls. ( Max 8 allowable) */
static struct macro_ptr MacroStk[MAX_MAC];

direct MACDSCR *NowMac;
direct int d00d8,
  d00da;
direct FILE *macfp1,
            *MacFile; /* I _think_ this will always = macfp1 */
static direct int   MacLblNbr;
static direct char *d00e2;
static direct int   MacDpth;            /* D00e4 */
static direct char *MacStrPtr;
    /*  MacArgBuf is a list of the arguments for current macro.
     *  It is defined as char[], and the first byte = arg count,
     *  followed by a list of args, each terminated by a null */
static direct char *MacArgBuf;        /* D00e8        */

static char *MovMacArg ();

static void putfield (
#ifdef __STDC__
    char *, int
#endif
);

static void clrargptr (
#ifdef __STDC__
    char **
#endif
);

static void mac_esc (
#ifdef __STDC__
    int
#endif
);

static int gArgParm (
#ifdef __STDC__
    int m_count, int parm2
#endif
);

static void mfilerr();

/* ******************************************************************** *
 * createMac() - Creates a new Macro descriptor.  If tempfile has never *
 *            been created, create it, or if it already exists, open    *
 *            if for append.                                            *
 * ******************************************************************** */

int
createMac (ZILCH)    /* l3183 */
{
    register MACDSCR *_newmac;
    char *_namptr;

    if (PASS1)
    {
        /* If tmpfile is not open, open or create it now */

        if ( ! macfp1)
        {
            if ( ! (MacFile = macfp1 = fopen (TmpNam,
#ifdef WIN32
                            "wb+"
#else
                            "w+"
#endif
                            )))
            {
                errexit ("can't open macro work file");
            }
            /* Let's forget this for now.. can't seem to understand how it's
             * done in Linux...
             */
#ifdef COCO
            macfp1->_bufsiz = 512;
#endif
        }

        /* Allocate a MACDSCR */

        _newmac = (MACDSCR *) getmem (sizeof (MACDSCR));

        /* let new MACDSCR be NowMac and point new->LastMac to old NowMac */

        _newmac->LastMac = NowMac;
        NowMac = _newmac;
        _newmac->MSblPtr = CurntLabel;
        _newmac->MWrkFil = macfp1;
        fseek (macfp1, 0l, END);   /* might be appending to an existing file */
        _namptr = CurntLabel->symblnam;

        /* Write macro name to tmpfile */

        while (*_namptr)
        {
            putc (*(_namptr++), macfp1);
        }

        putc ('\0', macfp1);

        if (ferror (macfp1))
        {
            mfilerr ();
        }
        
        _newmac->MacStart = ftell (macfp1);/* Begin of data (following name) */
    }

    return 1;
}

/* ******************************************************************** *
 * mcWrtLin() - Write an assembly command line to mac tmpfile           *
 *              using global data vars Label, Operatr, SrcChar          *
 *              followed by a newline                                   *
 * ******************************************************************** */

int
mcWrtLin (ZILCH)
{
    if (PASS1)
    {
        putfield (Label, ' ');
        putfield (Operatr, ' ');
        putfield (SrcChar, '\n');
    }

    return 1;
}

/* ******************************************************************** *
 * putfield() - Writes to the mac tmpfile the string passed as a        *
 *              parameter (1) followed by the whitespace character      *
 *              provided in parameter (2)                               *
 * ******************************************************************** */

static void
#ifdef __STDC__
putfield (char *strng, int whitespc)      /* l324c() */
#else
putfield (strng, whitespc)
     char *strng;
     char whitespc;
#endif
{
    register char *_curchr;

    if ((_curchr = strng))
    {
        while (*_curchr != '\0')
        {
            putc (*(_curchr++), macfp1);
        }
    }

    putc (whitespc, macfp1);
    
    if (ferror (macfp1))
    {
        mfilerr ();
    }
}

int
add_nul (ZILCH)
{
    if (PASS1)
    {
        putc ('\0', macfp1);
    }

    return 1;
}

/* **************************************************************** *
 * McNamCmp() - Beginning with macdscr stored in NowMac, work back  *
 *              through the list searching for a match of the       *
 *              name string provided as a parameter with the names  *
 *              in the symboldscr for that macro.  If a match is    *
 *              found return the MACDSCR, else return NULL          *
 * **************************************************************** */

MACDSCR * 
#ifdef __STDC__
McNamCmp (char *srchname)
#else
McNamCmp (srchname)
     char *srchname;
#endif
{
    register MACDSCR *_curntmac = NowMac;

    while (_curntmac)
    {
        /* First simply compare first char of names */
        if (srchname[0] == (_curntmac->MSblPtr)->symblnam[0])
        {
            if (strcmp (srchname, ((_curntmac->MSblPtr)->symblnam)) == 0)
            {
                return _curntmac;
            }
        }

        _curntmac = _curntmac->LastMac;
    }

    return 0;
}

/* **************************************************************** *
 * pushMac() - Adds a new macro onto MacroStk                       *
 *      Sets fields in the macro descriptor                         *
 *      errors out if macro depth exceeds max of 8                  *
 * **************************************************************** */

void
#ifdef __STDC__
pushMac (MACDSCR * newmac)
#else
pushMac (newmac)
     MACDSCR *newmac;
#endif
{
    register struct macro_ptr *_nxtmac = &MacroStk[MacDpth++];

    if (MacDpth > MAX_MAC)
    {
        errexit ("macro nesting too deep");
    }

    _nxtmac->myfileptr = MacFile;       /* Always the same, I think */
    _nxtmac->PrevMpos = ftell (MacFile);/* Save curnt pos. for return */
    _nxtmac->lblno = MacLblNbr;
    _nxtmac->params = d00e2;        /* params to pass to new macro */
    MacFile = newmac->MWrkFil;      /* Never changes ??? */
    fseek (MacFile, newmac->MacStart, BEGIN);
    
    if (MacDefining == 0)   /* Probably always ? */
    {
        ++d00da;
    }
    
    MacLblNbr = d00da;
    d00e2 = MovMacArg ();   /* Read macro parameter defs return string */
    DoingMacro = 1;
}

/* **************************************************************** *
 * pullMac() - Pulls a macro descriptor off the MacroStk stack       *
 *          and sets the macdscr below it to proper status          *
 * Errors out if attempting to pull off the only remaining macdscr  *
 * **************************************************************** */

static void
pullMac (ZILCH)
{
    register struct macro_ptr *_upmac = &MacroStk[--MacDpth];

    if (MacDpth < 0)
    {
        errexit ("asm err: macro nest");
    }

    MacFile = _upmac->myfileptr;
    fseek (MacFile, _upmac->PrevMpos, BEGIN);
    MacLblNbr = _upmac->lblno;
    clrargptr ((char **)d00e2); /* Cast to avoid warning */
    d00e2 = _upmac->params;
}

/* ******************************************************************** *
 * getmacline() - Retrieves a line from macro file.  This function is   *
 *                called prior to attempting to read the current        *
 *                sourcefile, and 0 is returned if not reading macro.   *
 * Returns: 1 - on successful read of a line, newline converted to NULL *
 *          0 - If END-OF-MACRO (NULL) or EOF.  In either case, pull    *
 *              pull macro dscr off stack and dec counters              *
 * ******************************************************************** */

int
getmacline (ZILCH)
{
    char _macchr;

    while (DoingMacro)
    {
        MacStrPtr = SrcChar = InpBuf;

        /* Read line from macro file until EOF, null, or a newline */

        while ((_macchr = getc (MacFile)) > 0)  /* END of MACRO or EOF */
        {
            if (_macchr == '\n')    /* End of line? */
            {
                *MacStrPtr = '\0';  /* Convert to null */

                if (ListMacs)       /* Inc line # if listing macros */
                {
                    ++ListLin;
                }

                return 1;
            }

            if ((FalseIf == 0) && (_macchr == '\\'))
            {
                mac_esc (getc (MacFile));
            }
            else    /* Regular char, store it */
            {
                *(MacStrPtr++) = _macchr;
            }
        }       /* end while (getc(macfp1)) */

        /* If we get here, we are at EOF (end of macro) so we pull off stack.
         * First, be sure that MacDpth is not 0, and if so, exit
         */

        if (MacDpth == 0)
        {
            DoingMacro = 0;
            break;
        }
        
        pullMac ();
    }

    return 0;
}

/* ******************************************************************** *
 * mac_esc() - Handle macro escape sequences                            *
 *         Writes converted label name or value represented by the      *
 *         escaped char to MacStrPtr and updates MacStrPtr              *
 *         If the char is not a defined escape char, pass it on to      *
 *         string as-is.                                                *
 * ******************************************************************** */

static void
#ifdef __STDC__
mac_esc (int mychr)
#else
mac_esc (mychr)
     char mychr;
#endif
{
    if (isdigit (mychr))    /* "\{1-9}" - Parameter number */
    {
        gArgParm (mychr, 1);        /* Copy label "mychr" to MacStrPtr */
    }
    else
    {
        switch (mychr)
        {
            case '@':   /* "\@" - Label */
                *(MacStrPtr++) = mychr;
                sprintf (MacStrPtr, "%05u", MacLblNbr);
                MacStrPtr += 5;
                break;
            case 'L':
            case 'l':       /* Parameter length */
            case '#':       /* Parameter count  */
                sprintf (MacStrPtr, "%02d",
                            (mychr == '#' ? *d00e2 :
                                            gArgParm (getc (MacFile), 0)));
                MacStrPtr += 2;
                break;
            default: /* Not an escape chr - Pass backslash on to destination */
                *(MacStrPtr++) = mychr;
        }
    }
}

/* ******************************************************************** *
 * MovMacArg() - gets argument list from macro definition or call       *
 *          Uses MacArgBuf if defined, else allocates memory for def    *
 * Returns: address of argument list                                    *
 * ******************************************************************** */

static char * 
MovMacArg (ZILCH)
{
    register char *dst;
    char *argbgn;
    int MArgCnt,
      arglen;

    MArgCnt = 0;
    arglen = 59;

    if (MacArgBuf)
    {
        dst = MacArgBuf;
        MacArgBuf = *(char **) MacArgBuf;
    }
    else
    {
        dst = getmem (61);
    }

    SkipSpac ();
    Oprand = SrcChar;
    d00d8 = 1;
    argbgn = dst++;

    if (*SrcChar != '\0')
    {
        char mquote,
             argchar;
    
        while (arglen && ((argchar = *SrcChar) != '\0') && (notblank(argchar)))
        {
            switch (*SrcChar)
            {
                case ',':   /* Comma = new macro parameter */
                    *(dst++) = '\0';
                    ++MArgCnt;
                    ++SrcChar;
                    break;
                case ' ':
                case '\t':
                    goto endloop;
                case '\'':
                case '\"':
                    mquote = *(SrcChar++);

                    while ( ((argchar = *(SrcChar++)) != '\0') &&
                            (argchar != mquote)                   )
                    {
                        if (argchar == '\\')
                        {
                            argchar = *(SrcChar++);
                        }

                        *(dst++) = argchar;
                    }

                    if (argchar == '\0')
                    {
                        e_report ("unmatched quotes");
                    }
                    
                    break;
                case '\\':
                    ++SrcChar;      /* fall through to next case */
                default:
                    *(dst++) = *(SrcChar++);
                    break;
            }

            --arglen;
        }
        
        ++MArgCnt;
    }

endloop:
    *dst = '\0';

    if (arglen == 0)    /* Max space exhausted */
    {
        e_report ("macro arg too long");
    }
    
    if (MArgCnt > 9)
    {
        e_report ("too many args");
    }

    *argbgn = MArgCnt;
    return argbgn;
}

/* ************************************************************ *
 * clrargptr() - Unsets the argument list by setting first      *
 *      Integer of the string to NULL, Also saves the address   *
 *      of this buffer into MacArgBuf  for future use           *
 * ************************************************************ */

static void
#ifdef __STDC__
clrargptr (char **parm)
#else
clrargptr (parm)
     char **parm;
#endif
{
    *parm = MacArgBuf;      /* Set first integer-size bytes to NULL.  This  */
                            /* clears count byte & nulls begin of buffer    */
    MacArgBuf = (char *)parm;  /* Save Buffer address for later use */
}

/* **************************************************************** *
 * gArgParm() - Retrieve the specified argument parameter # from    *
 *              list andstores it in string pointed to by MacStrPtr *
 *              if cpyflg != 0                                      *
 * Passed : (1) ASCII value of Label number of desired label        *
 *          (2) cpyflg - nonzero if copy to MacStrPtr is desired    *
 * Returns: length of name string.                                  *
 * **************************************************************** */

static int
#ifdef __STDC__
gArgParm (int desired, int cpyflg)
#else
gArgParm (desired, cpyflg)
     int desired;
#endif
{
    int _namlen = 0;

    if ((desired -= '0') > 0)   /* convert ASCII to digit */
    {
        register char *_namptr;

        if (*(_namptr = d00e2) >= desired)
        {
            ++_namptr;

            /* Move pointer to desired name in list */

            while (--desired > 0)
            {
                /* move _namptr past this string */

                while (*(_namptr++))
                {
                }
            }
            
            while (*_namptr != '\0')    /* Parse desired name */
            {
                if (cpyflg)
                {
                    *(MacStrPtr++) = *_namptr;
                }
            
                ++_namptr;
                ++_namlen;      /* Count elements in name */
            }
        }
        else
        {
            e_report ("no param for arg");
        }
    }

    return _namlen;
}

void
closmac (ZILCH)
{
    /* original version didn't have this if test in it
     * However, Linux sqawks with segfault if macfp1 == 0
     * This is the correct way */

    if (macfp1)
    {
        fclose (macfp1);
        unlink (TmpNam);
    }
}

static void
mfilerr (ZILCH)
{
    errexit ("macro file error");
}
