##################################################
#
# Makefile for X68000
#
# Copyright 2016 Yoshinori Sato

extra-y			:= head.o
obj-y			+= entry.o ints.o mfp-timer.o x68k.o zs_earlyprintk.o

targets := vmlinux.bin vmlinux.bin.gz

OBJCOPYFLAGS_vmlinux.bin  := -Obinary

$(obj)/vmlinux.bin:  vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/vmlinux.bin.gz: $(obj)/vmlinux.bin FORCE
	$(call if_changed,gzip)

CLEAN_FILES += arch/$(ARCH)/x68k/vmlinux.bin arch/$(ARCH)/x68k/vmlinux.bin.gz
