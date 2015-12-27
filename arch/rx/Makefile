#
# arch/rx/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# (C) Copyright 2009 Yoshinori Sato <ysato@users.sourceforge.jp>
#

KBUILD_CFLAGS += $(cflags-y)
KBUILD_CFLAGS += -g
KBUILD_CFLAGS += -D__linux__
KBUILD_CFLAGS += -DUTS_SYSNAME=\"uClinux\"
LDFLAGS += $(ldflags-y)
LDFLAGS += --no-flag-mismatch-warnings
KBUILD_AFLAGS += -Wa,-mcpu=rxv2

LIBGCC := $(shell $(CROSS-COMPILE)$(CC) $(KBUILD_CFLAGS) -print-libgcc-file-name)

core-y	+= arch/$(ARCH)/kernel/ \
	   arch/$(ARCH)/mm/

ifneq '$(CONFIG_RX_BUILTIN_DTB)' '""'
core-y += arch/$(ARCH)/boot/dts/
endif

libs-y	+= arch/$(ARCH)/lib/ $(LIBGCC)

boot := arch/$(ARCH)/boot

%.dtb %.dtb.S %.dtb.o: | scripts
	$(Q)$(MAKE) $(build)=arch/$(ARCH)/boot/dts arch/$(ARCH)/boot/dts/$@

PHONY += dtbs
dtbs: scripts
	$(Q)$(MAKE) $(build)=arch/$(ARCH)/boot/dts

archmrproper:

archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

vmlinux.srec vmlinux.bin zImage uImage uImage.bin: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

define archhelp
  @echo  'vmlinux.bin  - Create raw binary'
  @echo  'vmlinux.srec - Create srec binary'
  @echo  'zImage       - Compressed kernel image'
endef
