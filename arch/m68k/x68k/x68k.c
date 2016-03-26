#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/machdep.h>
#include <asm/scsi_spc.h>

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

struct resource scc_a_rsrcs[] = {
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

struct platform_device scc_b_pdev = {
       .name	       = "scc",
       .id	       = 1,
       .num_resources  = 0,
};

static struct resource spc_rsrcs[] = {
	{
		.flags = IORESOURCE_MEM,
		.start = 0xe96020,
		.end = 0xe96040,
	},
	
	{
		.flags = IORESOURCE_IRQ,
		.start = 0x6c,
	},
};

static struct spc_platform_data spc_platform_data = {
	.scsiid = 7,
	.parity = 0,
	.delay = 1,
};


static struct platform_device spc_pdev = {
	.name		= "spc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(spc_rsrcs),
	.resource       = spc_rsrcs,
	.dev = {
		.platform_data = &spc_platform_data,
	},
};

static struct platform_device *x68k_devices[] = {
	&scc_a_pdev,
	&spc_pdev,
};

static int __init x68k_platform_init(void)
{
	__raw_writeb(0, (void *)0xe86001);
	platform_add_devices(x68k_devices, ARRAY_SIZE(x68k_devices));

	return 0;
}

arch_initcall(x68k_platform_init);
