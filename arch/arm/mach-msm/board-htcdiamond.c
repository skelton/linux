/* linux/arch/arm/mach-msm/board-htcdiamond.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>,
 * Octavian Voicu, Martijn Stolk
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/android_pmem.h>

#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/mach/mmc.h>
#include <asm/setup.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include <mach/msm_fb.h>
#include <mach/msm_hsusb.h>
#include <mach/vreg.h>

#include <mach/gpio.h>
#include <mach/io.h>
#include <linux/delay.h>

#include "proc_comm_wince.h"
#include "devices.h"
#include "board-htcraphael.h"

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

static int halibut_ffa;
module_param_named(ffa, halibut_ffa, int, S_IRUGO | S_IWUSR | S_IWGRP);

static void htcdiamond_device_specific_fixes(void);

extern void htcraphael_init_keypad(void);
extern int msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat);

static struct resource msm_serial0_resources[] = {
	{
		.start	= INT_UART1,
		.end	= INT_UART1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART1_PHYS,
		.end	= MSM_UART1_PHYS + MSM_UART1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device msm_serial0_device = {
	.name	= "msm_serial",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_serial0_resources),
	.resource	= msm_serial0_resources,
};

static int halibut_phy_init_seq[] = { 0x40, 0x31, 0x1D, 0x0D, 0x1D, 0x10, -1 };

static void halibut_phy_reset(void)
{
	//XXX: find out how to reset usb
	// gpio_set_value(TROUT_GPIO_USB_PHY_RST_N, 0);
        // mdelay(10);
        // gpio_set_value(TROUT_GPIO_USB_PHY_RST_N, 1);
	mdelay(10);
}

static char *halibut_usb_functions[] = {
	"diag",
        "ether", // netripper
//	"adb",
};

static struct msm_hsusb_product halibut_usb_products[] = {
	{
		.product_id = 0x0001, // when ether is disabled
		.functions = 0x01,
	},
	{
		.product_id = 0x505a,
		.functions = 0x02,
	},
	{
		.product_id = 0x505a,
		.functions = 0x03,
	},
};

// netripper
// orig vendor_id 0x18d1
// orig product_id 0xd00d
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_reset      = halibut_phy_reset,
	.phy_init_seq	= halibut_phy_init_seq,
	.vendor_id      = 0x049F,
	.product_id     = 0x0002, // by default (no funcs)
	.version        = 0x0100,
	.product_name   = "MSM USB",
	.manufacturer_name = "HTC",
	.functions	= halibut_usb_functions,
	.num_functions	= ARRAY_SIZE(halibut_usb_functions),
	.products = halibut_usb_products,
	.num_products = ARRAY_SIZE(halibut_usb_products),
};

static struct i2c_board_info i2c_devices[] = {
#if 0
	{
		// Navi cap sense controller
		I2C_BOARD_INFO("cy8c20434", 0x62),
	},
#endif
	{
		// LED & Backlight controller
		I2C_BOARD_INFO("microp-klt", 0x66),
	},
	{		
		I2C_BOARD_INFO("mt9t013", 0x6c>>1),
		/* .irq = TROUT_GPIO_TO_INT(TROUT_GPIO_CAM_BTN_STEP1_N), */
	},	
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.start = MSM_PMEM_MDP_BASE,
	.size = MSM_PMEM_MDP_SIZE,
	.no_allocator = 1,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.start = MSM_PMEM_ADSP_BASE,
	.size = MSM_PMEM_ADSP_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_gpu0_pdata = {
        .name = "pmem_gpu0",
        .start = MSM_PMEM_GPU0_BASE,
        .size = MSM_PMEM_GPU0_SIZE,
        .no_allocator = 1,
        .cached = 0,
};

static struct android_pmem_platform_data android_pmem_gpu1_pdata = {
        .name = "pmem_gpu1",
        .start = MSM_PMEM_GPU1_BASE,
        .size = MSM_PMEM_GPU1_SIZE,
        .no_allocator = 1,
        .cached = 0,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_gpu0_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_gpu0_pdata },
};

static struct platform_device android_pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 3,
	.dev = { .platform_data = &android_pmem_gpu1_pdata },
};

static struct platform_device *devices[] __initdata = {
#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_serial0_device,
#endif
	&msm_device_hsusb,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_gpu0_device,
	&android_pmem_gpu1_device,
        &msm_device_smd,
        &msm_device_nand,
        &msm_device_i2c,
};

static unsigned int diamond_sdcc_slot_status(struct device *dev)
{
	/*
	 * The Diamond has a MoviNAND using mmc interface connected
	 * to SDC2. It is hardwired, thus we always return 1.
	 */
	return 1;
}

static struct mmc_platform_data halibut_sdcc_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.status		= diamond_sdcc_slot_status,
};

extern struct sys_timer msm_timer;

static void __init halibut_init_irq(void)
{
	msm_init_irq();
}

static struct msm_acpu_clock_platform_data halibut_clock_data = {
	.acpu_switch_time_us = 50,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.power_collapse_khz = 19200000,
	.wait_for_irq_khz = 128000000,
};

void msm_serial_debug_init(unsigned int base, int irq, 
			   const char *clkname, int signal_irq);

static void htcraphael_reset(void)
{
        gpio_set_value(25, 0);
}

static void __init halibut_init_mmc(void)
{
	struct vreg *vreg_mmc;
	int rc;

	vreg_mmc = vreg_get(0, "gp6");
	rc = vreg_enable(vreg_mmc);
	if (rc)
		printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc);

	// // Diamond's MoviNAND can be found on SDC2 for GSM and SDC3 for CDMA version
	if (machine_is_htcdiamond()) {
		msm_add_sdcc(2, &halibut_sdcc_data);
	}
	else if (machine_is_htcdiamond_cdma()) {
		msm_add_sdcc(3, &halibut_sdcc_data);
	}
}

static void __init halibut_init(void)
{
	// Fix data in arrays depending on GSM/CDMA version
	htcdiamond_device_specific_fixes();

        msm_acpu_clock_init(&halibut_clock_data);
	msm_proc_comm_wince_init();

        msm_hw_reset_hook = htcraphael_reset;

	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata,

	platform_add_devices(devices, ARRAY_SIZE(devices));
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));
	halibut_init_mmc();
	htcraphael_init_keypad();

	/* TODO: detect vbus and correctly notify USB about its presence 
	 * For now we just declare that VBUS is present at boot and USB
	 * copes, but this is not ideal.
	 */
	msm_hsusb_set_vbus_state(1);

	/* A little vibrating welcome */
	msm_proc_comm_wince(PCOM_PMIC_VIBRA_ON, 0, 0);
	mdelay(150);
	msm_proc_comm_wince(PCOM_PMIC_VIBRA_OFF, 0, 0);
	mdelay(75);
	msm_proc_comm_wince(PCOM_PMIC_VIBRA_ON, 0, 0);
	mdelay(150);
	msm_proc_comm_wince(PCOM_PMIC_VIBRA_OFF, 0, 0);
}

static void __init halibut_map_io(void)
{
	msm_map_common_io();
	msm_clock_init();
}

#if 0
static void __init htcraphael_fixup(struct machine_desc *desc, struct tag *tags,
                                    char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = PAGE_ALIGN(PHYS_OFFSET);
	mi->bank[0].node = PHYS_TO_NID(mi->bank[0].start);
	mi->bank[0].size = (94 * 1024 * 1024);
#if 1
	mi->nr_banks++;
	mi->bank[1].start = PAGE_ALIGN(PHYS_OFFSET + 0x10000000);
	mi->bank[1].node = PHYS_TO_NID(mi->bank[1].start);
	mi->bank[1].size = (128 * 1024 * 1024);
#endif
	printk(KERN_INFO "fixup: nr_banks = %d\n", mi->nr_banks);
	printk(KERN_INFO "fixup: bank0 start=%08lx, node=%08x, size=%08lx\n", mi->bank[0].start, mi->bank[0].node, mi->bank[0].size);
	if (mi->nr_banks > 1)
		printk(KERN_INFO "fixup: bank1 start=%08lx, node=%08x, size=%08lx\n", mi->bank[1].start, mi->bank[1].node, mi->bank[1].size);
}
#endif

/* Already implemented for the Raphael board, to facilitate differences between GSM/CDMA */
static void htcdiamond_device_specific_fixes(void)
{
	if (machine_is_htcdiamond()) {
	}
	if (machine_is_htcdiamond_cdma()) {
	}
}

MACHINE_START(HTCDIAMOND, "HTC Diamond GSM phone (aka HTC Touch Diamond)")
//	.fixup 		= htcraphael_fixup,
	.boot_params	= 0x10000100,
	.map_io		= halibut_map_io,
	.init_irq	= halibut_init_irq,
	.init_machine	= halibut_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(HTCDIAMOND_CDMA, "HTC Diamond CDMA phone (aka HTC Touch Diamond)")
//	.fixup 		= htcraphael_fixup,
	.boot_params	= 0x10000100,
	.map_io		= halibut_map_io,
	.init_irq	= halibut_init_irq,
	.init_machine	= halibut_init,
	.timer		= &msm_timer,
MACHINE_END
