ifneq '$(CONFIG_RX_BUILTIN_DTB)' '""'
BUILTIN_DTB := $(patsubst "%",%,$(CONFIG_RX_BUILTIN_DTB)).dtb.o
endif

obj-y += $(BUILTIN_DTB)

dtb-$(CONFIG_RX_SIM) := rx_sim.dtb

dtstree		:= $(srctree)/$(src)
dtb-$(CONFIG_OF_ALL_DTBS) := $(patsubst $(dtstree)/%.dts,%.dtb, $(wildcard $(dtstree)/*.dts))

always	    := $(dtb-y)
clean-files := *.dtb.S *.dtb
