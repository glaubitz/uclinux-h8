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

;
; mulsi3 for H8/300H - based on Renesas SH implementation
;
; by Toshiyasu Morita
;
; Old code:
;
; 16b * 16b = 372 states (worst case)
; 32b * 32b = 724 states (worst case)
;
; New code:
;
; 16b * 16b =  48 states
; 16b * 32b =  72 states
; 32b * 32b =  92 states
;

	.global __mulsi3
__mulsi3:
	mov.w	r1,r2   ; ( 2 states) b * d
	mulxu	r0,er2  ; (22 states)

	mov.w	e0,r3   ; ( 2 states) a * d
	beq	L_skip1 ; ( 4 states)
	mulxu	r1,er3  ; (22 states)
	add.w	r3,e2   ; ( 2 states)

L_skip1:
	mov.w	e1,r3   ; ( 2 states) c * b
	beq	L_skip2 ; ( 4 states)
	mulxu	r0,er3  ; (22 states)
	add.w	r3,e2   ; ( 2 states)

L_skip2:
	mov.l	er2,er0	; ( 2 states)
	rts		; (10 states)
