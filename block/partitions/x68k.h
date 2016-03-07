/*
 *  fs/partitions/x68k.h
 */

#include <linux/compiler.h>

struct x68k_drive_info
{
	char sig[4];
	__be32 max;
	__be32 alt;
	__be32 shipping;
};

struct x68k_partition_info
{
	char system[8];
	__be32 start;
	__be32 length;
};

struct x68k_rootsector
{
	struct x68k_drive_info head;
	struct x68k_partition_info partition[15];
} __packed;

int x68k_partition(struct parsed_partitions *state);
