#ifndef __ASM_RX_SYSCALL_H__
#define __ASM_RX_SYSCALL_H__

#include <uapi/linux/audit.h>

static inline int syscall_get_arch(struct task_struct *task)
{
	return AUDIT_ARCH_RX;
}

#endif /* __ASM_RX_SYSCALL_H__ */
