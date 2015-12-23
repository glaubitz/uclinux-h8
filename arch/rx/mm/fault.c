/*
 * arch/rx/mm/fault.c
 */

#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/kdebug.h>

asmlinkage int do_page_fault(struct pt_regs *regs, unsigned long address)
{
	if ((unsigned long) address < PAGE_SIZE) {
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	} else
		printk(KERN_ALERT "Unable to handle kernel access");
	printk(" at virtual address %08lx\n",address);
	if (!user_mode(regs))
		die("Oops", regs);
	do_exit(SIGKILL);

	return 1;
}

