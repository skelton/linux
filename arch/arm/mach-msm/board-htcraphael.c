/* linux/arch/arm/mach-msm/board-htcraphael.c
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
#include <linux/mm.h>

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

#include <linux/microp-keypad.h>

#include "proc_comm_wince.h"
#include "devices.h"
#include "board-htcraphael.h"

static int halibut_ffa;
module_param_named(ffa, halibut_ffa, int, S_IRUGO | S_IWUSR | S_IWGRP);

static void htcraphael_device_specific_fixes(void);

extern int htcraphael_init_mmc(void);

static struct resource raphael_keypad_resources[] = {
	{ 
		.start = MSM_GPIO_TO_INT(27), /* Modified in htcraphael_device_specific_fixes() */
		.end = MSM_GPIO_TO_INT(27), /* Modified in htcraphael_device_specific_fixes() */
		.flags = IORESOURCE_IRQ,
	},
};

static struct microp_keypad_platform_data raphael_keypad_data = {
	.clamshell = { 
		.gpio = 39, /* Modified in htcraphael_device_specific_fixes() */
		.irq = MSM_GPIO_TO_INT(39), /* Modified in htcraphael_device_specific_fixes() */
	},
	.backlight_gpio = 86, /* Modified in htcraphael_device_specific_fixes() */
};

static struct platform_device raphael_keypad_device = {
	.name = "microp-keypad",
	.id = 0,
	.num_resources = ARRAY_SIZE(raphael_keypad_resources),
	.resource = raphael_keypad_resources,
	.dev = { .platform_data = &raphael_keypad_data, },
};


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
        "ether",
//	"diag",
//	"adb",
};

static struct msm_hsusb_product halibut_usb_products[] = {
	/* Use product_id 0x505a always, as we moved ether to the top of the list */
	{
		.product_id = 0x505a,
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
		// Keyboard controller
		I2C_BOARD_INFO("microp-ksc", 0x67),
	},
	{		
		I2C_BOARD_INFO("mt9t013", 0x6c>>1),
		/* .irq = TROUT_GPIO_TO_INT(TROUT_GPIO_CAM_BTN_STEP1_N), */
	},
	{
		// Raphael NaviPad
		I2C_BOARD_INFO("raph_navi_pad", 0x62),
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
	&raphael_keypad_device,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_gpu0_device,
	&android_pmem_gpu1_device,
        &msm_device_smd,
        &msm_device_nand,
        &msm_device_i2c,
	&msm_device_rtc,
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
	struct msm_dex_command dex = { .cmd = PCOM_RESET_ARM9 };
	msm_proc_comm_wince(&dex, 0);
	msleep(0x15e);
	gpio_configure(25, GPIOF_OWNER_ARM11);
	gpio_direction_output(25, 0);
	printk(KERN_INFO "%s: Soft reset done.\n", __func__);
}

static void __init halibut_init(void)
{
	int i;
	struct msm_dex_command vibra = { .cmd = 0, };

	// Fix data in arrays depending on GSM/CDMA version
	htcraphael_device_specific_fixes();

        msm_acpu_clock_init(&halibut_clock_data);
	msm_proc_comm_wince_init();

        msm_hw_reset_hook = htcraphael_reset;

	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata,

	platform_add_devices(devices, ARRAY_SIZE(devices));
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));
	htcraphael_init_mmc();

	/* TODO: detect vbus and correctly notify USB about its presence 
	 * For now we just declare that VBUS is present at boot and USB
	 * copes, but this is not ideal.
	 */
	msm_hsusb_set_vbus_state(1);

	/* A little vibrating welcome */
	for (i=0; i<2; i++) {
		vibra.cmd = PCOM_VIBRA_ON;
		msm_proc_comm_wince(&vibra, 0);
		mdelay(150);
		vibra.cmd = PCOM_VIBRA_OFF;
		msm_proc_comm_wince(&vibra, 0);
		mdelay(75);
	}
}

static void __init halibut_map_io(void)
{
	msm_map_common_io();
	msm_clock_init();
}

static void __init htcraphael_fixup(struct machine_desc *desc, struct tag *tags,
                                    char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = PAGE_ALIGN(PHYS_OFFSET);
	mi->bank[0].node = PHYS_TO_NID(mi->bank[0].start);
	mi->bank[0].size = (89 * 1024 * 1024); // Why 89? See board-htcraphael.h
#if 0
	/* TODO: detect whether a 2nd memory bank is actually present, not all devices have it */
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

static void htcraphael_device_specific_fixes(void)
{
	if (machine_is_htcraphael()) {
		raphael_keypad_resources[0].start = MSM_GPIO_TO_INT(27);
		raphael_keypad_resources[0].end = MSM_GPIO_TO_INT(27);
		raphael_keypad_data.clamshell.gpio = 38;
		raphael_keypad_data.clamshell.irq = MSM_GPIO_TO_INT(38);
		raphael_keypad_data.backlight_gpio = 86;
	}
	if (machine_is_htcraphael_cdma()) {
		raphael_keypad_resources[0].start = MSM_GPIO_TO_INT(27);
		raphael_keypad_resources[0].end = MSM_GPIO_TO_INT(27);
		raphael_keypad_data.clamshell.gpio = 39;
		raphael_keypad_data.clamshell.irq = MSM_GPIO_TO_INT(39);
		raphael_keypad_data.backlight_gpio = 86;
	}
}

MACHINE_START(HTCRAPHAEL, "HTC Raphael GSM phone (aka HTC Touch Pro)")
	.fixup 		= htcraphael_fixup,
	.boot_params	= 0x10000100,
	.map_io		= halibut_map_io,
	.init_irq	= halibut_init_irq,
	.init_machine	= halibut_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(HTCRAPHAEL_CDMA, "HTC Raphael CDMA phone (aka HTC Touch Pro)")
	.fixup 		= htcraphael_fixup,
	.boot_params	= 0x10000100,
	.map_io		= halibut_map_io,
	.init_irq	= halibut_init_irq,
	.init_machine	= halibut_init,
	.timer		= &msm_timer,
MACHINE_END
