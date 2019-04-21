#ifndef _RX_IRQFLAGS_H_
#define _RX_IRQFLAGS_H_

static inline unsigned long arch_local_save_flags(void)
{
	unsigned long iflag;
	asm volatile ("mvfc psw, %0\n\t"
		      : "=r"(iflag));
	return iflag;
}

static inline void arch_local_irq_disable(void)
{
	asm volatile ("clrpsw i");
}

static inline void arch_local_irq_enable(void)
{
	asm volatile ("setpsw i");
}

static inline unsigned long arch_local_irq_save(void)
{
	unsigned long iflag;
	iflag = arch_local_save_flags();
	arch_local_irq_disable();
	return iflag;
}

static inline void arch_local_irq_restore(unsigned long iflag)
{
	asm volatile("btst #16, %0\n\t"
		     "mvfc psw, %0\n\t"
		     "bmc  #16, %0\n\t"
		     "mvtc %0, psw"
		     ::"r"(iflag):"cc");
}
		
static inline int arch_irqs_disabled_flags(unsigned long psw)
{
	return (psw & (1 << 16)) == 0;
}

static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}

#endif
