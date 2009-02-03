// OR'ed with scancode on the release event
#define MICROP_KSC_RELEASED_BIT 0x80
#define MICROP_KSC_SCANCODE_MASK (MICROP_KSC_RELEASED_BIT - 1)

#define MICROP_KSC_ID_SCANCODE	0x10
#define MICROP_KSC_ID_MODIFIER	0x11
#define MICROP_KSC_ID_VERSION	0x12
#define MICROP_KSC_ID_LED	0x13

enum {
	MICROP_KSC_LED_RESET,	// Resets LEDs to off
	MICROP_KSC_LED_CAPS,	// Caps Lock
	MICROP_KSC_LED_FN,	// FN lock
	MICROP_KSC_LED_MAX,
};

extern int micropksc_read_scancode(unsigned char *scancode, unsigned char *isdown);
extern int micropksc_set_led(unsigned int led, int value);
