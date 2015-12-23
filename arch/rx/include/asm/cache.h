#ifndef __ASM_RX_CACHE_H__
#define __ASM_RX_CACHE_H__

/* bytes per L1 cache line */
#define        L1_CACHE_SHIFT  2
#define        L1_CACHE_BYTES  (1 << L1_CACHE_SHIFT)

#define __cacheline_aligned
#define ____cacheline_aligned

#endif
