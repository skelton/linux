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
#define SMD_BUF_SIZE 8192
#define SMD_SS_OPENED            0x00000002
#define BURST_SIZE 0xfffffff

#if 0
#define D(x...) printk(KERN_DEBUG "smd_7500: " x)
#else
#define D(x...) do {} while (0)
#endif

extern void *smem_alloc(unsigned id, unsigned size);

struct smd_7500_buffer {
	// Take up space so none of the half_channel attributes clobber our attributes
	unsigned unused[5];
	// and our attributes end up inside the half_channel data buffer
	unsigned short size;
	unsigned short *head;
	unsigned short *tail;
	unsigned char *open;
	char *buffer;
};

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
	unsigned char data[SMD_BUF_SIZE];
};

struct smd_shared {
	struct smd_half_channel ch0;
	struct smd_half_channel ch1;
};

struct smd_channel {
	volatile struct smd_half_channel *send;
	volatile struct smd_half_channel *recv;
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
	short * open;
};

static inline void notify_other_smd(int ch) {
	writel(1, MSM_A2M_INT(3 + ch));
}

int smd_7500_read(smd_channel_t *ch, void *data, int len);
int smd_7500_write(smd_channel_t *ch, const void *data, int len);
int smd_7500_read_avail(smd_channel_t *ch);
int smd_7500_write_avail(smd_channel_t *ch);
void smd_7500_update_state(smd_channel_t *ch);
void smd_7500_check_for_data(smd_channel_t *ch);

void do_smd_7500_probe(unsigned char * smd_ch_allocated, struct list_head * smd_ch_closed_list) {
	struct smd_channel *ch;
	struct smd_7500_buffer *p;
	int i;

	if (!machine_is_htcraphael_cdma() && !machine_is_htcdiamond_cdma())
		return;

	// 7500 only needs special attention on ports 0 and 1
	for (i=0; i<2; i++) {
		if (smd_ch_allocated[i])
		{
			printk(KERN_WARNING "%s: smd ch %d already initialized?!\n", __func__, i);
			continue;
		}

		ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
		if (ch == 0) {
			printk(KERN_ERR "%s: Out of memory\n", __func__);
			return;
		}

		ch->send = kzalloc(sizeof(struct smd_half_channel), GFP_KERNEL);
		ch->recv = kzalloc(sizeof(struct smd_half_channel), GFP_KERNEL);
		ch->n = i;

		memcpy(ch->name, "SMD_", 4);

		if (i == 0) {
			
			p = (struct smd_7500_buffer*)ch->recv;
			p->size = 0x200;
			p->buffer = (char*)(MSM_SHARED_RAM_BASE + 0xf0000);
			p->head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fc8);
			p->tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fca);

			p = (struct smd_7500_buffer*)ch->send;
			p->size = 0x200;
			p->buffer = (char*)(MSM_SHARED_RAM_BASE + 0xf0200);
			p->head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fcc);
			p->tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fce);

			ch->open = (short *)(MSM_SHARED_RAM_BASE + 0xf3fc4);
			*ch->open = 0;

			memcpy(ch->name + 4, "DS", 20);
		} else if (i == 1) {
			p = (struct smd_7500_buffer*)ch->recv;
			p->size = 0xd82;
			p->buffer = (char*)(MSM_SHARED_RAM_BASE + 0xf0a00);
			p->head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fd8);
			p->tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fda);

			p = (struct smd_7500_buffer*)ch->send;
			p->size = 0x282;
			p->buffer = (char*)(MSM_SHARED_RAM_BASE + 0xf1782);
			p->head = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fdc);
			p->tail = (unsigned short *)(MSM_SHARED_RAM_BASE + 0xf3fde);

			ch->open = (short *)(MSM_SHARED_RAM_BASE + 0xf3fd4);
			*ch->open = 0;

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
			__func__, ch->name, ch->n,
			((struct smd_7500_buffer*)ch->send)->buffer,
			((struct smd_7500_buffer*)ch->recv)->buffer );
		
		list_add(&ch->ch_list, smd_ch_closed_list);
		smd_ch_allocated[i] = 1;
		
		platform_device_register(&ch->pdev);
	}
}

int smd_7500_read(smd_channel_t *ch, void *data, int len) {
	struct smd_7500_buffer *p;
	int recvd;
	unsigned short mytail;
	unsigned char *buf;
	
	recvd = 0;
	buf = data;
	p = (struct smd_7500_buffer *)ch->recv;
	
	D("%s on ch %d\n", __func__, ch->n);
	
	if (len < 0)
		return -EINVAL;
	
	notify_other_smd(ch->n);
	
	mytail = *p->tail;
	
	while (recvd < len) {
		mytail = (mytail + 1) % p->size;
		if (buf)
			*buf++ = *(p->buffer + mytail);
		++recvd;
		if ((recvd % BURST_SIZE) == 0) {
			*p->tail = mytail;
			notify_other_smd(ch->n);
		}
	}
	*p->tail = mytail;
	D("received %d <- %d\n", recvd, ch->n);

	notify_other_smd(ch->n);

	return recvd;
}

int smd_7500_write(smd_channel_t *ch, const void *data, int len) {
	struct smd_7500_buffer *p;
	int sent;
	unsigned short myhead;
	const unsigned char *buf;
	
	sent = 0;
	buf = data;
	p = (struct smd_7500_buffer *)ch->send;
	
	D("%s: on ch %d, %d bytes\n", __func__, ch->n, len);
	
	ch->check_for_data(ch);

	if (len < 0)
		return -EINVAL;
	
	myhead = *p->head;
	notify_other_smd(ch->n);
	while (len > 0) {
		myhead = (myhead + 1) % p->size;
		while (myhead == *p->tail)
			udelay(5);
		
		--len;
		
		*(p->buffer + myhead) = *buf++;
		++sent;
		
		if ((sent % BURST_SIZE) == 0 || (myhead == (p->size - 1))) {
			*p->head = myhead;
			notify_other_smd(ch->n);
		}
	}
	*p->head = myhead;

	notify_other_smd(ch->n);

	ch->check_for_data(ch);

	return sent;
}

int smd_7500_read_avail(smd_channel_t *ch) {
	struct smd_7500_buffer *p;
	p = (struct smd_7500_buffer *)ch->recv;
	return (*p->head - *p->tail + p->size) % p->size;
}

int smd_7500_write_avail(smd_channel_t *ch) {
	struct smd_7500_buffer *p;
	p = (struct smd_7500_buffer *)ch->send;
	return p->size - ((*p->head - *p->tail + p->size) % p->size) - 1;
}

void smd_7500_check_for_data(smd_channel_t *ch) {
	struct smd_7500_buffer *p;
	if (ch->open && *ch->open) {
		p = (struct smd_7500_buffer *)ch->recv;
		if (*p->head != *p->tail) {
			if (ch->notify)
				ch->notify(ch->priv, SMD_EVENT_DATA);
			ch->recv->fHEAD = 0;
		}
		p = (struct smd_7500_buffer *)ch->send;
		if (*p->head != *p->tail) {
			notify_other_smd(ch->n);
			ch->send->fTAIL = 0;
		}
	}
	
}

void smd_7500_update_state(smd_channel_t *ch) {
	// Nothing to see here
	return;
}
