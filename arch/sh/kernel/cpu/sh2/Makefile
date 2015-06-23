#
# Makefile for the Linux/SuperH SH-2 backends.
#

obj-y	:= ex.o probe.o entry.o

obj-$(CONFIG_CPU_SUBTYPE_SH7619) += setup-sh7619.o
ifneq ($(CONFIG_COMMON_CLK),y)
obj-y += clock-sh7619.o
endif
