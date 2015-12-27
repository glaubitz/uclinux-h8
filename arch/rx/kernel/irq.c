/*
 * arch/rx/kernel/irq.c
 *
 * Copyright (C) 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <asm/processor.h>
#include <asm/io.h>

void setup_rx_irq_desc(void);

extern unsigned long rx_int_table[NR_IRQS];
extern unsigned long rx_exp_table[32];
static unsigned long *interrupt_vector[NR_IRQS];

void __init setup_vector(void)
{
	int i;
	for (i = 0; i < NR_IRQS; i++)
		interrupt_vector[i] = &rx_int_table[i];
	__asm__ volatile("mvtc %0,intb"::"i"(interrupt_vector));
}


void __init init_IRQ(void)
{
	setup_vector();
	irqchip_init();
}

asmlinkage int do_IRQ(unsigned int irq, struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();
	generic_handle_irq(irq);
	irq_exit();

	set_irq_regs(old_regs);
	return 1;
}
