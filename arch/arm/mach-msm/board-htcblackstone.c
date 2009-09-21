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
#include <linux/gpio_keys.h>


#include <linux/microp-keypad.h>

#include "proc_comm_wince.h"
#include "devices.h"
#include "htc_hw.h"
#include "board-htcblackstone.h"

static int halibut_ffa;
module_param_named(ffa, halibut_ffa, int, S_IRUGO | S_IWUSR | S_IWGRP);

static void blackstone_device_specific_fixes(void);

extern int htcraphael_init_mmc(void);

/*
 * GPIO Keys
 */

static struct gpio_keys_button blackstone_button_table[] = {
        {KEY_POWER,      83,      0, "Power button"},
        {KEY_UP,         39,      0, "Up button"},
        {KEY_DOWN,       40,      0, "Down button"},
};

static struct gpio_keys_platform_data gpio_keys_data = {
        .buttons  = blackstone_button_table,
        .nbuttons = ARRAY_SIZE(blackstone_button_table),
};

static struct platform_device gpio_keys = {
        .name = "gpio-keys",
        .dev  = {
                .platform_data = &gpio_keys_data,
        },
        .id   = -1,
};

//END GPIO keys

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

static int halibut_phy_init_seq_raph100[] = {
	0x40, 0x31, /* Leave this pair out for USB Host Mode */
	0x1D, 0x0D,
	0x1D, 0x10,
	-1
};

static void halibut_phy_reset(void)
{
	gpio_set_value(0x64, 0);
	mdelay(1);
	gpio_set_value(0x64, 1);
	mdelay(3);
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

static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_reset      = halibut_phy_reset,
	.phy_init_seq	= halibut_phy_init_seq_raph100,
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

#define MSM_LOG_BASE 0x0e0000
#define MSM_LOG_SIZE 0x020000

static struct resource ram_console_resource[] = {
        {
                .start = MSM_LOG_BASE,
                .end = MSM_LOG_BASE+MSM_LOG_SIZE-1,
                .flags  = IORESOURCE_MEM,
        }
};
 
static struct platform_device ram_console_device = {
        .name = "ram_console",
        .id = -1,
        .num_resources  = ARRAY_SIZE(ram_console_resource),
        .resource       = ram_console_resource,
};

#define SND(num, desc) { .name = desc, .id = num }
static struct snd_endpoint snd_endpoints_list[] = {
	SND(0, "HANDSET"),
	SND(1, "SPEAKER"),
	SND(2, "HEADSET"),
	SND(3, "BT"),
	SND(44, "BT_EC_OFF"),
	SND(10, "HEADSET_AND_SPEAKER"),
	SND(256, "CURRENT"),

	/* Bluetooth accessories. */

	SND(12, "HTC BH S100"),
	SND(13, "HTC BH M100"),
	SND(14, "Motorola H500"),
	SND(15, "Nokia HS-36W"),
	SND(16, "PLT 510v.D"),
	SND(17, "M2500 by Plantronics"),
	SND(18, "Nokia HDW-3"),
	SND(19, "HBH-608"),
	SND(20, "HBH-DS970"),
	SND(21, "i.Tech BlueBAND"),
	SND(22, "Nokia BH-800"),
	SND(23, "Motorola H700"),
	SND(24, "HTC BH M200"),
	SND(25, "Jabra JX10"),
	SND(26, "320Plantronics"),
	SND(27, "640Plantronics"),
	SND(28, "Jabra BT500"),
	SND(29, "Motorola HT820"),
	SND(30, "HBH-IV840"),
	SND(31, "6XXPlantronics"),
	SND(32, "3XXPlantronics"),
	SND(33, "HBH-PV710"),
	SND(34, "Motorola H670"),
	SND(35, "HBM-300"),
	SND(36, "Nokia BH-208"),
	SND(37, "Samsung WEP410"),
	SND(38, "Jabra BT8010"),
	SND(39, "Motorola S9"),
	SND(40, "Jabra BT620s"),
	SND(41, "Nokia BH-902"),
	SND(42, "HBH-DS220"),
	SND(43, "HBH-DS980"),
};
#undef SND

static struct msm_snd_endpoints blac_snd_endpoints = {
        .endpoints = snd_endpoints_list,
        .num = ARRAY_SIZE(snd_endpoints_list),
};

static struct platform_device blac_snd = {
	.name = "msm_snd",
	.id = -1,
	.dev	= {
		.platform_data = &blac_snd_endpoints,
	},
};

static struct platform_device raphael_gps = {
    .name       = "raphael_gps",
};


static struct platform_device *devices[] __initdata = {
        &ram_console_device,
//#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
//	&msm_serial0_device,
//#endif
	&msm_device_hsusb,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_gpu0_device,
	&android_pmem_gpu1_device,
    &msm_device_smd,
    &msm_device_nand,
    &msm_device_i2c,
	&msm_device_rtc,
	&msm_device_htc_hw,	
    &gpio_keys,
    &blac_snd,
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

static void blac_set_vibrate(uint32_t val)
{
	struct msm_dex_command vibra;

	if (val == 0) {
		vibra.cmd = PCOM_VIBRA_OFF;
		msm_proc_comm_wince(&vibra, 0);
	} else if (val > 0) {
		if (val == 1 || val > 0xb22)
			val = 0xb22;
		writel(val, MSM_SHARED_RAM_BASE + 0xfc130);
		vibra.cmd = PCOM_VIBRA_ON;
		msm_proc_comm_wince(&vibra, 0);
	}
}

static htc_hw_pdata_t msm_htc_hw_pdata = {
	.set_vibrate = blac_set_vibrate,
	.battery_smem_offset = 0xfc140, //XXX: raph800
	.battery_smem_field_size = 4,
};

static void __init halibut_init(void)
{
	int i;

	// Fix data in arrays depending on GSM/CDMA version
	blackstone_device_specific_fixes();

    msm_acpu_clock_init(&halibut_clock_data);
	msm_proc_comm_wince_init();

    msm_hw_reset_hook = htcraphael_reset;

	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	msm_device_htc_hw.dev.platform_data = &msm_htc_hw_pdata;
	
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
		blac_set_vibrate(1);
		mdelay(150);
		blac_set_vibrate(0);
		mdelay(75);
	}
}

static void __init halibut_map_io(void)
{
	msm_map_common_io();
	msm_clock_init();
}

static void __init blac_fixup(struct machine_desc *desc, struct tag *tags,
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

static void blackstone_device_specific_fixes(void)
{
		msm_hsusb_pdata.phy_init_seq = halibut_phy_init_seq_raph100;
		msm_htc_hw_pdata.battery_smem_offset = 0xfc110;
		msm_htc_hw_pdata.battery_smem_field_size = 2;
}

MACHINE_START(HTCBLACKSTONE, "HTC blackstone cellphone (aka HTC Touch HD)")
	.fixup 		= blac_fixup,
	.boot_params	= 0x10000100,
	.map_io		= halibut_map_io,
	.init_irq	= halibut_init_irq,
	.init_machine	= halibut_init,
	.timer		= &msm_timer,
MACHINE_END
