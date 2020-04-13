// SPDX-License-Identifier: GPL-2.0
/*
 * H8/300H interrupt controller driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/io.h>

static const char ipr_bit[] = {
	 7,  6,  5,  5,
	 4,  4,  4,  4,  3,  3,  3,  3,
	 2,  2,  2,  2,  1,  1,  1,  1,
	 0,  0,  0,  0, 15, 15, 15, 15,
	14, 14, 14, 14, 13, 13, 13, 13,
	-1, -1, -1, -1, 11, 11, 11, 11,
	10, 10, 10, 10,  9,  9,  9,  9,
};

#define NR_IRQS 64
#define EXTIRQ 12
#define INTIRQ 20

#define ISCR (intc_baseaddr + 2)
#define IER (intc_baseaddr + 3)
#define ISR (intc_baseaddr + 4)
#define IPR (intc_baseaddr + 6)

static void h8300h_mask(struct irq_data *data)
{
	int bit;
	int irq = data->irq;
	void __iomem *intc_baseaddr;

	intc_baseaddr = data->domain->host_data;
	if (irq < INTIRQ)
		ctrl_bclr(irq - EXTIRQ, IER);
	bit = ipr_bit[irq - EXTIRQ];
	if (bit >= 0)
		ctrl_bclr(bit & 7, IPR + bit / 8);
}

static void h8300h_unmask(struct irq_data *data)
{
	int bit;
	int irq = data->irq;
	void __iomem *intc_baseaddr;

	intc_baseaddr = data->domain->host_data;
	if (irq < INTIRQ) {
		ctrl_bclr(irq - EXTIRQ, ISR);
		ctrl_bset(irq - EXTIRQ, IER);
	}
}

static void h8300h_eoi(struct irq_data *data)
{
	int bit;
	int irq = data->irq;
	void __iomem *intc_baseaddr;

	intc_baseaddr = data->domain->host_data;
	if (irq < INTIRQ) {
		bit = irq - EXTIRQ;
		ctrl_bclr(bit, ISR);
	}
}

static int h8300h_set_type(struct irq_data *data, unsigned int type)
{
	int irq = data->irq;
	void __iomem *intc_baseaddr;

	intc_baseaddr = data->domain->host_data;
	if (irq < INTIRQ) {
		switch (type) {
		case IRQ_TYPE_EDGE_FALLING:
			ctrl_bclr(irq - EXTIRQ, ISCR);
			return 0;
		case IRQ_TYPE_LEVEL_LOW:
			ctrl_bset(irq - EXTIRQ, ISCR);
			return 0;
		}
	}
	return -EINVAL;

}

static struct irq_chip h8300h_irq_chip = {
	.name		= "H8/300H-INTC",
	.irq_unmask	= h8300h_unmask,
	.irq_mask	= h8300h_mask,
	.irq_disable	= h8300h_mask,
	.irq_eoi	= h8300h_eoi,
	.irq_set_type	= h8300h_set_type,
};

static int irq_map(struct irq_domain *d, unsigned int virq,
		   irq_hw_number_t hw_irq_num)
{
	irq_set_chip_and_handler(virq, &h8300h_irq_chip,
				 handle_simple_irq);

	return 0;
}

static const struct irq_domain_ops irq_ops = {
       .map    = irq_map,
       .xlate  = irq_domain_xlate_onecell,
};

static int __init h8300h_intc_of_init(struct device_node *intc,
				      struct device_node *parent)
{
	struct irq_domain *domain;
	void __iomem *intc_baseaddr;

	intc_baseaddr = of_iomap(intc, 0);
	BUG_ON(!intc_baseaddr);

	/* All interrupt priority low */
	iowrite8(0x00, IPR + 0);
	iowrite8(0x00, IPR + 1);

	domain = irq_domain_add_linear(intc, NR_IRQS, &irq_ops, intc_baseaddr);
	BUG_ON(!domain);
	irq_set_default_host(domain);
	irq_domain_associate_many(domain, 0, 0, NR_IRQS);
	return 0;
}

IRQCHIP_DECLARE(h8300h_intc, "renesas,h8300h-intc", h8300h_intc_of_init);
