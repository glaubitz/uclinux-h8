#ifndef __H8300_TIMER_H
#define __H8300_TIMER_H

unsigned long get_cpu_clock(void);

#define H8300_TIMER_FREQ get_cpu_clock() /* Timer input freq. */

#define H8300_TMR8_CLKSRC    0
#define H8300_TMR8_CLKEVTDEV 1

#define H8300_TMR8_DIV_8 0
#define H8300_TMR8_DIV_64 1
#define H8300_TMR8_DIV_8192 2

struct h8300_timer8_config {
	int mode;
	int div;
	int rating;
};

struct h8300_timer16_config {
	int rating;
	__u8 enb;
	__u8 imfa;
	__u8 imiea;
};

struct h8300_tpu_config {
	int rating;
};
#endif
