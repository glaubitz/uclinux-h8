#
# Makefile for the linux kernel.
#

extra-y := vmlinux.lds

obj-y := head.o process.o traps.o irq.o time.o signal.o \
         setup.o syscalls.o entry.o ptrace.o dma-nommu.o \
	 timer/ cpu/

obj-$(CONFIG_EARLY_PRINTK) += early-printk.o
