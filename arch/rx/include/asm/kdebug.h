#ifndef __ASM_RX_KDEBUG_H__
#define __ASM_RX_KDEBUG_H__

#include <asm/ptrace.h>

enum die_val {
	DIE_NONE,
	DIE_TRAP,
	DIE_NMI,
};
extern void die(const char *err, struct pt_regs *regs);

#endif
