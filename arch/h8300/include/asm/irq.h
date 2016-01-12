#ifndef _H8300_IRQ_H_
#define _H8300_IRQ_H_

#include <linux/irqchip.h>

#if defined(CONFIG_CPU_H8300H)
#define NR_IRQS 64
#elif defined(CONFIG_CPU_H8S)
#define NR_IRQS 128
#define EXT_IRQ0 16
#define EXT_IRQS 16
#endif

static inline int irq_canonicalize(int irq)
{
	return irq;
}

#endif /* _H8300_IRQ_H_ */
