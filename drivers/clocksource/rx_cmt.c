/*
 *  RX CMT Driver
 *
 *  Copyright 2015 Yoshinori Sato <ysato@users.sourcefoge.jp>
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/io.h>
#include <asm/irq.h>


enum {
	STR = 0,
	CMCR = 2,
	CMCNT = 4,
	CMCOR = 6
};

#define SCALE 128
#define FLAG_STARTED 1

struct cmt_priv {
	struct clock_event_device ced;
	void __iomem *base;
	unsigned long flags;
	unsigned int rate;
};

static irqreturn_t cmt_interrupt(int irq, void *dev_id)
{
	struct cmt_priv *p = dev_id;

	if (clockevent_state_oneshot(&p->ced))
		iowrite16(0x0000, p->base + STR);

	p->ced.event_handler(&p->ced);
	return IRQ_HANDLED;
}

static void cmt_set_next(struct cmt_priv *p, unsigned long delta)
{
	if (delta >= 0x10000)
		pr_warn("delta out of range\n");
	iowrite16(delta, p->base + CMCOR);
	iowrite16(0x0000, p->base + CMCNT);
}

static int cmt_enable(struct cmt_priv *p)
{
	iowrite16(0xffff, p->base + CMCOR);
	iowrite16(0x0000, p->base + CMCNT);
	iowrite16(0x00c2, p->base + CMCR);
	iowrite16(0x0001, p->base + STR);

	return 0;
}

static int cmt_start(struct cmt_priv *p)
{
	int ret;

	if ((p->flags & FLAG_STARTED))
		return 0;

	ret = cmt_enable(p);
	if (!ret)
		p->flags |= FLAG_STARTED;

	return ret;
}

static void cmt_stop(struct cmt_priv *p)
{
	iowrite16be(0x0000, p->base + STR);
}

static inline struct cmt_priv *ced_to_priv(struct clock_event_device *ced)
{
	return container_of(ced, struct cmt_priv, ced);
}

static void cmt_clock_event_start(struct cmt_priv *p, unsigned long delta)
{
	struct clock_event_device *ced = &p->ced;

	cmt_start(p);

	ced->shift = 32;
	ced->mult = div_sc(p->rate, NSEC_PER_SEC, ced->shift);
	ced->max_delta_ns = clockevent_delta2ns(0xffff, ced);
	ced->min_delta_ns = clockevent_delta2ns(0x0001, ced);

	cmt_set_next(p, delta);
}

static int cmt_clock_event_shutdown(struct clock_event_device *ced)
{
	cmt_stop(ced_to_priv(ced));
	return 0;
}

static int cmt_clock_event_periodic(struct clock_event_device *ced)
{
	struct cmt_priv *p = ced_to_priv(ced);

	pr_info("%s: used for periodic clock events\n", ced->name);
	cmt_stop(p);
	cmt_clock_event_start(p, (p->rate + HZ/2) / HZ);

	return 0;
}

static int cmt_clock_event_oneshot(struct clock_event_device *ced)
{
	struct cmt_priv *p = ced_to_priv(ced);

	pr_info("%s: used for oneshot clock events\n", ced->name);
	cmt_stop(p);
	cmt_clock_event_start(p, 0x10000);

	return 0;
}

static int cmt_clock_event_next(unsigned long delta,
				   struct clock_event_device *ced)
{
	struct cmt_priv *p = ced_to_priv(ced);

	BUG_ON(!clockevent_state_oneshot(ced));
	cmt_set_next(p, delta - 1);

	return 0;
}

static struct cmt_priv cmt_priv = {
	.ced = {
		.name = "rx-cmt",
		.features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
		.rating = 200,
		.set_next_event = cmt_clock_event_next,
		.set_state_shutdown = cmt_clock_event_shutdown,
		.set_state_periodic = cmt_clock_event_periodic,
		.set_state_oneshot = cmt_clock_event_oneshot,
	},
};

static void __init cmt_init(struct device_node *node)
{
	void __iomem *base;
	int irq;
	int ret = 0;
	int rate;
	struct clk *clk;

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("failed to get clock for clockevent\n");
		return;
	}

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("failed to map registers for clockevent\n");
		goto free_clk;
	}

	irq = irq_of_parse_and_map(node, 0);
	if (!irq) {
		pr_err("failed to get irq for clockevent\n");
		goto unmap_reg;
	}

	cmt_priv.base = base;

	rate = clk_get_rate(clk) / SCALE;
	if (!rate) {
		pr_err("Failed to get rate for the clocksource\n");
		goto unmap_reg;
	}
	cmt_priv.rate = rate;

	ret = request_irq(irq, cmt_interrupt,
			  IRQF_TIMER, cmt_priv.ced.name, &cmt_priv);
	if (ret < 0) {
		pr_err("failed to request irq %d for clockevent\n", irq);
		goto unmap_reg;
	}

	clockevents_config_and_register(&cmt_priv.ced, rate, 1, 0x0000ffff);

	return;
unmap_reg:
	iounmap(base);
free_clk:
	clk_put(clk);
}

CLOCKSOURCE_OF_DECLARE(rx_cmt, "renesas,rx-cmt", cmt_init);
