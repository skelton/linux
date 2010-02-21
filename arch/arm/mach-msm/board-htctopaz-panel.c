/* linux/arch/arm/mach-msm/board-htctopaz-panel.c
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
#include <linux/microp-klt.h>

#include "board-htctopaz.h"
#include "proc_comm_wince.h"
#include "devices.h"

static struct clk *gp_clk;

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

struct mddi_table {
	uint32_t reg;
	uint32_t value;
};
static struct mddi_table mddi_toshiba_init_table[] = {
#if 0
	{ DPSET0,       0x09e90046 },
	{ DPSET1,       0x00000118 },
	{ DPSUS,        0x00000000 },
	{ DPRUN,        0x00000001 },
	{ 1,            14         }, /* msleep 14 */
	{ SYSCKENA,     0x00000001 },
	//{ CLKENB,       0x000000EF },
	{ CLKENB,       0x0000A1EF },  /*    # SYS.CLKENB  # Enable clocks for each module (without DCLK , i2cCLK) */
	//{ CLKENB,       0x000025CB }, /* Clock enable register */

	{ GPIODATA,     0x02000200 },  /*   # GPI .GPIODATA  # GPIO2(RESET_LCD_N) set to 0 , GPIO3(eDRAM_Power) set to 0 */
	{ GPIODIR,      0x000030D  },  /* 24D   # GPI .GPIODIR  # Select direction of GPIO port (0,2,3,6,9 output) */
	{ GPIOSEL,      0/*0x00000173*/},  /*   # SYS.GPIOSEL  # GPIO port multiplexing control */
	{ GPIOPC,       0x03C300C0 },  /*   # GPI .GPIOPC  # GPIO2,3 PD cut */
	{ WKREQ,        0x00000000 },  /*   # SYS.WKREQ  # Wake-up request event is VSYNC alignment */

	{ GPIOIBE,      0x000003FF },
	{ GPIOIS,       0x00000000 },
	{ GPIOIC,       0x000003FF },
	{ GPIOIE,       0x00000000 },

	{ GPIODATA,     0x00040004 },  /*   # GPI .GPIODATA  # eDRAM VD supply */
	{ 1,            1          }, /* msleep 1 */
	{ GPIODATA,     0x02040004 },  /*   # GPI .GPIODATA  # eDRAM VD supply */
	{ DRAMPWR,      0x00000001 }, /* eDRAM power */
#endif
};


static struct mddi_table mddi_epson_init_table[] = {
	{ SRST,         0x00000003 }, /* FIFO/LCDC not reset */
	{ PORT_ENB,     0x00000001 }, /* Enable sync. Port */
	{ START,        0x00000000 }, /* To stop operation */
#if 0
	//{ START,        0x00000001 }, /* To start operation */
	{ PORT,         0x00000004 }, /* Polarity of VS/HS/DE. */
	{ CMN,          0x00000000 },
	{ GAMMA,        0x00000000 }, /* No Gamma correction */
	{ INTFLG,       0x00000000 }, /* VSYNC interrupt flag clear/status */
	{ INTMSK,       0x00000000 }, /* VSYNC interrupt mask is off. */
	{ MPLFBUF,      0x00000000 }, /* Select frame buffer's base address. */
	{ HDE_LEFT,     0x00000000 }, /* The value of HDE_LEFT. */
	{ VDE_TOP,      0x00000000 }, /* The value of VDE_TPO. */
	{ PXL,          0x00000001 }, /* 1. RGB666 */
	                              /* 2. Data is valid from 1st frame of beginning. */
	{ HDE_START,    0x00000006 }, /* HDE_START= 14 PCLK */
	{ HDE_SIZE,     0x0000009F }, /* HDE_SIZE=320 PCLK */
	{ HSW,          0x00000004 }, /* HSW= 10 PCLK */
	{ VSW,          0x00000001 }, /* VSW=2 HCYCLE */
	{ VDE_START,    0x00000003 }, /* VDE_START=4 HCYCLE */
	{ VDE_SIZE,     0x000001DF }, /* VDE_SIZE=480 HCYCLE */
	{ WAKEUP,       0x000001e2 }, /* Wakeup position in VSYNC mode. */
	{ WSYN_DLY,     0x00000000 }, /* Wakeup position in VSIN mode. */
	{ REGENB,       0x00000001 }, /* Set 1 to enable to change the value of registers. */
	{ CLKENB,       0x000025CB }, /* Clock enable register */
#endif
	{ SSICTL,       0x00000170 }, /* SSI control register */
	{ SSITIME,      0x00000250 }, /* SSI timing control register */
	{ SSICTL,       0x00000172 }, /* SSI control register */
};

static struct mddi_table mddi_epson_deinit_table[] = {
	{ 1,            5        }, /* usleep 5 */
};

static struct mddi_table mddi_sharp_init_table[] = {
	{ VCYCLE,       0x000001eb },
	{ HCYCLE,       0x000000ae },
	{ REGENB,       0x00000001 }, /* Set 1 to enable to change the value of registers. */
	{ GPIODATA,     0x00040000 }, /* GPIO2 low */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ 1,            1          }, /* msleep 1 */
	{ GPIODATA,     0x00040004 }, /* GPIO2 high */
	{ 1,            10         }, /* msleep 10 */
	SPI_WRITE(0x5f, 0x01)
	SPI_WRITE1(0x11)
	{ 1,            200        }, /* msleep 200 */
	SPI_WRITE1(0x29)
	SPI_WRITE1(0xde)
	{ START,        0x00000001 }, /* To start operation */
};

static struct mddi_table mddi_sharp_deinit_table[] = {
	{ 1,            200        }, /* msleep 200 */
	SPI_WRITE(0x10, 0x1)
	{ 1,            100        }, /* msleep 100 */
	{ GPIODATA,     0x00040004 }, /* GPIO2 high */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ GPIODATA,     0x00040000 }, /* GPIO2 low */
	{ 1,            10         }, /* msleep 10 */
};



static struct microp_spi_table spi_init_table[] = {
	{0xc0,0x00,0x86},
	{0xc0,0x01,0x00},
	{0xc0,0x02,0x86},
	{0xc0,0x03,0x00},
	{0xc1,0x00,0x40},
	{0xc2,0x00,0x21},
	{0xc2,0x02,0x02},
	{0xc7,0x00,0x91},
	{0x35,0x00,0x02},
	{0x44,0x00,0x00},
	{0x44,0x01,0x4f},
	{0xe0,0x00,0x01},
	{0xe0,0x01,0x05},
	{0xe0,0x02,0x1c},
	{0xe0,0x03,0x33},
	{0xe0,0x04,0x21},
	{0xe0,0x05,0x35},
	{0xe0,0x06,0x60},
	{0xe0,0x07,0x33, 25},
	{0xe0,0x08,0x24},
	{0xe0,0x09,0x26},
	{0xe0,0x0a,0x84},
	{0xe0,0x0b,0x15},
	{0xe0,0x0c,0x3a},
	{0xe0,0x0d,0x4f},
	{0xe0,0x0e,0x8c},
	{0xe0,0x0f,0xaf},
	{0xe0,0x10,0x4b},
	{0xe0,0x11,0x4d},
	{0xe1,0x00,0x01},
	{0xe1,0x01,0x05},
	{0xe1,0x02,0x1c},
	{0xe1,0x03,0x33},
	{0xe1,0x04,0x21},
	{0xe1,0x05,0x35},
	{0xe1,0x06,0x62},
	{0xe1,0x07,0x33},
	{0xe1,0x08,0x26},
	{0xe1,0x09,0x26, 25},
	{0xe1,0x0a,0x84},
	{0xe1,0x0b,0x11},
	{0xe1,0x0c,0x38},
	{0xe1,0x0d,0x4d},
	{0xe1,0x0e,0x8a},
	{0xe1,0x0f,0xad},
	{0xe1,0x10,0x49},
	{0xe1,0x11,0x4d},
	{0xe2,0x00,0x08},
	{0xe2,0x01,0x19},
	{0xe2,0x02,0x28},
	{0xe2,0x03,0x3e},
	{0xe2,0x04,0x1f},
	{0xe2,0x05,0x34},
	{0xe2,0x06,0x68},
	{0xe2,0x07,0x43},
	{0xe2,0x08,0x2b},
	{0xe2,0x09,0x2a},
	{0xe2,0x0a,0x88},
	{0xe2,0x0b,0x19, 25},
	{0xe2,0x0c,0x3c},
	{0xe2,0x0d,0x52},
	{0xe2,0x0e,0x8b},
	{0xe2,0x0f,0xad},
	{0xe2,0x10,0x4d},
	{0xe2,0x11,0x4d},
	{0xe3,0x00,0x08},
	{0xe3,0x01,0x19},
	{0xe3,0x02,0x28},
	{0xe3,0x03,0x3e},
	{0xe3,0x04,0x1f},
	{0xe3,0x05,0x34},
	{0xe3,0x06,0x68},
	{0xe3,0x07,0x43},
	{0xe3,0x08,0x2d},
	{0xe3,0x09,0x2a},
	{0xe3,0x0a,0x88},
	{0xe3,0x0b,0x15},
	{0xe3,0x0c,0x3a},
	{0xe3,0x0d,0x50, 25},
	{0xe3,0x0e,0x89},
	{0xe3,0x0f,0xab},
	{0xe3,0x10,0x4b},
	{0xe3,0x11,0x4d},
	{0xe4,0x00,0x7a},
	{0xe4,0x01,0x76},
	{0xe4,0x02,0x7d},
	{0xe4,0x03,0x90},
	{0xe4,0x04,0x3b},
	{0xe4,0x05,0x40},
	{0xe4,0x06,0x6a},
	{0xe4,0x07,0x61},
	{0xe4,0x08,0x29},
	{0xe4,0x09,0x29},
	{0xe4,0x0a,0x94},
	{0xe4,0x0b,0x15},
	{0xe4,0x0c,0x35},
	{0xe4,0x0d,0x4f},
	{0xe4,0x0e,0x9b},
	{0xe4,0x0f,0xcc, 25},
	{0xe4,0x10,0x4d},
	{0xe4,0x11,0x4d},
	{0xe5,0x00,0x7a},
	{0xe5,0x01,0x76},
	{0xe5,0x02,0x7d},
	{0xe5,0x03,0x90},
	{0xe5,0x04,0x3b},
	{0xe5,0x05,0x40},
	{0xe5,0x06,0x6c},
	{0xe5,0x07,0x61},
	{0xe5,0x08,0x2b},
	{0xe5,0x09,0x29},
	{0xe5,0x0a,0x94},
	{0xe5,0x0b,0x11},
	{0xe5,0x0c,0x33},
	{0xe5,0x0d,0x4d},
	{0xe5,0x0e,0x99},
	{0xe5,0x0f,0xca},
	{0xe5,0x10,0x4b},
	{0xe5,0x11,0x4d, 25},
	{0xf4,0x02,0x14},
	{0xf1,0x00,0x0c, 25},
	{0xb6,0x00,0x10},
};

static struct microp_spi_table spi_deinit_table[] = {
	{0x53,0x00,0x00},
	{0x53,0x00,0x00, 25},
	{0x28,0x00,0x00},
	{0x10,0x00,0x00},
};

static struct vreg *vreg_lcd_1;	/* LCD1 */
static struct vreg *vreg_lcd_2;	/* LCD2 */

#define	spicmdreg	0x70;

#define GPIOSEL_VWAKEINT (1U << 0)
#define INTMASK_VWAKEOUT (1U << 0)

static void htcraphael_process_mddi_table(struct msm_mddi_client_data *client_data,
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
		else
			client_data->remote_write(client_data, value, reg);
	}
}


extern void  micropklt_lcd_ctrl(int);

static void htcraphael_mddi_power_client(struct msm_mddi_client_data *client_data,
				    int on)
{
	printk("htcraphael_mddi_power_client(%d)\n", on);
#if 0
#warning htcraphael_mddi_power_client not yet implemented
#endif


	return 0;

	
}



static int htcraphael_mddi_epson_client_init(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
  	printk(KERN_ERR "EPSON PANEL\n");
	int panel_id;

/*	client_data->auto_hibernate(client_data, 0);
	htcraphael_process_mddi_table(client_data, mddi_epson_init_table,
				 ARRAY_SIZE(mddi_epson_init_table));
	client_data->auto_hibernate(client_data, 1);
	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	if (panel_id < 3 || panel_id > 3) {
		printk("unknown panel id (0x%08x) at mddi_enable\n", panel_id);
		return -1;
	}

*/
	return 0;
}

extern int micropklt_lcd_precess_spi_table(uint32_t, struct microp_spi_table*, size_t);
extern int micropklt_lcd_precess_cmd(char*, size_t);
static int htctopaz_mddi_hitachi_panel_init(
					     struct msm_mddi_bridge_platform_data *bridge_data,
					      struct msm_mddi_client_data *client_data)
{
	if(machine_is_htcrhodium())
		return 0;
	struct msm_dex_command dex;
	int ret;
	char con[2];
	client_data->auto_hibernate(client_data, 0);
	ret = vreg_enable(vreg_lcd_1);
	if (ret)
		return ret;
	ret = vreg_enable(vreg_lcd_2);
	if (ret)
		return ret;
	mdelay(50);
	
	gpio_configure(57, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	gpio_set_value(57, 0);
	gpio_configure(58, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	gpio_set_value(58, 0);

	msleep(5);
	dex.cmd=PCOM_PMIC_REG_ON;
	dex.has_data=1;
	dex.data=0x80;
	msm_proc_comm_wince(&dex,0);
	
	msleep(5);
	dex.data=0x2000;
	msm_proc_comm_wince(&dex,0);
	msleep(5);

	gpio_configure(87, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
	gpio_set_value(87, 1);
	msleep(10);
	
	
	char c0[]={MICROP_KLT_ID_SPICTRL, 0x01};
	micropklt_lcd_precess_cmd(c0, ARRAY_SIZE(c0));
	msleep(10);
	micropklt_lcd_precess_spi_table(MICROP_KLT_ID_SPILCMDDATA, spi_init_table, ARRAY_SIZE(spi_init_table));
	msleep(20);
	char c2[]={MICROP_KLT_ID_LCD_BRIGHTNESS, 0x90};
	micropklt_lcd_precess_cmd(c2, ARRAY_SIZE(c2));
	char c3[]={MICROP_KLT_ID_SPILCMDDATA, 0x11, 0x00, 0x00};
	micropklt_lcd_precess_cmd(c3, ARRAY_SIZE(c3));
	char c4[]={MICROP_KLT_ID_SPILCMDDATA, 0x29, 0x00, 0x00};
	micropklt_lcd_precess_cmd(c4, ARRAY_SIZE(c4));
	micropklt_lcd_precess_cmd(c2, ARRAY_SIZE(c2));
	char c5[]={MICROP_KLT_ID_SPILCMDDATA, 0x53, 0x00, 0x2c};
	micropklt_lcd_precess_cmd(c5, ARRAY_SIZE(c5));

	client_data->auto_hibernate(client_data, 1);

	return 0;
}

static int htctopaz_mddi_hitachi_panel_deinit(
					     struct msm_mddi_bridge_platform_data *bridge_data,
					      struct msm_mddi_client_data *client_data)
{
	if(machine_is_htcrhodium())
		return 0;
	int ret;
	char con[2];
	client_data->auto_hibernate(client_data, 0);
	mdelay(2);
	struct msm_dex_command dex;
	
	micropklt_lcd_precess_spi_table(MICROP_KLT_ID_SPILCMDDATA, spi_deinit_table, ARRAY_SIZE(spi_deinit_table));
	char c1[]={MICROP_KLT_ID_SPICTRL, 0x00};
	micropklt_lcd_precess_cmd(c1, ARRAY_SIZE(c1));

	msleep(50);
	gpio_configure(87, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	gpio_set_value(87, 0);
	msleep(10);

	
	dex.cmd=PCOM_PMIC_REG_OFF;
	dex.has_data=1;
	dex.data=0x2000;
	msm_proc_comm_wince(&dex,0);
	msleep(5);
	dex.data=0x80;
	msm_proc_comm_wince(&dex,0);

	gpio_configure(57, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
	gpio_set_value(57, 1);
	gpio_configure(58, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
	gpio_set_value(58, 1);
	
	vreg_disable(vreg_lcd_1);
	vreg_disable(vreg_lcd_2);
	mdelay(50);
	client_data->auto_hibernate(client_data, 1);

	return 0;
}

static int htcraphael_mddi_epson_client_uninit(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	return 0;
}

static int htcraphael_mddi_toshiba_client_init(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	printk(KERN_ERR "TOSHIBA PANEL\n");

	if(machine_is_htcrhodium())
		return 0;
	int panel_id;

	client_data->auto_hibernate(client_data, 0);
	htcraphael_process_mddi_table(client_data, mddi_toshiba_init_table,
				 ARRAY_SIZE(mddi_toshiba_init_table));
	client_data->auto_hibernate(client_data, 1);
	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	if (panel_id > 1) {
		printk("unknown panel id (0x%08x) at mddi_enable\n", panel_id);
		return -1;
	}
	return 0;
}

static int htcraphael_mddi_toshiba_client_uninit(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	return 0;
}

static int htcraphael_mddi_panel_unblank(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{

	 printk(KERN_ERR "UNBLANK\n");
	int panel_id, ret = 0;
	
	if(machine_is_htcrhodium())
		return 0;
	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;

	switch(panel_id) {
		case 0:
			printk("unknown panel\n");
			break;
		case 3:
			printk(KERN_ERR "init hitachi panel on toshiba client\n");
			htctopaz_mddi_hitachi_panel_init(bridge_data,client_data);
			break;
		default:
			printk("unknown panel_id: %d\n", panel_id);
	};

	return 0;

}

static int htcraphael_mddi_panel_blank(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	if(machine_is_htcrhodium())
		return 0;
  	printk(KERN_ERR "BLANK\n");
	int panel_id, ret = 0;

	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;

	switch(panel_id) {
		case 0:
			printk("unknown panel\n");
			break;
		case 3:
			printk(KERN_ERR "deinit hitachi panel on epson client\n");
			htctopaz_mddi_hitachi_panel_deinit(bridge_data,client_data);
			break;
		default:
			printk("unknown panel_id: %d\n", panel_id);
	};

	return 0;

}

extern struct resource resources_msm_fb[];

struct msm_mddi_bridge_platform_data toshiba_client_data = {
	.init = htcraphael_mddi_toshiba_client_init,
	.uninit = htcraphael_mddi_toshiba_client_uninit,
	.blank = htcraphael_mddi_panel_blank,
	.unblank = htcraphael_mddi_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 800, //orux
		.output_format = 0,
	},
};


struct msm_mddi_bridge_platform_data epson_client_data = {
	.init = htcraphael_mddi_epson_client_init,
	.uninit = htcraphael_mddi_epson_client_uninit,
	.blank = htcraphael_mddi_panel_blank,
	.unblank = htcraphael_mddi_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 800, //orux
		.output_format = 0,
	},
};

struct msm_mddi_platform_data mddi_pdata = {
	.clk_rate = 122880000,
	.power_client = htcraphael_mddi_power_client,
	.fb_resource = resources_msm_fb,
	.num_clients = 4,
	.client_platform_data = {
		{
			.product_id = (0xd263 << 16 | 0),
			.name = "TC358720XBG",
			.id = 0,
			.client_data = &toshiba_client_data,
			.clk_rate = 0,
		},
		{
			.product_id = (0x4ca3 << 16 | 0),
			.name = "S1D13774",
			.id = 0,
			.client_data = &epson_client_data,
			.clk_rate = 0,
		},
		{
			.product_id = (0xb9f6 << 16 | 0x5580),
			.name = "S1D13774", /* TODO */
			.id = 0,
			.client_data = &epson_client_data,
			.clk_rate = 0,
		},
		{
			.product_id = (0xb9f6 << 16 | 0x5582),
			.name = "S1D13774", /* TODO */
			.id = 0,
			.client_data = &epson_client_data,
			.clk_rate = 0,
		},
	},
};

int __init htctopaz_init_panel(void)
{
	int rc;
	
	printk(KERN_INFO "%s: Initializing panel\n", __func__);
	if(!machine_is_htctopaz() && !machine_is_htcrhodium()) {
		printk(KERN_INFO "%s: panel does not apply to this device, aborted\n", __func__);
		return 0;
	}

	gp_clk = clk_get(NULL, "gp_clk");
	if (IS_ERR(gp_clk)) {
		printk(KERN_ERR "%s: could not get gp clock\n", __func__);
		gp_clk = NULL;
	}
	rc = clk_set_rate(gp_clk, 19200000);
	if (rc)
	{
		printk(KERN_ERR "%s: set clock rate failed\n", __func__);
	}
	vreg_lcd_1 = vreg_get(0, "gp2");
	if (IS_ERR(vreg_lcd_1))
		return PTR_ERR(vreg_lcd_1);

	vreg_lcd_2 = vreg_get(0, "gp4");
	if (IS_ERR(vreg_lcd_2))
		return PTR_ERR(vreg_lcd_2);

	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;
	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	return platform_device_register(&msm_device_mddi0);
}

device_initcall(htctopaz_init_panel);
