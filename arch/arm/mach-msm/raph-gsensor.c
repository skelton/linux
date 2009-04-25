/*
 * raph-gsensor.c
 * G-Sensor found in HTC Raphael (Touch Pro) and HTC Diamond mobile phones
 *
 * Job Bolle <jb@b4m.com>
 */

#include <asm/mach-types.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif

#define MODULE_NAME "raph_gsensor"
// #define RAPH_GSENS_DEBUG

struct raph_gsensor {
	struct i2c_client *client;
	struct input_dev *inputdev;
	struct hrtimer timer;
	struct delayed_work work;
	struct mutex lock;
#ifdef CONFIG_ANDROID_POWER
	android_suspend_lock_t suspend_lock;
#endif
	int state;
	int step;
};

enum {
	RAPH_GSENS_OFF,
	RAPH_GSENS_ENABLE,
	RAPH_GSENS_ON,
	RAPH_GSENS_DISABLE,
};

struct {
	int len;
	unsigned char *data;
} raph_gsensor_enable_seq[] = {
	{ 2, "\x0d""\xc0" },
	{ 1, "\x1e" },
	{ 2, "\x0a""\xca" },
	{ 2, "\x0b""\x60" },
	{ 2, "\x0c""\xe3" },
	{ 1, "\x00" },
}, raph_gsensor_disable_seq[] = {
	{ 2, "\x0d""\x00" },
};

static ktime_t raph_gsensor_poll_time = {.tv.nsec =  100 * NSEC_PER_MSEC };
static ktime_t raph_gsensor_seq_time = {.tv.nsec =  2 * NSEC_PER_MSEC };


int raph_gsensor_enable(struct raph_gsensor *gsens,int on)
{
	if (on) {
		if (gsens->state == RAPH_GSENS_OFF
				|| gsens->state == RAPH_GSENS_DISABLE) {
			on = gsens->state == RAPH_GSENS_OFF;
			gsens->state = RAPH_GSENS_ENABLE;
			gsens->step = 0;
			if (on) {
				hrtimer_start(&gsens->timer,
						raph_gsensor_seq_time,
						HRTIMER_MODE_REL);
			}
		}
	} else {
		if (gsens->state == RAPH_GSENS_ON
				|| gsens->state == RAPH_GSENS_ENABLE) {
			gsens->state = RAPH_GSENS_DISABLE;
			gsens->step = 0;
		}
	}
	return 0;
}

static int raph_gsensor_i2c_read(struct i2c_client *client, unsigned id,
						char *buf, int len)
{
	int r;
	char outbuffer[2] = { 0, 0 };

	outbuffer[0] = id;
	// maejrep: Have to separate the "ask" and "read" chunks
	r = i2c_master_send(client, outbuffer, 1);
	if (r < 0) {
		printk(KERN_WARNING "%s: error asking for gsensor data at "
			"address %02x,%02x: %d\n",
			__func__, client->addr, id, r);
		return r;
	}
	mdelay(1);
	r = i2c_master_recv(client, buf, len);
	if (r < 0) {
		printk(KERN_ERR "%s: error reading gsensor data at "
			"address %02x,%02x: %d\n",
			__func__, client->addr, id, r);
		return r;
	}
	return 0;
}

static enum hrtimer_restart raph_gsensor_poll_timer(struct hrtimer *timer)
{
	struct raph_gsensor *gsens;

	gsens = container_of(timer, struct raph_gsensor, timer);
#ifdef CONFIG_ANDROID_POWER
	android_lock_suspend(&gsens->suspend_lock);
#endif
	schedule_work(&gsens->work.work);
	return HRTIMER_NORESTART;
}

static void raph_gsensor_work(struct work_struct *work)
{
	struct raph_gsensor *gsens;
	int err;
	char buf[6];
	int x,y,z;

	gsens = container_of(work, struct raph_gsensor, work.work);
	mutex_lock(&gsens->lock);
	switch (gsens->state) {
	case RAPH_GSENS_ENABLE:
		err = i2c_master_send(gsens->client,
				raph_gsensor_enable_seq[gsens->step].data,
				raph_gsensor_enable_seq[gsens->step].len);
		if (err < 0)
			printk(KERN_WARNING MODULE_NAME
					": %s: Enable %d error %d\n",
					__func__, gsens->step, err);
		if (++gsens->step >= ARRAY_SIZE(raph_gsensor_enable_seq))
			gsens->state = RAPH_GSENS_ON;
		hrtimer_start(&gsens->timer, raph_gsensor_seq_time,
				HRTIMER_MODE_REL);
		break;

	case RAPH_GSENS_DISABLE:
		err = i2c_master_send(gsens->client,
				raph_gsensor_disable_seq[gsens->step].data,
				raph_gsensor_disable_seq[gsens->step].len);
		if (err < 0)
			printk(KERN_WARNING MODULE_NAME
					": %s: Disable %d error %d\n",
					__func__, gsens->step, err);
		if (++gsens->step >= ARRAY_SIZE(raph_gsensor_disable_seq))
			gsens->state = RAPH_GSENS_OFF;
		hrtimer_start(&gsens->timer, raph_gsensor_seq_time,
				HRTIMER_MODE_REL);
		break;
	
	case RAPH_GSENS_ON:
		err = raph_gsensor_i2c_read(gsens->client, 0, buf, 6);
		x = buf[0] * 0x100 + buf[1] - 0x8000;
		y = buf[2] * 0x100 + buf[3] - 0x8000;
		z = buf[4] * 0x100 + buf[5] - 0x8000;
#ifdef RAPH_GSENS_DEBUG
		printk(KERN_INFO "G=( %6d , %6d , %6d )\n",
				x, y, z);
#endif
		input_report_abs(gsens->inputdev, ABS_X, x);
		input_report_abs(gsens->inputdev, ABS_Y, y);
		input_report_abs(gsens->inputdev, ABS_Z, z);
		input_sync(gsens->inputdev);
		hrtimer_start(&gsens->timer, raph_gsensor_poll_time,
				HRTIMER_MODE_REL);
		/* don't break */
	case RAPH_GSENS_OFF:
#ifdef CONFIG_ANDROID_POWER
		android_unlock_suspend(&gsens->suspend_lock);
#endif
		break;
	}
	mutex_unlock(&gsens->lock);
}

static int raph_gsensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct raph_gsensor *gsens;
	struct input_dev *idev;

	printk(KERN_INFO MODULE_NAME ": Initializing Raphael G-Sensor device "
					"at addr: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR MODULE_NAME ": i2c bus not supported\n");
		return -EINVAL;
	}

	gsens = kzalloc(sizeof *gsens, GFP_KERNEL);
	if (gsens < 0) {
		printk(KERN_ERR MODULE_NAME ": Not enough memory\n");
		return -ENOMEM;
	}
	mutex_init(&gsens->lock);
	gsens->client = client;
	i2c_set_clientdata(client, gsens);

	idev = input_allocate_device();
	if (idev) {
		idev->name = MODULE_NAME;
		set_bit(EV_ABS, idev->evbit);
		input_set_abs_params(idev, ABS_X, -32768, 32767, 0, 0);
		input_set_abs_params(idev, ABS_Y, -32768, 32767, 0, 0);
		input_set_abs_params(idev, ABS_Z, -32768, 32767, 0, 0);
		if (!input_register_device(idev)) {
			gsens->inputdev = idev;
		} else {
			gsens->inputdev = 0;
		}
	}
#ifdef CONFIG_ANDROID_POWER
	gsens->suspend_lock.name = "raph_gsensor";
	android_init_suspend_lock(&gsens->suspend_lock);
	android_lock_suspend(&gsens->suspend_lock);
#endif
	INIT_DELAYED_WORK(&gsens->work, raph_gsensor_work);
	hrtimer_init(&gsens->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gsens->timer.function = raph_gsensor_poll_timer;
	
	gsens->state = RAPH_GSENS_OFF;
	raph_gsensor_enable(gsens,1);
	return 0;
}

static int raph_gsensor_remove(struct i2c_client * client)
{
	struct raph_gsensor *gsens = i2c_get_clientdata(client);


	input_unregister_device(gsens->inputdev);
	input_free_device(gsens->inputdev);
#ifdef CONFIG_ANDROID_POWER
	android_uninit_suspend_lock(&gsens->suspend_lock);
#endif
	kfree(gsens);
	return 0;
}

#if CONFIG_PM
static int raph_gsensor_suspend(struct i2c_client * client, pm_message_t mesg)
{
#ifdef RAPH_GSENS_DEBUG
	printk(KERN_INFO MODULE_NAME ": suspending device...\n");
#endif
	return 0;
}

static int raph_gsensor_resume(struct i2c_client * client)
{
#ifdef RAPH_GSENS_DEBUG
	printk(KERN_INFO MODULE_NAME ": resuming device...\n");
#endif
	return 0;
}
#else
#define raph_gsensor_suspend NULL
#define raph_gsensor_resume NULL
#endif

static const struct i2c_device_id raph_gsensor_ids[] = {
        { "raph_gsensor", 0 },
        { }
};

static struct i2c_driver raph_gsensor_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = raph_gsensor_ids,
	.probe = raph_gsensor_probe,
	.remove = raph_gsensor_remove,
#if CONFIG_PM
	.suspend = raph_gsensor_suspend,
	.resume = raph_gsensor_resume,
#endif
};

static int __init raph_gsensor_init(void)
{
	printk(KERN_INFO MODULE_NAME ": Registering Raphael G-Sensor driver\n");
	return i2c_add_driver(&raph_gsensor_driver);
}

static void __exit raph_gsensor_exit(void)
{
	printk(KERN_INFO MODULE_NAME ": Unregistered Raphael G-Sensor driver\n");
	i2c_del_driver(&raph_gsensor_driver);
}

module_init(raph_gsensor_init);
module_exit(raph_gsensor_exit);
