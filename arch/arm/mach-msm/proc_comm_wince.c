/* arch/arm/mach-msm/proc_comm_wince.c
 *
 * Author: maejrep
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
 * Based on proc_comm.c by Brian Swetland
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>

#include "proc_comm_wince.h"

#define MSM_A2M_INT(n) (MSM_CSR_BASE + 0x400 + (n) * 4)

static inline void notify_other_proc_comm(void)
{
	writel(1, MSM_A2M_INT(6));
}

#define PC_DEBUG 1

#define PC_COMMAND      0x00
#define PC_STATUS       0x04
#define PC_SERIAL       0x08
#define PC_SERIAL_CHECK 0x0C

#define PC_DATA1        0x20
#define PC_DATA_RESULT1 0x24
#define PC_DATA2        0x30
#define PC_DATA_RESULT2 0x34

static DEFINE_SPINLOCK(proc_comm_lock);

/* The higher level SMD support will install this to
 * provide a way to check for and handle modem restart?
 */
int (*msm_check_for_modem_crash)(void);

#define TIMEOUT (10000000) /* 10s in microseconds */

/* Poll for a state change, checking for possible
 * modem crashes along the way (so we don't wait
 * forever while the ARM9 is blowing up.
 *
 * Return an error in the event of a modem crash and
 * restart so the msm_proc_comm() routine can restart
 * the operation from the beginning.
 */
static int proc_comm_wince_wait_for(unsigned addr, unsigned value)
{
	unsigned timeout = TIMEOUT;

	do {
		if (readl(addr) == value)
			return 0;

		if (msm_check_for_modem_crash)
			if (msm_check_for_modem_crash())
				return -EAGAIN;

		udelay(1);
	} while (--timeout != 0);
	printk(KERN_WARNING "proc_comm_wince_wait_for: timed out\n");
	return -EAGAIN;
}

int msm_proc_comm_wince(unsigned cmd, unsigned *data1, unsigned *data2)
{
#if !defined(CONFIG_MSM_AMSS_VERSION_WINCE)
  #warning NON-WinCE compatible AMSS version selected. WinCE proc_comm implementation is disabled and stubbed to return -EIO.
        return -EIO;
#else
	unsigned base = (unsigned)(MSM_SHARED_RAM_BASE + 0xfc100);
	unsigned long flags;
	unsigned timeout;
	unsigned status;
	unsigned num;
	int ret;

	spin_lock_irqsave(&proc_comm_lock, flags);

#if defined(PC_DEBUG) && PC_DEBUG
	printk(KERN_INFO "%s: waiting for modem; command=0x%08x "
		"data1=0x%08x, data2=0x%08x\n", __func__, cmd, data1 ? *data1 : 0,
		data2 ? *data2 : 0);
#endif

	if (proc_comm_wince_wait_for(base + PC_STATUS, 0))
	{
		printk(KERN_ERR "%s: timed out waiting for PCOMCE_CMD_READY\n", __func__);
		return -EIO;
	}

#if defined(PC_DEBUG) && PC_DEBUG
	printk(KERN_INFO "%s: PCOM ready, sending command.\n", __func__);
#endif	
	writel(cmd, base + PC_COMMAND);
	if ( (data1 && *data1) || (data2 && *data2) )
	{
		writel(cmd | 0x100, base + PC_COMMAND);
	}
	
	num = readl(base + PC_SERIAL) + 1;
	writel(num, base + PC_SERIAL);
	
	writel(data1 ? *data1 : 0, base + PC_DATA1);
	writel(data2 ? *data2 : 0, base + PC_DATA2);

#if defined(PC_DEBUG) && PC_DEBUG
	printk(KERN_INFO "%s: command and data sent (id=0x%x) ...\n", __func__, num);
#endif
	notify_other_proc_comm();

	timeout = TIMEOUT;
	while ( --timeout && readl(base + PC_SERIAL_CHECK) != num )
		udelay(1);

	if ( ! timeout )
	{
		printk(KERN_ERR "%s: command timed out waiting for modem response. "
			"status=0x%08x\n", __func__,  readl(base + PC_STATUS));
		writel(0, base + PC_STATUS);
		writel(0, base + PC_COMMAND);
		return -EIO;
	}
	
	
	status = readl(base + PC_STATUS);
#if defined(PC_DEBUG) && PC_DEBUG
	printk(KERN_INFO "%s: command status = 0x%08x\n", __func__, status);
#endif
	status ^= cmd;
	writel( status, base + PC_STATUS);
	
	if ( status == 0x100 || status == 0 )
	{
		if (data1)
			*data1 = readl(base + PC_DATA_RESULT1);
		if (data2)
			*data2 = readl(base + PC_DATA_RESULT2);
		ret = 0;
#if defined(PC_DEBUG) && PC_DEBUG
		printk(KERN_INFO "%s: success. data1=0x%08x, data2=0x%08x\n", __func__, data1 ? *data1 : 0, data2 ? *data2 : 0);
#endif
	} else {
		ret = -EIO;
	}

	writel(0, base + PC_STATUS);
	writel(0, base + PC_DATA1);
	writel(0, base + PC_DATA2);
	writel(0, base + PC_DATA_RESULT1);
	writel(0, base + PC_DATA_RESULT2);
	
	spin_unlock_irqrestore(&proc_comm_lock, flags);
	return ret;
#endif
}


// Initialize PCOM registers
int msm_proc_comm_wince_init()
{
#if !defined(CONFIG_MSM_AMSS_VERSION_WINCE)
  #warning NON-WinCE compatible AMSS version selected. WinCE proc_comm implementation is disabled and stubbed to return -EIO.
        return 0;
#else
	unsigned base = (unsigned)(MSM_SHARED_RAM_BASE + 0xfc100);
	unsigned long flags;

	spin_lock_irqsave(&proc_comm_lock, flags);

	writel(0, base + PC_DATA1);
	writel(0, base + PC_DATA2);
	writel(0, base + PC_DATA_RESULT1);
	writel(0, base + PC_DATA_RESULT2);
	writel(0, base + PC_SERIAL);
	writel(0, base + PC_SERIAL_CHECK);
	writel(0, base + PC_STATUS);
	writel(0, base + PC_COMMAND);

	spin_unlock_irqrestore(&proc_comm_lock, flags);
	printk(KERN_INFO "%s: WinCE PCOM initialized.\n", __func__);
	return 0;
#endif
}
