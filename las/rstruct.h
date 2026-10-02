
/* **************************************************************** *
 * rstruct.h : Structure definitions for rma                        *
 *                                                                  *
 * $Id::                                                        $   *
 * **************************************************************** */

#define SYMLEN 64
                /* some helpful defines */
#define PASS1 !Pass2
#define endof(s) &(s[sizeof(s)/sizeof(s[0])])
#define ON 1
#define OFF 0

#ifndef NULL
#   define NULL 0
#endif

# ifndef TRUE
#   define TRUE 1
#   define FALSE 0
# endif

#ifndef COCO
#   define _ushort unsigned short
#else
#   define _ushort unsigned
/*#   define void     no "void" in CoCo compiler */
#endif

               /* define tofst flags */

#define NO_IMMED 0x40
#define PRE_10  0x80    /* Orig code was 0x10, but needed this byte */
#define PRE_11  0x20

               /* symblstruct->locmsk bit field definitions */

#define GLBL 4
#define XTRNL $10

               /* LblType bit field  definitions */

#define DPG 2
#define COD 4

typedef struct asm_cod
{
    char *ccmd,
          op_cod,
          tofst;
} CODFORM;

/*typedef struct jmptbl { int (*fn)(); } JMPTBL;*/
#ifdef COCO
#define JMPTBL(x) int (*x)()
#else
typedef int (*JMPTBL) (void);
#endif

            /* Structure definitions   */

/* Symbols and reference records are reached only through handles and
 * the functions in symtab.c, which alone knows how they are stored
 * (phase 8: they live in banked 8K blocks on the 6809, where a pointer
 * into them is only good until another block is mapped in). 0 = none. */

typedef unsigned SYMH;          /* a symbol */
typedef unsigned REFH;          /* a reference record */

#ifdef COCO
typedef long SVAL;              /* a symbol's value (s_ofst) */
#else
typedef int SVAL;
#endif

struct ref_ent
{
    char ETyp;
    SYMH RAddr;
};

/* Delete this temporarily.. try to create a structure to hold all
 * of command bytes */
/*union storer1 {
   char C_Byt[4];*/
                    /*#ifdef COCO*//* Defined this way because of 68K byte alignment */
/*   short C_Int[2];
   long q_long;
   struct {
      char f_byt;
      short f_int;
   } shrtlng;*/
/*#endif*/
/*};*/

/* the storer1 union is a device by which any length value can
 * be stored and retrieved on a coco
 */
#ifdef COCO
union storer1
{
    short C_Int[2];     /* FIXME : Do we only need a single int ?? */
    long q_long;
    struct              /* for 3-byte strings */
    {
        char f_byt;
        short f_int;
    } shrtlng;
};
#endif

/* ************************************************************ *
 * struct cmdbytes is an array of bytes or an assembly command  *
 *      This is the string of characters that will be written   *
 *      into the code section of the ROF.                       *
 *      Prior to writing, a pointer will be set up to point to  *
 *      the begin of code to write.  It will point to opcode    *
 *      unless a prebyte is used.  In this case, prebyte will   *
 *      be the beginning of the write                           *
 * ************************************************************ */

struct cmdbytes
{
    char prebyte,   /* Only used if a prebyte is used */
         opcode;    /* The begin of most codes        */

    /* The following is the operand for the command. */

#ifdef COCO
    union storer1 opers;    /* See above.  variable length datas */
#else

    /* Since most systems used will be BIGENDIAN, we have to reverse
     * the byte order for multi-byte values.  We do this by converting
     * the operand to a string
     */

    char opers[5];
#endif
};

                       /* Macro Descriptor */
typedef struct macdescr
{
    struct macdescr *LastMac;   /* Previous macro descriptor              */
    SYMH MSblPtr;               /* The macro's name in the symbol table   */
    FILE *MWrkFil;              /* File descriptor for macro work file    */
    long MacStart;              /* Address in work file for macro's begin */
} MACDSCR;

