#ifndef _H8300_IO_H
#define _H8300_IO_H

#ifdef __KERNEL__

#include <linux/types.h>

#define __raw_readb(addr) ({ u8 __v = *(volatile u8 *)(addr); __v; })

#define __raw_readw(addr) ({ u16 __v = *(volatile u16 *)(addr); __v; })

#define __raw_readl(addr) ({ u32 __v = *(volatile u32 *)(addr); __v; })

#define __raw_writeb(b, addr) (void)((*(volatile u8 *)(addr)) = (b))

#define __raw_writew(b, addr) (void)((*(volatile u16 *)(addr)) = (b))

#define __raw_writel(b, addr) (void)((*(volatile u32 *)(addr)) = (b))

#define readb __raw_readb
#define readw __raw_readw
#define readl __raw_readl
#define writeb __raw_writeb
#define writew __raw_writew
#define writel __raw_writel

#if defined(CONFIG_H83069)
#define ABWCR  0xFEE020
#elif defined(CONFIG_H8S2678)
#define ABWCR  0xFFFEC0
#endif

#ifdef CONFIG_H8300_BUBSSWAP
#define _swapw(x) __builtin_bswap16(x)
#define _swapl(x) __builtin_bswap32(x)
#else
#define _swapw(x) (x)
#define _swapl(x) (x)
#endif

static inline int h8300_buswidth(unsigned int addr)
{
	return (*(volatile u8 *)ABWCR & (1 << ((addr >> 21) & 7))) == 0;
}

static inline void io_outsb(unsigned int addr, const void *buf, int len)
{
	volatile unsigned char  *ap_b = (volatile unsigned char *) addr;
	volatile unsigned short *ap_w = (volatile unsigned short *) addr;
	unsigned char *bp = (unsigned char *) buf;

	if (h8300_buswidth(addr) && (addr & 1)) {
		while (len--)
			*ap_w = *bp++;
	} else {
		while (len--)
			*ap_b = *bp++;
	}
}

static inline void io_outsw(unsigned int addr, const void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;

	while (len--)
		*ap = _swapw(*bp++);
}

static inline void io_outsl(unsigned int addr, const void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;

	while (len--) {
		*(ap + 1) = _swapw(*(bp + 0));
		*(ap + 0) = _swapw(*(bp + 1));
		bp += 2;
	}
}

static inline void io_outsw_noswap(unsigned int addr, const void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;

	while (len--)
		*ap = *bp++;
}

static inline void io_outsl_noswap(unsigned int addr, const void *buf, int len)
{
	volatile unsigned long *ap = (volatile unsigned long *) addr;
	unsigned long *bp = (unsigned long *) buf;

	while (len--)
		*ap = *bp++;
}

static inline void io_insb(unsigned int addr, void *buf, int len)
{
	volatile unsigned char  *ap_b;
	volatile unsigned short *ap_w;
	unsigned char *bp = (unsigned char *) buf;

	if (h8300_buswidth(addr)) {
		ap_w = (volatile unsigned short *)(addr & ~1);
		while (len--)
			*bp++ = *ap_w & 0xff;
	} else {
		ap_b = (volatile unsigned char *)addr;
		while (len--)
			*bp++ = *ap_b;
	}
}

static inline void io_insw(unsigned int addr, void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;

	while (len--)
		*bp++ = _swapw(*ap);
}

static inline void io_insl(unsigned int addr, void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;

	while (len--) {
		*(bp + 0) = _swapw(*(ap + 1));
		*(bp + 1) = _swapw(*(ap + 0));
		bp += 2;
	}
}

static inline void io_insw_noswap(unsigned int addr, void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;

	while (len--)
		*bp++ = *ap;
}

static inline void io_insl_noswap(unsigned int addr, void *buf, int len)
{
	volatile unsigned long *ap = (volatile unsigned long *) addr;
	unsigned long *bp = (unsigned long *) buf;

	while (len--)
		*bp++ = *ap;
}

/*
 *	make the short names macros so specific devices
 *	can override them as required
 */

#define memset_io(a, b, c)	memset((void *)(a), (b), (c))
#define memcpy_fromio(a, b, c)	memcpy((a), (void *)(b), (c))
#define memcpy_toio(a, b, c)	memcpy((void *)(a), (b), (c))

#define mmiowb()

#define inb(addr)    ((h8300_buswidth(addr)) ? \
		      __raw_readw((addr) & ~1) & 0xff:__raw_readb(addr))
#define inw(addr)    _swapw(__raw_readw(addr))
#define inl(addr)    (_swapw(__raw_readw(addr) << 16 | \
			     _swapw(__raw_readw(addr + 2))))
#define outb(x, addr) ((void)((h8300_buswidth(addr) && ((addr) & 1)) ? \
			      __raw_writeb(x, (addr) & ~1) : \
			      __raw_writeb(x, addr)))
#define outw(x, addr) ((void) __raw_writew(_swapw(x), addr))
#define outl(x, addr) \
		((void) __raw_writel(_swapw(x & 0xffff) | \
				      _swapw(x >> 16) << 16, addr))

#define inb_p(addr)    inb(addr)
#define inw_p(addr)    inw(addr)
#define inl_p(addr)    inl(addr)
#define outb_p(x, addr) outb(x, addr)
#define outw_p(x, addr) outw(x, addr)
#define outl_p(x, addr) outl(x, addr)

#define outsb(a, b, l) io_outsb(a, b, l)
#define outsw(a, b, l) io_outsw(a, b, l)
#define outsl(a, b, l) io_outsl(a, b, l)

#define insb(a, b, l) io_insb(a, b, l)
#define insw(a, b, l) io_insw(a, b, l)
#define insl(a, b, l) io_insl(a, b, l)

#define ioread8(a)		__raw_readb(a)
#define ioread16(a)		__raw_readw(a)
#define ioread32(a)		__raw_readl(a)

#define iowrite8(v, a)		__raw_writeb((v), (a))
#define iowrite16(v, a)		__raw_writew((v), (a))
#define iowrite32(v, a)		__raw_writel((v), (a))

#define ioread8_rep(p, d, c)	insb(p, d, c)
#define ioread16_rep(p, d, c)	insw(p, d, c)
#define ioread32_rep(p, d, c)	insl(p, d, c)
#define iowrite8_rep(p, s, c)	outsb(p, s, c)
#define iowrite16_rep(p, s, c)	outsw(p, s, c)
#define iowrite32_rep(p, s, c)	outsl(p, s, c)

#define IO_SPACE_LIMIT 0xffffff

/* Values for nocacheflag and cmode */
#define IOMAP_FULL_CACHING		0
#define IOMAP_NOCACHE_SER		1
#define IOMAP_NOCACHE_NONSER		2
#define IOMAP_WRITETHROUGH		3

extern void *__ioremap(unsigned long physaddr, unsigned long size,
		       int cacheflag);
extern void __iounmap(void *addr, unsigned long size);

static inline void *ioremap(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_NOCACHE_SER);
}
static inline void *ioremap_nocache(unsigned long physaddr,
				    unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_NOCACHE_SER);
}
static inline void *ioremap_writethrough(unsigned long physaddr,
					 unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_WRITETHROUGH);
}
static inline void *ioremap_fullcache(unsigned long physaddr,
				      unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_FULL_CACHING);
}

extern void iounmap(void *addr);

#define ioremap_wc ioremap_nocache

/* H8/300 internal I/O functions */
static inline unsigned char ctrl_inb(unsigned long addr)
{
	return *(volatile unsigned char *)addr;
}

static inline unsigned short ctrl_inw(unsigned long addr)
{
	return *(volatile unsigned short *)addr;
}

static inline unsigned long ctrl_inl(unsigned long addr)
{
	return *(volatile unsigned long *)addr;
}

static inline void ctrl_outb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char *)addr = b;
}

static inline void ctrl_outw(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short *)addr = b;
}

static inline void ctrl_outl(unsigned long b, unsigned long addr)
{
	*(volatile unsigned long *)addr = b;
}

static inline void ctrl_bclr(int b, unsigned long addr)
{
	if (__builtin_constant_p(b))
		__asm__("bclr %1,%0" : : "WU"(addr), "i"(b));
	else
		__asm__("bclr %w1,%0" : : "WU"(addr), "r"(b));
}

static inline void ctrl_bset(int b, unsigned long addr)
{
	if (__builtin_constant_p(b))
		__asm__("bset %1,%0" : : "WU"(addr), "i"(b));
	else
		__asm__("bset %w1,%0" : : "WU"(addr), "r"(b));
}

/*
 * Macros used for converting between virtual and physical mappings.
 */
#define phys_to_virt(vaddr)	((void *) (vaddr))
#define virt_to_phys(vaddr)	((unsigned long) (vaddr))

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	(p)

#endif /* __KERNEL__ */

#endif /* _H8300_IO_H */
