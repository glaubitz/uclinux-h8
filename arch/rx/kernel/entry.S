/*
 * arch/rx/kernel/entry.S
 *
 * The RX exception entry
 *
 * Copyright (C) 2009,2010 Yoshinori Sato
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <asm/page.h>
#include <asm/ptrace.h>
	
#if !defined(CONFIG_PREEMPT)
#define resume_kernel restore_all
#endif

#define MIN_IRQ 20
#define MAX_EXCEPTION (32 + 20)
#define SYSVEC 8
	
.global	rx_int_table
.global ret_from_fork
.global rx_exception_handler
	
	.section	.text
	.align	4
rx_int_table:	
	.rept	256
	bsr.a	interrupt_entry
	.endr

interrupt_entry:
	sub	#4,r0
	pushm	r1-r15
	mov.l	r0,r13
	add	#-(19*4),r13
	mov.l	r13,[-r0]
	mvfc	usp,r13
	mov.l	r13,OFF_USP[r0]
	mov.l	OFF_VEC[r0],r13
	mov.l	#rx_int_table,r14
	sub	r14,r13
	shlr	#2,r13
	sub	#1,r13
	mov.l	r13,OFF_VEC[r0]
	cmp	#MIN_IRQ,r13
	ble	1f
	mvfc	psw,r1
	and	#~0x07000000,r1		; enable nested interrupt
	mvtc	r1,psw
	mov.l	r13,r1
	mov.l	r0,r2
	bsr	do_IRQ
	bra	ret_from_interrupt
1:
	add	#32,r13

rx_exception_handler:
	cmp	#32+SYSVEC,r13
	beq	syscall_entry
	cmp	#MAX_EXCEPTION,r13
	bge	1f
	add	#0x1000,r13,r14
	mov.l	r13,OFF_VEC[r0]
	mov.l	r0,r1
	shll	#2,r13
	add	#exception_table,r13
	mov.l	[r13],r13
	tst	r13,r13
	beq	1f
	jsr	r13
	bra	ret_from_exception
1:
	mov.l	r0,r1
	mov.l	r13,r2
	bsr	unhandled_exception
ret_from_exception:	
#if defined(CONFIG_PREEMPT)
	clrpsw	i
#endif
ret_from_interrupt:	
	mov.l	OFF_PSW[r0],r1
	btst	#20,r1
	beq	resume_kernel
resume_userspace:
	clrpsw	i
	mov.l	#~(THREAD_SIZE - 1),r13
	and	r0, r13
	mov.l	TI_FLAGS[r13],r14
	tst	#_TIF_WORK_MASK,r14
	beq	restore_all
work_pending:
	btst	#TIF_NEED_RESCHED,r14
	bne	work_resched
	mov.l	r0,r1			; pt_regs
	mov	r15,r3
	mov.l	r14,r2
	bsr	do_notify_resume	; do_notify_resume(pt_regs *, ti_flags, saved_r1)
	bra	resume_userspace
work_resched:
	bsr	schedule
	clrpsw	i
	mov	TI_FLAGS[r13],r14
	tst	#_TIF_WORK_MASK,r14
	bne	work_pending
restore_all:
	mov.l	OFF_USP[r0],r1
	mvtc	r1,usp
	add	#4,r0
	popm	r1-r15
	add	#8,r0
	rte
#if defined(CONFIG_PREEMPT)
resume_kernel:
	clrpsw	i
	mov.l	#~(THREAD_SIZE - 1),r13
	and	r0,r13
	mov.l	TI_PRECOUNT[r13],r14
	tst	r14,r14
	bne	restore_all
1:	
	mov.l	TI_FLAGS[r13],r14
	btst	#TIF_NEED_RESCHED,r14
	beq	restore_all
	mov.l	OFF_PSW[r0],r2
	btst	#16,r2		; I flag check
	beq	restore_all	; intterupt disable (exception cause)
	bsr	preempt_schedule_irq
	bra	1b
#endif
syscall_entry:
	setpsw	i
	mov.l	#~(THREAD_SIZE - 1),r13
	and	r0,r13
	mov.l	TI_FLAGS[r13],r14
	tst	#_TIF_WORK_SYSCALL_MASK,r14
	bnz	4f
1:	
	cmp	#NR_syscalls,r15
	blt	2f
	mov.l	#-ENOSYS,r1	; invalid no
	bra	3f
2:	
	shll	#2,r15
	add	#syscall_table,r15
	mov.l	[r15],r15
	mov.l	r7,[-r0]
	mov.l	r5,[-r0]
	jsr	r15
	add	#8,r0
3:	
	mov.l	OFF_R1[r0],r15	; r15 = saved r1
	mov.l	r1,OFF_R1[r0]
	clrpsw	i
	mov.l	#~(THREAD_SIZE - 1),r13
	and	r0,r13
	mov.l	TI_FLAGS[r13],r14
	tst	#_TIF_ALLWORK_MASK,r14
	bne	5f
	bra	restore_all
4:
	;; syscall trace enter
	mov.l	r0,r1
	bsr	syscall_trace_enter ; syscall_trace_enter(pt_regs *)
	mov.l	OFF_R1[r0],r1
	mov.l	OFF_R2[r0],r2
	mov.l	OFF_R3[r0],r3
	mov.l	OFF_R4[r0],r4
	mov.l	OFF_R5[r0],r5
	bra	1b
5:
	;; syscall trace leave
	btst	#TIF_SYSCALL_TRACE,r14
	beq	work_pending
	setpsw	i
	mov.l	r0,r1
	bsr	syscall_trace_leave ; syscall_trace_leave(pt_regs *)
	bra	resume_userspace

ret_from_fork:	
	bsr	schedule_tail
	bra	ret_from_exception

	.end
