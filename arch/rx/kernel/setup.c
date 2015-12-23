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
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/genhd.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/initrd.h>
#include <linux/platform_device.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

unsigned long rom_length;
unsigned long memory_start;
unsigned long memory_end;

#define COMMAND_LINE ((char *)0x400)

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

void early_device_register(void);

void __init setup_arch(char **cmdline_p)
{
	int bootmap_size;

#ifdef CONFIG_CMDLINE
	if (*COMMAND_LINE == 0)
		memcpy(boot_command_line, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#endif
	if(*boot_command_line == '\0')
		memcpy(boot_command_line, COMMAND_LINE, COMMAND_LINE_SIZE);
	*cmdline_p = boot_command_line;

#ifdef DEBUG
	if (strlen(boot_command_line)) 
		printk(KERN_DEBUG "Command line: '%s'\n", *cmdline_p);
#endif

	memory_start = (unsigned long) &_end;

	/* allow for ROMFS on the end of the kernel */
	if (memcmp((void *)memory_start, "-rom1fs-", 8) == 0) {
#if defined(CONFIG_BLK_DEV_INITRD)
		initrd_start = memory_start;
		initrd_end = memory_start += be32_to_cpu(((unsigned long *) (memory_start))[2]);
#else
		memory_start += be32_to_cpu(((unsigned long *) memory_start)[2]);
#endif
	}
	memory_start = PAGE_ALIGN(memory_start);
#if !defined(CONFIG_BLKDEV_RESERVE)
	memory_end = (unsigned long) &_ramend; /* by now the stack is part of the init task */
#else
	if ((memory_end < CONFIG_BLKDEV_RESERVE_ADDRESS) && 
	    (memory_end > CONFIG_BLKDEV_RESERVE_ADDRESS))
	    /* overlap userarea */
	    memory_end = CONFIG_BLKDEV_RESERVE_ADDRESS; 
#endif

	init_mm.start_code = (unsigned long) &_stext;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
	init_mm.brk = (unsigned long) &_end; 

	code_resource.start = virt_to_bus(&_stext);
	code_resource.end = virt_to_bus(&_etext)-1;
	data_resource.start = virt_to_bus(&_sdata);
	data_resource.end = virt_to_bus(&_edata)-1;
	bss_resource.start = virt_to_bus(&_sbss);
	bss_resource.end = virt_to_bus(&_ebss)-1;


#ifdef DEBUG
	printk(KERN_DEBUG "KERNEL -> TEXT=0x%06x-0x%06x DATA=0x%06x-0x%06x "
		"BSS=0x%06x-0x%06x\n", (int) &_stext, (int) &_etext,
		(int) &_sdata, (int) &_edata,
		(int) &_sbss, (int) &_ebss);
	printk(KERN_DEBUG "KERNEL -> ROMFS=0x%06x-0x%06x MEM=0x%06x-0x%06x "
		"STACK=0x%06x-0x%06x\n",
	       (int) &_ebss, (int) memory_start,
		(int) memory_start, (int) memory_end,
		(int) memory_end, (int) &_ramend);
#endif
	/*
	 * give all the memory to the bootmap allocator,  tell it to put the
	 * boot mem_map at the start of memory
	 */
	bootmap_size = init_bootmem_node(
			NODE_DATA(0),
			memory_start >> PAGE_SHIFT, /* map goes here */
			memory_start >> PAGE_SHIFT,
			memory_end >> PAGE_SHIFT);
	/*
	 * free the usable memory,  we have to make sure we do not free
	 * the bootmem bitmap so we then reserve it after freeing it :-)
	 */
	free_bootmem(memory_start, memory_end - memory_start);
	reserve_bootmem(memory_start, bootmap_size, BOOTMEM_DEFAULT);
	/* Let earlyprintk output early console messages */
	early_device_register();
	parse_early_param();
	early_platform_driver_probe("earlyprintk", 1, 1);
	/*
	 * get kmalloc into gear
	 */
	paging_init();

	insert_resource(&iomem_resource, &code_resource);
	insert_resource(&iomem_resource, &data_resource);
	insert_resource(&iomem_resource, &bss_resource);
#ifdef DEBUG
	printk(KERN_DEBUG "Done setup_arch\n");
#endif
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
