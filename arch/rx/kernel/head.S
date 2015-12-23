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
#include <asm/vmlinux.lds.h>

	.global	startup
	.section	.head.text,"ax"
startup:
	mov.l	#CONFIG_INITIAL_STACK,r0
#if !defined(CONFIG_RX_GDB_SIM)
#if defined(CONFIG_ROMKERNEL)
	mov.l	#setup_bsc,r1
	jsr	r1

	/* copy rw data for ram */
	mov.l	#__data_romtop,r2
	mov.l	#_sdata,r1
	mov.l	#_edata,r3
	sub	r1,r3
	smovf
#endif
#else
	/* setup args */
	mov.l	#12, r5
	int	#255
	mov.l	r1,r2
	mov.l	#0x400,r1
	mov.l	#13, r5
	int	#255
	/* r1 - argc */
	/* r2 -length */
	/* skip argv[0] */
	mov.l	r1,r4
	mov.l	r2,r3
	mov.l	#0x400,r1
	mov.l	#0,r2
	suntil.b
	mov.l	r3,r5
	mov.l	r1,r2
	mov.l	#0x400,r1
	smovf
	mov.l	#0x400,r1
	mov.l	#0,r2
	sub	#1,r4
	mov.l	r5,r3
1:	
	suntil.b
	sub	#1,r1
	mov.b	#' ',[r1]
	add	#1,r1
	sub	#1,r4
	bne	1b
#endif
	/* clear BSS */
	mov.l	#_sbss,r1
	mov.l	#_ebss,r3
	sub	r1,r3
	shlr	#2,r3
	mov.l	#0,r2
	sstr.l
	/* exception handler setup */
#if defined(CONFIG_RAMKERNEL)
	mov.l	#install_exception_entry,r1
	jsr	r1
#endif
	/* start kernel */
	mov.l	#init_thread_union, r1
	add	#PAGE_SIZE, r1, r0
	mov.l	#start_kernel, r1
	jmp	r1

	.weak	setup_bsc
setup_bsc:	
	rts

	.end
