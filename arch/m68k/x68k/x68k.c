#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/machdep.h>

/***************************************************************************/

static void reset(void)
{
	__asm__ volatile("movw #0x2700,%sr\n\t"
			 "movel #0xff0000, %a0\n\t"
			 "movel %a0@+,%sp\n\t"
			 "movel %a0@+,%a1\n\t"
			 "jmp %a1@");
}

static void poweroff(void)
{
	__raw_writeb(0x00, (void *)0xe8e00f);
	__raw_writeb(0x0f, (void *)0xe8e00f);
	__raw_writeb(0x0f, (void *)0xe8e00f);
}

void __init config_BSP(char *command, int len)
{
	mach_sched_init = hw_timer_init;
	mach_hwclk = NULL;
	mach_reset = reset;
	mach_power_off = poweroff;
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
