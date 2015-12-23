#ifndef __ASM_RX_ELF_H__
#define __ASM_RX_ELF_H__

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#include <asm/user.h>

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];
typedef unsigned long elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_RX)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_RX		173
#define ELF_ARCH	EM_RX

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         0xD0000000UL

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.  */

#define ELF_PLATFORM  (NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#define R_RX_NONE	0
#define R_RX_DIR32	1
#define R_RX_DIR24S	2
#define R_RX_DIR16	3
#define R_RX_DIR16U	4
#define R_RX_DIR16S	5
#define R_RX_DIR8	6
#define R_RX_DIR8U	7
#define R_RX_DIR8S	8
#define R_RX_DIR24S_PCREL	9
#define R_RX_DIR16S_PCREL	10
#define R_RX_DIR8S_PCREL	11
#define R_RX_DIR16UL	12
#define R_RX_DIR16UW	13
#define R_RX_DIR8UL	14
#define R_RX_DIR8UW	15
#define R_RX_DIR32_REV	16
#define R_RX_DIR16_REV	17
#define R_RX_DIR3U_PCREL	18
#define R_RX_RH_3_PCREL	32
#define R_RX_RH_16_OP	33
#define R_RX_RH_24_OP	34
#define R_RX_RH_32_OP	35
#define R_RX_RH_24_UNS	36
#define R_RX_RH_8_NEG	37
#define R_RX_RH_16_NEG	38
#define R_RX_RH_24_NEG	39
#define R_RX_RH_32_NEG	40
#define R_RX_RH_DIFF	41
#define R_RX_RH_GPRELB	42
#define R_RX_RH_GPRELW	43
#define R_RX_RH_GPRELL	44
#define R_RX_RH_RELAX	45
#define R_RX_ABS32	65
#define R_RX_ABS24S	66
#define R_RX_ABS16	67
#define R_RX_ABS16U	68
#define R_RX_ABS16S	69
#define R_RX_ABS8	70
#define R_RX_ABS8U	71
#define R_RX_ABS8S	72
#define R_RX_ABS24S_PCREL	73
#define R_RX_ABS16S_PCREL	74
#define R_RX_ABS8S_PCREL	75
#define R_RX_ABS16UL	76
#define R_RX_ABS16UW	77
#define R_RX_ABS8UL	78
#define R_RX_ABS8UW	79
#define R_RX_ABS32_REV	80
#define R_RX_ABS16_REV	81
#define R_RX_SYM	128
#define R_RX_OPneg	129
#define R_RX_OPadd	130
#define R_RX_OPsub	131
#define R_RX_OPmul	132
#define R_RX_OPdiv	133
#define R_RX_OPshla	134
#define R_RX_OPshra	135
#define R_RX_OPsctsize	136
#define R_RX_OPscttop	141
#define R_RX_OPand	144
#define R_RX_OPor	145
#define R_RX_OPxor	146
#define R_RX_OPnot	147
#define R_RX_OPmod	148
#define R_RX_OPromtop	149
#define R_RX_OPramtop	150

#endif
