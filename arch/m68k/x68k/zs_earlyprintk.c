#include <linux/console.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#if !defined(CONFIG_MMU)
#define CMD ((void *)0xe98005)
#define DATA ((void *)0xe98007)
#else
#define CMD ((void *)0xffe98005)
#define DATA ((void *)0xffe98007)
#endif

static void zs_write(struct console *co, const char *ptr,
				 unsigned len)
{
	for(;len>0;len--) {
		while(! (__raw_readb(CMD) & 4));
		if (*ptr == '\n') {
			__raw_writeb('\r', DATA);
			while(! (__raw_readb(CMD) & 4));
		}
		__raw_writeb(*ptr++, DATA);
	}
}

static struct console zs_console = {
	.name		= "zs_console",
	.write		= zs_write,
	.setup		= NULL,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

static char zs_console_buf[32];

static int zs_probe(struct platform_device *pdev)
{
	if (zs_console.data)
		return -EEXIST;

	if (!strstr(zs_console_buf, "keep"))
		zs_console.flags |= CON_BOOT;

	__raw_writeb(9, CMD);
	__raw_writeb(0, CMD);
	register_console(&zs_console);
	return 0;
}

static int zs_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver zs_driver = {
	.probe		= zs_probe,
	.remove		= zs_remove,
	.driver		= {
		.name	= "X68000-ZS",
		.owner	= THIS_MODULE,
	},
};

early_platform_init_buffer("earlyprintk", &zs_driver,
			   zs_console_buf, ARRAY_SIZE(zs_console_buf));

static struct platform_device zs_console_device = {
	.name		= "X68000-ZS",
	.id		= 0,
};

static struct platform_device *devices[] __initdata = {
	&zs_console_device,
};

void __init zs_console_register(void)
{
	early_platform_add_devices(devices,
				   ARRAY_SIZE(devices));
}
