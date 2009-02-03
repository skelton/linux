/**
 * These are combined into a bitmask to turn individual LEDs on
 */
enum microp_led_t {
	MICROP_KLT_LED_HOME,	// 0x01
	MICROP_KLT_LED_BACK,	// 0x02
	MICROP_KLT_LED_END,	// 0x04
	MICROP_KLT_LED_SEND,	// 0x08
	MICROP_KLT_LED_ACTION,	// 0x10
	MICROP_KLT_BKL_LCD = 13,// 0x2000
	MICROP_KLT_BKL_KBD = 14,// 0x4000
	MICROP_KLT_LED_CNT = 7
};
#define MICROP_KLT_LEDS_OFF   0x00

/**
 * These behaviors are repeated approximately every 5 seconds
 * These cannot be combined
 */
#define MICROP_KLT_SYSLED_NONE        0x0 // No predefined behavior
#define MICROP_KLT_SYSLED_RING      0x100 // Action and Send LEDs flash quickly
#define MICROP_KLT_SYSLED_BLINK     0x200 // Single flash of all 4 action LEDs
#define MICROP_KLT_SYSLED_BREATHE   0x400 // Bottom-to-top fade on action LEDs
#define MICROP_KLT_SYSLED_FADE      0x500 // Slow fade on/off of action LEDs
#define MICROP_KLT_SYSLED_ROTATE    0x800 // Counter-clockwise rotate of action LEDs
#define MICROP_KLT_SYSLED_VERTICAL 0x1000 // Top and bottom action LEDs flash twice

// Default state is LCD backlight on, LEDs off
#define MICROP_KLT_DEFAULT_LED_STATES ( (1U << MICROP_KLT_BKL_LCD) | MICROP_KLT_LEDS_OFF)
#define MICROP_KLT_ALL_LEDS	0xffff

/**
 * I2C data address IDs
 */
#define MICROP_KLT_ID_VERSION           0x30 // Chip revision
#define MICROP_KLT_ID_LED_STATE         0x40 // Set LED behavior using above bitmasks
#define MICROP_KLT_ID_LCD_BRIGHTNESS    0x22 // Set brightness of LCD backlight

extern int micropklt_set_led_states(unsigned leds_mask, unsigned leds_values);
extern int micropklt_set_lcd_state(int on);
extern int micropklt_set_kbd_state(int on);
