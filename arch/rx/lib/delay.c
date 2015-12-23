#include <linux/delay.h>

void __delay(unsigned long loops)
{
	if (loops == 0)
		return;
	__asm__ volatile("1: sub #1,%0\n\t"
			 "bne 1b"
			 :"=r"(loops)
			 :"0"(loops));
}

void __const_udelay(unsigned long xloops)
{
	__asm__("mov.l %1,r2\n\t"
		"emul %2,r2\n\t"
		"mov.l r3,%0"
		:"=r"(xloops)
		:"0"(xloops),"r"(loops_per_jiffy)
		:"r2","r3");
	__delay(xloops);
}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x000010c6);  /* 2**32 / 1000000 */
}

void __ndelay(unsigned long nsecs)
{
	__const_udelay(nsecs * 0x00000005);
}
