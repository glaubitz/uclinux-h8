#include <asm-generic/vmlinux.lds.h>
#include <asm/page.h>

#define ROMTOP 0x000000
#define RAMTOP CONFIG_RAMBASE

jiffies = jiffies_64 + 4;

ENTRY(_start)

SECTIONS
{
#if defined(CONFIG_ROMKERNEL)
	. = ROMTOP;
	.vectors :
	{
	_vector = . ;
		*(.vector*)
	}
#else
	. = RAMTOP;
	_ramstart = .;
	. = . + CONFIG_OFFSET;
#endif
	_text = .;
	HEAD_TEXT_SECTION
	.text : {
	_stext = . ;
		TEXT_TEXT
		SCHED_TEXT
		LOCK_TEXT
#if defined(CONFIG_ROMKERNEL)
		*(.int_redirect)
#endif
	_etext = . ;
	}
	EXCEPTION_TABLE(16)
	NOTES
	RO_DATA(4)
#if defined(CONFIG_ROMKERNEL)
	.init.text : {
		_sinittext = .;
		INIT_TEXT
		_einittext = .;
	}
	SECURITY_INIT
#endif
	ROMEND = .;
#if defined(CONFIG_ROMKERNEL)
	. = RAMTOP;
	_ramstart = .;
	.data :	AT(ROMEND)
#else
	.data :
#endif
	{
	_sdata = . ;
	__data_start = . ;
		INIT_TASK_DATA(0x2000)
		NOSAVE_DATA
		PAGE_ALIGNED_DATA(0x1000)
		CACHELINE_ALIGNED_DATA(0x0002)
		READ_MOSTLY_DATA(0x0002)
		DATA_DATA
		CONSTRUCTORS
	}
	. = ALIGN(0x4) ;
	__init_begin = .;
#if defined(CONFIG_RAMKERNEL)
	INIT_TEXT_SECTION(4)
#endif
	INIT_DATA_SECTION(4)
#if defined(CONFIG_RAMKERNEL)
	SECURITY_INIT
#endif
	__init_end = .;
	_edata = . ;
	_begin_data = LOADADDR(.data);
	_sbss =.;
	BSS_SECTION(4,4,4)
	_ebss =.;
	_ramend = .;
	_end = .;
	DISCARDS
}
