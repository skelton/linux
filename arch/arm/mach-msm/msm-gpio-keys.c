/* linux/arch/arm/mach-msm/gpio-keys.c
 *
 * Copyright (C) 2009 HUSSON Pierre-Hugues <phhusson@free.fr>
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

#include <asm/mach-types.h>
#include <mach/gpio.h>
#include <mach/io.h>
#include <linux/delay.h>
#include <linux/gpio_keys.h>

#define MODULE_NAME "msm_gpio_keys"
#ifdef CONFIG_ANDROID_PMEM
#define PWRK	KEY_HOME
#else
#define PWRK	KEY_POWER
#endif

static struct gpio_keys_button blackstone_button_table[] = {
	/*KEY 			GPIO	ACTIVE_LOW DESCRIPTION		type		wakeup	debounce*/
	{KEY_VOLUMEUP,		39,		1, "Volume Up",		EV_KEY,		0, 	0},
	{KEY_VOLUMEDOWN,	40,		1, "Volume Down",	EV_KEY,		0,	0},
	{PWRK,		       	83,		1, "Power button",	EV_KEY,		1,	0},
};

static struct gpio_keys_button raph_button_table[] = {
	/*KEY 			GPIO	ACTIVE_LOW DESCRIPTION		type		wakeup	debounce*/
	{PWRK,			83,		1, "Power button",	EV_KEY,		1,	0},
};

static struct gpio_keys_button topaz_button_table[] = {
	/*KEY 			GPIO	ACTIVE_LOW DESCRIPTION		type		wakeup	debounce*/
	{PWRK,			83,		1, "Power button",	EV_KEY,		1,	0},
};

static struct gpio_keys_button rhodium_button_table[] = {
	/*KEY                   GPIO    ACTIVE_LOW DESCRIPTION          type            wakeup  debounce*/
	{PWRK,                  83,             1, "Power button",      EV_KEY,         1,      0},
};

static struct gpio_keys_platform_data gpio_keys_data;

static struct platform_device gpio_keys = {
        .name = "gpio-keys",
        .dev  = {
                .platform_data = &gpio_keys_data,
        },
        .id   = -1,
};

static struct platform_device *devices[] __initdata = {
	&gpio_keys,
};

static void __init msm_gpio_keys_init(void) {
	if(machine_is_htcblackstone() || machine_is_htckovsky()) {
		gpio_keys_data.buttons=blackstone_button_table;
		gpio_keys_data.nbuttons=ARRAY_SIZE(blackstone_button_table);
	} else if(machine_is_htcraphael() || machine_is_htcraphael_cdma() || machine_is_htcraphael_cdma500() ||
			machine_is_htcdiamond() || machine_is_htcdiamond_cdma()) {
		gpio_keys_data.buttons=raph_button_table;
		gpio_keys_data.nbuttons=ARRAY_SIZE(raph_button_table);
	} else if(machine_is_htctopaz()) {
		gpio_keys_data.buttons=topaz_button_table;
		gpio_keys_data.nbuttons=ARRAY_SIZE(topaz_button_table);
	} else if(machine_is_htcrhodium()) {
		gpio_keys_data.buttons=rhodium_button_table;
		gpio_keys_data.nbuttons=ARRAY_SIZE(rhodium_button_table);
	} else {
		printk(KERN_INFO "Callled msm_gpio_keys on unsupported device!");
		return;
	}
	platform_add_devices(devices, ARRAY_SIZE(devices));

}

module_init(msm_gpio_keys_init);

MODULE_DESCRIPTION("MSM WinCE gpio keys driver");
MODULE_AUTHOR("HUSSON Pierre-Hugues <phhusson@free.fr>");
MODULE_LICENSE("GPL");
