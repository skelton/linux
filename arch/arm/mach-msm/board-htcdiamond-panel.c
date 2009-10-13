/* linux/arch/arm/mach-msm/board-htcdiamond-panel.c
** Based on board-trout-panel.c by: Brian Swetland <swetland@google.com>
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>

#include <mach/msm_fb.h>
#include <mach/vreg.h>

#include "board-htcdiamond.h"
#include "proc_comm_wince.h"
#include "devices.h"

//static struct clk *gp_clk;

#define MDDI_CLIENT_CORE_BASE  0x108000
#define LCD_CONTROL_BLOCK_BASE 0x110000
#define SPI_BLOCK_BASE         0x120000
#define I2C_BLOCK_BASE         0x130000
#define PWM_BLOCK_BASE         0x140000
#define GPIO_BLOCK_BASE        0x150000
#define SYSTEM_BLOCK1_BASE     0x160000
#define SYSTEM_BLOCK2_BASE     0x170000


#define	DPSUS       (MDDI_CLIENT_CORE_BASE|0x24)
#define	SYSCLKENA   (MDDI_CLIENT_CORE_BASE|0x2C)
#define	PWM0OFF	      (PWM_BLOCK_BASE|0x1C)

#define V_VDDE2E_VDD2_GPIO 0
#define MDDI_RST_N 82

#define	MDDICAP0    (MDDI_CLIENT_CORE_BASE|0x00)
#define	MDDICAP1    (MDDI_CLIENT_CORE_BASE|0x04)
#define	MDDICAP2    (MDDI_CLIENT_CORE_BASE|0x08)
#define	MDDICAP3    (MDDI_CLIENT_CORE_BASE|0x0C)
#define	MDCAPCHG    (MDDI_CLIENT_CORE_BASE|0x10)
#define	MDCRCERC    (MDDI_CLIENT_CORE_BASE|0x14)
#define	TTBUSSEL    (MDDI_CLIENT_CORE_BASE|0x18)
#define	DPSET0      (MDDI_CLIENT_CORE_BASE|0x1C)
#define	DPSET1      (MDDI_CLIENT_CORE_BASE|0x20)
#define	DPSUS       (MDDI_CLIENT_CORE_BASE|0x24)
#define	DPRUN       (MDDI_CLIENT_CORE_BASE|0x28)
#define	SYSCKENA    (MDDI_CLIENT_CORE_BASE|0x2C)
#define	TESTMODE    (MDDI_CLIENT_CORE_BASE|0x30)
#define	FIFOMONI    (MDDI_CLIENT_CORE_BASE|0x34)
#define	INTMONI     (MDDI_CLIENT_CORE_BASE|0x38)
#define	MDIOBIST    (MDDI_CLIENT_CORE_BASE|0x3C)
#define	MDIOPSET    (MDDI_CLIENT_CORE_BASE|0x40)
#define	BITMAP0     (MDDI_CLIENT_CORE_BASE|0x44)
#define	BITMAP1     (MDDI_CLIENT_CORE_BASE|0x48)
#define	BITMAP2     (MDDI_CLIENT_CORE_BASE|0x4C)
#define	BITMAP3     (MDDI_CLIENT_CORE_BASE|0x50)
#define	BITMAP4     (MDDI_CLIENT_CORE_BASE|0x54)

#define	SRST        (LCD_CONTROL_BLOCK_BASE|0x00)
#define	PORT_ENB    (LCD_CONTROL_BLOCK_BASE|0x04)
#define	START       (LCD_CONTROL_BLOCK_BASE|0x08)
#define	PORT        (LCD_CONTROL_BLOCK_BASE|0x0C)
#define	CMN         (LCD_CONTROL_BLOCK_BASE|0x10)
#define	GAMMA       (LCD_CONTROL_BLOCK_BASE|0x14)
#define	INTFLG      (LCD_CONTROL_BLOCK_BASE|0x18)
#define	INTMSK      (LCD_CONTROL_BLOCK_BASE|0x1C)
#define	MPLFBUF     (LCD_CONTROL_BLOCK_BASE|0x20)
#define	HDE_LEFT    (LCD_CONTROL_BLOCK_BASE|0x24)
#define	VDE_TOP     (LCD_CONTROL_BLOCK_BASE|0x28)
#define	PXL         (LCD_CONTROL_BLOCK_BASE|0x30)
#define	HCYCLE      (LCD_CONTROL_BLOCK_BASE|0x34)
#define	HSW         (LCD_CONTROL_BLOCK_BASE|0x38)
#define	HDE_START   (LCD_CONTROL_BLOCK_BASE|0x3C)
#define	HDE_SIZE    (LCD_CONTROL_BLOCK_BASE|0x40)
#define	VCYCLE      (LCD_CONTROL_BLOCK_BASE|0x44)
#define	VSW         (LCD_CONTROL_BLOCK_BASE|0x48)
#define	VDE_START   (LCD_CONTROL_BLOCK_BASE|0x4C)
#define	VDE_SIZE    (LCD_CONTROL_BLOCK_BASE|0x50)
#define	WAKEUP      (LCD_CONTROL_BLOCK_BASE|0x54)
#define	WSYN_DLY    (LCD_CONTROL_BLOCK_BASE|0x58)
#define	REGENB      (LCD_CONTROL_BLOCK_BASE|0x5C)
#define	VSYNIF      (LCD_CONTROL_BLOCK_BASE|0x60)
#define	WRSTB       (LCD_CONTROL_BLOCK_BASE|0x64)
#define	RDSTB       (LCD_CONTROL_BLOCK_BASE|0x68)
#define	ASY_DATA    (LCD_CONTROL_BLOCK_BASE|0x6C)
#define	ASY_DATB    (LCD_CONTROL_BLOCK_BASE|0x70)
#define	ASY_DATC    (LCD_CONTROL_BLOCK_BASE|0x74)
#define	ASY_DATD    (LCD_CONTROL_BLOCK_BASE|0x78)
#define	ASY_DATE    (LCD_CONTROL_BLOCK_BASE|0x7C)
#define	ASY_DATF    (LCD_CONTROL_BLOCK_BASE|0x80)
#define	ASY_DATG    (LCD_CONTROL_BLOCK_BASE|0x84)
#define	ASY_DATH    (LCD_CONTROL_BLOCK_BASE|0x88)
#define	ASY_CMDSET  (LCD_CONTROL_BLOCK_BASE|0x8C)

#define	SSICTL      (SPI_BLOCK_BASE|0x00)
#define	SSITIME     (SPI_BLOCK_BASE|0x04)
#define	SSITX       (SPI_BLOCK_BASE|0x08)
#define	SSIRX       (SPI_BLOCK_BASE|0x0C)
#define	SSIINTC     (SPI_BLOCK_BASE|0x10)
#define	SSIINTS     (SPI_BLOCK_BASE|0x14)
#define	SSIDBG1     (SPI_BLOCK_BASE|0x18)
#define	SSIDBG2     (SPI_BLOCK_BASE|0x1C)
#define	SSIID       (SPI_BLOCK_BASE|0x20)

#define	WKREQ       (SYSTEM_BLOCK1_BASE|0x00)
#define	CLKENB      (SYSTEM_BLOCK1_BASE|0x04)
#define	DRAMPWR     (SYSTEM_BLOCK1_BASE|0x08)
#define	INTMASK     (SYSTEM_BLOCK1_BASE|0x0C)
#define	GPIOSEL     (SYSTEM_BLOCK2_BASE|0x00)

#define	GPIODATA    (GPIO_BLOCK_BASE|0x00)
#define	GPIODIR     (GPIO_BLOCK_BASE|0x04)
#define	GPIOIS      (GPIO_BLOCK_BASE|0x08)
#define	GPIOIBE     (GPIO_BLOCK_BASE|0x0C)
#define	GPIOIEV     (GPIO_BLOCK_BASE|0x10)
#define	GPIOIE      (GPIO_BLOCK_BASE|0x14)
#define	GPIORIS     (GPIO_BLOCK_BASE|0x18)
#define	GPIOMIS     (GPIO_BLOCK_BASE|0x1C)
#define	GPIOIC      (GPIO_BLOCK_BASE|0x20)
#define	GPIOOMS     (GPIO_BLOCK_BASE|0x24)
#define	GPIOPC      (GPIO_BLOCK_BASE|0x28)
#define	GPIOID      (GPIO_BLOCK_BASE|0x30)

#define SPI_WRITE(reg, val) \
	{ SSITX,        0x00010000 | (((reg) & 0xff) << 8) | ((val) & 0xff) }, \
	{ 0, 5 },

#define SPI_WRITE1(reg) \
	{ SSITX,        (reg) & 0xff }, \
	{ 0, 5 },

#define SPI_WRITE_S(reg,val) \
	{0x120000,0x130},\
	{0x120004,0x100},\
	{0x120008,0x80000 | (reg)},\
	{0x120008,(val)},\
	{0x120000,0x132}

// panel type, 0=unknown, 1=hitachi
static int type=0;
module_param(type, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct mddi_table {
	uint32_t reg;
	uint32_t value;
};

static struct mddi_table mddi_toshiba_common_init_table[] = {	
	{0x0010801c,0x4bec0066},
	{0x00108020,0x00000113},
	{0x00108024,0x00000000},
	{0x00108028,0x00000001},{1,300},
	{0x0010802c,0x00000001},
	{0x00160004,0x0000a1ef},
	{0x00170000,0x00000000},
	{0x00160000,0x00000000},
	{0x00150000,0x03cf0000},
	{0x00150004,0x000003cf},
	{0x00150028,0x00000000},
	{0x00160008,0x00000001},
	{0x00140008,0x00000060},
	{0x00140000,0x00001388},
	{0x0014001c,0x00000001},
	{0x00140028,0x00000060},
	{0x00140020,0x00001388},
	{0x0014003c,0x00000001},
	{0x00140008,0x000000e0},
	{0x00140028,0x000000e0},
	{0x00140068,0x00000003},{1,1},

};

static struct mddi_table mddi_sharp_table[] = {
	{0x110008,0},
	{0x110030,0x101},
	{0x11005c,0x1},
	{0x150004,0x3cf},
	{0x150000,0x40004},{1,2},
	{1,0x32},
        {0x120000,0x170},
        {0x120004,0x100},
        {0x120000,0x172},
	SPI_WRITE_S(0x12,1),
	{1,0x12c},
	SPI_WRITE_S(0x13,3),
	{1,0x30},
	{0x110030,1},
	{0x11005c,0x1},
	{0x110008,1},
};


static struct mddi_table mddi_toshiba_prim_start_table[] = {
	{0x00108044,0x028001e0},
	{0x00108048,0x01e000f0},
	{0x0010804c,0x01e000f0},
	{0x00108050,0x01e000f0},
	{0x00108054,0x00dc00b0},
	{0x00160004,0x0000a1eb},
	{0x00110004,0x00000001},
	{0x0011000c,0x00000008},
	{0x00110030,0x00000001},
	{0x00110020,0x00000000},
	{0x00110034,0x000000f9},
	{0x00110038,0x00000002},
	{0x0011003c,0x00000007},
	{0x00110040,0x000000ef},
	{0x00110044,0x000002ff},
	{0x00110048,0x00000005},
	{0x0011004c,0x00000009},
	{0x00110050,0x0000027f},
	{0x00110008,0x00000001},
};

struct spi_table {
	uint16_t reg;
	uint16_t value;
	uint16_t delay;
};

static struct spi_table hitachi_spi_table[] = {
	{2,0},
	{3,0},
	{4,0},
	{0x10,0x250},
	{0x20,2},
	{0x21,0x1a27},
	{0x22,0x3e},
	{0x23,0x7400},
	{0x24,0x7400},
	{0x25,0x6a06},
	{0x26,0x7400},
	{0x27,0x1906},
	{0x28,0x1925},
	{0x29,0x1944},
	{0x2a,0x666},
	{0x100,0x33},
	{0x101,3},
	{0x102,0x3700},
	{0x300,0x6657},
	{0x301,0x515},
	{0x302,0xc113},
	{0x303,0x273},
	{0x304,0x6131},
	{0x305,0xc416},
	{0x501,0xffff},
	{0x502,0xffff},
	{0x503,0xffff},
 
	{0x504,0xff},
	{0x518,0},
	{2,0x200,0xa},
	{1,1,2},
	{2,0x8210,0x14},
	{2,0x8310,0x14},
	{2,0x710,0x14},
	{2,0x1730,0x14},
	{1,0x12,0},
	{1,0x32,0},
	{0x23,0,0x14},
	{1,0x33,0},
	{0x23,0x7400,0},
 
 
};

static int client_state=1; // we are booting with the panel on.

static void htcdiamond_process_mddi_table(struct msm_mddi_client_data *client_data,
				     struct mddi_table *table, size_t count)
{
	int i;
	for(i = 0; i < count; i++) {
		uint32_t reg = table[i].reg;
		uint32_t value = table[i].value;
		if (reg == 0)
			udelay(value);
		else if (reg == 1)
			msleep(value);
		else {
			client_data->remote_write(client_data, value, reg);
		}
	}
}

static void htcdiamond_process_spi_table(struct msm_mddi_client_data *client_data,
					  struct spi_table *table, size_t count)
{
	int i;
	mdelay(0x32);
	client_data->remote_write(client_data, 0x170, SSICTL);
	client_data->remote_write(client_data, 0x100, SSITIME);
	client_data->remote_write(client_data, 0x172, SSICTL);
	for(i = 0; i < count; i++) {
		
		uint16_t reg = table[i].reg;
		uint16_t value = table[i].value;
		uint16_t delay = table[i].delay;
		
		client_data->remote_write(client_data, 0x170, SSICTL);
		client_data->remote_write(client_data, 0x80010, SSITX);
		client_data->remote_write(client_data, 0x10000 | reg, SSITX);
		client_data->remote_write(client_data, 0x172, SSICTL);
		client_data->remote_write(client_data, 0x170, SSICTL);
		client_data->remote_write(client_data, 0x80012, SSITX);
		client_data->remote_write(client_data, 0x10000 | value, SSITX);
		client_data->remote_write(client_data, 0x172, SSICTL);

		if(delay)
			msleep(delay);
	}
}

extern void  micropklt_lcd_ctrl(int);

static void htcdiamond_mddi_power_client(struct msm_mddi_client_data *client_data,
				    int on)
{
	struct msm_dex_command dex;
	int i;
	
	printk("htcdiamond_mddi_power_client(%d)\n", on);
	
	if(type==0) // don't power up/down if we don't know the panel type
		return;
	if(on) {
		msm_gpio_set_function(DEX_GPIO_CFG(RAPH100_LCD_PWR1,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		micropklt_lcd_ctrl(1);
		dex.cmd=PCOM_PMIC_REG_ON;
		dex.has_data=1;
		dex.data=0x800;
		msm_proc_comm_wince(&dex,0);
		mdelay(40);

		msm_gpio_set_function(DEX_GPIO_CFG(0x3c,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		msm_gpio_set_function(DEX_GPIO_CFG(0x3d,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		gpio_set_value(0x3d,0);
		udelay(10);
		gpio_set_value(0x3d,1);
		udelay(10);
		msm_gpio_set_function(DEX_GPIO_CFG(0x3d,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA,0));
		for(i=0;i<10;i++) {
			gpio_set_value(0x3c,0);
			udelay(10);
			gpio_set_value(0x3c,1);
			udelay(10);
		}
		msm_gpio_set_function(DEX_GPIO_CFG(0x3c,1,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		msm_gpio_set_function(DEX_GPIO_CFG(0x3d,1,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		msm_gpio_set_function(DEX_GPIO_CFG(0x1b,1,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,0));

		micropklt_lcd_ctrl(2);
		dex.data=0x2000;
		msm_proc_comm_wince(&dex,0);
		mdelay(50);
		msm_gpio_set_function(DEX_GPIO_CFG(RAPH100_LCD_PWR2,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		mdelay(200);
	} else {
		gpio_set_value(RAPH100_LCD_PWR2, 0);
		mdelay(1);
		dex.cmd=PCOM_PMIC_REG_OFF;
		dex.has_data=1;
		dex.data=0x2000;
		msm_proc_comm_wince(&dex,0);
		mdelay(7);
		msm_gpio_set_function(DEX_GPIO_CFG(0x1b,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,0));
		dex.data=0x800;
		micropklt_lcd_ctrl(5);
		msm_proc_comm_wince(&dex,0);
		mdelay(3);
		gpio_set_value(RAPH100_LCD_PWR1, 0);
		mdelay(10);
	}

	
}
static int htcdiamond_mddi_hitachi_panel_init(
					     struct msm_mddi_bridge_platform_data *bridge_data,
	  struct msm_mddi_client_data *client_data)
{

	client_data->auto_hibernate(client_data, 0);
	client_data->remote_write(client_data, 0x40004, GPIODATA);
	mdelay(2);
	htcdiamond_process_spi_table(client_data, hitachi_spi_table,
				      ARRAY_SIZE(hitachi_spi_table));
	client_data->auto_hibernate(client_data, 1);

	return 0;
}

static int htcdiamond_mddi_sharp_panel_init(
                                             struct msm_mddi_bridge_platform_data *bridge_data,
          struct msm_mddi_client_data *client_data)
{

        client_data->auto_hibernate(client_data, 0);
        htcdiamond_process_mddi_table(client_data, mddi_sharp_table,
                                      ARRAY_SIZE( mddi_sharp_table));
        client_data->auto_hibernate(client_data, 1);

        return 0;
}



static int htcdiamond_mddi_toshiba_client_init(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{

	printk("htcdiamond_mddi_toshiba_client_init\n");
	client_data->auto_hibernate(client_data, 0);
	
	if(!client_state && type) {
		htcdiamond_process_mddi_table(client_data, mddi_toshiba_common_init_table,
			ARRAY_SIZE(mddi_toshiba_common_init_table));
		mdelay(50);
		htcdiamond_process_mddi_table(client_data, mddi_toshiba_prim_start_table,
						ARRAY_SIZE(mddi_toshiba_prim_start_table));
		switch(type) {
			case 0:
				printk("unknown panel\n");
				break;
			case 1:
				printk("init hitachi panel on toshiba client\n");
				htcdiamond_mddi_hitachi_panel_init(bridge_data,client_data);
				break;
			case 2:
				printk("init sharp panel on toshiba client\n");
				htcdiamond_mddi_sharp_panel_init(bridge_data,client_data);
				break;
			default:
				printk("unknown panel_id: %d\n", type);
		};
	}
	client_state=1;

	client_data->auto_hibernate(client_data, 1);
	return 0;
}

static int htcdiamond_mddi_toshiba_client_uninit(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	client_state=0;
	return 0;
}

static int htcdiamond_mddi_panel_unblank(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	// not used
	return 0;

}

static int htcdiamond_mddi_panel_blank(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	// not used
	return 0;
}

static struct resource resources_msm_fb[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE,
		.flags = IORESOURCE_MEM,
	},
};

static struct msm_mddi_bridge_platform_data toshiba_client_data = {
	.init = htcdiamond_mddi_toshiba_client_init,
	.uninit = htcdiamond_mddi_toshiba_client_uninit,
	.blank = htcdiamond_mddi_panel_blank,
	.unblank = htcdiamond_mddi_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 640,
		.output_format = 0,
	},
};


static struct msm_mddi_platform_data mddi_pdata = {
	.clk_rate = 122880000,
	.power_client = htcdiamond_mddi_power_client,
	.fb_resource = resources_msm_fb,
	.num_clients = 2,
	.client_platform_data = {
		{
			.product_id = (0xd263 << 16 | 0),
			.name = "TC358720XBG",
			.id = 0,
			.client_data = &toshiba_client_data,
			.clk_rate = 0,
		},
	},
};

int __init htcdiamond_init_panel(void)
{
	int rc;
	
	if(!machine_is_htcdiamond()) {
		printk("Disabling Diamond Panel\n");
		return 0;
	}
	printk(KERN_INFO "%s: Initializing panel\n", __func__);

	if (!machine_is_htcdiamond() && !machine_is_htcdiamond_cdma() && !machine_is_htcdiamond() && !machine_is_htcdiamond_cdma()) {
		printk(KERN_INFO "%s: panel does not apply to this device, aborted\n", __func__);
		return 0;
	}

	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;
	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	return platform_device_register(&msm_device_mddi0);
}

device_initcall(htcdiamond_init_panel);
