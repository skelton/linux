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

enum {
	DEBUG_UNKNOWN_ID	= 1<<0,
	DEBUG_UNKNOWN_FREQ	= 1<<1,
	DEBUG_MDNS		= 1<<2,
	DEBUG_UNKNOWN_CMD	= 1<<3,
};
static int debug_mask=0;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

// easy defines...
#define REG(off) (MSM_CLK_CTL_BASE + (off))

/* Register offsets used more than once. */
/*#define USBH_MD						0x02BC
#define USBH_NS						0x02C0
#define USBH2_NS					0x046C
#define USBH3_NS					0x0470
#define CAM_VFE_NS				0x0044
#define GLBL_CLK_ENA_SC		0x03BC
#define GLBL_CLK_ENA_2_SC	0x03C0
#define SDAC_NS						0x009C
#define TV_NS							0x00CC
#define MI2S_RX_NS					0x0070
#define MI2S_TX_NS					0x0078
#define MI2S_NS						0x02E0
#define LPA_NS						0x02E8
#define MDC_NS						0x007C
#define MDP_VSYNC_REG		0x0460
#define PLL_ENA_REG				0x0260*/

#if 1
#define D(x...) printk(KERN_DEBUG "clock-wince: " x)
#else
#define D(x...) do {} while (0)
#endif

struct mdns_clock_params
{
	unsigned long freq;
	uint32_t calc_freq;
	uint32_t md;
	uint32_t ns;
	uint32_t pll_freq;
	uint32_t clk_id;
};

struct msm_clock_params
{
	unsigned clk_id;
	unsigned idx;
	unsigned offset;  // Offset points to .ns register
	unsigned ns_only; // value to fill in ns register, rather than using mdns_clock_params look-up table
	char	*name;
};

static int max_clk_rate[NR_CLKS], min_clk_rate[NR_CLKS];

#define PLLn_BASE(n)		(MSM_CLK_CTL_BASE + 0x300 + 28 * (n))
#define TCX0			19200000 // Hz
#define PLL_FREQ(l, m, n)	(TCX0 * (l) + TCX0 * (m) / (n))

static unsigned int pll_get_rate(int n)
{
	unsigned int mode, L, M, N, freq;

 if (n == -1) return TCX0;
 if (n > 3)
  return 0;
 else
 {
	mode = readl(PLLn_BASE(n) + 0x0);
	L = readl(PLLn_BASE(n) + 0x4);
	M = readl(PLLn_BASE(n) + 0x8);
	N = readl(PLLn_BASE(n) + 0xc);
	freq = PLL_FREQ(L, M, N);
	printk(KERN_INFO "PLL%d: MODE=%08x L=%08x M=%08x N=%08x freq=%u Hz (%u MHz)\n",
		n, mode, L, M, N, freq, freq / 1000000); \
 }

 return freq;
}

static unsigned int idx2pll(uint32_t idx)
{
 int ret;

 switch(idx)
 {
  case 0: /* TCX0 */
   ret=-1;
  break;
  case 1: /* PLL1 */
   ret=1;
  break;
  case 4: /* PLL0 */
   ret=0;
  break;
  default:
   ret=4; /* invalid */
 }

 return ret;
}
static struct msm_clock_params msm_clock_parameters[] = {
	// Full ena/md/ns clock
	{ .clk_id = SDC1_CLK, .idx =  7, .offset = 0xa4, .name="SDC1_CLK",},
	{ .clk_id = SDC2_CLK, .idx =  8, .offset = 0xac, .name="SDC2_CLK",},
	{ .clk_id = SDC3_CLK, .idx = 27, .offset = 0xb4, .name="SDC3_CLK",},
	{ .clk_id = SDC4_CLK, .idx = 28, .offset = 0xbc, .name="SDC4_CLK",},
	{ .clk_id = UART1DM_CLK, .idx = 17, .offset = 0xd4, .name="UART1DM_CLK",},
	{ .clk_id = UART2DM_CLK, .idx = 26, .offset = 0xdc, .name="UART2DM_CLK",},

	{ .clk_id = USB_HS_CLK, .idx = 25, .offset = 0x2c0, .ns_only = 0xb00, .name="USB_HS_CLK",},
	{ .clk_id = GRP_CLK, .idx = 3, .offset = 0x84, .ns_only = 0x4a80, .name="GRP_CLK", }, // these both enable the GRP and IMEM clocks.
	{ .clk_id = IMEM_CLK, .idx = 3, .offset = 0x84, .ns_only = 0x4a80, .name="IMEM_CLK", },


	// MD/NS only; offset = Ns reg
	{ .clk_id = VFE_CLK, .offset = 0x44, .name="VFE_CLK", },
	
	// Enable bit only; bit = 1U << idx
	{ .clk_id = MDP_CLK, .idx = 9, .name="MDP_CLK",},
 	
	
	// NS-reg only; offset = Ns reg, ns_only = Ns value
	{ .clk_id = GP_CLK, .offset = 0x5c, .ns_only = 0xa06, .name="GP_CLK" },
/*#if defined(CONFIG_MACH_HTCBLACKSTONE) || defined(CONFIG_MACH_HTCKOVSKY)
	{ .clk_id = PMDH_CLK, .offset = 0x8c, .ns_only = 0xa19, .name="PMDH_CLK"},
#else*/
	{ .clk_id = PMDH_CLK, .offset = 0x8c, .ns_only = 0xa0c, .name="PMDH_CLK"},
//#endif

	{ .clk_id = I2C_CLK, .offset = 0x64, .ns_only = 0xa00, .name="I2C_CLK"},
//	{ .clk_id = UART1_CLK, .offset = 0xe0, .ns_only = 0xa00, .name="UART1_CLK"},
};

void fix_mddi_clk_black() {
	msm_clock_parameters[12].ns_only=0xa19;
}

// This formula is used to generate md and ns reg values
#define MSM_CLOCK_REG(frequency,M,N,D,PRE,a5,SRC,MNE,pll_frequency) { \
	.freq = (frequency), \
	.md = ((0xffff & (M)) << 16) | (0xffff & ~((D) << 1)), \
	.ns = ((0xffff & ~((N) - (M))) << 16) \
	    | ((0xff & (0xa | (MNE))) << 8) \
	    | ((0x7 & (a5)) << 5) \
	    | ((0x3 & (PRE)) << 3) \
	    | (0x7 & (SRC)), \
	.pll_freq = (pll_frequency), \
	.calc_freq = (pll_frequency*M/((PRE+1)*N)), \
}

struct mdns_clock_params msm_clock_freq_parameters[] = {

	MSM_CLOCK_REG(  144000,   3, 0x64, 0x32, 3, 3, 0, 1, 19200000), /* SD, 144kHz */
#if 0 /* wince uses this clock setting for UART2DM */
	MSM_CLOCK_REG( 1843200,     3, 0x64, 0x32, 3, 2, 4, 1, 245760000), /*  115200*16=1843200 */
//	MSM_CLOCK_REG(            , 2, 0xc8, 0x64, 3, 2, 1, 1, 768888888), /* 1.92MHz for 120000 bps */
#else
	MSM_CLOCK_REG( 7372800,   3, 0x64, 0x32, 0, 2, 4, 1, 245760000), /*  460800*16, will be divided by 4 for 115200 */
#endif
	MSM_CLOCK_REG(12000000,   1, 0x20, 0x10, 1, 3, 1, 1, 768000000), /* SD, 12MHz */
	MSM_CLOCK_REG(14745600,   3, 0x32, 0x19, 0, 2, 4, 1, 245760000), /* BT, 921600 (*16)*/
	MSM_CLOCK_REG(19200000,   1, 0x0a, 0x05, 3, 3, 1, 1, 768000000), /* SD, 19.2MHz */
	MSM_CLOCK_REG(24000000,   1, 0x10, 0x08, 1, 3, 1, 1, 768000000), /* SD, 24MHz */
	MSM_CLOCK_REG(32000000,   1, 0x0c, 0x06, 1, 3, 1, 1, 768000000), /* SD, 32MHz */
	MSM_CLOCK_REG(58982400,   6, 0x19, 0x0c, 0, 2, 4, 1, 245760000), /* BT, 3686400 (*16) */
	MSM_CLOCK_REG(64000000,0x19, 0x60, 0x30, 0, 2, 4, 1, 245760000), /* BT, 4000000 (*16) */
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
	uint32_t nsreg;
	found = 0;
	retval = -EINVAL;
	
	params = msm_clk_get_params(id);
	offset = params.offset;

	if(debug_mask&DEBUG_MDNS)
		D("set mdns: %u, %lu; bitidx=%u, offset=%x, ns=%x\n", id, freq, 
	  params.idx, params.offset, params.ns_only);

	if (!params.offset)
	{
		printk(KERN_WARNING "%s: FIXME! Don't know how to set clock %u - no known Md/Ns reg\n", __func__, id);
		return -ENOTSUPP;
	}

	// Turn off clock-enable bit if supported
	if (params.idx > 0)
		writel(readl(MSM_CLK_CTL_BASE) & ~(1U << params.idx), MSM_CLK_CTL_BASE);

	if (params.ns_only > 0)
	{
		nsreg = readl(MSM_CLK_CTL_BASE + offset) & 0xfffff000;
		writel( nsreg | params.ns_only, MSM_CLK_CTL_BASE + offset);

		found = 1;
		retval = 0;

	} else {
		for (n = ARRAY_SIZE(msm_clock_freq_parameters)-1; n >= 0; n--) {
			if (freq >= msm_clock_freq_parameters[n].freq) {
				// This clock requires MD and NS regs to set frequency:
				writel(msm_clock_freq_parameters[n].md, MSM_CLK_CTL_BASE + offset - 4);
				writel(msm_clock_freq_parameters[n].ns, MSM_CLK_CTL_BASE + offset);
//				msleep(5);
				if(debug_mask&DEBUG_MDNS)
					D("%s: %u, freq=%lu calc_freq=%u pll%d=%u expected pll =%u\n", __func__, id, 
				  msm_clock_freq_parameters[n].freq,
				  msm_clock_freq_parameters[n].calc_freq,
				  msm_clock_freq_parameters[n].ns&7,
				  pll_get_rate(idx2pll(msm_clock_freq_parameters[n].ns&7)),
				  msm_clock_freq_parameters[n].pll_freq );
				retval = 0;
				found = 1;
				break;
			}
		}
	}

	// Turn clock-enable bit back on, if supported
	if (params.idx > 0)
		writel(readl(MSM_CLK_CTL_BASE) | (1U << params.idx), MSM_CLK_CTL_BASE);

	if (!found && debug_mask&DEBUG_UNKNOWN_FREQ) {
		printk(KERN_WARNING "clock-wince: FIXME! set_sdcc_host_clock could not "
		       "find suitable parameter for freq %lu\n", freq);
	}

//     return retval;
       return 0;
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

#define PRPH_WEB_NS_REG	0x0080
#define GRP_NS_REG				0x0084
#define AXI_RESET 					0x0208
#define ROW_RESET 				0x0214
#define VDD_GRP_GFS_CTL 	0x0284
#define MSM_RAIL_CLAMP_IO	0x0290

// starting from clock
static const char* REG_NAMES[] = 
{
	"GLBL_CLK_ENA", // 0x0
	"GLBL_CLK_STATE", // 0x4
	"GLBL_SRC0_NS_REG", // 0x8
	"GLBL_SRC1_NS_REG", // 0xC
	"GLBL_SRC_OUT_SEL", // 0x10
	"GLBL_CLK_DIV", // 0x14
	"GLBL_CLK_INV", // 0x18
	"GLBL_SLEEP_EN", // 0x1C
	"GLBL_SLEEP_EN2", // 0x20
	"DUAL_MODEM_NS_REG", // 0x24
	"EBIB_NS_REG", // 0x28
	"EBI1_NS_REG", // 0x2C
	"SMI_NS_REG", // 0x30
	"ADSP_NS_REG", // 0x34
	"BTPCM_MD_REG", // 0x38
	"BTPCM_NS_REG", // 0x3C
	"CAM_VFE_MD_REG", // 0x40
	"CAM_VFE_NS_REG", // 0x44
	"ECODEC_MD_REG", // 0x48
	"ECODEC_NS_REG", // 0x4C
	"EMDH_NS_REG", // 0x50
	"FUSE_NS_REG", // 0x54
	"GP_MD_REG", // 0x58
	"GP_NS_REG", // 0x5C
	"GSM_DAI_MD_REG", // 0x60
	"GSM_DAI_NS_REG", // 0x64
	"I2C_NS_REG", // 0x68
	"ICODEC_RX_MD_REG", // 0x6C
	"ICODEC_RX_NS_REG", // 0x70
	"ICODEC_TX_MD_REG", // 0x74
	"ICODEC_TX_NS_REG", // 0x78
	"MDC_NS_REG", // 0x7C
	"PRPH_WEB_NS_REG", // 0x80
	"GRP_NS_REG", // 0x84
	"PCM_NS_REG", // 0x88
	"PMDH_NS_REG", // 0x8C
	"QMEMBIST_NS_REG", // 0x90
	"SBI_NS_REG", // 0x94
	"SDAC_MD_REG", // 0x98
	"SDAC_NS_REG", // 0x9C
	"SDC1_MD_REG", // 0xA0
	"SDC1_NS_REG", // 0xA4
	"SDC2_MD_REG", // 0xA8
	"SDC2_NS_REG", // 0xAC
	"SDC3_MD_REG", // 0xB0
	"SDC3_NS_REG", // 0xB4
	"SDC4_MD_REG", // 0xB8
	"SDC4_NS_REG", // 0xBC
	"TLMM_NS_REG", // 0xC0
	"TSIF_NS_REG", // 0xC4
	"TV_MD_REG", // 0xC8
	"TV_NS_REG", // 0xCC
	"UART1DM_MD_REG", // 0xD0
	"UART1DM_NS_REG", // 0xD4
	"UART2DM_MD_REG", // 0xD8
	"UART2DM_NS_REG", // 0xDC
	"PUART_NS_REG", // 0xE0
	"USB_MD_REG", // 0xE4
	"USB_NS_REG", // 0xE8
	"VDC_MD_REG", // 0xEC
	"VDC_NS_REG", // 0xF0
	"MSM_CLK_RINGOSC", // 0xF4
	"TCXO_CNT", // 0xF8
	"RINGOSC_CNT", // 0xFC
	"ADSP_FS", // 0x100
	"MARM_FS", // 0x104
	"CLK_HALT_STATEA", // 0x108
	"CLK_HALT_STATEB", // 0x10C
	"MISC_CLK_CTL", // 0x110
	"PLLTEST_PAD_CFG", // 0x114
	"CLKTEST_PAD_CFG", // 0x118
	"PAD1[0X39]", // 0x11C
	"PAD1[0X39]", // 0x120
	"PAD1[0X39]", // 0x124
	"PAD1[0X39]", // 0x128
	"PAD1[0X39]", // 0x12C
	"PAD1[0X39]", // 0x130
	"PAD1[0X39]", // 0x134
	"PAD1[0X39]", // 0x138
	"PAD1[0X39]", // 0x13C
	"PAD1[0X39]", // 0x140
	"PAD1[0X39]", // 0x144
	"PAD1[0X39]", // 0x148
	"PAD1[0X39]", // 0x14C
	"PAD1[0X39]", // 0x150
	"PAD1[0X39]", // 0x154
	"PAD1[0X39]", // 0x158
	"PAD1[0X39]", // 0x15C
	"PAD1[0X39]", // 0x160
	"PAD1[0X39]", // 0x164
	"PAD1[0X39]", // 0x168
	"PAD1[0X39]", // 0x16C
	"PAD1[0X39]", // 0x170
	"PAD1[0X39]", // 0x174
	"PAD1[0X39]", // 0x178
	"PAD1[0X39]", // 0x17C
	"PAD1[0X39]", // 0x180
	"PAD1[0X39]", // 0x184
	"PAD1[0X39]", // 0x188
	"PAD1[0X39]", // 0x18C
	"PAD1[0X39]", // 0x190
	"PAD1[0X39]", // 0x194
	"PAD1[0X39]", // 0x198
	"PAD1[0X39]", // 0x19C
	"PAD1[0X39]", // 0x1A0
	"PAD1[0X39]", // 0x1A4
	"PAD1[0X39]", // 0x1A8
	"PAD1[0X39]", // 0x1AC
	"PAD1[0X39]", // 0x1B0
	"PAD1[0X39]", // 0x1B4
	"PAD1[0X39]", // 0x1B8
	"PAD1[0X39]", // 0x1BC
	"PAD1[0X39]", // 0x1C0
	"PAD1[0X39]", // 0x1C4
	"PAD1[0X39]", // 0x1C8
	"PAD1[0X39]", // 0x1CC
	"PAD1[0X39]", // 0x1D0
	"PAD1[0X39]", // 0x1D4
	"PAD1[0X39]", // 0x1D8
	"PAD1[0X39]", // 0x1DC
	"PAD1[0X39]", // 0x1E0
	"PAD1[0X39]", // 0x1E4
	"PAD1[0X39]", // 0x1E8
	"PAD1[0X39]", // 0x1EC
	"PAD1[0X39]", // 0x1F0
	"PAD1[0X39]", // 0x1F4
	"PAD1[0X39]", // 0x1F8
	"PAD1[0X39]", // 0x1FC
	"RESET_ALL", // 0x200
	"MSS_RESET", // 0x204
	"AXI_RESET", // 0x208
	"A11_RESET", // 0x20C
	"APPS_RESET", // 0x210
	"ROW_RESET", // 0x214
	"CLK_RESET", // 0x218
	"RESERVED_21C", // 0x21C
	"PAD2[0X18]", // 0x220
	"PAD2[0X18]", // 0x224
	"PAD2[0X18]", // 0x228
	"PAD2[0X18]", // 0x22C
	"PAD2[0X18]", // 0x230
	"PAD2[0X18]", // 0x234
	"PAD2[0X18]", // 0x238
	"PAD2[0X18]", // 0x23C
	"PAD2[0X18]", // 0x240
	"PAD2[0X18]", // 0x244
	"PAD2[0X18]", // 0x248
	"PAD2[0X18]", // 0x24C
	"PAD2[0X18]", // 0x250
	"PAD2[0X18]", // 0x254
	"PAD2[0X18]", // 0x258
	"PAD2[0X18]", // 0x25C
	"PAD2[0X18]", // 0x260
	"PAD2[0X18]", // 0x264
	"PAD2[0X18]", // 0x268
	"PAD2[0X18]", // 0x26C
	"PAD2[0X18]", // 0x270
	"PAD2[0X18]", // 0x274
	"PAD2[0X18]", // 0x278
	"PAD2[0X18]", // 0x27C
	"VDD_VFE_GFS_CTL", // 0x280
	"VDD_GRP_GFS_CTL", // 0x284
	"VDD_VDC_GFS_CTL", // 0x288
	"VDD_TSTM_GFS_CTL", // 0x28C
	"MSM_RAIL_CLAMP_IO", // 0x290
	"GFS_CTL_STATUS", // 0x294
	"VDD_APC_PLEVEL0", // 0x298
	"VDD_APC_PLEVEL1", // 0x29C
	"VDD_APC_PLEVEL2", // 0x2A0
	"VDD_APC_PLEVEL3", // 0x2A4
	"VDD_APC_PLEVEL4", // 0x2A8
	"VDD_APC_PLEVEL5", // 0x2AC
	"VDD_APC_PLEVEL6", // 0x2B0
	"VDD_APC_PLEVEL7", // 0x2B4
	"VDD_APC_SSBI_ADDR", // 0x2B8
	"USBH_MD_REG", // 0x2BC
	"USBH_NS_REG", // 0x2C0
	"PAD3[0XF]", // 0x2C4
	"PAD3[0XF]", // 0x2C8
	"PAD3[0XF]", // 0x2CC
	"PAD3[0XF]", // 0x2D0
	"PAD3[0XF]", // 0x2D4
	"PAD3[0XF]", // 0x2D8
	"PAD3[0XF]", // 0x2DC
	"PAD3[0XF]", // 0x2E0
	"PAD3[0XF]", // 0x2E4
	"PAD3[0XF]", // 0x2E8
	"PAD3[0XF]", // 0x2EC
	"PAD3[0XF]", // 0x2F0
	"PAD3[0XF]", // 0x2F4
	"PAD3[0XF]", // 0x2F8
	"PAD3[0XF]", // 0x2FC
	"PLL0_MODE", // 0x300
	"PLL0_L_VAL_REG", // 0x304
	"PLL0_M_VAL", // 0x308
	"PLL0_N_VAL", // 0x30C
	"PLL0_INTERNAL1", // 0x310
	"PLL0_INTERNAL2", // 0x314
	"PLL0_INTERNAL3", // 0x318
	"PLL1_MODE", // 0x31C
	"PLL1_L_VAL_REG", // 0x320
	"PLL1_M_VAL", // 0x324
	"PLL1_N_VAL", // 0x328
	"PLL1_INTERNAL1", // 0x32C
	"PLL1_INTERNAL2", // 0x330
	"PLL1_INTERNAL3", // 0x334
	"PLL2_MODE", // 0x338
	"PLL2_L_VAL_REG", // 0x33C
	"PLL2_M_VAL", // 0x340
	"PLL2_N_VAL", // 0x344
	"PLL2_INTERNAL1", // 0x348
	"PLL2_INTERNAL2", // 0x34C
	"PLL2_INTERNAL3", // 0x350
	"PLL3_MODE", // 0x354
	"PLL3_L_VAL_REG", // 0x358
	"PLL3_M_VAL", // 0x35C
	"PLL3_N_VAL", // 0x360
	"PLL3_INTERNAL1", // 0x364
	"PLL3_INTERNAL2", // 0x368
	"PLL3_INTERNAL3", // 0x36C
};

static const char* REG_NAMES_EBI1[] = {
	"EBI1_CS_ADEC",
	"EBI1_TEST_BUS_CFG",
	"EBI1_MISR_CTRL",
	"EBI1_MISR_OUT",
	"EBI1_MISR_OUT",
	"EBI1_MISR_OUT",
	"EBI1_MISR_OUT",
	//" unsigned long							filler01[9];							// offset 0x1C-0x3C
	"EBI_SDRAM_INTERFACE_CFG1",
	"EBI_SDRAM_INTERFACE_CFG2",
	"EBI_SDRAM_DEVICE_MODE1",
	"EBI_SDRAM_DEVICE_MODE2",
	"EBI_SDRAM_MEM_DEVICE_PARAMETER_CFG1",
	"EBI_SDRAM_MEM_DEVICE_PARAMETER_CFG2",
	"EBI_SDRAM_MEM_DEVICE_PARAMETER_CFG3",
	"EBI_SDRAM_CONTROLLER_CFG",
	"EBI_SDRAM_DEVICE_CFG",
	"EBI_SDRAM_DEVICE_PRECHARGE",
	"EBI_SDRAM_DEVICE_REFRESH",
	"EBI_SDRAM_INTERFACE_CFG3",
	"EBI_SDRAM_DEVICE_STATUS",
	//" unsigned long							filler02[3];							// offset 0x74-0x7C
	"EBI_SDRAM_CDC_CFG",
	"EBI_SDRAM_CDC_MODE0",
	"EBI_SDRAM_CDC_MODE1",
	"EBI_SDRAM_CDC0_MODE2",
	"EBI_SDRAM_CDC_CFG",
	"EBI_SDRAM_CDC_MODE0",
	"EBI_SDRAM_CDC_MODE1",
	"EBI_SDRAM_CDC_MODE2",
	"EBI_SDRAM_CDC_CFG",
	"EBI_SDRAM_CDC_MODE0",
	"EBI_SDRAM_CDC_MODE1",
	"EBI_SDRAM_CDC_MODE2",
	"EBI_SDRAM_CDC_CFG",
	"EBI_SDRAM_CDC_MODE0",
	"EBI_SDRAM_CDC_MODE1",
	"EBI_SDRAM_CDC_MODE2",
};

static void DumpClk()
{
	//REG_NAMES
	//0x36C
	int i;
	for ( i=0; i < 0x36C/4; i++ )
	{
		printk("%s : 0x%X\n", REG_NAMES[i], readl( REG( i*4 ) ));	
	}	
}

//bleh!!
static int printx(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);

	return r;
}


// function to do some evil memory dumping
static void print_hexview(const void *pSource, unsigned int sourceLength)
	{
	
	#define isprint(x) ( ((x) > 32) && ((x) < 127) )
        printx("- Size: 0x%X | %u\n", sourceLength, sourceLength);
		printx("--------|------------------------------------------------|----------------|\n");
		printx(" offset |00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F |0123456789ABCDEF|\n");
		printx("--------|------------------------------------------------|----------------|\n");

		unsigned int i = 0;
		unsigned int c = 0;
		unsigned int start;
		unsigned int written;
		unsigned char byte;
		const unsigned char *pData = (const unsigned char *)pSource;

		for( ; i < sourceLength; )
		{
			start = i;
			printx("%08X|", i);
			for( c = 0; c < 16 && i < sourceLength; )		// write 16 bytes per line
			{
				printx("%02X ", (int)pData[i]);
				++i; ++c;
			}

			written = c;
			for( ; c < 16; ++c )							// finish off any incomplete bytes
				printx("   ");

			// write the text part
			printx("|");
			for( c = 0; c < written; ++c )
			{
				byte = pData[start + c];
				if( isprint((int)byte) )
					printx("%c",(char)byte);
				else
					printx(".");
			}

			for( ; c < 16; ++c )
				printx(" ");

			printx("|\n");
		}

		printx("---------------------------------------------------------------------------\n");
	}

// evil hacking.... i'm not proud at myself
static unsigned char clock_map[0xC00 +1];

static inline int set_grp_clk( int arg )
{
	int i;
	if ( arg != 0 )
	{
	
	// part of evil shit
	//writel( 0xA1004, REG( 0x34 )); //ADSP_NS_REG
	
	//writel( 0x1FFF0, REG( 0x6C ));//ICODEC_RX_MD_REG
	//writel( 0xFFF1495C, REG( 0x6C+4 ));//ICODEC_RX_NS_REG
	//writel( 0x1FFE1, REG( 0x6C+4+4 ));//ICODEC_TX_MD_REG
	//writel( 0xFFE2095C, REG( 0x6C+4+4+4 ));//ICODEC_TX_NS_REG
	// end of evil shit
	
	printk( "imem_config: 0x%X\n" , readl( MSM_IMEM_BASE ));
	
	writel( 0, MSM_IMEM_BASE );
	
	
		printk("GRP_ENABLE before\n");
		DumpClk();
		
		//for (i = 0; i < 0xC00; i++)
		//{
		//	clock_map[i] = readb( REG( i ) );
		//}
		
		//print_hexview( &clock_map[0], 0xC00 );
		
		
		
		
		
		//axi_reset
		writel(readl(MSM_CLK_CTL_BASE+0x208) |0x20,          MSM_CLK_CTL_BASE+0x208); //AXI_RESET
		//row_reset
		writel(readl(MSM_CLK_CTL_BASE+0x214) |0x20000,       MSM_CLK_CTL_BASE+0x214); //ROW_RESET
		//vdd_grp gfs_ctl
		writel(                              0x11f,          MSM_CLK_CTL_BASE+0x284); //VDD_GRP_GFS_CTL
		
		// put delay here...
		// 0x2 ms in pbus cycles.. or something..
		mdelay(20);
		
		//grp NS
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x800,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x80,          MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x200,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		
		/* bit masks...
		0x4000 - CLK_SEL
		0x3000 - AXI_GRP_CLK_DIV
		0x0800 - GRP_ROOT_ENA
		0x0400 - IMEM_CLK_INV
		0x0200 - IMEM_CLK_BRANCH_ENA
		0x0100 - GRP_CLK_INV
		0x0080 - GRP_CLK_BRANCH_ENA
		0x0078 - SRC_DIV
		0x0007 - SRC_SEL
		*/
		
/* MSM_CLK_CTL_BASE docs
0x10000000 29 	GLBL_ROOT_ENA 			Enable global clock tree root cell. Must be enabled in order to
0x00000000 28 	SDC4_H_CLK_ENA			Enable secure digital controller4 AHB clock
0x08000000 27 	SDC3_H_CLK_ENA			Enable secure digital controller3 AHB clock
0x04000000 26 	UART2DM_P_CLK_ENA		Enable UART2DM pbus clock
0x02000000 25 	USBH_P_CLK_ENA 			usbh_p_clk_ena
0x01E00000 24:21 UNUSED 				UNUSED
0x00100000 20 	MARM_CLK_ENA 			Enable modem ARM9 processor clock
0x00080000 19 	RESERVED 				RESERVED
0x00040000 18 	TSIF_P_CLK_ENA 			Enable TSIF pbus clock
0x00020000 17 	UART1DM_P_CLK_ENA 		Enable UART1DM pbus clock
0x00010000 16 	AHB1_CLK_ENA 			Enable modem AHB bus clock 1
0x00008000 15 	AHB0_CLK_ENA 			Enable modem AHB bus clock 0
0x00004000 14 	PBUS_CLK_ENA 			Enable peripheral bus clock
0x00002000 13 	EBI2_CLK_ENA 			Enable external bus interface 2 clock
0x00001000 12 	AXI_EBI1_CLK_ENA 		Enable external bus interface 1 clock
0x00000800 11 	AXI_LI_MSS_CLK_EN	 	Enable modem subsystem AXI local interconnect clock
0x00000400 10 	AXI_ARB_CLK_ENA 		Enable AXI bus arbitration clock
0x00000200 9 	MDP_CLK_ENA 			Enable mobile display processor clock
0x00000100 8 	SDC2_H_CLK_ENA 			Enable secure digital controller2 AHB clock
0x00000080 7 	SDC1_H_CLK_ENA 			Enable secure digital controller1 AHB clock
0x00000040 6 	CE_CLK_ENA Enable 		crypto engine clock
0x00000020 5 	ADM_CLK_ENA Enable 		applications data mover clock
0x00000010 4 	MARM_ETM_CLK_ENA 		Enable modem ARM9 ETM clock
0x00000008 3 	AXI_LI_VG_CLK_ENA 		Enable video/graphics subsystem AXI local interconnect clock
0x00000004 2 	AXI_LI_APPS_CLK_ENA 	Enable applications subsystem AXI local interconnect clock
0x00000002 1 	AXI_LI_A11S_CLK_ENA 	Enable ARM11 subsystem AXI local interconnect clock
0x00000001 0 	AXI_SMI_CLK_ENA 		Enable stacked
*/

/* logs
0x4880 - disabled
0x4a80 - enabled
*/

		//grp idx
		writel(readl(MSM_CLK_CTL_BASE)       |0x8,           MSM_CLK_CTL_BASE);
		//grp clk ramp
		writel(readl(MSM_CLK_CTL_BASE+0x290) &(~(0x4)),      MSM_CLK_CTL_BASE+0x290); //MSM_RAIL_CLAMP_IO
		//writel(readl(MSM_CLK_CTL_BASE+0x290) |0x4,           MSM_CLK_CTL_BASE+0x290);
		//Suppress bit 0 of grp MD (?!?)
		writel(readl(MSM_CLK_CTL_BASE+0x80)  &(~(0x1)),      MSM_CLK_CTL_BASE+0x80);
		//axi_reset
		writel(readl(MSM_CLK_CTL_BASE+0x208) &(~(0x20)),     MSM_CLK_CTL_BASE+0x208); //AXI_RESET
		//row_reset
		writel(readl(MSM_CLK_CTL_BASE+0x214) &(~(0x20000)),  MSM_CLK_CTL_BASE+0x214); //ROW_RESET
		
		printk("GRP_ENABLE after\n");
		DumpClk();
	}
	else
	{
		printk("GRP_DISABLE before\n");
		DumpClk();
		//grp NS
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x800,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x80,          MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x200,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		
		//grp idx
		writel(readl(MSM_CLK_CTL_BASE)       |0x8,           MSM_CLK_CTL_BASE);
		
		//grp MD
		writel(readl(MSM_CLK_CTL_BASE+0x80)  |0x1,      	MSM_CLK_CTL_BASE+0x80);

		int i = 0;
		int status = 0;
		while ( status == 0 && i < 100)
		{
			i++;
			status = readl(MSM_CLK_CTL_BASE+0x84) & 0x1;			
		}
		
		writel(readl(MSM_CLK_CTL_BASE+0x208) |0x20,     	MSM_CLK_CTL_BASE+0x208); //AXI_RESET
		writel(readl(MSM_CLK_CTL_BASE+0x214) |0x20000,  	MSM_CLK_CTL_BASE+0x214); //ROW_RESET
		
		writel(readl(MSM_CLK_CTL_BASE+0x84)  &(~(0x800)),   MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  &(~(0x80)),    MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  &(~(0x200)),   MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG

		writel(readl(MSM_CLK_CTL_BASE+0x290) |0x4,      	MSM_CLK_CTL_BASE+0x290);
		writel(                              0x11f,         MSM_CLK_CTL_BASE+0x284);
		
		//R2 = * (R0 + 0x10);
		//R3 = * (R2 + 0x288);
		
		int control = readl(MSM_CLK_CTL_BASE+0x288); //VDD_VDC_GFS_CTL
		if ( control & 0x100 )
			writel(readl(MSM_CLK_CTL_BASE) &(~(0x8)),      	MSM_CLK_CTL_BASE);
		printk("GRP_DISABLE after\n");
		DumpClk();
	}
	
	/*printk(" PRPH_WEB_NS_REG: 0x%X\n", readl( REG( PRPH_WEB_NS_REG ) ));
	printk(" GRP_NS_REG: 0x%X\n", readl( REG( GRP_NS_REG ) ));
	printk(" AXI_RESET: 0x%X\n", readl( REG( AXI_RESET ) ));
	printk(" ROW_RESET: 0x%X\n", readl( REG( ROW_RESET ) ));
	printk(" VDD_GRP_GFS_CTL: 0x%X\n", readl( REG( VDD_GRP_GFS_CTL ) ));
	printk(" MSM_RAIL_CLAMP_IO: 0x%X\n", readl( REG( MSM_RAIL_CLAMP_IO ) ));*/
}

static int pc_clk_enable(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);

	//XXX: too spammy, extreme debugging only: D(KERN_DEBUG "%s: %d\n", __func__, id);
	
	if ( id == IMEM_CLK || id == GRP_CLK )
	{
		//set_grp_clk( 0 );
		set_grp_clk( 1 );
		writel(readl(MSM_CLK_CTL_BASE) | (1U << params.idx), MSM_CLK_CTL_BASE);
		return 0;
	}

	if (params.idx)
	{
		writel(readl(MSM_CLK_CTL_BASE) | (1U << params.idx), MSM_CLK_CTL_BASE);
		return 0;
	} else if (params.ns_only > 0 && params.offset)
	{
		writel((readl(MSM_CLK_CTL_BASE + params.offset) &0xffff0000) | params.ns_only, MSM_CLK_CTL_BASE + params.offset);
		return 0;
	}
	if(debug_mask&DEBUG_UNKNOWN_ID)
		printk(KERN_WARNING "%s: FIXME! enabling a clock that doesn't have an ena bit "
		       "or ns-only offset: %u\n", __func__, id);

	return 0;
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
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0xffff0000, MSM_CLK_CTL_BASE + params.offset);
	} else {
		if(debug_mask&DEBUG_UNKNOWN_ID)
			printk(KERN_WARNING "%s: FIXME! disabling a clock that doesn't have an "
			       "ena bit: %u\n", __func__, id);
	}
}

static int pc_clk_set_rate(uint32_t id, unsigned long rate)
{
	int retval;
	retval = 0;

	if(DEBUG_MDNS)
		D("%s: id=%u rate=%lu\n", __func__, id, rate);

	retval = set_mdns_host_clock(id, rate);

	return retval;
}

static int pc_clk_set_min_rate(uint32_t id, unsigned long rate)
{
	if (id < NR_CLKS)
	 min_clk_rate[id]=rate;
	else if(debug_mask&DEBUG_UNKNOWN_ID)
	 printk(KERN_WARNING " FIXME! clk_set_min_rate not implemented; %u:%lu NR_CLKS=%d\n", id, rate, NR_CLKS);

	return 0;
}

static int pc_clk_set_max_rate(uint32_t id, unsigned long rate)
{
	if (id < NR_CLKS)
	 max_clk_rate[id]=rate;
	else if(debug_mask&DEBUG_UNKNOWN_ID)
	 printk(KERN_WARNING " FIXME! clk_set_min_rate not implemented; %u:%lu NR_CLKS=%d\n", id, rate, NR_CLKS);

	return 0;
}

static unsigned long pc_clk_get_rate(uint32_t id)
{
	unsigned long rate = 0;

	switch (id) {
		/* known MD/NS clocks, MSM_CLK dump and arm/mach-msm/clock-7x30.c */
		case SDC1_CLK:
		case SDC2_CLK:
		case SDC3_CLK:
		case SDC4_CLK:
		case UART1DM_CLK:
		case UART2DM_CLK:
		case USB_HS_CLK:
		case SDAC_CLK:
		case TV_DAC_CLK:
		case TV_ENC_CLK:
		case USB_OTG_CLK:
			rate = get_mdns_host_clock(id);
			break;

		case SDC1_PCLK:
		case SDC2_PCLK:
		case SDC3_PCLK:
		case SDC4_PCLK:
			rate = 64000000; /* g1 value */
			break;

		default:
			//TODO: support all clocks
			if(debug_mask&DEBUG_UNKNOWN_ID)
				printk("%s: unknown clock: id=%u\n", __func__, id);
			rate = 0;
	}

	return rate;
}

static int pc_clk_set_flags(uint32_t id, unsigned long flags)
{
	if(debug_mask&DEBUG_UNKNOWN_CMD)
		printk(KERN_WARNING "%s not implemented for clock: id=%u, flags=%lu\n", __func__, id, flags);
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
	if(debug_mask&DEBUG_UNKNOWN_CMD)
		printk(KERN_WARNING "%s not implemented for PLL=%u\n", __func__, id);

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
	printk("clk_put called: id: 0x%X\n", clk->id);
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
	if (clk->flags & CLKFLAG_USE_MAX_TO_SET) {
		ret = pc_clk_set_max_rate(clk->id, rate);
		if (ret)
			return ret;
	}
	if (clk->flags & CLKFLAG_USE_MIN_TO_SET) {
		ret = pc_clk_set_min_rate(clk->id, rate);
		if (ret)
			return ret;
	}

	if (clk->flags & CLKFLAG_USE_MAX_TO_SET ||
		clk->flags & CLKFLAG_USE_MIN_TO_SET)
		return ret;

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
	struct clk *clk;

	spin_lock_init(&clocks_lock);
	mutex_lock(&clocks_mutex);
	for (clk = msm_clocks; clk && clk->name; clk++) {
		list_add_tail(&clk->list, &clocks);
	}
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
	int i;
	int val;

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
