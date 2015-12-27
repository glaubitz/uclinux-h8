	.global	memset
	.text
memset:
	mov	r1,r4
	sstr
	mov	r4,r1
	rts
