#ifndef __ASM_RX_IRQ_H__
#define __ASM_RX_IRQ_H__

#define NR_IRQS 256
#define EXT_IRQ0 64
#define EXT_IRQS 16
#define RX_MIN_IRQ 21

static __inline__ int irq_canonicalize(int irq)
{
	return irq;
}

void rx_force_interrupt(unsigned int no);

#endif /* __ASM_RX_IRQ_H__ */
