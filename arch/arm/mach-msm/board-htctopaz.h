/* linux/arch/arm/mach-msm/board-htctopaz.h
 */
#ifndef __ARCH_ARM_MACH_MSM_BOARD_HTCTOPAZ_H
#define __ARCH_ARM_MACH_MSM_BOARD_HTCTOPAZ_H

#include <mach/board.h>


#define TOPA100_USB_AC_PWR		1

#define TOPA100_BAT_IRQ			28
#define TOPA100_CHARGE_EN_N		44

#define TOPA100_POWER_KEY		83
/*
 * To be confirmed
#define TOPA100_VOLUP_KEY		25
#define TOPA100_VOLDOWN_KEY		24
*/

#define TOPA100_GPIO_CABLE_IN1         18
#define TOPA100_GPIO_CABLE_IN2         45
#define TOPA100_GPIO_H2W_DATA          31
#define TOPA100_GPIO_H2W_CLK           46
#define TOPA100_GPIO_AUD_HSMIC_DET_N   17
#define TOPA100_USBPHY_RST             100

#endif /* GUARD */
