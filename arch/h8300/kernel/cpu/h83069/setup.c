/*
 * H8/3069 Internal peripheral setup
 *
 *  Copyright (C) 2009,2014 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/platform_device.h>
#include <linux/serial_sci.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/timer.h>

static struct resource sci0_resources[] = {
	DEFINE_RES_MEM(0xffffb0, 8),
	DEFINE_RES_IRQ(52),
	DEFINE_RES_IRQ(53),
	DEFINE_RES_IRQ(54),
	DEFINE_RES_IRQ(55),
};


static struct plat_sci_port sci0_platform_data = {
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.type		= PORT_SCI,
};

static struct resource sci1_resources[] = {
	DEFINE_RES_MEM(0xffffb8, 8),
	DEFINE_RES_IRQ(56),
	DEFINE_RES_IRQ(57),
	DEFINE_RES_IRQ(58),
	DEFINE_RES_IRQ(59),
};

static struct plat_sci_port sci1_platform_data = {
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

static struct h8300_timer8_config tm8_unit0_platform_data = {
	.mode	= H8300_TMR8_CLKEVTDEV,
	.div	= H8300_TMR8_DIV_8,
};

static struct resource tm8_unit0_resources[] = {
	DEFINE_RES_MEM(0xffff80, 9),
	DEFINE_RES_IRQ(36),
	DEFINE_RES_IRQ(39),
};

static struct platform_device timer8_unit0_device = {
	.name		= "h8300-8timer",
	.id		= 0,
	.dev = {
		.platform_data	= &tm8_unit0_platform_data,
	},
	.resource	= tm8_unit0_resources,
	.num_resources	= ARRAY_SIZE(tm8_unit0_resources),
};

static struct h8300_timer8_config tm8_unit1_platform_data = {
	.mode	= H8300_TMR8_CLKSRC,
	.div	= H8300_TMR8_DIV_8,
};

static struct resource tm8_unit1_resources[] = {
	DEFINE_RES_MEM(0xffff90, 9),
	DEFINE_RES_IRQ(40),
	DEFINE_RES_IRQ(43),
};

static struct platform_device timer8_unit1_device = {
	.name		= "h8300-8timer",
	.id		= 1,
	.dev = {
		.platform_data	= &tm8_unit1_platform_data,
	},
	.resource	= tm8_unit1_resources,
	.num_resources	= ARRAY_SIZE(tm8_unit1_resources),
};


static struct h8300_timer16_config timer16data0 = {
	.enb	= 0,
	.imfa	= 0,
	.imiea	= 4,
};

static struct h8300_timer16_config timer16data1 = {
	.enb	= 1,
	.imfa	= 1,
	.imiea	= 5,
};

static struct h8300_timer16_config timer16data2 = {
	.enb	= 2,
	.imfa	= 2,
	.imiea	= 6,
};

static struct resource tm16ch0_resources[] = {
	DEFINE_RES_MEM(0xffff68, 8),
	DEFINE_RES_MEM(0xffff60, 7),
	DEFINE_RES_IRQ(24),
};

static struct resource tm16ch1_resources[] = {
	DEFINE_RES_MEM(0xffff70, 8),
	DEFINE_RES_MEM(0xffff60, 7),
	DEFINE_RES_IRQ(28),
};

static struct resource tm16ch2_resources[] = {
	DEFINE_RES_MEM(0xffff78, 8),
	DEFINE_RES_MEM(0xffff60, 7),
	DEFINE_RES_IRQ(32),
};

static struct platform_device timer16_ch0_device = {
	.name	= "h8300h-16timer",
	.id		= 0,
	.dev		= {
		.platform_data	= &timer16data0,
	},
	.resource	= tm16ch0_resources,
	.num_resources	= ARRAY_SIZE(tm16ch0_resources),
};

static struct platform_device timer16_ch1_device = {
	.name	= "h8300h-16timer",
	.id		= 1,
	.dev		= {
		.platform_data	= &timer16data1,
	},
	.resource	= tm16ch1_resources,
	.num_resources	= ARRAY_SIZE(tm16ch1_resources),
};

static struct platform_device timer16_ch2_device = {
	.name	= "h8300h-16timer",
	.id		= 2,
	.dev		= {
		.platform_data	= &timer16data2,
	},
	.resource	= tm16ch2_resources,
	.num_resources	= ARRAY_SIZE(tm16ch2_resources),
};

static struct platform_device *devices[] __initdata = {
	&timer8_unit1_device,
	&timer16_ch0_device,
	&timer16_ch1_device,
	&timer16_ch2_device,
	&sci0_device,
	&sci1_device,
};

static struct platform_device *early_devices[] __initdata = {
	&timer8_unit0_device,
	&sci0_device,
	&sci1_device,
};

static int __init devices_register(void)
{
	return platform_add_devices(devices,
				    ARRAY_SIZE(devices));
}

arch_initcall(devices_register);

void __init early_device_init(void)
{
	early_platform_add_devices(early_devices,
				   ARRAY_SIZE(early_devices));
}
