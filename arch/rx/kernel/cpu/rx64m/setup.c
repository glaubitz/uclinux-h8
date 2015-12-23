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
	/* SCIF0 to SCIF3 */
	{
		.mapbase	= 0x000d0000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_RXSCIF,
		.scscr		= SCSCR_RE | SCSCR_TE,
		.scbrr_algo_id	= -1,
		.irqs		= { 100, 101, 0, 0 },
	}, {
		.mapbase	= 0x000d0020,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_RXSCIF,
		.scscr		= SCSCR_RE | SCSCR_TE,
		.scbrr_algo_id	= -1,
		.irqs		= { 102, 103, 0, 0 },
	}, {
		.mapbase	= 0x000d0040,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_RXSCIF,
		.scscr		= SCSCR_RE | SCSCR_TE,
		.scbrr_algo_id	= -1,
		.irqs		= { 104, 105, 0, 0 },
	}, {
		.mapbase	= 0x000d0060,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_RXSCIF,
		.scscr		= SCSCR_RE | SCSCR_TE,
		.scbrr_algo_id	= -1,
		.irqs		= { 114, 115, 0, 0 },
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
	{
		.name		= "sh-sci",
		.id		= 2,
		.dev		= {
			.platform_data	= &sci_platform_data[2],
		},
	},
	{
		.name		= "sh-sci",
		.id		= 3,
		.dev		= {
			.platform_data	= &sci_platform_data[3],
		},
	},
};

static struct platform_device *rx64m_devices[] __initdata = {
	&sci_device[0],
	&sci_device[1],
	&sci_device[2],
	&sci_device[3],
};

static int __init devices_register(void)
{
	return platform_add_devices(rx64m_devices,
				    ARRAY_SIZE(rx64m_devices));
}
arch_initcall(devices_register);

void __init early_device_register(void)
{
	early_platform_add_devices(rx64m_devices,
				   ARRAY_SIZE(rx64m_devices));
	*(volatile unsigned long *)0x00080010 &= ~0x00008000;
}
