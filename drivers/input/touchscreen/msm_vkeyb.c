/* drivers/input/touchscreen/msm_vkeyb.c
 *
 * By Octavian Voicu <octavian@voicu.gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* README
 *
 * Note: there are a few ugly hacks to make this work, but it works
 * so it's ok for now (touchscreen irqs don't fire, so we just poll
 * 20 times a second, we access framebuffer memory directly, so
 * msm_fb was modified to update the whole screen every time).
 *
 * Using the virtual keyboard:
 *
 * 1. Don't touch the screen while booting
 * 2. After you entered the shell, tap the screen once
 *    (you will get a message saying you entered calibration mode)
 * 3. Tap the red pixel in the upper-left corner of the screen
 *    (a message will confirm your selection)
 * 4. Tap the red pixel in the lower-right corner of the screen
 *    (a message will confirm your selection)
 * 5. You can draw yellow dots on the screen by tapping and dragging
 *    (netripper: removed 2009-01-09, not needed anymore)
 * 6. Type by tapping the keys;
 *    Shift/Alt/Ctrl/Caps/SysRq are sticky keys, they work in toggle mode;
 *    Left Shift/Alt/Ctrl reset automatically after tapping a normal key;
 *    Activating Shift shows the alternate chars on each key;
 *    Alt+Sys+KEY is the magic SysRq, see Alt+Sys+h for more info.
 *    Shift+PgU/PgD work for scrolling the framebuffer console.
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/font.h>
#include <linux/fb.h>
#include <linux/fb_helper.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_fb.h>

#ifndef CONFIG_FONT_8x8
#error "Must compile with CONFIG_FONT_8x8 (VGA 8x8 font) for this module to work"
#endif

#define MSM_VKEYB_LAST_KEY	KEY_PAUSE
#define MSM_VKEYB_ROWS		6
#define MSM_VKEYB_COLS		15

#define MSM_VKEYB_LCD_WIDTH	480
#define MSM_VKEYB_LCD_HEIGHT	640

static int MSM_VKEYB_LEFT =	85;
static int MSM_VKEYB_TOP =	0;
#define MSM_VKEYB_KEY_W		26
#define MSM_VKEYB_KEY_H		13
#define MSM_VKEYB_DRAG		30

#define MSM_VKEYB_FONT_COLS	8
#define MSM_VKEYB_FONT_ROWS	8

#define FONTDATA(ch, x, y)	((*(((unsigned char*) font_vga_8x8.data) + ch * MSM_VKEYB_FONT_ROWS + y)) & (1 << (MSM_VKEYB_FONT_COLS - 1 - (x))))
#define SHIFTACTIVE()		(vkeyb_key_state[KEY_LEFTSHIFT] || vkeyb_key_state[KEY_RIGHTSHIFT])
#define CAPSACTIVE()		(vkeyb_key_state[KEY_CAPSLOCK])
#define ISALPHA(x)		(((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z'))
#define TOGGLECASE(x)		((x) ^ 0x20)

#define RGB(r, g, b)		(((((r) * 0x1f / 0xff) & 0x1f) << 11) | ((((g) * 0x3f / 0xff) & 0x3f) << 5) | ((((b) * 0x1f / 0xff) & 0x1f) << 0))

#define COLOR_TRANSPARENT	0x6666
#define COLOR_BLACK		RGB(0x00, 0x00, 0x00)
#define COLOR_WHITE		RGB(0xFF, 0xFF, 0xFF)
#define COLOR_DARKGRAY		RGB(0x40, 0x40, 0x40)
#define COLOR_GRAY		RGB(0x80, 0x80, 0x80)
#define COLOR_LIGHTGRAY		RGB(0xC0, 0xC0, 0xC0)
#define COLOR_RED		RGB(0xFF, 0x00, 0x00)
#define COLOR_GREEN		RGB(0x00, 0xFF, 0x3F)
#define COLOR_BLUE		RGB(0x00, 0x3F, 0xFF)

void msm_vkeyb_plot(int x,int y,unsigned short c);

/* Font for drawing the keyboard */
extern const struct font_desc font_vga_8x8;

/* returns whether position was inside vkeyb, so as to eat event */
typedef int msm_ts_handler_t(int, int, int);
extern msm_ts_handler_t *msm_ts_handler;

static struct input_dev *msm_vkeyb_dev;

static int vkeyb_key_state[MSM_VKEYB_LAST_KEY];

/* Sticky keys remain pressed after being tapped */
static int vkeyb_key_sticky[MSM_VKEYB_LAST_KEY] = {
	[KEY_LEFTSHIFT] = 1,
	[KEY_RIGHTSHIFT] = 1,
	[KEY_LEFTCTRL] = 1,
	[KEY_RIGHTCTRL] = 1,
	[KEY_LEFTALT] = 1,
	[KEY_RIGHTALT] = 1,
	[KEY_CAPSLOCK] = 1,
	[KEY_SYSRQ] = 1,
};

/* Sticky keys that have auto reset, will release automatically after a normal key is pressed */
static int vkeyb_key_auto_reset[MSM_VKEYB_LAST_KEY] = {
	[KEY_LEFTSHIFT] = 1,
	[KEY_LEFTCTRL] = 1,
	[KEY_LEFTALT] = 1,
};

static int vkeyb_keyboard[MSM_VKEYB_ROWS][MSM_VKEYB_COLS] = {
	{ KEY_ESC, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_INSERT, KEY_DELETE },
	{ KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_HOME },
	{ KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_PAGEUP },
	{ KEY_CAPSLOCK, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_ENTER, KEY_ENTER, KEY_PAGEDOWN },
	{ KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_SYSRQ, KEY_UP, KEY_END },
	{ KEY_LEFTCTRL, KEY_LEFTALT, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_RIGHTALT, KEY_RIGHTCTRL, KEY_LEFT, KEY_DOWN, KEY_RIGHT}
};

static int vkeyb_active_key;
static int vkeyb_landscape = 0;
/* drag thing */
static int vkeyb_drag_track = 0;
static int vkeyb_drag_x;
static int vkeyb_drag_y;
static int vkeyb_moved;
static int vkeyb_toggle = 1;

static char *vkeyb_key_func[MSM_VKEYB_LAST_KEY] = {
	[KEY_ESC] = "Esc",
	[KEY_F1] = "F1",
	[KEY_F2] = "F2",
	[KEY_F3] = "F3",
	[KEY_F4] = "F4",
	[KEY_F5] = "F5",
	[KEY_F6] = "F6",
	[KEY_F7] = "F7",
	[KEY_F8] = "F8",
	[KEY_F9] = "F9",
	[KEY_F10] = "F10",
	[KEY_F11] = "F11",
	[KEY_F12] = "F12",
	[KEY_INSERT] = "Ins",
	[KEY_DELETE] = "Del",
	[KEY_GRAVE] = "`",
	[KEY_1] = "1",
	[KEY_2] = "2",
	[KEY_3] = "3",
	[KEY_4] = "4",
	[KEY_5] = "5",
	[KEY_6] = "6",
	[KEY_7] = "7",
	[KEY_8] = "8",
	[KEY_9] = "9",
	[KEY_0] = "0",
	[KEY_MINUS] = "-",
	[KEY_EQUAL] = "=",
	[KEY_BACKSPACE] = "Bks",
	[KEY_HOME] = "Hom",
	[KEY_TAB] = "Tab",
	[KEY_Q] = "q",
	[KEY_W] = "w",
	[KEY_E] = "e",
	[KEY_R] = "r",
	[KEY_T] = "t",
	[KEY_Y] = "y",
	[KEY_U] = "u",
	[KEY_I] = "i",
	[KEY_O] = "o",
	[KEY_P] = "p",
	[KEY_LEFTBRACE] = "[",
	[KEY_RIGHTBRACE] = "]",
	[KEY_BACKSLASH] = "\\",
	[KEY_PAGEUP] = "PgU",
	[KEY_CAPSLOCK] = "Cap",
	[KEY_A] = "a",
	[KEY_S] = "s",
	[KEY_D] = "d",
	[KEY_F] = "f",
	[KEY_G] = "g",
	[KEY_H] = "h",
	[KEY_J] = "j",
	[KEY_K] = "k",
	[KEY_L] = "l",
	[KEY_SEMICOLON] = ";",
	[KEY_APOSTROPHE] = "'",
	[KEY_ENTER] = "Enter",
	[KEY_PAGEDOWN] = "PgD",
	[KEY_LEFTSHIFT] = "\x1e",
	[KEY_Z] = "z",
	[KEY_X] = "x",
	[KEY_C] = "c",
	[KEY_V] = "v",
	[KEY_B] = "b",
	[KEY_N] = "n",
	[KEY_M] = "m",
	[KEY_COMMA] = ",",
	[KEY_DOT] = ".",
	[KEY_SLASH] = "/",
	[KEY_RIGHTSHIFT] = "\x1e",
	[KEY_SYSRQ] = "Sys",
	[KEY_UP] = "\x18",
	[KEY_END] = "End",
	[KEY_LEFTCTRL] = "Ctr",
	[KEY_LEFTALT] = "Alt",
	[KEY_SPACE] = "Space",
	[KEY_RIGHTALT] = "Alt",
	[KEY_RIGHTCTRL] = "Ctr",
	[KEY_LEFT] = "\x1b",
	[KEY_DOWN] = "\x19",
	[KEY_RIGHT] = "\x1a",
};

static char *vkeyb_key_shift_func[MSM_VKEYB_LAST_KEY] = {
	[KEY_GRAVE] = "~",
	[KEY_1] = "!",
	[KEY_2] = "@",
	[KEY_3] = "#",
	[KEY_4] = "$",
	[KEY_5] = "%",
	[KEY_6] = "^",
	[KEY_7] = "&",
	[KEY_8] = "*",
	[KEY_9] = "(",
	[KEY_0] = ")",
	[KEY_MINUS] = "_",
	[KEY_EQUAL] = "+",
	[KEY_Q] = "Q",
	[KEY_W] = "W",
	[KEY_E] = "E",
	[KEY_R] = "R",
	[KEY_T] = "T",
	[KEY_Y] = "Y",
	[KEY_U] = "U",
	[KEY_I] = "I",
	[KEY_O] = "O",
	[KEY_P] = "P",
	[KEY_LEFTBRACE] = "{",
	[KEY_RIGHTBRACE] = "}",
	[KEY_BACKSLASH] = "|",
	[KEY_A] = "A",
	[KEY_S] = "S",
	[KEY_D] = "D",
	[KEY_F] = "F",
	[KEY_G] = "G",
	[KEY_H] = "H",
	[KEY_J] = "J",
	[KEY_K] = "K",
	[KEY_L] = "L",
	[KEY_SEMICOLON] = ":",
	[KEY_APOSTROPHE] = "\"",
	[KEY_Z] = "Z",
	[KEY_X] = "X",
	[KEY_C] = "C",
	[KEY_V] = "V",
	[KEY_B] = "B",
	[KEY_N] = "N",
	[KEY_M] = "M",
	[KEY_COMMA] = "<",
	[KEY_DOT] = ">",
	[KEY_SLASH] = "?",
};

void msm_vkeyb_plot(int x,int y,unsigned short c)
{
	int k;

	/* draw keyboard rotated if landscape*/
	if (vkeyb_landscape) {
		k = y;
		y = x;
		x = MSM_VKEYB_LCD_WIDTH - k - 1;
	}

	/* Draw the pixel */
	fb_helper_plot(NULL, x, y, c);
}

void msm_vkeyb_putc(unsigned char ch, int x, int y, unsigned short color)
{
	int i, j;

	for (i = 0; i < MSM_VKEYB_FONT_ROWS; i++) {
		for (j = 0; j < MSM_VKEYB_FONT_COLS; j++) {
			if(FONTDATA(ch, j, i)) msm_vkeyb_plot(x + j, y + i, color);
		}
	}
}

void msm_vkeyb_puts(unsigned char *str, int x, int y, unsigned short color)
{
	while(*str) {
		msm_vkeyb_putc(*str, x, y, color);
		x += MSM_VKEYB_FONT_COLS;
		str++;
	}
}

void msm_vkeyb_rect(int x, int y, int w, int h, unsigned short border, unsigned short fill)
{
	int i, j;

	for (i = y; i < y + h; i++) {
		for (j = x; j < x + w; j++) {
			if (i == y || j == x || i == y + h - 1 || j == x + w - 1) {
				msm_vkeyb_plot(j,i,border);
			} else if (fill != COLOR_TRANSPARENT) {
				msm_vkeyb_plot(j,i,fill);
			}
		}
	}
}

void msm_vkeyb_draw_keyboard(struct msmfb_update_area *area)
{
	static int vkeyb_landscape_prev = -1;
	int i, j, jj, key, x, y, w;
	char *label, tmp[2];
	unsigned int fg, bg;

	/* Determine if we should draw landscape or not. Currently this is only used for the Raphael. The keyboard slide GPIO is tested. */
	if (machine_is_htcraphael())
		vkeyb_landscape = gpio_get_value(38);
	else if (machine_is_htcraphael_cdma())
		vkeyb_landscape = gpio_get_value(39);
	else
		vkeyb_landscape = 0;
	
	/* When vkeyb_landscape was modified, we make this update a full-screen one */
	if (vkeyb_landscape != vkeyb_landscape_prev) {
		struct fb_info *fb = fb_helper_get_first_fb();
		area->x = 0;
		area->y = 0;
		area->width = fb->var.xres;
		area->height = fb->var.yres;
	}
	
	/* Draw the keyboard (if visible) */
	if (vkeyb_toggle) {
		y = MSM_VKEYB_TOP;
		for (i = 0; i < MSM_VKEYB_ROWS; i++) {
			x = MSM_VKEYB_LEFT;
			for (j = 0; j < MSM_VKEYB_COLS; j++) {
				key = vkeyb_keyboard[i][j];
				for (jj = j + 1; jj < MSM_VKEYB_COLS && vkeyb_keyboard[i][jj] == vkeyb_keyboard[i][jj - 1]; jj++) ;
				w = MSM_VKEYB_KEY_W * (jj - j);
				j = jj - 1;
				if (key == vkeyb_active_key) {
					/* Key under stylus */
					fg = COLOR_BLACK;
					bg = COLOR_RED;
				} else if (vkeyb_key_state[key]) {
					/* Sticky key that is pressed */
					fg = COLOR_WHITE;
					bg = COLOR_BLACK;
				} else if (vkeyb_key_sticky[key]) {
					/* Sticky key that is not pressed */
					fg = COLOR_BLACK;
					bg = COLOR_LIGHTGRAY;
				} else {
					/* Normal key */
					fg = COLOR_BLACK;
					bg = COLOR_WHITE;
				}
				label = SHIFTACTIVE() && vkeyb_key_shift_func[key] ? vkeyb_key_shift_func[key] : vkeyb_key_func[key];
				if (CAPSACTIVE() && strlen(label) == 1 && ISALPHA(label[0])) {
					tmp[0] = TOGGLECASE(label[0]);
					tmp[1] = '\0';
					label = tmp;
				}
				msm_vkeyb_rect(x, y, w + 1, MSM_VKEYB_KEY_H + 1, COLOR_GRAY, bg);
				msm_vkeyb_puts(label, x + (w - MSM_VKEYB_FONT_COLS * strlen(label)) / 2, y + (MSM_VKEYB_KEY_H - MSM_VKEYB_FONT_ROWS) / 2, fg);
				x += w;
			}
			y += MSM_VKEYB_KEY_H;
		}
	}

	/* Draw the drag and minimize/restore square */
	msm_vkeyb_rect(MSM_VKEYB_LEFT - MSM_VKEYB_DRAG, MSM_VKEYB_TOP, MSM_VKEYB_DRAG, MSM_VKEYB_DRAG, COLOR_BLACK, 
					vkeyb_drag_track ? COLOR_RED : vkeyb_toggle ? COLOR_GREEN : COLOR_BLUE);
}

void msm_vkeyb_issue_update(void)
{
	int x, y, width, height;
	
	/* Depending on whether the keyboard is fully visible or not, we update a large or small piece */
	if (vkeyb_toggle) {
		/* The keyboard is fully visible */
		x = MSM_VKEYB_LEFT - MSM_VKEYB_DRAG;
		y = MSM_VKEYB_TOP;
		width = MSM_VKEYB_LEFT + (MSM_VKEYB_KEY_W * MSM_VKEYB_COLS);
		height = MSM_VKEYB_TOP + (MSM_VKEYB_KEY_H * MSM_VKEYB_ROWS);
	}
	else {
		/* Only the dragging area is visible */
		x = MSM_VKEYB_LEFT - MSM_VKEYB_DRAG;
		y = MSM_VKEYB_TOP;
		width = MSM_VKEYB_DRAG;
		height = MSM_VKEYB_DRAG;
	}
	
	/* If neccesary, we rotate the coordinates to landscape */
	if (vkeyb_landscape) {
		// Rotate top left
		int temp = y;
		y = x;
		x = MSM_VKEYB_LCD_WIDTH - temp - 1;
		
		// We want the "new" top left
		x = x - (width - 1);
		
		// Now we turn width and heigh around
		temp = height;
		height = width;
		width = temp;
	}
	
	/* Perform the update */
	fb_helper_update(NULL, x, y, width, height);
}

int msm_vkeyb_handle_ts_event(int x, int y, int touched)
{
	int i, j, key;
	int res;

	/* map rotated coords to vkeyb */
	if (vkeyb_landscape) {
		key = y;
		y = MSM_VKEYB_LCD_WIDTH - x - 1;
		x = key;
	}

	res = 0;
	if (x >= MSM_VKEYB_LEFT && x < MSM_VKEYB_LEFT + MSM_VKEYB_KEY_W * MSM_VKEYB_COLS
			&& y >= MSM_VKEYB_TOP && y < MSM_VKEYB_TOP + MSM_VKEYB_KEY_H * MSM_VKEYB_ROWS
			&& vkeyb_toggle) {
		res = 1;
		j = (x - MSM_VKEYB_LEFT) / MSM_VKEYB_KEY_W;
		i = (y - MSM_VKEYB_TOP) / MSM_VKEYB_KEY_H;
		key = vkeyb_keyboard[i][j];
		if(touched) {
			vkeyb_active_key = key;
		} else {
			vkeyb_active_key = 0;
			if (vkeyb_key_sticky[key]) {
				vkeyb_key_state[key] = 1 - vkeyb_key_state[key];
				if (key == KEY_CAPSLOCK) {
					/* Caps lock is special, we generate key press then key release to toggle it for the system */
					input_event(msm_vkeyb_dev, EV_KEY, key, 1);
					input_sync(msm_vkeyb_dev);
					input_event(msm_vkeyb_dev, EV_KEY, key, 0);
					input_sync(msm_vkeyb_dev);
				} else {
					input_event(msm_vkeyb_dev, EV_KEY, key, vkeyb_key_state[key]);
					input_sync(msm_vkeyb_dev);
				}
			} else {
				input_event(msm_vkeyb_dev, EV_KEY, key, 1);
				input_sync(msm_vkeyb_dev);
				input_event(msm_vkeyb_dev, EV_KEY, key, 0);
				input_sync(msm_vkeyb_dev);
				for (i = 0; i < MSM_VKEYB_LAST_KEY; i++) {
					if (vkeyb_key_sticky[i] && vkeyb_key_auto_reset[i] && vkeyb_key_state[i]) {
						vkeyb_key_state[i] = 0;
						input_event(msm_vkeyb_dev, EV_KEY, i, 0);
						input_sync(msm_vkeyb_dev);
					}
				}
			}
		}
	} else {
		vkeyb_active_key = 0;
	}

	/* keyboard drag thing */
	if (touched < 0) {
		/* Just redraw the button */
	} else if (touched && vkeyb_drag_track) {
		/* move the keyboard around */
		if ((abs(x - vkeyb_drag_x) > 2) || (abs(y - vkeyb_drag_y) > 2))
			vkeyb_moved = 1;
		MSM_VKEYB_LEFT += x - vkeyb_drag_x;
		MSM_VKEYB_TOP += y - vkeyb_drag_y;

		i = vkeyb_landscape ? 248 : 88;
		j = vkeyb_landscape ? 380 : 540;
		if (MSM_VKEYB_LEFT < MSM_VKEYB_DRAG) MSM_VKEYB_LEFT = MSM_VKEYB_DRAG; else if (MSM_VKEYB_LEFT > i) MSM_VKEYB_LEFT = i;
		if (MSM_VKEYB_TOP < 0) MSM_VKEYB_TOP = 0; else if (MSM_VKEYB_TOP > j) MSM_VKEYB_TOP = j;
		res = 1;
	}
	if (x >= MSM_VKEYB_LEFT - MSM_VKEYB_DRAG && x < MSM_VKEYB_LEFT
		&& y >= MSM_VKEYB_TOP && y < MSM_VKEYB_TOP + MSM_VKEYB_DRAG) {
		if (touched) {
			vkeyb_drag_x = x;
			vkeyb_drag_y = y;
			if (!vkeyb_drag_track) {
				vkeyb_drag_track = 1;
				vkeyb_moved = 0;
			}
		} else {
			if (vkeyb_drag_track && !vkeyb_moved)
				vkeyb_toggle = !vkeyb_toggle;
			vkeyb_drag_track = 0;
		}
		res = 1;
	} else {
		vkeyb_drag_track = 0;
	}
	
	/* Issue the area of the keyboard to be redrawn, the callback will do the actual drawing */
	msm_vkeyb_issue_update();

	return res;
}

static int __init msmvkeyb_toggle_setup(char *str)
{
	if (!strcmp(str,"off"))
		vkeyb_toggle = -1;
	else if (!strcmp(str,"hide"))
		vkeyb_toggle = 0;
	else
		vkeyb_toggle = 1;
	return 0;
}
__setup("msmvkeyb_toggle=", msmvkeyb_toggle_setup);

static void msm_vkeyb_predma_callback(struct fb_info *fb, struct msmfb_update_area *area)
{
	/* Draw the keyboard on the framebuffer */
	msm_vkeyb_draw_keyboard(area);
}

static int __init msm_vkeyb_init(void)
{
	int err, i, j;

	printk(KERN_INFO "msm_vkeyb: init\n");

	/* In case vkeyb was disabled from kernel param, don't continue loading */
	if (vkeyb_toggle == -1) {
		printk(KERN_INFO "msm_vkeyb: virtual keyboard is disabled by cmdline, not loading\n");
		msm_ts_handler = NULL;
		return 0;
	}

	/* Setup the input device */
	msm_vkeyb_dev = input_allocate_device();
	if (!msm_vkeyb_dev)
		return -ENOMEM;

	msm_vkeyb_dev->evbit[0] = BIT_MASK(EV_KEY);

	msm_vkeyb_dev->name = "MSM on-screen virtual keyboard";
	msm_vkeyb_dev->phys = "msm_vkeyb/input0";

	for (i = 0; i < MSM_VKEYB_ROWS; i++) {
		for (j = 0; j < MSM_VKEYB_COLS; j++) {
			if (!j || vkeyb_keyboard[i][j] != vkeyb_keyboard[i][j - 1]) {
				input_set_capability(msm_vkeyb_dev, EV_KEY, vkeyb_keyboard[i][j]);
			}
		}
	}

        /* Register the msmfb pre-dma callback */
        if (msmfb_predma_register_callback(msm_vkeyb_predma_callback) != 0) {
                printk(KERN_ERR "msm_ts: unable to register pre-dma callback, vkeyb is unusable without it. Bailing out.\n");
		input_free_device(msm_vkeyb_dev);
		return -1;
        }

	/* Register the input device */
	err = input_register_device(msm_vkeyb_dev);
	if (err) {
		input_free_device(msm_vkeyb_dev);
		return err;
	}

	/* We're depending on input from the touchscreen driver */
	msm_ts_handler = msm_vkeyb_handle_ts_event;

	/* Done */
	printk(KERN_INFO "msm_vkeyb: loaded\n");

	return 0;
}

static void __exit msm_vkeyb_exit(void)
{
	msmfb_predma_unregister_callback(msm_vkeyb_predma_callback);
	msm_ts_handler = NULL;
	input_unregister_device(msm_vkeyb_dev);
}

module_init(msm_vkeyb_init);
module_exit(msm_vkeyb_exit);

MODULE_AUTHOR("Octavian Voicu, octavian.voicu@gmail.com");
MODULE_DESCRIPTION("MSM on-screen virtual keyboard driver");
MODULE_LICENSE("GPL");
