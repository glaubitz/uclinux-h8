/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _H8300_IO_H
#define _H8300_IO_H

#ifdef __KERNEL__

#include <linux/types.h>

/* H8/300 internal I/O functions */
static inline void ctrl_bclr(int b, void __iomem *addr)
{
	if (__builtin_constant_p(b))
		__asm__("bclr %1,%0" : "+WU"(*(u8 *)addr): "i"(b));
	else
		__asm__("bclr %w1,%0" : "+WU"(*(u8 *)addr): "r"(b));
}

static inline void ctrl_bset(int b, void __iomem *addr)
{
	if (__builtin_constant_p(b))
		__asm__("bset %1,%0" : "+WU"(*(u8 *)addr): "i"(b));
	else
		__asm__("bset %w1,%0" : "+WU"(*(u8 *)addr): "r"(b));
}

#include <asm-generic/io.h>

#endif /* __KERNEL__ */

#endif /* _H8300_IO_H */
