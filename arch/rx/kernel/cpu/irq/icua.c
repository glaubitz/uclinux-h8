/*
 * Interrupt handling for RX ICUa
 *
 * Copyright (C) 2011  Yoshinori Sato
 *
 */

#include <linux/linkage.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#include <asm/io.h>

#define IR  (0x00087000)
#define IER (0x00087200)
#define IPR (0x00087300)

static void disable_icua_irq(struct irq_data *data);
static void enable_icua_irq(struct irq_data *data);
static void icua_eoi(struct irq_data *data);

struct irq_chip chip = {
	.name	= "RX-ICUa",
	.irq_mask 	= disable_icua_irq,
	.irq_unmask = enable_icua_irq,
	.irq_eoi = icua_eoi,
	.irq_mask_ack = disable_icua_irq,
};

static void disable_icua_irq(struct irq_data *data)
{
	void __iomem *ier = (void *)(IER + (data->irq >> 3));
	unsigned char val;
	val = __raw_readb(ier);
	val &= ~(1 << (data->irq & 7));
	__raw_writeb(val, ier);
}

static void enable_icua_irq(struct irq_data *data)
{
	void __iomem *ier = (void *)(IER + (data->irq >> 3));
	unsigned char val;
	val = __raw_readb(ier);
	val |= 1 << (data->irq & 7);
	__raw_writeb(val, ier);
}

static void icua_eoi(struct irq_data *data)
{
	__raw_writeb(0, (void *)(IR + data->irq));
}

void __init setup_rx_irq_desc(void)
{
	int i;

	for (i = 16; i < 256; i++) {
		struct irq_desc *irq_desc;

		irq_desc = irq_alloc_desc_at(i, numa_node_id());
		if (unlikely(!irq_desc)) {
			printk(KERN_INFO "can not get irq_desc for %d\n", i);
			continue;
		}

		disable_irq_nosync(i);
		irq_set_chip_and_handler_name(i, &chip, handle_fasteoi_irq, "icua");
	}
	for (i = 0; i < 0x90; i++)
		__raw_writeb(1, (void __iomem *)(IPR + i));
}
