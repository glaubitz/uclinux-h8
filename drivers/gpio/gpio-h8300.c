/*
 * H8/3069 GPIO driver
 *
 * Copyright (C) 2015 Yoshinori Sato
 *
 */

#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/gpio.h>

#define MAX_GPIO_BANKS		16
#define MAX_NB_GPIO_PER_BANK	8

struct gpio_type {
	char *name;
	char *label;
	int num_io;
};

static const struct gpio_type h83069gpio = {
	.name	= "H8/3069",
	.label	= "123456789AB",
	.num_io	= 11,
};

static const struct gpio_type h8s2678gpio = {
	.name	= "H8S2678",
	.label	= "12345678ABCDEFGH",
	.num_io	= 16,
};

struct h8300_gpio_chip {
	struct gpio_chip	chip;
	struct pinctrl_gpio_range range;
	void __iomem *dr;
	void __iomem *ddr;
	unsigned char direction;
	unsigned char idx;
};

#define to_h8300_gpio_chip(c) container_of(c, struct h8300_gpio_chip, chip)

static struct h8300_gpio_chip *gpio_chips[MAX_GPIO_BANKS];

static int gpio_banks;

static int h8300_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	int gpio = chip->base + offset;

	return pinctrl_request_gpio(gpio);
}

static void h8300_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	int gpio = chip->base + offset;

	pinctrl_free_gpio(gpio);
}

static int h8300_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	struct h8300_gpio_chip *h8300_gpio = to_h8300_gpio_chip(chip);
	unsigned char mask = 1 << offset;

	return (h8300_gpio->direction & mask);
}

static int h8300_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct h8300_gpio_chip *h8300_gpio = to_h8300_gpio_chip(chip);
	void __iomem *ddr = h8300_gpio->ddr;
	unsigned char mask = 1 << offset;

	h8300_gpio->direction &= ~mask;
	iowrite8(h8300_gpio->direction, ddr);
	return 0;
}

static int h8300_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
				int val)
{
	struct h8300_gpio_chip *h8300_gpio = to_h8300_gpio_chip(chip);
	void __iomem *ddr = h8300_gpio->ddr;
	unsigned char mask = 1 << offset;

	h8300_gpio->direction |= mask;
	iowrite8(h8300_gpio->direction, ddr);
	return 0;
}

static int h8300_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct h8300_gpio_chip *h8300_gpio = to_h8300_gpio_chip(chip);
	void __iomem *dr = h8300_gpio->dr;
	unsigned char mask = 1 << offset;

	return (ioread8(dr) & mask) != 0;
}

static void h8300_gpio_set(struct gpio_chip *chip, unsigned offset,
				int val)
{
	struct h8300_gpio_chip *h8300_gpio = to_h8300_gpio_chip(chip);
	unsigned char *dr = h8300_gpio->dr;

	if (val)
		ctrl_bset(offset, dr);
	else
		ctrl_bclr(offset, dr);
}

static const struct of_device_id h8300_gpio_of_match[] = {
	{ .compatible = "renesas,h83069-gpio", .data = &h83069gpio, },
	{ .compatible = "renesas,h8s2678-gpio", .data = &h8s2678gpio, },
	{ },
};

static int h8300_gpio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	struct h8300_gpio_chip *h8300_chip = NULL;
	struct gpio_chip *chip;
	struct pinctrl_gpio_range *range;
	const struct of_device_id *match;
	const struct gpio_type *type;
	int ret = 0;
	int i;
	int alias_idx = of_alias_get_id(np, "gpio");
	uint32_t ngpio, direction;
	char **names;

	match = of_match_node(h8300_gpio_of_match, np);
	if (unlikely(!match)) {
		ret = -ENODEV;
		goto err;
	}
	type = match->data;
	
	BUG_ON(alias_idx >= type->num_io);
	if (gpio_chips[alias_idx]) {
		ret = -EBUSY;
		goto err;
	}

	h8300_chip = devm_kzalloc(&pdev->dev, sizeof(*h8300_chip), GFP_KERNEL);
	if (!h8300_chip) {
		ret = -ENOMEM;
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	h8300_chip->dr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(h8300_chip->dr)) {
		ret = PTR_ERR(h8300_chip->dr);
		goto free_for_chip;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	h8300_chip->ddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(h8300_chip->ddr)) {
		ret = PTR_ERR(h8300_chip->ddr);
		goto free_for_chip;
	}

	h8300_chip->idx = alias_idx;

	chip = &h8300_chip->chip;
	chip->of_node = np;
	chip->label = dev_name(&pdev->dev);
	chip->dev = &pdev->dev;
	chip->owner = THIS_MODULE;
	chip->base = alias_idx * MAX_NB_GPIO_PER_BANK;
	chip->request = h8300_gpio_request;
	chip->free = h8300_gpio_free;
	chip->get_direction = h8300_gpio_get_direction;
	chip->direction_input = h8300_gpio_direction_input;
	chip->direction_output = h8300_gpio_direction_output;
	chip->get = h8300_gpio_get;
	chip->set = h8300_gpio_set;
	if (of_property_read_u32(np, "renesas,direction", &direction)) {
		dev_err(&pdev->dev, "direction not defined\n");
		goto free_for_chip;
	}
	h8300_chip->direction = direction;
	
	if (!of_property_read_u32(np, "#gpio-lines", &ngpio)) {
		if (ngpio > MAX_NB_GPIO_PER_BANK) {
			pr_err("h8300_gpio.%d, gpio-nb > %d failback to %d\n",
			       alias_idx, MAX_NB_GPIO_PER_BANK,
			       MAX_NB_GPIO_PER_BANK);
			chip->ngpio = MAX_NB_GPIO_PER_BANK;
		} else
			chip->ngpio = ngpio;
	}

	names = devm_kzalloc(&pdev->dev, sizeof(char *) * chip->ngpio,
			     GFP_KERNEL);

	if (!names) {
		ret = -ENOMEM;
		goto free_for_chip;
	}

	for (i = 0; i < chip->ngpio; i++)
		names[i] = kasprintf(GFP_KERNEL, "gpio%c%d",
				     type->label[alias_idx], i);

	chip->names = (const char *const *)names;

	range = &h8300_chip->range;
	range->name = chip->label;
	range->id = alias_idx;
	range->pin_base = range->base = range->id * MAX_NB_GPIO_PER_BANK;

	range->npins = chip->ngpio;
	range->gc = chip;

	ret = gpiochip_add(chip);
	if (ret)
		goto gpiochip_add_err;

	gpio_chips[alias_idx] = h8300_chip;
	gpio_banks = max(gpio_banks, alias_idx + 1);

	dev_info(&pdev->dev, "at address %p\n", h8300_chip->dr);

	return 0;

gpiochip_add_err:
	kfree(names);
free_for_chip:
	kfree(h8300_chip);
err:
	dev_err(&pdev->dev, "Failure %i for GPIO %i\n", ret, alias_idx);

	return ret;
}

static struct platform_driver h8300_gpio_driver = {
	.driver = {
		.name = "gpio-h8300",
		.of_match_table = h8300_gpio_of_match,
	},
	.probe = h8300_gpio_probe,
};

static int __init h8300_gpio_init(void)
{
	return platform_driver_register(&h8300_gpio_driver);
}
module_init(h8300_gpio_init);

static void __exit h8300_gpio_exit(void)
{
	platform_driver_unregister(&h8300_gpio_driver);
}

module_exit(h8300_gpio_exit);
MODULE_AUTHOR("Yoshinori Sato <ysato@users.sourceforge.jp>");
MODULE_DESCRIPTION("Renesas H8/300 GPIO driver");
MODULE_LICENSE("GPL v2");
