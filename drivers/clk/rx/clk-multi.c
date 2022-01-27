/*
 * RX610/RX62N clock multiply driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#define MIN_FREQ 8000000

static DEFINE_SPINLOCK(clklock);

struct pll_clock {
	struct clk_hw hw;
	void __iomem *sckcr;
	int offset;
	int maxfreq;
};

#define to_pll_clock(_hw) container_of(_hw, struct pll_clock, hw)

static unsigned long pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct pll_clock *pll_clock = to_pll_clock(hw);
	unsigned long sckcr;
	int mul;

	sckcr = ioread32(pll_clock->sckcr);
	sckcr >>= pll_clock->offset;
	mul = 1 << (3 - sckcr);

	return parent_rate * mul;
}

static long pll_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	struct pll_clock *pll_clock = to_pll_clock(hw);
	int i, m = -1;
	long offset[4];

	if (rate > pll_clock->maxfreq)
		rate = pll_clock->maxfreq;
	if (rate < MIN_FREQ)
		rate = MIN_FREQ;

	for (i = 0; i < 4; i++)
		offset[i] = abs(rate - (*prate * (1 << i)));
	for (i = 0; i < 4; i++)
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
	unsigned long val;
	unsigned long flags;
	struct pll_clock *pll_clock = to_pll_clock(hw);

	pll = 3 - ffs(rate / parent_rate);
	spin_lock_irqsave(&clklock, flags);
	val = ioread32(pll_clock->sckcr);
	val &= ~(0x0f << pll_clock->offset);
	val |= pll << pll_clock->offset;
	iowrite32(val, pll_clock->sckcr);
	while (ioread32(pll_clock->sckcr) != val);
	spin_unlock_irqrestore(&clklock, flags);
	return 0;
}

static const struct clk_ops pll_ops = {
	.recalc_rate = pll_recalc_rate,
	.round_rate = pll_round_rate,
	.set_rate = pll_set_rate,
};

static void __init rx_mul_clk_setup(struct device_node *node)
{
	int num_parents;
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


	pll_clock = kzalloc(sizeof(*pll_clock), GFP_KERNEL);
	if (!pll_clock)
		return;

	pll_clock->sckcr = of_iomap(node, 0);
	if (pll_clock->sckcr == NULL) {
		pr_err("%s: failed to map sckcr register", clk_name);
		goto free_clock;
	}
	of_property_read_u32(node,"renesas,offset", &pll_clock->offset);
	of_property_read_u32(node,"renesas,maxfreq", &pll_clock->maxfreq);

	parent_name = of_clk_get_parent_name(node, 0);
	init.name = clk_name;
	init.ops = &pll_ops;
	init.flags = 0;
	init.parent_names = &parent_name;
	init.num_parents = 1;
	pll_clock->hw.init = &init;

	clk = clk_register(NULL, &pll_clock->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register %s div clock (%ld)\n",
		       __func__, clk_name, PTR_ERR(clk));
		goto unmap_sckcr;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);
	return;

unmap_sckcr:
	iounmap(pll_clock->sckcr);
free_clock:
	kfree(pll_clock);
}

CLK_OF_DECLARE(rx_mul_clk, "renesas,rx-mul-clock",
	       rx_mul_clk_setup);
