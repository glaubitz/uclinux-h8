/*
 * arch/rx/kernel/sim-console.c
 *
 *  Copyright (C) 2015 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/serial_core.h>

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

static int __init sim_setup(struct earlycon_device *device, const char *opt)
{
	device->con->write = gdb_write;
	return 0;
}

EARLYCON_DECLARE(rxsim, sim_setup);
