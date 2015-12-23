#ifndef __ASM_RX_SYSCALL_H__
#define __ASM_RX_SYSCALL_H__

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <asm/ptrace.h>

/* The system call number is given by the user in R3 */
static inline long syscall_get_nr(struct task_struct *task,
				  struct pt_regs *regs)
{
	return (regs->vec >= 0x1000) ? regs->r[8] : -1L;
}

static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	return IS_ERR_VALUE(regs->r[1]) ? regs->r[1] : 0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->r[1];
}

static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	if (error)
		regs->r[1] = -error;
	else
		regs->r[1] = val;
}

static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 unsigned long *args)
{
	/*
	 * Do this simply for now. If we need to start supporting
	 * fetching arguments from arbitrary indices, this will need some
	 * extra logic. Presently there are no in-tree users that depend
	 * on this behaviour.
	 */
	BUG_ON(i);

	switch (n) {
	case 6: args[5] = regs->r[7];
	case 5: args[4] = regs->r[5];
	case 4: args[3] = regs->r[4];
	case 3: args[2] = regs->r[3];
	case 2: args[1] = regs->r[2];
	case 1:	args[0] = regs->r[1];
	case 0:
		break;
	default:
		BUG();
	}
}

static inline void syscall_set_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 const unsigned long *args)
{
	/* Same note as above applies */
	BUG_ON(i);

	switch (n) {
	case 6: regs->r[7] = args[5];
	case 5: regs->r[5] = args[4];
	case 4: regs->r[4] = args[3];
	case 3: regs->r[3] = args[2];
	case 2: regs->r[2] = args[1];
	case 1: regs->r[1] = args[0];
		break;
	default:
		BUG();
	}
}

#endif /* __ASM_RX_SYSCALL_H__ */
