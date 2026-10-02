/* ******************************************************************** *
 * rma.h - Data definitions for rma                                     *
 *                                                                      *
 * $Id::                                                            $   *
 * ******************************************************************** */

#ifdef COCO
#   define const
#   define void int
#   define u_char char
#   define LBLLEN 9
#   define ZILCH    /* To allow void parameter in prototypes */
#else
#  define direct
#    ifndef u_char
#      define u_char unsigned char
#      define __u_char_defined
#     endif
#   define LBLLEN 9
#   define ZILCH void
#endif

#ifndef MAIN
#  define GLOBAL extern
#else
#  define GLOBAL
#endif


/* symbol table types */
/* symbol definition/reference type/location */

/* data type flags */

#define CODENT      0x04        /* data/code flag */

/* location flags */

#define DIRENT      0x02        /* global/direct flag */
#define INIENT      0x01        /* clear/init. data flag */
#define CODLOC      0x20        /* data/code flag */
#define DIRLOC      0x10        /* global/direct flag */
#define LOC1BYT     0x08        /* two/one byte size flag */
#define LOCMASK     (CODLOC | DIRLOC)
#define NEGMASK     0x40        /* negate on resolution */
#define RELATIVE    0x80        /* relative reference */

/* Masks in LblTyp */

#define LT_INPSECT 2
#define LT_GLOBAL 4
#define LT_SET 5
#define LT_EQU 6
#define LT_HASRIGHT 8       /* <=LOC1BYT=>*/
#define LT_HASREFS 0x20     /* <=CODLOC=> */
#define LT_CONST 0x40
#define LT_INIT 0x80        /* Flags that the symblstr has been initialized */

#include "cocotype.h"
#include <stdio.h>
#include <string.h>

/* We could use #include <ctype.h> for non-coco systems, but it's too
 * complicated for just getting one macro definition.
 */

#ifdef NEED_CTYPE
#   define isblank(c) ((c == ' ') || (c == '\t'))
#   define notblank(c) ! isblank(c)
#endif

#include "rstruct.h"
#ifndef COCO
#   include "proto.h"
#endif
#   include "proto.h"

GLOBAL direct char *incdirs[32];
GLOBAL direct int  inccount
#ifdef MAIN
=0
#endif
;

GLOBAL direct char e_flg
#ifdef MAIN
    = ON
#endif
    ,
    ListMacs
#ifdef MAIN
    = ON
#endif
    ;
GLOBAL direct int PgDepth
#ifdef MAIN
    = 66
#endif
    ,
    PgWidth
#ifdef MAIN
    = 80
#endif
    ;
GLOBAL direct FILE *SrcFile,    /* D0015 */
 *OutFile,                      /* D0017 */
 *PrtPth;
GLOBAL direct int NumBytes,
  d001d,
  jsrOfst,
  d0021,
  HadError;
GLOBAL direct unsigned d0025,
  d0027,
  _If_Dpth,
  FalseIf;
GLOBAL direct unsigned ListPag,
  ListLin;                      /* d002d, d002f */
    /* NmbrInt is a general catch-all storage for integral #
     * is used to store operand values, count, etc
     */
#ifdef COCO
GLOBAL direct long NmbrInt;
#else
GLOBAL int NmbrInt;
#endif
GLOBAL direct short IndxVal;
GLOBAL direct int HadBrkt,
                  HadArrow;     /* -1 if extended, 1 if direct, else 0 */
GLOBAL direct int LongJmp[2];     /*  ???   */
GLOBAL direct int doList,
                  IsExtrn,
                  U_data,
                  U_dpd,
                  I_data,
                  I_dpd;
GLOBAL direct int  CsectOrg,
                  *S_Addrs;
GLOBAL direct int  CodeSize;
GLOBAL direct int *Adrs_Ptr;
GLOBAL direct int  CodeAddrs,
                  *CurntOrg;
GLOBAL direct short GlobCnt,
                    ref_cnt,        /* D0055, D0057 */
                    comn_cnt,
                    locl_cnt;
GLOBAL direct char Pass2,           /* D005d    */
                   TypeBasic,       /* Raw type */
                   WhereLoc,        /* Used as a copy of TypeBasic */
                   DLocField,
                   LblTyp,
                   TmpLocMsk,
                   LOC1Msk,
                  *SrcChar,         /* d0064 originally */
                  *Label,
                  *Operatr,
                  *CmdNamEnd,       /* The final character in the asm cmd */
                  *Oprand,
                  *coment;
GLOBAL
#ifdef COCO
    direct JMPTBL (*jmp_ptr);
#else
    JMPTBL *jmp_ptr;
#endif
GLOBAL direct CODFORM *Cod_Ent;
GLOBAL direct CODFORM *CodTbBgn,
                      *CodTbEnd;
GLOBAL direct struct symblstr *CurntLabel,                      /* D007a */
                               *CurLclRef;
GLOBAL struct symblstr          LRefs_Code,
                                LRefs_DPD,
                                LRefs_NonDPdat;
/* delete this.. try to make whole thing a single structure..
 * seems that these aren't inline in linux, at least */
/*GLOBAL direct char prebyt, OpCod;
GLOBAL direct union storer1 C_Oprnd, *opr_ptr;*/
GLOBAL struct cmdbytes MLCod;
GLOBAL direct char *opr_ptr;
GLOBAL direct char AIMFlg;
GLOBAL direct int RefExtern;

            /* Command-line option flags */

GLOBAL direct char C_Flg,
                   ff_flg,
                   g_flg,
                   d00c4,
                   ListFlg,
                   listON,
                   NarrowList,
                   OFlg,
                   S_Flg,
                   R_Keep;                       /* D00c8, D00c9 */

GLOBAL direct int list_lin;     /* D00ca */
GLOBAL direct char *CmdCod;
GLOBAL direct int MacDefining,
  DoingMacro,
  ReptCount,
  InRept;

GLOBAL char TmpNam[30]
#ifdef MAIN
    = "rma.tmp"
#endif
    ;

GLOBAL char AltLbl[SYMLEN],
            LabelName[SYMLEN];
GLOBAL char d0587[SYMLEN],
            _nam[128],
            _ttl[128];
GLOBAL char CmdMnemonic[SYMLEN];
GLOBAL long reptFpos;
/* lwasm mode (--lwasm): ifp1 is true in both passes, as in lwasm, and
 * names containing '@' or '?' are local to a block between blank lines */
GLOBAL char LwasmMode;
GLOBAL int LocalCtx,        /* local label block number (symtab.c) */
           NextCtx;         /* last block number handed out */
GLOBAL char InDataDir;      /* evaluating an fcb/fdb value (numeric.c) */
GLOBAL char IdxLitZero;     /* indexed offset written as "0" (registers.c) */

/* lwasm pragmas set with --pragma= (file_io.c GetOpts) */
GLOBAL char PragNoCase,     /* nosymbolcase: symbol names ignore case */
            PragUndefZero;  /* condundefzero: undefined symbol in an if = 0 */

/* OS-9 module format (module.c) */
GLOBAL int AbsCtr,      /* location counter outside any section/module */
           DataCtr;     /* data counter (".") inside a module */
GLOBAL char InModule,   /* between mod and emod */
            InTop,      /* lwasm mode, outside a module: code allowed */
            TopPending, /* switch to InTop at the start of the next line */
            UsedModule, /* this file has a module: raw output, no ROF */
            SawPsect;   /* this file has a psect */

GLOBAL struct ref_ent  d0791[SYMLEN],
                       *OptBPtr;

/* File positions in ROF */

GLOBAL long RofHdLoc,
            OCodeBegin,
            O_idpdBegin,
            O_idatBegin;

                /* Following is from part4.c */
GLOBAL direct char Is_W;        /* Flag that we are indexing off W  */

                /* Following is from part8.c */
/* FrstFil is a "stack", so to speak, of Files being read.  When
 * a "use" statement is encountered, the file is opened, and the
 * FILE * is appended onto the end of the stack.  The variable
 * "ThisFile" (below) is a pointer to the last file on the stack,
 * actually always points to the next empty slot.  When the use-file
 * is ended, "ThisFile" is decremented back to the previous file pointer
 */

GLOBAL FILE *FrstFil[12];
GLOBAL const char *SrcNam[32];
GLOBAL char InpBuf[256];

#include "rtables.h"

GLOBAL FILE **ThisFile  /* Ptr to first empty slot in the use-file stack */
#ifdef MAIN
    = FrstFil
#endif
    ;

GLOBAL const char **LastSrc   /* Ptr to first empty slot in srcfiles array */
#ifdef MAIN
    = SrcNam
#endif
    ;

GLOBAL char *O_Nam
#ifdef MAIN
    = 0
#endif
    ;
GLOBAL int SColumn
#ifdef MAIN
    = 0
#endif
    ;
GLOBAL int rversion
#ifdef MAIN
    = 1
#endif
    ;

/* This is a special version of chcodes required for "rma" */

GLOBAL u_char _chcodes[]
#ifdef MAIN
    = {
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x01', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x02', '\x00',
    '\x38', '\x38', '\x28', '\x28', '\x28', '\x28', '\x28', '\x28',
    '\x28', '\x28', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x02', '\x22', '\x22', '\x22', '\x22', '\x22', '\x22', '\x02',
    '\x02', '\x02', '\x02', '\x02', '\x02', '\x02', '\x02', '\x02',
    '\x02', '\x02', '\x02', '\x02', '\x02', '\x02', '\x02', '\x02',
    '\x02', '\x02', '\x02', '\x00', '\x00', '\x00', '\x00', '\x02',
    '\x00', '\x04', '\x04', '\x04', '\x04', '\x04', '\x04', '\x04',
    '\x04', '\x04', '\x04', '\x04', '\x04', '\x04', '\x04', '\x04',
    '\x04', '\x04', '\x04', '\x04', '\x04', '\x04', '\x04', '\x04',
    '\x04', '\x04', '\x04', '\x00', '\x00', '\x00', '\x00', '\x00'
}
#endif
;

