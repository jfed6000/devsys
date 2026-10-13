# clib/GNUmakefile - the C libraries, built by devsys's own DCC and las (step 18)
#
#   make -f GNUmakefile          # lib/cstart.r, lib/clib.l, lib/clibt.l, lib/sys.l
#
# The sources are our fork of CoCoC's (Source/Libs/KLibc and SysLib, at
# CoCoC 68fdbf9). CoCoC's own Lib/*.l were built by Microware-era tools that
# kept 8 characters of a name (localtim, sinterse), which llink bridges with
# D19's aliases; built here, every name is whole. A library is its members'
# ROF files one after another (OS-9's "merge"), in the order of KLibc's
# OS-9 makefiles: llink takes a member when it defines something still
# unresolved, so a member must come before the members it calls.
#
# Each .c goes through the passes dcc runs (see native/GNUmakefile):
#   dcpp -> dcc68 -s (no stack checks, as KLibc's makefiles) -> dco68 -> las
# The host tools must be built first (see CLAUDE.md "Key files").

DEVSYS	:= $(abspath ..)
COCOC	?= $(HOME)/projects/dcc/CoCoC
DEFS	:= $(COCOC)/Defs
SRC	?= .

DCPP	:= $(DEVSYS)/dcc/CPP/dcpp
DCC68	:= $(DEVSYS)/dcc/CC09/dcc68
DCO68	:= $(DEVSYS)/dcc/COpt/dco68
LAS	:= $(DEVSYS)/las/build/unix/las
HOSTTOOLS := $(DCPP) $(DCC68) $(DCO68) $(LAS)

# -2: Level 2 patterns, as the tools (devsys is Level 2 only, D4)
OPTFLAGS ?= -2

OBJ	?= obj
LIB	?= lib
K	:= $(SRC)/klibc

# The members, by directory, in library order (KLibc's makefiles)
cstuff	:= prof pwent getopt adump defdrive popen asctime ctime systime time \
	   localtime mktime isleap timevars
main	:= swab pwcryp bsearch dirutil errmsg prgname datmods devtyp skip sets \
	   pflinit pffinit_c fopen puts gets mktemp fread fwrite printf scanf_c \
	   setbuf fseek putc ftelldummy getc flshdummy setbase findstr pfldummy \
	   pffdummy htoi htol ltoa xtoa \
	   system reverse strings strcmp strlen strncpy strncat strncmp strhcpy \
	   strtok strpbrk strspn index patmatch \
	   stringsu strucmp strnucmp case strclr memccpy memchr memcmp memcpy \
	   memset atol atoi iob_data chcodes \
	   l3tol_c ltol3_c qsort_c sleep setjmp strass realloc calloc memory rand \
	   atof scale ldexp frexp gs1 gs2 ss1 ss2 ss3
math	:= abs minmax uminmax clmul cldiv cludiv claddsub clcompare \
	   clbits clnegcompl clconvert clmove clincdec clshifts clushr \
	   clcommon ccmult ccmod ccdiv cshifts rpterr
trans	:= trig hyp log trnser dsqrt dinc dtype dmul ddiv daddsub dser dcmpr
syslib	:= abort signal chown chmod stat access create \
	   open read write lseek misc mod dir brk ibrk sbrk mem \
	   time process id intercept syscall syscommon \
	   cfinish profdummy tidyup
sys	:= os9 rbf scf coco

R	= $(patsubst %,$(OBJ)/$(1)/%.r,$(2))
CSTUFF	:= $(call R,lib.c,$(cstuff))
MAIN	:= $(call R,lib.a,$(main))
MATH	:= $(call R,imath,$(math))
TRANS	:= $(call R,tmath,$(trans))
SYSLIB	:= $(call R,sys.a,$(syslib))
CFLOATS	:= $(OBJ)/fmath/cfloats.r
SYS	:= $(call R,syslib,$(sys))

all: $(LIB)/cstart.r $(LIB)/clib.l $(LIB)/clibt.l $(LIB)/sys.l

.PHONY: all clean
.SECONDARY:

$(LIB)/clib.l: $(CSTUFF) $(MAIN) $(CFLOATS) $(MATH) $(SYSLIB) | $(LIB)
	cat $^ >$@
$(LIB)/clibt.l: $(CSTUFF) $(MAIN) $(TRANS) $(MATH) $(SYSLIB) | $(LIB)
	cat $^ >$@
$(LIB)/sys.l: $(SYS) | $(LIB)
	cat $^ >$@
$(LIB)/cstart.r: $(OBJ)/sys.a/cstart.r | $(LIB)
	cp $< $@

# The sources say "use /dd/defs/os9defs.a", as on OS-9. On Linux that path
# may hold some other install, so the copy las reads says "use os9defs.a"
# and -i= gives defs/ (our os9defs.a: CoCoC's without its unfinished CMOC
# blocks, which no RMA-syntax assembler takes) and the source's directory
# (for "use ../defs/stdio.a")
UNDD	:= sed -e 's,^\( *use  *\)/[dD][dD]/[dD][eE][fF][sS]/,\1,'

# lib.c's own headers (dbg.h) come after the installed ones
$(OBJ)/lib.c/%.r: $(K)/lib.c/%.c $(HOSTTOOLS) | $(OBJ)/lib.c
	cd $(@D) && $(DCPP) -DASMOPT -I=$(DEFS) -I=$(abspath $(K)/defs) $(abspath $<) >$*.i
	cd $(@D) && $(DCC68) -s $*.i -o=$*.raw.a
	cd $(@D) && $(DCO68) $(OPTFLAGS) <$*.raw.a >$*.a
	cd $(@D) && $(LAS) -o=$*.r $*.a

$(OBJ)/lib.a/%.r: $(K)/lib.a/%.c $(HOSTTOOLS) | $(OBJ)/lib.a
	cd $(@D) && $(DCPP) -I=$(DEFS) -I=$(abspath $(K)/defs) $(abspath $<) >$*.i
	cd $(@D) && $(DCC68) -s $*.i -o=$*.raw.a
	cd $(@D) && $(DCO68) $(OPTFLAGS) <$*.raw.a >$*.a
	cd $(@D) && $(LAS) -o=$*.r $*.a

# The five members CoCoC has no source for (Microware's objects), as is
$(OBJ)/lib.a/%_c.r: $(K)/lib.a/RELS/%_c | $(OBJ)/lib.a
	cp $< $@

define ASM
$(OBJ)/$(1)/%.r: $(K)/$(1)/%.a $(LAS) | $(OBJ)/$(1)
	$(UNDD) <$$< >$(OBJ)/$(1)/$$*.a
	cd $(OBJ)/$(1) && $(LAS) -i=$(abspath $(SRC)/defs) -i=$(abspath $(K)/$(1)) -o=$$*.r $$*.a
endef
$(foreach d,lib.a fmath imath tmath sys.a,$(eval $(call ASM,$(d))))

# sys.l: NitrOS9's names as constants. The .p files are the .d files
# through asm2rma.sed (OS-9 sed: "use" files with RMA's syntax)
$(OBJ)/syslib/%.p: $(SRC)/syslib/%.d $(SRC)/syslib/asm2rma.sed | $(OBJ)/syslib
	sed -f $(SRC)/syslib/asm2rma.sed <$< >$@
$(OBJ)/syslib/nopublic.a: $(SRC)/syslib/os9.d $(SRC)/syslib/asm2rma.sed | $(OBJ)/syslib
	sed -f $(SRC)/syslib/asm2rma.sed -e s/://g $< >$@
$(OBJ)/syslib/os9.r: $(OBJ)/syslib/os9.p
$(OBJ)/syslib/rbf.r $(OBJ)/syslib/scf.r $(OBJ)/syslib/coco.r: $(OBJ)/syslib/nopublic.a
$(OBJ)/syslib/%.r: $(SRC)/syslib/%.a $(OBJ)/syslib/%.p $(LAS)
	$(UNDD) <$< >$(OBJ)/syslib/$*.a
	cd $(OBJ)/syslib && $(LAS) -i=$(abspath $(SRC)/defs) -o=$*.r $*.a

DIRS	:= $(addprefix $(OBJ)/,lib.c lib.a fmath imath tmath sys.a syslib)
$(DIRS) $(LIB):
	mkdir -p $@

clean:
	rm -rf $(OBJ) $(LIB)
