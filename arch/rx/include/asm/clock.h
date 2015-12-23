#ifndef __ASM_RX_CLOCK_H__
#define __ASM_RX_CLOCK_H__

#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/err.h>

#define DIV_NR 4

struct clk {
	struct list_head	node;
	const char		*name;
	int			id;
	struct module		*owner;

	struct clk		*parent;
	int (*set_rate)(struct clk *clk, unsigned long rate);
	int (*update)(struct clk *clk);

	struct list_head	children;
	struct list_head	sibling;	/* node for children */

	int			usecount;

	unsigned long		rate;
	unsigned long		flags;

	void __iomem		*mstpcr;
	unsigned int		mstpbit;

	unsigned long		arch_flags;
	void			*priv;
	struct dentry		*dentry;
	int			nr_selectable;
	int			selectable[DIV_NR];
};

struct clk_lookup {
	struct list_head	node;
	const char		*dev_id;
	const char		*con_id;
	struct clk		*clk;
};

#define CLK_ENABLE_ON_INIT	(1 << 0)

int __init arch_clk_init(void);

int clk_init(void);
unsigned long followparent_recalc(struct clk *);
void recalculate_root_clocks(void);
void propagate_rate(struct clk *);
int clk_reparent(struct clk *child, struct clk *parent);
int clk_register(struct clk *);
void clk_unregister(struct clk *);

#endif /* __ASM_RX_CLOCK_H__ */
