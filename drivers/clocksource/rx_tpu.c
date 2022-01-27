/*
 *  RX TPU / MTU2 clocksource driver
 *
 *  Copyright 2015 Yoshinori Sato <ysato@users.sourcefoge.jp>
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define TCR	0x0
#define TSR	0x5
#define TCNT	0x6

struct tpu_priv {
	struct clocksource cs;
	void __iomem *mapbase1;
	void __iomem *mapbase2;
	void __iomem *tstr;
	raw_spinlock_t lock;
	unsigned int cs_enabled;
};

static inline unsigned long read_tcnt32(struct tpu_priv *p)
{
	unsigned long tcnt;

	tcnt = ioread16(p->mapbase1 + TCNT) << 16;
	tcnt |= ioread16(p->mapbase2 + TCNT);
	return tcnt;
}

static void tpu_get_counter(struct tpu_priv *p, unsigned long long *val)
{
	unsigned long v1, v2, v3;

	/* Make sure the timer value is stable. Stolen from acpi_pm.c */
	do {
		v1 = read_tcnt32(p);
		v2 = read_tcnt32(p);
		v3 = read_tcnt32(p);
	} while (unlikely((v1 > v2 && v1 < v3) || (v2 > v3 && v2 < v1) ||
			  (v3 > v1 && v3 < v2)));

	*val = v2;
}

static inline struct tpu_priv *cs_to_priv(struct clocksource *cs)
{
	return container_of(cs, struct tpu_priv, cs);
}

static u64 tpu_clocksource_read(struct clocksource *cs)
{
	struct tpu_priv *p = cs_to_priv(cs);
	unsigned long flags;
	unsigned long long value;

	raw_spin_lock_irqsave(&p->lock, flags);
	tpu_get_counter(p, &value);
	raw_spin_unlock_irqrestore(&p->lock, flags);

	return value;
}

static int tpu_clocksource_enable(struct clocksource *cs)
{
	struct tpu_priv *p = cs_to_priv(cs);

	WARN_ON(p->cs_enabled);

	iowrite16(0, p->mapbase1 + TCNT);
	iowrite16(0, p->mapbase2 + TCNT);
	iowrite8(0x0f, p->mapbase1 + TCR);
	iowrite8(0x03, p->mapbase2 + TCR);
	iowrite8(ioread8(p->tstr) | 0x06, p->tstr);

	p->cs_enabled = true;
	return 0;
}

static void tpu_clocksource_disable(struct clocksource *cs)
{
	struct tpu_priv *p = cs_to_priv(cs);

	WARN_ON(!p->cs_enabled);

	iowrite8(ioread8(p->tstr) & ~0x06, p->tstr);
	p->cs_enabled = false;
}

static struct tpu_priv tpu_priv = {
	.cs = {
		.rating = 200,
		.read = tpu_clocksource_read,
		.enable = tpu_clocksource_enable,
		.disable = tpu_clocksource_disable,
		.mask = CLOCKSOURCE_MASK(sizeof(unsigned long) * 8),
		.flags = CLOCK_SOURCE_IS_CONTINUOUS,
	},
};

static void __init rx_tpu_init(struct device_node *node)
{
	void __iomem *base;
	struct clk *clk;

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("failed to get clock for clocksource\n");
		return;
	}

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("failed to map registers for clocksource\n");
		goto free_clk;
	}

	tpu_priv.cs.name = "rx-tpu";
	tpu_priv.mapbase1 = base + 0x20;
	tpu_priv.mapbase2 = base + 0x30;
	tpu_priv.tstr =  base;
	clocksource_register_hz(&tpu_priv.cs, clk_get_rate(clk) / 64);

	return;

free_clk:
	clk_put(clk);
}

static void __init rx_mtu_init(struct device_node *node)
{
	void __iomem *chbase, *tstr;
	struct clk *clk;

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("failed to get clock for clocksource\n");
		return;
	}

	chbase = of_iomap(node, 0);
	if (!chbase) {
		pr_err("failed to map registers for clocksource\n");
		goto free_clk;
	}

	tstr = of_iomap(node, 1);
	if (!tstr) {
		pr_err("failed to map registers for clocksource\n");
		goto unmap_base;
	}

	tpu_priv.cs.name = "rx-mtu2";
	tpu_priv.mapbase1 = chbase;
	tpu_priv.mapbase2 = chbase + 0x80;
	tpu_priv.tstr =  tstr;
	clocksource_register_hz(&tpu_priv.cs, clk_get_rate(clk) / 64);

	return;
unmap_base:
	iounmap(chbase);
free_clk:
	clk_put(clk);
}

TIMER_OF_DECLARE(rx_tpu, "renesas,rx-tpu", rx_tpu_init);
TIMER_OF_DECLARE(rx_mtu, "renesas,rx-mtu2", rx_mtu_init);
