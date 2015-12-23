#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/profile.h>

#include <asm/io.h>
#include <asm/irq.h>

static const u32 base[] = {0x00088002, 0x00088008, 0x00088012, 0x00088018};
static const u32 str[] = {0x00088000, 0x00088010};
static const int irqno[] = {28, 29, 30, 31};

enum {CMCR = 0, CMCNT = 2, CMCOR = 4};

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */

static void (*tick_handler)(void);

static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	tick_handler();
	return IRQ_HANDLED;
}

static struct irqaction cmt_irq = {
	.name		= "rx-cmt",
	.handler	= timer_interrupt,
	.flags		= IRQF_TIMER,
};

static const int __initdata divide_rate[] = {8, 32, 128, 512};

void __init rx_clk_init(void (*tick)(void))
{
	unsigned int div;
	unsigned int cnt;
	unsigned long p_freq;
	u16 str_r;

	tick_handler = tick;

	p_freq = CONFIG_INPUT_CLOCK_FREQ * CONFIG_PCLK_MULT;
	for (div = 0; div < 4; div++) {
		cnt = p_freq / HZ / divide_rate[div];
		if (cnt < 0x00010000)
			break;
	}

	/* stop all timer (it enable u-boot)*/
	__raw_writew(0, (void __iomem *)str[0]);
	__raw_writew(0, (void __iomem *)str[1]);

	/* initalize timer */
	__raw_writew(0x0000, (void __iomem *)(base[CONFIG_RX_CMT_CH] + CMCNT));
	__raw_writew(cnt, (void __iomem *)(base[CONFIG_RX_CMT_CH] + CMCOR));
	__raw_writew(0x00c0 | div, (void __iomem *)(base[CONFIG_RX_CMT_CH] + CMCR));
	setup_irq(irqno[CONFIG_RX_CMT_CH], &cmt_irq);
	str_r = __raw_readw((void __iomem *)str[CONFIG_RX_CMT_CH/2]);
	str_r |= 1 << (CONFIG_RX_CMT_CH % 2);
	__raw_writew(str_r, (void __iomem *)str[CONFIG_RX_CMT_CH/2]);
}
