// OR'ed with scancode on the release event
#define MICROP_KSC_RELEASED_BIT 0x80
#define MICROP_KSC_SCANCODE_MASK (MICROP_KSC_RELEASED_BIT - 1)

#define MICROP_KSC_ID_SCANCODE 0x10
#define MICROP_KSC_ID_MODIFIER 0x11
#define MICROP_KSC_ID_VERSION  0x12
//XXX: This has more functions than just reset.. Rename when those other functions are identified
#define MICROP_KSC_ID_RESET    0x13

extern int micropklt_set_ksc_notifications(int on);

