ifneq '$(CONFIG_RX_BUILTIN_DTB)' '""'
BUILTIN_DTB := $(patsubst "%",%,$(CONFIG_RX_BUILTIN_DTB)).dtb.o
endif

obj-y += $(BUILTIN_DTB)

dtb-$(CONFIG_RX_GDB_SIM) := rx_sim.dtb
dtb-$(CONFIG_TKDN_RX62N) := tkdn_rx62n.dtb
