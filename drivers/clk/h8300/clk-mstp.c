/*
 * H8/300 gated clock driver
 *
 * Copyright (C) 2016 Yoshinoi Sato <ysato@users.sourceforge.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#define MAX_CLOCK 16

struct mstp_data {
	struct clk_onecell_data data;
	void __iomem *mstcr;
};

struct mstp_clock {
	struct clk_hw hw;
	u32 bit_index;
	struct mstp_data *data;
};

static inline struct mstp_clock *to_mstp_clock(struct clk_hw *clk_hw)
{
	return container_of(clk_hw, struct mstp_clock, hw);
}

static int cpg_mstp_clock_enable(struct clk_hw *hw)
{
	struct mstp_clock *clock = to_mstp_clock(hw);
	u16 value;
	
	value = ioread16be(clock->data->mstcr);
	value &= ~BIT(clock->bit_index);
	iowrite16be(value, clock->data->mstcr);
	return 0;
}

static void cpg_mstp_clock_disable(struct clk_hw *hw)
{
	struct mstp_clock *clock = to_mstp_clock(hw);
	u16 value;

	value = ioread16be(clock->data->mstcr);
	value |= BIT(clock->bit_index);
	iowrite16be(value, clock->data->mstcr);
}

static int cpg_mstp_clock_is_enabled(struct clk_hw *hw)
{
	struct mstp_clock *clock = to_mstp_clock(hw);
	u16 value;

	value = ioread16be(clock->data->mstcr);
	return !(value & BIT(clock->bit_index));
}

static const struct clk_ops cpg_mstp_clock_ops = {
	.enable = cpg_mstp_clock_enable,
	.disable = cpg_mstp_clock_disable,
	.is_enabled = cpg_mstp_clock_is_enabled,
};

static struct clk * __init
cpg_mstp_clock_register(const char *name, const char *parent_name,
			unsigned int index, struct mstp_data *data)
{
	struct clk_init_data init;
	struct mstp_clock *clock;
	struct clk *clk;

	clock = kzalloc(sizeof(*clock), GFP_KERNEL);
	if (!clock) {
		pr_err("%s: failed to allocate MSTP clock.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &cpg_mstp_clock_ops;
	init.flags = CLK_IS_BASIC | CLK_SET_RATE_PARENT;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clock->bit_index = index;
	clock->data = data;
	clock->hw.init = &init;

	clk = clk_register(NULL, &clock->hw);

	if (IS_ERR(clk))
		kfree(clock);

	return clk;
}

static void __init mstp_clk_setup(struct device_node *node)
{
	const char *pclk;
	int i;
	void __iomem *mstcr;
	struct clk **clks;
	struct mstp_data *data;

	if (!(pclk = of_clk_get_parent_name(node, 0))) {
		pr_err("%s: parent clock not found\n", __func__);
		return;
	}
	mstcr = of_iomap(node, 0);
	if (!mstcr) {
		pr_err("%s: MSTP regieter map failed\n", __func__);
		return;
	}
		
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s: malloc failed\n", __func__);
		return;
	}
	clks = kmalloc(MAX_CLOCKS * sizeof(*clks), GFP_KERNEL);
	if (!clks) {
		pr_err("%s: malloc failed\n", __func__);
		goto free_data;
	}

	for (i = 0; i < MAX_CLOCKS; i++) {
		const char *name;
		int ret;

		clks[i] = ERR_PTR(-ENOENT);
		ret = of_property_read_string_index(node, "clock-output-names",
						    i, &name);
		if (ret < 0 || strlen(name) == 0)
			continue;
		clks[i] = cpg_mstp_clock_register(name, pclk, i, data);
		if (IS_ERR(clks[i]))
			pr_err("%s: failed to register %s %s clock (%ld)\n",
			       __func__, node->name, name, PTR_ERR(clks[i]));
	}
	data->data.clk_num = MAX_CLOCKS;
	data->data.clks = clks;
	data->mstcr = mstcr;
	of_clk_add_provider(node, of_clk_src_onecell_get, data);
	return;
free_data:
	kfree(data);
}

CLK_OF_DECLARE(renesas_h8_mstp_clk, "renesas,h8300-mstp-clock", mstp_clk_setup);
