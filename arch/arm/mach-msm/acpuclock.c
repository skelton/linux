/* arch/arm/mach-msm/acpuclock.c
 *
 * MSM architecture clock driver
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
 * Copyright (c) 2007-2009, Code Aurora Forum. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <linux/debugfs.h>
#include <linux/poll.h>

#include "proc_comm.h"
#include "acpuclock.h"

#define PERF_SWITCH_DEBUG 0
#define PERF_SWITCH_STEP_DEBUG 0

static int oc_freq_khz = 0;
module_param_named(oc_freq_khz, oc_freq_khz, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct clock_state
{
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	uint32_t			acpu_switch_time_us;
	uint32_t			max_speed_delta_khz;
	uint32_t			vdd_switch_time_us;
	unsigned long			power_collapse_khz;
	unsigned long			wait_for_irq_khz;
	unsigned int			max_axi_khz;
};

struct clkctl_acpu_speed {
	unsigned int	use_for_scaling;
	unsigned int	a11clk_khz;
	int		pll;
	unsigned int	a11clk_src_sel;
	unsigned int	a11clk_src_div;
	unsigned int	ahbclk_khz;
	unsigned int	ahbclk_div;
	unsigned int 	axiclk_khz;
	int		vdd;
	unsigned long	lpj; /* loops_per_jiffy */
/* Index in acpu_freq_tbl[] for steppings. */
	short		down;
	short		up;
};

static struct clk *ebi1_clk;
static struct clock_state drv_state = { 0 };
static struct clkctl_acpu_speed *acpu_freq_tbl;

static void __init acpuclk_init(void);

/* MSM7201A Levels 3-6 all correspond to 1.2V, level 7 corresponds to 1.325V. */

/*
 * ACPU freq tables used for different PLLs frequency combinations. The
 * correct table is selected during init.
 *
 * Table stepping up/down is calculated during boot to choose the largest
 * frequency jump that's less than max_speed_delta_khz and preferrably on the
 * same PLL. If no frequencies using the same PLL are within
 * max_speed_delta_khz, then the farthest frequency that is within
 * max_speed_delta_khz is chosen.
 */

/* 7x01/7x25 normal with GSM capable modem */
static struct clkctl_acpu_speed pll0_245_pll1_768_pll2_1056[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 30720, 0 },
	{ 0, 122880, ACPU_PLL_0, 4, 1,  61440, 1,  61440, 0 },
	{ 1, 128000, ACPU_PLL_1, 1, 5,  64000, 1,  61440, 0 },
	{ 0, 176000, ACPU_PLL_2, 2, 5,  88000, 1,  61440, 3 },
	{ 0, 245760, ACPU_PLL_0, 4, 0,  81920, 2,  61440, 4 },
	{ 1, 256000, ACPU_PLL_1, 1, 2, 128000, 1, 128000, 5 },
	{ 0, 352000, ACPU_PLL_2, 2, 2,  88000, 3, 128000, 5 },
	{ 1, 384000, ACPU_PLL_1, 1, 1, 128000, 2, 128000, 6 },
	{ 1, 528000, ACPU_PLL_2, 2, 1, 132000, 3, 128000, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x01/7x25 normal with CDMA-only modem */
static struct clkctl_acpu_speed pll0_196_pll1_768_pll2_1056[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 24576, 0 },
	{ 0,  98304, ACPU_PLL_0, 4, 1,  49152, 1,  24576, 0 },
	{ 1, 128000, ACPU_PLL_1, 1, 5,  64000, 1,  24576, 0 },
	{ 0, 176000, ACPU_PLL_2, 2, 5,  88000, 1,  24576, 3 },
	{ 0, 196608, ACPU_PLL_0, 4, 0,  65536, 2,  24576, 4 },
	{ 1, 256000, ACPU_PLL_1, 1, 2, 128000, 1, 128000, 5 },
	{ 0, 352000, ACPU_PLL_2, 2, 2,  88000, 3, 128000, 5 },
	{ 1, 384000, ACPU_PLL_1, 1, 1, 128000, 2, 128000, 6 },
	{ 1, 528000, ACPU_PLL_2, 2, 1, 132000, 3, 128000, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x01/7x25 turbo with GSM capable modem */
static struct clkctl_acpu_speed pll0_245_pll1_960_pll2_1056[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 30720, 0 },
	{ 0, 120000, ACPU_PLL_1, 1, 7,  60000, 1,  61440, 0 },
	{ 1, 122880, ACPU_PLL_0, 4, 1,  61440, 1,  61440, 0 },
	{ 0, 176000, ACPU_PLL_2, 2, 5,  88000, 1,  61440, 3 },
	{ 1, 245760, ACPU_PLL_0, 4, 0,  81920, 2,  61440, 4 },
	{ 1, 320000, ACPU_PLL_1, 1, 2, 107000, 2, 120000, 5 },
	{ 0, 352000, ACPU_PLL_2, 2, 2,  88000, 3, 120000, 5 },
	{ 1, 480000, ACPU_PLL_1, 1, 1, 120000, 3, 120000, 6 },
	{ 1, 528000, ACPU_PLL_2, 2, 1, 132000, 3, 122880, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x01/7x25 turbo with CDMA-only modem */
static struct clkctl_acpu_speed pll0_196_pll1_960_pll2_1056[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 24576, 0 },
	{ 1,  98304, ACPU_PLL_0, 4, 1,  49152, 1,  24576, 0 },
	{ 0, 120000, ACPU_PLL_1, 1, 7,  60000, 1,  24576, 0 },
	{ 0, 176000, ACPU_PLL_2, 2, 5,  88000, 1,  24576, 3 },
	{ 1, 196608, ACPU_PLL_0, 4, 0,  65536, 2,  24576, 4 },
	{ 1, 320000, ACPU_PLL_1, 1, 2, 107000, 2, 120000, 5 },
	{ 0, 352000, ACPU_PLL_2, 2, 2,  88000, 3, 120000, 5 },
	{ 1, 480000, ACPU_PLL_1, 1, 1, 120000, 3, 120000, 6 },
	{ 1, 528000, ACPU_PLL_2, 2, 1, 132000, 3, 120000, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x27 normal with GSM capable modem */
static struct clkctl_acpu_speed pll0_245_pll1_960_pll2_1200[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 30720, 0 },
	{ 0, 120000, ACPU_PLL_1, 1, 7,  60000, 1,  61440, 3 },
	{ 1, 122880, ACPU_PLL_0, 4, 1,  61440, 1,  61440, 3 },
	{ 0, 200000, ACPU_PLL_2, 2, 5,  66667, 2,  61440, 4 },
	{ 1, 245760, ACPU_PLL_0, 4, 0, 122880, 1, 122880, 4 },
	{ 1, 320000, ACPU_PLL_1, 1, 2, 160000, 1, 122880, 5 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 133333, 2, 122880, 5 },
	{ 1, 480000, ACPU_PLL_1, 1, 1, 160000, 2, 122880, 6 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 200000, 2, 122880, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x27 normal with CDMA-only modem */
static struct clkctl_acpu_speed pll0_196_pll1_960_pll2_1200[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 24576, 0 },
	{ 1,  98304, ACPU_PLL_0, 4, 1,  98304, 0,  49152, 3 },
	{ 0, 120000, ACPU_PLL_1, 1, 7,  60000, 1,  49152, 3 },
	{ 1, 196608, ACPU_PLL_0, 4, 0,  65536, 2,  98304, 4 },
	{ 0, 200000, ACPU_PLL_2, 2, 5,  66667, 2,  98304, 4 },
	{ 1, 320000, ACPU_PLL_1, 1, 2, 160000, 1, 120000, 5 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 133333, 2, 120000, 5 },
	{ 1, 480000, ACPU_PLL_1, 1, 1, 160000, 2, 120000, 6 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 200000, 2, 120000, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x27 normal with GSM capable modem - PLL0 and PLL1 swapped */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 30720, 0 },
	{ 0, 120000, ACPU_PLL_0, 4, 7,  60000, 1,  61440, 3 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  61440, 1,  61440, 3 },
	{ 0, 200000, ACPU_PLL_2, 2, 5,  66667, 2,  61440, 4 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 122880, 1, 122880, 4 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 160000, 1, 122880, 5 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 133333, 2, 122880, 5 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 160000, 2, 122880, 6 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 200000, 2, 122880, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* 7x27 normal with CDMA-only modem - PLL0 and PLL1 swapped */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 24576, 0 },
	{ 1,  98304, ACPU_PLL_1, 1, 1,  98304, 0,  49152, 3 },
	{ 0, 120000, ACPU_PLL_0, 4, 7,  60000, 1,  49152, 3 },
	{ 1, 196608, ACPU_PLL_1, 1, 0,  65536, 2,  98304, 4 },
	{ 0, 200000, ACPU_PLL_2, 2, 5,  66667, 2,  98304, 4 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 160000, 1, 120000, 5 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 133333, 2, 120000, 5 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 160000, 2, 120000, 6 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 200000, 2, 120000, 7 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

#define PLL_196_MHZ	10
#define PLL_245_MHZ	12
#define PLL_491_MHZ	25
#define PLL_768_MHZ	40
#define PLL_960_MHZ	50
#define PLL_1056_MHZ	55
#define PLL_1200_MHZ	62

#define PLL_CONFIG(m0, m1, m2) { \
	PLL_##m0##_MHZ, PLL_##m1##_MHZ, PLL_##m2##_MHZ, \
	pll0_##m0##_pll1_##m1##_pll2_##m2 \
}

struct pll_freq_tbl_map {
	unsigned int	pll0_l;
	unsigned int	pll1_l;
	unsigned int	pll2_l;
	struct clkctl_acpu_speed *tbl;
};

static struct pll_freq_tbl_map acpu_freq_tbl_list[] = {
	PLL_CONFIG(196, 768, 1056),
	PLL_CONFIG(245, 768, 1056),
	PLL_CONFIG(196, 960, 1056),
	PLL_CONFIG(245, 960, 1056),
	PLL_CONFIG(196, 960, 1200),
	PLL_CONFIG(245, 960, 1200),
	PLL_CONFIG(960, 196, 1200),
	PLL_CONFIG(960, 245, 1200),
	{ 0, 0, 0, 0 }
};

const uint8_t nbr_vdd = 9;
static uint8_t vdd_user_data[9];
static uint8_t user_vdd = 0;
static uint8_t user_vdd_max = 8;

#if defined(CONFIG_MSM_CPU_FREQ_ONDEMAND) || \
    defined(CONFIG_MSM_CPU_FREQ_USERSPACE) || \
    defined(CONFIG_MSM_CPU_FREQ_MSM7K)
static struct cpufreq_frequency_table freq_table[20];

static void __init cpufreq_table_init(void)
{
	unsigned int i;
	unsigned int freq_cnt = 0;

	/* Construct the freq_table table from acpu_freq_tbl since the
	 * freq_table values need to match frequencies specified in
	 * acpu_freq_tbl and acpu_freq_tbl needs to be fixed up during init.
	 */
	for (i = 0; acpu_freq_tbl[i].a11clk_khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table)-1; i++) {
		if (acpu_freq_tbl[i].use_for_scaling) {
			freq_table[freq_cnt].index = freq_cnt;
			freq_table[freq_cnt].frequency
				= acpu_freq_tbl[i].a11clk_khz;
			freq_cnt++;
		}
	}

	/* freq_table not big enough to store all usable freqs. */
	BUG_ON(acpu_freq_tbl[i].a11clk_khz != 0);

	freq_table[freq_cnt].index = freq_cnt;
	freq_table[freq_cnt].frequency = CPUFREQ_TABLE_END;

	pr_info("%d scaling frequencies supported.\n", freq_cnt);
}
#endif

static int acpu_debug_mask;
module_param_call(debug_mask, param_set_int, param_get_int,
		&acpu_debug_mask, S_IWUSR | S_IRUGO);

static int pc_pll_request(unsigned id, unsigned on)
{
#if 0
	int res;
	on = !!on;

	if (acpu_debug_mask & PERF_SWITCH_PLL_DEBUG) {
		if (on)
			printk(KERN_DEBUG "Enabling PLL %d\n", id);
		else
			printk(KERN_DEBUG "Disabling PLL %d\n", id);
	}

	res = msm_proc_comm(PCOM_CLKCTL_RPC_PLL_REQUEST, &id, &on);
	if (res < 0)
		return res;

	if (acpu_debug_mask & PERF_SWITCH_PLL_DEBUG) {
		if (on)
			printk(KERN_DEBUG "PLL %d enabled\n", id);
		else
			printk(KERN_DEBUG "PLL %d disabled\n", id);
	}
	return res;
#endif
	return 0;
}


/*----------------------------------------------------------------------------
 * ARM11 'owned' clock control
 *---------------------------------------------------------------------------*/
module_param_call(pwrc_khz, param_set_int, param_get_int,
		&drv_state.power_collapse_khz, S_IWUSR | S_IRUGO);
module_param_call(wfi_khz, param_set_int, param_get_int,
		&drv_state.wait_for_irq_khz, S_IWUSR | S_IRUGO);

unsigned long acpuclk_power_collapse(void) {
	int ret = acpuclk_get_rate();
	acpuclk_set_rate(drv_state.power_collapse_khz, SETRATE_PC);
	return ret * 1000;
}

unsigned long acpuclk_wait_for_irq(void) {
	int ret = acpuclk_get_rate();
	acpuclk_set_rate(drv_state.wait_for_irq_khz, SETRATE_PC);
	return ret * 1000;
}

static int acpuclk_set_vdd_level(int vdd)
{
	uint32_t current_vdd;

	current_vdd = readl(A11S_VDD_SVS_PLEVEL_ADDR) & 0x07;

#if PERF_SWITCH_DEBUG
	printk(KERN_DEBUG "acpuclock: Switching VDD from %u -> %d\n",
	       current_vdd, vdd);
#endif
writel((1 << 7) | (vdd << 3), A11S_VDD_SVS_PLEVEL_ADDR);
	udelay(drv_state.vdd_switch_time_us);
	if ((readl(A11S_VDD_SVS_PLEVEL_ADDR) & 0x7) != vdd) {
#if PERF_SWITCH_DEBUG
		printk(KERN_ERR "acpuclock: VDD set failed\n");
#endif
		return -EIO;
	}

#if PERF_SWITCH_DEBUG
	printk(KERN_DEBUG "acpuclock: VDD switched\n");
#endif
	return 0;
}

/* Set proper dividers for the given clock speed. */
static void acpuclk_set_div(const struct clkctl_acpu_speed *hunt_s) {
	uint32_t reg_clkctl, reg_clksel, clk_div, src_sel;

	reg_clksel = readl(A11S_CLK_SEL_ADDR);

	/* AHB_CLK_DIV */
	clk_div = (reg_clksel >> 1) & 0x03;
	/* CLK_SEL_SRC1NO */
	src_sel = reg_clksel & 1;

	/*
	 * If the new clock divider is higher than the previous, then
	 * program the divider before switching the clock
	 */
	if (hunt_s->ahbclk_div > clk_div) {
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel(reg_clksel, A11S_CLK_SEL_ADDR);
	}
	
	/* Program clock source and divider */
	reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
	reg_clkctl &= ~(0xFF << (8 * src_sel));
	reg_clkctl |= hunt_s->a11clk_src_sel << (4 + 8 * src_sel);
	reg_clkctl |= hunt_s->a11clk_src_div << (0 + 8 * src_sel);
	writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

	/* Program clock source selection */
	reg_clksel ^= 1;
	writel(reg_clksel, A11S_CLK_SEL_ADDR);

	/*
	 * If the new clock divider is lower than the previous, then
	 * program the divider after switching the clock
	 */
	if (hunt_s->ahbclk_div < clk_div) {
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel(reg_clksel, A11S_CLK_SEL_ADDR);
	}
}

int acpuclk_set_rate(unsigned long rate, enum setrate_reason reason)
{
	uint32_t reg_clkctl;
	struct clkctl_acpu_speed *cur_s, *tgt_s, *strt_s;
	int rc = 0;
	unsigned int plls_enabled = 0, pll;
	unsigned int v_val;
	strt_s = cur_s = drv_state.current_speed;

	WARN_ONCE(cur_s == NULL, "acpuclk_set_rate: not initialized\n");
	if (cur_s == NULL)
		return -ENOENT;

	if (rate == (cur_s->a11clk_khz * 1000))
		return 0;

	for (tgt_s = acpu_freq_tbl; tgt_s->a11clk_khz != 0; tgt_s++) {
		if (tgt_s->a11clk_khz == (rate / 1000))
			break;
	}

	if (tgt_s->a11clk_khz == 0)
		return -EINVAL;

	if(user_vdd)    // Switch to the user VREG
		v_val = vdd_user_data[tgt_s-acpu_freq_tbl];
	else
		v_val = tgt_s->vdd;

	/* Choose the highest speed speed at or below 'rate' with same PLL. */
	if (reason != SETRATE_CPUFREQ
			&& tgt_s->a11clk_khz < cur_s->a11clk_khz) {
		while (tgt_s->pll != ACPU_PLL_TCXO && tgt_s->pll != cur_s->pll)
			tgt_s--;
	}

	if (strt_s->pll != ACPU_PLL_TCXO)
		plls_enabled |= 1 << strt_s->pll;

	if (reason == SETRATE_CPUFREQ) {
		mutex_lock(&drv_state.lock);
		if (strt_s->pll != tgt_s->pll && tgt_s->pll != ACPU_PLL_TCXO) {
			rc = pc_pll_request(tgt_s->pll, 1);
			if (rc < 0) {
				pr_err("PLL%d enable failed (%d)\n", tgt_s->pll, rc);
					goto out;
			}
			plls_enabled |= 1 << tgt_s->pll;
		}
		/* Increase VDD if needed. */
		if (v_val > cur_s->vdd) {
			if ((rc = acpuclk_set_vdd_level(v_val)) < 0) {
				printk(KERN_ERR "Unable to switch ACPU vdd\n");
				goto out;
			}
		}
	} else {
		/* Power collapse should also increase VDD. */
		if (v_val > cur_s->vdd) {
			if ((rc = acpuclk_set_vdd_level(v_val)) < 0) {
				printk(KERN_ERR "Unable to switch ACPU vdd\n");
				goto out;
			}
		}
	}

	/* Set wait states for CPU inbetween frequency changes */
	reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
	reg_clkctl |= (100 << 16); /* set WT_ST_CNT */
	writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

#if PERF_SWITCH_DEBUG
	printk(KERN_INFO "acpuclock: Switching from ACPU rate %u -> %u\n",
	       strt_s->a11clk_khz * 1000, tgt_s->a11clk_khz * 1000);
#endif

	while (cur_s != tgt_s) {
		/*
		 * Always jump to target freq if within 256mhz, regulardless of
		 * PLL. If differnece is greater, use the predefinied
		 * steppings in the table.
		 */
		int d = abs((int)(cur_s->a11clk_khz - tgt_s->a11clk_khz));
		if (d > drv_state.max_speed_delta_khz) {
			/* Step up or down depending on target vs current. */
			int clk_index = tgt_s->a11clk_khz > cur_s->a11clk_khz ?
				cur_s->up : cur_s->down;
			if (clk_index < 0) { /* This should not happen. */
				printk(KERN_ERR "cur:%u target: %u\n",
					cur_s->a11clk_khz, tgt_s->a11clk_khz);
				rc = -EINVAL;
				goto out;
			}
			cur_s = &acpu_freq_tbl[clk_index];
		} else {
			cur_s = tgt_s;
		}
#if PERF_SWITCH_STEP_DEBUG
		printk(KERN_DEBUG "%s: STEP khz = %u, pll = %d\n",
			__FUNCTION__, cur_s->a11clk_khz, cur_s->pll);
#endif
		/* Power collapse should also request pll.(19.2->528) */
		if (cur_s->pll != ACPU_PLL_TCXO
		    && !(plls_enabled & (1 << cur_s->pll))) {
			rc = pc_pll_request(cur_s->pll, 1);
			if (rc < 0) {
				pr_err("PLL%d enable failed (%d)\n",
					cur_s->pll, rc);
				goto out;
			}
			plls_enabled |= 1 << cur_s->pll;
		}

		acpuclk_set_div(cur_s);
		drv_state.current_speed = cur_s;
		/* Re-adjust lpj for the new clock speed. */
		loops_per_jiffy = cur_s->lpj;
		udelay(drv_state.acpu_switch_time_us);
	}

	/* Change the AXI bus frequency if we can. */
	/* Don't change it at power collapse, it will cause stability issue. */
	if (strt_s->axiclk_khz != tgt_s->axiclk_khz && reason!=SETRATE_PC) {
		rc = clk_set_rate(ebi1_clk, tgt_s->axiclk_khz * 1000);
		if (rc < 0)
			pr_err("Setting AXI min rate failed!\n");
	}

	/* Nothing else to do for power collapse */
	if (reason == SETRATE_PC)
		return 0;

	/* Disable PLLs we are not using anymore. */
	plls_enabled &= ~(1 << tgt_s->pll);
	for (pll = ACPU_PLL_0; pll <= ACPU_PLL_2; pll++)
		if (plls_enabled & (1 << pll)) {
			rc = pc_pll_request(pll, 0);
			if (rc < 0) {
				pr_err("PLL%d disable failed (%d)\n", pll, rc);
				goto out;
			}
		}

	/* Drop VDD level if we can. */
	if (tgt_s->vdd < strt_s->vdd) {
		if (acpuclk_set_vdd_level(tgt_s->vdd) < 0)
			printk(KERN_ERR "acpuclock: Unable to drop ACPU vdd\n");
	}

#if PERF_SWITCH_DEBUG
	printk(KERN_DEBUG "%s: ACPU speed change complete\n", __FUNCTION__);
#endif
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __init acpuclk_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel ;
	int rc;

	unsigned int a11clk_khz_new;
	uint32_t reg_clkctl;

	/*
	 * Determine the rate of ACPU clock
	 */

	if (!(readl(A11S_CLK_SEL_ADDR) & 0x01)) { /* CLK_SEL_SRC1N0 */
		/* CLK_SRC0_SEL */
		sel = (readl(A11S_CLK_CNTL_ADDR) >> 12) & 0x7;
		/* CLK_SRC0_DIV */
		div = (readl(A11S_CLK_CNTL_ADDR) >> 8) & 0x0f;
	} else {
		/* CLK_SRC1_SEL */
		sel = (readl(A11S_CLK_CNTL_ADDR) >> 4) & 0x07;
		/* CLK_SRC1_DIV */
		div = readl(A11S_CLK_CNTL_ADDR) & 0x0f;
	}

	if (oc_freq_khz) {
		// make sure target freq is multpile of 19.2mhz
		oc_freq_khz = (oc_freq_khz / 19200) * 19200;

	        // set pll2 frequency
		writel(oc_freq_khz / 19200, MSM_CLK_CTL_BASE+0x33c);
	        udelay(50);

		// for overclocking we will set pll2 to a 1 divider
		// to have headroom over the max default 1.2ghz/2 setting
		if ((sel == ACPU_PLL_2) && div) {
			reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
			if (!(readl(A11S_CLK_SEL_ADDR) & 0x01)) { /* CLK_SEL_SRC1N0 */
				reg_clkctl &= ~(0xf << 8);
			} else {
				reg_clkctl &= ~0xf;
			}
			writel(reg_clkctl, A11S_CLK_CNTL_ADDR);
			udelay(50);
			div = 0;
		}

		// adjust pll2 frequencies
		for (speed = acpu_freq_tbl; speed->a11clk_khz != 0; speed++) {
			if (speed->pll == ACPU_PLL_2) {
				speed->a11clk_src_div = (speed->a11clk_src_div + 2) /2 - 1;
				a11clk_khz_new = oc_freq_khz / (speed->a11clk_src_div + 1);

				if ((sel == ACPU_PLL_2) && (div == speed->a11clk_src_div)) {
					// adjust jiffy to new clock speed
					loops_per_jiffy = cpufreq_scale(loops_per_jiffy,
						speed->a11clk_khz,
						a11clk_khz_new);
				}
				speed->a11clk_khz = a11clk_khz_new;
				speed->ahbclk_khz = speed->a11clk_khz / (speed->ahbclk_div+1);
				printk("OC: ADJUSTING FREQ TABLE freq=%d div=%d ahbclk=%d ahbdiv=%d\n", speed->a11clk_khz, speed->a11clk_src_div, speed->ahbclk_khz, speed->ahbclk_div);
			}
			if ((speed-> up < 0) && ((speed + 1)->a11clk_khz)) {
				// make sure all up entries are populated
				// because set_rate does not know how to jump					// in greater than 256mhz increments
				speed->up = speed - acpu_freq_tbl + 1;
			}
		}
	}

	for (speed = acpu_freq_tbl; speed->a11clk_khz != 0; speed++) {
		if (speed->a11clk_src_sel == sel
		 && (speed->a11clk_src_div == div))
			break;
	}
	if (speed->a11clk_khz == 0) {
		printk(KERN_WARNING "Warning - ACPU clock reports invalid speed\n");
		return;
	}

	drv_state.current_speed = speed;

	rc = clk_set_rate(ebi1_clk, speed->axiclk_khz * 1000);
	if (rc < 0)
		pr_err("Setting AXI min rate failed!\n");

	printk(KERN_INFO "ACPU running at %d KHz\n", speed->a11clk_khz);
}

unsigned long acpuclk_get_rate(void)
{
	WARN_ONCE(drv_state.current_speed == NULL,
		  "acpuclk_get_rate: not initialized\n");
	if (drv_state.current_speed)
		return drv_state.current_speed->a11clk_khz;
	else
		return 0;
}

uint32_t acpuclk_get_switch_time(void)
{
	return drv_state.acpu_switch_time_us;
}

/*----------------------------------------------------------------------------
 * Clock driver initialization
 *---------------------------------------------------------------------------*/

#define DIV2REG(n)		((n)-1)
#define REG2DIV(n)		((n)+1)
#define SLOWER_BY(div, factor)	div = DIV2REG(REG2DIV(div) * factor)

static void __init acpu_freq_tbl_fixup(void)
{
	unsigned long pll0_l, pll1_l, pll2_l;
	int axi_160mhz = 0, axi_200mhz = 0;
	struct pll_freq_tbl_map *lst;
	struct clkctl_acpu_speed *t;
	unsigned int pll0_needs_fixup = 0;

	/* Wait for the PLLs to be initialized and then read their frequency.
	 */
	do {
		pll0_l = readl(PLLn_L_VAL(0)) & 0x3f;
		cpu_relax();
		udelay(50);
	} while (pll0_l == 0);
	do {
		pll1_l = readl(PLLn_L_VAL(1)) & 0x3f;
		cpu_relax();
		udelay(50);
	} while (pll1_l == 0);
	do {
		pll2_l = readl(PLLn_L_VAL(2)) & 0x3f;
		cpu_relax();
		udelay(50);
	} while (pll2_l == 0);

	printk(KERN_INFO "L val: PLL0: %d, PLL1: %d, PLL2: %d\n",
				(int)pll0_l, (int)pll1_l, (int)pll2_l);

	/* Some configurations run PLL0 twice as fast. Instead of having
	 * separate tables for this case, we simply fix up the ACPU clock
	 * source divider since it's a simple fix up.
	 */
	if (pll0_l == PLL_491_MHZ) {
		pll0_l = PLL_245_MHZ;
		pll0_needs_fixup = 1;
	}

	/* Select the right table to use. */
	for (lst = acpu_freq_tbl_list; lst->tbl != 0; lst++) {
		if (lst->pll0_l == pll0_l && lst->pll1_l == pll1_l
		    && lst->pll2_l == pll2_l) {
			acpu_freq_tbl = lst->tbl;
			break;
		}
	}

	if (acpu_freq_tbl == NULL) {
		pr_crit("Unknown PLL configuration!\n");
		BUG();
	}

	/* Fix up PLL0 source divider if necessary. Also, fix up the AXI to
	 * the max that's supported by the board (RAM used in board).
	 */
	axi_160mhz = (pll0_l == PLL_960_MHZ || pll1_l == PLL_960_MHZ);
	axi_200mhz = (pll2_l == PLL_1200_MHZ);
	for (t = &acpu_freq_tbl[0]; t->a11clk_khz != 0; t++) {

		if (pll0_needs_fixup && t->pll == ACPU_PLL_0)
			SLOWER_BY(t->a11clk_src_div, 2);
		if (axi_160mhz && drv_state.max_axi_khz >= 160000
		    && t->ahbclk_khz > 128000)
			t->axiclk_khz = 160000;
		if (axi_200mhz && drv_state.max_axi_khz >= 200000
		    && t->ahbclk_khz > 160000)
			t->axiclk_khz = 200000;
	}

	t--;
	if (!axi_160mhz)
		pr_info("Turbo mode not supported.\n");
	else if (t->axiclk_khz == 160000)
		pr_info("Turbo mode supported and enabled.\n");
	else
		pr_info("Turbo mode supported but not enabled.\n");
}

/* Initalize the lpj field in the acpu_freq_tbl. */
static void __init lpj_init(void)
{
	int i;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;
	for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {
		acpu_freq_tbl[i].lpj = cpufreq_scale(loops_per_jiffy,
						base_clk->a11clk_khz,
						acpu_freq_tbl[i].a11clk_khz);
	}
}

static void __init precompute_stepping(void)
{
	int i, step_idx, step_same_pll_idx;

#define cur_freq acpu_freq_tbl[i].a11clk_khz
#define step_freq acpu_freq_tbl[step_idx].a11clk_khz
#define cur_pll acpu_freq_tbl[i].pll
#define step_pll acpu_freq_tbl[step_idx].pll

	for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {

		/* Calculate "Up" step. */
		step_idx = i + 1;
		step_same_pll_idx = -1;
		while (step_freq && (step_freq - cur_freq)
					<= drv_state.max_speed_delta_khz) {
			if (step_pll == cur_pll)
				step_same_pll_idx = step_idx;
			step_idx++;
		}

		/* Highest freq within max_speed_delta_khz. No step needed. */
		if (step_freq == 0)
			acpu_freq_tbl[i].up = -1;
		else if (step_idx == (i + 1)) {
			pr_crit("Delta between freqs %u KHz and %u KHz is"
				" too high!\n", cur_freq, step_freq);
			BUG();
		} else {
			/* There is only one TCXO freq. So don't complain. */
			if (cur_pll == ACPU_PLL_TCXO)
				step_same_pll_idx = step_idx - 1;
			if (step_same_pll_idx == -1) {
				pr_warning("Suboptimal up stepping for CPU "
					   "freq %u KHz.\n", cur_freq);
				acpu_freq_tbl[i].up = step_idx - 1;
			} else
				acpu_freq_tbl[i].up = step_same_pll_idx;
		}

		/* Calculate "Down" step. */
		step_idx = i - 1;
		step_same_pll_idx = -1;
		while (step_idx >= 0 && (cur_freq - step_freq)
					<= drv_state.max_speed_delta_khz) {
			if (step_pll == cur_pll)
				step_same_pll_idx = step_idx;
			step_idx--;
		}

		/* Lowest freq within max_speed_delta_khz. No step needed. */
		if (step_idx == -1)
			acpu_freq_tbl[i].down = -1;
		else if (step_idx == (i - 1)) {
			pr_crit("Delta between freqs %u KHz and %u KHz is"
				" too high!\n", cur_freq, step_freq);
			BUG();
		} else {
			if (step_same_pll_idx == -1) {
				pr_warning("Suboptimal down stepping for CPU "
					   "freq %u KHz.\n", cur_freq);
				acpu_freq_tbl[i].down = step_idx + 1;
			} else
				acpu_freq_tbl[i].down = step_same_pll_idx;
		}
	}
}

static void __init print_acpu_freq_tbl(void)
{
	struct clkctl_acpu_speed *t;
	pr_info("CPU-Freq  PLL  DIV  AHB-Freq  ADIV  AXI-Freq Dn Up\n");
	for (t = &acpu_freq_tbl[0]; t->a11clk_khz != 0; t++)
		pr_info("%8d  %3d  %3d  %8d  %4d  %8d %2d %2d\n",
			t->a11clk_khz, t->pll, t->a11clk_src_div + 1,
			t->ahbclk_khz, t->ahbclk_div + 1, t->axiclk_khz,
			t->down, t->up);
}

void __init msm_acpu_clock_init(struct msm_acpu_clock_platform_data *clkdata)
{
	pr_info("acpu_clock_init()\n");

	ebi1_clk = clk_get(NULL, "ebi1_clk");

	mutex_init(&drv_state.lock);
	drv_state.acpu_switch_time_us = clkdata->acpu_switch_time_us;
	drv_state.max_speed_delta_khz = clkdata->max_speed_delta_khz;
	drv_state.vdd_switch_time_us = clkdata->vdd_switch_time_us;
	drv_state.power_collapse_khz = clkdata->power_collapse_khz;
	drv_state.wait_for_irq_khz = clkdata->wait_for_irq_khz;
	//drv_state.max_axi_khz = clkdata->max_axi_khz;
	acpu_freq_tbl_fixup();
	precompute_stepping();
	acpuclk_init();
	lpj_init();
	print_acpu_freq_tbl();
#if defined(CONFIG_MSM_CPU_FREQ_ONDEMAND) || \
    defined(CONFIG_MSM_CPU_FREQ_USERSPACE) || \
    defined(CONFIG_MSM_CPU_FREQ_MSM7K)
	cpufreq_table_init();
	cpufreq_frequency_table_get_attr(freq_table, smp_processor_id());
#endif
}

unsigned long acpuclk_get_max_rate_override(void)
{
	return oc_freq_khz;
}

#if defined(CONFIG_DEBUG_FS)
// Read the custom VDDs. They have to be seperated by a ',' and with \0 exactly nbr_vdd(Number config lines in the Table)*2
static ssize_t acpu_vdd_fops_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{	
	struct msm_rpc_endpoint	*ept;
	int rc = 0, i;
	uint8_t val;
	void *k_buffer;
	char *data_pnt;
	char *token=NULL; 
	ept = (struct msm_rpc_endpoint *) filp->private_data;

	k_buffer = kmalloc(count, GFP_KERNEL);
	if (!k_buffer)
		return -ENOMEM;

	if (copy_from_user(k_buffer, buf, count)) {
		rc = -EFAULT;
		goto write_out_free;
	}

	if (count!=nbr_vdd*2) {
		rc = -EFAULT;
		goto write_out_free;
	}
	
	data_pnt = k_buffer;
	token=strsep(&data_pnt, ",");
	for(i=0; token!=NULL && i<nbr_vdd; i++) {
		val=simple_strtoul(token, NULL, 10);
		if(val>user_vdd_max||val<0){
			rc = -EFAULT;
			goto write_out_free;
		}
		vdd_user_data[i]=val;
		token=strsep(&data_pnt, ",");
	}
	user_vdd = 1;
	rc = count;
write_out_free:
	kfree(k_buffer);
	return rc;
}

// Write the active VDDs. They are seperated by a ','
static ssize_t acpu_vdd_fops_read(struct file *file, char __user * buf,
		                size_t len, loff_t * ppos)
{
	char k_buffer[nbr_vdd*2];
	int i=0, j=0;
	struct clkctl_acpu_speed *tgt_s;
	if(user_vdd) {
		for(j=0; j<nbr_vdd; j++){
			sprintf(&k_buffer[i], "%d", vdd_user_data[j]);
			k_buffer[i+1]=',';
			i+=2;
		}
	}
	else {
		for (tgt_s = acpu_freq_tbl; tgt_s->a11clk_khz != 0; tgt_s++) {
			sprintf(&k_buffer[i], "%d", tgt_s->vdd);
			k_buffer[i+1]=',';
			i+=2;
		}
	}
	k_buffer[nbr_vdd*2-1]= '\0';
	if (len < sizeof (k_buffer))
		return -EINVAL;
	return simple_read_from_buffer(buf, len, ppos, k_buffer,
				       sizeof (k_buffer));
}


static struct file_operations acpu_vdd_fops = {
	.write = acpu_vdd_fops_write,
	.read = acpu_vdd_fops_read,
 };

static int acpu_vdd_reset_get(void *dat, u64 *val) {
	return 0;
}

// Resets the custom VDDs to default Values
static int acpu_vdd_reset_set(void *dat, u64 val)
{
	user_vdd=0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(acpu_vdd_reset_fops,
		acpu_vdd_reset_get,
		acpu_vdd_reset_set, "%llu\n");

static int __init acpu_dbg_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("acpu_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("acpu_vdd", 0644, dent, NULL,
			    &acpu_vdd_fops);
			    
	debugfs_create_file("acpu_vdd_reset", 0644, dent, NULL,
			&acpu_vdd_reset_fops);

	return 0;
}

device_initcall(acpu_dbg_init);

#endif
