/*
 * arch/rx/kernel/cpu/clock.c - RX clock framework
 *
 * Based on SH version by:
 * 
 *  Copyright (C) 2005 - 2009  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cpufreq.h>
#include <asm/clock.h>
#include <asm/io.h>

static LIST_HEAD(clock_list);
static DEFINE_SPINLOCK(clock_lock);
static DEFINE_MUTEX(clock_list_sem);

static void __clk_disable(struct clk *clk)
{
	if (clk->usecount == 0) {
		printk(KERN_ERR "Trying disable clock %s with 0 usecount\n",
		       clk->name);
		WARN_ON(1);
		return;
	}

	if (!(--clk->usecount)) {
		if (likely(clk->mstpcr)) {
			unsigned char mstpcr;
			mstpcr = __raw_readb(clk->mstpcr);
			mstpcr |= clk->mstpbit;
			__raw_writeb(mstpcr,clk->mstpcr);
		}
		if (likely(clk->parent))
			__clk_disable(clk->parent);
	}
}

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if (!clk)
		return;

	spin_lock_irqsave(&clock_lock, flags);
	__clk_disable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);
}
EXPORT_SYMBOL_GPL(clk_disable);

static int __clk_enable(struct clk *clk)
{
	int ret = 0;

	if (clk->usecount++ == 0) {
		if (clk->parent) {
			ret = __clk_enable(clk->parent);
			if (unlikely(ret))
				goto err;
		}
		if (likely(clk->mstpcr)) {
			unsigned char mstpcr;
			mstpcr = __raw_readb(clk->mstpcr);
			mstpcr &= ~clk->mstpbit;
			__raw_writeb(mstpcr,clk->mstpcr);
		}
	}

	return ret;
err:
	clk->usecount--;
	return ret;
}

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret;

	if (!clk)
		return -EINVAL;

	spin_lock_irqsave(&clock_lock, flags);
	ret = __clk_enable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_enable);

static LIST_HEAD(root_clks);

int clk_register(struct clk *clk)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;

	/*
	 * trap out already registered clocks
	 */
	if (clk->node.next || clk->node.prev)
		return 0;

	mutex_lock(&clock_list_sem);

	INIT_LIST_HEAD(&clk->children);
	clk->usecount = 0;

	if (clk->parent)
		list_add(&clk->sibling, &clk->parent->children);
	else
		list_add(&clk->sibling, &root_clks);

	list_add(&clk->node, &clock_list);
	mutex_unlock(&clock_list_sem);

	return 0;
}
EXPORT_SYMBOL_GPL(clk_register);

void clk_unregister(struct clk *clk)
{
	mutex_lock(&clock_list_sem);
	list_del(&clk->sibling);
	list_del(&clk->node);
	mutex_unlock(&clock_list_sem);
}
EXPORT_SYMBOL_GPL(clk_unregister);

static void clk_enable_init_clocks(void)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &clock_list, node)
		if (clkp->flags & CLK_ENABLE_ON_INIT)
			clk_enable(clkp);
}

int clk_reparent(struct clk *child, struct clk *parent)
{
	list_del_init(&child->sibling);
	if (parent)
		list_add(&child->sibling, &parent->children);
	child->parent = parent;

	/* now do the debugfs renaming to reattach the child
	   to the proper parent */

	return 0;
}

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate?clk->rate:clk->selectable[clk->nr_selectable];
}
EXPORT_SYMBOL_GPL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EOPNOTSUPP;
	unsigned long flags;
	struct clk *clkp;

	spin_lock_irqsave(&clock_lock, flags);

	if (likely(clk->set_rate)) {
		ret = clk->set_rate(clk, rate);
		if (ret != 0)
			goto out_unlock;
	} else {
		clk->rate = rate;
		ret = 0;
	}
	list_for_each_entry(clkp, &clk->children, sibling)
		if (likely(clk->update))
			clk->update(clkp);
out_unlock:
	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;
	int ret = -EINVAL;

	if (!parent || !clk)
		return ret;
	if (clk->parent == parent)
		return 0;

	spin_lock_irqsave(&clock_lock, flags);
	if (clk->usecount == 0) {
		ret = clk_reparent(clk, parent);

		if (ret == 0) {
			pr_debug("clock: set parent of %s to %s (new rate %ld)\n",
				 clk->name, clk->parent->name, clk->rate);
		}
	} else
		ret = -EBUSY;
	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}
EXPORT_SYMBOL_GPL(clk_get_parent);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	if (clk->nr_selectable > 0) {
		int idx, roundclk;

		spin_lock_irqsave(&clock_lock, flags);
		roundclk = clk->selectable[clk->nr_selectable - 1];
		for(idx = 0; idx < clk->nr_selectable; idx++) {
			if (rate < clk->selectable[idx]) {
				if (idx > 0 && 
				    ((rate - clk->selectable[idx - 1]) < (clk->selectable[idx ])))
					roundclk =  clk->selectable[idx - 1];
				else
					roundclk = clk->selectable[idx];
				break;
			}
		}
		spin_unlock_irqrestore(&clock_lock, flags);

		return roundclk;
	}

	return clk_get_rate(clk);
}
EXPORT_SYMBOL_GPL(clk_round_rate);

/*
 * Find the correct struct clk for the device and connection ID.
 * We do slightly fuzzy matching here:
 *  An entry with a NULL ID is assumed to be a wildcard.
 *  If an entry has a device ID, it must match
 *  If an entry has a connection ID, it must match
 * Then we take the most specific entry - with the following
 * order of precidence: dev+con > dev only > con only.
 */
static struct clk *clk_find(const char *dev_id, const char *con_id)
{
	struct clk_lookup *p;
	struct clk *clk = NULL;
	int match, best = 0;

	list_for_each_entry(p, &clock_list, node) {
		match = 0;
		if (p->dev_id) {
			if (!dev_id || strcmp(p->dev_id, dev_id))
				continue;
			match += 2;
		}
		if (p->con_id) {
			if (!con_id || strcmp(p->con_id, con_id))
				continue;
			match += 1;
		}
		if (match == 0)
			continue;

		if (match > best) {
			clk = p->clk;
			best = match;
		}
	}
	return clk;
}

struct clk *clk_get_sys(const char *dev_id, const char *con_id)
{
	struct clk *clk;

	mutex_lock(&clock_list_sem);
	clk = clk_find(dev_id, con_id);
	mutex_unlock(&clock_list_sem);

	return clk ? clk : ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL_GPL(clk_get_sys);

/*
 * Returns a clock. Note that we first try to use device id on the bus
 * and clock name. If this fails, we try to use clock name only.
 */
struct clk *clk_get(struct device *dev, const char *id)
{
	const char *dev_id = dev ? dev_name(dev) : NULL;
	struct clk *p, *clk = ERR_PTR(-ENOENT);
	int idno;

	clk = clk_get_sys(dev_id, id);
	if (clk && !IS_ERR(clk))
		return clk;

	if (dev == NULL || dev->bus != &platform_bus_type)
		idno = -1;
	else
		idno = to_platform_device(dev)->id;

	mutex_lock(&clock_list_sem);
	list_for_each_entry(p, &clock_list, node) {
		if (p->id == idno &&
		    strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			goto found;
		}
	}

	list_for_each_entry(p, &clock_list, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}

found:
	mutex_unlock(&clock_list_sem);

	return clk;
}
EXPORT_SYMBOL_GPL(clk_get);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}
EXPORT_SYMBOL_GPL(clk_put);

int __init clk_init(void)
{
	int ret;
	struct clk *clkp;
		struct clk *child_clkp;

	ret = arch_clk_init();
	if (unlikely(ret)) {
		pr_err("%s: CPU clock registration failed.\n", __func__);
		return ret;
	}

	list_for_each_entry(clkp, &root_clks, sibling) {
		if (likely(clkp->update))
			clkp->update(clkp);
		list_for_each_entry(child_clkp, &clkp->children, sibling) {
			if (likely(child_clkp->update))
				child_clkp->update(child_clkp);
		}
	}

	/* Enable the necessary init clocks */
	clk_enable_init_clocks();

	return ret;
}

#ifdef CONFIG_DEBUG_FS
/*
 *	debugfs support to trace clock tree hierarchy and attributes
 */
static struct dentry *clk_debugfs_root;

static int clk_debugfs_register_one(struct clk *c)
{
	int err;
	struct dentry *d, *child;
	struct clk *pa = c->parent;
	char s[255];
	char *p = s;

	p += sprintf(p, "%s", c->name);
	if (c->id >= 0)
		sprintf(p, ":%d", c->id);
	d = debugfs_create_dir(s, pa ? pa->dentry : clk_debugfs_root);
	if (!d)
		return -ENOMEM;
	c->dentry = d;

	d = debugfs_create_u8("usecount", S_IRUGO, c->dentry, (u8 *)&c->usecount);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_u32("rate", S_IRUGO, c->dentry, (u32 *)&c->rate);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_x32("flags", S_IRUGO, c->dentry, (u32 *)&c->flags);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	return 0;

err_out:
	d = c->dentry;
	list_for_each_entry(child, &d->d_subdirs, d_u.d_child)
		debugfs_remove(child);
	debugfs_remove(c->dentry);
	return err;
}

static int clk_debugfs_register(struct clk *c)
{
	int err;
	struct clk *pa = c->parent;

	if (pa && !pa->dentry) {
		err = clk_debugfs_register(pa);
		if (err)
			return err;
	}

	if (!c->dentry) {
		err = clk_debugfs_register_one(c);
		if (err)
			return err;
	}
	return 0;
}

static int __init clk_debugfs_init(void)
{
	struct clk *c;
	struct dentry *d;
	int err;

	d = debugfs_create_dir("clock", NULL);
	if (!d)
		return -ENOMEM;
	clk_debugfs_root = d;

	list_for_each_entry(c, &clock_list, node) {
		err = clk_debugfs_register(c);
		if (err)
			goto err_out;
	}
	return 0;
err_out:
	debugfs_remove(clk_debugfs_root); /* REVISIT: Cleanup correctly */
	return err;
}
late_initcall(clk_debugfs_init);
#endif
