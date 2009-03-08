/* arch/arm/mach-msm/clock.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
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
#include <linux/module.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include <mach/msm_iomap.h>
#include <asm/io.h>

#include "clock.h"
#include "proc_comm.h"

static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clocks_lock);
static LIST_HEAD(clocks);

#if 1
#define D(x...) printk(KERN_DEBUG "clock-wince: " x)
#else
#define D(x...) do {} while (0)
#endif

struct mdns_clock_params
{
	unsigned long freq;
	uint32_t md;
	uint32_t ns;
};

struct msm_clock_params
{
	unsigned clk_id;
	unsigned idx;
	unsigned offset;  // Offset points to .ns register
	unsigned ns_only; // value to fill in ns register, rather than using mdns_clock_params look-up table
};

struct msm_clock_params msm_clock_parameters[] = {
	// Full ena/md/ns clock
	{ .clk_id = SDC1_CLK, .idx =  7, .offset = 0xa4, },
	{ .clk_id = SDC2_CLK, .idx =  8, .offset = 0xac, },
	{ .clk_id = SDC3_CLK, .idx = 27, .offset = 0xb4, },
	{ .clk_id = SDC4_CLK, .idx = 28, .offset = 0xbc, },
	{ .clk_id = UART1DM_CLK, .idx = 17, .offset = 0xd4, },
	{ .clk_id = UART2DM_CLK, .idx = 26, .offset = 0xdc, },

	{ .clk_id = USB_HS_CLK, .idx = 25, .offset = 0x2c0, .ns_only = 0xb00 },
	{ .clk_id = GRP_CLK, .idx = 3, .offset = 0x84, .ns_only = 0xa80 },

	// MD/NS only; offset = Ns reg
	{ .clk_id = VFE_CLK, .offset = 0x44 },
	
	// Enable bit only; bit = 1U << idx
	{ .clk_id = MDP_CLK, .idx = 9, },
	
	// NS-reg only; offset = Ns reg, ns_only = Ns value
	{ .clk_id = GP_CLK, .offset = 0x5c, .ns_only = 0xa06 },
	{ .clk_id = PMDH_CLK, .offset = 0x8c, .ns_only = 0xa00, },
	{ .clk_id = I2C_CLK, .offset = 0x64, .ns_only = 0xa00, },
};

// This formula is used to generate md and ns reg values
#define MSM_CLOCK_REG(frequency,a1,a2,a3,a4,a5,a6,a7) { \
	.freq = (frequency), \
	.md = ((0xffff & (a1)) << 16) | (0xffff & ~((a3) << 1)), \
	.ns = ((0xffff & ~((a2) - (a1))) << 16) \
	    | ((0xff & (0xa | (a7))) << 8) \
	    | ((0x7 & (a5)) << 5) \
	    | ((0x3 & (a4)) << 3) \
	    | (0x3 & (a6)), \
}


struct mdns_clock_params msm_clock_freq_parameters[] = {
	MSM_CLOCK_REG(  144000, 3, 0x64, 0x32, 3, 3, 0, 1), /* 144kHz */
	MSM_CLOCK_REG(12000000, 1, 0x20, 0x10, 1, 3, 1, 1), /* 12MHz */
	MSM_CLOCK_REG(19200000, 1, 0x0a, 0x05, 3, 3, 1, 1), /* 19.2MHz */
	MSM_CLOCK_REG(24000000, 1, 0x10, 0x08, 1, 3, 1, 1), /* 24MHz */
	MSM_CLOCK_REG(32000000, 1, 0x0c, 0x06, 1, 3, 1, 1), /* 32MHz */
};

static inline struct msm_clock_params msm_clk_get_params(uint32_t id)
{
	int i;
	struct msm_clock_params empty = { };
	for (i = 0; i < ARRAY_SIZE(msm_clock_parameters); i++) {
		if (id == msm_clock_parameters[i].clk_id) {
			return msm_clock_parameters[i];
		}
	}
	return empty;
}

static inline uint32_t msm_clk_enable_bit(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	if (!params.idx) return 0;
	return 1U << params.idx;
}

static inline unsigned msm_clk_reg_offset(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	return params.offset;
}

static int set_mdns_host_clock(uint32_t id, unsigned long freq)
{
	int n;
	unsigned offset;
	int retval;
	bool found;
	struct msm_clock_params params;
	found = 0;
	retval = -EINVAL;
	
	params = msm_clk_get_params(id);
	offset = params.offset;

	D("set mdns: %u, %lu; bitidx=%u, offset=%x, ns=%x\n", id, freq, 
	  params.idx, params.offset, params.ns_only);

	if (!params.offset)
	{
		printk(KERN_WARNING "%s: Don't know how to set clock %u - no known Md/Ns reg\n", __func__, id);
		return -ENOTSUPP;
	}

	// Turn off clock-enable bit if supported
	if (params.idx > 0)
		writel(readl(MSM_CLK_CTL_BASE) & ~(1U << params.idx), MSM_CLK_CTL_BASE);

	if (params.ns_only > 0)
	{
		writel(params.ns_only, MSM_CLK_CTL_BASE + offset);
		found = 1;
		retval = 0;
	} else {
		for (n = ARRAY_SIZE(msm_clock_freq_parameters); n >= 0; n--) {
			if (freq >= msm_clock_freq_parameters[n].freq) {
				// This clock requires MD and NS regs to set frequency:
				writel(msm_clock_freq_parameters[n].md, MSM_CLK_CTL_BASE + offset - 4);
				writel(msm_clock_freq_parameters[n].ns, MSM_CLK_CTL_BASE + offset);
				msleep(5);
				D("%s: %u, freq=%lu\n", __func__, id, 
				  msm_clock_freq_parameters[n].freq );
				retval = 0;
				found = 1;
				break;
			}
		}
	}

	// Turn clock-enable bit back on, if supported
	if (params.idx > 0)
		writel(readl(MSM_CLK_CTL_BASE) | (1U << params.idx), MSM_CLK_CTL_BASE);

	if (!found) {
		printk(KERN_WARNING "clock-wince: set_sdcc_host_clock could not "
		       "find suitable parameter for freq %lu\n", freq);
	}

	return retval;
}

static unsigned long get_mdns_host_clock(uint32_t id)
{
	int n;
	unsigned offset;
	uint32_t mdreg;
	uint32_t nsreg;
	unsigned long freq = 0;

	offset = msm_clk_reg_offset(id);
	if (offset == 0)
		return -EINVAL;

	mdreg = readl(MSM_CLK_CTL_BASE + offset - 4);
	nsreg = readl(MSM_CLK_CTL_BASE + offset);

	for (n = 0; n < ARRAY_SIZE(msm_clock_freq_parameters); n++) {
		if (msm_clock_freq_parameters[n].md == mdreg &&
			msm_clock_freq_parameters[n].ns == nsreg) {
			freq = msm_clock_freq_parameters[n].freq;
			break;
		}
	}

	return freq;
}

#ifndef CONFIG_MSM_AMSS_VERSION_WINCE
static inline int pc_clk_enable(unsigned id)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_ENABLE, &id, NULL);
}

static inline void pc_clk_disable(unsigned id)
{
	msm_proc_comm(PCOM_CLKCTL_RPC_DISABLE, &id, NULL);
}

static inline int pc_clk_set_rate(unsigned id, unsigned rate)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_SET_RATE, &id, &rate);
}

static inline int pc_clk_set_min_rate(unsigned id, unsigned rate)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_MIN_RATE, &id, &rate);
}

static inline int pc_clk_set_max_rate(unsigned id, unsigned rate)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_MAX_RATE, &id, &rate);
}

static inline int pc_clk_set_flags(unsigned id, unsigned flags)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_SET_FLAGS, &id, &flags);
}

static inline unsigned pc_clk_get_rate(unsigned id)
{
	if (msm_proc_comm(PCOM_CLKCTL_RPC_RATE, &id, NULL))
		return 0;
	else
		return id;
}

static inline unsigned pc_clk_is_enabled(unsigned id)
{
	if (msm_proc_comm(PCOM_CLKCTL_RPC_ENABLED, &id, NULL))
		return 0;
	else
		return id;
}

static inline int pc_pll_request(unsigned id, unsigned on)
{
	on = !!on;
	return msm_proc_comm(PCOM_CLKCTL_RPC_PLL_REQUEST, &id, &on);
}
#else

static int pc_clk_enable(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);

	//XXX: too spammy, extreme debugging only: D(KERN_DEBUG "%s: %d\n", __func__, id);

	if (params.idx)
	{
		writel(readl(MSM_CLK_CTL_BASE) | (1U << params.idx), MSM_CLK_CTL_BASE);
		return 0;
	} else if (params.ns_only > 0 && params.offset)
	{
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | params.ns_only, MSM_CLK_CTL_BASE + params.offset);
		return 0;
	}
	printk(KERN_WARNING "%s: enabling a clock that doesn't have an ena bit "
	       "or ns-only offset: %u\n", __func__, id);
	return -1;
}

static void pc_clk_disable(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);

	//XXX: D(KERN_DEBUG "%s: %d\n", __func__, id);

	if (params.idx)
	{
		writel(readl(MSM_CLK_CTL_BASE) & ~(1U << params.idx), MSM_CLK_CTL_BASE);
	} else if (params.ns_only > 0 && params.offset)
	{
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~params.ns_only, MSM_CLK_CTL_BASE + params.offset);
	} else {
		printk(KERN_WARNING "%s: disabling a clock that doesn't have an "
		       "ena bit: %u\n", __func__, id);
	}
}

static int pc_clk_set_rate(uint32_t id, unsigned long rate)
{
	int retval;
	retval = 0;

	D("%s: id=%u rate=%lu\n", __func__, id, rate);

	retval = set_mdns_host_clock(id, rate);

	return retval;
}

static int pc_clk_set_min_rate(uint32_t id, unsigned long rate)
{
	printk(KERN_WARNING "clk_set_min_rate not implemented; %u:%lu\n", id, rate);
	return 0;
}

static int pc_clk_set_max_rate(uint32_t id, unsigned long rate)
{
	printk(KERN_WARNING "clk_set_max_rate not implemented; %u:%lu\n", id, rate);
	return 0;
}

static unsigned long pc_clk_get_rate(uint32_t id)
{
	unsigned long rate = 0;

	switch (id) {
		case SDC1_CLK:
		case SDC2_CLK:
		case SDC3_CLK:
		case SDC4_CLK:
			rate = get_mdns_host_clock(id);
			break;

		case SDC1_PCLK:
		case SDC2_PCLK:
		case SDC3_PCLK:
		case SDC4_PCLK:
			rate = 66000000;
			break;

		default:
			//TODO: support all clocks
			printk("unknown clock: %u\n", id);
			rate = 0;
	}

	return rate;
}

static int pc_clk_set_flags(uint32_t id, unsigned long flags)
{
	printk(KERN_WARNING "%s not implemented; id=%u, flags=%lu\n", __func__, id, flags);
	return 0;
}

static int pc_clk_is_enabled(uint32_t id)
{
	int is_enabled = 0;
	unsigned bit;
	bit = msm_clk_enable_bit(id);
	if (bit > 0)
	{
		is_enabled = (readl(MSM_CLK_CTL_BASE) & bit) != 0;
	}
	//XXX: is this necessary?
	if (id==SDC1_PCLK || id==SDC2_PCLK || id==SDC3_PCLK || id==SDC4_PCLK)
		is_enabled = 1;
	return is_enabled;
}

static int pc_pll_request(unsigned id, unsigned on)
{
	return 0;
}

#endif

/*
 * Standard clock functions defined in include/linux/clk.h
 */
struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *clk;

	mutex_lock(&clocks_mutex);

	list_for_each_entry(clk, &clocks, list)
		if (!strcmp(id, clk->name) && clk->dev == dev)
			goto found_it;

	list_for_each_entry(clk, &clocks, list)
		if (!strcmp(id, clk->name) && clk->dev == NULL)
			goto found_it;

	clk = ERR_PTR(-ENOENT);
found_it:
	mutex_unlock(&clocks_mutex);
	return clk;
}
EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_put);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	if (clk->id == ACPU_CLK)
	{
		return -ENOTSUPP;
	}
	spin_lock_irqsave(&clocks_lock, flags);
	clk->count++;
	if (clk->count == 1)
		pc_clk_enable(clk->id);
	spin_unlock_irqrestore(&clocks_lock, flags);
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&clocks_lock, flags);
	BUG_ON(clk->count == 0);
	clk->count--;
	if (clk->count == 0)
		pc_clk_disable(clk->id);
	spin_unlock_irqrestore(&clocks_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	return pc_clk_get_rate(clk->id);
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;
	if (clk->flags & CLKFLAG_USE_MIN_MAX_TO_SET) {
		ret = pc_clk_set_max_rate(clk->id, rate);
		if (ret)
			return ret;
		return pc_clk_set_min_rate(clk->id, rate);
	}
	return pc_clk_set_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	return ERR_PTR(-ENOSYS);
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_flags(struct clk *clk, unsigned long flags)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;
	return pc_clk_set_flags(clk->id, flags);
}
EXPORT_SYMBOL(clk_set_flags);


void __init msm_clock_init(void)
{
	unsigned n;

	spin_lock_init(&clocks_lock);
	mutex_lock(&clocks_mutex);
	for (n = 0; n < msm_num_clocks; n++)
		list_add_tail(&msm_clocks[n].list, &clocks);
	mutex_unlock(&clocks_mutex);
}

/* The bootloader and/or AMSS may have left various clocks enabled.
 * Disable any clocks that belong to us (CLKFLAG_AUTO_OFF) but have
 * not been explicitly enabled by a clk_enable() call.
 */
static int __init clock_late_init(void)
{
	unsigned long flags;
	struct clk *clk;
	unsigned count = 0;

	mutex_lock(&clocks_mutex);
	list_for_each_entry(clk, &clocks, list) {
		if (clk->flags & CLKFLAG_AUTO_OFF) {
			spin_lock_irqsave(&clocks_lock, flags);
			if (!clk->count) {
				count++;
				pc_clk_disable(clk->id);
			}
			spin_unlock_irqrestore(&clocks_lock, flags);
		}
	}
	mutex_unlock(&clocks_mutex);
	pr_info("clock_late_init() disabled %d unused clocks\n", count);
	return 0;
}

late_initcall(clock_late_init);
