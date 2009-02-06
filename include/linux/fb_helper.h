/*
 * include/linux/fb_helper.h
 *
 * Author: Martijn Stolk <linuxtogo@netripper.nl>
 *
 * Helper functions for pixel-based plotting on the framebuffer. Allows to plot multiple pixels
 * and perform a single update on framebuffer devices that do not automatically read the
 * framebuffer memory.
 *
 * Note that these helpers only work for 16 bit framebuffers.
 */

#ifndef _LINUX_FB_HELPER_H
#define _LINUX_FB_HELPER_H

#include <linux/fb.h>

static inline struct fb_info* fb_helper_get_first_fb(void)
{
	if (num_registered_fb > 0) {
		return registered_fb[0];
	}
	return NULL;
}

static inline void fb_helper_plot(struct fb_info *fb, int x, int y, unsigned short color)
{
	unsigned short *pos;

	/* Make sure we have a framebuffer to work with */
	if (fb == NULL) {
		fb = fb_helper_get_first_fb();
		if (fb == NULL) {
			return;
		}
	}

	/* Validate the position */
	if (x < 0 || y < 0 || x > fb->var.xres || y > fb->var.yres) {
		printk(KERN_INFO "fb_helper: invalid position\n");
		return;
	}

	/* Get position in memory on which to plot */
	pos = ((unsigned short*)fb->screen_base) + fb->var.xres * y + x;

	/* Update the position */
	*pos = color;
	pos += fb->var.yres * fb->var.xres;
	*pos = color;
}

static inline void fb_helper_update(struct fb_info *fb, int x, int y, int width, int height)
{
	struct fb_copyarea fbcpa;

	/* Make sure we have a framebuffer to work with */
	if (fb == NULL) {
		fb = fb_helper_get_first_fb();
		if (fb == NULL) {
			return;
		}
	}

	/* Make sure the fb has operations defined */
	if (!fb->fbops) {
		return;
	}

	/* Make sure the fb supports the fb_copyarea() method */
	if (!fb->fbops->fb_copyarea) {
		return;
	}

	/* Fill in our copyarea */
	fbcpa.dx = x;
	fbcpa.dy = y;
	fbcpa.sx = x;
	fbcpa.sy = y;
	fbcpa.width = width;
	fbcpa.height = height;

	/* Call the framebuffer operation to copy a bit of the area to itself, forcing an update of that area */
	fb->fbops->fb_copyarea(fb, &fbcpa);
}

#endif /* _LINUX_FB_HELPER_H */
