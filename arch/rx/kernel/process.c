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

/*
 * Create a kernel thread
 */
static void __noreturn kernel_thread_helper(int dummy, int (*fn)(void *), void *arg)
{
	fn(arg);
	do_exit(-1);
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.r[2] = (unsigned long)fn;
	regs.r[3] = (unsigned long)arg;

	regs.pc = (unsigned long)kernel_thread_helper;
	regs.psw = 0;

	/* Ok, create the new process.. */
	return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0,
		      &regs, 0, NULL, NULL);
}

asmlinkage void ret_from_fork(void);

int copy_thread(unsigned long clone_flags,
                unsigned long usp, unsigned long topstk,
		 struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs *childregs = 
		(struct pt_regs *) (THREAD_SIZE + task_stack_page(p)) - 1;

	*childregs = *regs;
	childregs->usp  = usp;
	childregs->r[1] = 0;

	p->thread.sp = (unsigned long)childregs;
	p->thread.pc = (unsigned long)ret_from_fork;

	return 0;
}

asmlinkage int sys_fork(void)
{
	return -EINVAL;
}

asmlinkage int rx_clone(struct pt_regs *regs)
{
	unsigned long clone_flags = regs->r[1];
	unsigned long newsp = regs->r[2];
	unsigned long parent_tidptr = regs->r[3];
	unsigned long child_tidptr = regs->r[4];

	if (!newsp)
		newsp = regs->usp;
	return do_fork(clone_flags, newsp, regs, 0,
			(int __user *)parent_tidptr,
			(int __user *)child_tidptr);
}

asmlinkage int rx_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->usp, regs,
		       0, NULL, NULL);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(const char __user *ufilename, const char __user * __user *uargv,
			  const char __user * __user *uenvp, int dummy, ...)
{
	struct pt_regs *regs = (struct pt_regs *)
		((unsigned char *)&dummy + 8);
	int error;
	char *filename;

	filename = getname(ufilename);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	error = do_execve(filename, uargv, uenvp, regs);
	putname(filename);
out:
	return error;
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
