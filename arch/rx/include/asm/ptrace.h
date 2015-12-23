#ifndef __ASM_RX_PTRACE_H__
#define __ASM_RX_PTRACE_H__

#ifndef __ASSEMBLY__

/* this struct defines the way the registers are stored on the
   stack during a system call. */

struct pt_regs {
	unsigned long r[16];
	unsigned long usp;
	unsigned long vec;
	unsigned long pc;
	unsigned long psw;
};

/* Find the stack offset for a register, relative to thread.esp0. */
#define PT_REG(reg)	((long)&((struct pt_regs *)0)->reg)

#define user_mode(regs) (((regs)->psw & (1<<20)))
#define instruction_pointer(regs) ((regs)->pc)
#define profile_pc(regs) instruction_pointer(regs)
extern void show_regs(struct pt_regs *);
#endif /* __ASSEMBLY__ */
#define	OFF_R1	(1*4)
#define	OFF_R2	(2*4)
#define	OFF_R3	(3*4)
#define	OFF_R4	(4*4)
#define	OFF_R5	(5*4)
#define	OFF_R7	(7*4)
#define OFF_USP (16*4)
#define OFF_VEC (17*4)
#define OFF_PSW (19*4)
#endif /* __ASM_RX_PTRACE_H__ */
