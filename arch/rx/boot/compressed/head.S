/*
 *  linux/arch/rx/boot/compressed/head.S
 *
 *  Copyright (C) 2011 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

	.text

#include <asm/page.h>

	.global	startup
startup:
	mov.l	#initsp,r0
	mov.l	#_sbss,r1
	mov.l	#_ebss,r3
	sub	r1,r3
	mov.l	#0,r2
	sstr.b
	mov.l	#decompress_kernel, r1
	jsr	r1
	mov.l	#_text+PAGE_SIZE, r1
	jmp	r1
	
	.align	9
fake_headers_as_bzImage:
	.word	0
	.ascii	"HdrS"		! header signature
	.word	0x0202		! header version number (>= 0x0105)
				! or else old loadlin-1.5 will fail)
	.word	0		! default_switch
	.word	0		! SETUPSEG
	.word	0x1000
	.word	0		! pointing to kernel version string
	.byte	0		! = 0, old one (LILO, Loadlin,
				! 0xTV: T=0 for LILO
				!       V = version
	.byte	1		! Load flags bzImage=1
	.word	0x8000		! size to move, when setup is not
	.long	0x100000	! 0x100000 = default for big kernel
	.long	0		! address of loaded ramdisk image
	.long	0		# its size in bytes
