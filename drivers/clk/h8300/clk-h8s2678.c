/*
 * H8S2678 clock driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>

static DEFINE_SPINLOCK(clklock);

#define SCKCR 0xffff3b
#define PLLCR 0xffff45
#define DEVNAME "h8s2679-cpg"
#define MAX_FREQ 33333333
#define MIN_FREQ  8000000

static unsigned long pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	int mul = 1 << (ctrl_inb(PLLCR) & 3);

	return parent_rate * mul;
}

static long pll_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	int i, m = -1;
	long offset[3];

	if (rate > MAX_FREQ)
		rate = MAX_FREQ;
	if (rate < MIN_FREQ)
		rate = MIN_FREQ;

	for (i = 0; i < 3; i++)
		offset[i] = abs(rate - (*prate * (1 << i)));
	for (i = 0; i < 3; i++)
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

	pll = ((rate / parent_rate) / 2) & 0x03;
	spin_lock_irqsave(&clklock, flags);
	val = ctrl_inb(SCKCR);
	val |= 0x08;
	ctrl_outb(val, SCKCR);
	val = ctrl_inb(PLLCR);
	val &= ~0x03;
	val |= pll;
	ctrl_outb(val, PLLCR);
	spin_unlock_irqrestore(&clklock, flags);
	return 0;
}

static const struct clk_ops pll_ops = {
	.recalc_rate = pll_recalc_rate,
	.round_rate = pll_round_rate,
	.set_rate = pll_set_rate,
};

static struct clk *pll_clk_register(struct device *dev, const char *name,
				const char *parent)
{
	struct clk_hw *hw;
	struct clk *clk;
	struct clk_init_data init;

	hw = kzalloc(sizeof(struct clk_hw), GFP_KERNEL);
	if (!hw)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &pll_ops;
	init.flags = CLK_IS_BASIC;
	init.parent_names = &parent;
	init.num_parents = 1;
	hw->init = &init;

	clk = clk_register(dev, hw);
	if (IS_ERR(clk))
		kfree(hw);

	return clk;
}

static int clk_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int *hz = dev_get_platdata(&pdev->dev);

	clk = clk_register_fixed_rate(&pdev->dev, "master_clk", NULL,
				      CLK_IS_ROOT, *hz);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to register clock");
		return PTR_ERR(clk);
	}
	clk_register_clkdev(clk, "master_clk", DEVNAME ".%d", 0);

	clk = pll_clk_register(&pdev->dev, "pll_clk", "master_clk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to register clock");
		return PTR_ERR(clk);
	}
	clk_register_clkdev(clk, "pll_clk", DEVNAME ".%d", 0);

	clk = clk_register_divider(&pdev->dev, "core_clk", "pll_clk",
				   CLK_SET_RATE_GATE, (unsigned char *)SCKCR,
				   0, 3, CLK_DIVIDER_POWER_OF_TWO, &clklock);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to register clock");
		return PTR_ERR(clk);
	}
	clk_register_clkdev(clk, "core_clk", DEVNAME ".%d", 0);

	clk_add_alias("peripheral_clk", NULL, "core_clk", &pdev->dev);
	return 0;
}

static struct platform_driver cpg_driver = {
	.driver = {
		.name = DEVNAME,
	},
	.probe = clk_probe,
};

early_platform_init(DEVNAME, &cpg_driver);

static struct platform_device clk_device = {
	.name		= DEVNAME,
	.id		= 0,
};

static struct platform_device *devices[] __initdata = {
	&clk_device,
};

int __init h8300_clk_init(int hz)
{
	static int master_hz;

	master_hz = hz;
	clk_device.dev.platform_data = &master_hz;
	early_platform_add_devices(devices,
				   ARRAY_SIZE(devices));
	early_platform_driver_register_all(DEVNAME);
	early_platform_driver_probe(DEVNAME, 1, 0);
	return 0;
}
