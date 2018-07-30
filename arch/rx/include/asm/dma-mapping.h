#ifndef __ASM_RX_DMA_MAPPING_H
#define __ASM_RX_DMA_MAPPING_H

extern struct dma_map_ops *dma_ops;
extern void no_iommu_init(void);

static inline struct dma_map_ops *get_dma_ops(struct device *dev)
{
	return dma_ops;
}

#endif /* __ASM_RX_DMA_MAPPING_H */
