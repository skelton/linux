/* arch/arm/mach-msm/htc_battery_smem.c
 * Based on: htc_battery.c by HTC and Google
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2008 Google, Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <asm/gpio.h>
#include <mach/board.h>
#include <asm/mach-types.h>
#include <linux/io.h>

#include "proc_comm_wince.h"

#include <mach/msm_iomap.h>
#include <mach/htc_battery.h>

static struct wake_lock vbus_wake_lock;
static struct work_struct bat_work;

#define TRACE_BATT 1

#if TRACE_BATT
 #define BATT(x...) printk(KERN_INFO "[BATT] " x)
#else
 #define BATT(x...) do {} while (0)
#endif

#define MODULE_NAME "htc_battery"

/* module debugger */
#define HTC_BATTERY_DEBUG		1
#define BATTERY_PREVENTION		1

/* Enable this will shut down if no battery */
#define ENABLE_BATTERY_DETECTION	0

typedef enum {
	DISABLE = 0,
	ENABLE_SLOW_CHG,
	ENABLE_FAST_CHG
} batt_ctl_t;

/* This order is the same as htc_power_supplies[]
 * And it's also the same as htc_cable_status_update()
 */
typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC
} charger_type_t;

struct battery_info_reply {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	u32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	u32 batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
};

struct htc_battery_info {
	int present;
	unsigned long update_time;

	/* lock to protect the battery info */
	struct mutex lock;

	struct battery_info_reply rep;
	smem_batt_t *resources;
};

static struct htc_battery_info htc_batt_info;

static unsigned int cache_time = 1000;

static int htc_battery_initial = 0;

static enum power_supply_property htc_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property htc_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

/* HTC dedicated attributes */
static ssize_t htc_battery_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);

static int htc_power_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static int htc_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static struct power_supply htc_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = htc_battery_properties,
		.num_properties = ARRAY_SIZE(htc_battery_properties),
		.get_property = htc_battery_get_property,
	},
	{
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
};

static int fake_charger=0;
module_param_named(fake, fake_charger, int, S_IRUGO | S_IWUSR | S_IWGRP);

enum {
	DEBUG_BATT	= 1<<0,
	DEBUG_CABLE	= 1<<1,
};

static int debug_mask=0;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* -------------------------------------------------------------------------- */

#if defined(CONFIG_DEBUG_FS)
int htc_battery_set_charging(batt_ctl_t ctl);
static int batt_debug_set(void *data, u64 val)
{
	return htc_battery_set_charging((batt_ctl_t) val);
}

static int batt_debug_get(void *data, u64 *val)
{
	return -ENOSYS;
}

DEFINE_SIMPLE_ATTRIBUTE(batt_debug_fops, batt_debug_get, batt_debug_set, "%llu\n");
static int __init batt_debug_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("htc_battery", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("charger_state", 0644, dent, NULL, &batt_debug_fops);

	return 0;
}

device_initcall(batt_debug_init);
#endif

static int init_batt_gpio(void)
{
	if (gpio_request(htc_batt_info.resources->gpio_battery_detect, "batt_detect") < 0)
		goto gpio_failed;
	if (gpio_request(htc_batt_info.resources->gpio_charger_enable, "charger_en") < 0)
		goto gpio_failed;
	if (gpio_request(htc_batt_info.resources->gpio_charger_current_select, "charge_current") < 0)
		goto gpio_failed;
	if (machine_is_htckovsky())
		if (gpio_request(htc_batt_info.resources->gpio_ac_detect, "ac_detect") < 0)
			goto gpio_failed;

	return 0;

gpio_failed:	
	return -EINVAL;
	
}

/* 
 *	battery_charging_ctrl - battery charing control.
 * 	@ctl:			battery control command
 *
 */
static int battery_charging_ctrl(batt_ctl_t ctl)
{
	int result = 0;

	switch (ctl) {
	case DISABLE:
		if(debug_mask&DEBUG_CABLE)
			BATT("charger OFF\n");
		/* 0 for enable; 1 disable */
		result = gpio_direction_output(htc_batt_info.resources->gpio_charger_enable, 1);
		break;
	case ENABLE_SLOW_CHG:
		if(debug_mask&DEBUG_CABLE)
			BATT("charger ON (SLOW)\n");
		result = gpio_direction_output(htc_batt_info.resources->gpio_charger_current_select, 0);
		result = gpio_direction_output(htc_batt_info.resources->gpio_charger_enable, 0);
		break;
	case ENABLE_FAST_CHG:
		if(debug_mask&DEBUG_CABLE)
			BATT("charger ON (FAST)\n");
		result = gpio_direction_output(htc_batt_info.resources->gpio_charger_current_select, 1);
		result = gpio_direction_output(htc_batt_info.resources->gpio_charger_enable, 0);
		break;
	default:
		printk(KERN_ERR "Not supported battery ctr called.!\n");
		result = -EINVAL;
		break;
	}
	
	return result;
}

int htc_battery_set_charging(batt_ctl_t ctl)
{
	int rc;
	
	if ((rc = battery_charging_ctrl(ctl)) < 0)
		goto result;
	
	if (!htc_battery_initial) {
		htc_batt_info.rep.charging_enabled = ctl & 0x3;
	} else {
		mutex_lock(&htc_batt_info.lock);
		htc_batt_info.rep.charging_enabled = ctl & 0x3;
		mutex_unlock(&htc_batt_info.lock);
	}
result:	
	return rc;
}

int htc_battery_status_update(u32 curr_level)
{
	int notify;

	if (!htc_battery_initial)
		return 0;

	mutex_lock(&htc_batt_info.lock);
	notify = (htc_batt_info.rep.level != curr_level);
	htc_batt_info.rep.level = curr_level;
	mutex_unlock(&htc_batt_info.lock);

	if (notify)
		power_supply_changed(&htc_power_supplies[CHARGER_BATTERY]);
	return 0;
}

int htc_cable_status_update(int status)
{
	int rc = 0;
	unsigned source;

	if (!htc_battery_initial)
		return 0;

	mutex_lock(&htc_batt_info.lock);
	if(readl(MSM_SHARED_RAM_BASE+0xfc00c))
		status=CHARGER_USB;	/* vbus present */
	else
		status=CHARGER_BATTERY;	/* no vbus present */

	if(fake_charger)
		status=CHARGER_USB;

	switch(status) {
	case CHARGER_BATTERY:
		if(debug_mask&DEBUG_CABLE)
			BATT("cable NOT PRESENT\n");
		htc_batt_info.rep.charging_source = CHARGER_BATTERY;
		break;
	case CHARGER_USB:
		if(debug_mask&DEBUG_CABLE)
			BATT("cable USB\n");
		htc_batt_info.rep.charging_source = CHARGER_USB;
		break;
	case CHARGER_AC:
		if(debug_mask&DEBUG_CABLE)
			BATT("cable AC\n");
		htc_batt_info.rep.charging_source = CHARGER_AC;
		break;
	default:
		printk(KERN_ERR "%s: Not supported cable status received!\n",
				__FUNCTION__);
		rc = -EINVAL;
	}
	source = htc_batt_info.rep.charging_source;
	mutex_unlock(&htc_batt_info.lock);

	htc_battery_set_charging(status);
	msm_hsusb_set_vbus_state((source==CHARGER_USB) || (source==CHARGER_AC));

	if ((source == CHARGER_USB) || (source==CHARGER_AC)) {
		wake_lock(&vbus_wake_lock);
	} else {
		/* give userspace some time to see the uevent and update
		 * LED state or whatnot...
		 */
		wake_lock_timeout(&vbus_wake_lock, HZ / 2);
	}

	/* if the power source changes, all power supplies may change state */
	power_supply_changed(&htc_power_supplies[CHARGER_BATTERY]);
	power_supply_changed(&htc_power_supplies[CHARGER_USB]);
	power_supply_changed(&htc_power_supplies[CHARGER_AC]);

	return rc;
}

static void htc_bat_work(struct work_struct *work) {
	int rc = 0;
	int ac_detect = 1;

	if (htc_batt_info.resources->gpio_ac_detect == 0)
		return;

	ac_detect = gpio_get_value(htc_batt_info.resources->gpio_ac_detect);
	BATT("ac_detect=%d\n", ac_detect);
	if (ac_detect == 0) {
		rc = battery_charging_ctrl(ENABLE_SLOW_CHG);
		BATT("charging enable rc=%d\n", rc);
	} else {
		rc = battery_charging_ctrl(DISABLE);
		BATT("charging disable rc=%d\n", rc);
	}
}

static int battery_table_4[] = {
	0,      0,
	0xe11,	0,
	0xe1e,	5,
	0xe3c,	10,
	0xe7e,	20,
	0xe97,	30,
	0xeab,	40,
	0xec1,	50,
	0xed5,	60,
	0xf0c,	70,
	0xf43,	80,
	0xf93,	90,
	0xfcb,  100,
	0x1000, 100,
	0,	0
};

static int battery_table_2[] = {
	0,      0,
	0xb10,  0,
	0xb50,  30,
	0xbe0,  65,
	0xcac,  100,
	0x1000, 100,
	0,	0
};

static int htc_get_batt_info(struct battery_info_reply *buffer)
{
	int i, capacity, v;
	int *battery_table;

	volatile unsigned int *values_32 = NULL;
	volatile unsigned short *values_16 = NULL;
	struct msm_dex_command dex;

	if (buffer == NULL) 
		return -EINVAL;
	if (!htc_batt_info.resources || !htc_batt_info.resources->smem_offset) {
		printk(KERN_ERR MODULE_NAME ": smem_offset not set\n");
		return -EINVAL;
	}

	dex.cmd = PCOM_GET_BATTERY_DATA;
	msm_proc_comm_wince(&dex, 0);

	mutex_lock(&htc_batt_info.lock);

	if (htc_batt_info.resources->smem_field_size == 4) {
		values_32 = (void *)(MSM_SHARED_RAM_BASE + htc_batt_info.resources->smem_offset);
		// FIXME: Adding factors to make these numbers come out sane, but they're not being calculated correctly.
		v = (values_32[2] * 9 / 7) + (values_32[4] / 7) - (values_32[3] / 28);
		battery_table = battery_table_4;
		buffer->batt_id = values_32[0];
		buffer->batt_temp = values_32[1] / 10;
		buffer->batt_vol = values_32[2] * 9 / 7;
		buffer->batt_current = values_32[3];
	} else if (htc_batt_info.resources->smem_field_size == 2) {
		values_16 = (void *)(MSM_SHARED_RAM_BASE + htc_batt_info.resources->smem_offset);
		v = values_16[2] - (values_16[3] / 36);
		battery_table = battery_table_2;
		buffer->batt_id = values_16[4];
		buffer->batt_temp = values_16[1] / -6 + 750;
		buffer->batt_vol = values_16[2];
		buffer->batt_current = values_16[3];
	} else {
		printk(KERN_WARNING MODULE_NAME ": unsupported smem_field_size\n");
		mutex_unlock(&htc_batt_info.lock);
		return -ENOTSUPP;
	}

#if 0
	for (i=0x85;i<=0x8a;i++)
	{
	 dex.cmd = i;
	 dex_test=0;
	 msm_proc_comm_wince(&dex, &dex_test);
	 printk("dex_batt: 0x%x = 0x%x\n",i,dex_test);
	}
	 printk("dex_batt: 0x%x = 0x%x 0x%x 0x%x 0x%x\n",PCOM_GET_BATTERY_DATA, 
	  buffer->batt_id,  buffer->batt_temp, buffer->batt_vol, buffer->batt_current);
#endif

	v = (v < 0) ? 0 : (v > 0xfff) ? 0xfff : v;
	capacity = 100;
	for (i=2; battery_table[i]; i+=2) {
		if (v<battery_table[i]) {
			capacity = battery_table[i-1] + ((v - battery_table[i-2]) * (battery_table[i+1] - battery_table[i-1])) / (battery_table[i]-battery_table[i-2]);
			break;
		}
	}
	// since this is not very accurate, stop the phone from thinking it has 0 capacity.
	if(capacity<5)
		capacity=5;
	buffer->level = capacity;
	
	if(debug_mask&DEBUG_BATT) {
		if (htc_batt_info.resources->smem_field_size == 4) {
			BATT("%p: %08x %08x %08x %08x %08x  v=%4d c=%3d\n", values_32,
				values_32[0], values_32[1], values_32[2], values_32[3], values_32[4],
				v, capacity);
		} else {
			BATT("%p: %04x %04x %04x %04x %04x  v=%4d c=%3d\n", values_16,
				values_16[0], values_16[1], values_16[2], values_16[3], values_16[4],
				v, capacity);
		}
	}

	if (gpio_get_value(htc_batt_info.resources->gpio_charger_enable) == 0) {
		buffer->charging_enabled = 1;
		if (gpio_get_value(htc_batt_info.resources->gpio_charger_current_select)) {
			buffer->charging_source = CHARGER_AC;	// 900mA
		} else {
			buffer->charging_source = CHARGER_USB;	// 500mA
		}
	} else {
		buffer->charging_enabled = 0;
		buffer->charging_source = CHARGER_BATTERY;
	}
	buffer->full_bat = 100;

	if(!machine_is_htckovsky() && htc_batt_info.resources->smem_field_size==2) { 
		// these were taken out in a kovsky commit, so assume it doesn't need them.
		buffer->charging_enabled = (values_16[3] > 0x700);
		buffer->charging_source =  (values_16[3] < 0x200) ? CHARGER_BATTERY : CHARGER_USB;
	}

	mutex_unlock(&htc_batt_info.lock);

	htc_cable_status_update(buffer->charging_source);

	return 0;
}

/* -------------------------------------------------------------------------- */
static int htc_power_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	charger_type_t charger;
	
	mutex_lock(&htc_batt_info.lock);
	charger = htc_batt_info.rep.charging_source;
	mutex_unlock(&htc_batt_info.lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (charger ==  CHARGER_AC ? 1 : 0);
		else if (psy->type == POWER_SUPPLY_TYPE_USB)
			val->intval = (charger ==  CHARGER_USB ? 1 : 0);
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static int htc_battery_get_charging_status(void)
{
	u32 level;
	charger_type_t charger;	
	int ret;
	
	mutex_lock(&htc_batt_info.lock);
	charger = htc_batt_info.rep.charging_source;
	
	switch (charger) {
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
		level = htc_batt_info.rep.level;
		if (level == 100)
			ret = POWER_SUPPLY_STATUS_FULL;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}
	mutex_unlock(&htc_batt_info.lock);
	return ret;
}

static int htc_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = htc_battery_get_charging_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = htc_batt_info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&htc_batt_info.lock);
		val->intval = htc_batt_info.rep.level;
		mutex_unlock(&htc_batt_info.lock);
		break;
	default:		
		return -EINVAL;
	}
	
	return 0;
}

void htc_battery_external_power_changed(struct power_supply *psy) {
	BATT("external power changed\n");
	schedule_work(&bat_work);
	return;
}

static irqreturn_t htc_bat_gpio_isr(int irq, void *data) {
	BATT("IRQ %d for GPIO \n", irq);
	schedule_work(&bat_work);
	return IRQ_HANDLED;
}

#define HTC_BATTERY_ATTR(_name)							\
{										\
	.attr = { .name = #_name, .mode = S_IRUGO, .owner = THIS_MODULE },	\
	.show = htc_battery_show_property,					\
	.store = NULL,								\
}

static struct device_attribute htc_battery_attrs[] = {
	HTC_BATTERY_ATTR(batt_id),
	HTC_BATTERY_ATTR(batt_vol),
	HTC_BATTERY_ATTR(batt_temp),
	HTC_BATTERY_ATTR(batt_current),
	HTC_BATTERY_ATTR(charging_source),
	HTC_BATTERY_ATTR(charging_enabled),
	HTC_BATTERY_ATTR(full_bat),
};

enum {
	BATT_ID = 0,
	BATT_VOL,
	BATT_TEMP,
	BATT_CURRENT,
	CHARGING_SOURCE,
	CHARGING_ENABLED,
	FULL_BAT,
};

static int htc_battery_create_attrs(struct device *dev)
{
	int i, rc;
	
	for (i = 0; i < ARRAY_SIZE(htc_battery_attrs); i++) {
		rc = device_create_file(dev, &htc_battery_attrs[i]);
		if (rc)
			goto htc_attrs_failed;
	}

	goto succeed;
	
htc_attrs_failed:
	while (i--)
		device_remove_file(dev, &htc_battery_attrs[i]);
succeed:	
	return rc;
}

static ssize_t htc_battery_show_property(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - htc_battery_attrs;
	
	/* check cache time to decide if we need to update */
	if (htc_batt_info.update_time &&
            time_before(jiffies, htc_batt_info.update_time +
                                msecs_to_jiffies(cache_time)))
                goto dont_need_update;
	
	if (htc_get_batt_info(&htc_batt_info.rep) < 0) {
		printk(KERN_ERR "%s: get_batt_info failed!!!\n", __FUNCTION__);
	} else {
		htc_batt_info.update_time = jiffies;
	}
dont_need_update:

	mutex_lock(&htc_batt_info.lock);
	switch (off) {
	case BATT_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_id);
		break;
	case BATT_VOL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_vol);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_temp);
		break;
	case BATT_CURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_current);
		break;
	case CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.charging_source);
		break;
	case CHARGING_ENABLED:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.charging_enabled);
		break;		
	case FULL_BAT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.full_bat);
		break;
	default:
		i = -EINVAL;
	}	
	mutex_unlock(&htc_batt_info.lock);
	
	return i;
}

static int htc_battery_thread(void *data)
{
	daemonize("battery");
	allow_signal(SIGKILL);

	while (!signal_pending((struct task_struct *)current)) {
		msleep(10000);
		power_supply_changed(&htc_power_supplies[CHARGER_BATTERY]);
	}
	return 0;
}
static int htc_battery_probe(struct platform_device *pdev)
{
	int i, rc;

	INIT_WORK(&bat_work, htc_bat_work);
	htc_batt_info.resources = (smem_batt_t *)pdev->dev.platform_data;

	if (!htc_batt_info.resources) {
		printk(KERN_ERR "%s: no pdata resources!\n", __FUNCTION__);
		return -EINVAL;
	}

	/* init battery gpio */
	if ((rc = init_batt_gpio()) < 0) {
		printk(KERN_ERR "%s: init battery gpio failed!\n", __FUNCTION__);
		return rc;
	}

	/* init structure data member */
	htc_batt_info.update_time 	= jiffies;
	htc_batt_info.present 		= gpio_get_value(htc_batt_info.resources->gpio_battery_detect);
	
	/* init power supplier framework */
	for (i = 0; i < ARRAY_SIZE(htc_power_supplies); i++) {
		rc = power_supply_register(&pdev->dev, &htc_power_supplies[i]);
		if (rc)
			printk(KERN_ERR "Failed to register power supply (%d)\n", rc);	
	}

	/* create htc detail attributes */
	htc_battery_create_attrs(htc_power_supplies[CHARGER_BATTERY].dev);

	htc_battery_initial = 1;

	if (htc_get_batt_info(&htc_batt_info.rep) < 0)
		printk(KERN_ERR "%s: get info failed\n", __FUNCTION__);

	htc_batt_info.update_time = jiffies;
	kernel_thread(htc_battery_thread, NULL, CLONE_KERNEL);
	if (machine_is_htckovsky()) {
		rc = request_irq(gpio_to_irq(htc_batt_info.resources->gpio_ac_detect),
				htc_bat_gpio_isr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"htc_ac_detect", &htc_batt_info);
        if (rc)
            printk(KERN_ERR "IRQ-request rc=%d\n", rc);
    }

	return 0;
}

static struct platform_driver htc_battery_driver = {
	.probe	= htc_battery_probe,
	.driver	= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init htc_battery_init(void)
{
	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	mutex_init(&htc_batt_info.lock);
	platform_driver_register(&htc_battery_driver);
	printk(KERN_INFO "HTC Battery Driver initialized\n");
	return 0;
}

late_initcall(htc_battery_init);
MODULE_DESCRIPTION("HTC Battery Driver");
MODULE_LICENSE("GPL");
