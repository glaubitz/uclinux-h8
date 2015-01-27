/*
 *  linux/arch/h8300/kernel/timer/timer16.c
 *
 *  Yoshinori Sato <ysato@users.sourcefoge.jp>
 *
 *  16bit Timer clockevent device
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clockchips.h>
#include <linux/module.h>
#include <linux/clk.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/timer.h>

#define TSTR	0
#define TSNC	1
#define TMDR	2
#define TOLR	3
#define TISRA	4
#define TISRB	5
#define TISRC	6

#define TCR	0
#define TIOR	1
#define TCNT	2
#define GRA	4
#define GRB	6

#define FLAG_REPROGRAM (1 << 0)
#define FLAG_SKIPEVENT (1 << 1)
#define FLAG_IRQCONTEXT (1 << 2)
#define FLAG_STARTED (1 << 3)

#define ONESHOT  0
#define PERIODIC 1

#define RELATIVE 0
#define ABSOLUTE 1

struct timer16_priv {
	struct platform_device *pdev;
	struct clock_event_device ced;
	struct irqaction irqaction;
	unsigned long mapbase;
	unsigned long mapcommon;
	unsigned long flags;
	unsigned int rate;
	unsigned short gra;
	unsigned char enb;
	unsigned char imfa;
	unsigned char imiea;
	unsigned char ovf;
	raw_spinlock_t lock;
	struct clk *clk;
};

static unsigned long timer16_get_counter(struct timer16_priv *p)
{
	unsigned long v1, v2, v3;
	int o1, o2;

	o1 = ctrl_inb(p->mapcommon + TISRC) & p->ovf;

	/* Make sure the timer value is stable. Stolen from acpi_pm.c */
	do {
		o2 = o1;
		v1 = ctrl_inw(p->mapbase + TCNT);
		v2 = ctrl_inw(p->mapbase + TCNT);
		v3 = ctrl_inw(p->mapbase + TCNT);
		o1 = ctrl_inb(p->mapcommon + TISRC) & p->ovf;
	} while (unlikely((o1 != o2) || (v1 > v2 && v1 < v3)
			  || (v2 > v3 && v2 < v1) || (v3 > v1 && v3 < v2)));

	v2 |= 0x10000;
	return v2;
}


static irqreturn_t timer16_interrupt(int irq, void *dev_id)
{
	struct timer16_priv *p = (struct timer16_priv *)dev_id;

	ctrl_outb(ctrl_inb(p->mapcommon + TISRA) & ~p->imfa,
		  p->mapcommon + TISRA);

	p->flags |= FLAG_IRQCONTEXT;
	ctrl_outw(p->gra, p->mapbase + GRA);
	if (!(p->flags & FLAG_SKIPEVENT)) {
		if (p->ced.mode == CLOCK_EVT_MODE_ONESHOT) {
			ctrl_outb(ctrl_inb(p->mapcommon + TSTR) & ~p->enb,
				  p->mapcommon + TISRA);
		}
		p->ced.event_handler(&p->ced);
	}

	p->flags &= ~(FLAG_SKIPEVENT | FLAG_IRQCONTEXT);

	return IRQ_HANDLED;
}

static void timer16_set_next(struct timer16_priv *p, unsigned long delta)
{
	unsigned long flags;
	unsigned long now;

	raw_spin_lock_irqsave(&p->lock, flags);
	if (delta >= 0x10000)
		dev_warn(&p->pdev->dev, "delta out of range\n");
	now = timer16_get_counter(p);
	p->gra = delta;
	ctrl_outb(ctrl_inb(p->mapcommon + TISRA) | p->imiea,
		  p->mapcommon + TISRA);
	if (delta > now)
		ctrl_outw(delta, p->mapbase + GRA);
	else
		ctrl_outw(now + 1, p->mapbase + GRA);

	raw_spin_unlock_irqrestore(&p->lock, flags);
}

static int timer16_enable(struct timer16_priv *p)
{
	p->rate = clk_get_rate(p->clk) / 8;
	ctrl_outw(0xffff, p->mapbase + GRA);
	ctrl_outw(0x0000, p->mapbase + TCNT);
	ctrl_outb(0xa3, p->mapbase + TCR);
	ctrl_outb(ctrl_inb(p->mapcommon + TSTR) | p->enb,
		  p->mapcommon + TSTR);

	return 0;
}

static int timer16_start(struct timer16_priv *p)
{
	int ret = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&p->lock, flags);

	if (!(p->flags & FLAG_STARTED))
		ret = timer16_enable(p);

	if (ret)
		goto out;
	p->flags |= FLAG_STARTED;

 out:
	raw_spin_unlock_irqrestore(&p->lock, flags);

	return ret;
}

static void timer16_stop(struct timer16_priv *p)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&p->lock, flags);

	ctrl_outb(ctrl_inb(p->mapcommon + TSTR) & ~p->enb,
		  p->mapcommon + TSTR);

	raw_spin_unlock_irqrestore(&p->lock, flags);
}

static inline struct timer16_priv *ced_to_priv(struct clock_event_device *ced)
{
	return container_of(ced, struct timer16_priv, ced);
}

static void timer16_clock_event_start(struct timer16_priv *p, int periodic)
{
	struct clock_event_device *ced = &p->ced;

	timer16_start(p);

	ced->shift = 32;
	ced->mult = div_sc(p->rate, NSEC_PER_SEC, ced->shift);
	ced->max_delta_ns = clockevent_delta2ns(0xffff, ced);
	ced->min_delta_ns = clockevent_delta2ns(0x0001, ced);

	timer16_set_next(p, periodic?(p->rate + HZ/2) / HZ:0x10000);
}

static void timer16_clock_event_mode(enum clock_event_mode mode,
				    struct clock_event_device *ced)
{
	struct timer16_priv *p = ced_to_priv(ced);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		dev_info(&p->pdev->dev, "used for periodic clock events\n");
		timer16_stop(p);
		timer16_clock_event_start(p, PERIODIC);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		dev_info(&p->pdev->dev, "used for oneshot clock events\n");
		timer16_stop(p);
		timer16_clock_event_start(p, ONESHOT);
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		timer16_stop(p);
		break;
	default:
		break;
	}
}

static int timer16_clock_event_next(unsigned long delta,
				   struct clock_event_device *ced)
{
	struct timer16_priv *p = ced_to_priv(ced);

	BUG_ON(ced->mode != CLOCK_EVT_MODE_ONESHOT);
	timer16_set_next(p, delta - 1);

	return 0;
}

#define REG_CH   0
#define REG_COMM 1

static int timer16_setup(struct timer16_priv *p, struct platform_device *pdev)
{
	struct h8300_timer16_config *cfg = dev_get_platdata(&pdev->dev);
	struct resource *res[2];
	int ret, irq;

	memset(p, 0, sizeof(*p));
	p->pdev = pdev;

	res[REG_CH] = platform_get_resource(p->pdev,
					    IORESOURCE_MEM, REG_CH);
	res[REG_COMM] = platform_get_resource(p->pdev,
					      IORESOURCE_MEM, REG_COMM);
	if (!res[REG_CH] || !res[REG_COMM]) {
		dev_err(&p->pdev->dev, "failed to get I/O memory\n");
		return -ENXIO;
	}
	irq = platform_get_irq(p->pdev, 0);
	if (irq < 0) {
		dev_err(&p->pdev->dev, "failed to get irq\n");
		return irq;
	}

	p->clk = clk_get(&p->pdev->dev, "peripheral_clk");
	if (IS_ERR(p->clk)) {
		dev_err(&p->pdev->dev, "can't get clk\n");
		return PTR_ERR(p->clk);
	}

	p->pdev = pdev;
	p->mapbase = res[REG_CH]->start;
	p->mapcommon = res[REG_COMM]->start;
	p->enb = 1 << cfg->enb;
	p->imfa = 1 << cfg->imfa;
	p->imiea = 1 << cfg->imiea;
	p->ced.name = pdev->name;
	p->ced.features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	p->ced.rating = 200;
	p->ced.cpumask = cpumask_of(0);
	p->ced.set_next_event = timer16_clock_event_next;
	p->ced.set_mode = timer16_clock_event_mode;

	ret = request_irq(irq, timer16_interrupt,
			  IRQF_DISABLED | IRQF_TIMER, pdev->name, p);
	if (ret < 0) {
		dev_err(&p->pdev->dev, "failed to request irq %d\n", irq);
		return ret;
	}

	clockevents_register_device(&p->ced);

	return 0;
}

static int timer16_probe(struct platform_device *pdev)
{
	struct timer16_priv *p = platform_get_drvdata(pdev);

	if (p) {
		dev_info(&pdev->dev, "kept as earlytimer\n");
		return 0;
	}

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL) {
		dev_err(&pdev->dev, "failed to allocate driver data."
			" out of memory.\n");
		return -ENOMEM;
	}

	return timer16_setup(p, pdev);
}

static int timer16_remove(struct platform_device *pdev)
{
	return -EBUSY;
}

static struct platform_driver timer16_driver = {
	.probe		= timer16_probe,
	.remove		= timer16_remove,
	.driver		= {
		.name	= "h8300h-16timer",
	}
};

static int __init timer16_init(void)
{
	return platform_driver_register(&timer16_driver);
}

static void __exit timer16_exit(void)
{
	platform_driver_unregister(&timer16_driver);
}

subsys_initcall(timer16_init);
module_exit(timer16_exit);
MODULE_AUTHOR("Yoshinori Sato");
MODULE_DESCRIPTION("H8/300H 16bit Timer Driver");
MODULE_LICENSE("GPL v2");
