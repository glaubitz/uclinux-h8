// SPDX-License-Identifier: GPL-2.0
/*
 * linux/arch/h8300/kernel/irq.c
 *
 * Copyright 2014-2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of_irq.h>
#include <asm/traps.h>

#ifdef CONFIG_RAMKERNEL
#define JMP_OP 0x5a000000
#define JSR_OP 0x5e000000
#define VECTOR(address) ((JMP_OP)|((unsigned long)address))
#define REDIRECT(address) ((JSR_OP)|((unsigned long)address))
#define CPU_VECTOR ((unsigned long *)0x000000)
#define ADDR_MASK (0xffffff)

extern unsigned long *_interrupt_redirect_table;

typedef void (*h8300_vector)(void);

static const h8300_vector __initconst trap_table[] = {
	0, 0, 0, 0,
	_trace_break,
	0, 0,
	_nmi,
	_system_call,
	0, 0,
	_trace_break,
};

static unsigned long __init *get_vector_address(void)
{
	unsigned long *rom_vector = CPU_VECTOR;
	unsigned long base, tmp;
	int vec_no;

	base = (rom_vector[2] & ADDR_MASK) - (2 * 4);

	/* check romvector format */
	for (vec_no = 2; vec_no <= 15; vec_no++) {
		if ((base + vec_no * 4) !=
		    (rom_vector[vec_no] & ADDR_MASK))
			return NULL;
	}

	/* writerble? */
	tmp = ~(*(volatile unsigned long *)base);
	(*(volatile unsigned long *)base) = tmp;
	if ((*(volatile unsigned long *)base) != tmp)
		return NULL;
	return (unsigned long *)base;
}

static void __init setup_vector(void)
{
	int i;
	unsigned long *ramvec, *ramvec_p;
	const h8300_vector *trap_entry;

	ramvec = get_vector_address();
	if (ramvec == NULL)
		panic("interrupt vector serup failed.");
	else
		pr_debug("virtual vector at 0x%p\n", ramvec);

	/* create redirect table */
	ramvec_p = ramvec;
	trap_entry = trap_table;
	for (i = 0; i < NR_IRQS; i++) {
		if (i < 12) {
			if (*trap_entry)
				*ramvec_p = VECTOR(*trap_entry);
			ramvec_p++;
			trap_entry++;
		} else
			*ramvec_p++ = REDIRECT(_interrupt_entry);
	}
	_interrupt_redirect_table = ramvec;
}
#else
void setup_vector(void)
{
	/* noting do */
}
#endif

void __init init_IRQ(void)
{
	setup_vector();
	irqchip_init();
}

asmlinkage void do_IRQ(int irq)
{
	irq_enter();
	generic_handle_irq(irq);
	irq_exit();
}
