# arch/rx/boot/Makefile

MKIMAGE := $(srctree)/scripts/mkuboot.sh

suffix-y := bin
suffix-$(CONFIG_KERNEL_GZIP)	:= gz
suffix-$(CONFIG_KERNEL_BZIP2)	:= bz2
suffix-$(CONFIG_KERNEL_LZMA)	:= lzma
suffix-$(CONFIG_KERNEL_XZ)	:= xz
suffix-$(CONFIG_KERNEL_LZO)	:= lzo

targets := vmlinux.srec vmlinux.bin zImage uImage
subdir- := compressed

OBJCOPYFLAGS_vmlinux.srec := -Osrec -R .note.gnu.build-id
OBJCOPYFLAGS_vmlinux.bin  := -Obinary -R .note.gnu.build-id

CONFIG_MEMORY_START	?= 0x08000000
CONFIG_BOOT_LINK_OFFSET	?= 0x00800000
CONFIG_ENTRY_OFFSET	?= 0x00001000

$(obj)/vmlinux.srec $(obj)/vmlinux.bin:  vmlinux FORCE
	$(call if_changed,objcopy)
	@echo '  Kernel: $@ is ready'

$(obj)/zImage: $(obj)/compressed/vmlinux FORCE
	$(call if_changed,objcopy)
	@echo 'Kernel: $@ is ready'

$(obj)/compressed/vmlinux: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/compressed $@

KERNEL_LOAD	:= $(shell /bin/bash -c 'printf "0x%08x" \
		     $$[$(CONFIG_MEMORY_START) + \
			$(CONFIG_ENTRY_OFFSET)]')

KERNEL_ENTRY	:= $(shell /bin/bash -c 'printf "0x%08x" \
		     $$[$(CONFIG_MEMORY_START) + \
			$(CONFIG_ENTRY_OFFSET)]')

quiet_cmd_uimage = UIMAGE  $@
      cmd_uimage = $(CONFIG_SHELL) $(MKIMAGE) -A rx -O linux -T kernel \
		   -C $(2) -a $(KERNEL_LOAD) -e $(KERNEL_ENTRY) \
		   -n 'Linux-$(KERNELRELEASE)' -d $< $@

$(obj)/vmlinux.bin.gz: $(obj)/vmlinux.bin FORCE
	$(call if_changed,gzip)

$(obj)/vmlinux.bin.bz2: $(obj)/vmlinux.bin FORCE
	$(call if_changed,bzip2)

$(obj)/vmlinux.bin.lzma: $(obj)/vmlinux.bin FORCE
	$(call if_changed,lzma)

$(obj)/vmlinux.bin.xz: $(obj)/vmlinux.bin FORCE
	$(call if_changed,xzkern)

$(obj)/vmlinux.bin.lzo: $(obj)/vmlinux.bin FORCE
	$(call if_changed,lzo)

$(obj)/uImage.bz2: $(obj)/vmlinux.bin.bz2
	$(call if_changed,uimage,bzip2)

$(obj)/uImage.gz: $(obj)/vmlinux.bin.gz
	$(call if_changed,uimage,gzip)

$(obj)/uImage.lzma: $(obj)/vmlinux.bin.lzma
	$(call if_changed,uimage,lzma)

$(obj)/uImage.xz: $(obj)/vmlinux.bin.xz
	$(call if_changed,uimage,xz)

$(obj)/uImage.lzo: $(obj)/vmlinux.bin.lzo
	$(call if_changed,uimage,lzo)

$(obj)/uImage.bin: $(obj)/vmlinux.bin
	$(call if_changed,uimage,none)

$(obj)/uImage: $(obj)/uImage.$(suffix-y)
	@ln -sf $(notdir $<) $@
	@echo '  Image $@ is ready'

CLEAN_FILES += arch/$(ARCH)/vmlinux.bin arch/$(ARCH)/vmlinux.srec

export CONFIG_MEMORY_START CONFIG_BOOT_LINK_OFFSET CONFIG_ENTRY_OFFSET
