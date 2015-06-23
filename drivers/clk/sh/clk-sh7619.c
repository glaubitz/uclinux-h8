/*
 * SH7619 clock driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <asm/io.h>

static DEFINE_SPINLOCK(clklock);

#define MAX_FREQ 125000000
#define MIN_FREQ  20000000

struct pll_clock {
	struct clk_hw hw;
	int pll2_mult;
	void __iomem *freqcr;
	void __iomem *wdt;
};

#define to_pll_clock(_hw) container_of(_hw, struct pll_clock, hw)

static unsigned long pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct pll_clock *pll_clock = to_pll_clock(hw);
	int mul = 1 << ((ioread16(pll_clock->freqcr) >> 8) & 7);

	return parent_rate * mul * pll_clock->pll2_mult;
}

static long pll_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	int i, m = -1;
	long offset[2];

	if (rate > MAX_FREQ)
		rate = MAX_FREQ;
	if (rate < MIN_FREQ)
		rate = MIN_FREQ;

	for (i = 0; i < 2; i++)
		offset[i] = abs(rate - (*prate * (1 << i)));
	for (i = 0; i < 2; i++)
		if (m < 0)
			m = i;
		else
			m = (offset[i] < offset[m])?i:m;

	return *prate * (1 << m);
}

static int pll_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	int pll;
	unsigned char val;
	unsigned long flags;
	struct pll_clock *pll_clock = to_pll_clock(hw);

	pll = (((rate / parent_rate) / 2) & 0x01) << 8;
	spin_lock_irqsave(&clklock, flags);
	iowrite16(0x5a00, pll_clock->wdt);
	iowrite16(0xa502, pll_clock->wdt + 2);
	val = ioread16(pll_clock->freqcr);
	val &= ~0x70;
	val |= pll;
	iowrite16(val, pll_clock->freqcr);
	spin_unlock_irqrestore(&clklock, flags);
	return 0;
}

static const struct clk_ops pll_ops = {
	.recalc_rate = pll_recalc_rate,
	.round_rate = pll_round_rate,
	.set_rate = pll_set_rate,
};

static void __init sh7619_pll_clk_setup(struct device_node *node)
{
	unsigned int num_parents;
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parent_name;
	struct pll_clock *pll_clock;
	struct clk_init_data init;

	num_parents = of_clk_get_parent_count(node);
	if (num_parents < 1) {
		pr_err("%s: no parent found", clk_name);
		return;
	}


	pll_clock = kzalloc(sizeof(struct pll_clock), GFP_KERNEL);
	if (!pll_clock) {
		pr_err("%s: failed to alloc memory", clk_name);
		return;
	}

	pll_clock->freqcr = of_iomap(node, 0);
	if (pll_clock->freqcr == NULL) {
		pr_err("%s: failed to map divide register", clk_name);
		goto free_clock;
	}

	pll_clock->wdt = of_iomap(node, 1);
	if (pll_clock->wdt == NULL) {
		pr_err("%s: failed to map divide register", clk_name);
		goto unmap_freqcr;
	}

	if (test_mode_pin(MODE_PIN2 | MODE_PIN0) ||
	    test_mode_pin(MODE_PIN2 | MODE_PIN1))
		pll_clock->pll2_mult = 2;
	else if (test_mode_pin(MODE_PIN0) || test_mode_pin(MODE_PIN1))
		pll_clock->pll2_mult = 4;

	parent_name = of_clk_get_parent_name(node, 0);
	init.name = clk_name;
	init.ops = &pll_ops;
	init.flags = CLK_IS_BASIC;
	init.parent_names = &parent_name;
	init.num_parents = 1;
	pll_clock->hw.init = &init;

	clk = clk_register(NULL, &pll_clock->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register %s div clock (%ld)\n",
		       __func__, clk_name, PTR_ERR(clk));
		goto unmap_wdt;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);
	return;

unmap_wdt:
	iounmap(pll_clock->pllcr);
unmap_freqcr:
	iounmap(pll_clock->sckcr);
free_clock:
	kfree(pll_clock);
}

CLK_OF_DECLARE(sh7619_pll_clk, "renesas,sh7619-pll-clock",
	       sh7619_pll_clk_setup);
