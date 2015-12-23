#include <asm/ptrace.h>

	.section	.vector,"a"
no	=	0
	.rept	32
	.long	rx_exp_table + (no * 4)
no	=	no + 1	
	.endr

	.section	".rx.exception","ax"
rx_exp_table:	
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
	mov.l	#rx_exp_table,r14
	sub	[r14],r13
	shlr	#2,r13
	sub	#1,r13
	/* r13 is exception no */
	mov	#rx_exception_handler,r1
	jmp	r1
	.section	.commandline,"a"
	.space	1024
	
	.end
