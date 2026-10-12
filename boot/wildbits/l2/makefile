# devsys boot disks: NitrOS9's own wildbits l2 recipe, built from a
# NitrOS9 tree outside devsys (default ~/projects/wild/main/nitros9;
# make N9=DIR for another), plus pipes and /Nil in the boot. Nothing in
# that tree is edited; the build writes its outputs here, except what
# every NitrOS9 build generates in its own tree (defs/buildinfo, the
# fonts and backgrounds).
#
#   make PLATFORM=k2      l2_wildbits_devsysk2.dsk
#   make PLATFORM=jr2     l2_wildbits_devsysjr2.dsk (the default)
#
# Why pipes and /Nil: dcc -w sends the compiler's output to /nil, and
# shell pipelines need PipeMan/Piper/Pipe. /Nil is the lean NDrv driver
# (nzdrv.asm, 46 bytes), not VRN. See docs/DESIGN.md D44.
#
# NITROS9's recipes find their helpers by paths relative to the recipe
# folder (../wildbits.mak, ../../rules.mak, ../../libs.mak); the files of
# those names in boot/ and boot/wildbits/ include the real ones.

N9 ?= $(HOME)/projects/wild/main/nitros9

# recipes/rules.mak takes NITROS9DIR from the environment or cuts the
# current directory at /recipes/; here it must be told
override NITROS9DIR := $(N9)
export NITROS9DIR

RECIPE = wildbits_devsys
BOOTMODS_EXTRA = $(PIPE) nil.dr nil.dd

# The recipe's own files live in its folder: defsfile (found through -I)
# and padup256
AFLAGS_EXTRA += -I$(NITROS9DIR)/recipes/wildbits/l2
PADUP = $(NITROS9DIR)/recipes/wildbits/l2/padup256 bootfile

include $(NITROS9DIR)/recipes/wildbits/l2/makefile

# Lean /Nil pseudo-device (nzdrv/nzdesc with NIL=1; as the picothing recipe)
$(MODDIR)/nil.dr: $(LEVEL1)/modules/nzdrv.asm | $(MODDIR)
	$(AS) $(AFLAGS) -DNIL=1 $< $(ASOUT)$@
$(MODDIR)/nil.dd: $(LEVEL1)/modules/nzdesc.asm | $(MODDIR)
	$(AS) $(AFLAGS) -DNIL=1 $< $(ASOUT)$@
