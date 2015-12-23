/*
 * DMA mapping support
 *
 * Copyright (C) 2011 Yoshinori Sato
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/dma-mapping.h>
#include <linux/io.h>

struct dma_map_ops *dma_ops;
EXPORT_SYMBOL(dma_ops);

static void *dma_generic_alloc_coherent(struct device *dev, size_t size,
				 dma_addr_t *dma_handle, gfp_t gfp)
{
	void *ret;
	int order = get_order(size);

	gfp |= __GFP_ZERO;

	ret = (void *)__get_free_pages(gfp, order);
	if (!ret)
		return NULL;

	split_page(pfn_to_page(virt_to_phys(ret) >> PAGE_SHIFT), order);

	*dma_handle = virt_to_phys(ret);

	return ret;
}

static void dma_generic_free_coherent(struct device *dev, size_t size,
			       void *vaddr, dma_addr_t dma_handle)
{
	int order = get_order(size);
	unsigned long pfn = dma_handle >> PAGE_SHIFT;
	int k;

	for (k = 0; k < (1 << order); k++)
		__free_pages(pfn_to_page(pfn + k), 0);

	iounmap(vaddr);
}

static dma_addr_t nommu_dma_map_page(struct device *dev, struct page *page,
				 unsigned long offset, size_t size,
				 enum dma_data_direction dir,
				 struct dma_attrs *attrs)
{
	WARN_ON(size == 0);

	return page_to_phys(page) + offset;
}

static int nommu_dma_map_sg(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir,
			struct dma_attrs *attrs)
{
	struct scatterlist *s;
	int i;

	WARN_ON(nents == 0 || sg[0].length == 0);

	for_each_sg(sg, s, nents, i) {
		BUG_ON(!sg_page(s));

		s->dma_address = sg_phys(s);
		s->dma_length = s->length;
	}

	return nents;
}

struct dma_map_ops rx_dma_ops = {
	.alloc_coherent		= dma_generic_alloc_coherent,
	.free_coherent		= dma_generic_free_coherent,
	.map_page		= nommu_dma_map_page,
	.map_sg			= nommu_dma_map_sg,
	.is_phys		= 1,
};

void __init no_iommu_init(void)
{
	if (dma_ops)
		return;
	dma_ops = &rx_dma_ops;
}
