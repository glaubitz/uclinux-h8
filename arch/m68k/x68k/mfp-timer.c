/*
 * X68000 MFP Timer
 * Copyright 2016 Yoshinori Sato
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/clockchips.h>

#define INPUT_FREQ (4000000 / 200)
#define MFP_IRQ 12
#if !defined(CONFIG_MMU)
#define TDDR (void *)0xe88025
#define TCDCR (void *)0xe8801d
#define IERB (void *)0xe88009
#define IMRB (void *)0xe88015
#else
#define TDDR (void *)0xffe88025
#define TCDCR (void *)0xffe8801d
#define IERB (void *)0xffe88009
#define IMRB (void *)0xffe88015
#endif

static irqreturn_t interrupt(int irq, void *dev_id)
{
	struct clock_event_device *ced = (struct clock_event_device *)dev_id;

	ced->event_handler(ced);
	return IRQ_HANDLED;
}

static int periodic(struct clock_event_device *ced)
{
	__raw_writeb((INPUT_FREQ + HZ/2) / HZ, TDDR);
	__raw_writeb(0x07, TCDCR);
	__raw_writeb(__raw_readb(IERB) | 0x10, IERB);
	__raw_writeb(__raw_readb(IMRB) | 0x10, IMRB);
	return 0;
}

static struct clock_event_device mfp_ced = {
	.name = "timer",
	.features = CLOCK_EVT_FEAT_PERIODIC,
	.rating = 200,
	.set_state_periodic = periodic,
};

void __init hw_timer_init(irq_handler_t handler)
{
	int ret;
	ret = request_irq(MFP_IRQ, interrupt, IRQF_TIMER, "timer", &mfp_ced);
	if (ret < 0) {
		pr_err("%s: Failed to request_irq %d\n", __func__, ret);
		return;
	}
	clockevents_config_and_register(&mfp_ced, INPUT_FREQ, 2, 0xff);
}
