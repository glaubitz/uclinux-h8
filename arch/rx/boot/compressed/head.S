/*
 *  linux/arch/rx/boot/compressed/head.S
 *
 *  Copyright (C) 2011 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

	.text

#include <asm/page.h>

	.global	startup
startup:
	mov	#__stext,r0
	mov	r1,r6
	mov	#__sbss,r1
	mov	#__ebss,r3
	sub	r1,r3
	mov	#0,r2
	sstr.b
	mov	#decompress_kernel, r1
	jsr	r1
	mov	#output, r2
	mov	r6,r1
	jmp	r2
	
	.align	1 << 9
fake_headers_as_bzImage:
	.byte	0, 0
	.ascii	"HdrS"		;  header signature 
	.word	0x0202		; header version number (>= 0x0105)
				; or else old loadlin-1.5 will fail)
	.word	0		; default_switch
	.word	0		; SETUPSEG
	.word	0x1000
	.word	0		; pointing to kernel version string
	.byte	0		; = 0, old one (LILO, Loadlin,
				; 0xTV: T=0 for LILO
				;       V = version
	.byte	1		; Load flags bzImage=1
	.word	0x8000		; size to move, when setup is not
	.long	0x100000	; 0x100000 = default for big kernel
	.long	0		; address of loaded ramdisk image
	.long	0		; its size in bytes 
