#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _OS9
#define void int        /* DCC itself has no void (the 6809 build) */
#endif

#define LABELSIZE       32  /* dcc68's 31-character names, one spare; the
                                * ':' export mark isn't stored (was 11) */
#define MNEMSIZE        10
#define ARGSIZE         90

#define LINESIZE 100

#define LABEL       1
#define MNEM        2
#define ARGS        3
#define BRALIM     36	/* used to calculate branch distance somehow */
/* devsys: the whole file on both machines (bank.c); it was 300 on OS-9,
 * all that fitted in 64K, so the F256's output differed from Linux's */
#define MAXINS    32767

/* instruction type flags */
#define SUBR        0x0001
#define BRANCH      0x0020
#define BRATYPE     0x001F
#define BRAMASK     (BRANCH|BRATYPE)
#define LONG        0x0040
#define LONGBRA     (LONG|BRANCH)
#define BLOCKDUP    0x0080
#define CODEBRK     0x0100
#define DESTPTR     0x0200

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#define FORWARD 4
#define BACK    5

#ifndef _OS9
# define direct
#endif

#define GLOBAL      0x01    /* global label flag */

#if defined (__STDC__)
# if defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 199409L) /* C94+ */
#   define HAVE_C94	1
#  endif
#  if (__STDC_VERSION__ >= 199901L) /* C99+ */
#   define HAVE_C99	1
#  endif
#  if (__STDC_VERSION__ >= 201112L) /* C11+ */
#   define HAVE_C11	1
#  endif
#  if (__STDC_VERSION__ >= 201710L) /* C18+ */
#   define HAVE_C18	1
#  endif
# endif
#endif

/* devsys: the instructions, labels, reference cells and operand texts
 * dco68 holds live in 8K blocks of extra RAM (bank.c), so that a whole
 * file fits on the F256 as it does on Linux (the window used to be 300
 * instructions there, MAXINS). Each record is named by a 16-bit handle
 * (H, 0 = none) and reached through bank_ptr(), whose pointer is good
 * only until the next bank_ptr() of a handle in another block. So: no
 * statement may use a pointer from one record after mapping another
 * (a = b's field, with both mapped, goes through a local), and no call
 * takes pointers into two records (copy one first). The BANKWIN build
 * (Linux) emulates the one window and catches a stale pointer. */

#ifdef _OS9
typedef unsigned H;
typedef int SHORT;
#else
typedef unsigned short H;
typedef short SHORT;
#endif

/* a list cell: list heads (ilist, the label buckets), reference cells
 * (succ: the next cell, pred: the instruction that refers), free lists */
typedef struct chstruct {
    H succ, pred;
} chain;

typedef struct lstruct {
    H succ, pred;           /* the bucket's list (ltable) */
    H dest;                 /* the instruction it labels, 0 if none */
    H rlist;                /* reference cells of the branches to it */
    H nextl;                /* the next label on the same instruction */
    SHORT lflags;
    char lname[LABELSIZE+1];    /* allocated as long as the name needs */
} label;

#define LABHDR  (5 * sizeof(H) + sizeof(SHORT))     /* label before lname */

typedef struct istruct {
    H succ, pred;           /* ilist */
    H llist;                /* its labels */
    SHORT itype;
    SHORT mnem;             /* the mnemonic: its index in mnemtab (mnemix) */
    H arg;                  /* BRANCH with DESTPTR: the label; else the
                             * operand text (ARG) */
} instruction;

#define INS(h)  ((instruction *) bank_ptr (h))
#define LAB(h)  ((label *) bank_ptr (h))
#define CHN(h)  ((chain *) bank_ptr (h))
#define ARG(h)  ((char *) bank_ptr (h))

/* Reading a 2-byte field. On the 6809 each is a call to an assembly
 * accessor named by the field's offset (bank.c), 4 bytes shorter at
 * every read than bank_ptr + ldd n,x; the offsets are the 6809 layout
 * of the records above (no padding) and must follow any change there */
#ifdef _OS9
#define I_SUCC(h)   bkget0(h)
#define I_PRED(h)   bkget2(h)
#define I_LLIST(h)  bkget4(h)
#define I_ITYPE(h)  bkget6(h)
#define I_MNEM(h)   bkget8(h)
#define I_ARG(h)    bkget10(h)
#define L_SUCC(h)   bkget0(h)
#define L_DEST(h)   bkget4(h)
#define L_RLIST(h)  bkget6(h)
#define L_NEXTL(h)  bkget8(h)
#define L_LFLAGS(h) bkget10(h)
#define C_SUCC(h)   bkget0(h)
#define C_PRED(h)   bkget2(h)
#define RET_OLDER(h) bkget2(h)
#define RET_INS(h)  bkget4(h)
extern H bkget0(), bkget2(), bkget4(), bkget6(), bkget8(), bkget10();
#else
#define I_SUCC(h)   (INS(h)->succ)
#define I_PRED(h)   (INS(h)->pred)
#define I_LLIST(h)  (INS(h)->llist)
#define I_ITYPE(h)  (INS(h)->itype)
#define I_MNEM(h)   (INS(h)->mnem)
#define I_ARG(h)    (INS(h)->arg)
#define L_SUCC(h)   (LAB(h)->succ)
#define L_DEST(h)   (LAB(h)->dest)
#define L_RLIST(h)  (LAB(h)->rlist)
#define L_NEXTL(h)  (LAB(h)->nextl)
#define L_LFLAGS(h) (LAB(h)->lflags)
#define C_SUCC(h)   (CHN(h)->succ)
#define C_PRED(h)   (CHN(h)->pred)
#define RET_OLDER(h) (((H *) bank_ptr (h))[1])  /* inst.c's rcell */
#define RET_INS(h)  (((H *) bank_ptr (h))[2])
#endif

/* An instruction's mnemonic: a string in ordinary memory, good for as
 * long as dco68 runs (each distinct mnemonic is kept once, inst.c) */
#ifdef _OS9
#define MNEMOF(h)   mnemof(h)
extern char *mnemof();
#else
#define MNEMOF(h)   (mnemtab[I_MNEM(h)])
#endif
extern char *mnemtab[];

extern char *bank_ptr ();
extern H bank_alloc (), bank_static ();

extern direct H ilist;          /* the list head: a chain record */
extern H ltable[];              /* the label buckets' heads */
extern H newchain(), newarg(), inslabel(), newlabel(), findlabel(), talloc();
extern H insins(), newins();
extern direct H retnew;          /* inst.c: the newest return's cell */
extern char *argcopy();
extern char *grab();
extern direct int inscount;
extern FILE *in,*out,*datfile;
extern direct int lbf,lbdone,opsdone;
extern direct int dbflag,l2flag;
extern char *parse();

extern match();
extern matchcpy();

#ifndef CONFDIR
# if defined(_OS9) || defined(_OSK) || defined(_OS9000)
#  define CONFDIR "/DD/Lib"
# else
#  define CONFDIR "/usr/local/share/dcc"
# endif
#endif
