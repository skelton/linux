/* arch/arm/mach-msm/smd_7500.c
 * Author: Joe Hansche <madcoder@gmail.com>
 * Based on vogue-smd.c by Martin Johnson <M.J.Jonson@massey.ac.nz>
 *            and smd.c by Brian Swetland <swetland@google.com>
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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/mach-types.h>

#include <mach/msm_smd.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include "smd_private.h"

#define MSM_A2M_INT(n) (MSM_CSR_BASE + 0x400 + (n) * 4)
#define BURST_SIZE 0xfffffff

#if 0
 #define D(fmt, arg...) printk(KERN_DEBUG "smd_7500: " fmt, ## arg)
#else
 #define D(x...) do {} while (0)
#endif

extern void *smem_alloc(unsigned id, unsigned size);

struct smd_7500_buffer {
	unsigned short send_size;
	volatile unsigned short *send_head;
	volatile unsigned short *send_tail;

	unsigned short recv_size;
	volatile unsigned short *recv_head;
	volatile unsigned short *recv_tail;
};

static inline void notify_other_smd(int ch)
{
	writel(1, MSM_A2M_INT(3 + ch));
}

static inline void smd_7500_update_state(smd_channel_t *ch)
{ }

static int smd_7500_read(smd_channel_t *ch, void *data, int len);
static int smd_7500_write(smd_channel_t *ch, const void *data, int len);
static int smd_7500_read_avail(smd_channel_t *ch);
static int smd_7500_write_avail(smd_channel_t *ch);
static void smd_7500_check_for_data(smd_channel_t *ch);

void do_smd_7500_probe(unsigned char *smd_ch_allocated, struct list_head *smd_ch_closed_list)
{
	struct smd_channel *ch;
	struct smd_7500_buffer *p;
	int i;

	if (!machine_is_htcraphael_cdma() && !machine_is_htcdiamond_cdma() && !machine_is_htcraphael_cdma500())
		return;

	// 7500 only needs special attention on ports 0 and 1
	for (i=0; i<2; i++) {
		if (smd_ch_allocated[i]) {
			printk(KERN_WARNING "%s: smd ch %d already initialized\n",
			                                              __func__, i);
			continue;
		}

		ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
		if (ch == 0) {
			printk(KERN_ERR "%s: Out of memory\n", __func__);
			return;
		}

		ch->send = kzalloc(sizeof(struct smd_half_channel), GFP_KERNEL);
		ch->recv = kzalloc(sizeof(struct smd_half_channel), GFP_KERNEL);
		ch->buf_7500 = kzalloc(sizeof(struct smd_7500_buffer), GFP_KERNEL);
		ch->n = i;

		memcpy(ch->name, "SMD_", 4);

		if (i == 0) {
			p = ch->buf_7500;

			p->recv_size = 0x200;
			ch->recv_buf = (unsigned char*)(MSM_SHARED_RAM_BASE + 0xf0000);
			p->recv_head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fc8);
			p->recv_tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fca);

			p->send_size = 0x200;
			ch->send_buf = (unsigned char*)(MSM_SHARED_RAM_BASE + 0xf0200);
			p->send_head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fcc);
			p->send_tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fce);

			ch->open = (short *)(MSM_SHARED_RAM_BASE + 0xf3fc4);
			// Mark this as always-open, to avoid watchdog from crashing the modem
			*ch->open = 1;

			memcpy(ch->name + 4, "DS", 20);
		} else if (i == 1) {
			p = ch->buf_7500;

			p->recv_size = 0xd82;
			ch->recv_buf = (char*)(MSM_SHARED_RAM_BASE + 0xf0a00);
			p->recv_head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fd8);
			p->recv_tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fda);

			p->send_size = 0x282;
			ch->send_buf = (char*)(MSM_SHARED_RAM_BASE + 0xf1782);
			p->send_head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fdc);
			p->send_tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fde);

			ch->open = (short *)(MSM_SHARED_RAM_BASE + 0xf3fd4);
			// Mark this as always-open, to avoid watchdog from crashing the modem
			*ch->open = 1;

			memcpy(ch->name + 4, "DIAG", 20);
		}
		
		ch->read = smd_7500_read;
		ch->write = smd_7500_write;
		ch->read_avail = smd_7500_read_avail;
		ch->write_avail = smd_7500_write_avail;
		ch->update_state = smd_7500_update_state;
		ch->check_for_data = smd_7500_check_for_data;
		
		ch->name[23] = 0;
		ch->pdev.name = ch->name;
		ch->pdev.id = -1;
		
		printk(KERN_INFO "%s: late-alloc '%s' cid=%d, send=%p, recv=%p\n",
			__func__, ch->name, ch->n, ch->send_buf, ch->recv_buf);
		
		list_add(&ch->ch_list, smd_ch_closed_list);
		smd_ch_allocated[i] = 1;
		
		platform_device_register(&ch->pdev);
	}
}

static int smd_7500_read(smd_channel_t *ch, void *data, int len)
{
	struct smd_7500_buffer *p;
	int recvd;
	unsigned short mytail;
	unsigned char *buf;
	
	recvd = 0;
	buf = data;
	p = ch->buf_7500;
	
	D("%s on ch %d\n", __func__, ch->n);
	
	if (len < 0)
		return -EINVAL;

	if (data == NULL)
		return -EINVAL;
	
	notify_other_smd(ch->n);
	
	mytail = *p->recv_tail;
	
	while (recvd < len) {
		unsigned int n;

		mytail = (mytail + 1) % p->recv_size;
		n = min(len-recvd, p->recv_size-mytail);

		memcpy(buf, (const void *)(ch->recv_buf + mytail), n);

		buf += n;
		recvd += n;

		/* recv_tail has to be incremented by 1 before it can be
		 * read from, as the old design did
		 */
		mytail = (mytail + n - 1) % p->recv_size;

		if ((recvd % BURST_SIZE) == 0) {
			*p->recv_tail = mytail;
			notify_other_smd(ch->n);
		}
	}
	*p->recv_tail = mytail;
	D("received %d bytes from cid=%d\n", recvd, ch->n);

	notify_other_smd(ch->n);

	return recvd;
}

static int smd_7500_write(smd_channel_t *ch, const void *data, int len)
{
	struct smd_7500_buffer *p;
	int sent;
	unsigned short myhead;
	const unsigned char *buf;
	
	sent = 0;
	buf = data;
	p = ch->buf_7500;
	
	D("%s: on ch %d, %d bytes\n", __func__, ch->n, len);
	
	ch->check_for_data(ch);

	if (len < 0)
		return -EINVAL;
	
	myhead = *p->send_head;
	notify_other_smd(ch->n);
	while (len > 0) {
		myhead = (myhead + 1) % p->send_size;
		while (myhead == *p->send_tail)
			udelay(5);
		
		--len;
		
		*(ch->send_buf + myhead) = *buf++;
		++sent;
		
		if ((sent % BURST_SIZE) == 0 || (myhead == (p->send_size - 1))) {
			*p->send_head = myhead;
			notify_other_smd(ch->n);
		}
	}
	*p->send_head = myhead;
	D("wrote %d bytes to cid=%d\n", sent, ch->n);

	notify_other_smd(ch->n);

	ch->check_for_data(ch);

	return sent;
}

static int smd_7500_read_avail(smd_channel_t *ch)
{
	struct smd_7500_buffer *p;
	p = ch->buf_7500;
	return (*p->recv_head - *p->recv_tail + p->recv_size) % p->recv_size;
}

static int smd_7500_write_avail(smd_channel_t *ch)
{
	struct smd_7500_buffer *p;
	p = ch->buf_7500;
	return p->send_size - ((*p->send_head - *p->send_tail + p->send_size) % p->send_size) - 1;
}

static void smd_7500_check_for_data(smd_channel_t *ch)
{
	struct smd_7500_buffer *p;
	if (ch->open && *ch->open) {
		p = ch->buf_7500;
		if (*p->recv_head != *p->recv_tail) {
			if (ch->notify && ch->priv)
				ch->notify(ch->priv, SMD_EVENT_DATA);
			ch->recv->fHEAD = 0;
		}
		if (*p->send_head != *p->send_tail) {
			notify_other_smd(ch->n);
			ch->send->fTAIL = 0;
		}
	}
	
}
