/*
 * RX interrupt controller driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#define IRBASE	0x0000
#define IERBASE 0x0200
#define IPRBASE 0x0300

#define IR (icu_base + IRBASE)
#define IER (icu_base + IERBASE)
#define IPR (icu_base + IPRBASE)

static void *icu_base;

static void disable_icua_irq(struct irq_data *data)
{
	void __iomem *ier = (void *)(IER + (data->irq >> 3));
	unsigned char val;
	val = ioread8(ier);
	val &= ~(1 << (data->irq & 7));
	iowrite8(val, ier);
}

static void enable_icua_irq(struct irq_data *data)
{
	void __iomem *ier = (void *)(IER + (data->irq >> 3));
	unsigned char val;
	val = ioread8(ier);
	val |= 1 << (data->irq & 7);
	iowrite8(val, ier);
}

static void icua_eoi(struct irq_data *data)
{
	iowrite8(0, (void *)(IR + data->irq));
}

static struct irq_chip chip = {
	.name	      = "RX-ICUa",
	.irq_mask     = disable_icua_irq,
	.irq_unmask   = enable_icua_irq,
	.irq_eoi      = icua_eoi,
	.irq_mask_ack = disable_icua_irq,
};

static int irq_map(struct irq_domain *h, unsigned int virq,
		   irq_hw_number_t hw_irq_num)
{
       irq_set_chip_and_handler(virq, &chip, handle_fasteoi_irq);

       return 0;
}

static struct irq_domain_ops irq_ops = {
       .map    = irq_map,
       .xlate  = irq_domain_xlate_onecell,
};

static int __init rx_icu_of_init(struct device_node *icu,
				 struct device_node *parent)
{
	struct irq_domain *domain;
	int i;

	icu_base = of_iomap(icu, 0);
	domain = irq_domain_add_linear(icu, NR_IRQS, &irq_ops, NULL);
	BUG_ON(!domain);
	irq_set_default_host(domain);
	irq_domain_associate_many(domain, 0, 0, NR_IRQS);
	for (i = 0; i < 0x90; i++)
		__raw_writeb(1, (void __iomem *)(IPR + i));
	return 0;
}

IRQCHIP_DECLARE(rxicu, "renesas,rx-icu", rx_icu_of_init);
