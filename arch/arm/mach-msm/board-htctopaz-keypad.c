/* arch/arm/mach-msm/board-topaz-keypad.c
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <asm/mach-types.h>
#include "gpio_chip.h"
#include "board-htctopaz.h"
static char *keycaps = "--qwerty";
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "board_topaz."
module_param_named(keycaps, keycaps, charp, 0);

#define TOPAZ_POWER_KEY     83

static unsigned int topaz_col_gpios[] = { 35, 34 };

/* KP_MKIN2 (GPIO40) is not used? */
static unsigned int topaz_row_gpios[] = { 42, 41, 40 };

#define KEYMAP_INDEX(col, row) ((col)*ARRAY_SIZE(topaz_row_gpios) + (row))

/* HOME(up) + MENU (down)*/
static const unsigned short topaz_keymap[ARRAY_SIZE(topaz_col_gpios) *
					ARRAY_SIZE(topaz_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_SEND,
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEDOWN,

	[KEYMAP_INDEX(1, 0)] = KEY_HOME,
	[KEYMAP_INDEX(1, 1)] = KEY_MENU,
	[KEYMAP_INDEX(1, 2)] = KEY_BACK,
};


static struct gpio_event_matrix_info topaz_keypad_matrix_info = {
	.info.func = gpio_event_matrix_func,
	.keymap = topaz_keymap,
	.output_gpios = topaz_col_gpios,
	.input_gpios = topaz_row_gpios,
	.noutputs = ARRAY_SIZE(topaz_col_gpios),
	.ninputs = ARRAY_SIZE(topaz_row_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.debounce_delay.tv.nsec = 50 * NSEC_PER_MSEC,
	.flags = GPIOKPF_LEVEL_TRIGGERED_IRQ |
		 GPIOKPF_REMOVE_PHANTOM_KEYS |
		 GPIOKPF_PRINT_UNMAPPED_KEYS /*| GPIOKPF_PRINT_MAPPED_KEYS*/
};

static struct gpio_event_direct_entry topaz_keypad_nav_map[] = {
	{ TOPAZ_POWER_KEY,              KEY_END   },
};

static struct gpio_event_input_info topaz_keypad_nav_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_KEY,
	.keymap = topaz_keypad_nav_map,
	.debounce_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.keymap_size = ARRAY_SIZE(topaz_keypad_nav_map)
};

static struct gpio_event_info *topaz_keypad_info[] = {
	&topaz_keypad_matrix_info.info,
	&topaz_keypad_nav_info.info,
};

static struct gpio_event_platform_data topaz_keypad_data = {
	.name = "topaz-keypad",
	.info = topaz_keypad_info,
	.info_count = ARRAY_SIZE(topaz_keypad_info)
};

static struct platform_device topaz_keypad_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev		= {
		.platform_data	= &topaz_keypad_data,
	},
};

static int __init topaz_init_keypad(void)
{
	if (!machine_is_htctopaz())
		return 0;

	topaz_keypad_matrix_info.keymap = topaz_keymap;

	return platform_device_register(&topaz_keypad_device);
}

device_initcall(topaz_init_keypad);

