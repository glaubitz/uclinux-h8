#
# Makefile for the linux kernel.
#

extra-y := vmlinux.lds

obj-y := head.o process.o traps.o irq.o signal.o \
         setup.o syscalls.o entry.o ptrace.o dma-nommu.o

obj-$(CONFIG_RX_GDB_SIM) += sim-console.o
