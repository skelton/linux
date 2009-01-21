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
#include <linux/delay.h>
#include <linux/mutex.h>
#include <asm/io.h>
#include <asm/gpio.h>

#include <linux/microp-klt.h>

static int micropklt_read(struct i2c_client *, unsigned, char *, int);
static int micropklt_write(struct i2c_client *, const char *, int);

#define MODULE_NAME "microp-klt"

static struct microp_klt {
	struct i2c_client *client;
	struct mutex lock;
	u16 led_states;
	unsigned short version;
} * micropklt_t;

int micropklt_set_ksc_notifications(int on)
{
	struct microp_klt *data;
	struct i2c_client *client;
	char buffer[3] = "\0\0\0";
	int r;
	
	data = micropklt_t;
	if (!data) return -EAGAIN;
	client = data->client;

	mutex_lock(&data->lock);
	
	buffer[0] = MICROP_KLT_ID_KSC_NOTIFICATIONS;
	buffer[1] = !!on;
	
	r = micropklt_write(client, buffer, 2);
	if (r < 0)
	{
		printk(KERN_WARNING MODULE_NAME ": Error while setting KSC notification state: %u\n", r);
	}

	mutex_unlock(&data->lock);
	return r;
}
EXPORT_SYMBOL(micropklt_set_ksc_notifications);

int micropklt_set_led_states(unsigned state)
{
	struct microp_klt *data;
	struct i2c_client *client;
	char buffer[4] = { 0, 0, 0, 0 };
	int r;

	data = micropklt_t;
	if (!data) return -EAGAIN;
	client = data->client;
	
	mutex_lock(&data->lock);
	
	buffer[0] = MICROP_KLT_ID_LED_STATE;
	buffer[1] = 0xff & state;
	buffer[2] = 0xff & (state >> 8);
	
	data->led_states = 0xffff & state;
	
	r = micropklt_write(client, buffer, 3);
	mutex_unlock(&data->lock);
	return r;
}
EXPORT_SYMBOL(micropklt_set_led_states);

int micropklt_set_lcd_state(int on)
{
	struct microp_klt *data;
	unsigned state, r;

	data = micropklt_t;
	if (!data) return -EAGAIN;
	
	if (on)
	{
		state = data->led_states | MICROP_KLT_LCD_BIT;
	} else {
		state = data->led_states & ~MICROP_KLT_LCD_BIT;
	}
	r = micropklt_set_led_states(state);
	return r;
}
EXPORT_SYMBOL(micropklt_set_lcd_state);

static int micropklt_remove(struct i2c_client * client)
{
	struct microp_klt *data;

	data = i2c_get_clientdata(client);
	
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES);

	kfree(data);
	micropklt_t = NULL;
	return 0;
}

static int micropklt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct microp_klt *data;
	char buf[3] = { 0, 0, 0 };
	int supported = 1, r;

	printk(KERN_INFO MODULE_NAME ": Initializing MicroP-LED chip driver at addr: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        {
        	printk(KERN_ERR MODULE_NAME ": i2c bus not supported\n");
        	return -EINVAL;
        }

	data = kzalloc(sizeof *data, GFP_KERNEL);
	if (data < 0)
	{
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
	
	// Check version against what we think we should support
	switch (buf[0])
	{
		case 0x01:
		case 0x02:
		case 0x0a:
		case 0x0b:
			switch (buf[1])
			{
				case 0x05:
					// These are supported
					break;
				default:
					supported = 0;
			}
			break;
		case 0x0d:
			switch (buf[1])
			{
				case 0x02:
					break;
				default:
					supported = 0;
			}
			break;
		case 0x0c:
			switch (buf[1])
			{
				case 0x82:
				case 0x85:
					// These are supported
					break;
				default:
					supported = 0;
			}
			break;
		default:
			supported = 0;
	}
	data->version = (buf[0] << 8) | buf[1];
	if (!supported)
	{
		printk(KERN_WARNING MODULE_NAME ": This hardware is not yet supported: %04x\n", data->version);
		r = -ENOTSUPP;
		goto fail;
	}

	mutex_unlock(&data->lock);

#if 0
// Do this via timer?
	// A nice test sequence to run through the LEDs
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES | MICROP_KLT_LED_HOME);
	mdelay(100);
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES | MICROP_KLT_LED_BACK);
	mdelay(100);
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES | MICROP_KLT_LED_END);
	mdelay(100);
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES | MICROP_KLT_LED_SEND);
	mdelay(100);
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES | MICROP_KLT_LED_HOME |
	                         MICROP_KLT_LED_BACK | MICROP_KLT_LED_END |
	                         MICROP_KLT_LED_SEND | MICROP_KLT_SYSLED_ROTATE);
	mdelay(500);
#endif

	// Set default LED state
	micropklt_set_led_states(MICROP_KLT_DEFAULT_LED_STATES);	

	printk(MODULE_NAME ": Initialized MicroP-LED chip revision v%04x\n", data->version);

	return 0;
fail:
	kfree(data);
	return r;
}

static int micropklt_write(struct i2c_client *client, const char *sendbuf, int len)
{
	int r;

	r = i2c_master_send(client, sendbuf, len);
	if (r < 0)
		printk(KERN_ERR "Couldn't send ch id %02x\n", sendbuf[0]);
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	else {
		printk(KERN_INFO "micropklt_write:   >>> 0x%02x, 0x%02x -> %02x %02x\n", 
			client->addr, sendbuf[0], 
			(len > 1 ? sendbuf[1] : 0), 
			(len > 2 ? sendbuf[2] : 0));
	}
#endif
	return r;
}

static int micropklt_read(struct i2c_client *client, unsigned id, char *buf, int len)
{
	int r;
	char outbuffer[2] = { 0, 0 };

	outbuffer[0] = id;
	
	// Have to separate the "ask" and "read" chunks
	r = i2c_master_send(client, outbuffer, 1);
	if (r < 0)
	{
		printk(KERN_WARNING "micropklt_read: error while asking for "
			"data address %02x,%02x: %d\n", client->addr, id, r);
		return r;
	}
	mdelay(1);
	r = i2c_master_recv(client, buf, len);
	if (r < 0)
	{
		printk(KERN_ERR "micropklt_read: error while reading data at "
			"address %02x,%02x: %d\n", client->addr, id, r);
		return r;
	}
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	printk(KERN_INFO "micropklt_read:   <<< 0x%02x, 0x%02x -> %02x %02x\n", 
		client->addr, id, buf[0], buf[1]);
#endif
	return 0;
}

#if CONFIG_PM
static int micropklt_suspend(struct i2c_client * client, pm_message_t mesg)
{
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	printk(KERN_INFO MODULE_NAME ": suspending device...\n");
#endif
	return 0;
}

static int micropklt_resume(struct i2c_client * client)
{
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	printk(KERN_INFO MODULE_NAME ": resuming device...\n");
#endif
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
