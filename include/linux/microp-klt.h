/**
 * These are combined into a bitmask to turn individual LEDs on
 */
#define MICROP_KLT_LEDS_OFF   0x00
#define MICROP_KLT_LED_HOME   0x01
#define MICROP_KLT_LED_BACK   0x02
#define MICROP_KLT_LED_END    0x04
#define MICROP_KLT_LED_SEND   0x08
#define MICROP_KLT_LED_ACTION 0x10 // Turns the 4 action LEDs solid

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

#define MICROP_KLT_LCD_BIT 0x2000

// Default state is LCD backlight on, LEDs off
#define MICROP_KLT_DEFAULT_LED_STATES (MICROP_KLT_LCD_BIT | MICROP_KLT_LEDS_OFF)

/**
 * I2C data address IDs
 */
#define MICROP_KLT_ID_VERSION           0x30 // Chip revision
#define MICROP_KLT_ID_LED_STATE         0x40 // Set LED behavior using above bitmasks
#define MICROP_KLT_ID_KSC_NOTIFICATIONS 0x50 // When enabled, allows the keyboard IRQ to fire
