/*
 *  H8S TPU Driver
 *
 *  Copyright 2015 Yoshinori Sato <ysato@users.sourcefoge.jp>
 *
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clocksource.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/irq.h>

#define TCR	0
#define TMDR	1
#define TIOR	2
#define TER	4
#define TSR	5
#define TCNT	6
#define TGRA	8
#define TGRB	10
#define TGRC	12
#define TGRD	14

struct tpu_priv {
	struct clocksource cs;
	struct clk *clk;
	unsigned long mapbase1;
	unsigned long mapbase2;
	raw_spinlock_t lock;
	unsigned int cs_enabled;
};

static inline unsigned long read_tcnt32(struct tpu_priv *p)
{
	unsigned long tcnt;

	tcnt = ctrl_inw(p->mapbase1 + TCNT) << 16;
	tcnt |= ctrl_inw(p->mapbase2 + TCNT);
	return tcnt;
}

static int tpu_get_counter(struct tpu_priv *p, unsigned long long *val)
{
	unsigned long v1, v2, v3;
	int o1, o2;

	o1 = ctrl_inb(p->mapbase1 + TSR) & 0x10;

	/* Make sure the timer value is stable. Stolen from acpi_pm.c */
	do {
		o2 = o1;
		v1 = read_tcnt32(p);
		v2 = read_tcnt32(p);
		v3 = read_tcnt32(p);
		o1 = ctrl_inb(p->mapbase1 + TSR) & 0x10;
	} while (unlikely((o1 != o2) || (v1 > v2 && v1 < v3)
			  || (v2 > v3 && v2 < v1) || (v3 > v1 && v3 < v2)));

	*val = v2;
	return o1;
}

static inline struct tpu_priv *cs_to_priv(struct clocksource *cs)
{
	return container_of(cs, struct tpu_priv, cs);
}

static cycle_t tpu_clocksource_read(struct clocksource *cs)
{
	struct tpu_priv *p = cs_to_priv(cs);
	unsigned long flags;
	unsigned long long value;

	raw_spin_lock_irqsave(&p->lock, flags);
	if (tpu_get_counter(p, &value))
		value += 0x100000000;
	raw_spin_unlock_irqrestore(&p->lock, flags);

	return value;
}

static int tpu_clocksource_enable(struct clocksource *cs)
{
	struct tpu_priv *p = cs_to_priv(cs);

	WARN_ON(p->cs_enabled);

	ctrl_outw(0, p->mapbase1 + TCNT);
	ctrl_outw(0, p->mapbase2 + TCNT);
	ctrl_outb(0x0f, p->mapbase1 + TCR);
	ctrl_outb(0x03, p->mapbase2 + TCR);

	p->cs_enabled = true;
	return 0;
}

static void tpu_clocksource_disable(struct clocksource *cs)
{
	struct tpu_priv *p = cs_to_priv(cs);

	WARN_ON(!p->cs_enabled);

	ctrl_outb(0, p->mapbase1 + TCR);
	ctrl_outb(0, p->mapbase2 + TCR);
	p->cs_enabled = false;
}

#define CH_L 0
#define CH_H 1

static void __init h8300_tpu_init(struct device_node *node)
{
	void __iomem *base[2] = {NULL, NULL};
	int i;
	struct tpu_priv *p;
	struct clk *clk;

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("failed to get clock for clocksource\n");
		return;
	}

	for (i = 0; i < 2; i++) {
		base[i] = of_iomap(node, 0);
		if (!base[i]) {
			pr_err("failed to map registers for clocksource\n");
			goto err_iomap;
		}
	}

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p) {
		pr_err("failed to allocate memory for clocksource\n");
		goto err_alloc;
	}

	p->mapbase1 = (unsigned long)base[CH_L];
	p->mapbase2 = (unsigned long)base[CH_H];

	p->cs.name = node->name;
	p->cs.rating = 200;
	p->cs.read = tpu_clocksource_read;
	p->cs.enable = tpu_clocksource_enable;
	p->cs.disable = tpu_clocksource_disable;
	p->cs.mask = CLOCKSOURCE_MASK(sizeof(unsigned long) * 8);
	p->cs.flags = CLOCK_SOURCE_IS_CONTINUOUS;
	clocksource_register_hz(&p->cs, clk_get_rate(p->clk) / 64);

	return;

err_alloc:

err_iomap:
	for (i = 0; i < 2; i++) {
		if(base[i])
			iounmap(base[i]);
	}
	clk_put(clk);
}

CLOCKSOURCE_OF_DECLARE(h8300_tpu, "renesas,tpu", h8300_tpu_init);
