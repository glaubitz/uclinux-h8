	.global	memcpy
	.text
memcpy:
	mov	r1,r4
	smovf
	mov	r4,r1
	rts
