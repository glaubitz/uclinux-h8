#ifndef __ARCH_RX_CMPXCHG__
#define __ARCH_RX_CMPXCHG__

#define xchg(ptr, x) \
	((__typeof__(*(ptr)))__xchg((unsigned long)(x), (ptr), \
				    sizeof(*(ptr))))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((volatile struct __xchg_dummy *)(x))

extern void __bad_xchg_size(void);

static inline unsigned long __xchg(unsigned long x,
				   volatile void *ptr, int size)
{
	switch (size) {
	case 1:
		__asm__ __volatile__
			("xchg %1.b,%0"
			 : "=&r" (x), "=Q" (*__xg(ptr)));
		break;
	case 2:
		__asm__ __volatile__
			("xchg %1.w,%0"
			 : "=&r" (x), "=Q" (*__xg(ptr)));
		break;
	case 4:
		__asm__ __volatile__
			("xchg %1.l,%0"
			 : "=&r" (x), "=Q" (*__xg(ptr)));
		break;
	default:
		__bad_xchg_size();
	}
	return x;
}

#include <asm-generic/cmpxchg-local.h>

/*
 * cmpxchg_local and cmpxchg64_local are atomic wrt current CPU. Always make
 * them available.
 */
#define cmpxchg_local(ptr, o, n)					 \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr),		 \
						     (unsigned long)(o), \
						     (unsigned long)(n), \
						     sizeof(*(ptr))))
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#ifndef CONFIG_SMP
#include <asm-generic/cmpxchg.h>
#endif

#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

#endif /* __ARCH_RX_CMPXCHG__ */
