/*
    microp-keypad.c - i2c keyboard driver found on certain HTC Phones
    Depends on microp-ksc and microp-klt i2c chip drivers
    
    Joe Hansche <madcoder@gmail.com>
    Based in part on htc-spi-kbd.c from Kevin2

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/gpio.h>

#include <linux/microp-keypad.h>
#include <linux/microp-ksc.h>
#include <linux/microp-klt.h>

#define MODULE_NAME "microp-keypad"

extern int micropklt_set_ksc_notifications(int on);
extern int micropklt_set_led_state(unsigned short state);

extern int micropksc_read_scancode(unsigned char *scancode, unsigned char *isdown);


// This is raph800's default keymap.  can be remapped by userland
static int microp_keymap[] = {
        KEY_RESERVED, // invalid
        KEY_TAB,
        KEY_Q,
        KEY_W,
        KEY_E,
        KEY_R,
        KEY_T,
        KEY_Y,
        KEY_1,
        KEY_U,
        KEY_RESERVED, // 0x0a
        KEY_RESERVED, // 0x0b
        KEY_I,
        KEY_O,
        KEY_P,
        KEY_BACKSPACE,
        KEY_CAPSLOCK,
        KEY_A,
        KEY_4,
        KEY_RESERVED, // 0x13
        KEY_RESERVED, // 0x14
        KEY_S,
        KEY_D,
        KEY_F,
        KEY_G,
        KEY_H,
        KEY_J,
        KEY_K,
        KEY_7,
        KEY_RESERVED, // 0x1d
        KEY_RESERVED, // 0x1e
        KEY_L,
        KEY_ENTER,
        KEY_LEFTSHIFT,
        KEY_Z,
        KEY_X,
        KEY_C,
        KEY_V,
        KEY_9,
        KEY_RESERVED, // 0x27
        KEY_RESERVED, // 0x28
        KEY_B,
        KEY_N,
        KEY_M,
        KEY_RIGHTSHIFT,
        KEY_UP,
        KEY_0,
        KEY_LEFTCTRL,
        KEY_2,
        KEY_RESERVED, // 0x31
        KEY_RESERVED, // 0x32
        KEY_FN,
        KEY_TEXT,     // TXT/SMS
        KEY_MINUS,
        KEY_UNKNOWN,  // SYM/Data
        KEY_SPACE,
        KEY_COMMA,
        KEY_DOT,
        KEY_5,
        KEY_RESERVED, // 0x3b
        KEY_RESERVED, // 0x3c
        KEY_RIGHT,
        KEY_DOWN,
        KEY_LEFT,
        KEY_EQUAL,
        KEY_SLASH,
        KEY_3,
        KEY_6,
        KEY_8,
        KEY_RESERVED, // 0x45
        KEY_RESERVED, // 0x46
        KEY_EMAIL,
};


static struct microp_keypad {
	struct mutex lock;
	struct delayed_work work;

	struct microp_keypad_platform_data *pdata;
	struct platform_device *pdev;

	struct input_dev *input;
	int keycount;

	int clamshell_open:1;
	int keypress_irq;
} * microp_keypad_t;

static irqreturn_t microp_keypad_interrupt(int irq, void *handle)
{
	struct microp_keypad *data;
	data = (struct microp_keypad *)handle;

	disable_irq(data->keypress_irq);

	schedule_work(&data->work.work);
	return IRQ_HANDLED;
}

#if 0
static irqreturn_t microp_keypad_clam_interrupt(int irq, void *handle)
{
	struct microp_keypad *data;

	data = (struct microp_keypad *)handle;
	
	printk("Clamshell slider interrupt %d received..\n", irq);

	printk(KERN_INFO "microp_keypad: Clamshell is %s keyboard...\n", gpio_get_value(39) ? "open, resetting" : "closed, disabling");
	return IRQ_HANDLED;
}
#endif

static void microp_keypad_work(struct work_struct *work)
{
	struct microp_keypad *data;
	struct microp_keypad_platform_data *pdata;
	unsigned char key, isdown;
	
	data = container_of(work, struct microp_keypad, work.work);
	pdata = data->pdata;
	key = 0;

	mutex_lock(&data->lock);
	
	micropksc_read_scancode(&key, &isdown);
	if (key != 0)
	{
#if defined(MICROP_DEBUG) && MICROP_DEBUG
		printk(KERN_INFO " :::   Scancode = %02x; currently pressed: %01x\n", key, isdown);
#endif
		if (key < ARRAY_SIZE(microp_keymap))
		{
			input_report_key(data->input, microp_keymap[key], isdown);
			input_event(data->input, EV_MSC, MSC_SCAN, key);
			input_sync(data->input);
#if defined(MICROP_DEBUG) && MICROP_DEBUG
			printk(KERN_INFO "       Input keycode = %d, scancode = %d\n", microp_keymap[key], key);
#endif
			//XXX: do we need multiple keys?  break here?
		}
	}

	/* REQUEST_NOTIFICATIONS */
	micropklt_set_ksc_notifications(1);

	mutex_unlock(&data->lock);

	enable_irq(data->keypress_irq);
}

static int microp_keypad_remove(struct platform_device *pdev)
{
	struct microp_keypad *data;
	struct microp_keypad_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	data = platform_get_drvdata(pdev);
	
	if (pdata->backlight_gpio > 0)
		gpio_set_value(pdata->backlight_gpio, 0);

	micropklt_set_ksc_notifications(0);

	if (data->keypress_irq > 0)
		free_irq(data->keypress_irq, data);

#if 0
	if ( pdata->clamshell.irq > 0 )
	{
		free_irq(pdata->clamshell.irq, data);
	}
#endif
	
	flush_scheduled_work();
	kfree(data);
	printk("microp_keypad_remove\n");
	return 0;
}

static int microp_keypad_probe(struct platform_device *pdev)
{
	struct microp_keypad *data;
	struct input_dev *input = NULL;
	struct microp_keypad_platform_data *pdata = pdev->dev.platform_data;
	int r, i;
	unsigned char key;
	
	printk(KERN_INFO MODULE_NAME ": Initializing MicroP keypad driver\n");

	data = kzalloc(sizeof *data, GFP_KERNEL);
	if (data < 0)
	{
		printk(KERN_ERR MODULE_NAME ": Not enough memory\n");
		return -ENOMEM;
	}
	
	mutex_init(&data->lock);
	INIT_DELAYED_WORK(&data->work, microp_keypad_work);
	
	// Initialize input device
	input = input_allocate_device();
	if (!input)
		goto fail;
	input->name = MODULE_NAME;
	set_bit(EV_KEY, input->evbit);
	set_bit(EV_MSC, input->evbit);
	set_bit(MSC_SCAN, input->mscbit);
	
	input->keycodesize = sizeof(microp_keymap[0]);
	input->keycodemax = ARRAY_SIZE(microp_keymap);
	input->keycode = microp_keymap;
	for (i = 0; i < ARRAY_SIZE(microp_keymap); i++)
	{
		if (microp_keymap[i] != KEY_RESERVED)
		{
			set_bit(microp_keymap[i], input->keybit);
		}
	}
	data->keycount = i;
	
	r = input_register_device(input);
	if (r)
		goto fail;

	data->pdev = pdev;
	data->input = input;
	data->pdata = pdata;
	platform_set_drvdata(pdev, data);
	
	data->keypress_irq = 0;
	for (i = 0; i < pdev->num_resources; i++)
	{
		if (pdev->resource[i].flags == IORESOURCE_IRQ && 
			pdev->resource[i].start > 0)
		{
			data->keypress_irq = pdev->resource[i].start;
			r = request_irq(data->keypress_irq, microp_keypad_interrupt, 
		                IRQF_TRIGGER_FALLING | IRQF_SAMPLE_RANDOM, 
		                MODULE_NAME, data);
			if (r < 0)
			{
				printk(KERN_ERR "Couldn't request IRQ %d; error: %d\n", data->keypress_irq, r);
				goto fail;
			}
			break;
		}
	}
	if (!data->keypress_irq)
	{
		printk(KERN_ERR MODULE_NAME ": not using IRQ!  polling not implemented\n");
		goto fail;
	}
	if ( pdata->clamshell.gpio > 0 )
	{
		data->clamshell_open = !!gpio_get_value(pdata->clamshell.gpio);
		printk(KERN_INFO MODULE_NAME ": Clamshell status: %d\n", data->clamshell_open);
		//TODO: set this to 0, and only enable when keypad is open
		micropklt_set_ksc_notifications(1);
	} else {
		micropklt_set_ksc_notifications(1);
	}

	//TODO: turn this off; on keypress, turn it on, with timeout delay after last keypress to turn it off
	if ( pdata->backlight_gpio > 0 )
	{
		gpio_direction_output( pdata->backlight_gpio, 1 );
	}

	microp_keypad_t = data;

	printk("microp_keypad_probe done\n");
	return 0;
fail:
	input_unregister_device(input);
	kfree(data);
	return -ENOSYS;
}

#if CONFIG_PM
static int microp_keypad_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct microp_keypad_platform_data *pdata = pdev->dev.platform_data;
	if (pdata->backlight_gpio > 0)
		gpio_set_value(pdata->backlight_gpio, 0);
	return 0;
}

static int microp_keypad_resume(struct platform_device *pdev)
{
	struct microp_keypad_platform_data *pdata = pdev->dev.platform_data;
	if (pdata->backlight_gpio > 0)
		gpio_set_value(pdata->backlight_gpio, 1);
	return 0;
}
#else
 #define microp_keypad_suspend NULL
 #define microp_keypad_resume NULL
#endif

static struct platform_driver microp_keypad_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.probe = microp_keypad_probe,
	.remove = microp_keypad_remove,
#if defined(CONFIG_PM)
	.suspend = microp_keypad_suspend,
	.resume = microp_keypad_resume,
#endif
};

static int __init microp_keypad_init(void)
{
	printk(KERN_INFO "Registering MicroP keypad driver\n");
	return platform_driver_register(&microp_keypad_driver);
}

static void __exit microp_keypad_exit(void)
{
	printk(KERN_INFO "Unregistered MicroP keypad driver\n");
	platform_driver_unregister(&microp_keypad_driver);
}

MODULE_AUTHOR("Joe Hansche <madcoder@gmail.com>");
MODULE_DESCRIPTION("MicroP keypad driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

late_initcall(microp_keypad_init);
module_exit(microp_keypad_exit);
