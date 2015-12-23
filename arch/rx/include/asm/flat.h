#ifndef __ASM_RX_FLAT_H__
#define __ASM_RX_FLAT_H__

#define	flat_argvp_envp_on_stack()		0
#define	flat_old_ram_flag(flags)		1
#define	flat_reloc_valid(reloc, size)		((reloc) <= (size))
#define	flat_set_persistent(relval, p)		0

#define	flat_get_relocate_addr(rel)		(rel)
#define flat_get_addr_from_rp(rp, relval, flags, persistent) \
        get_unaligned(rp)
#define flat_put_addr_at_rp(rp, addr, rel) \
	put_unaligned (addr, rp)

#endif /* __ASM_RX_FLAT_H__ */
