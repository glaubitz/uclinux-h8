#ifndef __ASM_RX_PTRACE_H__
#define __ASM_RX_PTRACE_H__

#include <uapi/asm/ptrace.h>

#define task_pt_regs(tsk)						\
	((struct pt_regs *)(task_stack_page(tsk) + THREAD_SIZE) - 1)

#endif
