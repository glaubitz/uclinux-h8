#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/audit.h>
#include <linux/tracehook.h>

#include <trace/events/syscalls.h>

long arch_ptrace(struct task_struct *child, long request, 
		 unsigned long addr, unsigned long data) {
	return -ENOSYS;
}

void ptrace_disable(struct task_struct *child)
{
}

asmlinkage long do_syscall_trace_enter(struct pt_regs *regs)
{
	long ret = 0;

	if (test_thread_flag(TIF_SYSCALL_TRACE) &&
	    tracehook_report_syscall_entry(regs))
		/*
		 * Tracing decided this syscall should not happen.
		 * We'll return a bogus call number to get an ENOSYS
		 * error, but leave the original number in regs->r[8].
		 */
		ret = -1L;

	audit_syscall_entry(regs->r[1], regs->r[2], regs->r[3],
			    regs->r[4], regs->r[5]);

	return ret ?: regs->r[1];
}

asmlinkage void syscall_trace_leave(struct pt_regs *regs)
{
	int step;

	audit_syscall_exit(regs);

	step = test_thread_flag(TIF_SINGLESTEP);
	if (step || test_thread_flag(TIF_SYSCALL_TRACE))
		tracehook_report_syscall_exit(regs, step);
}
