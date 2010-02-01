/* linux/arch/arm/mach-msm/board-htctopaz.h
 */
#ifndef __ARCH_ARM_MACH_MSM_BOARD_HTCRHODIUM_H
#define __ARCH_ARM_MACH_MSM_BOARD_HTCRHODIUM_H

#include <mach/board.h>


#define DECLARE_MSM_IOMAP
#include <mach/msm_iomap.h>



#define RHODIUM_BAT_IRQ			28  // GPIO IRQ
#define RHODIUM_USB_AC_PWR		32
#define RHODIUM_CHARGE_EN_N		44
#define RHODIUM_KPD_IRQ			27  //Keyboard IRQ
#define RHODIUM_KB_SLIDER_IRQ		37  //Keyboard Slider IRQ //Currently Unknown, using stylus detect GPIO right now (37).
#define RHODIUM_BKL_PWR			86  //Keyboard blacklight  //Currently Unknown if this is right
#define RHODIUM_POWER_KEY     		83  //Power key


#define RHODIUM_USBPHY_RST		100


#endif 
