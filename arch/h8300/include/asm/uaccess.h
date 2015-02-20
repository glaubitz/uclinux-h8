#ifndef __H8300_UACCESS_H
#define __H8300_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/segment.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1

/* We let the MMU do all checking */
#define access_ok(type, addr, size) __access_ok((unsigned long)addr, size)
static inline int __access_ok(unsigned long addr, unsigned long size)
{
#define	RANGE_CHECK_OK(addr, size, lower, upper) \
	(((addr) >= (lower)) && (((addr) + (size)) < (upper)))

	extern unsigned long  memory_end;

	return RANGE_CHECK_OK(addr, size, 0L, memory_end);
}

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry {
	unsigned long insn, fixup;
};

/* Returns 0 if exception not found and fixup otherwise.  */
extern unsigned long search_exception_table(unsigned long);


/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 */

#define put_user(x, ptr)				\
({							\
	int __pu_err = 0;				\
	typeof(*(ptr)) __pu_val = (x);			\
	switch (sizeof(*(ptr))) {			\
	case 1:						\
	/* falll through */ \
	case 2:						\
	/* fall through */ \
	case 4:						\
		*(ptr) = x;				\
		break;					\
	case 8:						\
		memcpy(ptr, &__pu_val, sizeof(*(ptr))); \
		break;					\
	default:					\
		__pu_err = __put_user_bad();		\
		break;					\
	}						\
	__pu_err;					\
})

#define __put_user(x, ptr) put_user(x, ptr)

extern int __put_user_bad(void);

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */

#define __ptr(x) ((unsigned long *)(x))

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */

#define get_user(x, ptr)					\
({								\
	typeof(*(ptr)) __gu_val;				\
	int __gu_err = 0;					\
	switch (sizeof(*(ptr))) {				\
	case 1:							\
		*(u8 *)&__gu_val = *((u8 *)(ptr));		\
		break;						\
	case 2:							\
		*(u16 *)&__gu_val = *((u16 *)ptr);		\
		break;						\
	case 4:							\
		*(u32 *)&__gu_val = *((u32 *)ptr);		\
		break;						\
	case 8:							\
		memcpy((void *)&__gu_val, ptr, sizeof(*(ptr)));	\
		break;						\
	default:						\
		__gu_err = __get_user_bad();			\
		break;						\
	}							\
	(x) = (typeof(*(ptr)))__gu_val;				\
	__gu_err;						\
})
#define __get_user(x, ptr) get_user(x, ptr)

extern int __get_user_bad(void);

#define copy_from_user(to, from, n)		(memcpy(to, from, n), 0)
#define copy_to_user(to, from, n)		(memcpy(to, from, n), 0)

#define __copy_from_user(to, from, n) copy_from_user(to, from, n)
#define __copy_to_user(to, from, n) copy_to_user(to, from, n)
#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

unsigned long clear_user(void __user *addr, unsigned long size);
#define strnlen_user(s, n) (strnlen(s, n) + 1)
long strncpy_from_user(char *d, const char *s, long n);

#define __clear_user	clear_user

#endif /* _H8300_UACCESS_H */
