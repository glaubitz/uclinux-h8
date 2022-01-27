/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _RX_PAGE_H
#define _RX_PAGE_H

#ifndef __ASSEMBLY__
extern unsigned int rx_ram_pfn_base;
#define ARCH_PFN_OFFSET (rx_ram_pfn_base)
#endif
#include <asm-generic/page.h>
#include <linux/types.h>

#define MAP_NR(addr) (((uintptr_t)(addr)-PAGE_OFFSET) >> PAGE_SHIFT)
#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif
