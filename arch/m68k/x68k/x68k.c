#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/machdep.h>

/***************************************************************************/

void scc_puts(char *);

void __init config_BSP(char *command, int len)
{
	mach_sched_init = hw_timer_init;
	mach_hwclk = NULL;
	mach_reset = NULL;
	memcpy(command, (void *)0x7e00, len);
}

static struct resource scc_a_rsrcs[] = {
	{
		.flags = IORESOURCE_MEM,
		.start = 0xe98005,
		.end = 0xe98007,
	},
	
	{
		.flags = IORESOURCE_IRQ,
		.start = 0x50,
	},
};

struct platform_device scc_a_pdev = {
	.name           = "scc",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(scc_a_rsrcs),
	.resource       = scc_a_rsrcs,
};
EXPORT_SYMBOL(scc_a_pdev);

struct platform_device scc_b_pdev = {
	.name           = "scc",
	.id             = 1,
	.num_resources  = 0,
};
EXPORT_SYMBOL(scc_b_pdev);

static int __init x68k_platform_init(void)
{
	__raw_writeb(0, (void *)0xe86001);
	platform_device_register(&scc_a_pdev);

	return 0;
}

arch_initcall(x68k_platform_init);
