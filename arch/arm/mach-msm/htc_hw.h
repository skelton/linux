#ifndef _ARCH_ARM_MACH_MSM_HW_H
#define _ARCH_ARM_MACH_MSM_HW_H

struct htc_hw_platform_data {
	void (*set_vibrate)(uint32_t);
	uint32_t battery_smem_offset;
	unsigned battery_smem_field_size:3; // 1..4

};

typedef struct htc_hw_platform_data htc_hw_pdata_t;

#endif
