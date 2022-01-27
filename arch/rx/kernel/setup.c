/*
 * arch/rx/kernel/setup.c
 * This file handles the architecture-dependent parts of system setup
 *
 * Copyright (C) 2009 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
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
#include <linux/major.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/memblock.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <linux/screen_info.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

unsigned long memory_start;
unsigned long memory_end;
unsigned int rx_ram_pfn_base;

#ifdef CONFIG_VT
struct screen_info screen_info;
#endif


void __init rx_fdt_init(phys_addr_t dt_phys)
{
	void *dt_virt;

#ifdef CONFIG_USE_BUILTIN_DTB
	dt_virt = __dtb_start;
#else
	dt_virt = phys_to_virt(dt_phys);
#endif

	early_init_dt_scan(dt_virt);
}

static void __init bootmem_init(void)
{
	memory_end = memory_start = 0;

	/* Find main memory where is the kernel */
	memory_start = memblock_start_of_DRAM();
	memory_end = memblock_end_of_DRAM();

	if (!memory_end)
		panic("No memory!");

	/* setup bootmem globals (we use no_bootmem, but mm still depends on this) */
	rx_ram_pfn_base = memory_start >> PAGE_SHIFT;
	min_low_pfn = PFN_UP(memory_start);
	max_low_pfn = PFN_DOWN(memory_end);
	max_pfn = max_low_pfn;

	memblock_reserve(__pa(_stext), _end - _stext);

	early_init_fdt_reserve_self();
	early_init_fdt_scan_reserved_mem();

}

void __init setup_arch(char **cmdline_p)
{
	unflatten_device_tree();

	setup_initial_init_mm(_text, _etext, _edata, _end);

	*cmdline_p = boot_command_line;

	parse_early_param();
	bootmem_init();
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

#if defined(CONFIG_CPU_RX610)
    cpu = "RX610";
#elif defined(CONFIG_CPU_RX62N)
    cpu = "RX62N";
#else
    cpu = "Unknown";
#endif

    seq_printf(m,  "CPU:\t\t%s\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
	           cpu,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ));

    return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? ((void *) 0x12345678) : NULL;
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

static int __init device_probe(void)
{
	of_platform_default_populate(NULL, NULL, NULL);

	return 0;
}

device_initcall(device_probe);

void __init time_init(void)
{
	of_clk_init(NULL);
	timer_probe();
}

void __init calibrate_delay(void)
{
	struct device_node *cpu;
	int freq;
	int cycle;

	cpu = of_find_compatible_node(NULL, NULL, "renesas,rx");
	of_property_read_s32(cpu, "clock-frequency", &freq);
	of_property_read_s32(cpu, "mem-cycle", &cycle);
	loops_per_jiffy = freq / HZ / cycle;
	pr_cont("%lu.%02lu BogoMIPS (lpj=%lu)\n",
		loops_per_jiffy / (500000 / HZ),
		(loops_per_jiffy / (5000 / HZ)) % 100, loops_per_jiffy);
}
