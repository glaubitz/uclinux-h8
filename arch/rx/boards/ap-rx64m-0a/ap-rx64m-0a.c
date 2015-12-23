/*
 * AP-RX64M-0A board on-board peripheral setup
 *
 *  Copyright (C) 2015  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/platform_device.h>
#include <linux/serial_sci.h>
#include <asm/sh_eth.h>
#include <asm/io.h>

static struct sh_eth_plat_data rx_eth_plat_data = {
	.phy = 0,
	.edmac_endian = EDMAC_LITTLE_ENDIAN,
};

static struct resource eth_resources[] = {
	[0] = {
		.start = 0x000c0000,
		.end =   0x000c01fc,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 32,
		.end = 32,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device eth_device = {
	.name = "sh-eth",
	.id	= -1,
	.dev = {
		.platform_data = &rx_eth_plat_data,
	},
	.num_resources = ARRAY_SIZE(eth_resources),
	.resource = eth_resources,
};

static struct platform_device *ap_rx64m_0a_devices[] __initdata = {
#ifdef CONFIG_NET
	&eth_device,
#endif
};

static int __init init_board(void)
{
#ifdef CONFIG_NET
	/* Use RMII interface */
	writeb(0x82, (void __iomem *)0x8c10e);
	writeb(readb((void __iomem *)0x8c065) | 0x10, (void __iomem *)0x8c065);
	writeb(readb((void __iomem *)0x8c067) | 0xf2, (void __iomem *)0x8c067);
	writeb(readb((void __iomem *)0x8c068) | 0x08, (void __iomem *)0x8c068);
	/* enable EDMAC */
	writel(readl((void __iomem *)0x80014) & ~0x00008000, 
	       (void __iomem *)0x80014);

#endif
	return platform_add_devices(ap_rx64m_0a_devices,
				    ARRAY_SIZE(ap_rx64m_0a_devices));
}
arch_initcall(init_board);

