#define	LOAD_OFFSET 0
#include <asm-generic/vmlinux.lds.h>
#include <asm/page.h>
#include <asm/thread_info.h>

#define EXCEPTION_FIXED 0xffffff80

#if defined(CONFIG_ROMKERNEL)
ROMTOP = CONFIG_ROMSTART;
#endif
RAMTOP = CONFIG_RAMSTART;
RAMSIZE = CONFIG_RAMEND - CONFIG_RAMSTART;

#if defined(CONFIG_RX_GDB_SIM)
/*INPUT(romfs.o)*/
#endif

jiffies = jiffies_64;

ENTRY(startup)

SECTIONS
{
#if defined(CONFIG_ROMKERNEL)
	. = ROMTOP;
	HEAD_TEXT_SECTION
	_text = .;
	_stext = . ;
#else
	. = RAMTOP;
	HEAD_TEXT_SECTION
	_text = .;
	_stext = . ;
#endif
        .text :
	{
	TEXT_TEXT
	SCHED_TEXT
	LOCK_TEXT
	_etext = . ;
	}
	EXCEPTION_TABLE(16)
	NOTES
#if defined(CONFIG_ROMKERNEL)
	INIT_TEXT_SECTION(PAGE_SIZE)
	ROEND = .; 
	__data_romtop = .;
	. = RAMTOP;
	_sdata = .;
	RODATA
	RW_DATA_SECTION(0, PAGE_SIZE, THREAD_SIZE)
	_edata = .;
#else
	_sdata = .;
	RODATA
	RW_DATA_SECTION(0, PAGE_SIZE, THREAD_SIZE)
	_edata = .;
#endif

	. = ALIGN(PAGE_SIZE);		/* Init code and data */
#if defined(CONFIG_RAMKERNEL)
	__init_begin = .;
	INIT_TEXT_SECTION(PAGE_SIZE)
#else
	__init_begin = .;
#endif
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
        .romfs :	
	{
		*(.romfs*)
	}
	. = RAMTOP + RAMSIZE;
	_ramend = .;
#if defined(CONFIG_ROMKERNEL)
	. = EXCEPTION_FIXED;
	.vector :
	{
		*(.vector*)
	}
#endif
	DISCARDS
}
