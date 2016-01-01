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
	mov.l	#1b,r14
	sub	[r14],r13
	shlr	#2,r13
	sub	#1,r13
	/* r13 is exception no */
	mov	#rx_exception_handler,r1
	jmp	r1


	.text
install_exception_entry:
	mvtc	#ram_exception_handler,extb
	rts

	.end
