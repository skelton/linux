/* drivers/input/touchscreen/msm_ts.c
 *
 * By Octavian Voicu <octavian@voicu.gmail.com>
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

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>

#define MSM_TS_ABS_X_MIN	0
#define MSM_TS_ABS_X_MAX	479
#define MSM_TS_ABS_Y_MIN	0
#define MSM_TS_ABS_Y_MAX	639
#define MSM_TS_ABS_PRESSURE_MIN 0
#define MSM_TS_ABS_PRESSURE_MAX 1

#define MSM_TS_LCD_WIDTH	480
#define MSM_TS_LCD_HEIGHT	640

/* Touchscreen registers */
#define TSSC_CTL		0x100
#define TSSC_STATUS		0x10c
#define TSSC_DATA_LO_AVE	0x110
#define TSSC_DATA_UP_AVE	0x114

void msmfb_update(struct fb_info *info, uint32_t left, uint32_t top,
	uint32_t eright, uint32_t ebottom);
void msm_vkeyb_plot(int x,int y,unsigned short c);

/* Registered framebuffers */
extern struct fb_info *registered_fb[];

/* Address of video memory buffer (msm_fb transfers this on the screen using dma) */
void __iomem *msm_ts_fbram;

/* returns whether position was inside vkeyb, so as to eat event */
typedef int msm_ts_handler_t(int, int, int);
msm_ts_handler_t *msm_ts_handler;

static void do_softint(struct work_struct *work);

static struct input_dev *msm_ts_dev;
static DECLARE_DELAYED_WORK(work, do_softint);

static int calib_step, calib_xmin, calib_xmax, calib_ymin, calib_ymax;
static int irq_disabled;

/* lavender.t: (temporarily) add a kernel option to set calibration. */
static int __init msmts_calib_setup(char *str)
{
	int xmin, ymin, xmax, ymax;
	char* endp;
	int ok = 0;

	endp = NULL;
	xmin = simple_strtol(str, &endp, 0);

	if (endp > str && *endp == '.')
	{
		str = endp + 1;
		endp = NULL;
		ymin = simple_strtol(str, &endp, 0);

		if (endp > str && *endp == '.')
		{
			str = endp + 1;
			endp = NULL;
			xmax = simple_strtol(str, &endp, 0);

			if (endp > str && *endp == '.')
			{
				str = endp + 1;
				endp = NULL;
				ymax = simple_strtol(str, &endp, 0);

				if (endp > str)
				{
					calib_step = 3;
					calib_xmin = xmin;
					calib_ymin = ymin;
					calib_xmax = xmax;
					calib_ymax = ymax;
					ok = 1;
				}
			}
		}
	}

	return ok;
}

__setup("msmts_calib=", msmts_calib_setup);

static void do_softint(struct work_struct *tmp_work)
{
	unsigned long status, data;
	int absx, absy, touched, x, y;
	int vkey;
	static int prev_absx, prev_absy, prev_touched = 0, nrskipped = 0;

	/* Draw two red dots during calibration (in upper-left and lower-right corners) */
	if (calib_step > 0 && calib_step < 3) {
		if (msm_ts_fbram) {
			for (x = 0;x < 3;x++) {
				for (y = 0;y < 3;y++) {
					msm_vkeyb_plot(x,y,0xF800);
					msm_vkeyb_plot(MSM_TS_LCD_WIDTH - 1 - x, MSM_TS_LCD_HEIGHT - 1 - y, 0xF800);
					msmfb_update(registered_fb[0], 0, 0, MSM_TS_LCD_WIDTH, MSM_TS_LCD_HEIGHT);
				}
			}
		}
	}

	/* Read status and data */
	status = readl(MSM_TS_BASE + TSSC_STATUS);
	data = readl(MSM_TS_BASE + TSSC_DATA_LO_AVE);

	/* Without next 2 lines, position won't get updated */
	writel(readl(MSM_TS_BASE + TSSC_CTL) & ~0x400, MSM_TS_BASE + TSSC_CTL);
	writel(readl(MSM_TS_BASE + TSSC_CTL) & ~0xc00, MSM_TS_BASE + TSSC_CTL);

	/* Get stylus position and press state */
	absx = data & 0xFFFF;
	absy = data >> 16;
	touched = status & 0x100 ? 1 : 0;

	/* Only do something when the touch state or position have changed */
	if (absx != prev_absx || absy != prev_absy || touched != prev_touched) {
		/* First three tap are to calibrate upper-left and lower-right corners */
		if (calib_step < 3) {
			if (touched && !prev_touched) {
				if (calib_step == 1) {
					/* This is upper-left corner */
					calib_xmax = absx;
					calib_ymin = absy;
					printk(KERN_WARNING "msm_ts: maxx=%04x miny=%04x\n", calib_xmax, calib_ymin);
					printk(KERN_WARNING "Calibration step 2: tap red dot in lower-right corner\n");
				} else if(calib_step == 2) {
					/* This is lower-right corner */
					calib_xmin = absx;
					calib_ymax = absy;
					printk(KERN_WARNING "msm_ts: minx=%04x maxy=%04x\n", calib_xmin, calib_ymax);
					printk(KERN_WARNING "Calibration successful!\n\n# ");
				} else {
					printk(KERN_WARNING "Calibration step 1: tap red dot in upper-left corner\n");
				}
				calib_step++;
			}
		} else {
			/* On stylus up, use same coordinates as last position we had (since they're 0 otherwise) */
			if (!touched) {
				absx = prev_absx;
				absy = prev_absy;
			}

			/* Calculate coordinates in pixels, clip to screen and make sure we don't to divisions by zero */
			x = MSM_TS_LCD_WIDTH - 1 - (absx - calib_xmin) * MSM_TS_LCD_WIDTH / (calib_xmax == calib_xmin ? MSM_TS_LCD_HEIGHT : calib_xmax - calib_xmin);
			if (x < 0) x = 0;
			if (x >= MSM_TS_LCD_WIDTH) x = MSM_TS_LCD_WIDTH - 1;
			y = (absy - calib_ymin) * MSM_TS_LCD_HEIGHT / (calib_ymax == calib_ymin ? MSM_TS_LCD_HEIGHT : calib_ymax - calib_ymin);
			if (y < 0) y = 0;
			if (y >= MSM_TS_LCD_HEIGHT) y = MSM_TS_LCD_HEIGHT - 1;

			/* Call our handler if it's registered -- the virtual keyboards gets data from this */
			if (msm_ts_handler) {
				nrskipped = 0;
				vkey = (*msm_ts_handler)(x, y, touched);
			} else {
				vkey = 0;
			}

			/* Send data to linux input system, if not eaten by vkeyb */
			if (touched && !vkey) {
				input_report_abs(msm_ts_dev, ABS_X, x);
				input_report_abs(msm_ts_dev, ABS_Y, y);
				input_report_abs(msm_ts_dev, ABS_PRESSURE, MSM_TS_ABS_PRESSURE_MAX);
				input_report_key(msm_ts_dev, BTN_TOUCH, 1);
			} else {
				input_report_abs(msm_ts_dev, ABS_PRESSURE, MSM_TS_ABS_PRESSURE_MIN);
				input_report_key(msm_ts_dev, BTN_TOUCH, 0);
			}

			input_sync(msm_ts_dev);
		}

		prev_absx = touched ? absx : 0;
		prev_absy = touched ? absy : 0;
		prev_touched = touched;
	} else {
		/* Let the virtual keyboard redraw itself anyway */
		if (msm_ts_handler) {
			/* Call it at least four times per second if no updates */
			if (nrskipped >= 5) {
				nrskipped = 0;
				(*msm_ts_handler)(0, 0, -1);
			} else {
				nrskipped++;
			}
		}
	}

	if (irq_disabled) {
		enable_irq(INT_TCHSCRN1);
		irq_disabled = 0;
	}

	/* Reschedule ourselves; poll the touchscreen 20 times each second */
	schedule_delayed_work(&work, HZ / 20);
}

static irqreturn_t msm_ts_interrupt(int irq, void *dev)
{
	printk(KERN_WARNING "IRQ %d fired! XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n\n\n\n", irq);

	if (!irq_disabled) {
		disable_irq_nosync(irq);
		irq_disabled = 1;
	}

	schedule_delayed_work(&work, HZ / 20);

	return IRQ_HANDLED;
}

static int __init msm_ts_init(void)
{
	int err;

	msm_ts_fbram = registered_fb[0] ? registered_fb[0]->screen_base : 0;
	if (!msm_ts_fbram) {
		printk(KERN_WARNING "msm_ts: no framebuffer detected!\n");
	}

	printk(KERN_WARNING "msm_ts: TSSC_CTL=%08x TSSC_STATUS=%08x\n", (unsigned int) readl(MSM_TS_BASE + TSSC_CTL), (unsigned int) readl(MSM_TS_BASE + TSSC_STATUS));

	msm_ts_dev = input_allocate_device();
	if (!msm_ts_dev)
		return -ENOMEM;

	msm_ts_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	msm_ts_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(msm_ts_dev, ABS_X,
		MSM_TS_ABS_X_MIN, MSM_TS_ABS_X_MAX, 0, 0);
	input_set_abs_params(msm_ts_dev, ABS_Y,
		MSM_TS_ABS_Y_MIN, MSM_TS_ABS_Y_MAX, 0, 0);
	input_set_abs_params(msm_ts_dev, ABS_PRESSURE,
		MSM_TS_ABS_PRESSURE_MIN, MSM_TS_ABS_PRESSURE_MAX, 0, 0);

	msm_ts_dev->name = "MSM touchscreen";
	msm_ts_dev->phys = "msm_ts/input0";

	if (request_irq(INT_TCHSCRN1, msm_ts_interrupt, IRQF_DISABLED, "msm_ts", 0) < 0) {
		printk(KERN_ERR "msm_ts: Can't allocate irq %d\n", INT_TCHSCRN1);
		err = -EBUSY;
		goto fail1;
	}

	err = input_register_device(msm_ts_dev);
	if (err)
		goto fail2;

	/* Call our work function first time */
	schedule_delayed_work(&work, HZ);

	printk(KERN_WARNING "msm_ts_init successful\n");

	return 0;

 fail2:	free_irq(INT_TCHSCRN1, NULL);
	cancel_delayed_work(&work);
	flush_scheduled_work();
 fail1:	input_free_device(msm_ts_dev);
	return err;
}

static void __exit msm_ts_exit(void)
{
	free_irq(INT_TCHSCRN1, NULL);
	cancel_delayed_work(&work);
	flush_scheduled_work();
	input_unregister_device(msm_ts_dev);
}

module_init(msm_ts_init);
module_exit(msm_ts_exit);

MODULE_AUTHOR("Octavian Voicu, octavian.voicu@gmail.com");
MODULE_DESCRIPTION("MSM touchscreen driver");
MODULE_LICENSE("GPL");
