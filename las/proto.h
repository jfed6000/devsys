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
int is_def(void);
int set_fcn(void);
void addzero(void);
int set_dtb(void);
int set_fill(void);
int is_ndef(void);
int l1723(void);
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
void wrtCmmns(SYMH parm);
/* findcmd.c */
CODFORM *FindCmd(char *src_str, CODFORM *cod_strt, CODFORM *cod_stop);
/* labels.c */
int MovLbl(char *destpos);
int do_label(void);
int lblnumeric(void);
void l2069(int newmode);
void l20a4(int address);
void RefCreat(SYMH mylabel, int rtyp, int adrs);
char SkipSpac(void);
void *getmem(int memreq);
void do_defines(void);
/* defsimg.c */
int img_use(char *name);
void img_pass(void);
void img_end(void);
void img_bad(void);
void img_touch(SYMH h);
void img_op(char *op);
void img_file(char *use);
int preload_count(void);
int cond_count(void);
int cond_bit(int i);
void cond_skip(int n);
unsigned bank_mark(void);
unsigned bank_after(unsigned h, int size);
int img_made(SYMH h);
SYMH sym_after(SYMH h);
FILE *u_find(char *fna, char *fmo);

/* preload.c */
void preload_add(char *file);
int preload_find(char *name, int *value);
void preload_load(void);
int preload_resolve(struct ref_ent *from, long *value);
int pl_fix(struct ref_ent *from);
/* sizing.c */
void sz_pass(void);
void sz_label(SYMH sym);
int sz_pcr(int value, int len8);
void sz_other(void);
void sz_trial(void);
void sz_finish(void);
void sz_child(char *arg);
/* lwobj.c */
void sec_pass(void);
int do_section(void);
int do_endsect(void);
int sec_export(char *name);
int sec_org(void);
int sec_rmb(void);
/* module.c */
int cond_log(int result);
int mod_peek(void);
void top_setup(void);
void mod_pass(void);
void mod_write(char *bytes, int len);
int do_mod(void);
int mod_emod(void);
int mod_rmb(void);
int mod_org(void);
int mod_dot(void);
/* symtab.c */
char *bank_ptr(unsigned h);
unsigned bank_alloc(int size);
void bank_done(void);
int bank_share(void);
char *bank_mapnum(int n);
void bank_unshare(int n);
void bank_stats(void);
SYMH sym_find(char *name);
SYMH sym_add(char *name);
SYMH sym_anon(void);
void sym_walk(void (*fn)(SYMH));
int sym_typ(SYMH h);
int sym_msk(SYMH h);
SVAL sym_val(SYMH h);
REFH sym_refs(SYMH h);
void sym_settyp(SYMH h, int v);
void sym_setmsk(SYMH h, int v);
void sym_setv(SYMH h, SVAL v);
void sym_setrefs(SYMH h, REFH r);
char *sym_name(SYMH h);
REFH ref_new(int typ, int offset, REFH next);
int ref_typ(REFH r);
int ref_off(REFH r);
REFH ref_next(REFH r);
void ref_setnext(REFH r, REFH next);
#ifdef SYMSTATS
void sym_stats(long refs);
#endif
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
int st_line(void);
int st_inst(char *op);
int st_post(void);
void errexit(char *report);

#else

/* directives.c */
int do_rzb();
int do_rmb();
int is_def();
int set_fcn();
void addzero();
int set_dtb();
int set_fill();
int is_ndef();
int l1723();
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
void *getmem();
void do_defines();
/* defsimg.c */
int img_use();
void img_pass();
void img_end();
void img_bad();
void img_touch();
void img_op();
void img_file();
int preload_count();
int cond_count();
int cond_bit();
void cond_skip();
unsigned bank_mark();
unsigned bank_after();
int img_made();
SYMH sym_after();
FILE *u_find();

/* preload.c */
void preload_add();
int preload_find();
void preload_load();
int preload_resolve();
int pl_fix();
/* sizing.c */
void sz_pass();
void sz_label();
int sz_pcr();
void sz_other();
void sz_trial();
void sz_finish();
void sz_child();
/* lwobj.c */
void sec_pass();
int do_section();
int do_endsect();
int sec_export();
int sec_org();
int sec_rmb();
/* module.c */
int cond_log();
int mod_peek();
void top_setup();
void mod_pass();
void mod_write();
int do_mod();
int mod_emod();
int mod_rmb();
int mod_org();
int mod_dot();
/* symtab.c */
char *bank_ptr();
unsigned bank_alloc();
void bank_done();
int bank_share();
char *bank_mapnum();
void bank_unshare();
void bank_stats();
SYMH sym_find();
SYMH sym_add();
SYMH sym_anon();
int sym_typ();
int sym_msk();
SVAL sym_val();
REFH sym_refs();
void sym_settyp();
void sym_setmsk();
void sym_setv();
void sym_setrefs();
char *sym_name();
REFH ref_new();
int ref_typ();
int ref_off();
REFH ref_next();
void ref_setnext();
void sym_walk();
#ifdef SYMSTATS
void sym_stats();
#endif
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
int st_line();
int st_inst();
int st_post();
void errexit();
#endif   /* __STDC__*/

/* symtab.c: always pass a symbol value as an SVAL */
#define sym_setval(h, v) sym_setv ((h), (SVAL) (v))

#endif   /* #ifndef _HAVEPROTO_*/

