/*
 * RX610 CPG setup
 *
 *  Copyright (C) 2009  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

/*
 * clock diagram
 * xtal/ext clock -- *8 pll -+- div1 -- cpu core
 *                           |
 *                           +- div2 -- internal peripheral
 *                           |
 *                           +- div3 -- external bus
 */

#include <linux/linkage.h>
#include <linux/clk.h>
#include <asm/clock.h>
#include <asm/io.h>

#define MHz 1000000

/* pll output clock */
static struct clk master = {
	.name  = "master_clk",
        .flags = CLK_ENABLE_ON_INIT,
        .rate  = CONFIG_INPUT_CLOCK_FREQ * 8,
	.set_rate = NULL,
	.update = NULL,
};

const int __initdata div_rate[] = {1, 2, 4, 8};

static int update_clock(struct clk *clk, unsigned long rate, int bits)
{
	unsigned long master_freq = clk_get_rate(clk->parent);
	int i;
	rate = clk_round_rate(clk, rate);
	for (i = 0; i < ARRAY_SIZE(div_rate); i++)
		if (rate == master_freq / div_rate[i]) {
			unsigned long flags;
			unsigned long sckcr;
			local_irq_save(flags);
			sckcr = __raw_readl((void *)0x00080020);
			sckcr &= ~(0x0f << bits);
			sckcr |= i << bits;
			__raw_writel(sckcr, (void *)0x00080020);
			local_irq_restore(flags);
			clk->rate = rate;
			return 0;
		}
	return 1;
}


static int cpu_set_rate(struct clk *clk, unsigned long rate)
{
	if (rate > 100 * MHz)
		return 1;
	return update_clock(clk, rate, 24);
}

static int cpu_update(struct clk *clk)
{
	return update_clock(clk, clk->rate, 24);
}

static struct clk cpu = {
	.name   = "cpu_clk",
	.parent = &master,
	.flags	= CLK_ENABLE_ON_INIT,
	.set_rate = cpu_set_rate,
	.update = cpu_update,
};

static int peripheral_set_rate(struct clk *clk, unsigned long rate)
{
	if (rate > 50 * MHz)
		return 1;
	return update_clock(clk, rate, 8);
}

static int peripheral_update(struct clk *clk)
{
	return update_clock(clk, clk->rate, 8);
}

static struct clk peripheral = {
	.name   = "peripheral_clk",
	.parent = &master,
	.flags	= CLK_ENABLE_ON_INIT,
	.set_rate = peripheral_set_rate,
	.update = peripheral_update,
};

static int bus_set_rate(struct clk *clk, unsigned long rate)
{
	if (rate > 25 * MHz)
		return 1;
	return update_clock(clk, rate, 16);
}

static int bus_update(struct clk *clk)
{
	return update_clock(clk, clk->rate, 16);
}

static struct clk bus = {
	.name   = "bus_clk",
	.parent = &master,
	.flags	= CLK_ENABLE_ON_INIT,
	.set_rate = bus_set_rate,
	.update = bus_update,
};

static void __init set_selectable(struct clk *clk, int limit)
{
	int i, nr;
	for(i = ARRAY_SIZE(div_rate) - 1, nr = 0; i >= 0; i--) {
		int f = (CONFIG_INPUT_CLOCK_FREQ * 8) / div_rate[i];
		if (f > limit)
			break;
		clk->selectable[nr++] = f;
	}
	clk->nr_selectable = nr;
}

int __init arch_clk_init(void)
{
	int ret;
	if ((ret = clk_register(&master)))
		pr_err("%s: CPU clock registration failed.\n", __func__);

	set_selectable(&cpu, 100 * MHz);
	if ((ret = clk_register(&cpu))) {
		pr_err("%s: CPU clock registration failed.\n", __func__);
		goto end;
	}
	set_selectable(&peripheral, 50 * MHz);
	if ((ret = clk_register(&peripheral))) {
		pr_err("%s: CPU clock registration failed.\n", __func__);
		goto end;
	}
	set_selectable(&bus, 25 * MHz);
	if ((ret = clk_register(&bus))) {
		pr_err("%s: CPU clock registration failed.\n", __func__);
		goto end;
	}
end:
	return ret;
}
