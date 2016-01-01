/*
 * arch/rx/kernel/process.c
 *
 * This file handles the architecture-dependent parts of process handling..
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  RX version:  Copyright (C) 2009 Yoshinori Sato
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/elfcore.h>
#include <linux/kallsyms.h>
#include <linux/fs.h>
#include <linux/ftrace.h>
#include <linux/preempt.h>
#include <asm/uaccess.h>
#include <asm/mmu_context.h>
#include <asm/pgalloc.h>
#include <asm/system.h>

void (*pm_power_off)(void) = NULL;
EXPORT_SYMBOL(pm_power_off);

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	while (1) {
		while (!need_resched())
			__asm__ volatile("wait");
		preempt_enable_no_resched();
		schedule();
		preempt_disable();
	}
}

void __noreturn machine_restart(char * __unused)
{
	unsigned long tmp;
	local_irq_disable();
	__asm__ volatile("mov.l #0xfffffffc,%0\n\t"
			 "mov.l [%0],%0\n\t"
			 "jmp %0"
			 :"=r"(tmp));
	for(;;);
}

void __noreturn machine_halt(void)
{
	local_irq_disable();

	while (1)
		__asm__ volatile("wait");
}

void machine_power_off(void)
{
	/* Nothing do. */
}

void show_regs(struct pt_regs * regs)
{
	printk("\n");
	printk("Pid : %d, Comm: \t\t%s\n", task_pid_nr(current), current->comm);
	print_symbol("PC is at %s\n", instruction_pointer(regs));

	printk("PC  : %08lx SP  : %08lx PSW  : %08lx\n",
	       regs->pc, regs->r[0], regs->psw);
	printk("R1  : %08lx R2  : %08lx R3  : %08lx\n",
	       regs->r[1],
	       regs->r[2],regs->r[3]);
	printk("R4  : %08lx R5  : %08lx R6  : %08lx R7  : %08lx\n",
	       regs->r[4],regs->r[5],
	       regs->r[6],regs->r[7]);
	printk("R8  : %08lx R9  : %08lx R10 : %08lx R11 : %08lx\n",
	       regs->r[8],regs->r[9],
	       regs->r[10],regs->r[11]);
	printk("R12 : %08lx R13 : %08lx R14 : %08lx R15 : %08lx\n",
	       regs->r[12],regs->r[13],
	       regs->r[14],regs->r[15]);
}

asmlinkage void ret_from_fork(void);
asmlinkage void ret_from_kernel_thread(void);

int copy_thread(unsigned long clone_flags,
                unsigned long usp, unsigned long topstk,
		 struct task_struct * p)
{
	struct pt_regs *childregs = 
		(struct pt_regs *) (THREAD_SIZE + task_stack_page(p)) - 1;

	if (unlikely(p->flags & PF_KTHREAD)) {
		memset(childregs, 0, sizeof(struct pt_regs));
		p->thread.pc = (unsigned long) ret_from_kernel_thread;
		childregs->r[1] = topstk; /* arg */
		childregs->r[2] = usp; /* fn */
	}  else {
		*childregs = *current_pt_regs();
		childregs->r[1] = 0;
		p->thread.pc = (unsigned long)ret_from_fork;
		childregs->usp  = usp;
	}

	p->thread.sp = (unsigned long)childregs;

	return 0;
}

unsigned long get_wchan(struct task_struct *p)
{
	int count = 0;
	unsigned long pc;
	unsigned long fp;
	unsigned long stack_page;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)p;
	fp = ((struct pt_regs *)p->thread.sp)->r[6];
	do {
		if (fp < stack_page+sizeof(struct thread_info) ||
		    fp >= (THREAD_SIZE - sizeof(struct pt_regs) +stack_page))
			return 0;
		pc = ((unsigned long *)fp)[1];
		if (!in_sched_functions(pc))
			return pc;
		fp = *(unsigned long *) fp;
	} while (count++ < 16);
	return 0;
}
