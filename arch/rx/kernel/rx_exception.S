/*
 * arch/rx/board/tkdn-rx62n/exception.S
 *  Copyright (C) 2011 Yoshinori Sato
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <asm/ptrace.h>
	
	.global	install_exception_entry
	.section	".rx.exception","ax"
	.rept	32
1:	
	bsr.a	2f
	.endr
2:	
	sub	#4,r0
	pushm	r1-r15
	mov.l	r0,r13
	add	#-(19*4),r13
	mov.l	r13,[-r0]
	mvfc	usp,r13
	mov.l	r13,OFF_USP[r0]
	mov.l	OFF_VEC[r0],r13
	mov.l	#exception_top,r14
	sub	[r14],r13
	shlr	#2,r13
	sub	#1,r13
	/* r13 is exception no */
	mov	#rx_exception_handler,r1
	jmp	r1


	.text
install_exception_entry:
	mov.l	#0xfffffff8,r1	/* NMI entry */
	mov.l	[r1],r1
	sub	#31*4,r1
	mov.l	#exception_top,r2
	mov.l	r1,[r2]
	mov.l	#ram_exception_handler,r2
	mov.l	#sizeof_ram_exception_handler,r3
	smovf
	rts

	.bss
exception_top:
	.space	4

	.end
