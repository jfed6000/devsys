/* bank.h - what dcc68's copy of las/bank.c needs (step 20) */

#if defined(_OS9) && !defined(COCO)
#  define COCO
#endif

#ifdef COCO
#  define void int
#  define ZILCH
#else
#  define ZILCH void
#endif

#define errexit(s) fatal(M_OUT_OF_MEMORY)

char *bank_ptr ();
unsigned bank_alloc ();
void bank_done ();
void bank_stats ();
