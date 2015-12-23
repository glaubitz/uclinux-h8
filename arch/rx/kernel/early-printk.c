/*
 * arch/rx/kernel/early-printk.c
 *
 *  Copyright (C) 2009 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>

#if defined(CONFIG_RX_GDB_SIM)
static void gdb_write(struct console *co, const char *ptr,
				 unsigned len)
{
	register const int fd __asm__("r1") = 1; /* stdout */
	register const char *_ptr __asm__("r2") = ptr;
	register const unsigned _len __asm__("r3") = len;
	register const unsigned syscall __asm__("r5") = 5; /* sys_write */
	__asm__("int #255"
		::"g"(fd),"g"(_ptr),"g"(_len),"g"(syscall));
}

static struct console gdb_console = {
	.name		= "gdb_console",
	.write		= gdb_write,
	.setup		= NULL,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};
#endif

static struct console *early_console = NULL;

static int __init setup_early_printk(char *buf)
{
	if (!buf)
		return 0;

#if defined(CONFIG_RX_GDB_SIM)
	if (!strncmp(buf, "gdb", 3))
		early_console = &gdb_console;
#endif
#if defined(CONFIG_EARLY_SCI_CONSOLE)
	if (!strncmp(buf, "serial", 6)) {
		early_console = &sci_console;
		sci_early_init(buf + 6);
	}
#endif
	if (!early_console)
		return 0;

	if (strstr(buf, "keep"))
		early_console->flags &= ~CON_BOOT;
	else
		early_console->flags |= CON_BOOT;
	register_console(early_console);

	return 0;
}
early_param("earlyprintk", setup_early_printk);
