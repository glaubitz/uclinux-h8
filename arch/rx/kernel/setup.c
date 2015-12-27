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
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/memblock.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

unsigned long rom_length;
unsigned long memory_start;
unsigned long memory_end;

static struct resource code_resource = {
	.name	= "Kernel code",
};

static struct resource data_resource = {
	.name	= "Kernel data",
};

static struct resource bss_resource = {
	.name	= "Kernel bss",
};

extern void *_ramstart, *_ramend;
extern void *_stext, *_etext;
extern void *_sdata, *_edata;
extern void *_sbss, *_ebss;
extern void *_end;

void __init rx_fdt_init(void *fdt)
{
	char saved_command_line[512];

	*saved_command_line = 0;
	if (fdt == (void *)-1) {
		memcpy(saved_command_line, boot_command_line,
		       sizeof(saved_command_line));
		fdt = NULL;
	}
	if (!fdt)
		fdt = __dtb_start;

	early_init_dt_scan(fdt);
	memblock_allow_resize();
	if (*saved_command_line) {
		memcpy(boot_command_line, saved_command_line,
		       sizeof(saved_command_line));
	}
}

static void __init bootmem_init(void)
{
	int bootmap_size;
	unsigned long ram_start_pfn;
	unsigned long free_ram_start_pfn;
	unsigned long ram_end_pfn;
	struct memblock_region *region;

	memory_end = memory_start = 0;

	/* Find main memory where is the kernel */
	for_each_memblock(memory, region) {
		memory_start = region->base;
		memory_end = region->base + region->size;
	}

	if (!memory_end)
		panic("No memory!");

	ram_start_pfn = PFN_UP(memory_start);
	/* free_ram_start_pfn is first page after kernel */
	free_ram_start_pfn = PFN_UP(__pa(&_end));
	ram_end_pfn = PFN_DOWN(memblock_end_of_DRAM());

	max_pfn = ram_end_pfn;

	/*
	 * give all the memory to the bootmap allocator,  tell it to put the
	 * boot mem_map at the start of memory
	 */
	bootmap_size = init_bootmem_node(NODE_DATA(0),
					 free_ram_start_pfn,
					 0,
					 ram_end_pfn);
	/*
	 * free the usable memory,  we have to make sure we do not free
	 * the bootmem bitmap so we then reserve it after freeing it :-)
	 */
	free_bootmem(PFN_PHYS(free_ram_start_pfn),
		     (ram_end_pfn - free_ram_start_pfn) << PAGE_SHIFT);
	reserve_bootmem(PFN_PHYS(free_ram_start_pfn), bootmap_size,
			BOOTMEM_DEFAULT);

	for_each_memblock(reserved, region) {
		reserve_bootmem(region->base, region->size, BOOTMEM_DEFAULT);
	}
}

void __init setup_arch(char **cmdline_p)
{
	unflatten_and_copy_device_tree();

	init_mm.start_code = (unsigned long) &_stext;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
	init_mm.brk = 0; 

	*cmdline_p = boot_command_line;

	parse_early_param();
	bootmem_init();
	/*
	 * get kmalloc into gear
	 */
	paging_init();

	insert_resource(&iomem_resource, &code_resource);
	insert_resource(&iomem_resource, &data_resource);
	insert_resource(&iomem_resource, &bss_resource);
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
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	return 0;
}

device_initcall(device_probe);

void __init time_init(void)
{
	of_clk_init(NULL);
	clocksource_probe();
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
