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
	mov.l	#CONFIG_INITIAL_STACK,r0
#if defined(CONFIG_ROMKERNEL)
	mov.l	#setup_bsc,r1
	jsr	r1

	/* copy rw data to ram */
	mov.l	#__data_romtop,r2
	mov.l	#_sdata,r1
	mov.l	#_edata,r3
	sub	r1,r3
	smovf
	sub	r4,r4
#else
	mov	r1,r4
#endif
	/* clear BSS */
	mov	#_sbss,r1
	mov	#_ebss,r3
	sub	r1,r3
	shlr	#2,r3
	mov	#0,r2
	sstr.l
	mov	r4,r1
	mov	#rx_fdt_init,r2
	jsr	r2
	/* exception handler setup */
#if defined(CONFIG_RAMKERNEL)
	mov	#install_exception_entry,r1
	jsr	r1
#endif
	/* start kernel */
	mov	#init_thread_union, r1
	add	#PAGE_SIZE, r1, r0
	mov	#start_kernel, r1
	jmp	r1

	.weak	setup_bsc
setup_bsc:	
	rts

	.end
