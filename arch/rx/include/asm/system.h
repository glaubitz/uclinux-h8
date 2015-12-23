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

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((volatile struct __xchg_dummy *)(x))

static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
	unsigned long tmp, flags;

	switch (size) {
	case 1:
		local_irq_save(flags);
		tmp = *(u8 *)__xg(ptr);
		*(u8 *)__xg(ptr) = x;
		local_irq_restore(flags);
		break;
	case 2:
		local_irq_save(flags);
		tmp = *(u16 *)__xg(ptr);
		*(u16 *)__xg(ptr) = x;
		local_irq_restore(flags);
		break;
	case 4:
		__asm__ volatile("xchg %0, %1"
				 :"=m"(__xg(ptr)),"=r"(x));
		break;
	default:
		tmp = 0;	  
	}
	return tmp;
}

#include <asm-generic/cmpxchg-local.h>

/*
 * cmpxchg_local and cmpxchg64_local are atomic wrt current CPU. Always make
 * them available.
 */
#define cmpxchg_local(ptr, o, n)				  	       \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o),\
			(unsigned long)(n), sizeof(*(ptr))))
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#ifndef CONFIG_SMP
#include <asm-generic/cmpxchg.h>
#endif

#define arch_align_stack(x) (x)

#endif /* __ASM_RX_SYSTEM_H__ */
