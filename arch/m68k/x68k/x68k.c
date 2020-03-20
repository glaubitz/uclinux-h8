#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/scsi_spc.h>

extern void x68k_init_IRQ(void);

/***************************************************************************/

static void reset(void)
{
	local_irq_disable(); /* lets not screw this up, ok? */
	if (CPU_IS_030) {

		/* 030-specific reset routine.  The idea is general, but the
		 * specific registers to reset are '030-specific.  Until I
		 * have a non-030 machine, I can't test anything else.
		 *  -- C. Scott Ananian <cananian@alumni.princeton.edu>
		 */

		unsigned long rombase = 0x40000000;

		/* make a 1-to-1 mapping, using the transparent tran. reg. */
		unsigned long virt = (unsigned long) reset;
		unsigned long phys = virt_to_phys(reset);
		unsigned long addr = (phys&0xFF000000)|0x8777;
		unsigned long offset = phys-virt;

		__asm__ __volatile__(".chip 68030\n\t"
				     "pmove %0,%/tt0\n\t"
				     ".chip 68k"
				     : : "m" (addr));
		/* Now jump to physical address so we can disable MMU */
		__asm__ __volatile__(
		    ".chip 68030\n\t"
		    "lea %/pc@(1f),%/a0\n\t"
		    "addl %0,%/a0\n\t"/* fixup target address and stack ptr */
		    "addl %0,%/sp\n\t"
		    "pflusha\n\t"
		    "jmp %/a0@\n\t" /* jump into physical memory */
		    "0:.long 0\n\t" /* a constant zero. */
		    /* OK.  Now reset everything and jump to reset vector. */
		    "1:\n\t"
		    "lea %/pc@(0b),%/a0\n\t"
		    "pmove %/a0@, %/tc\n\t" /* disable mmu */
		    "pmove %/a0@, %/tt0\n\t" /* disable tt0 */
		    "pmove %/a0@, %/tt1\n\t" /* disable tt1 */
		    "movel #0, %/a0\n\t"
		    "movec %/a0, %/vbr\n\t" /* clear vector base register */
		    "movec %/a0, %/cacr\n\t" /* disable caches */
		    "movel #0x0808,%/a0\n\t"
		    "movec %/a0, %/cacr\n\t" /* flush i&d caches */
		    "movew #0x2700,%/sr\n\t" /* set up status register */
		    "movel %1@(0x0),%/a0\n\t"/* load interrupt stack pointer */
		    "movec %/a0, %/isp\n\t"
		    ".chip 68k"
		    : : "r" (offset), "a" (rombase) : "a0");
	} else {
		__asm__ volatile( "movel #0xff0000, %a0\n\t"
			 "movel %a0@+,%sp\n\t"
			 "movel %a0@+,%a1\n\t"
			 "jmp %a1@");
	}
}

#if !defined(CONFIG_MMU)
#define SYSPORT6 0xe8e00f
#else
#define SYSPORT6 0xffe8e00f
#endif

static void poweroff(void)
{
	__raw_writeb(0x00, (void *)SYSPORT6);
	__raw_writeb(0x0f, (void *)SYSPORT6);
	__raw_writeb(0x0f, (void *)SYSPORT6);
}

#if !defined(CONFIG_MMU)
void __init config_BSP(char *command, int len)
{
	mach_sched_init = hw_timer_init;
	mach_hwclk = NULL;
	mach_reset = reset;
	mach_power_off = poweroff;
	memcpy(command, (void *)0x7e00, len);
}
#else
void __init config_x68k(void)
{
	mach_sched_init = hw_timer_init;
	mach_init_IRQ = x68k_init_IRQ;
	mach_hwclk = NULL;
	mach_reset = reset;
	mach_max_dma_address = 0xffffff;
	mach_power_off = poweroff;
}

#endif

struct resource scc_a_rsrcs[] = {
	{
		.flags = IORESOURCE_MEM,
		.start = 0xffe98005,
		.end = 0xffe98007,
	},
	
	{
		.flags = IORESOURCE_IRQ,
		.start = 24,
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
		.start = 0xffe96020,
		.end = 0xffe96040,
	},
	
	{
		.flags = IORESOURCE_IRQ,
		.start = 52,
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
	__raw_writeb(0, (void *)0xffe86001);
	platform_add_devices(x68k_devices, ARRAY_SIZE(x68k_devices));

	return 0;
}

arch_initcall(x68k_platform_init);
