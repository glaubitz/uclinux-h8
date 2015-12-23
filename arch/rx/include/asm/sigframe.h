#ifndef __ASM_RX_SIGCONTEXT_H__
#define __ASM_RX_SIGCONTEXT_H__

struct sigcontext {
	unsigned long  sc_mask; 	/* old sigmask */
	unsigned long  sc_usp;		/* old user stack pointer */
	unsigned long  sc_r[16];
	unsigned long  sc_psw;
	unsigned long  sc_pc;
};

#endif
