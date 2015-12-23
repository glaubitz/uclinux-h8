/*
 * RX62N Internal peripheral setup
 *
 *  Copyright (C) 2011  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/platform_device.h>
#include <linux/serial_sci.h>

static struct plat_sci_port sci_platform_data[] = {
	/* SCI0 to SCI1 */
	{
		.mapbase	= 0x00088240,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCI,
		.scscr		= SCSCR_RE | SCSCR_TE,
		.scbrr_algo_id	= -1,
		.irqs		= { 214, 215, 216, 0 },
	}, {
		.mapbase	= 0x00088248,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCI,
		.scscr		= SCSCR_RE | SCSCR_TE,
		.scbrr_algo_id	= -1,
		.irqs		= { 218, 219, 220, 0 },
	}, {
		.flags = 0,
	}
};

static struct platform_device sci_device[] = {
	{
		.name		= "sh-sci",
		.id		= 0,
		.dev		= {
			.platform_data	= &sci_platform_data[0],
		},
	},
	{
		.name		= "sh-sci",
		.id		= 1,
		.dev		= {
			.platform_data	= &sci_platform_data[1],
		},
	},
};

static struct platform_device *rx62n_devices[] __initdata = {
	&sci_device[0],
	&sci_device[1],
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
	*(volatile unsigned long *)0x00080010 &= ~0x00008000;
}
