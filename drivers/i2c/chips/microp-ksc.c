/*
    microp-ksc.c - i2c chip driver for microp-key
    
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
#include <linux/workqueue.h> /* for keyboard LED worker */
#include <linux/microp-ksc.h>


static int micropksc_read(struct i2c_client *, unsigned, char *, int);
static int micropksc_write(struct i2c_client *, const char *, int);
static int micropksc_probe(struct i2c_client *, const struct i2c_device_id *id);
static int __devexit micropksc_remove(struct i2c_client *);

#define MODULE_NAME "microp-ksc"

static struct microp_ksc {
	struct i2c_client *client;
	struct mutex lock;
	unsigned short version;
	unsigned led_state:2;
	struct work_struct work;
} * micropksc_t = 0;

int micropksc_read_scancode(unsigned char *outkey, unsigned char *outdown)
{
	struct microp_ksc *data;
	struct i2c_client *client;
	char buffer[8] = "\0\0\0\0\0\0\0\0";
	unsigned char key, isdown;

	if (!micropksc_t)
	{
		if (outkey)
			*outkey = -1;
		return -EAGAIN;
	}

	data = micropksc_t;

	mutex_lock(&data->lock);

	client = data->client;
	key = 0;

	micropksc_read(client, MICROP_KSC_ID_SCANCODE, buffer, 2);

	key = buffer[0] & MICROP_KSC_SCANCODE_MASK;
	isdown = (buffer[0] & MICROP_KSC_RELEASED_BIT) == 0;

	//TODO: Find out what channel 0x11 is for
	micropksc_read(client, MICROP_KSC_ID_MODIFIER, buffer, 2);

	if (outkey)
		*outkey = key;
	if (outdown)
		*outdown = isdown;

	mutex_unlock(&data->lock);

	return 0;
}
EXPORT_SYMBOL(micropksc_read_scancode);

int micropksc_set_led(unsigned int led, int on)
{
	struct microp_ksc *data;
	
	if (!micropksc_t)
		return -EAGAIN;
	if (led >= MICROP_KSC_LED_MAX)
		return -EINVAL;
	
	data = micropksc_t;
	
	mutex_lock(&data->lock);

	if (led == MICROP_KSC_LED_RESET) {
		data->led_state = 0;
	} else if (on) {
		data->led_state |= led;
	} else {
		data->led_state &= ~led;
	}

	schedule_work(&data->work);

	mutex_unlock(&data->lock);
	
	return 0;
}
EXPORT_SYMBOL(micropksc_set_led);

static void micropksc_led_work_func(struct work_struct *work)
{
        struct microp_ksc *data =
            container_of(work, struct microp_ksc, work);
	struct i2c_client *client;
	char buffer[3] = { MICROP_KSC_ID_LED, 0, 0 };
	client = data->client;
	buffer[1] = 0x16 - (data->led_state << 1);
	micropksc_write(client, buffer, 2);
}

/**
 * The i2c buffer holds all the keys that are pressed, 
 * even when microp-ksc isn't listening. It's safe to assume
 * we don't care about those bytes, so we need to flush
 * the i2c buffer by reading scancodes until it's empty.
 */
int micropksc_flush_buffer(void)
{
	unsigned char key;
	int r, i;

	i = 0;

	if (!micropksc_t)
	{
		printk(KERN_WARNING MODULE_NAME ": not initialized yet..\n");
		return -EAGAIN;
	}

        r = micropksc_read_scancode(&key, 0);
        if (key != 0)
        {
                do {
                        mdelay(5);
                        r = micropksc_read_scancode(&key, 0);
                } while (++i < 50 && key != 0);
                printk(KERN_WARNING MODULE_NAME ": Keyboard buffer was dirty! Flushed %d byte(s) from buffer\n", i);
        }
        return i;
}
EXPORT_SYMBOL(micropksc_flush_buffer);

static int micropksc_remove(struct i2c_client * client)
{
	struct microp_ksc *data;

	data = i2c_get_clientdata(client);

	cancel_work_sync(&data->work);
	
	kfree(data);
	return 0;
}

static int micropksc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct microp_ksc *data;
	char buf[3] = { 0, 0, 0 };
	
	printk(KERN_INFO MODULE_NAME ": Initializing MicroP-KEY chip driver at addr: 0x%02x\n", client->addr);

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
	
	micropksc_t = data;

	INIT_WORK(&data->work, micropksc_led_work_func);
	mutex_init(&data->lock);
	
	data->client = client;
	i2c_set_clientdata(client, data);
	
	// Read version
	micropksc_read(client, MICROP_KSC_ID_VERSION, buf, 2);
	data->version = buf[0] << 8 | buf[1];
	//TODO: Check version against known good revisions, and fail if it's not supported

	micropksc_flush_buffer();

	printk(MODULE_NAME ": Initialized MicroP-KEY chip revision v%04x\n", data->version);
	return 0;
#if 0 // See TODO above
fail:
	kfree(data);
	return -ENOSYS;
#endif
}

static int micropksc_write(struct i2c_client *client, const char *sendbuf, int len)
{
	int r;

	r = i2c_master_send(client, sendbuf, len);
	if (r < 0)
		printk(KERN_ERR "Couldn't send ch id %02x\n", sendbuf[0]);
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	else {
		printk(KERN_INFO "micropksc_write:   >>> 0x%02x, 0x%02x -> %02x %02x\n", 
			client->addr, sendbuf[0], 
			(len > 1 ? sendbuf[1] : 0), 
			(len > 2 ? sendbuf[2] : 0));
	}
#endif
	return r;
}

static int micropksc_read(struct i2c_client *client, unsigned id, char *buf, int len)
{
	int r;
	char outbuffer[2] = { 0, 0 };

	outbuffer[0] = id;
	
	// Have to separate the "ask" and "read" chunks
	r = i2c_master_send(client, outbuffer, 1);
	if (r < 0)
	{
		printk(KERN_WARNING "micropksc_read: error while asking for "
			"data address %02x,%02x: %d\n", client->addr, id, r);
		return r;
	}
	mdelay(1);
	r = i2c_master_recv(client, buf, len);
	if (r < 0)
	{
		printk(KERN_ERR "micropksc_read: error while reading data at "
			"address %02x,%02x: %d\n", client->addr, id, r);
		return r;
	}
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	printk(KERN_INFO "micropksc_read:   <<< 0x%02x, 0x%02x -> %02x %02x\n", 
		client->addr, id, buf[0], buf[1]);
#endif
	return 0;
}

#if CONFIG_PM
static int micropksc_suspend(struct i2c_client * client, pm_message_t mesg)
{
	flush_scheduled_work();
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	printk(KERN_INFO MODULE_NAME ": suspending device...\n");
#endif
	return 0;
}

static int micropksc_resume(struct i2c_client * client)
{
#if defined(MICROP_DEBUG) && MICROP_DEBUG
	printk(KERN_INFO MODULE_NAME ": resuming device...\n");
#endif
	return 0;
}
#else

#define micropksc_suspend NULL
#define micropksc_resume NULL

#endif

static const struct i2c_device_id microp_ksc_ids[] = {
        { "microp-ksc", 0 },
        { }
};

static struct i2c_driver micropksc_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = microp_ksc_ids,
	.probe = micropksc_probe,
	.remove = micropksc_remove,
#if CONFIG_PM
	.suspend = micropksc_suspend,
	.resume = micropksc_resume,
#endif
};

static int __init micropksc_init(void)
{
	micropksc_t = NULL;
	printk(KERN_INFO "microp-ksc: Registering MicroP-KEY driver\n");
	return i2c_add_driver(&micropksc_driver);
}

static void __exit micropksc_exit(void)
{
	printk(KERN_INFO "microp-ksc: Unregistered MicroP-KEY driver\n");
	i2c_del_driver(&micropksc_driver);
}

MODULE_AUTHOR("Joe Hansche <madcoder@gmail.com>");
MODULE_DESCRIPTION("MicroP-KEY chip driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

module_init(micropksc_init);
module_exit(micropksc_exit);
