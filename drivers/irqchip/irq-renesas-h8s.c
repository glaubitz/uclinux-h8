/*
 * H8S interrupt contoller driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/io.h>

static void __iomem *pri_baseaddr;
static void __iomem *ctl_baseaddr;

#define IPRA (pri_baseaddr)
#define IPRE (pri_baseaddr + 8)
#define ISCRH (pri_baseaddr + 26)
#define ISCRL (pri_baseaddr + 28)

#define IER  (ctl_baseaddr + 1)
#define ISR  (ctl_baseaddr + 3)

#define INTIRQ 32

static const unsigned char ipr_table[] = {
	0x03, 0x02, 0x01, 0x00, 0x13, 0x12, 0x11, 0x10, /* 16 - 23 */
	0x23, 0x22, 0x21, 0x20, 0x33, 0x32, 0x31, 0x30, /* 24 - 31 */
	0x43, 0x42, 0x41, 0x40, 0x53, 0x53, 0x52, 0x52, /* 32 - 39 */
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, /* 40 - 47 */
	0x50, 0x50, 0x50, 0x50, 0x63, 0x63, 0x63, 0x63, /* 48 - 55 */
	0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, /* 56 - 63 */
	0x61, 0x61, 0x61, 0x61, 0x60, 0x60, 0x60, 0x60, /* 64 - 71 */
	0x73, 0x73, 0x73, 0x73, 0x72, 0x72, 0x72, 0x72, /* 72 - 79 */
	0x71, 0x71, 0x71, 0x71, 0x70, 0x83, 0x82, 0x81, /* 80 - 87 */
	0x80, 0x80, 0x80, 0x80, 0x93, 0x93, 0x93, 0x93, /* 88 - 95 */
	0x92, 0x92, 0x92, 0x92, 0x91, 0x91, 0x91, 0x91, /* 96 - 103 */
	0x90, 0x90, 0x90, 0x90, 0xa3, 0xa3, 0xa3, 0xa3, /* 104 - 111 */
	0xa2, 0xa2, 0xa2, 0xa2, 0xa1, 0xa1, 0xa1, 0xa1, /* 112 - 119 */
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, /* 120 - 127 */
};

#define ctrl_bclr16(b, a) iowrite16be(ioread16be(a) & ~(1 << (b)), a)

static void h8s_mask(struct irq_data *data)
{
	int pos;
	void __iomem *addr;
	unsigned short pri;
	int irq = data->irq;

	addr = IPRA + ((ipr_table[irq - 16] & 0xf0) >> 3);
	pos = (ipr_table[irq - 16] & 0x0f) * 4;
	pri = ~(0x000f << pos);
	pri &= ioread16be(addr);
	iowrite16be(pri, addr);
}

static void h8s_unmask(struct irq_data *data)
{
	int pos;
	void __iomem *addr;
	unsigned short pri;
	int irq = data->irq;

	addr = IPRA + ((ipr_table[irq - 16] & 0xf0) >> 3);
	pos = (ipr_table[irq - 16] & 0x0f) * 4;
	pri = ~(0x000f << pos);
	pri &= ioread16be(addr);
	pri |= 1 << pos;
	iowrite16be(pri, addr);
}

static void h8s_eoi(struct irq_data *data)
{
	int irq = data->irq;

	if (irq < INTIRQ)
		ctrl_bclr16(irq - 16, ISR);
}

static int h8s_set_type(struct irq_data *data, unsigned int type)
{
	int irq = data->irq;
	void __iomem *iscr;
	int bit;
	u16 iscr_val;
	if (irq < INTIRQ) {
		irq -= 16;
		if (irq < 8) {
			iscr = ISCRL;
			bit = irq * 2;
		} else {
			iscr = ISCRH;
			bit = (irq - 8) * 2;
		}
		iscr_val = ioread16be(iscr) & ~(3 << bit);
		switch (type) {
		case IRQ_TYPE_EDGE_FALLING:
			iowrite16be(iscr_val | (1 << bit), iscr);
			return 0;
		case IRQ_TYPE_EDGE_RISING:
			iowrite16be(iscr_val | (2 << bit), iscr);
			return 0;
		case IRQ_TYPE_EDGE_BOTH:
			iowrite16be(iscr_val | (3 << bit), iscr);
			return 0;
		case IRQ_TYPE_LEVEL_LOW:
			iowrite16be(iscr_val, iscr);
			return 0;
		}
	}
	return -EINVAL;
}

struct irq_chip h8s_irq_chip = {
	.name		= "H8S-INTC",
	.irq_unmask	= h8s_unmask,
	.irq_mask	= h8s_mask,
	.irq_disable	= h8s_mask,
	.irq_eoi	= h8s_eoi,
	.irq_set_type	= h8s_set_type,
};

static __init int irq_map(struct irq_domain *d, unsigned int virq,
			  irq_hw_number_t hw_irq_num)
{
	irq_set_chip_and_handler(virq, &h8s_irq_chip, handle_fasteoi_irq);
	return 0;
}

static struct irq_domain_ops irq_ops = {
	.xlate	= irq_domain_xlate_twocell,
	.map    = irq_map,
};

static int __init h8s_intc_of_init(struct device_node *intc,
				   struct device_node *parent)
{
	struct irq_domain *domain;
	int n;
	u16 itsr;

	pri_baseaddr = of_iomap(intc, 0);
	ctl_baseaddr = of_iomap(intc, 1);
	BUG_ON(!pri_baseaddr || !ctl_baseaddr);

	/* Internal interrupt priority is 0 (disable) */
	for (n = 0; n <= 'k' - 'a'; n++)
		iowrite16be(0x0000, IPRA + (n * 2));
	ioread16be(ISR);
	iowrite16be(0x0000, ISR);
	iowrite16be(0xffff, IER);
	domain = irq_domain_add_linear(intc, NR_IRQS, &irq_ops, NULL);
	BUG_ON(!domain);
	irq_set_default_host(domain);
	irq_domain_associate_many(domain, 0, 0, NR_IRQS);
	if (of_property_read_u16(intc, "renesas,itsr", &itsr))
		iowrite16be(itsr, pri_baseaddr + 0x16);
	return 0;
}

IRQCHIP_DECLARE(h8s_intc, "renesas,h8s-intc", h8s_intc_of_init);
