/*
 *  linux/drivers/video/x68kfb.c -- X68000 frame buffer device
 *
 *  Copyright (C) 2016 Yoshinori Sato
 *
 *  Based on vfb.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <linux/fb.h>
#include <linux/init.h>
#include "core/fb_draw.h"

    /*
     *  RAM we reserve for the frame buffer. This defines the maximum screen
     *  size
     *
     *  The default can be overridden if the driver is compiled as a module
     */

#define VIDEOMEMSIZE	(2*1024*1024)	/* 2 MB */

static void *videomemory;
static u_long videomemorysize = VIDEOMEMSIZE;

static struct fb_var_screeninfo x68kfb_default = {
	.xres =		768,
	.yres =		512,
	.xres_virtual =	1024,
	.yres_virtual =	1024,
	.bits_per_pixel = 16,
	.red =		{ 6, 5, 0 },
      	.green =	{ 11, 5, 0 },
      	.blue =		{ 1, 5, 0 },
      	.activate =	FB_ACTIVATE_TEST,
      	.height =	-1,
      	.width =	-1,
      	.pixclock =	20000,
      	.left_margin =	64,
      	.right_margin =	64,
      	.upper_margin =	32,
      	.lower_margin =	32,
      	.hsync_len =	64,
      	.vsync_len =	2,
      	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo x68kfb_fix = {
	.id =		"X68k FB",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_PSEUDOCOLOR,
	.xpanstep =	1,
	.ypanstep =	1,
	.ywrapstep =	1,
	.accel =	FB_ACCEL_NONE,
};

static bool x68kfb_enable __initdata = 0;	/* disabled by default */
module_param(x68kfb_enable, bool, 0);

static int x68kfb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info);
static int x68kfb_set_par(struct fb_info *info);
static int x68kfb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info);
static int x68kfb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma);
static int x68kfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			    u_int transp, struct fb_info *info);
static void x68kfb_imageblit(struct fb_info *p, const struct fb_image *image);

static struct fb_ops x68kfb_ops = {
	.fb_check_var	= x68kfb_check_var,
	.fb_set_par	= x68kfb_set_par,
	.fb_pan_display	= x68kfb_pan_display,
	.fb_setcolreg	= x68kfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= x68kfb_imageblit,
	.fb_mmap	= x68kfb_mmap,
};

    /*
     *  Internal routines
     */

static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp / 8;
	return (length);
}

    /*
     *  Setting the video mode has been split into two parts.
     *  First part, xxxfb_check_var, must not write anything
     *  to hardware, it should only verify and adjust var.
     *  This means it doesn't alter par but it does use hardware
     *  data from it to check this var. 
     */

static int x68kfb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
	u_long line_length;

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
	    get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	/*
	 * Now that we checked it we alter var. The reason being is that the video
	 * mode passed in might not work but slight changes to it might make it 
	 * work. This way we let the user know what is acceptable.
	 */
	var->red.offset = 0;
	var->red.length = 1;
	var->green.offset = 0;
	var->green.length = 1;
	var->blue.offset = 0;
	var->blue.length = 1;
	var->transp.offset = 0;
	var->transp.length = 1;
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
static int x68kfb_set_par(struct fb_info *info)
{
	info->fix.line_length = get_line_length(info->var.xres_virtual,
						info->var.bits_per_pixel);
	return 0;
}

static int x68kfb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset >= info->var.yres_virtual ||
		    var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + info->var.xres > info->var.xres_virtual ||
		    var->yoffset + info->var.yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

    /*
     *  Most drivers don't need their own mmap function 
     */

static int x68kfb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	if (size > info->fix.smem_len)
		return -EINVAL;
	if (offset > info->fix.smem_len - size)
		return -EINVAL;

	pos = (unsigned long)info->fix.smem_start + offset;

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED)) {
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return 0;

}

#define PSEUDO_PALETTE_SIZE 16

static int x68kfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			      u_int transp, struct fb_info *info)
{
	u16 *pal = info->pseudo_palette;
	u32 cr = red >> (16 - info->var.red.length);
	u32 cg = green >> (16 - info->var.green.length);
	u32 cb = blue >> (16 - info->var.blue.length);
	u16 value;

	if (regno >= PSEUDO_PALETTE_SIZE)
		return -EINVAL;

	value = (cr << info->var.red.offset) |
		(cg << info->var.green.offset) |
		(cb << info->var.blue.offset);
	if (info->var.transp.length > 0) {
		u32 mask = (1 << info->var.transp.length) - 1;
		mask <<= info->var.transp.offset;
		value |= mask;
	}
	pal[regno] = value;

	return 0;
}

static const u32 x68kfb_tab16_be[] = {
    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff
};

#define FB_WRITEL fb_writel
#define FB_READL  fb_readl

static inline void color_imageblit(const struct fb_image *image, 
				   struct fb_info *p, u8 __iomem *dst1, 
				   u32 start_index,
				   u32 pitch_index)
{
	/* Draw the penguin */
	u32 __iomem *dst, *dst2;
	u32 color = 0, val, shift;
	int i, n, bpp = p->var.bits_per_pixel;
	u32 null_bits = 32 - bpp;
	u32 *palette = (u32 *) p->pseudo_palette;
	const u8 *src = image->data;
	u32 bswapmask = fb_compute_bswapmask(p);

	dst2 = (u32 __iomem *) dst1;
	for (i = image->height; i--; ) {
		n = image->width;
		dst = (u32 __iomem *) dst1;
		shift = 0;
		val = 0;
		
		if (start_index) {
			u32 start_mask = ~fb_shifted_pixels_mask_u32(p,
						start_index, bswapmask);
			val = FB_READL(dst) & start_mask;
			shift = start_index;
		}
		while (n--) {
			if (p->fix.visual == FB_VISUAL_TRUECOLOR ||
			    p->fix.visual == FB_VISUAL_DIRECTCOLOR )
				color = palette[*src];
			else
				color = *src;
			color <<= FB_LEFT_POS(p, bpp);
			val |= FB_SHIFT_HIGH(p, color, shift ^ bswapmask);
			if (shift >= null_bits) {
				FB_WRITEL(val, dst++);
	
				val = (shift == null_bits) ? 0 : 
					FB_SHIFT_LOW(p, color, 32 - shift);
			}
			shift += bpp;
			shift &= (32 - 1);
			src++;
		}
		if (shift) {
			u32 end_mask = fb_shifted_pixels_mask_u32(p, shift,
						bswapmask);

			FB_WRITEL((FB_READL(dst) & end_mask) | val, dst);
		}
		dst1 += p->fix.line_length;
		if (pitch_index) {
			dst2 += p->fix.line_length;
			dst1 = (u8 __iomem *)((long __force)dst2 & ~(sizeof(u32) - 1));

			start_index += pitch_index;
			start_index &= 32 - 1;
		}
	}
}

static inline void fast_imageblit(const struct fb_image *image, struct fb_info *p, 
				  u8 __iomem *dst1, u32 fgcolor, 
				  u32 bgcolor) 
{
	u32 fgx = fgcolor, bgx = bgcolor, bpp = p->var.bits_per_pixel;
	u32 ppw = 16/bpp, spitch = (image->width + 7)/8;
	u32 bit_mask, end_mask, eorx, shift;
	const char *s = image->data, *src;
	u16 __iomem *dst;
	const u32 *tab = NULL;
	int i, j, k;

	tab = x68kfb_tab16_be;

	for (i = ppw-1; i--; ) {
		fgx <<= bpp;
		bgx <<= bpp;
		fgx |= fgcolor;
		bgx |= bgcolor;
	}
	
	bit_mask = (1 << ppw) - 1;
	eorx = fgx ^ bgx;
	k = image->width/ppw;

	for (i = image->height; i--; ) {
		dst = (u16 __iomem *) dst1, shift = 8; src = s;
		
		for (j = k; j--; ) {
			shift -= ppw;
			end_mask = tab[(*src >> shift) & bit_mask];
			__raw_writew((end_mask & eorx)^bgx, dst++);
			if (!shift) { shift = 8; src++; }		
		}
		dst1 += p->fix.line_length;
		s += spitch;
	}
}	

static void x68kfb_imageblit(struct fb_info *p, const struct fb_image *image)
{
	u32 fgcolor, bgcolor, start_index, bitstart, pitch_index = 0;
	u32 bpl = sizeof(u32), bpp = p->var.bits_per_pixel;
	u32 dx = image->dx, dy = image->dy;
	u8 __iomem *dst1;

	if (p->state != FBINFO_STATE_RUNNING)
		return;

	bitstart = (dy * p->fix.line_length * 8) + (dx * bpp);
	start_index = bitstart & (32 - 1);
	pitch_index = (p->fix.line_length & (bpl - 1)) * 8;

	bitstart /= 8;
	bitstart &= ~(bpl - 1);
	dst1 = p->screen_base + bitstart;

	if (p->fbops->fb_sync)
		p->fbops->fb_sync(p);

	if (image->depth == 1) {
		if (p->fix.visual == FB_VISUAL_TRUECOLOR ||
		    p->fix.visual == FB_VISUAL_DIRECTCOLOR) {
			fgcolor = ((u32*)(p->pseudo_palette))[image->fg_color];
			bgcolor = ((u32*)(p->pseudo_palette))[image->bg_color];
		} else {
			fgcolor = image->fg_color;
			bgcolor = image->bg_color;
		}	
		
		fast_imageblit(image, p, dst1, fgcolor, bgcolor);
	} else
		color_imageblit(image, p, dst1, start_index, pitch_index);
}

#ifndef MODULE
/*
 * The virtual framebuffer driver is only enabled if explicitly
 * requested by passing 'video=vfb:' (or any actual options).
 */
static int __init x68kfb_setup(char *options)
{
	char *this_opt;

	x68kfb_enable = 0;

	if (!options)
		return 1;

	x68kfb_enable = 1;

	if (!*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		/* Test disable for backwards compatibility */
		if (!strcmp(this_opt, "disable"))
			x68kfb_enable = 0;
	}
	return 1;
}
#endif  /*  MODULE  */

    /*
     *  Initialisation
     */

static int x68kfb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	int retval = -ENOMEM;

	/*
	 * For real video cards we use ioremap.
	 */
	if (!(videomemory = ioremap(0xc00000, 0x200000)))
		return retval;

	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		goto err;

	info->screen_base = (char __iomem *)videomemory;
	info->fbops = &x68kfb_ops;

	info->var = x68kfb_default;
	x68kfb_fix.smem_start = (unsigned long) videomemory;
	x68kfb_fix.smem_len = videomemorysize;
	info->fix = x68kfb_fix;
	info->pseudo_palette = info->par;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->pseudo_palette = (void *)0xffe82000;;

	retval = fb_alloc_cmap(&info->cmap, 256, 0);
	if (retval < 0)
		goto err1;

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err2;
	platform_set_drvdata(dev, info);

	fb_info(info, "X68000 frame buffer device, using %ldK of video memory\n",
		videomemorysize >> 10);
	return 0;
err2:
	fb_dealloc_cmap(&info->cmap);
err1:
	framebuffer_release(info);
err:
	iounmap(videomemory);
	return retval;
}

static int x68kfb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		unregister_framebuffer(info);
		iounmap(videomemory);
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver x68kfb_driver = {
	.probe	= x68kfb_probe,
	.remove = x68kfb_remove,
	.driver = {
		.name	= "X68kfb",
	},
};

static struct platform_device *x68kfb_device;

static int __init x68kfb_init(void)
{
	int ret = 0;

#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("x68kfb", &option))
		return -ENODEV;
	x68kfb_setup(option);
#endif

	if (!x68kfb_enable)
		return -ENXIO;

	ret = platform_driver_register(&x68kfb_driver);

	if (!ret) {
		x68kfb_device = platform_device_alloc("X68kfb", 0);
		if (x68kfb_device)
			ret = platform_device_add(x68kfb_device);
		else
			ret = -ENOMEM;

		if (ret) {
			platform_device_put(x68kfb_device);
			platform_driver_unregister(&x68kfb_driver);
		}
	}

	return ret;
}

module_init(x68kfb_init);

#ifdef MODULE
static void __exit x68kfb_exit(void)
{
	platform_device_unregister(x68kfb_device);
	platform_driver_unregister(&x68kfb_driver);
}

module_exit(x68kfb_exit);

MODULE_LICENSE("GPL");
#endif				/* MODULE */
