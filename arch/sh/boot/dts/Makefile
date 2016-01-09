ifneq '$(CONFIG_SH_BUILTIN_DTB)' '""'
BUILTIN_DTB := $(patsubst "%",%,$(CONFIG_SH_BUILTIN_DTB)).dtb.o
endif

obj-y += $(BUILTIN_DTB)

dtb-$(SH_7619_SOLUTION_ENGINE) := se7619.dtb

always      := $(dtb-y)
clean-files := *.dtb.S *.dtb
