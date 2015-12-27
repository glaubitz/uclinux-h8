#ifndef __ASM_RX_SWITCH_TO_H__
#define __ASM_RX_SWITCH_TO_H__

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

#endif
