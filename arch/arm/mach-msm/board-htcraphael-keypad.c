/* linux/arch/arm/mach-msm/board-htcraphael-keypad.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>,
 * Job Bolle
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

/*
 * Jobo:
 * This file is a copy of board-halibut-keypad.c
 * modified to use the gpi matrix layout of the HTC Raphael (Touch Pro)
 * HTC Diamond phones and the keyboard slider switch of the Raphael.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>

#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>

#include <linux/gpio_event.h>

static unsigned int htcraphael_row_gpios[] = {	32, 33, 34, 35 };
static unsigned int htcraphael_col_gpios[] = { 40, 41, 42 };

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(htcraphael_col_gpios) + (col))

static const unsigned short htcraphael_keymap[ARRAY_SIZE(htcraphael_col_gpios) * ARRAY_SIZE(htcraphael_row_gpios)] = {
//	[KEYMAP_INDEX(0, 0)] = 0,
//	[KEYMAP_INDEX(0, 1)] = 0,
	[KEYMAP_INDEX(0, 2)] = KEY_END, // right half

	[KEYMAP_INDEX(1, 0)] = KEY_SEND, // left half
//	[KEYMAP_INDEX(1, 1)] = 0,
	[KEYMAP_INDEX(1, 2)] = KEY_BACK, // down

//	[KEYMAP_INDEX(2, 0)] = 0,
	[KEYMAP_INDEX(2, 1)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(2, 2)] = KEY_MENU, // up

//	[KEYMAP_INDEX(3, 0)] = 0,
	[KEYMAP_INDEX(3, 1)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(3, 2)] = 232, // center / select / enter
};

static struct gpio_event_matrix_info htcraphael_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= htcraphael_keymap,
	.output_gpios	= htcraphael_row_gpios,
	.input_gpios	= htcraphael_col_gpios,
	.noutputs	= ARRAY_SIZE(htcraphael_row_gpios),
	.ninputs	= ARRAY_SIZE(htcraphael_col_gpios),
	.settle_time.tv.nsec = 0,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE | GPIOKPF_PRINT_UNMAPPED_KEYS /* | GPIOKPF_PRINT_MAPPED_KEYS */
};

static struct gpio_event_direct_entry htcraphael_keyboard_switch_map[] = {
	{ 39, SW_LID	}
};

static struct gpio_event_input_info htcraphael_keyboard_switch_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_SW,
	.keymap = htcraphael_keyboard_switch_map,
	.keymap_size = ARRAY_SIZE(htcraphael_keyboard_switch_map)
};

struct gpio_event_info *htcraphael_keypad_info[] = {
	&htcraphael_matrix_info.info,
	&htcraphael_keyboard_switch_info.info
};

static struct gpio_event_platform_data htcraphael_keypad_data = {
	.name		= "htcraphael_keypad",
	.info		= htcraphael_keypad_info,
	.info_count	= ARRAY_SIZE(htcraphael_keypad_info)
};

static struct platform_device htcraphael_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &htcraphael_keypad_data,
	},
};

void htcraphael_init_keypad(void)
{
	/* keyboard slide is only for raphael, this check assumes slider to be last in array */
	if (!machine_is_htcraphael() && !machine_is_htcraphael_cdma()) {
		printk(KERN_INFO "%s: not a raphael, disabling hardware keyboard slider detection\n", __func__);
		htcraphael_keypad_data.info_count = ARRAY_SIZE(htcraphael_keypad_info) - 1;
	}
	
	platform_device_register(&htcraphael_keypad_device);
}

