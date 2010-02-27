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
#include <linux/debugfs.h>
#include <linux/earlysuspend.h>
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif

#define MODULE_NAME "rhodium_audio"
#define SPKR_PWR 0x54
#define I2C_READ_RETRY_TIMES 10
#define I2C_WRITE_RETRY_TIMES 10

struct data_t {
	struct i2c_client *a1010;
	struct i2c_client *tpa2016;
};
static struct data_t _dat;

static int i2c_read(struct i2c_client *client, unsigned addr,
		char *data, int len) {
	int retry;
	int ret;
	struct i2c_msg msgs[] = {
	{
		.addr = client->addr,
		.flags = 0,
		.len = 1,
		.buf = &addr,
	},
	{
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = len,
		.buf = data,
	}
	};

	mdelay(1);
	for (retry = 0; retry <= I2C_READ_RETRY_TIMES; retry++) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2) {
			return 0;
		}
		msleep(10);
		printk("read retry\n");
	}

	dev_err(&client->dev, "i2c_read_block retry over %d\n",
			I2C_READ_RETRY_TIMES);
	return -EIO;
}

#define MAX_I2C_WRITE 20
static int i2c_write(struct i2c_client *client, unsigned addr,
		char *data, int len) {
	int retry;
	uint8_t buf[MAX_I2C_WRITE];
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		}
	};

	if (len + 1 > MAX_I2C_WRITE) {
		dev_err(&client->dev, "i2c_write_block length too long\n");
		return -E2BIG;
	}

	buf[0] = addr;
	memcpy((void *)&buf[1], (void *)data, len);

	mdelay(1);
	for (retry = 0; retry <= I2C_WRITE_RETRY_TIMES; retry++) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret == 1)
			return 0;
		msleep(10);
	}
	dev_err(&client->dev, "i2c_write_block retry over %d\n",
			I2C_WRITE_RETRY_TIMES);
	return -EIO;
}


#define IC_REG		0x1
#define ATK_REG		0x2
#define REL_REG		0x3
#define HOLD_REG	0x4
#define FIXED_GAIN_REG	0x5
#define AGC_REG1	0x6
#define AGC_REG2	0x7

#define SPK_EN_L 	1<<6
#define SPK_EN_R 	1<<7

void tpa_set_power(uint8_t arg) {
	char buf;
	if(i2c_read(_dat.tpa2016, IC_REG, &buf, 1)) {
		pr_err("Can't get tpa's power i2c reg\n");
		return;
	}
	buf&=0xc0;//power settings are two last bits
	buf|=arg;
	if(i2c_write(_dat.tpa2016, IC_REG, &buf, 1))
		pr_err("Can't get tpa's power i2c reg\n");
}

void tpa_set_attack_time(uint8_t arg) {
	//Only 6 lower bits
	arg&=0x3f;
	if(i2c_write(_dat.tpa2016, ATK_REG, &arg, 1))
		pr_err("Can't set tpa's attack time i2c reg\n");
}

void tpa_set_release_time(uint8_t arg) {
	//Only 6 lower bits
	arg&=0x3f;
	if(i2c_write(_dat.tpa2016, REL_REG, &arg, 1))
		pr_err("Can't set tpa's release time i2c reg\n");
}

void tpa_set_hold_time(uint8_t arg) {
	//Only 6 lower bits
	arg&=0x3f;
	if(i2c_write(_dat.tpa2016, HOLD_REG, &arg, 1))
		pr_err("Can't set tpa's hold time i2c reg\n");
}

void tpa_set_fixed_gain(uint8_t arg) {
	//Only 6 lower bits
	arg&=0x3f;
	if(i2c_write(_dat.tpa2016, FIXED_GAIN_REG, &arg, 1))
		pr_err("Can't set tpa's fixed gain i2c reg\n");
}

void tpa_set_output_limiter(uint8_t arg) {
	char buf;
	if(i2c_read(_dat.tpa2016, AGC_REG1, &buf, 1)) {
		pr_err("Can't get tpa's agc reg1 i2c reg\n");
		return;
	}
	buf&=0xf0;//output limiter is the lowest 4 bits
	//0xff = disable output limiter
	if(arg==0xff)
		buf&=~(1<<7);
	else {
		arg&=0xf;
		buf|=arg;
	}
	if(i2c_write(_dat.tpa2016, AGC_REG1, &buf, 1))
		pr_err("Can't set tpa's agc reg1 i2c reg\n");
}

void tpa_set_noise_gate_threshold(uint8_t arg) {
	char buf;
	if(i2c_read(_dat.tpa2016, AGC_REG1, &buf, 1)) {
		pr_err("Can't get tpa's agc reg1 i2c reg\n");
		return;
	}
	buf&=0x9f;//noise gate threshold is the {5,6} bits
	arg&=0x3;
	buf|=arg<<5;
	if(i2c_write(_dat.tpa2016, AGC_REG1, &buf, 1))
		pr_err("Can't get tpa's agc reg1 i2c reg\n");
}

void tpa_set_max_gain(uint8_t arg) {
	char buf;
	if(i2c_read(_dat.tpa2016, AGC_REG2, &buf, 1)) {
		pr_err("Can't get tpa's agc reg2 i2c reg\n");
		return;
	}
	buf&=0x0f;//max gain is the highest 4 bits
	arg&=0xf;
	buf|=arg<<4;
	if(i2c_write(_dat.tpa2016, AGC_REG2, &buf, 1))
		pr_err("Can't set tpa's agc reg2 i2c reg\n");
}

void tpa_set_compression_ratio(uint8_t arg) {
	char buf;
	if(i2c_read(_dat.tpa2016, AGC_REG2, &buf, 1)) {
		pr_err("Can't get tpa's agc reg2 i2c reg\n");
		return;
	}
	buf&=0xfc;//compression ratio is the lowest 2 bits
	arg&=3;
	buf|=arg;
	if(i2c_write(_dat.tpa2016, AGC_REG2, &buf, 1))
		pr_err("Can't set tpa's agc reg2 i2c reg\n");
}

void enable_speaker_rhod(void) {
	if(!machine_is_htcrhodium())
		return;
	gpio_direction_output(SPKR_PWR, 1);
	mdelay(1);
	//Default is use wince's setting
	tpa_set_power(SPK_EN_L|SPK_EN_R);
	tpa_set_attack_time(32/*5.335ms*/);
	tpa_set_release_time(1/*137ms*/);
	tpa_set_hold_time(0/*disabled*/);
	tpa_set_fixed_gain(0x10/*+16dB*/);
	tpa_set_output_limiter(0x19/*6dBV*/);
	tpa_set_max_gain(0xc/*30dB*/);
	tpa_set_compression_ratio(0/*1:1*/);
}

void disable_speaker_rhod(void) {
	if(!machine_is_htcrhodium())
		return;
	gpio_direction_output(SPKR_PWR, 0);
}

void speaker_vol_rhod(int arg) {
	//arg ranges from 0 to 5 (0 is supposed to be off I think)
	//Gain ranges from 0 to 30
	int gain=arg*6;
	tpa_set_fixed_gain(gain);
}

//TI TPA2016 speaker amplificator
//Datasheet available on TI's site.
static int tpa_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	int res;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[TPA2016] i2c_check_functionality error!\n");
		return -ENOTSUPP;
	}
        strlcpy(client->name, "tpa2016", I2C_NAME_SIZE);
	_dat.tpa2016=client;
	res = gpio_request(SPKR_PWR, "SPK AMP");
	if(res<0) {
		pr_err("[TPA2016] Can't reserve gpio\n");
	}
	return 0;
}

static const struct i2c_device_id tpa_ids[] = {
        { "tpa2016", 0 },
        { }
};

static struct i2c_driver tpa_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = tpa_ids,
	.probe = tpa_probe,
};

//Audience A1010 (sound cancelation, etc)
static int aud_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	//Not much to do here uh ?
	_dat.a1010=client;
	return 0;
}

static const struct i2c_device_id aud_ids[] = {
        { "a1010", 0 },
        { }
};

static struct i2c_driver aud_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = aud_ids,
	.probe = aud_probe,
};

static int __init rhod_audio_init(void) {
	int rc;

	if(!machine_is_htcrhodium())
		return 0;

	printk(KERN_INFO "Rhodium audio registering drivers\n");
	rc=i2c_add_driver(&tpa_driver);
	return rc ? rc : i2c_add_driver(&aud_driver);
}

module_init(rhod_audio_init);

