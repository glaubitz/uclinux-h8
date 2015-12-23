/*
 * RX610 Internal peripheral setup
 *
 *  Copyright (C) 2009  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/platform_device.h>
#include <linux/serial_sci.h>

static struct plat_sci_port sci0_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.type		= PORT_SCI,
	.scscr		= SCSCR_RE | SCSCR_TE,
};
static struct plat_sci_port sci1_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.type		= PORT_SCI,
	.scscr		= SCSCR_RE | SCSCR_TE,
};
static struct plat_sci_port sci2_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.type		= PORT_SCI,
	.scscr		= SCSCR_RE | SCSCR_TE,
};

static struct resource sci0_resource[] = {
	DEFINE_RES_MEM(0x00088240, 8),
	DEFINE_RES_IRQ(214),
};

static struct resource sci1_resource[] = {
	DEFINE_RES_MEM(0x00088248, 8),
	DEFINE_RES_IRQ(218),
};

static struct resource sci2_resource[] = {
	DEFINE_RES_MEM(0x00088250, 8),
	DEFINE_RES_IRQ(222),
};

static struct platform_device sci_device[] = {
	{
		.name		= "sh-sci",
		.id		= 0,
		.resource	= sci0_resource,
		.num_resources	= ARRAY_SIZE(sci0_resource),
		.dev		= {
			.platform_data	= &sci0_platform_data,
		},
	},
	{
		.name		= "sh-sci",
		.id		= 1,
		.resource	= sci1_resource,
		.num_resources	= ARRAY_SIZE(sci1_resource),
		.dev		= {
			.platform_data	= &sci1_platform_data,
		},
	},
	{
		.name		= "sh-sci",
		.id		= 2,
		.resource	= sci2_resource,
		.num_resources	= ARRAY_SIZE(sci2_resource),
		.dev		= {
			.platform_data	= &sci2_platform_data,
		},
	},
};

static struct platform_device *rx62n_devices[] __initdata = {
	&sci_device[0],
	&sci_device[1],
	&sci_device[2],
};

static int __init devices_register(void)
{
	return platform_add_devices(rx62n_devices,
				    ARRAY_SIZE(rx62n_devices));
}
arch_initcall(devices_register);

void __init early_device_register(void)
{
	early_platform_add_devices(rx62n_devices,
				   ARRAY_SIZE(rx62n_devices));
}
