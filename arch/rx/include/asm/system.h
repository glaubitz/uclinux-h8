#ifndef __ASM_RX_SYSTEM_H__
#define __ASM_RX_SYSTEM_H__

#include <linux/types.h>
#include <linux/linkage.h>
#include <linux/irqflags.h>

#define switch_to(prev,next,last)			\
do {							\
__asm__ volatile("switch_to:");				\
	__asm__ volatile("pushm r1-r15\n\t"		\
			 "mov.l #1f,%0\n\t"		\
			 "mov.l r0,%1\n\t"		\
			 "mov.l %3,r0\n\t"		\
			 "jmp %2\n"			\
			 "1:\n\t"			\
			 "popm r1-r15\n\t"		\
			 ::"m"(prev->thread.pc),	\
				 "m"(prev->thread.sp),	\
				 "r"(next->thread.pc),	\
				 "g"(next->thread.sp));	\
	last = prev;					\
} while(0)


#define nop()  asm volatile ("nop"::)
#define mb()   asm volatile (""   : : :"memory")
#define rmb()  asm volatile (""   : : :"memory")
#define wmb()  asm volatile (""   : : :"memory")
#define set_mb(var, value) do { var = value; } while (0)

#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()	do { } while(0)

#define arch_align_stack(x) (x)

#endif /* __ASM_RX_SYSTEM_H__ */
