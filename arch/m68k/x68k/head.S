/*
 * head.S - Common startup code for 68000 core based CPU's
 *
 * 2012.10.21, Luis Alves <ljalvs@gmail.com>, Single head.S file for all
 *             68000 core based CPU's. Based on the sources from:
 *             Coldfire by Greg Ungerer <gerg@snapgear.com>
 *             68328 by D. Jeff Dionne <jeff@ryeham.ee.ryerson.ca>,
 *                      Kenneth Albanowski <kjahds@kjahds.com>,
 *                      The Silver Hammer Group, Ltd.
 *
 */

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>


.global _start
.global _rambase
.global _ramvec
.global _ramstart
.global _ramend

.data

/*****************************************************************************
 * RAM setup pointers. Used by the kernel to determine RAM location and size.
 *****************************************************************************/

_rambase:
	.long	0
_ramvec:
	.long	0
_ramstart:
	.long	0
_ramend:
	.long	0

__HEAD

/*****************************************************************************
 * Entry point, where all begins!
 *****************************************************************************/

_start:
	movew	#0x2700, %sr			/* disable all interrupts */

	moveal	#_etext, %a0
	moveal	#_sdata, %a1
	moveal	#__bss_start, %a2
/*****************************************************************************
 * Setup basic memory information for kernel
 *****************************************************************************/
	movel	#CONFIG_RAMBASE,_rambase	/* set the base of RAM */
	movel	#0x400000, _ramend		/* set end ram addr */
	lea	__bss_stop,%a1
	movel	%a1,_ramstart

	lea	__bss_start, %a0		/* get start of bss */
	lea	__bss_stop, %a1			/* get end of bss */
_clear_bss:
	movel	#0, (%a0)+			/* clear each word */
	cmpl	%a0, %a1			/* check if at end */
	bne	_clear_bss

/*****************************************************************************
 * Load the current task pointer and stack.
 *****************************************************************************/
	lea	init_thread_union,%a0
	lea	THREAD_SIZE(%a0),%sp
	jsr	start_kernel			/* start Linux kernel */
_exit:
	jmp	_exit				/* should never get here */
