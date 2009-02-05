/* linux/arch/arm/mach-msm/board-trout.h
** Author: Brian Swetland <swetland@google.com>
*/
#ifndef __ARCH_ARM_MACH_MSM_BOARD_HTCRAPHAEL_H
#define __ARCH_ARM_MACH_MSM_BOARD_HTCRAPHAEL_H

#include <mach/board.h>


#define MSM_SMI_BASE            0x00000000
#define MSM_SMI_SIZE            0x900000

#define MSM_EBI_BASE            0x10000000
#define MSM_EBI_SIZE            0x6e00000

#define MSM_PMEM_GPU0_BASE      0x0
#define MSM_PMEM_GPU0_SIZE      0x800000

#define MSM_LINUX_BASE          MSM_EBI_BASE
#define MSM_LINUX_SIZE          0x4c00000

#define MSM_PMEM_MDP_BASE       MSM_LINUX_BASE + MSM_LINUX_SIZE
#define MSM_PMEM_MDP_SIZE       0x800000

#define MSM_PMEM_ADSP_BASE      MSM_PMEM_MDP_BASE + MSM_PMEM_MDP_SIZE
#define MSM_PMEM_ADSP_SIZE      0x800000

#define MSM_PMEM_GPU1_BASE      MSM_PMEM_ADSP_BASE + MSM_PMEM_ADSP_SIZE
#define MSM_PMEM_GPU1_SIZE      0x800000

#define MSM_FB_BASE             MSM_PMEM_GPU1_BASE + MSM_PMEM_GPU1_SIZE
#define MSM_FB_SIZE             0x200000

#define DECLARE_MSM_IOMAP
#include <mach/msm_iomap.h>

#endif /* GUARD */
