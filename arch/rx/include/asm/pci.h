#ifndef __ASM_RX_PCI_H__
#define __ASM_RX_PCI_H__

/*
 * pci.h - RX specific PCI declarations.
 *
 * Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#define pcibios_assign_all_busses()	0

static inline void pcibios_set_master(struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	/* We don't do dynamic PCI IRQ allocation */
}

#define PCI_DMA_BUS_IS_PHYS	(1)

#endif /* __ASM_RX_PCI_H__ */
