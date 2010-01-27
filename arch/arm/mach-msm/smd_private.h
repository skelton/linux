/* arch/arm/mach-msm/smd_private.h
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_

#include <linux/platform_device.h>
#include <linux/list.h>
#include <mach/msm_smd.h>

struct smd_half_channel {
	unsigned state;
	unsigned char fDSR;
	unsigned char fCTS;
	unsigned char fCD;
	unsigned char fRI;
	unsigned char fHEAD;
	unsigned char fTAIL;
	unsigned char fSTATE;
	unsigned char fUNUSED;
	unsigned tail;
	unsigned head;
};

struct smd_channel {
	volatile struct smd_half_channel *send;
	volatile struct smd_half_channel *recv;
	volatile unsigned char *send_buf;
	volatile unsigned char *recv_buf;
	unsigned buf_size;
	struct list_head ch_list;

	unsigned current_packet;
	unsigned n;
	void *priv;
	void (*notify)(void *priv, unsigned flags);

	int (*read)(smd_channel_t *ch, void *data, int len);
	int (*write)(smd_channel_t *ch, const void *data, int len);
	int (*read_avail)(smd_channel_t *ch);
	int (*write_avail)(smd_channel_t *ch);

	void (*update_state)(smd_channel_t *ch);
	void (*check_for_data)(smd_channel_t *ch);
	unsigned last_state;

	char name[32];
	struct platform_device pdev;
	volatile short * open;

	struct smd_7500_buffer *buf_7500;
};

struct smem_heap_info
{
	unsigned initialized;
	unsigned free_offset;
	unsigned heap_remaining;
	unsigned reserved;
};

struct smem_heap_entry
{
	unsigned allocated;
	unsigned offset;
	unsigned size;
	unsigned reserved;
};

struct smem_proc_comm
{
	unsigned command;
	unsigned status;
	unsigned data1;
	unsigned data2;
};

#define PC_APPS  0
#define PC_MODEM 1

#define VERSION_QDSP6     4
#define VERSION_APPS_SBL  6
#define VERSION_MODEM_SBL 7
#define VERSION_APPS      8
#define VERSION_MODEM     9

struct smem_shared
{
	struct smem_proc_comm proc_comm[4];
	unsigned version[32];
	struct smem_heap_info heap_info;
	struct smem_heap_entry heap_toc[128];
};

struct smsm_shared
{
	unsigned host;
	unsigned state;
};

//SMEM
struct smsm_interrupt_info_5200
{
	uint32_t aArm_en_mask;
	uint32_t aArm_interrupts_pending;
	uint32_t aArm_wakeup_reason;
	uint32_t padding;
};

struct smsm_interrupt_info_6120
{
	uint32_t aArm_en_mask;
	uint32_t aArm_interrupts_pending;
	uint32_t aArm_wakeup_reason;
	uint32_t padding;
	uint32_t reserved[8];
	/*
	 * Something like:
uint32_t aArm_rpc_prog;
uint32_t aArm_rpc_proc;
char aArm_smd_port_name[3];
uint32_t aArm_gpio_info;
	 */
};

//Kernel use
struct smsm_interrupt_info
{
	uint32_t aArm_en_mask;
	uint32_t aArm_interrupts_pending;
	uint32_t aArm_wakeup_reason;
	uint32_t padding;
};

#define SZ_DIAG_ERR_MSG 0xC8
#define ID_DIAG_ERR_MSG SMEM_DIAG_ERR_MESSAGE
#define ID_SMD_CHANNELS SMEM_SMD_BASE_ID
#define ID_SHARED_STATE SMEM_SMSM_SHARED_STATE
#define ID_CH_ALLOC_TBL SMEM_CHANNEL_ALLOC_TBL

#define SMSM_INIT          0x000001
#define SMSM_SMDINIT       0x000008
#define SMSM_RPCINIT       0x000020
#define SMSM_RESET         0x000040
#define SMSM_RSA               0x0080
#define SMSM_RUN           0x000100
#define SMSM_PWRC              0x0200
#define SMSM_TIMEWAIT          0x0400
#define SMSM_TIMEINIT          0x0800
#define SMSM_PWRC_EARLY_EXIT   0x1000
#define SMSM_WFPI              0x2000
#define SMSM_SLEEP             0x4000
#define SMSM_SLEEPEXIT         0x8000
#define SMSM_OEMSBL_RELEASE    0x10000
#define SMSM_PWRC_SUSPEND      0x200000

#define SMSM_WKUP_REASON_RPC	0x00000001
#define SMSM_WKUP_REASON_INT	0x00000002
#define SMSM_WKUP_REASON_GPIO	0x00000004
#define SMSM_WKUP_REASON_TIMER	0x00000008
#define SMSM_WKUP_REASON_ALARM	0x00000010
#define SMSM_WKUP_REASON_RESET	0x00000020

void *smem_alloc(unsigned id, unsigned size);
int smsm_change_state(uint32_t clear_mask, uint32_t set_mask);
uint32_t smsm_get_state(void);
int smsm_set_sleep_duration(uint32_t delay);
int smsm_set_interrupt_info(struct smsm_interrupt_info *info);
int smsm_get_interrupt_info(struct smsm_interrupt_info *info);
void smsm_print_sleep_info(void);

#define SMEM_NUM_SMD_CHANNELS        64

#define SMEM_NUM_SMD_STREAM_CHANNELS        64
#define SMEM_NUM_SMD_BLOCK_CHANNELS         64

typedef enum
{
	/* fixed items */
	SMEM_PROC_COMM = 0,
	SMEM_HEAP_INFO,
	SMEM_ALLOCATION_TABLE,
	SMEM_VERSION_INFO,
	SMEM_HW_RESET_DETECT,
	SMEM_AARM_WARM_BOOT,
	SMEM_DIAG_ERR_MESSAGE,
	SMEM_SPINLOCK_ARRAY,
	SMEM_MEMORY_BARRIER_LOCATION,

	/* dynamic items */
	SMEM_AARM_PARTITION_TABLE,
	SMEM_AARM_BAD_BLOCK_TABLE,
	SMEM_RESERVE_BAD_BLOCKS,
	SMEM_WM_UUID,
	SMEM_CHANNEL_ALLOC_TBL,
	SMEM_SMD_BASE_ID,
	SMEM_SMEM_LOG_IDX = SMEM_SMD_BASE_ID + SMEM_NUM_SMD_CHANNELS,
	SMEM_SMEM_LOG_EVENTS,
	SMEM_SMEM_STATIC_LOG_IDX,
	SMEM_SMEM_STATIC_LOG_EVENTS,
	SMEM_SMEM_SLOW_CLOCK_SYNC,
	SMEM_SMEM_SLOW_CLOCK_VALUE,		// b4e0,8
	SMEM_BIO_LED_BUF,
	SMEM_SMSM_SHARED_STATE,			// c28,10
	SMEM_SMSM_INT_INFO,			// c38,10
	SMEM_SMSM_SLEEP_DELAY,			// c48,8
	SMEM_SMSM_LIMIT_SLEEP,			// c50,8
	SMEM_SLEEP_POWER_COLLAPSE_DISABLED,	// b4e8,8
	SMEM_KEYPAD_KEYS_PRESSED,
	SMEM_KEYPAD_STATE_UPDATED,
	SMEM_KEYPAD_STATE_IDX,
	SMEM_GPIO_INT,				// c68,70
	SMEM_MDDI_LCD_IDX,
	SMEM_MDDI_HOST_DRIVER_STATE,
	SMEM_MDDI_LCD_DISP_STATE,
	SMEM_LCD_CUR_PANEL,
	SMEM_MARM_BOOT_SEGMENT_INFO,
	SMEM_AARM_BOOT_SEGMENT_INFO,
	SMEM_SLEEP_STATIC,
	SMEM_SCORPION_FREQUENCY,
	SMEM_SMD_PROFILES,
	SMEM_TSSC_BUSY,
	SMEM_HS_SUSPEND_FILTER_INFO,
	SMEM_BATT_INFO,
	SMEM_APPS_BOOT_MODE,
	SMEM_VERSION_FIRST,
	SMEM_VERSION_LAST = SMEM_VERSION_FIRST + 24,
	SMEM_OSS_RRCASN1_BUF1,
	SMEM_OSS_RRCASN1_BUF2,
	SMEM_ID_VENDOR0,
	SMEM_ID_VENDOR1,
	SMEM_ID_VENDOR2,
	SMEM_HW_SW_BUILD_ID,
#define SMEM_NUM_ITEMS_V1 SMEM_HW_SW_BUILD_ID
	SMEM_SMD_BLOCK_PORT_BASE_ID,
	SMEM_SMD_BLOCK_PORT_PROC0_HEAP = SMEM_SMD_BLOCK_PORT_BASE_ID +
						SMEM_NUM_SMD_BLOCK_CHANNELS,
	SMEM_SMD_BLOCK_PORT_PROC1_HEAP = SMEM_SMD_BLOCK_PORT_PROC0_HEAP +
						SMEM_NUM_SMD_BLOCK_CHANNELS,
	SMEM_I2C_MUTEX = SMEM_SMD_BLOCK_PORT_PROC1_HEAP +
						SMEM_NUM_SMD_BLOCK_CHANNELS,
	SMEM_SCLK_CONVERSION,
	SMEM_SMD_SMSM_INTR_MUX,
	SMEM_SMSM_CPU_INTR_MASK,
	SMEM_APPS_DEM_SLAVE_DATA,
	SMEM_QDSP6_DEM_SLAVE_DATA,
	SMEM_CLKREGIM_BSP,
	SMEM_CLKREGIM_SOURCES,
	SMEM_SMD_FIFO_BASE_ID,
	SMEM_USABLE_RAM_PARTITION_TABLE = SMEM_SMD_FIFO_BASE_ID +
						SMEM_NUM_SMD_STREAM_CHANNELS,
	SMEM_POWER_ON_STATUS_INFO,
	SMEM_DAL_AREA,
	SMEM_SMEM_LOG_POWER_IDX,
	SMEM_SMEM_LOG_POWER_WRAP,
	SMEM_SMEM_LOG_POWER_EVENTS,
	SMEM_ERR_CRASH_LOG,
	SMEM_ERR_F3_TRACE_LOG,
	SMEM_NUM_ITEMS_V2,
} smem_mem_type;

#endif
