/*
 *  linux/arch/h8300/kernel/setup.c
 *
 *  Copyright (C) 2001-2014 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

/*
 * This file handles the architecture-dependent parts of system setup
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/console.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/bootparams.h>
#include <asm/page.h>

#if defined(CONFIG_CPU_H8300H)
#define CPU "H8/300H"
#elif defined(CONFIG_CPU_H8S)
#define CPU "H8S"
#else
#define CPU "Unknown"
#endif

struct bootparams bootparams;
unsigned long rom_length;
unsigned long memory_start;
unsigned long memory_end;
EXPORT_SYMBOL(memory_end);

char __initdata command_line[COMMAND_LINE_SIZE];

void sim_console_register(void);
int h8300_clk_init(unsigned long hz);

void __init __attribute__((weak)) early_device_init(void)
{
}

void __init setup_arch(char **cmdline_p)
{
	int bootmap_size;

#if defined(CONFIG_ROMKERNEL)
	bootparams.ram_end = CONFIG_RAMBASE + CONFIG_RAMSIZE;
	bootparams.clock_freq = CONFIG_CPU_CLOCK;
#endif

	memory_start = (unsigned long) &_ramend;

	memory_start = PAGE_ALIGN(memory_start);
	memory_end = bootparams.ram_end;

	init_mm.start_code = (unsigned long) _stext;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = (unsigned long) 0;

	pr_notice("\r\n\nuClinux " CPU "\n");
	pr_notice("Flat model support (C) 1998,1999 Kenneth Albanowski, D. Jeff Dionne\n");

	strcpy(boot_command_line, command_line);
	*cmdline_p = command_line;

	parse_early_param();

	/*
	 * give all the memory to the bootmap allocator,  tell it to put the
	 * boot mem_map at the start of memory
	 */
	bootmap_size = init_bootmem_node(
			NODE_DATA(0),
			memory_start >> PAGE_SHIFT, /* map goes here */
			PAGE_OFFSET >> PAGE_SHIFT,	/* 0 on coldfire */
			memory_end >> PAGE_SHIFT);
	/*
	 * free the usable memory,  we have to make sure we do not free
	 * the bootmem bitmap so we then reserve it after freeing it :-)
	 */
	free_bootmem(memory_start, memory_end - memory_start);
	reserve_bootmem(memory_start, bootmap_size, BOOTMEM_DEFAULT);
#if defined(CONFIG_H8300H_SIM) || defined(CONFIG_H8S_SIM)
	sim_console_register();
#endif

	early_device_init();
	early_platform_driver_probe("earlyprintk", 1, 0);
	/*
	 * get kmalloc into gear
	 */
	paging_init();
}

/*
 *	Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
	char *cpu;
	u_long clockfreq;

	cpu = CPU;
	clockfreq = bootparams.clock_freq;

	seq_printf(m,  "CPU:\t\t%s\n"
		   "Clock:\t\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu,
		   clockfreq/1000, clockfreq%1000,
		   (loops_per_jiffy*HZ)/500000,
		   ((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ));

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < num_possible_cpus() ?
		((void *) 0x12345678) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= show_cpuinfo,
};

void __init parse_bootparam(struct bootparams *bp)
{
	memcpy(&bootparams, bp, bp->size);
	strncpy(command_line, bootparams.command_line, sizeof(command_line));
}

static void __init timer_setup(void)
{
	early_platform_driver_register_all("earlytimer");
	early_platform_driver_probe("earlytimer", 1, 0);
}

void __init time_init(void)
{
	h8300_clk_init(bootparams.clock_freq);
	late_time_init = timer_setup;
}
