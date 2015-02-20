/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#undef DEBUG

#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/export.h>

#include <asm/pgalloc.h>

void *dma_alloc_coherent(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t gfp)
{
	void *ret;
	/* ignore region specifiers */
	gfp &= ~(__GFP_DMA | __GFP_HIGHMEM);

	if (dev == NULL || (*dev->dma_mask < 0xffffffff))
		gfp |= GFP_DMA;
	ret = (void *)__get_free_pages(gfp, get_order(size));

	if (ret != NULL) {
		memset(ret, 0, size);
		*dma_handle = virt_to_phys(ret);
	}
	return ret;
}
EXPORT_SYMBOL(dma_alloc_coherent);

void dma_free_coherent(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	free_pages((unsigned long)vaddr, get_order(size));
}
EXPORT_SYMBOL(dma_free_coherent);

void dma_sync_single_for_device(struct device *dev, dma_addr_t handle,
				size_t size, enum dma_data_direction dir)
{
}
EXPORT_SYMBOL(dma_sync_single_for_device);

void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir)
{
	int i;

	for (i = 0; i < nents; sg++, i++)
		dma_sync_single_for_device(dev, sg->dma_address,
					   sg->length, dir);
}
EXPORT_SYMBOL(dma_sync_sg_for_device);

dma_addr_t dma_map_single(struct device *dev, void *addr, size_t size,
			  enum dma_data_direction dir)
{
	dma_addr_t handle = virt_to_bus(addr);

	dma_sync_single_for_device(dev, handle, size, dir);
	return handle;
}
EXPORT_SYMBOL(dma_map_single);

dma_addr_t dma_map_page(struct device *dev, struct page *page,
			unsigned long offset, size_t size,
			enum dma_data_direction dir)
{
	dma_addr_t handle = page_to_phys(page) + offset;

	dma_sync_single_for_device(dev, handle, size, dir);
	return handle;
}
EXPORT_SYMBOL(dma_map_page);

int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
	       enum dma_data_direction dir)
{
	int i;

	for (i = 0; i < nents; sg++, i++) {
		sg->dma_address = sg_phys(sg);
		dma_sync_single_for_device(dev, sg->dma_address,
					   sg->length, dir);
	}
	return nents;
}
EXPORT_SYMBOL(dma_map_sg);
