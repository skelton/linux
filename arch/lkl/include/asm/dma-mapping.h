#ifndef _ASM_LKL_DMA_MAPPING_H
#define _ASM_LKL_DMA_MAPPING_H

extern struct dma_map_ops lkl_dma_ops;
#define get_dma_ops(x) (&lkl_dma_ops)

extern dma_addr_t dma_map_single(struct device *dev, void *cpu_addr,
		size_t size, enum dma_data_direction dir);

extern void dma_unmap_single(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir);

extern int dma_map_sg(struct device *dev, struct scatterlist *sglist,
		int nents, enum dma_data_direction direction);

extern void dma_unmap_sg(struct device *dev, struct scatterlist *sglist,
		int nents, enum dma_data_direction direction);

#include <asm-generic/dma-mapping-common.h>

#endif
