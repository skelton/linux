#include <linux/dma-mapping.h>

static void* lkl_dma_alloc(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp,
		struct dma_attrs *attrs) {
	__WARN();
}

static dma_addr_t lkl_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size,
		enum dma_data_direction dir,
		struct dma_attrs *attrs) {
	__WARN();
}

static int lkl_dma_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir,
		struct dma_attrs *attrs) {
	__WARN();
}

struct dma_map_ops lkl_dma_ops = {
	.alloc = lkl_dma_alloc,
	.map_page = lkl_dma_map_page,
	.map_sg = lkl_dma_map_sg,
	.is_phys = 1,
};

dma_addr_t dma_map_single(struct device *dev, void *cpu_addr,
		size_t size, enum dma_data_direction dir) {
	__WARN();
}

void dma_unmap_single(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir) {
	__WARN();
}

int dma_map_sg(struct device *dev, struct scatterlist *sglist,
		int nents, enum dma_data_direction direction) {
	__WARN();
}

void dma_unmap_sg(struct device *dev, struct scatterlist *sglist,
		int nents, enum dma_data_direction direction) {
	__WARN();
}
