/* rl_02.c */
extern readrofs ();

/* L0937 (binhead *bp, int do_vars); */
extern L0937 ();

extern adj_ofst ();

extern adjsmofst ();

extern setbgns ();

/* goextref (OBFILES *ob); */
extern goextref ();

/* skiprefs (int nrefs); */
extern skiprefs ();

/* int ncodlocs(register int maxcount); */
extern int ncodlocs ();

/* void skipcomn(unsigned short vrsn); */
extern skipcomn ();

/* SYMDAT *lastobfile (register SYMDAT *curob); */
extern SYMDAT *lastobfile ();

/* rl_03.c */

extern wrtmodule ();

//void L1457 (int parm1);
extern L1457 ();

extern L14d3 ();

/* void procref(def_ref *b3, SYMDAT *ob); */
extern procref ();

/* int pty_gen (register char *ip, int c); */
extern int pty_gen ();

extern rderr();

extern wrerr();

extern int crc();

/* replacement for getw */
/* int16_t getwrd (FILE *stream); */
int16_t getwrd ();

/* replacement for putw */
/* int putwrd(short wrd, FILE *stream); */
extern int putwrd();

/* rl_04.c */

extern int hash();

SYMDAT *lookup();
int L1bcf(SYMDAT *symbl);

extern fatal();

extern int req_mem(char *linpt);
extern ref2base();
struct ext_ref *findext(char *srchname, int findonly);
struct ext_ref *L1d61(char *nam);

extern char *grab();
extern char *get_loc();

int writemap(void);
int unrsolved(void);
extern getname();
char *_fgets(register char *dest, register FILE *fp);

extern tidy();

/* rl_05.c */
extern creatdnodes ();
extern setrefvoid(register unsigned short *mylist, int count);
char *emptyref(register short *reg01);
extern notused(char *p1);
extern getrefs(register struct dref_node **baseref, int addr);
extern writrefs(register struct dref_node *lref);
extern putref(int parm1);
extern flushbuf(void);
/* rlink_main.c */
int main(int argc, char **argv);
char *nxtfnam();
char *nxtlib();
extern illmainline();
extern cantopen();
extern notROF();
extern asmerrs();
extern toonew();
extern twomains();
extern in_rderr();

unsigned o9_int (UINT val);
/* lwsel.c */
extern lw_select();
