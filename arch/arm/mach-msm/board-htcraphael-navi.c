/*
 * board-htcraphael-navi.c - Raphael NaviPad
 *
 * Job Bolle <jb@b4m.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/microp-klt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif

#define MODULE_NAME "raph_navi_pad"

/*
 * Jobo: Driver for the navipad on the HTC Touch Pro (Raphael) and Diamond
 * The navipad has 7 buttons in a gpio matrix (left, up, center, down, right,
 * volume-up, volume-down) and a touch sensitive surface.
 * The left and right buttons have multiple functions. The touch pad is
 * used to determine where a button was pressed. The touch pad also
 * contains a scroll wheel around the center button. 
 *
 * Heavily based on
 *   microp-ksc.c (by Joe Hansche <madcoder@gmail.com>)
 * and
 *   drivers/input/misc/gpio_matrix.c (by Google, Inc.)
 *
 * This modules is technically an i2c chip driver but it also processes the
 * data, invokes its own gpio matrix driver, creates an input device, and
 * combines everything to report events based on touch pad and button input.
 * It even reports lid switch and controls keyboard backlight.
 * TODO: slice up into 2 or 3 modules and move lid switch into keyboard driver.
 * Improve wheel response, maybe layout.
 *

	struct i2c_board_info i2c_devices[] = {
		{
			// Raphael NaviPad
			I2C_BOARD_INFO("raph_navi_pad", 0x62),
		},
	};
 */

struct raphnavi_info {
	int *cols;	// columns (inputs) in gpio matrix
	int ncols;
	int *rows;	// rows (outputs) in gpio matrix
	int nrows;
	int gpio_tp;	// signals touch pad activity
	int gpio_lid;	// keyboard slider lid switch
};


//#define RAPHNAVI_DEBUG
//#define RAPHNAVI_LID_SWITCH /* drivers/input/keyboard/microp-keypad.c handles this now. */

#define RAPHNAVI_WHEEL REL_Y
static int raphnavi_cols[] = { 40, 41, 42 };
static int raphnavi_rows[] = { 32, 33, 34, 35 };

static struct raphnavi_info navi_info = {
	.rows = raphnavi_rows,
	.nrows = ARRAY_SIZE(raphnavi_rows),
	.cols = raphnavi_cols,
	.ncols = ARRAY_SIZE(raphnavi_cols),
	.gpio_tp = 94,
	.gpio_lid = 38,
};

static int raphnavi_keymap[] =
{
	KEY_RESERVED,
/* 1..6 = position sensed keys*/
	KEY_MENU, //KEY_HOME,
	KEY_SEND,
	KEY_LEFT,
	KEY_BACK,
	KEY_END,
	KEY_RIGHT,

/*7*/	KEY_RESERVED,
	KEY_RESERVED,
	KEY_LEFT, // impossible: = 1..3
/*10*/	KEY_RIGHT, // impossible: = 4..6
	KEY_RESERVED,
	KEY_DOWN,
	KEY_RESERVED,
	KEY_VOLUMEUP,
/*15*/	KEY_UP,
	KEY_RESERVED,
	KEY_VOLUMEDOWN,
	232, // center, select
};

/*
        0  lx|0x30    0x13|rx
      0 +----------------------+
        | 1   __| \__/ |__   4 |
   y __ |____| 3  /  \   6|____|
   0x22 |    |__  \__/  __|    |
        | 2     | /  \ |     5 |
        +----------------------+
 */

#define RAPHNAVI_I2C_MSGLEN 19
#define RAPHNAVI_PAD_AREAS  0
#define RAPHNAVI_PAD_LX	    1
#define RAPHNAVI_PAD_LY     2
#define RAPHNAVI_PAD_WHEEL   3
#define RAPHNAVI_PAD_TOUCH  4
#define RAPHNAVI_PAD_RX     5
#define RAPHNAVI_PAD_RY     6

#define RAPHNAVI_KLT_LED_MASK (   (1 << MICROP_KLT_LED_HOME) \
				| (1 << MICROP_KLT_LED_BACK) \
				| (1 << MICROP_KLT_LED_END) \
				| (1 << MICROP_KLT_LED_SEND) \
				| (1 << MICROP_KLT_LED_ACTION) \
				| MICROP_KLT_SYSLED_RING \
				| MICROP_KLT_SYSLED_BLINK \
				| MICROP_KLT_SYSLED_BREATHE \
				| MICROP_KLT_SYSLED_ROTATE \
				| MICROP_KLT_SYSLED_VERTICAL )


struct raphnavi {
	struct raphnavi_info *info;

	struct i2c_client *client;
	struct input_dev *inputdev;

	int tp_irq;
	int sw_irq;
	struct hrtimer timer;
	struct mutex lock;
	struct delayed_work work;
#ifdef CONFIG_ANDROID_POWER
	android_suspend_lock_t suspend_lock;
#endif

	unsigned long *btns_now;
	unsigned long *btns_prev;
	int lkey;
	int rkey;
	int ltouch;
	int rtouch;
	int swlid;

	struct {
		int touch;
		int left_x;
		int left_y;
		int right_x;
		int right_y;
		int wheel;
	} pad;

	int current_row;
	unsigned int changed_now : 1;
	unsigned int changed_prev : 1;
	unsigned int have_btns_down : 2;
};

static ktime_t raphnavi_gpio_poll_time = {.tv.nsec =  40 * NSEC_PER_MSEC };


static void raphnavi_report_key(struct raphnavi *navi, int keycode, int pressed)
{
	input_report_key(navi->inputdev, keycode, pressed);
	input_sync(navi->inputdev);
}

static void raphnavi_report_wheel(struct raphnavi *navi, int delta)
{
	input_report_rel(navi->inputdev, RAPHNAVI_WHEEL, delta);
	input_sync(navi->inputdev);
}
#ifdef RAPHNAVI_LID_SWITCH
static void raphnavi_report_lidswitch(struct raphnavi *navi, int opened)
{
	micropklt_set_ksc_bklt(opened);
	input_report_switch(navi->inputdev, SW_LID, !opened);
	input_sync(navi->inputdev);
}
#endif


static void raphnavi_button(struct raphnavi *navi,int btnidx,int pressed)
{
	int keycode;

	if (pressed)
		set_bit(btnidx,navi->btns_prev);
	else
		clear_bit(btnidx,navi->btns_prev);

	switch (btnidx) {
		case 3:
			if (pressed) {
				btnidx = navi->lkey = navi->ltouch;
			} else {
				btnidx = navi->lkey;
				navi->lkey = 0;
			}
			break;
		case 2:
			if (pressed) {
				btnidx = navi->rkey = navi->rtouch;
			} else {
				btnidx = navi->rkey;
				navi->rkey = 0;
			}
			break;
		default:
			btnidx += 7;
			break;
	}

	keycode = raphnavi_keymap[btnidx];
	if (navi->inputdev && keycode != KEY_RESERVED) {
#ifdef RAPHNAVI_DEBUG
		printk(KERN_WARNING "%s: key %2d code %3d is %s\n", __func__,
				btnidx, keycode, pressed ? "pressed" : "released");
#endif
		raphnavi_report_key(navi,keycode,pressed);
	}
}

static void raphnavi_pad(struct raphnavi *navi, char *data)
{
	int v;
	int leds;

#ifdef RAPHNAVI_DEBUG
	{
		int i;
		char msgtxt[3 * RAPHNAVI_I2C_MSGLEN + 1],one[4];

		for (msgtxt[0] = 0, i = 0;i < 8;i++) {
			sprintf(one," %02X",data[i]);
			strcat(msgtxt,one);
		}
		printk(KERN_WARNING "navi:%s\n", msgtxt);
	}
#endif
	leds = MICROP_KLT_LEDS_OFF;
	if (data[RAPHNAVI_PAD_TOUCH] == 0x82) {
		navi->pad.touch = 0;
	} else {
		if (data[RAPHNAVI_PAD_AREAS] & 0x02) {
			navi->pad.left_x = data[RAPHNAVI_PAD_LX];
			navi->pad.left_y = data[RAPHNAVI_PAD_LY];
			if (navi->pad.left_x > 0x30
					&& navi->pad.left_y > 0x10
					&& navi->pad.left_y < 0x33) {
				navi->ltouch = 3;
				leds |= (1 << MICROP_KLT_LED_HOME) | (1 << MICROP_KLT_LED_SEND);
			} else if (navi->pad.left_y < 0x22) {
				navi->ltouch = 1;
				leds |= (1 << MICROP_KLT_LED_HOME);
			} else {
				navi->ltouch = 2;
				leds |= (1 << MICROP_KLT_LED_SEND);
			}
		} else {
			navi->ltouch = 3;
		}

		if (data[RAPHNAVI_PAD_AREAS] & 0x08) {
			navi->pad.right_x = data[RAPHNAVI_PAD_RX];
			navi->pad.right_y = data[RAPHNAVI_PAD_RY];
			if (navi->pad.right_x < 0x13
					&& navi->pad.right_y > 0x10
					&& navi->pad.right_y < 0x33) {
				navi->rtouch = 6;
				leds |= (1 << MICROP_KLT_LED_BACK) | (1 << MICROP_KLT_LED_END);
			} else if (navi->pad.right_y < 0x22) {
				navi->rtouch = 4;
				leds |= (1 << MICROP_KLT_LED_BACK);
			} else {
				navi->rtouch = 5;
				leds |= (1 << MICROP_KLT_LED_END);
			}
		} else {
			navi->rtouch = 6;
		}

		v = 0;
		if ((data[RAPHNAVI_PAD_AREAS] & 0x44) == 0x44) {
			if (data[RAPHNAVI_PAD_AREAS] & 0x11) {
				navi->pad.wheel = 0xFF;
			} else {
				if (navi->pad.wheel != 0xFF) { // not 1st time, need good delta
					v = data[RAPHNAVI_PAD_WHEEL] - navi->pad.wheel;
					if (v > 0x4A/2)
						v -= 0x4A;
					else if (v < -0x4A/2)
						v += 0x4A;
					if (v) {
#ifdef RAPHNAVI_DEBUG
						printk(KERN_WARNING
							"%s: wheel = %d delta = %d\n",
							__func__, data[RAPHNAVI_PAD_WHEEL], v);
#endif
						leds |= MICROP_KLT_SYSLED_BLINK;
						raphnavi_report_wheel(navi,v);
					}
				}
				navi->pad.wheel = data[RAPHNAVI_PAD_WHEEL];
			}
		} else {
			navi->pad.wheel = 0xFF;
		}

		if (!v /*&& !data[RAPHNAVI_PAD_TOUCH]*/
				&& (data[RAPHNAVI_PAD_AREAS] & 0x04)) {
			v = data[RAPHNAVI_PAD_WHEEL];
			if (v > 55) // down
				leds |= (1 << MICROP_KLT_LED_ACTION);
			else if (v > 18 && v < 37) // up
				leds |= (1 << MICROP_KLT_LED_ACTION);
		}
	}
	micropklt_set_led_states(RAPHNAVI_KLT_LED_MASK, leds);
}

static int raphnavi_i2c_read(struct i2c_client *client, unsigned id, char *buf, int len)
{
	int r;
	char outbuffer[2] = { 0, 0 };

	outbuffer[0] = id;
	// maejrep: Have to separate the "ask" and "read" chunks
	r = i2c_master_send(client, outbuffer, 1);
	if (r < 0) {
		printk(KERN_WARNING "%s: error while asking for "
			"navi address %02x,%02x: %d\n", __func__, client->addr, id, r);
		return r;
	}
	mdelay(1);
	r = i2c_master_recv(client, buf, len);
	if (r < 0) {
		printk(KERN_ERR "%s: error while reading navi at "
			"address %02x,%02x: %d\n", __func__, client->addr, id, r);
		return r;
	}
	return 0;
}

static irqreturn_t raphnavi_irq_handler(int irq, void *dev_id)
{
	int i;
	struct raphnavi *navi = dev_id;


	if (irq == navi->tp_irq || irq == navi->sw_irq) {
		disable_irq(navi->tp_irq);
#ifdef RAPHNAVI_LID_SWITCH
		disable_irq(navi->sw_irq);
#endif
		schedule_work(&navi->work.work);
	} else {
		for(i = 0; i < navi->info->ncols; i++)
			disable_irq(gpio_to_irq(navi->info->cols[i]));
		for(i = 0; i < navi->info->nrows; i++)
			gpio_set_value(navi->info->rows[i],1);
		hrtimer_start(&navi->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
	}
#ifdef CONFIG_ANDROID_POWER
	android_lock_suspend(&navi->suspend_lock);
#endif
	return IRQ_HANDLED;
}

static void raphnavi_work(struct work_struct *work)
{
	struct raphnavi *navi;
	int err;
	char buffer[RAPHNAVI_I2C_MSGLEN];

	navi = container_of(work, struct raphnavi, work.work);
	mutex_lock(&navi->lock);
#ifdef RAPHNAVI_LID_SWITCH
	if (machine_is_htcraphael()) {
		int sw = !gpio_get_value(navi->info->gpio_lid);
		if (sw != navi->swlid) {
			navi->swlid = sw;
#ifdef RAPHNAVI_DEBUG
			printk(KERN_WARNING "%s: lid switch is %s\n",
					__func__, sw ? "open" : "closed");
#endif
			raphnavi_report_lidswitch(navi,!sw);
		}
	}
#endif
	err = raphnavi_i2c_read(navi->client, 1, buffer, RAPHNAVI_I2C_MSGLEN);
	if (!err)
		raphnavi_pad(navi,buffer);
	raphnavi_i2c_read(navi->client, 1, buffer, RAPHNAVI_I2C_MSGLEN); //XXX: maejrep: Why do we have to do this a second time?
	mutex_unlock(&navi->lock);
#ifdef RAPHNAVI_LID_SWITCH
	enable_irq(navi->sw_irq);
#endif
	enable_irq(navi->tp_irq);
#ifdef CONFIG_ANDROID_POWER
	android_unlock_suspend(&navi->suspend_lock);
#endif
}

static enum hrtimer_restart raphnavi_kp_timer(struct hrtimer *timer)
{
	struct raphnavi *navi = container_of(timer, struct raphnavi, timer);
	int row, col, btnidx;

	row = navi->current_row;
	if (row == navi->info->nrows) {
		row = 0;
		navi->changed_prev = navi->changed_now;
		navi->changed_now = 0;
		navi->have_btns_down = 0;
	} else {
		btnidx = row * navi->info->ncols;
		for(col = 0; col < navi->info->ncols; col++, btnidx++) {
			if (gpio_get_value(navi->info->cols[col]) ^ 1) {
				if (navi->have_btns_down < 3)
					navi->have_btns_down++;
				navi->changed_now |= !__test_and_set_bit(btnidx, navi->btns_now);
			} else
				navi->changed_now |= __test_and_clear_bit(btnidx, navi->btns_now);
		}
		gpio_set_value(navi->info->rows[row], 1);
		row++;
	}
	navi->current_row = row;
	if (row < navi->info->nrows) {
		gpio_set_value(navi->info->rows[row], 0);
		hrtimer_start(timer, ktime_set(0, 0), HRTIMER_MODE_REL);
		return HRTIMER_NORESTART;
	}

	if (navi->changed_now) {
		for(btnidx = row = 0; row < navi->info->nrows; row++) {
			for(col = 0; col < navi->info->ncols; col++, btnidx++) {
				int pressed = test_bit(btnidx, navi->btns_now);
				if (pressed != test_bit(btnidx, navi->btns_prev)) {
					raphnavi_button(navi,btnidx,pressed);
				}
			}
		}
	}

	if (navi->have_btns_down) {
		hrtimer_start(timer, raphnavi_gpio_poll_time, HRTIMER_MODE_REL);
	} else {
		for(row = 0; row < navi->info->nrows; row++)
			gpio_set_value(navi->info->rows[row], 0);
		for(col = 0; col < navi->info->ncols; col++)
			enable_irq(gpio_to_irq(navi->info->cols[col]));
#ifdef CONFIG_ANDROID_POWER
		android_unlock_suspend(&navi->suspend_lock);
#endif
	}
	return HRTIMER_NORESTART;
}

static int raphnavi_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct raphnavi *navi;
	struct input_dev *idev;
//	int irq_flags;
	unsigned int irq;
	int i;
	int btn_count;

	printk(KERN_INFO MODULE_NAME ": Initializing Raphael Navi Trackpad chip driver at addr: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR MODULE_NAME ": i2c bus not supported\n");
		return -EINVAL;
	}

	navi = kzalloc(sizeof *navi, GFP_KERNEL);
	if (navi < 0) {
		printk(KERN_ERR MODULE_NAME ": Not enough memory\n");
		return -ENOMEM;
	}
	mutex_init(&navi->lock);
	INIT_DELAYED_WORK(&navi->work, raphnavi_work);
	navi->info = &navi_info;
	navi->client = client;
	i2c_set_clientdata(client, navi);
//	local_irq_save(irq_flags);

	gpio_request(navi->info->gpio_tp, "raphnavi_tp");
	gpio_direction_input(navi->info->gpio_tp);
	navi->tp_irq = gpio_to_irq(navi->info->gpio_tp);
	request_irq(navi->tp_irq, raphnavi_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW, 
			"raphnavi_tp", navi);
	set_irq_wake(navi->tp_irq, 1);
	disable_irq(navi->tp_irq);

#ifdef RAPHNAVI_LID_SWITCH
	if (machine_is_htcraphael() || machine_is_htcraphael_cdma()) {
		gpio_request(navi->info->gpio_lid, "raphnavi_lid");
		gpio_direction_input(navi->info->gpio_lid);
		navi->sw_irq = gpio_to_irq(navi->info->gpio_lid);
		request_irq(navi->sw_irq, raphnavi_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"raphnavi_lid", navi);
		set_irq_wake(navi->sw_irq, 1);
		disable_irq(navi->sw_irq);
	}
#endif

	btn_count = navi->info->ncols * navi->info->nrows;
	navi->btns_now = kzalloc(sizeof (unsigned long) * BITS_TO_LONGS(btn_count), GFP_KERNEL);
	navi->btns_prev = kzalloc(sizeof (unsigned long) * BITS_TO_LONGS(btn_count), GFP_KERNEL);

	for(i = 0; i < navi->info->nrows; i++) {
		gpio_request(navi->info->rows[i], "raphnavi_row");
		gpio_configure(navi->info->rows[i], GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
	}
	for(i = 0; i < navi->info->ncols; i++) {
		gpio_request(navi->info->cols[i], "raphnavi_col");
		gpio_direction_input(navi->info->cols[i]);
	}
	for(i = 0; i < navi->info->ncols; i++) {
		irq = gpio_to_irq(navi->info->cols[i]);
		request_irq(irq, raphnavi_irq_handler, IRQF_TRIGGER_LOW, "raphnavi_gpio", navi);
		set_irq_wake(irq, 1);
		disable_irq(irq);
	}

	idev = input_allocate_device();
	if (idev) {
		idev->name = MODULE_NAME;

		set_bit(EV_KEY, idev->evbit);
		idev->keycodesize = sizeof(raphnavi_keymap[0]);
		idev->keycodemax = ARRAY_SIZE(raphnavi_keymap);
		idev->keycode = raphnavi_keymap;
		for (i = 0; i < ARRAY_SIZE(raphnavi_keymap); i++)
			if (raphnavi_keymap[i] != KEY_RESERVED)
				set_bit(raphnavi_keymap[i], idev->keybit);
		set_bit(EV_REL, idev->evbit);
		// Android only likes REL_ controls that do  REL_X, REL_Y _and_ BTN_MOUSE
		// I otoh like jog/shuttle wheels..
		input_set_capability(idev, EV_KEY, BTN_MOUSE);
		input_set_capability(idev, EV_REL, REL_X);
		input_set_capability(idev, EV_REL, REL_Y);
		input_set_capability(idev, EV_REL, RAPHNAVI_WHEEL);
#ifdef RAPHNAVI_LID_SWITCH
		if (machine_is_htcraphael()) {
			set_bit(EV_SW, idev->evbit);
			input_set_capability(idev, EV_SW, SW_LID);
		}
#endif
		if (!input_register_device(idev)) {
			navi->inputdev = idev;
		} else {
			navi->inputdev = 0;
		}
	} else {
	}
//	local_irq_restore(irq_flags);
#ifdef CONFIG_ANDROID_POWER
	navi->suspend_lock.name = "raphnavi_tp";
	android_init_suspend_lock(&navi->suspend_lock);
	android_lock_suspend(&navi->suspend_lock);
#endif
	navi->current_row = navi->info->nrows;
	navi->changed_now = 1;

	schedule_work(&navi->work.work);

	hrtimer_init(&navi->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	navi->timer.function = raphnavi_kp_timer;
	hrtimer_start(&navi->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
	return 0;
}

static int raphnavi_remove(struct i2c_client * client)
{
	struct raphnavi *navi = i2c_get_clientdata(client);
	int i;

#ifdef CONFIG_ANDROID_POWER
	android_uninit_suspend_lock(&navi->suspend_lock);
#endif
	hrtimer_cancel(&navi->timer);
	gpio_free(navi->info->gpio_tp);
	free_irq(navi->tp_irq,navi);
#ifdef RAPHNAVI_LID_SWITCH
	if (machine_is_htcraphael()) {
		gpio_free(navi->info->gpio_lid);
		free_irq(navi->sw_irq,navi);
	}
#endif
	for(i = 0; i < navi->info->nrows; i++)
		gpio_free(navi->info->rows[i]);
	for(i = 0; i < navi->info->ncols; i++)
		gpio_free(navi->info->cols[i]);
	for(i = 0; i < navi->info->ncols; i++)
		free_irq(gpio_to_irq(navi->info->cols[i]),navi);

	input_unregister_device(navi->inputdev);
	kfree(navi->btns_now);
	kfree(navi->btns_prev);
	kfree(navi);
	return 0;
}

#if CONFIG_PM
static int raphnavi_suspend(struct i2c_client * client, pm_message_t mesg)
{
#ifdef RAPHNAVI_DEBUG
	printk(KERN_INFO MODULE_NAME ": suspending device...\n");
#endif
	return 0;
}

static int raphnavi_resume(struct i2c_client * client)
{
#ifdef RAPHNAVI_DEBUG
	printk(KERN_INFO MODULE_NAME ": resuming device...\n");
#endif
	return 0;
}
#else
#define raphnavi_suspend NULL
#define raphnavi_resume NULL
#endif

static const struct i2c_device_id raphnavi_ids[] = {
        { "raph_navi_pad", 0 },
        { }
};

static struct i2c_driver raphnavi_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = raphnavi_ids,
	.probe = raphnavi_probe,
	.remove = raphnavi_remove,
#if CONFIG_PM
	.suspend = raphnavi_suspend,
	.resume = raphnavi_resume,
#endif
};

static int __init raphnavi_init(void)
{
	printk(KERN_INFO "raphnavi_tp: Registering Raphael NaviPad driver\n");
	return i2c_add_driver(&raphnavi_driver);
}

static void __exit raphnavi_exit(void)
{
	printk(KERN_INFO "raphnavi_tp: Unregistered Raphael NaviPad driver\n");
	i2c_del_driver(&raphnavi_driver);
}

MODULE_AUTHOR("Job Bolle");
MODULE_DESCRIPTION("Raphael NaviPad driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

module_init(raphnavi_init);
module_exit(raphnavi_exit);

