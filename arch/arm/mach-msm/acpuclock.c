/* arch/arm/mach-msm/acpuclock.c
 *
 * MSM architecture clock driver
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
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
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <linux/debugfs.h>
#include <linux/poll.h>

#include "proc_comm.h"
#include "acpuclock.h"

enum {
	PERF_SWITCH_DEBUG = 1U << 0,
	PERF_SWITCH_STEP_DEBUG = 1U << 1,
	PERF_SWITCH_PLL_DEBUG = 1U << 2,
	PERF_SWITCH_VDD_DEBUG = 1U << 3,
};

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
};

static struct clk *ebi1_clk;
static struct clock_state drv_state = { 0 };
static void __init acpuclk_init(void);

/* MSM7201A Levels 3-6 all correspond to 1.2V, level 7 corresponds to 1.325V. */
enum {
	VDD_0 = 0,
	VDD_1 = 1,
	VDD_2 = 2,
	VDD_3 = 3,
	VDD_4 = 4,
	VDD_5 = 5,
	VDD_6 = 6,
	VDD_7 = 7,
	VDD_END
};

struct clkctl_acpu_speed {
	unsigned int	a11clk_khz;
	int		pll;
	unsigned int	a11clk_src_sel;
	unsigned int	a11clk_src_div;
	unsigned int	ahbclk_khz;
	unsigned int	ahbclk_div;
	int		vdd;
	unsigned int	axiclk_khz;
	unsigned long	lpj; /* loops_per_jiffy */
/* Index in acpu_freq_tbl[] for steppings. */
	short		down;
	short		up;
};

/*
 * ACPU speed table. Complete table is shown but certain speeds are commented
 * out to optimized speed switching. Initalize loops_per_jiffy to 0.
 *
 * Table stepping up/down is optimized for 256mhz jumps while staying on the
 * same PLL.
 */
#if (0)
static struct clkctl_acpu_speed  acpu_freq_tbl[] = {
	{ 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, VDD_0, 30720, 0, 0, 8 },
	{ 61440, ACPU_PLL_0,  4, 3, 61440,  0, VDD_0, 30720,  0, 0, 8 },
	{ 81920, ACPU_PLL_0,  4, 2, 40960,  1, VDD_0, 61440,  0, 0, 8 },
	{ 96000, ACPU_PLL_1,  1, 7, 48000,  1, VDD_0, 61440,  0, 0, 9 },
	{ 122880, ACPU_PLL_0, 4, 1, 61440,  1, VDD_3, 61440,  0, 0, 8 },
	{ 128000, ACPU_PLL_1, 1, 5, 64000,  1, VDD_3, 61440,  0, 0, 12 },
	{ 176000, ACPU_PLL_2, 2, 5, 88000,  1, VDD_3, 61440,  0, 0, 11 },
	{ 192000, ACPU_PLL_1, 1, 3, 64000,  2, VDD_3, 61440,  0, 0, 12 },
	{ 245760, ACPU_PLL_0, 4, 0, 81920,  2, VDD_4, 61440,  0, 0, 12 },
	{ 256000, ACPU_PLL_1, 1, 2, 128000, 2, VDD_5, 128000, 0, 0, 12 },
	{ 264000, ACPU_PLL_2, 2, 3, 88000,  2, VDD_5, 128000, 0, 6, 13 },
	{ 352000, ACPU_PLL_2, 2, 2, 88000,  3, VDD_5, 128000, 0, 6, 13 },
	{ 384000, ACPU_PLL_1, 1, 1, 128000, 2, VDD_6, 128000, 0, 5, -1 },
	{ 528000, ACPU_PLL_2, 2, 1, 132000, 3, VDD_7, 128000, 0, 11, -1 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};
#else /* Table of freq we currently use. */
#if defined(CONFIG_TURBO_MODE)
/* 7200a turbo mode, PLL0(mpll):245.76, PLL1(gpll):960, PLL2(bpll0):1056 */
static struct clkctl_acpu_speed  acpu_freq_tbl[] = {
	{ 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, VDD_0, 30720, 0, 0, 4 },
	{ 122880, ACPU_PLL_0, 4, 1, 61440, 1, VDD_3, 61440, 0, 0, 4 },
#if 1 /* QCT fixup */
	{ 160000, ACPU_PLL_1, 1, 5, 53333, 2, VDD_3, 61440, 0, 0, 6 },
#else /* Google */
	{ 160000, ACPU_PLL_1, 1, 5, 64000, 1, VDD_3, 61440, 0, 0, 6 },
#endif
	{ 176000, ACPU_PLL_2, 2, 5, 88000, 1, VDD_3, 61440, 0, 0, 5 },
	{ 245760, ACPU_PLL_0, 4, 0, 81920, 2, VDD_4, 61440, 0, 0, 5 },
	{ 352000, ACPU_PLL_2, 2, 2, 88000, 3, VDD_5, 128000, 0, 3, 7 },
#if 1 /* QCT fixup */
	{ 480000, ACPU_PLL_1, 1, 1, 120000, 3, VDD_6, 120000, 0, 2, -1 },
#else /* Google */
	{ 480000, ACPU_PLL_1, 1, 1, 128000, 2, VDD_6, 160000, 0, 2, -1 },
#endif
	{ 528000, ACPU_PLL_2, 2, 1, 132000, 3, VDD_7, 160000, 0, 5, -1 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};
#else
static struct clkctl_acpu_speed  acpu_freq_tbl[] = {
	{ 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, VDD_0, 30720, 0, 0, 4 },
	{ 122880, ACPU_PLL_0, 4, 1, 61440, 1, VDD_3, 61440, 0, 0, 4 },
	{ 128000, ACPU_PLL_1, 1, 5, 64000, 1, VDD_3, 61440, 0, 0, 6 },
	{ 176000, ACPU_PLL_2, 2, 5, 88000, 1, VDD_3, 61440, 0, 0, 5 },
	{ 245760, ACPU_PLL_0, 4, 0, 81920, 2, VDD_4, 61440, 0, 0, 5 },
	{ 352000, ACPU_PLL_2, 2, 2, 88000, 3, VDD_5, 128000, 0, 3, 7 },
	{ 384000, ACPU_PLL_1, 1, 1, 128000, 2, VDD_6, 128000, 0, 2, -1 },
	{ 528000, ACPU_PLL_2, 2, 1, 132000, 3, VDD_7, 128000, 0, 5, -1 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};
#endif
#endif

const uint8_t nbr_vdd = 8;
static uint8_t vdd_user_data[8];
static uint8_t user_vdd = 0;
static uint8_t user_vdd_max = VDD_7;

#if defined(CONFIG_MSM_CPU_FREQ_ONDEMAND) || \
    defined(CONFIG_MSM_CPU_FREQ_USERSPACE) || \
    defined(CONFIG_MSM_CPU_FREQ_MSM7K)
#if defined(CONFIG_TURBO_MODE)
static struct cpufreq_frequency_table freq_table[] = {
	{ 0, 19200 },
	{ 1, 122880 },
	{ 2, 160000 },
	{ 3, 245760 },
	{ 4, 480000 },
	{ 5, 528000 },
	{ 6, CPUFREQ_TABLE_END },
};
#else
static struct cpufreq_frequency_table freq_table[] = {
	{ 0, 19200 },
	{ 1, 122880 },
	{ 2, 128000 },
	{ 3, 245760 },
	{ 4, 384000 },
	{ 5, 528000 },
	{ 6, CPUFREQ_TABLE_END },
};
#endif
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
	acpuclk_set_rate(drv_state.power_collapse_khz, 1);
	return ret * 1000;
}

unsigned long acpuclk_wait_for_irq(void) {
	int ret = acpuclk_get_rate();
	acpuclk_set_rate(drv_state.wait_for_irq_khz, 1);
	return ret * 1000;
}

static int acpuclk_set_vdd_level(int vdd)
{
	uint32_t current_vdd;

	current_vdd = readl(A11S_VDD_SVS_PLEVEL_ADDR) & 0x07;

	if (acpu_debug_mask & PERF_SWITCH_VDD_DEBUG)
		printk(KERN_DEBUG "acpuclock: Switching VDD from %u -> %d\n",
			current_vdd, vdd);

	writel((1 << 7) | (vdd << 3), A11S_VDD_SVS_PLEVEL_ADDR);
	udelay(drv_state.vdd_switch_time_us);
	if ((readl(A11S_VDD_SVS_PLEVEL_ADDR) & 0x7) != vdd) {
		if (acpu_debug_mask & PERF_SWITCH_VDD_DEBUG)
			printk(KERN_ERR "acpuclock: VDD set failed\n");
		return -EIO;
	}

	if (acpu_debug_mask & PERF_SWITCH_VDD_DEBUG)
		printk(KERN_DEBUG "acpuclock: VDD switched\n");
	return 0;
}

/* Set proper dividers for the given clock speed. */
static void acpuclk_set_div(const struct clkctl_acpu_speed *hunt_s) {
	uint32_t reg_clkctl, reg_clksel, clk_div;

	/* AHB_CLK_DIV */
	clk_div = (readl(A11S_CLK_SEL_ADDR) >> 1) & 0x03;
	/*
	 * If the new clock divider is higher than the previous, then
	 * program the divider before switching the clock
	 */
	if (hunt_s->ahbclk_div > clk_div) {
		reg_clksel = readl(A11S_CLK_SEL_ADDR);
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel(reg_clksel, A11S_CLK_SEL_ADDR);
	}
	if ((readl(A11S_CLK_SEL_ADDR) & 0x01) == 0) {
		/* SRC0 */

		/* Program clock source */
		reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
		reg_clkctl &= ~(0x07 << 4);
		reg_clkctl |= (hunt_s->a11clk_src_sel << 4);
		writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

		/* Program clock divider */
		reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
		reg_clkctl &= ~0xf;
		reg_clkctl |= hunt_s->a11clk_src_div;
		writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

		/* Program clock source selection */
		reg_clksel = readl(A11S_CLK_SEL_ADDR);
		reg_clksel |= 1; /* CLK_SEL_SRC1NO  == SRC1 */
		writel(reg_clksel, A11S_CLK_SEL_ADDR);
	} else {
		/* SRC1 */

		/* Program clock source */
		reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
		reg_clkctl &= ~(0x07 << 12);
		reg_clkctl |= (hunt_s->a11clk_src_sel << 12);
		writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

		/* Program clock divider */
		reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
		reg_clkctl &= ~(0xf << 8);
		reg_clkctl |= (hunt_s->a11clk_src_div << 8);
		writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

		/* Program clock source selection */
		reg_clksel = readl(A11S_CLK_SEL_ADDR);
		reg_clksel &= ~1; /* CLK_SEL_SRC1NO  == SRC0 */
		writel(reg_clksel, A11S_CLK_SEL_ADDR);
	}

	/*
	 * If the new clock divider is lower than the previous, then
	 * program the divider after switching the clock
	 */
	if (hunt_s->ahbclk_div < clk_div) {
		reg_clksel = readl(A11S_CLK_SEL_ADDR);
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel(reg_clksel, A11S_CLK_SEL_ADDR);
	}
}

int acpuclk_set_rate(unsigned long rate, int for_power_collapse)
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

	if(user_vdd)	// Switch to the user VREG
		v_val = vdd_user_data[tgt_s-acpu_freq_tbl];
	else
		v_val = tgt_s->vdd;

	/* Choose the highest speed speed at or below 'rate' with same PLL. */
	if (for_power_collapse && tgt_s->a11clk_khz < cur_s->a11clk_khz) {
		while (tgt_s->pll != ACPU_PLL_TCXO && tgt_s->pll != cur_s->pll)
			tgt_s--;
	}

	if (strt_s->pll != ACPU_PLL_TCXO)
		plls_enabled |= 1 << strt_s->pll;

	if (!for_power_collapse) {
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
		if (tgt_s->vdd > cur_s->vdd) {
			if ((rc = acpuclk_set_vdd_level(tgt_s->vdd)) < 0) {
				printk(KERN_ERR "Unable to switch ACPU vdd\n");
				goto out;
			}
		}
	}

	/* Set wait states for CPU inbetween frequency changes */
	reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
	reg_clkctl |= (100 << 16); /* set WT_ST_CNT */
	writel(reg_clkctl, A11S_CLK_CNTL_ADDR);

	if (acpu_debug_mask & PERF_SWITCH_DEBUG)
		printk(KERN_INFO "%s: Switching from ACPU rate %u -> %u\n",
			__func__, strt_s->a11clk_khz * 1000,
			tgt_s->a11clk_khz * 1000);

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
		if (acpu_debug_mask & PERF_SWITCH_STEP_DEBUG)
			printk(KERN_DEBUG "%s: STEP khz = %u, pll = %d\n",
				__func__, cur_s->a11clk_khz, cur_s->pll);

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

	/* Nothing else to do for power collapse. */
	if (for_power_collapse)
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

	/* Change the AXI bus frequency if we can. */
	/* Don't change it at power collapse, it will cause stability issue. */
	if (strt_s->axiclk_khz != tgt_s->axiclk_khz) {
		rc = clk_set_rate(ebi1_clk, tgt_s->axiclk_khz * 1000);
		if (rc < 0)
			pr_err("Setting AXI min rate failed!\n");
	}

	/* Drop VDD level if we can. */
	if (v_val < strt_s->vdd) {
		if (acpuclk_set_vdd_level(v_val) < 0)
			printk(KERN_ERR "acpuclock: Unable to drop ACPU vdd\n");
	}

	if (acpu_debug_mask & PERF_SWITCH_DEBUG)
		printk(KERN_DEBUG "%s: ACPU speed change complete\n",
				__func__);
out:
	if (!for_power_collapse)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __init acpuclk_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel;
	int rc;
       
	unsigned int a11clk_khz_new;
	struct cpufreq_frequency_table *freq_tbl_entry;
	uint32_t reg_clkctl;

	if (oc_freq_khz) {
		// make sure target freq is multpile of 19.2mhz
		oc_freq_khz = (oc_freq_khz / 19200) * 19200;

	        // set pll2 frequency
		writel(oc_freq_khz / 19200, MSM_CLK_CTL_BASE+0x33c);
	        udelay(50);

		// for overclocking we will set pll2 to a 1 divider
		// to have headroom over the max default 1.2ghz/2 setting
		reg_clkctl = readl(A11S_CLK_CNTL_ADDR);
		if (!(readl(A11S_CLK_SEL_ADDR) & 0x01)) { /* CLK_SEL_SRC1N0 */
			/* CLK_SRC0_SEL */
	                if (ACPU_PLL_2 == ((reg_clkctl >> 12) & 0x7)) {
	               		/* CLK_SRC0_DIV */
	                	if ((reg_clkctl >> 8) & 0x0f) {
					reg_clkctl &= ~(0xf << 8);
					writel(reg_clkctl, A11S_CLK_CNTL_ADDR);
					udelay(50);
				}
		}
		} else {
			/* CLK_SRC1_SEL */
	                if (ACPU_PLL_2 == ((reg_clkctl >> 4) & 0x07)) {
	                	/* CLK_SRC1_DIV */
		                if (reg_clkctl & 0x0f) {
					reg_clkctl &= ~0xf;
					writel(reg_clkctl, A11S_CLK_CNTL_ADDR);
					udelay(50);
				}
			}
		}

		// adjust pll2 frequencies
		for (speed = acpu_freq_tbl; speed->a11clk_khz != 0; speed++) {
			if (speed->pll == ACPU_PLL_2) {
				speed->a11clk_src_div = (speed->a11clk_src_div + 2) /2 - 1;
				a11clk_khz_new = oc_freq_khz / (speed->a11clk_src_div + 1);

				for (freq_tbl_entry = freq_table; freq_tbl_entry->frequency != CPUFREQ_TABLE_END; freq_tbl_entry++) {
					if (freq_tbl_entry->frequency == speed->a11clk_khz) {
						printk("OC: ADJUST freq_tbl_entry: %d to %d\n", freq_tbl_entry->frequency, a11clk_khz_new);
						freq_tbl_entry->frequency = a11clk_khz_new;
						break;
					}
				}	
				speed->a11clk_khz = a11clk_khz_new;
				speed->ahbclk_khz = speed->a11clk_khz / (speed->ahbclk_div+1);
				speed->axiclk_khz = 160000;
				printk("OC: ADJUSTING FREQ TABLE freq=%d div=%d ahbclk=%d ahbdiv=%d\n", speed->a11clk_khz, speed->a11clk_src_div, speed->ahbclk_khz, speed->ahbclk_div);
			}
		}
	}
 
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

unsigned long acpuclk_get_ebi1(unsigned long acpu_rate)
{
	int i;

	for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {
		if (acpu_freq_tbl[i].a11clk_khz == (acpu_rate / 1000))
			break;
	}
	return acpu_freq_tbl[i].axiclk_khz * 1000;
}

/*----------------------------------------------------------------------------
 * Clock driver initialization
 *---------------------------------------------------------------------------*/

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
	acpuclk_init();
	lpj_init();
#if defined(CONFIG_MSM_CPU_FREQ_ONDEMAND) || \
    defined(CONFIG_MSM_CPU_FREQ_USERSPACE) || \
    defined(CONFIG_MSM_CPU_FREQ_MSM7K)
	cpufreq_frequency_table_get_attr(freq_table, smp_processor_id());
#endif
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
