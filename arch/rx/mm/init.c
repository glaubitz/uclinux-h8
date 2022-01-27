/*
 *  arch/rx/mm/init.c
 * Copyright (C) 2009 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 *  
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/memblock.h>
#include <linux/gfp.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/sections.h>

/*
 * ZERO_PAGE is a special page that is used for zero-initialized
 * data and COW.
 */
unsigned long empty_zero_page;

/*
 * paging_init() continues the virtual memory environment setup which
 * was begun by the code in arch/head.S.
 * The parameters are pointers to where to stick the starting and ending
 * addresses of available kernel virtual memory.
 */
void __init paging_init(void)
{
	/*
	 * Make sure start_mem is page aligned,  otherwise bootmem and
	 * page_alloc get different views og the world.
	 */
	unsigned long start_mem = PAGE_ALIGN(memory_start);
	unsigned long end_mem   = memory_end & PAGE_MASK;

	pr_debug ("start_mem is %#lx\nvirtual_end is %#lx\n",
		start_mem, end_mem);

	/*
	 * Initialize the bad page table and bad page to point
	 * to a couple of allocated pages.
	 */
	empty_zero_page = (unsigned long)memblock_alloc(PAGE_SIZE, PAGE_SIZE);
	if (!empty_zero_page)
		panic("%s: Failed to allocate %lu bytes align=0x%lx\n",
		      __func__, PAGE_SIZE, PAGE_SIZE);

	pr_debug ("before free_area_init\n");
	pr_debug ("free_area_init -> start_mem is %#lx\nvirtual_end is %#lx\n",
		start_mem, end_mem);

	{
		unsigned long max_zone_pfn[MAX_NR_ZONES] = {0, };

		max_zone_pfn[ZONE_NORMAL] = max_low_pfn;
		free_area_init(max_zone_pfn);
	}
}

void __init mem_init(void)
{
	pr_devel("Mem_init: start=%lx, end=%lx\n", memory_start, memory_end);

	high_memory = (void *) (memory_end & PAGE_MASK);
	max_mapnr = MAP_NR(high_memory);

	/* this will put all low memory onto the freelists */
	memblock_free_all();
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	free_reserved_area((void *)start, (void *)end, -1, "initrd");
}
#endif

void
free_initmem(void)
{
	free_initmem_default(-1);
}
