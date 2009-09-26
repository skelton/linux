/* linux/arch/arm/mach-msm/board-htcraphael-panel.c
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

#include "board-htcraphael.h"
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

#define	PWM0OFF	      (PWM_BLOCK_BASE|0x1C)

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

#define SPI_WRITE16(reg, val) \
	{ SSICTL,	0x170 }, \
	{ SSITX,        0x00080000 | 0x10 }, \
	{ SSITX,        0x00010000 | (reg) & 0xffff) }, \
	{ SSICTL, 	0x172 }, \
	{ SSICTL, 	0x170 }, \
	{ SSITX,        0x00080000 | 0x12 }, \
	{ SSITX,        0x00010000 | (val) & 0xffff) }, \
	{ SSICTL, 	0x172 },

struct mddi_table {
	uint32_t reg;
	uint32_t value;
};

static struct mddi_table mddi_lcm_init_table[] = {
{0x0010801c,0x4bec0066},{1,50},
{0x00108020,0x00000113},
{0x00108024,0x00000000},
{0x00108028,0x00000001},{1,100},
{0x0010802c,0x00000001},
{0x00160004,0x0000a1ef},
{0x00170000,0x00000000},{1,50},
{0x00160000,0x00000000},
{0x00150000,0x03cf0000},
{0x00150004,0x000003cf},
{0x00150028,0x00000000},{1,50},
{0x00160008,0x00000001},
{0x00140008,0x00000060},
{0x00140000,0x00001388},{1,60},
{0x0014001c,0x00000001},
{0x00140028,0x00000060},
{0x00140020,0x00001388},
{0x0014001c,0x00000001},
{0x00140028,0x00000060},
{0x00140020,0x00001388},
{0x0014003c,0x00000001},
{0x00140008,0x000000e0},
{0x00140028,0x000000e0},
{0x00140068,0x00000003},
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
{0x00150000,0x00040004},
};

static struct mddi_table mddi_hitachi_panel_init_table[] = {
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

static struct mddi_table mddi_toshiba_panel_init_table[] = {
	{ SRST,         0x00000003 }, /* FIFO/LCDC not reset */
	{ PORT_ENB,     0x00000001 }, /* Enable sync. Port */
	{ START,        0x00000000 }, /* To stop operation */
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

	{ SSICTL,       0x00000170 }, /* SSI control register */
	{ SSITIME,      0x00000250 }, /* SSI timing control register */
	{ SSICTL,       0x00000172 }, /* SSI control register */
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

static struct mddi_table mddi_tpo_init_table[] = {
	{ VCYCLE,       0x000001e5 },
	{ HCYCLE,       0x000000ac },
	{ REGENB,       0x00000001 }, /* Set 1 to enable to change the value of registers. */
	{ 0,            20         }, /* udelay 20 */
	{ GPIODATA,     0x00000004 }, /* GPIO2 high */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ 0,            20         }, /* udelay 20 */

	SPI_WRITE(0x08, 0x01)
	{ 0,            500        }, /* udelay 500 */
	SPI_WRITE(0x08, 0x00)
	SPI_WRITE(0x02, 0x00)
	SPI_WRITE(0x03, 0x04)
	SPI_WRITE(0x04, 0x0e)
	SPI_WRITE(0x09, 0x02)
	SPI_WRITE(0x0b, 0x08)
	SPI_WRITE(0x0c, 0x53)
	SPI_WRITE(0x0d, 0x01)
	SPI_WRITE(0x0e, 0xe0)
	SPI_WRITE(0x0f, 0x01)
	SPI_WRITE(0x10, 0x58)
	SPI_WRITE(0x20, 0x1e)
	SPI_WRITE(0x21, 0x0a)
	SPI_WRITE(0x22, 0x0a)
	SPI_WRITE(0x23, 0x1e)
	SPI_WRITE(0x25, 0x32)
	SPI_WRITE(0x26, 0x00)
	SPI_WRITE(0x27, 0xac)
	SPI_WRITE(0x29, 0x06)
	SPI_WRITE(0x2a, 0xa4)
	SPI_WRITE(0x2b, 0x45)
	SPI_WRITE(0x2c, 0x45)
	SPI_WRITE(0x2d, 0x15)
	SPI_WRITE(0x2e, 0x5a)
	SPI_WRITE(0x2f, 0xff)
	SPI_WRITE(0x30, 0x6b)
	SPI_WRITE(0x31, 0x0d)
	SPI_WRITE(0x32, 0x48)
	SPI_WRITE(0x33, 0x82)
	SPI_WRITE(0x34, 0xbd)
	SPI_WRITE(0x35, 0xe7)
	SPI_WRITE(0x36, 0x18)
	SPI_WRITE(0x37, 0x94)
	SPI_WRITE(0x38, 0x01)
	SPI_WRITE(0x39, 0x5d)
	SPI_WRITE(0x3a, 0xae)
	SPI_WRITE(0x3b, 0xff)
	SPI_WRITE(0x07, 0x09)
	{ 0,            10         }, /* udelay 10 */
	{ START,        0x00000001 }, /* To start operation */
};

static struct mddi_table mddi_tpo_deinit_table[] = {
	SPI_WRITE(0x07, 0x19)
	{ START,        0x00000000 }, /* To stop operation */
	{ GPIODATA,     0x00040004 }, /* GPIO2 high */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ GPIODATA,     0x00040000 }, /* GPIO2 low */
	{ 0,            5        }, /* usleep 5 */
};


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

//static struct vreg *vreg_mddi_1v5;
//static struct vreg *vreg_lcm_2v85;

static void htcraphael_mddi_power_client(struct msm_mddi_client_data *client_data,
				    int on)
{
	struct msm_dex_command dex;
	int i;
	
	printk("htcraphael_mddi_power_client(%d)\n", on);
	printk("XC=%x\n", i=readl(MSM_SHARED_RAM_BASE + 0xfc048));
	

	if(on) {
		/*
		gpio_set_value(RAPH100_LCD_PWR1, 1);
		dex.cmd=PCOM_PMIC_REG_ON;
		dex.has_data=1;
		dex.data=0x800;
		msm_proc_comm_wince(&dex,0);
		mdelay(50);
		dex.data=0x2000;
		msm_proc_comm_wince(&dex,0);
		mdelay(50);
//		msm_gpio_set_function(DEX_GPIO_CFG(RAPH100_LCD_PWR2,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA,1));
		gpio_set_value(RAPH100_LCD_PWR2,1);		
		mdelay(50);
		*/
	} else {
/*
		gpio_set_value(RAPH100_LCD_PWR2, 0);
		mdelay(1);
		dex.cmd=PCOM_PMIC_REG_OFF;
		dex.has_data=1;
		dex.data=0x2000;
		msm_proc_comm_wince(&dex,0);
		mdelay(5); // delay time >5ms and <10ms
		dex.data=0x800;
		msm_proc_comm_wince(&dex,0);
		mdelay(3);
		gpio_set_value(RAPH100_LCD_PWR1, 0);
		mdelay(10);
*/
	}
	
	
}

static int htcraphael_mddi_epson_client_init(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	int panel_id;

	client_data->auto_hibernate(client_data, 0);
//	htcraphael_process_mddi_table(client_data, mddi_epson_init_table,
//				 ARRAY_SIZE(mddi_epson_init_table));
	client_data->auto_hibernate(client_data, 1);
#if 0
	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	if (panel_id > 1) {
		printk("unknown panel id (0x%08x) at mddi_enable\n", panel_id);
		return -1;
	}
#endif
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
	int panel_id, gpio_val;
	char *panels[]={"Hitachi","Sharp","Toppoly","Toppoly2"};

	client_data->auto_hibernate(client_data, 0);
//	htcraphael_process_mddi_table(client_data, mddi_lcm_init_table,
//				 ARRAY_SIZE(mddi_lcm_init_table));
	client_data->auto_hibernate(client_data, 1);

	gpio_val = client_data->remote_read(client_data, GPIODATA);
	panel_id=0;

	if ( (gpio_val & 0x10) != 0 ) panel_id++;
	if ( (gpio_val & 4) != 0 ) panel_id+=2;

	printk("toshiba GPIODATA=0x%08x panel_id=%d at toshiba_mddi_enable\n", gpio_val, panel_id);

	printk("found panel_id=%d at toshiba_mddi_enable, panel=%s\n", panel_id,panels[panel_id]);
	
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

	int panel_id, ret = 0;
	
	client_data->auto_hibernate(client_data, 0);
//	htcraphael_process_mddi_table(client_data, mddi_lcm_init_table,
//		ARRAY_SIZE(mddi_toshiba_panel_init_table));
	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	switch(panel_id) {
	 case 0:
		printk("init sharp panel\n");
//		htcraphael_process_mddi_table(client_data,
//					 mddi_sharp_init_table,
//					 ARRAY_SIZE(mddi_sharp_init_table));
		break;
	case 1:
		printk("init tpo panel\n");
//		htcraphael_process_mddi_table(client_data,
//					 mddi_tpo_init_table,
//					 ARRAY_SIZE(mddi_tpo_init_table));
		break;
	case 3:
		printk("init hitachi panel\n");
//		htcraphael_process_mddi_table(client_data,
//					 mddi_epson_init_table,
//					 ARRAY_SIZE(mddi_epson_init_table));
		break;
	default:
		printk("unknown panel_id: %d\n", panel_id);
		ret = -1;
	};
	//XXX: client_data->auto_hibernate(client_data, 1);
//	client_data->remote_write(client_data, GPIOSEL_VWAKEINT, GPIOSEL);
//	client_data->remote_write(client_data, INTMASK_VWAKEOUT, INTMASK);
	return ret;

}

static int htcraphael_mddi_panel_blank(
	struct msm_mddi_bridge_platform_data *bridge_data,
	struct msm_mddi_client_data *client_data)
{
	int panel_id, ret = 0;

	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	client_data->auto_hibernate(client_data, 0);
	switch(panel_id) {
	case 0:
		printk("deinit sharp panel\n");
//		htcraphael_process_mddi_table(client_data,
//					 mddi_sharp_deinit_table,
//					 ARRAY_SIZE(mddi_sharp_deinit_table));
		break;
	case 1:
		printk("deinit tpo panel\n");
//		htcraphael_process_mddi_table(client_data,
//					 mddi_tpo_deinit_table,
//					 ARRAY_SIZE(mddi_tpo_deinit_table));
		break;
	case 3:
		printk("deinit epson panel\n");
//		htcraphael_process_mddi_table(client_data,
//					 mddi_epson_deinit_table,
//					 ARRAY_SIZE(mddi_epson_deinit_table));
		break;
	default:
		printk("unknown panel_id: %d\n", panel_id);
		ret = -1;
	};
	client_data->auto_hibernate(client_data, 1);
	
//	client_data->remote_write(client_data, 0, SYSCLKENA);
//	client_data->remote_write(client_data, 1, DPSUS);
	return ret;
}

static struct resource resources_msm_fb[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE,
		.flags = IORESOURCE_MEM,
	},
};

struct msm_mddi_bridge_platform_data toshiba_client_data = {
	.init = htcraphael_mddi_toshiba_client_init,
	.uninit = htcraphael_mddi_toshiba_client_uninit,
	.blank = htcraphael_mddi_panel_blank,
	.unblank = htcraphael_mddi_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 640,
		.output_format = 0,
	},
};

static struct msm_mddi_bridge_platform_data epson_client_data = {
	.init = htcraphael_mddi_epson_client_init,
	.uninit = htcraphael_mddi_epson_client_uninit,
	.blank = htcraphael_mddi_panel_blank,
	.unblank = htcraphael_mddi_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 640,
		.output_format = 0,
	},
};

static struct msm_mddi_platform_data mddi_pdata = {
	.clk_rate = 122880000,
	.power_client = htcraphael_mddi_power_client,
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
		{
			.product_id = (0x4ca3 << 16 | 0),
			.name = "S1D13774",
			.id = 0,
			.client_data = &epson_client_data,
			.clk_rate = 0,
		},
	},
};

int __init htcraphael_init_panel(void)
{
	int rc;
	
	printk(KERN_INFO "%s: Initializing panel\n", __func__);

	if (!machine_is_htcraphael() && !machine_is_htcraphael_cdma() /*&& !machine_is_htcdiamond()*/ && !machine_is_htcdiamond_cdma()) {
		printk(KERN_INFO "%s: disabling raphael panel\n", __func__);
		return 0;
	}

/*	vreg_mddi_1v5 = vreg_get(0, "gp2");
	if (IS_ERR(vreg_mddi_1v5))
		return PTR_ERR(vreg_mddi_1v5);
	vreg_lcm_2v85 = vreg_get(0, "gp4");
	if (IS_ERR(vreg_lcm_2v85))
		return PTR_ERR(vreg_lcm_2v85);

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
*/
	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;
	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	return platform_device_register(&msm_device_mddi0);
}

device_initcall(htcraphael_init_panel);
