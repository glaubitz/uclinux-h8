/*
 * int_sr.c
 *
 * Copyright 2016 Yoshinori Sato
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/traps.h>
#include <asm/io.h>
#include <asm/machdep.h>

/* assembler routines */
asmlinkage void system_call(void);
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void inthandler(void);

extern e_vector *inttable;

/* Bitmap of IRQ masked */
#define IMASK_PRIORITY	7

static DECLARE_BITMAP(imask_mask, IMASK_PRIORITY);
static int interrupt_priority;

const static char priority[] = {
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	1,1,1,1,3,3,3,3,3,3,3,3,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static inline void set_interrupt_registers(int ip)
{
	unsigned short sr;

	asm volatile("movew	%%sr, %0\n\t"
		     "andw	#0x0700, %0\n\t"
		     "cmpw	#0x0700, %0\n\t"
		     "beq	1f\n\t"
		     "movew	%%sr,%0\n\t"
		     "andw	#~0x0700, %0\n\t"
		     "orw	%1, %0\n\t"
		     "movew	%0, %%sr\n"
		     "1:"
		     : "=&r" (sr)
		     : "g" (ip));
}

static void sr_mask(struct irq_data *data)
{
	unsigned int irq = data->irq;

	clear_bit(priority[irq - 0x40], imask_mask);
	if (interrupt_priority < priority[irq - 0x40])
		interrupt_priority = priority[irq - 0x40];
	set_interrupt_registers(interrupt_priority << 8);
}

static void sr_unmask(struct irq_data *data)
{
	unsigned int irq = data->irq;

	set_bit(priority[irq - 0x40], imask_mask);
	interrupt_priority = find_first_zero_bit(imask_mask, IMASK_PRIORITY);
	set_interrupt_registers(interrupt_priority << 8);
}

static struct irq_chip intc_irq_chip = {
	.name		= "M68K-SR-INT",
	.irq_mask	= sr_mask,
	.irq_unmask	= sr_unmask,
};

/*
 * This function should be called during kernel startup to initialize
 * the machine vector table.
 */
void __init trap_init(void)
{
	int i;
	unsigned long v = (unsigned long)&inttable;

	for (i = 0; i < VEC_USER; i++)
		vectors[i] = trap;
	for (i = VEC_SPUR; i <= VEC_INT7; i++)
		vectors[i] = bad_inthandler;

	for (i = VEC_USER; i < 256; i++)
		vectors[i] = v + (i - VEC_USER)*4;
	vectors[VEC_BUSERR] = buserr;
	vectors[VEC_SYS] = system_call;

}

void __init init_IRQ(void)
{
	int i;

	for (i = 0; i < NR_IRQS; i++) {
		irq_set_chip(i, &intc_irq_chip);
		irq_set_handler(i, handle_level_irq);
	}
}

