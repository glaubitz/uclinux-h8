/*
 * H8/3069 clock driver
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

#define DIVCR (unsigned char *)0xfee01b
#define DEVNAME "h83069-cpg"

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

	clk = clk_register_divider(&pdev->dev, "core_clk", "master_clk",
				   CLK_SET_RATE_GATE, DIVCR, 0, 2,
				   CLK_DIVIDER_POWER_OF_TWO, &clklock);
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
