/*
 *  fs/partitions/x68k.c
 */

#include <linux/ctype.h>
#include "check.h"
#include "x68k.h"

int x68k_partition(struct parsed_partitions *state)
{
	Sector sect;
	struct x68k_rootsector *rs;
	struct x68k_partition_info *pi;
	int slot;

	rs = read_part_sector(state, 2 * 2, &sect);
	if (!rs)
		return -1;

	if (memcmp(rs->head.sig, "X68K", 4)) {
		put_dev_sector(sect);
		return 0;
	}

	pi = &rs->partition[0];
	for (slot = 1; slot < 16; slot++, pi++) {
		if (!pi->start)
			continue;
		put_partition(state, slot,
			      (be32_to_cpu(pi->start) & 0xffffff) * 2,
			      (be32_to_cpu(pi->length) & 0xffffff) * 2);
	}
	strlcat(state->pp_buf, "\n", PAGE_SIZE);

	return 1;
}
