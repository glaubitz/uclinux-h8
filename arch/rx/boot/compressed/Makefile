#
# linux/arch/rx/boot/compressed/Makefile
#
# create a compressed vmlinux image from the original vmlinux
#

targets		:= vmlinux vmlinux.bin head.o misc.o piggy.o

OBJECTS = $(obj)/head.o $(obj)/misc.o

#
# IMAGE_OFFSET is the load offset of the compression loader
# Assign dummy values if these 2 variables are not defined,
# in order to suppress error message.
#
CONFIG_RAMSTART     ?= 0x08000000
CONFIG_BOOT_LINK_OFFSET ?= 0x00800000

IMAGE_OFFSET := $(shell printf "0x%08x" $$(($(CONFIG_RAMSTART)+$(CONFIG_BOOT_LINK_OFFSET))))

LIBGCC := $(shell $(CROSS-COMPILE)$(CC) $(KBUILD_CFLAGS) -print-libgcc-file-name)
LDFLAGS_vmlinux := -Ttext $(IMAGE_OFFSET) -estartup -T $(obj)/vmlinux.lds \
	--defsym output=$(CONFIG_RAMSTART)
CFLAGS_misc.o := -ggdb

$(obj)/vmlinux: $(OBJECTS) $(obj)/piggy.o $(LIBGCC) FORCE
	$(call if_changed,ld)
	@:

$(obj)/vmlinux.bin: vmlinux FORCE
	$(call if_changed,objcopy)

suffix-$(CONFIG_KERNEL_GZIP)    := gzip
suffix-$(CONFIG_KERNEL_LZO)     := lzo

$(obj)/vmlinux.bin.$(suffix-y): $(obj)/vmlinux.bin FORCE
	$(call if_changed,$(suffix-y))

LDFLAGS_piggy.o := -r --format binary --oformat elf32-rx-linux -T
OBJCOPYFLAGS := -O binary

$(obj)/piggy.o: $(obj)/vmlinux.scr $(obj)/vmlinux.bin.$(suffix-y) FORCE
	$(call if_changed,ld)
