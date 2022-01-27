#define	LOAD_OFFSET 0
#include <asm-generic/vmlinux.lds.h>
#include <asm/page.h>
#include <asm/thread_info.h>

#define EXCEPTION_FIXED 0xffffff80

RAMTOP = CONFIG_RAMSTART;
RAMSIZE = CONFIG_RAMEND - CONFIG_RAMSTART;

#if defined(CONFIG_RX_GDB_SIM)
/*INPUT(romfs.o)*/
#endif

jiffies = jiffies_64;

ENTRY(startup)

SECTIONS
{
	. = RAMTOP;
	HEAD_TEXT_SECTION
	_text = .;
	_stext = . ;
        .text :
	{
	TEXT_TEXT
	SCHED_TEXT
	CPUIDLE_TEXT
	LOCK_TEXT
	_etext = . ;
	}
	EXCEPTION_TABLE(16)
	NOTES
	_sdata = .;
	RO_DATA(PAGE_SIZE)
	RW_DATA(0, PAGE_SIZE, THREAD_SIZE)
	_edata = .;

	. = ALIGN(PAGE_SIZE);		/* Init code and data */
	__init_begin = .;
	INIT_TEXT_SECTION(0)
	INIT_DATA_SECTION(16)
	.rx.exception : {
	ram_exception_handler = .;
	*(.rx.exception);
	}
	sizeof_ram_exception_handler = SIZEOF(.rx.exception);
	__init_end = .;
	_sbss = .;
	BSS_SECTION(0, PAGE_SIZE, 4)
	_ebss = .;		/* uClinux MTD sucks */
	_end = . ;
	_ramstart = .;
	. = RAMTOP + RAMSIZE;
	_ramend = .;
	DISCARDS
}
