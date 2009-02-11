/* linux/arch/arm/mach-msm/board-htcraphael.h
 */
#ifndef __ARCH_ARM_MACH_MSM_BOARD_HTCRAPHAEL_H
#define __ARCH_ARM_MACH_MSM_BOARD_HTCRAPHAEL_H

#include <mach/board.h>

#define MSM_SMI_BASE            0x00000000
#define MSM_SMI_SIZE            0x900000 /* 9mb */

#define MSM_EBI_BASE            0x10000000
#define MSM_EBI_SIZE            0x8000000 /* 128mb */

/* Define the SMI layout */
#define MSM_SMI_READONLY_BASE	MSM_SMI_BASE
#define MSM_SMI_READONLY_SIZE	0x100000 /* 1mb */

#define MSM_PMEM_GPU0_BASE      MSM_SMI_READONLY_BASE + MSM_SMI_READONLY_SIZE
#define MSM_PMEM_GPU0_SIZE      0x800000 /* 8mb */

/* Define the EBI layout */
#define MSM_LINUX_BASE          MSM_EBI_BASE
#define MSM_LINUX_SIZE          0x5900000 /* 89mb */

#define MSM_PMEM_MDP_BASE       MSM_LINUX_BASE + MSM_LINUX_SIZE
#define MSM_PMEM_MDP_SIZE       0x800000 /* 8mb */

#define MSM_PMEM_ADSP_BASE      MSM_PMEM_MDP_BASE + MSM_PMEM_MDP_SIZE
#define MSM_PMEM_ADSP_SIZE      0x800000 /* 8mb */

#define MSM_PMEM_GPU1_BASE      MSM_PMEM_ADSP_BASE + MSM_PMEM_ADSP_SIZE
#define MSM_PMEM_GPU1_SIZE      0x800000 /* 8mb */

#define MSM_FB_BASE		MSM_PMEM_GPU1_BASE + MSM_PMEM_GPU1_SIZE
#define MSM_FB_SIZE		0x200000 /* 2mb */

#define MSM_EBI_LOCKED_BASE	MSM_FB_BASE + MSM_FB_SIZE
#define MSM_EBI_LOCKED_SIZE	0xD00000 /* 13mb */

#define DECLARE_MSM_IOMAP
#include <mach/msm_iomap.h>

#endif /* GUARD */


