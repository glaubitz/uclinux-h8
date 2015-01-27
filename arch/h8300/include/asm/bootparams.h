/*
H8/300 kernel boot parameters
*/

#ifndef __ASM_H8300_BOOTPARAMS__
#define __ASM_H8300_BOOTPARAMS__

struct bootparams {
	short size;
	unsigned char gpio_ddr[24];
	unsigned char gpio_use[24];
	unsigned int clock_freq;
	unsigned int ram_end;
	unsigned char *command_line;
} __packed __aligned(2);

#endif
