;; libgcc routines for the Renesas H8/300 CPU.
;; Contributed by Steve Chamberlain <sac@cygnus.com>
;; Optimizations by Toshiyasu Morita <toshiyasu.morita@renesas.com>

/* Copyright (C) 1994, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

This file is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

This file is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#include "libgcc.h"

; numerator in A0/A1
; denominator in A2/A3
	.global	__modsi3
__modsi3:
	PUSHP	S2P
	bsr	modnorm
	bsr	__divsi3
	mov.l	er3,er0
	bra	exitdiv

	;; H8/300H and H8S version of ___udivsi3 is defined later in
	;; the file.

	.global	__umodsi3
__umodsi3:
	bsr	__udivsi3:16
	mov.l	er3,er0
	rts

	.global	__divsi3
__divsi3:
	PUSHP	S2P
	bsr	divnorm
	bsr	__udivsi3:16

	; examine what the sign should be
exitdiv:
	btst	#3,S2L
	beq	reti

	; should be -ve
	neg.l	A0P

reti:
	POPP	S2P
	rts

divnorm:
	mov.l	A0P,A0P		; is the numerator -ve
	stc	ccr,S2L		; keep the sign in bit 3 of S2L
	bge	postive

	neg.l	A0P		; negate arg

postive:
	mov.l	A1P,A1P		; is the denominator -ve
	bge	postive2

	neg.l	A1P		; negate arg
	xor.b	#0x08,S2L	; toggle the result sign

postive2:
	rts

;; Basically the same, except that the sign of the divisor determines
;; the sign.
modnorm:
	mov.l	A0P,A0P		; is the numerator -ve
	stc	ccr,S2L		; keep the sign in bit 3 of S2L
	bge	mpostive

	neg.l	A0P		; negate arg

mpostive:
	mov.l	A1P,A1P		; is the denominator -ve
	bge	mpostive2

	neg.l	A1P		; negate arg

mpostive2:
	rts

	.end
