#include <linux/i2c.h>
#include <linux/microp-klt.h>
#include <linux/microp.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/bma150.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/input.h>

static struct microp_klt {
	struct i2c_client *client;
	struct mutex lock;
	u16 led_states;
	unsigned short version;
	struct led_classdev leds[MICROP_KLT_LED_CNT];
} *micropklt_t = 0;


/*
 * Comes from mahimahi's microp driver.
 * This is OMFG ugly.
 */
static char *hex2string(uint8_t *data, int len)
{
	static char buf[101];
	int i;

	i = (sizeof(buf) - 1) / 4;
	if (len > i)
		len = i;

	for (i = 0; i < len; i++)
		sprintf(buf + i * 4, "[%02X]", data[i]);

	return buf;
}

#define I2C_READ_RETRY_TIMES  10
#define I2C_WRITE_RETRY_TIMES 10

static int i2c_read_block(struct i2c_client *client, uint8_t addr,
	uint8_t *data, int length)
{
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
		.len = length,
		.buf = data,
	}
	};

	mdelay(1);
	for (retry = 0; retry <= I2C_READ_RETRY_TIMES; retry++) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2) {
			dev_dbg(&client->dev, "R [%02X] = %s\n", addr,
					hex2string(data, length));
			return 0;
		}
		msleep(10);
	}

	dev_err(&client->dev, "i2c_read_block retry over %d\n",
			I2C_READ_RETRY_TIMES);
	return -EIO;
}

#define MICROP_I2C_WRITE_BLOCK_SIZE 21
static int i2c_write_block(struct i2c_client *client, uint8_t addr,
	uint8_t *data, int length)
{
	int retry;
	uint8_t buf[MICROP_I2C_WRITE_BLOCK_SIZE];
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	dev_dbg(&client->dev, "W [%02X] = %s\n", addr,
			hex2string(data, length));

	if (length + 1 > MICROP_I2C_WRITE_BLOCK_SIZE) {
		dev_err(&client->dev, "i2c_write_block length too long\n");
		return -E2BIG;
	}

	buf[0] = addr;
	memcpy((void *)&buf[1], (void *)data, length);

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
/*
 * G-sensor
 * Comes from mahimahi gsensor driver
 */
static int microp_spi_enable(uint8_t on)
{
	struct i2c_client *client;
	int ret;

	client = micropklt_t->client;;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_SPI_EN, &on, 1);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}
	msleep(10);
	return ret;
}

static int gsensor_read_reg(uint8_t reg, uint8_t *data)
{
	struct i2c_client *client;
	int ret;
	uint8_t tmp[2];

	client = micropklt_t->client;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GSENSOR_REG_DATA_REQ,
			      &reg, 1);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}
	msleep(10);

	ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_REG_DATA, tmp, 2);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_read_block fail\n", __func__);
		return ret;
	}
	*data = tmp[1];
	return ret;
}

static int gsensor_write_reg(uint8_t reg, uint8_t data)
{
	struct i2c_client *client;
	int ret;
	uint8_t tmp[2];

	client = micropklt_t->client;

	tmp[0] = reg;
	tmp[1] = data;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GSENSOR_REG, tmp, 2);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}

	return ret;
}

static int gsensor_read_acceleration(short *buf)
{
	struct i2c_client *client;
	int ret;
	uint8_t tmp[6];

	client = micropklt_t->client;


	tmp[0] = 1;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GSENSOR_DATA_REQ,
			      tmp, 1);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}

	msleep(10);

	if (micropklt_t->version <= 0x615 || micropklt_t->version == 0x0a0e) {
		/*
		 * Note the data is a 10bit signed value from the chip.
		*/
		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_X_DATA,
				     tmp, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[0] = (short)(tmp[0] << 8 | tmp[1]);
		buf[0] >>= 6;

		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_Y_DATA,
				     tmp, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[1] = (short)(tmp[0] << 8 | tmp[1]);
		buf[1] >>= 6;

		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_Z_DATA,
				     tmp, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[2] = (short)(tmp[0] << 8 | tmp[1]);
		buf[2] >>= 6;
	} else {
		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_DATA,
				     tmp, 6);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[0] = (short)(tmp[0] << 8 | tmp[1]);
		buf[0] >>= 6;
		buf[1] = (short)(tmp[2] << 8 | tmp[3]);
		buf[1] >>= 6;
		buf[2] = (short)(tmp[4] << 8 | tmp[5]);
		buf[2] >>= 6;
	}

#ifdef DEBUG_BMA150
	/* Log this to debugfs */
	gsensor_log_status(ktime_get(), buf[0], buf[1], buf[2]);
#endif
	return 1;
}

static int gsensor_init_hw(void)
{
	uint8_t reg;
	int ret;

	pr_debug("%s\n", __func__);

	microp_spi_enable(1);

	ret = gsensor_read_reg(RANGE_BWIDTH_REG, &reg);
	if (ret < 0 )
		return -EIO;
	reg &= 0xe0;
	ret = gsensor_write_reg(RANGE_BWIDTH_REG, reg);
	if (ret < 0 )
		return -EIO;

	ret = gsensor_read_reg(SMB150_CONF2_REG, &reg);
	if (ret < 0 )
		return -EIO;
	reg |= (1 << 3);
	ret = gsensor_write_reg(SMB150_CONF2_REG, reg);

	return ret;
}

static int bma150_set_mode(char mode)
{
	uint8_t reg;
	int ret;

	pr_debug("%s mode = %d\n", __func__, mode);
	if (mode == BMA_MODE_NORMAL)
		microp_spi_enable(1);


	ret = gsensor_read_reg(SMB150_CTRL_REG, &reg);
	if (ret < 0 )
		return -EIO;
	reg = (reg & 0xfe) | mode;
	ret = gsensor_write_reg(SMB150_CTRL_REG, reg);

	if (mode == BMA_MODE_SLEEP)
		microp_spi_enable(0);

	return ret;
}
static int gsensor_read(uint8_t *data)
{
	int ret;
	uint8_t reg = data[0];

	ret = gsensor_read_reg(reg, &data[1]);
	pr_debug("%s reg = %x data = %x\n", __func__, reg, data[1]);
	return ret;
}

static int gsensor_write(uint8_t *data)
{
	int ret;
	uint8_t reg = data[0];

	pr_debug("%s reg = %x data = %x\n", __func__, reg, data[1]);
	ret = gsensor_write_reg(reg, data[1]);
	return ret;
}

static int bma150_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);
	return nonseekable_open(inode, file);
}

static int bma150_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int bma150_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	char rwbuf[8];
	int ret = -1;
	short buf[8], temp;

	switch (cmd) {
	case BMA_IOCTL_READ:
	case BMA_IOCTL_WRITE:
	case BMA_IOCTL_SET_MODE:
		if (copy_from_user(&rwbuf, argp, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		if (copy_from_user(&buf, argp, sizeof(buf)))
			return -EFAULT;
		break;
	default:
		break;
	}

	switch (cmd) {
	case BMA_IOCTL_INIT:
		ret = gsensor_init_hw();
		if (ret < 0)
			return ret;
		break;

	case BMA_IOCTL_READ:
		if (rwbuf[0] < 1)
			return -EINVAL;
		ret = gsensor_read(rwbuf);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_WRITE:
		if (rwbuf[0] < 2)
			return -EINVAL;
		ret = gsensor_write(rwbuf);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		ret = gsensor_read_acceleration(&buf[0]);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_SET_MODE:
		bma150_set_mode(rwbuf[0]);
		break;
	case BMA_IOCTL_GET_INT:
		temp = 0;
		break;
	default:
		return -ENOTTY;
	}

	switch (cmd) {
	case BMA_IOCTL_READ:
		if (copy_to_user(argp, &rwbuf, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		if (copy_to_user(argp, &buf, sizeof(buf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_GET_INT:
		if (copy_to_user(argp, &temp, sizeof(temp)))
			return -EFAULT;
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations bma_fops = {
	.owner = THIS_MODULE,
	.open = bma150_open,
	.release = bma150_release,
	.ioctl = bma150_ioctl,
};

static struct miscdevice spi_bma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = BMA150_G_SENSOR_NAME,
	.fops = &bma_fops,
};

int init_spi_bma150(struct microp_klt *data) {
	pr_info("%s: init_spi_bma150\n", __func__);
	int ret;
	ret = misc_register(&spi_bma_device);
	if (ret < 0) {
		pr_err("%s: init bma150 misc_register fail\n",
				__func__);
		return -1;
	}
	micropklt_t=data;
	return 0;
}
