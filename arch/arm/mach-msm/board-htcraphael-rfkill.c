/*
 * Copyright (C) 2008 Google, Inc.
 * Author: Nick Pelly <npelly@google.com>
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

/* Control bluetooth power for trout platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <mach/vreg.h>
#include <linux/err.h>
#include <asm/mach-types.h>

#include "board-htcraphael.h"
#include "board-htckovsky.h"
#include "proc_comm_wince.h" /* ?? GPIO ?? */

/* Kovsky */
#define KOVS_GPIO_BT_POWER	32
#define KOVS_GPIO_BT_ROUTER	63

void rfkill_switch_all(enum rfkill_type type, enum rfkill_state state);

static struct rfkill *bt_rfk;
static const char bt_name[] = "brf6300";
static struct vreg *vreg_bt;

/* ---- COMMON ---- */
static void config_gpio_table(struct msm_gpio_config *table, int len)
{
	int n;
	struct msm_gpio_config id;
	for(n = 0; n < len; n++) {
		id = table[n];
		msm_gpio_set_function( id );
	}
}
static struct msm_gpio_config bt_on_gpio_table_raph100[] = {
        	DEX_GPIO_CFG(RAPH100_UART2DM_RTS,  4, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0), /* RTS */
                DEX_GPIO_CFG(RAPH100_UART2DM_CTS,  4, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA, 0), /* CTS */
		DEX_GPIO_CFG(RAPH100_UART2DM_RX,   4, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA, 1), /* RX */ 
		DEX_GPIO_CFG(RAPH100_UART2DM_TX,   2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 1), /* TX */
};
static struct msm_gpio_config bt_off_gpio_table_raph100[] = {
        	DEX_GPIO_CFG(RAPH100_UART2DM_RTS,  0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0), /* RTS */
                DEX_GPIO_CFG(RAPH100_UART2DM_CTS,  0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA, 0), /* CTS */
		DEX_GPIO_CFG(RAPH100_UART2DM_RX,   0, GPIO_INPUT,  GPIO_PULL_UP, GPIO_2MA, 1), /* RX */ 
		DEX_GPIO_CFG(RAPH100_UART2DM_TX,   0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 1), /* TX */
};

static int bluetooth_set_power(void *data, enum rfkill_state state)
{
	int rc;
	if (state ==  RFKILL_STATE_ON) {
		printk("   bluetooth rfkill state ON\n");
		config_gpio_table(bt_on_gpio_table_raph100,ARRAY_SIZE(bt_on_gpio_table_raph100));

		switch(__machine_arch_type){
		case MACH_TYPE_HTCRHODIUM:
			gpio_configure(91, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
			gpio_set_value(91, 1);
			mdelay(50);
			gpio_configure(35, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(35, 0);
			mdelay(50);
			gpio_configure(31, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(31, 0);
			mdelay(100);
			gpio_set_value(31, 1);
		break;
		case MACH_TYPE_HTCTOPAZ:
			rc = vreg_enable(vreg_bt);	// Don't use on Rhod, disables the display!
			if(rc) {
				printk(KERN_ERR "BT VREG Activate Error %d\n", rc);
				return rc;
			}
			vreg_set_level(vreg_bt, 1800);
			gpio_configure(RAPH100_BT_RST, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(RAPH100_BT_RST, 0);
			mdelay(50);
			gpio_set_value(RAPH100_BT_RST, 1);
		break;
		case MACH_TYPE_HTCRAPHAEL_CDMA500:
		case MACH_TYPE_HTCRAPHAEL_CDMA:
		case MACH_TYPE_HTCDIAMOND_CDMA:
			gpio_configure(0x52, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(0x52, 1);
		break;
		case MACH_TYPE_HTCDIAMOND:
		case MACH_TYPE_HTCRAPHAEL:
		case MACH_TYPE_HTCBLACKSTONE:
			gpio_configure(RAPH100_WIFI_BT_PWR2, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
			gpio_set_value(RAPH100_WIFI_BT_PWR2, 1);
			mdelay(50);
			gpio_configure(RAPH100_BT_RST, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(RAPH100_BT_RST, 0);
			mdelay(50);
			gpio_set_value(RAPH100_BT_RST, 1);
		break;
		case MACH_TYPE_HTCKOVSKY:
			gpio_configure(KOVS_GPIO_BT_ROUTER, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(KOVS_GPIO_BT_ROUTER, 1);
			mdelay(50);
			gpio_configure(KOVS_GPIO_BT_POWER, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(KOVS_GPIO_BT_POWER, 0);
			mdelay(50);
			gpio_set_value(KOVS_GPIO_BT_POWER, 1);
		break;
		default:
			printk("RFKILL: unknown device!\n");
		}
	}
	else if (state == RFKILL_STATE_OFF) {
		printk("   bluetooth rfkill state   OFF\n");
		config_gpio_table(bt_off_gpio_table_raph100,ARRAY_SIZE(bt_off_gpio_table_raph100));
		switch(__machine_arch_type) {
		case MACH_TYPE_HTCRHODIUM:
			gpio_configure(91, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(91, 0);
		break;
		case MACH_TYPE_HTCTOPAZ:	      	
			gpio_configure(RAPH100_BT_RST, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			vreg_set_level(vreg_bt, 0);
			vreg_disable(vreg_bt);
		break;
		case MACH_TYPE_HTCRAPHAEL_CDMA500:
		case MACH_TYPE_HTCRAPHAEL_CDMA:
		case MACH_TYPE_HTCDIAMOND_CDMA:
			gpio_set_value(0x52, 0);
		break;
		case MACH_TYPE_HTCDIAMOND:
		case MACH_TYPE_HTCRAPHAEL:
		case MACH_TYPE_HTCBLACKSTONE:
			gpio_configure(RAPH100_BT_RST, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(RAPH100_WIFI_BT_PWR2, 0);
		case MACH_TYPE_HTCKOVSKY:
			gpio_configure(KOVS_GPIO_BT_POWER, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
			gpio_set_value(KOVS_GPIO_BT_POWER, 0);
		break;
		default:
			printk("RFKILL: unknown device!\n");
		}
	}
	else {
		printk(KERN_ERR "bad bluetooth rfkill state %d\n", state);
	}
	return 0;
}

static int __init htcraphael_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;

	printk("BT RFK probe\n");
	/* default to bluetooth off */
	rfkill_switch_all(RFKILL_TYPE_BLUETOOTH, RFKILL_STATE_OFF);
	bluetooth_set_power(NULL, RFKILL_STATE_OFF);

	bt_rfk = rfkill_allocate(&pdev->dev, RFKILL_TYPE_BLUETOOTH);
	if (!bt_rfk)
		return -ENOMEM;

	bt_rfk->name = bt_name;
	bt_rfk->state = RFKILL_STATE_OFF;
	/* userspace cannot take exclusive control */
	bt_rfk->user_claim_unsupported = 1;
	bt_rfk->user_claim = 0;
	bt_rfk->data = NULL;  // user data
	bt_rfk->toggle_radio = bluetooth_set_power;

	rc = rfkill_register(bt_rfk);

	if (rc)
		rfkill_free(bt_rfk);
	return rc;
}

static struct platform_driver htcraphael_rfkill_driver = {
	.probe = htcraphael_rfkill_probe,
	.driver = {
		.name = "htcraphael_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init htcraphael_rfkill_init(void)
{
	printk("BT RFK register\n");
	vreg_bt=vreg_get(0, "rftx");
	if(IS_ERR(vreg_bt))
		return PTR_ERR(vreg_bt);
	return platform_driver_register(&htcraphael_rfkill_driver);
}

module_init(htcraphael_rfkill_init);
MODULE_DESCRIPTION("htcraphael rfkill");
MODULE_AUTHOR("Nick Pelly <npelly@google.com>");
MODULE_LICENSE("GPL");

