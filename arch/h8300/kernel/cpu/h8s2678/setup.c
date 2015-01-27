/*
 * H8S2678 Internal peripheral setup
 *
 *  Copyright (C) 2014  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/platform_device.h>
#include <linux/serial_sci.h>
#include <asm/timer.h>

static struct resource sci0_resources[] = {
	DEFINE_RES_MEM(0xffff78, 8),
	DEFINE_RES_IRQ(88),
	DEFINE_RES_IRQ(89),
	DEFINE_RES_IRQ(90),
	DEFINE_RES_IRQ(91),
};


static struct plat_sci_port sci0_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.type		= PORT_SCI,
};

static struct resource sci1_resources[] = {
	DEFINE_RES_MEM(0xffff80, 8),
	DEFINE_RES_IRQ(92),
	DEFINE_RES_IRQ(93),
	DEFINE_RES_IRQ(94),
	DEFINE_RES_IRQ(95),
};

static struct plat_sci_port sci1_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.type		= PORT_SCI,
};

static struct resource sci2_resources[] = {
	DEFINE_RES_MEM(0xffff88, 8),
	DEFINE_RES_IRQ(96),
	DEFINE_RES_IRQ(97),
	DEFINE_RES_IRQ(98),
	DEFINE_RES_IRQ(99),
};

static struct plat_sci_port sci2_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.type		= PORT_SCI,
};

static struct platform_device sci0_device = {
	.name		= "sh-sci",
	.id		= 0,
	.resource	= sci0_resources,
	.num_resources	= ARRAY_SIZE(sci0_resources),
	.dev		= {
		.platform_data	= &sci0_platform_data,
	},
};

static struct platform_device sci1_device = {
	.name		= "sh-sci",
	.id		= 1,
	.resource	= sci1_resources,
	.num_resources	= ARRAY_SIZE(sci1_resources),
	.dev		= {
		.platform_data	= &sci1_platform_data,
	},
};

static struct platform_device sci2_device = {
	.name		= "sh-sci",
	.id		= 2,
	.resource	= sci2_resources,
	.num_resources	= ARRAY_SIZE(sci2_resources),
	.dev		= {
		.platform_data	= &sci2_platform_data,
	},
};

static struct h8300_timer8_config timer8_platform_data = {
	.mode	= H8300_TMR8_CLKEVTDEV,
	.div	= H8300_TMR8_DIV_8,
};

static struct resource tm8_unit0_resources[] = {
	DEFINE_RES_MEM(0xffffb0, 10),
	DEFINE_RES_IRQ(72),
	DEFINE_RES_IRQ(75),
};

static struct platform_device tm8_unit0_device = {
	.name		= "h8300-8timer",
	.id		= 0,
	.dev = {
		.platform_data	= &timer8_platform_data,
	},
	.resource	= tm8_unit0_resources,
	.num_resources	= ARRAY_SIZE(tm8_unit0_resources),
};

static struct resource tpu12_resources[] = {
	DEFINE_RES_MEM(0xffffe0, 16),
	DEFINE_RES_MEM(0xfffff0, 12),
};

static struct resource tpu45_resources[] = {
	DEFINE_RES_MEM(0xfffe90, 16),
	DEFINE_RES_MEM(0xfffea0, 12),
};

static struct platform_device tpu12_device = {
	.name	= "h8s-tpu",
	.id		= 0,
	.resource	= tpu12_resources,
	.num_resources	= ARRAY_SIZE(tpu12_resources),
};

static struct platform_device tpu45_device = {
	.name	= "h8s-tpu",
	.id		= 1,
	.resource	= tpu45_resources,
	.num_resources	= ARRAY_SIZE(tpu45_resources),
};

static struct platform_device *devices[] __initdata = {
	&tpu12_device,
	&tpu45_device,
	&sci0_device,
	&sci1_device,
	&sci2_device,
};

static struct platform_device *early_devices[] __initdata = {
	&tm8_unit0_device,
	&sci0_device,
	&sci1_device,
	&sci2_device,
};

static int __init devices_register(void)
{
	return platform_add_devices(devices,
				    ARRAY_SIZE(devices));
}
arch_initcall(devices_register);

void __init early_device_init(void)
{
	/* SCI / Timer enable */
	ctrl_outw(0x07f0, 0xffff40);
	early_platform_add_devices(early_devices,
				   ARRAY_SIZE(early_devices));
}
