# native/GNUmakefile - cross-build the devsys tools for the 6809 (phase 7)
#
# Builds las, rlink and DCC (dcc, dcpp, dcc68, dco68) as OS-9 Level 2
# modules, compiled on Linux by devsys's own DCC and linked by its own rlink
# against CoCoC's C library. The result is what goes onto the F256:
#
#   make -f GNUmakefile          # everything into bin/
#   make -f GNUmakefile las      # one tool
#
# Each .c goes through the same passes the dcc driver runs on the F256:
#   dcpp (preprocess) -> dcc68 (compile) -> dco68 (optimise) -> las (assemble)
# and the .r files are linked with cstart.r, sys.l and clib.l.
# The host tools must be built first (see CLAUDE.md "Key files").

DEVSYS	:= $(abspath ..)
COCOC	?= $(HOME)/projects/dcc/CoCoC
CLIB	:= $(COCOC)/Lib

DCPP	:= $(DEVSYS)/dcc/CPP/dcpp
DCC68	:= $(DEVSYS)/dcc/CC09/dcc68
DCO68	:= $(DEVSYS)/dcc/COpt/dco68
LAS	:= $(DEVSYS)/las/build/unix/las
RLINK	:= $(DEVSYS)/rlink/rlink
HOSTTOOLS := $(DCPP) $(DCC68) $(DCO68) $(LAS) $(RLINK)

# -2: the optimizer's Level 2 patterns (D4). Level 2 data starts at
# address 0, so Y is always 0 and "leax >n,y" can be "ldx #n".
OPTFLAGS := -2

OBJ	:= obj
BIN	:= bin

# Per tool: source directory, objects, preprocessor defines, and extra
# memory (rlink -M, in 256-byte pages) added to the module's data size.
# The values are those of the tools' original native makefiles, except
# dcc68 (10, not 8). dcpp also takes its tables from this space (ibrk);
# with 16 pages it stops early without an error. dcpp itself defines OS9
# and _OS9, as the native cc does. See docs/DESIGN.md D20 and D25.
las_DIR		:= $(DEVSYS)/las
las_OBJS	:= rma_main findcmd part3 registers directives labels numeric \
		   file_io macros symtab bank module lwobj preload sizing
las_DEFS	:= -DCOCO
# 16 pages: the stack and stdio buffers (a macro's work file takes 512
# bytes) share this space; llwbsd overflowed without it. The data area
# still fits the two blocks below the window slot (D30)
las_MEM		:= 16

rlink_DIR	:= $(DEVSYS)/rlink
rlink_OBJS	:= rlink_main rl_02 rl_03 misc rl_05 util lwsel
rlink_DEFS	:= -DCOCO
rlink_MEM	:= 10

dcpp_DIR	:= $(DEVSYS)/dcc/CPP
dcpp_OBJS	:= main misc scan dues eval history lex macros parse
dcpp_DEFS	:= -DDCC
dcpp_MEM	:= 135

dcc68_DIR	:= $(DEVSYS)/dcc/CC09
dcc68_OBJS	:= cmain misc build optim stats longs floats inits get codgen \
		   local lex tranexp regscont declare bool scale
# dcc68 must stay at most 48,384 bytes (6 blocks ending by $FD00, D25):
# no -DPROF (no "dcc -p" natively), -s (no stack checks, as natively).
# It starts small (F$Fork won't fill all 8 slots) and grows with sbrk.
dcc68_DEFS	:= -DREGCONTS
dcc68_MEM	:= 10
dcc68_CC	:= -s

dco68_DIR	:= $(DEVSYS)/dcc/COpt
dco68_OBJS	:= main parse labels inst optim match misc ctype
dco68_DEFS	:=
dco68_MEM	:= 10

dcc_DIR		:= $(DEVSYS)/dcc/DCC
dcc_OBJS	:= dcc
dcc_DEFS	:=

TOOLS	:= las rlink dcpp dcc68 dco68 dcc

all: $(TOOLS)

.PHONY: all clean $(TOOLS)

# $(1) = tool name
define TOOL
$(1): $(BIN)/$(1)

$(OBJ)/$(1)/%.r: $$($(1)_DIR)/%.c $$(wildcard $$($(1)_DIR)/*.h) $(HOSTTOOLS) | $(OBJ)/$(1)
	cd $(OBJ)/$(1) && $(DCPP) $$($(1)_DEFS) -I=$$($(1)_DIR) -I=$(COCOC)/Defs $$< >$$*.i
	cd $(OBJ)/$(1) && $(DCC68) $$($(1)_CC) $$*.i -o=$$*.raw.a
	cd $(OBJ)/$(1) && $(DCO68) $(OPTFLAGS) <$$*.raw.a >$$*.a
	cd $(OBJ)/$(1) && $(LAS) -l -s -o=$$*.r $$*.a >$$*.lst || { grep -B1 -A1 '\*\*\*' $$*.lst; exit 1; }

$(BIN)/$(1): $$(patsubst %,$(OBJ)/$(1)/%.r,$$($(1)_OBJS)) | $(BIN)
	$(RLINK) -o=$$@ -n=$(1) $(if $($(1)_MEM),-M=$($(1)_MEM)) -m -s $(CLIB)/cstart.r $$^ -l=$(CLIB)/sys.l -l=$(CLIB)/clib.l >$(OBJ)/$(1).map

$(OBJ)/$(1):
	mkdir -p $$@
endef

$(foreach t,$(TOOLS),$(eval $(call TOOL,$(t))))

$(BIN):
	mkdir -p $@

# The object list of one tool, in link order, and its extra memory
# (harness/native.py)
print-%:
	@echo $($*_OBJS)

printmem-%:
	@echo $($*_MEM)

clean:
	rm -rf $(OBJ) $(BIN)
