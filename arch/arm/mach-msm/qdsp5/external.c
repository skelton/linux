/* arch/arm/mach-msm/qdsp5/external.c
 *
 * Handle power speaker/mic bias for devices not controlled by AMSS
 *
 * Copyright (C) 2010 HUSSON Pierre-Hugues <phhusson@free.fr>
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <mach/amss_para.h>

//from board-htcrhodium-audio
void enable_speaker_rhod(void);
void disable_speaker_rhod(void);
void speaker_vol_rhod(int);

void enable_speaker(void) {
	if(machine_is_htcblackstone()) {
		gpio_set_value(57,1);
	} else if(machine_is_htcrhodium()) {
		enable_speaker_rhod();
	}
}

void disable_speaker(void) {
	if(machine_is_htcblackstone()) {
		gpio_set_value(57, 0);
	} else if(machine_is_htcrhodium()) {
		disable_speaker_rhod();
	}
}

void speaker_vol(int arg) {
	if(machine_is_htcrhodium())
		speaker_vol_rhod(arg);
}
