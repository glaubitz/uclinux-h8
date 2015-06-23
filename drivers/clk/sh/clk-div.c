/*
 * SH7619 divide clock driver
 *
 * Copyright 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>

static DEFINE_SPINLOCK(clklock);

static const struct clk_div_table sh7619_div_table[] = {
        { .val = 0, .div = 1, },
        { .val = 1, .div = 2, },
        { .val = 3, .div = 4, },
        { .val = 4, .div = 5, },
};

static void __init sh7619_div_clk_setup(struct device_node *node)
{
	unsigned int num_parents;
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parent_name;
	void __iomem *freqcr = NULL;
	int offset;

	num_parents = of_clk_get_parent_count(node);
	if (num_parents < 1) {
		pr_err("%s: no parent found", clk_name);
		return;
	}

	freqcr = of_iomap(node, 0);
	if (freqcr == NULL) {
		pr_err("%s: failed to map divide register", clk_name);
		goto error;
	}

	parent_name = of_clk_get_parent_name(node, 0);
	clk = clk_register_divider_table(NULL, clk_name, parent_name,
					 CLK_SET_RATE_GATE, freqcr, 16, 3,
					 0, sh7619_div_table, &clklock);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		return;
	}
	pr_err("%s: failed to register %s div clock (%ld)\n",
	       __func__, clk_name, PTR_ERR(clk));
error:
	if (freqcr)
		iounmap(freqcr);
}

CLK_OF_DECLARE(sh7619_div_clk, "renesas,sh7619-div-clock", sh7619_div_clk_setup);
