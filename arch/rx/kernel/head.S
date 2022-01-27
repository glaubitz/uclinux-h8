/*
 * arch/rx/kernel/head.S
 *  Copyright (C) 2009 Yoshinori Sato
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
	

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/thread_info.h>

	.global	startup
	.section	.head.text,"ax"
startup:
	mov.l	r1,r0
	/* r1 is FDT header */
	mov	r1, r4
	/* clear BSS */
	mov	#_sbss,r1
	mov	#_ebss,r3
	sub	r1,r3
	shlr	#2,r3
	mov	#0,r2
	sstr.l
	/* exception handler setup */
	mov	#install_exception_entry,r1
	jsr	r1
	mov.l	#rx_fdt_init, r2
	mov	r4,r1
	jsr	r2
	/* start kernel */
	mov	#init_thread_union, r1
	add	#PAGE_SIZE, r1, r0
	mov	#start_kernel, r1
	jmp	r1

	.end
