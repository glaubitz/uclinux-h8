/*
 * H8/300 pinctrl and driver
 *
 * Copyright (C) 2015 Yoshinori Sato
 *
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>

#include "core.h"

#define MAX_NB_GPIO_PER_BANK 8

struct h8300_pmx_func {
	const char	*name;
	const char	**groups;
	unsigned	ngroups;
};

struct h8300_pmx_pin {
	uint32_t	bank;
	uint32_t	pin;
	unsigned long	conf;
};

struct h8300_pin_group {
	const char		*name;
	struct h8300_pmx_pin	*pins_conf;
	unsigned int		*pins;
	unsigned		npins;
};

struct h8300_pinctrl {
	struct device		*dev;
	struct pinctrl_dev	*pctl;
	struct h8300_pmx_func	*functions;
	int			nfunctions;
	struct h8300_pin_group	*groups;
	int			ngroups;
};

static const struct h8300_pin_group *pinctrl_find_group_by_name(
				const struct h8300_pinctrl *info,
				const char *name)
{
	const struct h8300_pin_group *grp = NULL;
	int i;

	for (i = 0; i < info->ngroups; i++) {
		if (strcmp(info->groups[i].name, name))
			continue;

		grp = &info->groups[i];
		break;
	}

	return grp;
}

static int get_groups_count(struct pinctrl_dev *pctldev)
{
	struct h8300_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->ngroups;
}

static const char *get_group_name(struct pinctrl_dev *pctldev,
				       unsigned selector)
{
	struct h8300_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->groups[selector].name;
}

static int dt_node_to_map(struct pinctrl_dev *pctldev,
			struct device_node *np,
			struct pinctrl_map **map, unsigned *num_maps)
{
	struct h8300_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	const struct h8300_pin_group *grp;
	struct pinctrl_map *new_map;
	struct device_node *parent;
	int map_num = 1;
	int i;

	/*
	 * first find the group of this node and check if we need to create
	 * config maps for pins
	 */
	grp = pinctrl_find_group_by_name(info, np->name);
	if (!grp) {
		dev_err(info->dev, "unable to find group for node %s\n",
			np->name);
		return -EINVAL;
	}

	map_num += grp->npins;
	new_map = devm_kzalloc(pctldev->dev, sizeof(*new_map) * map_num, GFP_KERNEL);
	if (!new_map)
		return -ENOMEM;

	*map = new_map;
	*num_maps = map_num;

	/* create mux map */
	parent = of_get_parent(np);
	if (!parent) {
		devm_kfree(pctldev->dev, new_map);
		return -EINVAL;
	}
	new_map[0].type = PIN_MAP_TYPE_MUX_GROUP;
	new_map[0].data.mux.function = parent->name;
	new_map[0].data.mux.group = np->name;
	of_node_put(parent);

	/* create config map */
	new_map++;
	for (i = 0; i < grp->npins; i++) {
		new_map[i].type = PIN_MAP_TYPE_CONFIGS_PIN;
		new_map[i].data.configs.group_or_pin =
				pin_get_name(pctldev, grp->pins[i]);
		new_map[i].data.configs.configs = &grp->pins_conf[i].conf;
		new_map[i].data.configs.num_configs = 1;
	}

	dev_dbg(pctldev->dev, "maps: function %s group %s num %d\n",
		(*map)->data.mux.function, (*map)->data.mux.group, map_num);

	return 0;
}

static void dt_free_map(struct pinctrl_dev *pctldev,
				struct pinctrl_map *map, unsigned num_maps)
{
}

static const struct pinctrl_ops h8300_pctrl_ops = {
	.get_groups_count	= get_groups_count,
	.get_group_name		= get_group_name,
	.dt_node_to_map		= dt_node_to_map,
	.dt_free_map		= dt_free_map,
};

static inline int pin_to_bank(unsigned pin)
{
	return pin /= MAX_NB_GPIO_PER_BANK;
}

static int h8300_pmx_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct h8300_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->nfunctions;
}

static const char *h8300_pmx_get_func_name(struct pinctrl_dev *pctldev,
					  unsigned selector)
{
	struct h8300_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->functions[selector].name;
}

static int h8300_pmx_get_groups(struct pinctrl_dev *pctldev, unsigned selector,
			       const char * const **groups,
			       unsigned * const num_groups)
{
	struct h8300_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	*groups = info->functions[selector].groups;
	*num_groups = info->functions[selector].ngroups;
	return 0;
}

static int h8300_pmx_set(struct pinctrl_dev *pctldev, unsigned selector,
			unsigned group)
{
	return 0;
}

static const struct pinmux_ops h8300_pmx_ops = {
	.get_functions_count	= h8300_pmx_get_funcs_count,
	.get_function_name	= h8300_pmx_get_func_name,
	.get_function_groups	= h8300_pmx_get_groups,
	.set_mux		= h8300_pmx_set,
};

static struct pinctrl_desc h8300_pinctrl_desc = {
	.pctlops	= &h8300_pctrl_ops,
	.pmxops		= &h8300_pmx_ops,
	.owner		= THIS_MODULE,
};

static const char *gpio_compat = "renesas,h8300-gpio";

static int h8300_pinctrl_child_count(struct h8300_pinctrl *info,
				     struct device_node *np)
{
	struct device_node *child;
	int gpio_banks = 0;

	for_each_child_of_node(np, child) {
		if (of_device_is_compatible(child, gpio_compat)) {
			gpio_banks++;
		} else {
			info->nfunctions++;
			info->ngroups += of_get_child_count(child);
		}
	}
	return gpio_banks;
}

static int h8300_pinctrl_parse_groups(struct device_node *np,
				       struct h8300_pin_group *grp,
				       struct h8300_pinctrl *info, u32 index)
{
	struct h8300_pmx_pin *pin;
	int size;
	const __be32 *list;
	int i, j;

	dev_dbg(info->dev, "group(%d): %s\n", index, np->name);

	/* Initialise group */
	grp->name = np->name;

	list = of_get_property(np, "renesas,pins", &size);
	/* we do not check return since it's safe node passed down */
	size /= sizeof(*list);
	if (!size || size % 2) {
		dev_err(info->dev, "wrong pins number or pins and configs should be by 2\n");
		return -EINVAL;
	}

	grp->npins = size / 2;
	pin = grp->pins_conf = devm_kzalloc(info->dev, grp->npins * sizeof(struct h8300_pmx_pin),
				GFP_KERNEL);
	grp->pins = devm_kzalloc(info->dev, grp->npins * sizeof(unsigned int),
				GFP_KERNEL);
	if (!grp->pins_conf || !grp->pins)
		return -ENOMEM;

	for (i = 0, j = 0; i < size; i += 2, j++) {
		pin->bank = be32_to_cpu(*list++);
		pin->pin = be32_to_cpu(*list++);
		pin++;
	}

	return 0;
}

static int h8300_pinctrl_parse_functions(struct device_node *np,
					struct h8300_pinctrl *info, u32 index)
{
	struct device_node *child;
	struct h8300_pmx_func *func;
	struct h8300_pin_group *grp;
	int ret;
	static u32 grp_index;
	u32 i = 0;

	dev_dbg(info->dev, "parse function(%d): %s\n", index, np->name);

	func = &info->functions[index];

	/* Initialise function */
	func->name = np->name;
	func->ngroups = of_get_child_count(np);
	if (func->ngroups == 0) {
		dev_err(info->dev, "no groups defined\n");
		return -EINVAL;
	}
	func->groups = devm_kzalloc(info->dev,
			func->ngroups * sizeof(char *), GFP_KERNEL);
	if (!func->groups)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		func->groups[i] = child->name;
		grp = &info->groups[grp_index++];
		ret = h8300_pinctrl_parse_groups(child, grp, info, i++);
		if (ret)
			return ret;
	}


	return 0;
}

static const struct of_device_id h8300_pinctrl_of_match[] = {
	{ .compatible = "renesas,h8300-pinctrl", },
	{ }
};

static int h8300_pinctrl_probe_dt(struct platform_device *pdev,
				 struct h8300_pinctrl *info)
{
	int ret = 0;
	int i;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	int gpio_banks;


	if (!np)
		return -ENODEV;

	info->dev = &pdev->dev;
	gpio_banks = h8300_pinctrl_child_count(info, np);
	h8300_pinctrl_desc.npins = gpio_banks * MAX_NB_GPIO_PER_BANK;

	if (unlikely(gpio_banks < 1)) {
		dev_err(&pdev->dev, "you need to specify at least one gpio-controller\n");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "nfunctions = %d\n", info->nfunctions);
	info->functions = devm_kzalloc(&pdev->dev, info->nfunctions * sizeof(struct h8300_pmx_func),
					GFP_KERNEL);
	if (unlikely(!info->functions))
		return -ENOMEM;

	info->groups = devm_kzalloc(&pdev->dev, info->ngroups * sizeof(struct h8300_pin_group),
					GFP_KERNEL);
	if (unlikely(!info->groups)) {
		ret = -ENOMEM;
		goto group_fail;
	}

	dev_dbg(&pdev->dev, "nbanks = %d\n", gpio_banks);
	dev_dbg(&pdev->dev, "ngroups = %d\n", info->ngroups);

	i = 0;

	for_each_child_of_node(np, child) {
		if (of_device_is_compatible(child, gpio_compat))
			continue;
		ret = h8300_pinctrl_parse_functions(child, info, i++);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse function\n");
			goto parse_fail;
		}
	}
	return 0;

parse_fail:
	kfree(info->groups);
group_fail:
	kfree(info->functions);

	return ret;
}

static int h8300_pinctrl_probe(struct platform_device *pdev)
{
	struct h8300_pinctrl *info;
	struct pinctrl_pin_desc *pdesc;
	int ret = 0;
	int i;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (unlikely(!info))
		return -ENOMEM;

	ret = h8300_pinctrl_probe_dt(pdev, info);
	if (ret) 
		goto probe_fail;

	h8300_pinctrl_desc.name = dev_name(&pdev->dev);
	pdesc = devm_kzalloc(&pdev->dev,
			     sizeof(*pdesc) * h8300_pinctrl_desc.npins,
			     GFP_KERNEL);
	if (unlikely(!pdesc)) {
		ret = -ENOMEM;
		goto desc_fail;
	}

	h8300_pinctrl_desc.pins = pdesc;
	for (i = 0; i < h8300_pinctrl_desc.npins; i++) {
		pdesc->number = i;
		pdesc++;
	}

	platform_set_drvdata(pdev, info);
	info->pctl = pinctrl_register(&h8300_pinctrl_desc, &pdev->dev, info);
	if (unlikely(!info->pctl)) {
		dev_err(&pdev->dev, "could not register H8/3000 pinctrl driver\n");
		ret = -EINVAL;
		goto register_fail;
	}

	dev_info(&pdev->dev, "initialized H8/300 pinctrl driver\n");
	return 0;
register_fail:
	kfree(h8300_pinctrl_desc.pins);
desc_fail:
probe_fail:
	kfree(info);
	return ret;
}

static int h8300_pinctrl_remove(struct platform_device *pdev)
{
	struct h8300_pinctrl *info = platform_get_drvdata(pdev);

	pinctrl_unregister(info->pctl);
	kfree(h8300_pinctrl_desc.pins);
	kfree(info);

	return 0;
}

static struct platform_driver h8300_pinctrl_driver = {
	.driver = {
		.name = "pinctrl-h8300",
		.of_match_table = h8300_pinctrl_of_match,
	},
	.probe = h8300_pinctrl_probe,
	.remove = h8300_pinctrl_remove,
};

static int __init h8300_pinctrl_init(void)
{
	return platform_driver_register(&h8300_pinctrl_driver);
}
module_init(h8300_pinctrl_init);

static void __exit h8300_pinctrl_exit(void)
{
	platform_driver_unregister(&h8300_pinctrl_driver);
}

module_exit(h8300_pinctrl_exit);
MODULE_AUTHOR("Yoshinori Sato <ysato@users.sourceforge.jp>");
MODULE_DESCRIPTION("Renesas H8/300 pinctrl driver");
MODULE_LICENSE("GPL v2");
