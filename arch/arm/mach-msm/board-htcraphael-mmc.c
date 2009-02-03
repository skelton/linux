/* linux/arch/arm/mach-msm/board-trout-mmc.c
** Author: Brian Swetland <swetland@google.com>
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <linux/debugfs.h>

#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>

#include <mach/vreg.h>
#include <mach/trout_pwrsink.h>

#include <asm/mach/mmc.h>

#include "devices.h"

#include "board-htcraphael.h"

#include "proc_comm_wince.h"

#define DEBUG_SDSLOT_VDD 1

extern int msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat);


/* This struct holds the device-specific numbers and tables */
static struct htcraphael_mmc_platform_data {
	unsigned sdcard_status_gpio;
	char sdcard_device_id:3;
	unsigned wifi_power_gpio1;
	unsigned wifi_power_gpio2;
	struct msm_gpio_config *sdcard_on_gpio_table;
	struct msm_gpio_config *sdcard_off_gpio_table;
	int sdcard_on_gpio_table_size;
	int sdcard_off_gpio_table_size;
	struct msm_gpio_config *wifi_on_gpio_table;
	struct msm_gpio_config *wifi_off_gpio_table;
	int wifi_on_gpio_table_size;
	int wifi_off_gpio_table_size;
} htcraphael_mmc_pdata;


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

/* ---- SDCARD ---- */

static struct msm_gpio_config sdcard_on_gpio_table_raph800[] = {
	DEX_GPIO_CFG( 88, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, 0 ), /* CLK */
	DEX_GPIO_CFG( 89, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, 0 ), /* CMD */
	DEX_GPIO_CFG( 90, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, 0 ), /* DAT3 */
	DEX_GPIO_CFG( 91, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, 0 ), /* DAT2 */
	DEX_GPIO_CFG( 92, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, 0 ), /* DAT1 */
	DEX_GPIO_CFG( 93, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, 0 ), /* DAT0 */
};

static struct msm_gpio_config sdcard_off_gpio_table_raph800[] = {
	DEX_GPIO_CFG( 88, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* CLK */
	DEX_GPIO_CFG( 89, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* CMD */
	DEX_GPIO_CFG( 90, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT3 */
	DEX_GPIO_CFG( 91, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT2 */
	DEX_GPIO_CFG( 92, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT1 */
	DEX_GPIO_CFG( 93, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT0 */
};

static uint opt_disable_sdcard;
static uint opt_disable_wifi;

static int __init htcraphael_disablesdcard_setup(char *str)
{
	int cal = simple_strtol(str, NULL, 0);
	
	opt_disable_sdcard = cal;
	return 1;
}

__setup("board_htcraphael.disable_sdcard=", htcraphael_disablesdcard_setup);

static int __init htcraphael_disablewifi_setup(char *str)
{
	int cal = simple_strtol(str, NULL, 0);
	
	opt_disable_wifi = cal;
	return 1;
}

__setup("board_htcraphael.disable_wifi=", htcraphael_disablewifi_setup);

static struct vreg *vreg_sdslot;	/* SD slot power */

struct mmc_vdd_xlat {
	int mask;
	int level;
};

static struct mmc_vdd_xlat mmc_vdd_table[] = {
	{ MMC_VDD_165_195,	1800 },
	{ MMC_VDD_20_21,	2050 },
	{ MMC_VDD_21_22,	2150 },
	{ MMC_VDD_22_23,	2250 },
	{ MMC_VDD_23_24,	2350 },
	{ MMC_VDD_24_25,	2450 },
	{ MMC_VDD_25_26,	2550 },
	{ MMC_VDD_26_27,	2650 },
	{ MMC_VDD_27_28,	2750 },
	{ MMC_VDD_28_29,	2850 },
	{ MMC_VDD_29_30,	2950 },
};

static unsigned int sdslot_vdd = 0xffffffff;
static unsigned int sdslot_vreg_enabled;

static uint32_t htcraphael_sdslot_switchvdd(struct device *dev, unsigned int vdd)
{
	int i, rc;

	BUG_ON(!vreg_sdslot);

	if (vdd == sdslot_vdd)
		return 0;

	sdslot_vdd = vdd;

	if (vdd == 0) {
#if DEBUG_SDSLOT_VDD
		printk("%s: Disabling SD slot power\n", __func__);
#endif
		config_gpio_table(htcraphael_mmc_pdata.sdcard_off_gpio_table,
				  htcraphael_mmc_pdata.sdcard_off_gpio_table_size);
		vreg_disable(vreg_sdslot);
		sdslot_vreg_enabled = 0;
		return 0;
	}

	if (!sdslot_vreg_enabled) {
		rc = vreg_enable(vreg_sdslot);
		if (rc) {
			printk(KERN_ERR "%s: Error enabling vreg (%d)\n",
			       __func__, rc);
		}
		config_gpio_table(htcraphael_mmc_pdata.sdcard_on_gpio_table,
				  htcraphael_mmc_pdata.sdcard_on_gpio_table_size);
		sdslot_vreg_enabled = 1;
	}

	for (i = 0; i < ARRAY_SIZE(mmc_vdd_table); i++) {
		if (mmc_vdd_table[i].mask == (1 << vdd)) {
#if DEBUG_SDSLOT_VDD
			printk("%s: Setting level to %u\n",
			        __func__, mmc_vdd_table[i].level);
#endif
			rc = vreg_set_level(vreg_sdslot,
					    mmc_vdd_table[i].level);
			if (rc) {
				printk(KERN_ERR
				       "%s: Error setting vreg level (%d)\n",
				       __func__, rc);
			}
			return 0;
		}
	}

	printk(KERN_ERR "%s: Invalid VDD %d specified\n", __func__, vdd);
	return 0;
}

static unsigned int htcraphael_sdslot_status(struct device *dev)
{
	unsigned int status;

	// For Diamond devices the MMC (MoviNAND) is built-in and always connected
	if (machine_is_htcdiamond() || machine_is_htcdiamond_cdma()) {
		return 1;
	}

	status = (unsigned int) gpio_get_value(htcraphael_mmc_pdata.sdcard_status_gpio);
	return (!status);
}

#define RAPH_MMC_VDD	MMC_VDD_165_195 | MMC_VDD_20_21 | MMC_VDD_21_22 \
			| MMC_VDD_22_23 | MMC_VDD_23_24 | MMC_VDD_24_25 \
			| MMC_VDD_25_26 | MMC_VDD_26_27 | MMC_VDD_27_28 \
			| MMC_VDD_28_29 | MMC_VDD_29_30

static struct mmc_platform_data htcraphael_sdslot_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.status_irq	= -1, /* Redefined in _init function */
	.status		= htcraphael_sdslot_status,
//	.translate_vdd	= htcraphael_sdslot_switchvdd,
};

/* ---- WIFI ---- */

static struct msm_gpio_config wifi_on_gpio_table_raph800[] = {
	DEX_GPIO_CFG( 51, 1, GPIO_INPUT , GPIO_PULL_UP, GPIO_2MA, 0 ), /* DAT3 */
	DEX_GPIO_CFG( 52, 1, GPIO_INPUT , GPIO_PULL_UP, GPIO_2MA, 0 ), /* DAT2 */
	DEX_GPIO_CFG( 53, 1, GPIO_INPUT , GPIO_PULL_UP, GPIO_2MA, 0 ), /* DAT1 */
	DEX_GPIO_CFG( 54, 1, GPIO_INPUT , GPIO_PULL_UP, GPIO_2MA, 0 ), /* DAT0 */
	DEX_GPIO_CFG( 55, 1, GPIO_INPUT , GPIO_PULL_UP, GPIO_2MA, 0 ), /* CMD */
	DEX_GPIO_CFG( 56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* CLK */
	DEX_GPIO_CFG( 29, 0, GPIO_INPUT , GPIO_PULL_UP, GPIO_2MA, 0 ), /* WLAN IRQ */
};

static struct msm_gpio_config wifi_off_gpio_table_raph800[] = {
	DEX_GPIO_CFG( 51, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT3 */
	DEX_GPIO_CFG( 52, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT2 */
	DEX_GPIO_CFG( 53, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT1 */
	DEX_GPIO_CFG( 54, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* DAT0 */
	DEX_GPIO_CFG( 55, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* CMD */
	DEX_GPIO_CFG( 56, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* CLK */
	DEX_GPIO_CFG( 29, 0, GPIO_INPUT , GPIO_NO_PULL, GPIO_2MA, 0 ), /* WLAN IRQ */
};

#if 0
//HERMAN:
	DEX_GPIO_CFG( 51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_14MA, 0 ), /* DAT3 */
	DEX_GPIO_CFG( 52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_14MA, 0 ), /* DAT2 */
	DEX_GPIO_CFG( 53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_14MA, 0 ), /* DAT1 */
	DEX_GPIO_CFG( 54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_14MA, 0 ), /* DAT0 */
	DEX_GPIO_CFG( 55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA, 0 ), /* CMD */
	DEX_GPIO_CFG( 56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_10MA, 0 ), /* CLK */
	DEX_GPIO_CFG( 29, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA, 0 ), /* WLAN IRQ */
#endif


static struct vreg *vreg_wifi_osc;	/* WIFI 32khz oscilator */
static struct vreg *vreg_wifi_2;	/* WIFI foo? */
static int htcraphael_wifi_cd = 0;	/* WIFI virtual 'card detect' status */

static struct sdio_embedded_func wifi_func = {
	.f_class	= SDIO_CLASS_WLAN,
	.f_maxblksize	= 512,
};

static struct embedded_sdio_data htcraphael_wifi_emb_data = {
	.cis	= {
		.vendor		= 0x104c,
		.device		= 0x9066,
		.blksize	= 512,
		/*.max_dtr	= 24000000,  Max of chip - no worky on Trout */
		.max_dtr	= 11000000,
	},
	.cccr	= {
		.multi_block	= 0,
		.low_speed	= 0,
		.wide_bus	= 1,
		.high_power	= 0,
		.high_speed	= 0,
	},
	.funcs	= &wifi_func,
	.num_funcs = 1,
};

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

static int htcraphael_wifi_status_register(void (*callback)(int card_present, void *dev_id), void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static unsigned int htcraphael_wifi_status(struct device *dev)
{
	return htcraphael_wifi_cd;
}

// trout_wifi_set_carddetect() is hard-coded in wlan driver...
int trout_wifi_set_carddetect(int val)
{
	printk("%s: %d\n", __func__, val);
	htcraphael_wifi_cd = val;
	if (wifi_status_cb) {
		wifi_status_cb(val, wifi_status_cb_devid);
	} else
		printk(KERN_WARNING "%s: Nobody to notify\n", __func__);
	return 0;
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(trout_wifi_set_carddetect);
#endif

static int htcraphael_wifi_power_state;

//XXX: trout_wifi_power() is hard-coded in wlan driver
int trout_wifi_power(int on)
{
	int rc;

	printk("%s: %d\n", __func__, on);

	if (on) {
		config_gpio_table(htcraphael_mmc_pdata.wifi_on_gpio_table,
				  htcraphael_mmc_pdata.wifi_on_gpio_table_size);

		rc = vreg_enable(vreg_wifi_osc);
		if (rc)
			return rc;
		rc = vreg_enable(vreg_wifi_2);
		if (rc)
			return rc;

		trout_pwrsink_set(PWRSINK_WIFI, 70);
	} else {
		config_gpio_table(htcraphael_mmc_pdata.wifi_off_gpio_table,
				  htcraphael_mmc_pdata.wifi_off_gpio_table_size);
		trout_pwrsink_set(PWRSINK_WIFI, 0);
	}

	gpio_direction_output( htcraphael_mmc_pdata.wifi_power_gpio1, on );
	mdelay(50);
	gpio_direction_output( htcraphael_mmc_pdata.wifi_power_gpio2, on );
	mdelay(50);

	if (!on) {
		vreg_disable(vreg_wifi_osc);
		vreg_disable(vreg_wifi_2);
	}
	htcraphael_wifi_power_state = on;
	return 0;
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(trout_wifi_power);
#endif

static int htcraphael_wifi_reset_state;
int trout_wifi_reset(int on)
{
	printk("%s: %d\n", __func__, on);
//	gpio_set_value( TROUT_GPIO_WIFI_PA_RESETX, !on );
	htcraphael_wifi_reset_state = on;
	mdelay(50);
	return 0;
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(trout_wifi_reset);
#endif

static struct mmc_platform_data htcraphael_wifi_data = {
	.ocr_mask		= MMC_VDD_28_29,
	.status			= htcraphael_wifi_status,
	.register_status_notify	= htcraphael_wifi_status_register,
	.embedded_sdio		= &htcraphael_wifi_emb_data,
};

static struct htcraphael_mmc_platform_data htcraphael_cdma_mmc_pdata = {
	.sdcard_status_gpio = 38,
	.sdcard_device_id = 3,
	.wifi_power_gpio1 = 102,
	.wifi_power_gpio2 = 103,
	// gpio config tables
	.sdcard_on_gpio_table = sdcard_on_gpio_table_raph800,
	.sdcard_off_gpio_table = sdcard_off_gpio_table_raph800,
	.wifi_on_gpio_table = wifi_on_gpio_table_raph800,
	.wifi_off_gpio_table = wifi_off_gpio_table_raph800,
	// gpio config table sizes
	.sdcard_on_gpio_table_size = ARRAY_SIZE(sdcard_on_gpio_table_raph800),
	.sdcard_off_gpio_table_size = ARRAY_SIZE(sdcard_off_gpio_table_raph800),
	.wifi_on_gpio_table_size = ARRAY_SIZE(wifi_on_gpio_table_raph800),
	.wifi_off_gpio_table_size = ARRAY_SIZE(wifi_off_gpio_table_raph800),
};

static struct htcraphael_mmc_platform_data htcraphael_gsm_mmc_pdata = {
	.sdcard_status_gpio = 23,
	.sdcard_device_id = 2,
	.wifi_power_gpio1 = 102,
	.wifi_power_gpio2 = 103,
//XXX: redefine these as gsm tables, when they are identified and created
	.sdcard_on_gpio_table = sdcard_on_gpio_table_raph800,
	.sdcard_off_gpio_table = sdcard_off_gpio_table_raph800,
	.wifi_on_gpio_table = wifi_on_gpio_table_raph800,
	.wifi_off_gpio_table = wifi_off_gpio_table_raph800,
	// table sizes
	.sdcard_on_gpio_table_size = ARRAY_SIZE(sdcard_on_gpio_table_raph800),
	.sdcard_off_gpio_table_size = ARRAY_SIZE(sdcard_off_gpio_table_raph800),
	.wifi_on_gpio_table_size = ARRAY_SIZE(wifi_on_gpio_table_raph800),
	.wifi_off_gpio_table_size = ARRAY_SIZE(wifi_off_gpio_table_raph800),
};


int __init htcraphael_init_mmc(void)
{
	wifi_status_cb = NULL;

	sdslot_vreg_enabled = 0;

	if (machine_is_htcraphael_cdma() || machine_is_htcdiamond_cdma()) {
		htcraphael_mmc_pdata = htcraphael_cdma_mmc_pdata;
	} else {
		htcraphael_mmc_pdata = htcraphael_gsm_mmc_pdata;
	}

	vreg_sdslot = vreg_get(0, "gp6");
	if (IS_ERR(vreg_sdslot))
		return PTR_ERR(vreg_sdslot);

	vreg_wifi_osc = vreg_get(0, "msmp");
	if (IS_ERR(vreg_wifi_osc))
		return PTR_ERR(vreg_wifi_osc);

	vreg_wifi_2 = vreg_get(0, "msme1");
	if (IS_ERR(vreg_wifi_2))
		return PTR_ERR(vreg_wifi_2);

	if (!opt_disable_wifi)
		msm_add_sdcc(1, &htcraphael_wifi_data);
	else
		printk(KERN_INFO "htcraphael: WiFi device disabled\n");

	if (!opt_disable_sdcard)
	{
		htcraphael_sdslot_data.status_irq = MSM_GPIO_TO_INT(htcraphael_mmc_pdata.sdcard_status_gpio);
		set_irq_wake(htcraphael_sdslot_data.status_irq, 1);
		gpio_configure(htcraphael_mmc_pdata.sdcard_status_gpio, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING);
		msm_add_sdcc(htcraphael_mmc_pdata.sdcard_device_id, &htcraphael_sdslot_data);
	} else
		printk(KERN_INFO "htcraphael: SD-Card interface disabled\n");
	return 0;
}

#if defined(CONFIG_DEBUG_FS)
static int htcraphaelmmc_dbg_wifi_reset_set(void *data, u64 val)
{
	trout_wifi_reset((int) val);
	return 0;
}

static int htcraphaelmmc_dbg_wifi_reset_get(void *data, u64 *val)
{
	*val = htcraphael_wifi_reset_state;
	return 0;
}

static int htcraphaelmmc_dbg_wifi_cd_set(void *data, u64 val)
{
	trout_wifi_set_carddetect((int) val);
	return 0;
}

static int htcraphaelmmc_dbg_wifi_cd_get(void *data, u64 *val)
{
	*val = htcraphael_wifi_cd;
	return 0;
}

static int htcraphaelmmc_dbg_wifi_pwr_set(void *data, u64 val)
{
	trout_wifi_power((int) val);
	return 0;
}

static int htcraphaelmmc_dbg_wifi_pwr_get(void *data, u64 *val)
{
	
	*val = htcraphael_wifi_power_state;
	return 0;
}

static int htcraphaelmmc_dbg_sd_pwr_set(void *data, u64 val)
{
	htcraphael_sdslot_switchvdd(NULL, (unsigned int) val);
	return 0;
}

static int htcraphaelmmc_dbg_sd_pwr_get(void *data, u64 *val)
{
	*val = sdslot_vdd;
	return 0;
}

static int htcraphaelmmc_dbg_sd_cd_set(void *data, u64 val)
{
	return -ENOSYS;
}

static int htcraphaelmmc_dbg_sd_cd_get(void *data, u64 *val)
{
	*val = htcraphael_sdslot_status(NULL);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(htcraphaelmmc_dbg_wifi_reset_fops,
			htcraphaelmmc_dbg_wifi_reset_get,
			htcraphaelmmc_dbg_wifi_reset_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(htcraphaelmmc_dbg_wifi_cd_fops,
			htcraphaelmmc_dbg_wifi_cd_get,
			htcraphaelmmc_dbg_wifi_cd_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(htcraphaelmmc_dbg_wifi_pwr_fops,
			htcraphaelmmc_dbg_wifi_pwr_get,
			htcraphaelmmc_dbg_wifi_pwr_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(htcraphaelmmc_dbg_sd_pwr_fops,
			htcraphaelmmc_dbg_sd_pwr_get,
			htcraphaelmmc_dbg_sd_pwr_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(htcraphaelmmc_dbg_sd_cd_fops,
			htcraphaelmmc_dbg_sd_cd_get,
			htcraphaelmmc_dbg_sd_cd_set, "%llu\n");

static int __init htcraphaelmmc_dbg_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("htcraphaelmmc_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("wifi_reset", 0644, dent, NULL,
			    &htcraphaelmmc_dbg_wifi_reset_fops);
	debugfs_create_file("wifi_cd", 0644, dent, NULL,
			    &htcraphaelmmc_dbg_wifi_cd_fops);
	debugfs_create_file("wifi_pwr", 0644, dent, NULL,
			    &htcraphaelmmc_dbg_wifi_pwr_fops);

	debugfs_create_file("sd_pwr", 0644, dent, NULL,
			    &htcraphaelmmc_dbg_sd_pwr_fops);
	debugfs_create_file("sd_cd", 0644, dent, NULL,
			    &htcraphaelmmc_dbg_sd_cd_fops);

	return 0;
}

device_initcall(htcraphaelmmc_dbg_init);

#endif
