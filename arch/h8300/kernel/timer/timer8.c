/*
 *  linux/arch/h8300/kernel/cpu/timer/timer8.c
 *
 *  Yoshinori Sato <ysato@users.sourcefoge.jp>
 *
 *  8bit Timer driver
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
#include <linux/clockchips.h>
#include <linux/module.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/timer.h>

#define _8TCR	0
#define _8TCSR	2
#define TCORA	4
#define TCORB	6
#define _8TCNT	8

#define FLAG_REPROGRAM (1 << 0)
#define FLAG_SKIPEVENT (1 << 1)
#define FLAG_IRQCONTEXT (1 << 2)
#define FLAG_STARTED (1 << 3)

#define ONESHOT  0
#define PERIODIC 1

#define RELATIVE 0
#define ABSOLUTE 1

struct timer8_priv {
	struct platform_device *pdev;
	int mode;
	union {
		struct clocksource cs;
		struct clock_event_device ced;
	} clk;
	struct irqaction irqaction;
	unsigned long mapbase;
	raw_spinlock_t lock;
	unsigned long total_cycles;
	unsigned int cs_enabled;
	unsigned long flags;
	unsigned int rate;
	unsigned short tcora;
	unsigned short div;
};

static const int div_rate[] = {8, 64, 8192};

static unsigned long timer8_get_counter(struct timer8_priv *p)
{
	unsigned long v1, v2, v3;
	int o1, o2;

	o1 = ctrl_inb(p->mapbase + _8TCSR) & 0x20;

	/* Make sure the timer value is stable. Stolen from acpi_pm.c */
	do {
		o2 = o1;
		v1 = ctrl_inw(p->mapbase + _8TCNT);
		v2 = ctrl_inw(p->mapbase + _8TCNT);
		v3 = ctrl_inw(p->mapbase + _8TCNT);
		o1 = ctrl_inb(p->mapbase + _8TCSR) & 0x20;
	} while (unlikely((o1 != o2) || (v1 > v2 && v1 < v3)
			  || (v2 > v3 && v2 < v1) || (v3 > v1 && v3 < v2)));

	v2 |= o1 << 10;
	return v2;
}

static irqreturn_t timer8_interrupt(int irq, void *dev_id)
{
	struct timer8_priv *p = dev_id;

	switch(p->mode) {
	case H8300_TMR8_CLKSRC:
		ctrl_outb(ctrl_inb(p->mapbase + _8TCSR) & ~0x20,
			  p->mapbase + _8TCSR);
		p->total_cycles += 0x10000;
		break;
	case H8300_TMR8_CLKEVTDEV:
		ctrl_outb(ctrl_inb(p->mapbase + _8TCSR) & ~0x40,
			  p->mapbase + _8TCSR);
		p->flags |= FLAG_IRQCONTEXT;
		ctrl_outw(p->tcora, p->mapbase + TCORA);
		if (!(p->flags & FLAG_SKIPEVENT)) {
			if (p->clk.ced.mode == CLOCK_EVT_MODE_ONESHOT)
				ctrl_outw(0x0000, p->mapbase + _8TCR);
			p->clk.ced.event_handler(&p->clk.ced);
		}
		p->flags &= ~(FLAG_SKIPEVENT | FLAG_IRQCONTEXT);
		break;
	}

	return IRQ_HANDLED;
}
	
static inline struct timer8_priv *cs_to_priv(struct clocksource *cs)
{
	return container_of(cs, struct timer8_priv, clk.cs);
}

static cycle_t timer8_clocksource_read(struct clocksource *cs)
{
	struct timer8_priv *p = cs_to_priv(cs);
	unsigned long flags, raw;
	unsigned long value;

	raw_spin_lock_irqsave(&p->lock, flags);
	value = p->total_cycles;
	raw = timer8_get_counter(p);
	raw_spin_unlock_irqrestore(&p->lock, flags);

	return value + raw;
}

static int timer8_clocksource_enable(struct clocksource *cs)
{
	struct timer8_priv *p = cs_to_priv(cs);

	WARN_ON(p->cs_enabled);

	p->total_cycles = 0;
	ctrl_outw(0, p->mapbase + _8TCNT);
	ctrl_outw(0x2400 | p->div, p->mapbase + _8TCR);
	
	p->cs_enabled = true;
	return 0;
}

static void timer8_clocksource_disable(struct clocksource *cs)
{
	struct timer8_priv *p = cs_to_priv(cs);

	WARN_ON(!p->cs_enabled);

	ctrl_outb(0, p->mapbase + _8TCR); 
	p->cs_enabled = false;
}

static void timer8_set_next(struct timer8_priv *p, unsigned long delta)
{
	unsigned long flags;
	unsigned long now;

	raw_spin_lock_irqsave(&p->lock, flags);
	if (delta >= 0x10000)
		dev_warn(&p->pdev->dev, "delta out of range\n");
	now = timer8_get_counter(p);
	p->tcora = delta;
	ctrl_outb(ctrl_inb(p->mapbase + _8TCR) | 0x40, p->mapbase + _8TCR);
	if (delta > now)
		ctrl_outw(delta, p->mapbase + TCORA);
	else
		ctrl_outw(now + 1, p->mapbase + TCORA);

	raw_spin_unlock_irqrestore(&p->lock, flags);
}

static int timer8_enable(struct timer8_priv *p)
{
	p->rate = get_cpu_clock() / div_rate[p->div];
	ctrl_outw(0xffff, p->mapbase + TCORA);
	ctrl_outw(0x0000, p->mapbase + _8TCNT);
	ctrl_outw(0x0c01, p->mapbase + _8TCR);

	return 0;
}

static int timer8_start(struct timer8_priv *p)
{
	int ret = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&p->lock, flags);

	if (!(p->flags & FLAG_STARTED))
		ret = timer8_enable(p);

	if (ret)
		goto out;
	p->flags |= FLAG_STARTED;

 out:
	raw_spin_unlock_irqrestore(&p->lock, flags);

	return ret;
}

static void timer8_stop(struct timer8_priv *p)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&p->lock, flags);

	ctrl_outw(0x0000, p->mapbase + _8TCR);

	raw_spin_unlock_irqrestore(&p->lock, flags);
}

static inline struct timer8_priv *ced_to_priv(struct clock_event_device *ced)
{
	return container_of(ced, struct timer8_priv, clk.ced);
}

static void timer8_clock_event_start(struct timer8_priv *p, int periodic)
{
	struct clock_event_device *ced = &p->clk.ced;

	timer8_start(p);

	ced->shift = 32;
	ced->mult = div_sc(p->rate, NSEC_PER_SEC, ced->shift);
	ced->max_delta_ns = clockevent_delta2ns(0xffff, ced);
	ced->min_delta_ns = clockevent_delta2ns(0x0001, ced);

	timer8_set_next(p, periodic?(p->rate + HZ/2) / HZ:0x10000);
}

static void timer8_clock_event_mode(enum clock_event_mode mode,
				    struct clock_event_device *ced)
{
	struct timer8_priv *p = ced_to_priv(ced);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		dev_info(&p->pdev->dev, "used for periodic clock events\n");
		timer8_stop(p);
		timer8_clock_event_start(p, PERIODIC);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		dev_info(&p->pdev->dev, "used for oneshot clock events\n");
		timer8_stop(p);
		timer8_clock_event_start(p, ONESHOT);
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		timer8_stop(p);
		break;
	default:
		break;
	}
}

static int timer8_clock_event_next(unsigned long delta,
				   struct clock_event_device *ced)
{
	struct timer8_priv *p = ced_to_priv(ced);

	BUG_ON(ced->mode != CLOCK_EVT_MODE_ONESHOT);
	timer8_set_next(p, delta - 1);

	return 0;
}

#define CMI 0
#define OVI 1
static int __init timer8_setup(struct timer8_priv *p, struct platform_device *pdev)
{
	struct h8300_timer8_config *cfg = dev_get_platdata(&pdev->dev);
	struct resource *res;
	int irq[2];
	int ret;

	memset(p, 0, sizeof(*p));
	p->pdev = pdev;

	res = platform_get_resource(p->pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&p->pdev->dev, "failed to get I/O memory\n");
		return -ENXIO;
	}

	irq[CMI] = platform_get_irq(p->pdev, CMI);
	irq[OVI] = platform_get_irq(p->pdev, OVI);
	if (irq[CMI] < 0 || irq[OVI] < 0) {
		dev_err(&p->pdev->dev, "failed to get irq\n");
		return -ENXIO;
	}

	p->mapbase = res->start;

	p->irqaction.name = dev_name(&p->pdev->dev);
	p->irqaction.handler = timer8_interrupt;
	p->irqaction.dev_id = p;
	p->irqaction.flags = IRQF_DISABLED | IRQF_TIMER;

	p->mode = cfg->mode;
	p->div = cfg->div;
	switch(p->mode) {
	case H8300_TMR8_CLKSRC:
		p->clk.cs.name = pdev->name;
		p->clk.cs.rating = cfg->rating;
		p->clk.cs.read = timer8_clocksource_read;
		p->clk.cs.enable = timer8_clocksource_enable;
		p->clk.cs.disable = timer8_clocksource_disable;
		p->clk.cs.mask = CLOCKSOURCE_MASK(sizeof(unsigned long) * 8);
		p->clk.cs.flags = CLOCK_SOURCE_IS_CONTINUOUS;

		if ((ret = setup_irq(irq[OVI], &p->irqaction)) < 0) {
			dev_err(&p->pdev->dev,
				"failed to request irq %d\n", irq[OVI]);
			return ret;
		}
		clocksource_register_hz(&p->clk.cs, 
					get_cpu_clock() / div_rate[p->div]);
		break;
	case H8300_TMR8_CLKEVTDEV:
		p->clk.ced.name = pdev->name;
		p->clk.ced.features = CLOCK_EVT_FEAT_PERIODIC | 
					CLOCK_EVT_FEAT_ONESHOT;
		p->clk.ced.rating = cfg->rating;
		p->clk.ced.cpumask = cpumask_of(0);
		p->clk.ced.set_next_event = timer8_clock_event_next;
		p->clk.ced.set_mode = timer8_clock_event_mode;
		if ((ret = setup_irq(irq[CMI], &p->irqaction)) < 0) {
			dev_err(&p->pdev->dev,
				"failed to request irq %d\n", irq[CMI]);
			return ret;
		}
		clockevents_register_device(&p->clk.ced);
		break;
	}
	platform_set_drvdata(pdev, p);

	return 0;
}

static int timer8_probe(struct platform_device *pdev)
{
	struct timer8_priv *p = platform_get_drvdata(pdev);

	if (p) {
		dev_info(&pdev->dev, "kept as earlytimer\n");
		return 0;
	}

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL) {
		dev_err(&pdev->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	return timer8_setup(p, pdev);
}

static int timer8_remove(struct platform_device *pdev)
{
	return -EBUSY;
}

static struct platform_driver timer8_driver = {
	.probe		= timer8_probe,
	.remove		= timer8_remove,
	.driver		= {
		.name	= "h8300-8timer",
	}
};

static int __init timer8_init(void)
{
	return platform_driver_register(&timer8_driver);
}

static void __exit timer8_exit(void)
{
	platform_driver_unregister(&timer8_driver);
}

early_platform_init("earlytimer", &timer8_driver);
subsys_initcall(timer8_init);
module_exit(timer8_exit);
MODULE_AUTHOR("Yoshinori Sato");
MODULE_DESCRIPTION("H8/300 8bit Timer Driver");
MODULE_LICENSE("GPL v2");
