/* linux/arch/arm/mach-msm/board-htctopaz.h
 */
#ifndef __ARCH_ARM_MACH_MSM_BOARD_HTCRHODIUM_H
#define __ARCH_ARM_MACH_MSM_BOARD_HTCRHODIUM_H

#include <mach/board.h>


#define DECLARE_MSM_IOMAP
#include <mach/msm_iomap.h>


#define TOPA100_USB_AC_PWR		1

#define TOPA100_BAT_IRQ			28
#define TOPA100_CHARGE_EN_N		44
#define RHODIUM_KPD_IRQ			27  //Keyboard IRQ
#define RHODIUM_KB_SLIDER_IRQ		37  //Keyboard Slider IRQ //Currently Unknown, using stylus detect GPIO right now (37).
#define RHODIUM_BKL_PWR			86  //Keyboard blacklight  //Currently Unknown if this is right




#endif 
