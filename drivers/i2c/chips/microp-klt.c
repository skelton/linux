/*
    microp-klt.c - i2c chip driver for microp-led
    
    Joe Hansche <madcoder@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <asm/io.h>
#include <asm/gpio.h>

#include <linux/microp-klt.h>

static int micropklt_read(struct i2c_client *, unsigned, char *, int);
static int micropklt_write(struct i2c_client *, const char *, int);

#define MODULE_NAME "microp-klt"

#if 0
 #define D(fmt, arg...) printk(KERN_DEBUG "[KLT] %s: " fmt "\n", __FUNCTION__, ## arg);
#else
 #define D(fmt, arg...) do {} while(0)
#endif

static struct microp_klt {
	struct i2c_client *client;
	struct mutex lock;
	u16 led_states;
	unsigned short version;
	struct led_classdev leds[MICROP_KLT_LED_CNT];
} *micropklt_t = 0;

static void micropklt_led_brightness_set(struct led_classdev *led_cdev,
                                         enum led_brightness brightness)
{
	struct microp_klt *data;
	struct i2c_client *client;
	char buffer[4] = { 0, 0, 0, 0 };
	int idx, b, state;

	if ( !strcmp(led_cdev->name, "klt::home") )
		idx = 0;
	else if ( !strcmp(led_cdev->name, "klt::back") )
		idx = 1;
	else if ( !strcmp(led_cdev->name, "klt::end") )
		idx = 2;
	else if ( !strcmp(led_cdev->name, "klt::send") )
		idx = 3;
	else if ( !strcmp(led_cdev->name, "klt::action") )
		idx = 4;
	else if ( !strcmp(led_cdev->name, "klt::lcd-bkl") || !strcmp(led_cdev->name, "lcd-backlight"))
		idx = 5;
	else if ( !strcmp(led_cdev->name, "klt::keypad-bkl") || !strcmp(led_cdev->name, "button-backlight"))
		idx = 6;
	else
		return;

	data = container_of(led_cdev, struct microp_klt, leds[idx]);
	client = data->client;

	mutex_lock(&data->lock);

	state = data->led_states;

	// idx 5 and 6 are bits 13 and 14
	if ( idx > 4 )
		idx += 8;

	b = 1U << idx;

	if ( brightness == LED_OFF )
		state &= ~b;
	else 
		state |= b;

	// lcd-backlight lets us do varied brightness
	if ( idx==5+8 /*&& brightness>0*/) {
		buffer[0] = MICROP_KLT_ID_LCD_BRIGHTNESS;
		buffer[1] = brightness/2 & 0xf0;

		printk(KERN_INFO MODULE_NAME ": Setting %s brightness to: 0x%02x\n", 
			led_cdev->name, buffer[1]);
		micropklt_write(client, buffer, 2);
		msleep(1);
	}

	if ( data->led_states != state ) {
		buffer[0] = MICROP_KLT_ID_LED_STATE;
		buffer[1] = 0xff & state;
		buffer[2] = 0xff & (state >> 8);
		data->led_states = state;
		micropklt_write(client, buffer, 3);
	}
	mutex_unlock(&data->lock);
}

int micropklt_set_led_states(unsigned leds_mask, unsigned leds_values)
{
	struct microp_klt *data;
	struct i2c_client *client;
	unsigned state;
	char buffer[4] = { 0, 0, 0, 0 };
	int r;

	data = micropklt_t;
	if (!data) return -EAGAIN;
	client = data->client;

	mutex_lock(&data->lock);
	state = data->led_states | (leds_mask & leds_values);
	state &= MICROP_KLT_ALL_LEDS & ~(leds_mask & ~leds_values);
	if (data->led_states != state) {
		data->led_states = state;
		buffer[0] = MICROP_KLT_ID_LED_STATE;
		buffer[1] = 0xff & state;
		buffer[2] = 0xff & (state >> 8);
		data->led_states = state;
		r = micropklt_write(client, buffer, 3);
	} else {
		r = 0;
	}
	mutex_unlock(&data->lock);
	return r;
}
EXPORT_SYMBOL(micropklt_set_led_states);

int micropklt_set_lcd_state(int on)
{
	return micropklt_set_led_states(1 << MICROP_KLT_BKL_LCD,on ? 1 << MICROP_KLT_BKL_LCD : 0);
}
EXPORT_SYMBOL(micropklt_set_lcd_state);

int micropklt_set_kbd_state(int on)
{
	return micropklt_set_led_states(1 << MICROP_KLT_BKL_KBD,on ? 1 << MICROP_KLT_BKL_KBD : 0);
}
EXPORT_SYMBOL(micropklt_set_kbd_state);

#define send_command(x) micropklt_write(client, x,ARRAY_SIZE(x));

void micropklt_lcd_ctrl(int v)
{
	struct microp_klt *data=micropklt_t;;
	struct i2c_client *client;
	if (!data) return -EAGAIN;
	client = data->client;

	// for power up
	char c1[]={0x20,0x48};
	char c2[]={0x20,0x0c};
        char c3[]={0x23,0,0};
	char c4[]={0x22,0x0};
	char c6[]={0x22,0x60};

	// for power down
	char c7[]={0x20,0x4c};
	char c8[]={0x40,0x10,0x00};
	char c9[]={0x20,0x08};

	
        int r;

        data = micropklt_t;
        if (!data) return -EAGAIN;
        client = data->client;

	switch(v) {
	case 1: // power up
		send_command(c1);
		break;
	case 2: 
		send_command(c2);
		break;
	case 3:
		send_command(c3);
		break;
	case 4: // power down
		send_command(c7);
		send_command(c3);
		send_command(c8);
		send_command(c3);
		break;
	case 5: 
		send_command(c9);
		break;
		
	}
}

EXPORT_SYMBOL(micropklt_lcd_ctrl);

static int micropklt_remove(struct i2c_client *client)
{
	struct microp_klt *data;
	int i;

	data = i2c_get_clientdata(client);
	
	micropklt_set_led_states(MICROP_KLT_ALL_LEDS, MICROP_KLT_DEFAULT_LED_STATES);

	for (i=0; i<ARRAY_SIZE(data->leds); i++) {
		led_classdev_unregister(&data->leds[i]);
	}

	kfree(data);
	micropklt_t = NULL;
	return 0;
}

static int micropklt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct microp_klt *data;
	int supported, r, i;
	char buf[3] = { 0, 0, 0 };

	printk(KERN_INFO MODULE_NAME ": Initializing MicroP-LED chip driver at "
	       "addr: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR MODULE_NAME ": i2c bus not supported\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof *data, GFP_KERNEL);
	if (data < 0) {
		printk(KERN_ERR MODULE_NAME ": Not enough memory\n");
		return -ENOMEM;
	}

	mutex_init(&data->lock);
	mutex_lock(&data->lock);

	data->client = client;
	i2c_set_clientdata(client, data);
	micropklt_t = data;

	// Read version
	micropklt_read(client, MICROP_KLT_ID_VERSION, buf, 2);
	
	/* Check version against what we think we should support
	 * Known supported versions: 
	 *   0105, 0205, 0a05, 0b05, 0d02
	 *   8182, 8185
	 *   0c82, 0c85
	 */
	supported = 0;
	switch (buf[0]) {
	case 0x01:
	case 0x02:
	case 0x0a:
	case 0x0b:
		switch (buf[1])	{
		case 0x01:
		case 0x05:
			supported = 1;
			break;
		}
		break;
	case 0x0d:
		switch (buf[1])	{
		case 0x02:
			supported = 1;
			break;
		}
		break;
	case 0x81:
	case 0x0c:
		switch (buf[1])	{
		case 0x82:
		case 0x85:
			supported = 1;
			break;
		}
		break;
	}
	data->version = (buf[0] << 8) | buf[1];

	if (!supported) {
		printk(KERN_WARNING MODULE_NAME ": This hardware is not yet supported: %04x\n", data->version);
		r = -ENOTSUPP;
		goto fail;
	}
	
	data->led_states = MICROP_KLT_DEFAULT_LED_STATES;

	data->leds[0].name = "klt::home";
	data->leds[0].brightness = LED_OFF;
	data->leds[0].brightness_set = micropklt_led_brightness_set;

	data->leds[1].name = "klt::back";
	data->leds[1].brightness = LED_OFF;
	data->leds[1].brightness_set = micropklt_led_brightness_set;

	data->leds[2].name = "klt::end";
	data->leds[2].brightness = LED_OFF;
	data->leds[2].brightness_set = micropklt_led_brightness_set;

	data->leds[3].name = "klt::send";
	data->leds[3].brightness = LED_OFF;
	data->leds[3].brightness_set = micropklt_led_brightness_set;

	data->leds[4].name = "klt::action";
	data->leds[4].brightness = LED_OFF;
	data->leds[4].brightness_set = micropklt_led_brightness_set;

	data->leds[5].name = "klt::lcd-bkl";
	data->leds[5].brightness = 0x90;
	data->leds[5].brightness_set = micropklt_led_brightness_set;

	data->leds[6].name = "klt::keypad-bkl";
	data->leds[6].brightness = LED_OFF;
	data->leds[6].brightness_set = micropklt_led_brightness_set;

#ifdef CONFIG_ANDROID_PMEM
	data->leds[5].name = "lcd-backlight";
	data->leds[6].name = "button-backlight";
#endif

	for (i=0; i<ARRAY_SIZE(data->leds); i++) {
		r = led_classdev_register(&client->dev, &data->leds[i]);
		if (r < 0) {
			goto err_led_classdev_register_failed;
			break;
		}
	}

	mutex_unlock(&data->lock);

	// Set default LED state
	micropklt_set_led_states(MICROP_KLT_ALL_LEDS, MICROP_KLT_DEFAULT_LED_STATES);

	printk(KERN_INFO MODULE_NAME ": Initialized MicroP-LED chip revision v%04x\n", data->version);

	return 0;

err_led_classdev_register_failed:
	printk(KERN_ERR MODULE_NAME ": led_classdev_register(%d) failed: %d\n", i, r);
	for (i=i; i>=0; i--) {
		led_classdev_unregister(&data->leds[i]);
	}
fail:
	kfree(data);
	micropklt_t = 0;
	return r;
}

static int micropklt_write(struct i2c_client *client, const char *sendbuf, int len)
{
	int r;

	r = i2c_master_send(client, sendbuf, len);
	if (r < 0) {
		printk(KERN_ERR "Couldn't send ch id %02x\n", sendbuf[0]);
	} else {
		D("  >>> 0x%02x, 0x%02x -> %02x %02x", client->addr,
		         sendbuf[0], (len > 1 ? sendbuf[1] : 0), 
		         (len > 2 ? sendbuf[2] : 0));
	}
	return r;
}

static int micropklt_read(struct i2c_client *client, unsigned id, char *buf, int len)
{
	int r;
	char outbuffer[2] = { 0, 0 };

	outbuffer[0] = id;
	
	// Have to separate the "ask" and "read" chunks
	r = i2c_master_send(client, outbuffer, 1);
	if (r < 0) {
		printk(KERN_WARNING "micropklt_read: error while asking for "
			"data address %02x,%02x: %d\n", client->addr, id, r);
		return r;
	}
	mdelay(1);
	r = i2c_master_recv(client, buf, len);
	if (r < 0) {
		printk(KERN_ERR "micropklt_read: error while reading data at "
			"address %02x,%02x: %d\n", client->addr, id, r);
		return r;
	}
	D("  <<< 0x%02x, 0x%02x -> %02x %02x", client->addr, id, buf[0], buf[1]);
	return 0;
}

#if CONFIG_PM
static int micropklt_suspend(struct i2c_client *client, pm_message_t mesg)
{
	D("suspending device...");
//	char cmd[]={0x20,0x08};
//	send_command(cmd);
//	micropklt_lcd_ctrl(4);
//	micropklt_set_led_states(0x10,0x10);
	return 0;
}

static int micropklt_resume(struct i2c_client *client)
{
	D("resuming device...");
//	char cmd[]={0x20,0x48};
//	send_command(cmd);
//	micropklt_lcd_ctrl(1);
//	micropklt_set_led_states(0x10,0x00);
	return 0;
}
#else
 #define micropklt_suspend NULL
 #define micropklt_resume NULL
#endif

static const struct i2c_device_id microp_klt_ids[] = {
	{ "microp-klt", 0 },
	{ }
};

static struct i2c_driver micropklt_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = microp_klt_ids,
	.probe = micropklt_probe,
	.remove = micropklt_remove,
	.suspend = micropklt_suspend,
	.resume = micropklt_resume,
};

static int __init micropklt_init(void)
{
	printk(KERN_INFO "microp-klt: Registering MicroP-LED driver\n");
	return i2c_add_driver(&micropklt_driver);
}

static void __exit micropklt_exit(void)
{
	printk(KERN_INFO "microp-klt: Unregistered MicroP-LED driver\n");
	i2c_del_driver(&micropklt_driver);
}

MODULE_AUTHOR("Joe Hansche <madcoder@gmail.com>");
MODULE_DESCRIPTION("MicroP-LED chip driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

module_init(micropklt_init);
module_exit(micropklt_exit);
