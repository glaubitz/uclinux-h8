/*
 * arch/m68k/x68k/x68kints.c -- X68030 Linux interrupt handling code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/irq.h>

#include <asm/traps.h>
#include <asm/irq.h>


/* Setup X68030 interrupt */

void __init x68k_init_IRQ(void)
{
	m68k_setup_user_interrupt(VEC_USER, NR_IRQS - VEC_USER);
}

