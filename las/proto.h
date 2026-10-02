/* ************************************************************** *
 * proto_h - Prototypes for functions in dis68                    $
 *                                                                $
 * This file handles both STDC and non-STDC forms                 $
 * $Id::                                                          $
 * ************************************************************** */

#ifndef _HAVEPROTO_
#define _HAVEPROTO_
#ifdef __STDC__
/* directives.c */
int do_rzb(void);
int do_rmb(void);
int do_equ(void);
int do_set(void);
int do_comon(void);
int set_fcc(void);
int set_fcs(void);
int cnstDef(void);
int set_fcb(void);
int set_fdb(void);
int do_os9(void);
int iscomma(void);
int l198d(void);
int donothing(void);
int do_nam(void);
int do_ttl(void);
int do_pag(void);
int do_newline(void);
int _isnum(void);
int set_opts(void);
int l1ae2(void);
int do_use(void);
int _isnul(void);
int not_nul(void);
int is_neg(void);
int le_zero(void);
int ge_zero(void);
int is_pos(void);
int is_pass1(void);
int set_vsec(void);
int is_dp(void);
int precodeset(void);
int tel_fail(void);
int do_rept(void);
int end_rept(void);
/* file_io.c */
void GetOpts(unsigned _argc, char **_argv);
FILE *f_opn(const char *fna, char *fmo);
FILE *u_opn(char *fna, char *fmo);
void pagehdr(void);
void newline(void);
int getsrcline(void);
void DoSymTbl(void);
void do_psect(void);
void wrtvalid(void);
void w_ocod(char *_codaddr, int _codlen);
void wrt_refs(void);
void wrtCmmns(struct symblstr *parm);
/* findcmd.c */
CODFORM *FindCmd(char *src_str, CODFORM *cod_strt, CODFORM *cod_stop);
/* labels.c */
int MovLbl(char *destpos);
int do_label(void);
int lblnumeric(void);
void l2069(int newmode);
void l20a4(int address);
void RefCreat(struct symblstr *mylabel, int rtyp, int adrs);
char SkipSpac(void);
struct symblstr *WlkTreLft(struct symblstr *thistree);
void *getmem(int memreq);
/* macros.c */
int createMac(void);
int mcWrtLin(void);
int add_nul(void);
MACDSCR *McNamCmp(char *srchname);
void pushMac(MACDSCR *newmac);
int getmacline(void);
void closmac(void);
/* numeric.c */
int do_long(void);
int proc_int(void);
int proc_byte(void);
int getNmbr(void);
int _getnum(void);
/* part3.c */
int l_brnch(void);
int do_aim(void);
int do_band(void);
int cc_stuff(void);
int q_immed(void);
int do_md(void);
int int_stuf(void);
int chr_ef(void);
int chr_stuf(void);
int do_divd(void);
int bit_stuf(void);
int no_opcod(void);
int lea_s(void);
int do_tfm(void);
int tfr_exg(void);
int do_addr(void);
int stk_stuf(void);
int lb_(void);
int do_brnch(void);
int equset(void);
int asm_dirct(void);
int do_ifs(void);
int ilExtRef(void);
int do_endc(void);
int _psect(void);
int codsetup(void);
int csectsetup(void);
int vsectsetup(void);
int end_csect(void);
int setmac(void);
int endmac(void);
/* registers.c */
int immediat(void);
int not_immed(void);
int brnch_size(void);
int getreg(int PshPul);
int regofst(void);
char skparrow(int ch);
int ilAdrMod(void);
int ilIdxReg(void);
void genreflist(void);
int rmcoderefs(void);
int l156b(void);
int pc_rel(void);
void storInt(char *dst, int vlu);
/* rma_main.c */
int main(int argc, char *argv[]);
int printline(void);
int bdrglist(void);
int bd_rgnam(void);
int nstmacdf(void);
int e_report(char *report);
void errexit(char *report);

#else

/* directives.c */
int do_rzb();
int do_rmb();
int do_equ();
int do_set();
int do_comon();
int set_fcc();
int set_fcs();
int cnstDef();
int set_fcb();
int set_fdb();
int do_os9();
int iscomma();
int l198d();
int donothing();
int do_nam();
int do_ttl();
int do_pag();
int do_newline();
int _isnum();
int set_opts();
int l1ae2();
int do_use();
int _isnul();
int not_nul();
int is_neg();
int le_zero();
int ge_zero();
int is_pos();
int is_pass1();
int set_vsec();
int is_dp();
int precodeset();
int tel_fail();
int do_rept();
int end_rept();
/* file_io.c */
void GetOpts();
FILE *f_opn();
FILE *u_opn();
void pagehdr();
void newline();
int getsrcline();
void DoSymTbl();
void do_psect();
void wrtvalid();
void w_ocod();
void wrt_refs();
void wrtCmmns();
/* findcmd.c */
CODFORM *FindCmd();
/* labels.c */
int MovLbl();
int do_label();
int lblnumeric();
void l2069();
void l20a4();
void RefCreat();
char SkipSpac();
struct symblstr *WlkTreLft();
void *getmem();
/* macros.c */
int createMac();
int mcWrtLin();
int add_nul();
MACDSCR *McNamCmp();
void pushMac();
int getmacline();
void closmac();
/* numeric.c */
int do_long();
int proc_int();
int proc_byte();
int getNmbr();
int _getnum();
/* part3.c */
int l_brnch();
int do_aim();
int do_band();
int cc_stuff();
int q_immed();
int do_md();
int int_stuf();
int chr_ef();
int chr_stuf();
int do_divd();
int bit_stuf();
int no_opcod();
int lea_s();
int do_tfm();
int tfr_exg();
int do_addr();
int stk_stuf();
int lb_();
int do_brnch();
int equset();
int asm_dirct();
int do_ifs();
int ilExtRef();
int do_endc();
int _psect();
int codsetup();
int csectsetup();
int vsectsetup();
int end_csect();
int setmac();
int endmac();
/* registers.c */
int immediat();
int not_immed();
int brnch_size();
int getreg();
int regofst();
char skparrow();
int ilAdrMod();
int ilIdxReg();
void genreflist();
int rmcoderefs();
int l156b();
int pc_rel();
void storInt();
/* rma_main.c */
int main();
int printline();
int bdrglist();
int bd_rgnam();
int nstmacdf();
int e_report();
void errexit();
#endif   /* __STDC__*/

#endif   /* #ifndef _HAVEPROTO_*/

