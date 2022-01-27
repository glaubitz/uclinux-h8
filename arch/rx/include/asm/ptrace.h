#ifndef __ASM_RX_PTRACE_H__
#define __ASM_RX_PTRACE_H__

#include <uapi/asm/ptrace.h>

#define task_pt_regs(tsk)						\
	((struct pt_regs *)(task_stack_page(tsk) + THREAD_SIZE) - 1)

#ifndef __ASSEMBLY__
static inline unsigned long user_stack_pointer(struct pt_regs *regs)
{
        return regs->r[0];
}

static inline unsigned long instruction_pointer(struct pt_regs *regs)
{
        return regs->pc;
}

#endif
#endif
