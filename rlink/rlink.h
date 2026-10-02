
#ifdef MAIN
#   define GLOBAL
#else
#   define GLOBAL extern
#endif

#ifdef _OS9
#   define void int
#else
#   define direct
#endif

#include <stdio.h>
#include <string.h>
#include "rof.h"
#include <stdint.h>

typedef uint16_t UINT;
typedef int32_t LONGTYPE;
typedef int16_t INTTYPE;

#ifndef _OS9
#   if defined __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#       define __LIL_END__
#       undef MODSYNC
#       define MODSYNC 0xcd87
#   else
#       undef MODSYNC
#       define MODSYNC 0x87cd
#       define o9_int(a) a
#       define getwrd(a) getw(a)
#       define putwrd(a,b) putw(a,b)
#   endif
#else
#   define MODSYNC 0x87cd
#   define o9_int(a) a
#   define getwrd(a) getw(a)
#   define putwrd(a,b) putw(a,b)
#endif

#ifndef EOF
#   define EOF -1
#endif

/* SYMDAT Type flag: defined in a ROF version 0 module (Microware's
 * tools), where only 8 characters of a name were kept (misc.c v0alias) */

#define V0NAME 0x200

/* Names are allocated at their own length (setname, misc.c), not in
 * SYMLEN+1 arrays: at 64 bytes a name, linking against clib.l ran the
 * 6809 out of memory (phase 7) */

typedef struct sym_dat
{
    char           *sname;
    unsigned        Type;
    UINT            Offset;
    struct sym_dat *sprev;
    struct sym_dat *snext;
} SYMDAT;

typedef struct ob_files
{
    char             obname[MAXNAME + 1];   /* getname keeps 16 */
    int              x10;
    unsigned         myaddr;
    struct ob_files *o_next;
    char             x16;
    struct ob_files *obn;      /* +17 */
    int              x19;
    char             edition;
    char             spare;
    UINT             UDat;     /* 23 */
    UINT             UDpD;     /* 25 */
    UINT             IDat;     /* 27 */
    UINT             IDpD;     /* 29 */
    UINT             codesiz;  /* 31 */
    UINT             stack;    /* 33 */
    UINT             entry;    /* 35 */
    int              x37;
    UINT             drefs;
    LONGTYPE         rcodbgn;
    SYMDAT          *symlist;
    char            *roffile;
} OBFILES;

struct ext_ref
{
    struct ext_ref *next;
    struct ext_ref *prev;
    char           *name;       /* setname */
    struct ob_files *myOb;      /* Don't know what this is */
};

struct dref_node {
    struct dref_node *dnext;
    INTTYPE refs[7];
};

#include "proto.h"

/*int L1031 ();
int L1037 ();
int L103d ();
int L1041 ();
int L1047 ();*/

/* Begin of uninit DP */
GLOBAL direct int   LwLink;     /* --lwlink: lwlink's module layout (rl_03.c) */
GLOBAL direct int   FileCount;
GLOBAL direct int   lfilenum;
GLOBAL direct int   printmap;
GLOBAL direct int   printsym;
GLOBAL direct int   D0011;
GLOBAL direct int   B09Mod;
GLOBAL direct UINT  okstatic;
GLOBAL direct UINT  TyLang;   /* D0017 */
GLOBAL direct UINT  namelen;
GLOBAL direct UINT  bgn_udat;
GLOBAL direct UINT  t_udat;
GLOBAL direct UINT  bgn_udpd;
GLOBAL direct UINT  t_udpd;
GLOBAL direct UINT  bgn_idat;
GLOBAL direct UINT  t_idat;
GLOBAL direct UINT  bgn_idpd; 
GLOBAL direct UINT  t_idpd;
GLOBAL direct UINT  bgn_codsiz;
GLOBAL direct UINT  t_code;
GLOBAL direct UINT  D002f;
GLOBAL direct UINT  extramem;
GLOBAL direct UINT  maxbufsiz;
GLOBAL direct UINT  t_drefs;
GLOBAL direct FILE     *InFile,
                       *OutFile;
GLOBAL direct OBFILES  *CurntOB;
GLOBAL direct OBFILES  *FirstMod;
GLOBAL direct OBFILES  *D003f;
GLOBAL direct SYMDAT   *commnsyms;

GLOBAL
/* Do following to match original */
#ifdef MAIN
direct
#endif
char                  *RofFil;
GLOBAL direct UINT     rofcount;    /* D0045 */
GLOBAL direct UINT     dup_find;
GLOBAL direct int      CurntFil;
GLOBAL direct int      LibFile;     /* D004b */
GLOBAL direct int      obmemreq;
GLOBAL direct char    *inpBuf;      /* D004f */
GLOBAL direct char    *curRofNa;
GLOBAL direct int      ddcount;
GLOBAL direct int      dtcount;
GLOBAL direct OBFILES *curObfil;
GLOBAL direct struct dref_node  *dtxtrefs,
                                *ddtarefs;
GLOBAL direct int      D005d;
GLOBAL direct int      D005f;
GLOBAL direct int      D0061;

GLOBAL char **filenames;  /* D01f6 */
GLOBAL char  *lfiles[16];
GLOBAL int    lpos[16];     /* input files given before each -l (lwsel.c) */
GLOBAL char   edition;
GLOBAL char  *NameFile;
GLOBAL char  *modname;
GLOBAL char  *OutfName;
GLOBAL char  *B09EntPt;
GLOBAL FILE  *NamFilFP;
