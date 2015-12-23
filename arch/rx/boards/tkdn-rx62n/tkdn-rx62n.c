/*
 * Extream RX62N board on-board peripheral setup
 *
 *  Copyright (C) 2011  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/platform_device.h>
#include <linux/serial_sci.h>
#include <asm/sh_eth.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_rspi.h>
#include <asm/io.h>

static void tkdn_rx62n_spi_cs(struct spi_device *spi, int is_on);

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

static struct spi_board_info tkdn_rx62n_spi_flash[] = {
	[0] = {
		.mode = 3,
		.bus_num = 1,
		.chip_select = 0,
		.max_speed_hz = 2500UL,
		.modalias = "m25p80",
	},
};

static struct rspi_platform_data rspi_platform_data[] = {
	[0] = {
		.num_cs = 0,
		.csfunc = NULL,
		.num_devices = 0,
		.devices = NULL,
	},
	[1] = {
		.num_cs = 1,
		.csfunc = tkdn_rx62n_spi_cs,
		.num_devices = 1,
		.devices = tkdn_rx62n_spi_flash,
	},
};

static struct resource rspi_resources[2][2] = {
	{
		[0] = {
			.start = 0x00088380,
			.end =   0x0008839f,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = 44,
			.end = 47,
			.flags = IORESOURCE_IRQ,
		},
	},
	{
		[0] = {
			.start = 0x000883a0,
			.end =   0x000883bf,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = 48,
			.end = 51,
			.flags = IORESOURCE_IRQ,
		},
	},
};

static struct platform_device rspi_device = {
	.name		= "rspi",
	.id		= 1,
	.dev		= {
		.platform_data	= &rspi_platform_data[1],
	},
	.num_resources = ARRAY_SIZE(rspi_resources[1]),
	.resource = rspi_resources[1],
};

static struct platform_device *tkdn_rx62n_devices[] __initdata = {
#ifdef CONFIG_NET
	&eth_device,
#endif
#ifdef CONFIG_SPI
	&rspi_device,
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
#ifdef CONFIG_SPI
	/* Use RSPI1-A 4-wire interface */
	writeb(0x0e, (void __iomem *)0x8c111);
	writeb(readb((void __iomem *)0x8c063) | 0x01, (void __iomem *)0x8c063);
	writeb(readb((void __iomem *)0x8c023) | 0x02, (void __iomem *)0x8c023);
	writeb(readb((void __iomem *)0x8c003) | 0x02, (void __iomem *)0x8c003);
	/* enable RSPI1 */
	writel(readl((void __iomem *)0x80014) & ~0x00010000,
	       (void __iomem *)0x80014);
#endif
	return platform_add_devices(tkdn_rx62n_devices,
				    ARRAY_SIZE(tkdn_rx62n_devices));
}
arch_initcall(init_board);

static void tkdn_rx62n_spi_cs(struct spi_device *spi, int is_on)
{
	unsigned char dr;
	dr = readb((void __iomem *)0x8c023);
	if (is_on)
		dr &= ~0x02;
	else
		dr |= 0x02;
	writeb(dr, (void __iomem *)0x8c023);
}
